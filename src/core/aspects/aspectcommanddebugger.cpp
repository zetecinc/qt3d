/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifdef QT3D_JOBS_RUN_STATS

#include "aspectcommanddebugger_p.h"
#include <Qt3DCore/qaspectengine.h>
#include <Qt3DCore/private/qabstractaspect_p.h>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

namespace Debug {

namespace {

const qint32 MagicNumber = 0x454;

struct CommandHeader
{
    qint32 magic;
    qint32 size;
};

} // anonymous

AspectCommandDebugger::ReadBuffer::ReadBuffer()
    : buffer(4096, 0)
    , startIdx(0)
    , endIdx(0)
{
}

AspectCommandDebugger::AspectCommandDebugger(QObject *parent)
    : QTcpServer(parent)
    , m_aspectEngine(nullptr)
{
}

void AspectCommandDebugger::initialize()
{
    QObject::connect(this, &QTcpServer::newConnection, [this] {
        QTcpSocket *socket = nextPendingConnection();
        m_connections.push_back(socket);

        QObject::connect(socket, &QTcpSocket::disconnected, [this, socket] {
            m_connections.removeOne(socket);
            // Destroy object to make sure all QObject connection are removed
            socket->deleteLater();
        });

        QObject::connect(socket, &QTcpSocket::readyRead, [this, socket] {
            onCommandReceived(socket);
        });
    });
    const bool listening = listen(QHostAddress::Any, 8883);
    if (!listening)
        qWarning() << Q_FUNC_INFO << "failed to listen on port 8883";
}

void AspectCommandDebugger::setAspectEngine(QAspectEngine *engine)
{
    m_aspectEngine = engine;
}

void AspectCommandDebugger::asynchronousReplyFinished(AsynchronousCommandReply *reply)
{
    Q_ASSERT(reply->isFinished());
    QTcpSocket *socket = m_asyncCommandToSocketEntries.take(reply);
    if (m_connections.contains(socket))
        sendReply(socket, reply->data());
    reply->deleteLater();
}


// Expects to receive commands in the form
// CommandHeader { MagicNumber; size }
// JSON {
//         command: "commandName"
//         data: JSON Obj
// }
void AspectCommandDebugger::onCommandReceived(QTcpSocket *socket)
{
    const QByteArray newData = socket->readAll();
    m_readBuffer.buffer.insert(m_readBuffer.endIdx, newData);
    m_readBuffer.endIdx += newData.size();

    const int commandPacketSize = sizeof(CommandHeader);
    while ((m_readBuffer.endIdx - m_readBuffer.startIdx) >= commandPacketSize) {
        CommandHeader *header = reinterpret_cast<CommandHeader *>(m_readBuffer.buffer.data() + m_readBuffer.startIdx);
        if (header->magic == MagicNumber &&
                (m_readBuffer.endIdx - (m_readBuffer.startIdx + commandPacketSize)) >= header->size) {
            // We have a valid command
            // We expect command to be a CommandHeader + some json text
            const QJsonDocument doc = QJsonDocument::fromJson(
                        QByteArray(m_readBuffer.buffer.data() + m_readBuffer.startIdx + commandPacketSize,
                                   header->size));

            if (!doc.isNull()) {
                qDebug() << Q_FUNC_INFO << doc.toJson();
                // Send command to the aspectEngine
                QJsonObject commandObj = doc.object();
                const QJsonValue commandNameValue = commandObj.value(QLatin1String("command"));
                executeCommand(commandNameValue.toString(), socket);
            }

            m_readBuffer.startIdx += commandPacketSize + header->size;
        }
    }
    if (m_readBuffer.startIdx != m_readBuffer.endIdx && m_readBuffer.startIdx != 0) {
        // Copy remaining length of buffer at begininning
        memcpy(m_readBuffer.buffer.data(),
               m_readBuffer.buffer.constData() + m_readBuffer.startIdx,
               m_readBuffer.endIdx - m_readBuffer.startIdx);
    }
    m_readBuffer.endIdx -= m_readBuffer.startIdx;
    m_readBuffer.startIdx = 0;

}

void AspectCommandDebugger::sendReply(QTcpSocket *socket, const QByteArray &payload)
{
    CommandHeader replyHeader;

    replyHeader.magic = MagicNumber;
    replyHeader.size = payload.size();
    // Write header
    socket->write(reinterpret_cast<const char *>(&replyHeader), sizeof(CommandHeader));
    // Write payload
    socket->write(payload.constData(), payload.size());
}

void AspectCommandDebugger::executeCommand(const QString &command,
                                           QTcpSocket *socket)
{
    // Only a single aspect is going to reply
    const QVariant response = m_aspectEngine->executeCommand(command);
    if (response.userType() == qMetaTypeId<AsynchronousCommandReply *>()) { // AsynchronousCommand
        // Store the command | socket in a table
        AsynchronousCommandReply *reply = response.value<AsynchronousCommandReply *>();
        // Command has already been completed
        if (reply->isFinished()) {
            sendReply(socket, reply->data());
        } else { // Command is not completed yet
            QObject::connect(reply, &AsynchronousCommandReply::finished,
                             this, &AspectCommandDebugger::asynchronousReplyFinished);
            m_asyncCommandToSocketEntries.insert(reply, socket);
        }
    } else { // Synchronous command
        // and send response to client
        QJsonObject reply;
        reply.insert(QStringLiteral("command"), QJsonValue(command));
        // TO DO: convert QVariant to QJsonDocument/QByteArray
        sendReply(socket, QJsonDocument(reply).toJson());

    }
}

} // Debug

} // Qt3DCore

QT_END_NAMESPACE

#endif