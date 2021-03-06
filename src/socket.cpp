/******************************************************************************\
 * Copyright (c) 2004-2013
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later 
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "socket.h"
#include "server.h"


/* Implementation *************************************************************/
void CSocket::Init ( const quint16 iPortNumber )
{
    // allocate memory for network receive and send buffer in samples
    vecbyRecBuf.Init ( MAX_SIZE_BYTES_NETW_BUF );

    // initialize the listening socket
    bool bSuccess;

    if ( bIsClient )
    {
        // Per definition use the port number plus ten for the client to make
        // it possible to run server and client on the same computer. If the
        // port is not available, try "NUM_SOCKET_PORTS_TO_TRY" times with
        // incremented port numbers
        quint16 iClientPortIncrement = 10; // start value: port nubmer plus ten
        bSuccess                     = false; // initialization for while loop
        while ( !bSuccess &&
                ( iClientPortIncrement <= NUM_SOCKET_PORTS_TO_TRY ) )
        {
            bSuccess = SocketDevice.bind (
                QHostAddress( QHostAddress::Any ),
                iPortNumber + iClientPortIncrement );

            iClientPortIncrement++;
        }
    }
    else
    {
        // for the server, only try the given port number and do not try out
        // other port numbers to bind since it is imporatant that the server
        // gets the desired port number
        bSuccess = SocketDevice.bind (
            QHostAddress ( QHostAddress::Any ), iPortNumber );
    }

    if ( !bSuccess )
    {
        // we cannot bind socket, throw error
        throw CGenErr ( "Cannot bind the socket (maybe "
            "the software is already running).", "Network Error" );
    }

    // connect the "activated" signal
#ifdef ENABLE_RECEIVE_SOCKET_IN_SEPARATE_THREAD
    if ( bIsClient )
    {
        // We have to use a blocked queued connection since in case we use a
        // separate socket thread, the "readyRead" signal would occur and our
        // "OnDataReceived" function would be run in another thread. This could
        // lead to a situation that a new "readRead" occurs while the processing
        // of the previous signal was not finished -> the error: "Multiple
        // socket notifiers for same socket" may occur.
        QObject::connect ( &SocketDevice, SIGNAL ( readyRead() ),
            this, SLOT ( OnDataReceived() ), Qt::BlockingQueuedConnection );
    }
    else
    {
        // the server does not use a separate socket thread right now, in that
        // case we must not use the blocking queued connection, otherwise we
        // would get a dead lock
        QObject::connect ( &SocketDevice, SIGNAL ( readyRead() ),
            this, SLOT ( OnDataReceived() ) );
    }
#else
    QObject::connect ( &SocketDevice, SIGNAL ( readyRead() ),
        this, SLOT ( OnDataReceived() ) );
#endif
}

void CSocket::SendPacket ( const CVector<uint8_t>& vecbySendBuf,
                           const CHostAddress&     HostAddr )
{
    QMutexLocker locker ( &Mutex );

    const int iVecSizeOut = vecbySendBuf.Size();

    if ( iVecSizeOut != 0 )
    {
        // send packet through network (we have to convert the constant unsigned
        // char vector in "const char*", for this we first convert the const
        // uint8_t vector in a read/write uint8_t vector and then do the cast to
        // const char*)
        SocketDevice.writeDatagram (
            (const char*) &( (CVector<uint8_t>) vecbySendBuf )[0],
            iVecSizeOut,
            HostAddr.InetAddr,
            HostAddr.iPort );
    }
}

void CSocket::OnDataReceived()
{
    while ( SocketDevice.hasPendingDatagrams() )
    {
        QHostAddress SenderAddress;
        quint16      SenderPort;

        // read block from network interface and query address of sender
        const int iNumBytesRead =
            SocketDevice.readDatagram ( (char*) &vecbyRecBuf[0],
                                        MAX_SIZE_BYTES_NETW_BUF,
                                        &SenderAddress,
                                        &SenderPort );

        // check if an error occurred
        if ( iNumBytesRead < 0 )
        {
            return;
        }

        // convert address of client
        const CHostAddress RecHostAddr ( SenderAddress, SenderPort );

        if ( bIsClient )
        {
            // client:

            // check if packet comes from the server we want to connect and that
            // the channel is enabled
            if ( ( pChannel->GetAddress() == RecHostAddr ) &&
                 pChannel->IsEnabled() )
            {
                // this network packet is valid, put it in the channel
                switch ( pChannel->PutData ( vecbyRecBuf, iNumBytesRead ) )
                {
                case PS_AUDIO_OK:
                    PostWinMessage ( MS_JIT_BUF_PUT, MUL_COL_LED_GREEN );
                    break;

                case PS_AUDIO_ERR:
                case PS_GEN_ERROR:
                    PostWinMessage ( MS_JIT_BUF_PUT, MUL_COL_LED_RED );
                    break;

                case PS_PROT_ERR:
                    PostWinMessage ( MS_JIT_BUF_PUT, MUL_COL_LED_YELLOW );
                    break;

                default:
                    // other put data states need not to be considered here
                    break;
                }
            }
            else
            {
                // inform about received invalid packet by fireing an event
                emit InvalidPacketReceived ( vecbyRecBuf,
                                             iNumBytesRead,
                                             RecHostAddr );
            }
        }
        else
        {
            // server:

            if ( pServer->PutData ( vecbyRecBuf, iNumBytesRead, RecHostAddr ) )
            {
                // this was an audio packet, start server
                // tell the server object to wake up if it
                // is in sleep mode (Qt will delete the event object when done)
                QCoreApplication::postEvent ( pServer,
                    new CCustomEvent ( MS_PACKET_RECEIVED, 0, 0 ) );
            }
        }
    }
}
