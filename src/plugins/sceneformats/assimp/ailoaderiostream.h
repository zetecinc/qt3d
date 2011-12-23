/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtQuick3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef AILOADERIOSTREAM_H
#define AILOADERIOSTREAM_H

#include <QtGlobal>
#include "IOStream.h"
#include "IOSystem.h"

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

class AiLoaderIOStream : public Assimp::IOStream
{
public:
    AiLoaderIOStream(QIODevice *device);
    ~AiLoaderIOStream();
    size_t Read( void* pvBuffer, size_t pSize, size_t pCount);
    size_t Write( const void* pvBuffer, size_t pSize, size_t pCount);
    aiReturn Seek( size_t pOffset, aiOrigin pOrigin);
    size_t Tell() const;
    size_t FileSize() const;
    void Flush();
    QIODevice *device() const { return m_device; }
private:
    QIODevice *m_device;
    bool m_errorState;
};

#endif // AILOADERIOSTREAM_H
