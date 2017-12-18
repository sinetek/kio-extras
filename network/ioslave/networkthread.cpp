/*
    This file is part of the network kioslave, part of the KDE project.

    Copyright 2009 Friedrich W. H. Kossebau <kossebau@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "networkthread.h"

// ioslave
#include "networkinitwatcher.h"
// network
#include "network.h"
#include "netdevice.h"
#include "netservice.h"
//Qt
#include <QtCore/QCoreApplication>

#include <QDebug>

NetworkThread::NetworkThread()
  : QThread()
  , mNetwork( nullptr )
  , mContinue( true )
{
}

Mollet::Network* NetworkThread::network() const { return mNetwork; }

void NetworkThread::pause()
{
//qDebug()<<"before lock";
    mMutex.lock();
//qDebug()<<"after lock";
    exit();
//qDebug()<<"after exit";
}


void NetworkThread::unpause()
{
//qDebug()<<"before unlock";
    mMutex.unlock();
//qDebug()<<"after unlock";
}

void NetworkThread::finish()
{
    mContinue = false;
    exit();
}


void NetworkThread::run()
{
    mNetwork = Mollet::Network::network();

//qDebug()<<"starting with lock";
    mMutex.lock();
    new NetworkInitWatcher( mNetwork, &mMutex );

    do
    {
//qDebug()<<"going exec()";
        exec();
//qDebug()<<"left exec()";
        mMutex.lock();
//qDebug()<<"after lock";
        mMutex.unlock();
//qDebug()<<"after unlock";
    }
    while( mContinue );
}

NetworkThread::~NetworkThread()
{
}
