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

#include "serverlist.h"


/* Implementation *************************************************************/
CServerListManager::CServerListManager ( const quint16  iNPortNum,
                                         const QString& sNCentServAddr,
                                         const QString& strServerInfo,
                                         const int      iNumChannels,
                                         const bool     bNCentServPingServerInList,
                                         CProtocol*     pNConLProt )
    : iPortNumber                     ( iNPortNum ),
      iNumPredefinedServers           ( 0 ),
      bUseDefaultCentralServerAddress ( false ),
      bCentServPingServerInList       ( bNCentServPingServerInList ),
      pConnLessProtocol               ( pNConLProt )
{
    // set the central server address
    SetCentralServerAddress ( sNCentServAddr );

    // prepare the server info information
    QStringList slServInfoSeparateParams;
    int         iServInfoNumSplitItems = 0;

    if ( !strServerInfo.isEmpty() )
    {
        // split the different parameter strings
        slServInfoSeparateParams = strServerInfo.split ( ";" );

        // get the number of items in the split list
        iServInfoNumSplitItems = slServInfoSeparateParams.count();
    }

    // per definition, the very first entry is this server and this entry will
    // never be deleted
    ServerList.clear();

    // init server list entry (server info for this server) with defaults, per
    // definition the client substitudes the IP address of the central server
    // itself for his server list
    CServerListEntry ThisServerListEntry ( CHostAddress(),
                                           iPortNumber,
                                           "",
                                           "",
                                           QLocale::system().country(),
                                           "",
                                           iNumChannels,
                                           true );

    // parse the server info string according to definition:
    // [this server name];[this server city]; ...
    //    [this server country as QLocale ID]; ...
    // per definition, we expect at least three parameters
    if ( iServInfoNumSplitItems >= 3 )
    {
        // [this server name]
        ThisServerListEntry.strName = slServInfoSeparateParams[0];

        // [this server city]
        ThisServerListEntry.strCity = slServInfoSeparateParams[1];

        // [this server country as QLocale ID]
        const int iCountry = slServInfoSeparateParams[2].toInt();
        if ( ( iCountry >= 0 ) && ( iCountry <= QLocale::LastCountry ) )
        {
            ThisServerListEntry.eCountry = static_cast<QLocale::Country> (
                iCountry );
        }
    }

    // per definition, the first entry in the server list is the own server
    ServerList.append ( ThisServerListEntry );

    // parse the predefined server infos (if any) according to definition:
    // [server1 address];[server1 name];[server1 city]; ...
    //    [server1 country as QLocale ID]; ...
    //    [server2 address];[server2 name];[server2 city]; ...
    //    [server2 country as QLocale ID]; ...
    //    ...
    int iCurUsedServInfoSplitItems = 3; // three items are used for this server

    // we always expect four items per new server, also check for maximum
    // allowed number of servers in the server list
    while ( ( iServInfoNumSplitItems - iCurUsedServInfoSplitItems >= 4 ) &&
            ( iNumPredefinedServers <= MAX_NUM_SERVERS_IN_SERVER_LIST ) )
    {
        // create a new server list entry
        CServerListEntry NewServerListEntry ( CHostAddress(),
                                              0, // port number not used
                                              "",
                                              "",
                                              QLocale::AnyCountry,
                                              "",
                                              iNumChannels,
                                              true );

        // [server n address]
        NetworkUtil().ParseNetworkAddress (
            slServInfoSeparateParams[iCurUsedServInfoSplitItems],
            NewServerListEntry.HostAddr );

        // [server n name]
        NewServerListEntry.strName =
            slServInfoSeparateParams[iCurUsedServInfoSplitItems + 1];

        // [server n city]
        NewServerListEntry.strCity =
            slServInfoSeparateParams[iCurUsedServInfoSplitItems + 2];

        // [server n country as QLocale ID]
        const int iCountry =
            slServInfoSeparateParams[iCurUsedServInfoSplitItems + 3].toInt();

        if ( ( iCountry >= 0 ) && ( iCountry <= QLocale::LastCountry ) )
        {
            NewServerListEntry.eCountry = static_cast<QLocale::Country> (
                iCountry );
        }

        // add the new server to the server list
        ServerList.append ( NewServerListEntry );

        // we have used four items and have created one predefined server
        // (adjust counters)
        iCurUsedServInfoSplitItems += 4;
        iNumPredefinedServers++;
    }


    // Connections -------------------------------------------------------------
    QObject::connect ( &TimerPollList, SIGNAL ( timeout() ),
        this, SLOT ( OnTimerPollList() ) );

    QObject::connect ( &TimerPingServerInList, SIGNAL ( timeout() ),
        this, SLOT ( OnTimerPingServerInList() ) );

    QObject::connect ( &TimerPingCentralServer, SIGNAL ( timeout() ),
        this, SLOT ( OnTimerPingCentralServer() ) );

    QObject::connect ( &TimerRegistering, SIGNAL ( timeout() ),
        this, SLOT ( OnTimerRegistering() ) );
}

void CServerListManager::SetCentralServerAddress ( const QString sNCentServAddr )
{
    QMutexLocker locker ( &Mutex );

    strCentralServerAddress = sNCentServAddr;

    // per definition: If the central server address is empty, the server list
    // is disabled.
    // per definition: If we are in server mode and the central server address
    // is the localhost address, we are in central server mode. For the central
    // server, the server list is always enabled.
    if ( !strCentralServerAddress.isEmpty() )
    {
        bIsCentralServer =
            ( !strCentralServerAddress.toLower().compare ( "localhost" ) ||
              !strCentralServerAddress.compare ( "127.0.0.1" ) );

        bEnabled = true;
    }
    else
    {
        bIsCentralServer = false;
        bEnabled         = true;
    }
}

void CServerListManager::Update()
{
    QMutexLocker locker ( &Mutex );

    if ( bEnabled )
    {
        if ( bIsCentralServer )
        {
            // start timer for polling the server list if enabled
            // 1 minute = 60 * 1000 ms
            TimerPollList.start ( SERVLIST_POLL_TIME_MINUTES * 60000 );

            if ( bCentServPingServerInList )
            {
                // start timer for sending ping messages to servers in the list
                TimerPingServerInList.start ( SERVLIST_UPDATE_PING_SERVERS_MS );
            }
        }
        else
        {
            // initiate registration right away so that we do not have to wait
            // for the first time out of the timer until the slave server gets
            // registered at the central server, note that we have to unlock
            // the mutex before calling the function since inside this function
            // the mutex is locked, too
            locker.unlock();
            {
                OnTimerRegistering();
            }
            locker.relock();

            // start timer for registering this server at the central server
            // 1 minute = 60 * 1000 ms
            TimerRegistering.start ( SERVLIST_REGIST_INTERV_MINUTES * 60000 );

            // Start timer for ping the central server in short intervals to
            // keep the port open at the NAT router.
            // If no NAT is used, we send the messages anyway since they do
            // not hurt (very low traffic). We also reuse the same update
            // time as used in the central server for pinging the slave
            // servers.
            TimerPingCentralServer.start ( SERVLIST_UPDATE_PING_SERVERS_MS );
        }
    }
    else
    {
        // disable service -> stop timer
        if ( bIsCentralServer )
        {
            TimerPollList.stop();

            if ( bCentServPingServerInList )
            {
                TimerPingServerInList.stop();
            }
        }
        else
        {
            TimerRegistering.stop();
            TimerPingCentralServer.stop();
        }
    }
}


/* Central server functionality ***********************************************/
void CServerListManager::OnTimerPingServerInList()
{
    QMutexLocker locker ( &Mutex );

    const int iCurServerListSize = ServerList.size();

    // send ping to list entries except of the very first one (which is the central
    // server entry) and the predefined servers
    for ( int iIdx = 1 + iNumPredefinedServers; iIdx < iCurServerListSize; iIdx++ )
    {
        // send empty message to keep NAT port open at slave server
        pConnLessProtocol->CreateCLEmptyMes ( ServerList[iIdx].HostAddr );
    }
}

void CServerListManager::OnTimerPollList()
{
    QMutexLocker locker ( &Mutex );

    // Check all list entries except of the very first one (which is the central
    // server entry) and the predefined servers if they are still valid.
    // Note that we have to use "ServerList.size()" function in the for loop
    // since we may remove elements from the server list inside the for loop.
    for ( int iIdx = 1 + iNumPredefinedServers; iIdx < ServerList.size(); iIdx++ )
    {
        // 1 minute = 60 * 1000 ms
        if ( ServerList[iIdx].RegisterTime.elapsed() >
                ( SERVLIST_TIME_OUT_MINUTES * 60000 ) )
        {
            // remove this list entry
            ServerList.removeAt ( iIdx );
        }
    }
}

void CServerListManager::CentralServerRegisterServer ( const CHostAddress&    InetAddr,
                                                       const CServerCoreInfo& ServerInfo )
{
    QMutexLocker locker ( &Mutex );

    if ( bIsCentralServer && bEnabled )
    {
        const int iCurServerListSize = ServerList.size();
        
        // check for maximum allowed number of servers in the server list
        if ( iCurServerListSize < MAX_NUM_SERVERS_IN_SERVER_LIST )
        {
            // define invalid index used as a flag
            const int ciInvalidIdx = -1;

            // Check if server is already registered. Use address to identify
            // a server. The very first list entry must not be checked since
            // this is per definition the central server (i.e., this server)
            int iSelIdx = ciInvalidIdx; // initialize with an illegal value
            for ( int iIdx = 1; iIdx < iCurServerListSize; iIdx++ )
            {
                if ( ServerList[iIdx].HostAddr == InetAddr )
                {
                    // store entry index
                    iSelIdx = iIdx;

                    // entry found, leave for-loop
                    continue;
                }
            }

            // if server is not yet registered, we have to create a new entry
            if ( iSelIdx == ciInvalidIdx )
            {
                // create a new server list entry and init with received data
                ServerList.append ( CServerListEntry ( InetAddr, ServerInfo ) );
            }
            else
            {
                // do not update the information in the predefined servers
                if ( iSelIdx > iNumPredefinedServers )
                {
                    // update all data and call update registration function
                    ServerList[iSelIdx].iLocalPortNumber = ServerInfo.iLocalPortNumber;
                    ServerList[iSelIdx].strName          = ServerInfo.strName;
                    ServerList[iSelIdx].strTopic         = ServerInfo.strTopic;
                    ServerList[iSelIdx].eCountry         = ServerInfo.eCountry;
                    ServerList[iSelIdx].strCity          = ServerInfo.strCity;
                    ServerList[iSelIdx].iMaxNumClients   = ServerInfo.iMaxNumClients;
                    ServerList[iSelIdx].bPermanentOnline = ServerInfo.bPermanentOnline;

                    ServerList[iSelIdx].UpdateRegistration();
                }
            }
        }
    }
}

void CServerListManager::CentralServerUnregisterServer ( const CHostAddress& InetAddr )
{
    QMutexLocker locker ( &Mutex );

    if ( bIsCentralServer && bEnabled )
    {
        const int iCurServerListSize = ServerList.size();

        // Find the server to unregister in the list. The very first list entry
        // must not be checked since this is per definition the central server
        // (i.e., this server), also the predefined servers must not be checked.
        for ( int iIdx = 1 + iNumPredefinedServers; iIdx < iCurServerListSize; iIdx++ )
        {
            if ( ServerList[iIdx].HostAddr == InetAddr )
            {
                // remove this list entry
                ServerList.removeAt ( iIdx );

                // entry found, leave for-loop (it is important to exit the
                // for loop since when we remove an item from the server list,
                // "iCurServerListSize" is not correct anymore and we could get
                // a segmentation fault)
                break;
            }
        }
    }
}

void CServerListManager::CentralServerQueryServerList ( const CHostAddress& InetAddr )
{
    QMutexLocker locker ( &Mutex );

    if ( bIsCentralServer && bEnabled )
    {
        const int iCurServerListSize = ServerList.size();

        // allocate memory for the entire list
        CVector<CServerInfo> vecServerInfo ( iCurServerListSize );

        // copy the list (we have to copy it since the message requires
        // a vector but the list is actually stored in a QList object and
        // not in a vector object
        for ( int iIdx = 0; iIdx < iCurServerListSize; iIdx++ )
        {
            // copy list item
            vecServerInfo[iIdx] = ServerList[iIdx];

            if ( iIdx > 0 )
            {
                // check if the address of the client which is requesting the
                // list is the same address as one server in the list -> in this
                // case he has to connect to the local host address
                if ( vecServerInfo[iIdx].HostAddr.InetAddr == InetAddr.InetAddr )
                {
                    vecServerInfo[iIdx].HostAddr.InetAddr =
                        QHostAddress ( QHostAddress::LocalHost );

                    // take the local port number instead of the received port
                    // number since some NAT (network address translation) might
                    // have changed the port, note that the predefined servers
                    // are treated differently, for these we assume that the
                    // received port number is the same as the actual port
                    // number
                    if ( iIdx > iNumPredefinedServers )
                    {
                        vecServerInfo[iIdx].HostAddr.iPort =
                            ServerList[iIdx].iLocalPortNumber;
                    }
                }
                else
                {
                    // create "send empty message" for all registered servers
                    // (except of the very first list entry since this is this
                    // server (central server) per definition) and also it is
                    // not required to send this message, if the server is on
                    // the same computer
                    pConnLessProtocol->CreateCLSendEmptyMesMes ( 
                        vecServerInfo[iIdx].HostAddr,
                        InetAddr );
                }
            }
        }

        // send the server list to the client
        pConnLessProtocol->CreateCLServerListMes ( InetAddr, vecServerInfo );
    }
}


/* Slave server functionality *************************************************/
void CServerListManager::OnTimerPingCentralServer()
{
    QMutexLocker locker ( &Mutex );

    // first check if central server address is valid
    if ( !( SlaveCurCentServerHostAddress == CHostAddress() ) )
    {
        // send empty message to central server to keep NAT port open -> we do
        // not require any answer from the central server
        pConnLessProtocol->CreateCLEmptyMes ( SlaveCurCentServerHostAddress );
    }
}

void CServerListManager::SlaveServerRegisterServer ( const bool bIsRegister )
{
    // we need the lock since the user might change the server properties at
    // any time
    QMutexLocker locker ( &Mutex );

    // get the correct central server address
    const QString strCurCentrServAddr =
        SELECT_SERVER_ADDRESS ( bUseDefaultCentralServerAddress,
                                strCentralServerAddress );

    // For the slave server, the slave server properties are stored in the
    // very first item in the server list (which is actually no server list
    // but just one item long for the slave server).
    // Note that we always have to parse the server address again since if
    // it is an URL of a dynamic IP address, the IP address might have
    // changed in the meanwhile.
    if ( NetworkUtil().ParseNetworkAddress ( strCurCentrServAddr,
                                             SlaveCurCentServerHostAddress ) )
    {
        if ( bIsRegister )
        {
            // register server
            pConnLessProtocol->CreateCLRegisterServerMes ( SlaveCurCentServerHostAddress,
                                                           ServerList[0] );
        }
        else
        {
            // unregister server
            pConnLessProtocol->CreateCLUnregisterServerMes ( SlaveCurCentServerHostAddress );
        }
    }
}
