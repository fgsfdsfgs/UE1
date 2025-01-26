/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver.
	Copyright 1997 Epic MegaGames, Inc. This software is a trade secret.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "IpDrvPrivate.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Package.
IMPLEMENT_PACKAGE(IpDrv);

// Classes.
class UTcpNetDriver;
class UTcpipConnection;

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define WINSOCK_MAX_PACKET (512)

// Linked list of drivers.
static TArray<UTcpNetDriver*> GDrivers;
UBOOL GInitialized;

// Register names.
#define NAMES_ONLY
#define DECLARE_NAME(name) IPDRV_API FName IPDRV_##name;
#include "IpDrvClasses.h"
#undef DECLARE_NAME
#undef NAMES_ONLY

/*-----------------------------------------------------------------------------
	Host resolution thread.
-----------------------------------------------------------------------------*/

struct FResolveInfo
{
	in_addr Addr;
	DWORD   ThreadId;
	char    HostName[256];
	char    Error   [256];
	FResolveInfo( const char* InHostName )
	{	
		appStrcpy( HostName, InHostName );
		*Error = 0;
	}
};
#ifdef PLATFORM_WIN32
DWORD __stdcall ResolveThreadEntry( void* Arg )
#else
void* ResolveThreadEntry( void* Arg )
#endif
{
	FResolveInfo* Info = (FResolveInfo*)Arg;
	IpSetInt( Info->Addr, 0 );
	HOSTENT* HostEnt = gethostbyname( Info->HostName ); 
	if( HostEnt==NULL || HostEnt->h_addrtype!=PF_INET )
		appSprintf( Info->Error, "Can't find host %s", Info->HostName );
	else
		Info->Addr = *(in_addr*)( *HostEnt->h_addr_list );
	Info->ThreadId = 0;
	return 0;
}

/*-----------------------------------------------------------------------------
	UTcpNetDriver.
-----------------------------------------------------------------------------*/

//
// Windows sockets network driver.
//
class DLL_EXPORT UTcpNetDriver : public UNetDriver
{
	DECLARE_CLASS(UTcpNetDriver,UNetDriver,CLASS_Transient|CLASS_Config)

	// Variables.
	sockaddr_in	LocalAddr;
	SOCKET		Socket;
	in_addr		HostAddr;
	char		HostName[256];

	// Constructor.

	// UObject interface.
	void Destroy();

	// UNetDriver interface.
	UBOOL Init( UBOOL Connect, FNetworkNotify* InNotify, FURL& ConnectURL, char* Error256 );
	void Tick();
	UBOOL IsInternet() {return 1;}

	// FExec interface.
	INT Exec( const char* Cmd, FOutputDevice* Out=GSystem );

	// UTcpNetDriver interface.
	UTcpipConnection* GetServerConnection() {return (UTcpipConnection*)ServerConnection;}
};
IMPLEMENT_CLASS(UTcpNetDriver);

/*-----------------------------------------------------------------------------
	UTcpipConnection.
-----------------------------------------------------------------------------*/

//
// Windows socket class.
//
class DLL_EXPORT UTcpipConnection : public UNetConnection
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT(UTcpipConnection,UNetConnection,CLASS_Config|CLASS_Transient)
	NO_DEFAULT_CONSTRUCTOR(UTcpipConnection)

	// Variables.
	sockaddr_in		RemoteAddr;
	UBOOL			OpenedLocally;
	char            LastStatusText[256];
	FResolveInfo*	ResolveInfo;

	// Latent queue.
	struct FLatentQueue
	{
		INT				Num;
		BYTE*			Data;
		DOUBLE			Time;
		FLatentQueue*	Next;
		FLatentQueue( FLatentQueue* InNext, INT InNum, BYTE* InData, DOUBLE InTime )
		:	Num		( InNum )
		,	Data	( (BYTE*)memcpy( new BYTE[InNum], InData, InNum ) )
		,	Next	( InNext )
		,	Time	( InTime )
		{}
	} *LatentQueue;

	// Constructors and destructors.
	static void InternalClassInitializer( UClass* Class )
	{
		guard(UTcpipConnection::InternalClassInitializer);
		if( Class==UTcpipConnection::StaticClass )
		{
			new(Class,"SimLatency",   RF_Public)UIntProperty( CPP_PROPERTY(SimLatency   ), "Network", CPF_Config );
			new(Class,"SimPacketLoss",RF_Public)UIntProperty( CPP_PROPERTY(SimPacketLoss), "Network", CPF_Config );
		}
		unguard;
	}
	UTcpipConnection( UTcpNetDriver* InDriver, sockaddr_in InRemoteAddr, EConnectionState InState, UBOOL InOpenedLocally )
	:	UNetConnection	( InDriver )
	,	RemoteAddr		( InRemoteAddr )
	,	OpenedLocally	( InOpenedLocally )
	,	LatentQueue		( NULL )
	{
		guard(UTcpipConnection::UTcpipConnection);
		LoadConfig( NAME_Config );

		// Init the connection.
		State                 = InState;
		LastStatusText[0]     = 0;
		MaxPacket			  = WINSOCK_MAX_PACKET;

		unguard;
	}
	void Destroy()
	{
		guard(UTcpipConnection::Destroy);
		if( Channels[0] )
		{
			Channels[0]->Close();
			FlushNet();
		}
		Super::Destroy();
		unguard;
	}

	// UNetConnection interface.
	void ReadInput(FLOAT) {};
	void Tick()
	{
		guard(UTcpipConnection::Tick);
		UNetConnection::Tick();

		// Update latent queue.
		FLatentQueue** Q = &LatentQueue;
		while( *Q != NULL )
		{
			if( Driver->Time - (*Q)->Time > SimLatency/1000.0 )
			{
				sendto( GetDriver()->Socket, (char*)(*Q)->Data, (*Q)->Num, 0, (sockaddr*)&RemoteAddr, sizeof(RemoteAddr) );
				FLatentQueue* Next = (*Q)->Next;
				delete *Q;
				*Q = Next;
			}
			else
			{
				Q = &(*Q)->Next;
			}
		}
		unguard;
	}
	UTcpNetDriver* GetDriver()
	{
		return (UTcpNetDriver*)Driver;
	}
	void FlushNet( UBOOL Duplicate=0 )
	{
		guard(UTcpipConnection::FlushNet);
		Super::FlushNet( Duplicate );

		// If destination address isn't resolved yet, send nowhere.
		if( ResolveInfo )
		{
			if( ResolveInfo->ThreadId )
			{
				OutNum = 0;
				return;
			}
			if( *ResolveInfo->Error )
			{
				debugf( NAME_Log, "%s", ResolveInfo->Error );
				appStrcpy( LastStatusText, ResolveInfo->Error );
				Driver->ServerConnection->State = USOCK_Closed;
				delete ResolveInfo;
				ResolveInfo = NULL;
				return;
			}
			RemoteAddr.sin_addr = ResolveInfo->Addr;
			debugf
			(
				"Resolved %s (%i.%i.%i.%i)",
				ResolveInfo->HostName,
				IPBYTE(ResolveInfo->Addr, 1),
				IPBYTE(ResolveInfo->Addr, 2),
				IPBYTE(ResolveInfo->Addr, 3),
				IPBYTE(ResolveInfo->Addr, 4)
			);
			delete ResolveInfo;
			ResolveInfo = NULL;
		}

		// If there is any pending data to send, send it.
		if( OutNum>0 || Driver->Time-LastSendTime>Driver->KeepAliveTime )
		{
			LastSendTime = Driver->Time;
			if( SimLatency == 0 )
			{
				// Send now.
				if(	!SimPacketLoss || 100*appFrand()>SimPacketLoss )
				{
					if( sendto( GetDriver()->Socket, (char*)OutData, OutNum, 0, (sockaddr*)&RemoteAddr, sizeof(RemoteAddr) )!=OutNum )
						debugf( NAME_DevNet, "Failed to send UDP packet" );
				}
				if( Duplicate )
				{
					// Send a copy in case packets were lost.
					if(	!SimPacketLoss || 100*appFrand()>SimPacketLoss )
						sendto( GetDriver()->Socket, (char*)OutData, OutNum, 0, (sockaddr*)&RemoteAddr, sizeof(RemoteAddr) );
					QueuedBytes += OutNum + UDP_HEADER_SIZE;
				}
			}
			else
			{
				// Stick in simulated latency queue.
				LatentQueue = new FLatentQueue( LatentQueue, OutNum, OutData, Driver->Time );
			}
			QueuedBytes += OutNum + UDP_HEADER_SIZE;
			OutNum = 0;
		}
		unguard;
	}
	char* Describe( char* String256 )
	{
		guard(UTcpipConnection::Describe);
		String256 += appSprintf
		(
			String256,
			"%s [%i.%i.%i.%i]:%i state: ",
			URL.Host,
			IPBYTE(RemoteAddr.sin_addr, 1),
			IPBYTE(RemoteAddr.sin_addr, 2),
			IPBYTE(RemoteAddr.sin_addr, 3),
			IPBYTE(RemoteAddr.sin_addr, 4),
			ntohs(RemoteAddr.sin_port)
		);
		switch( State )
		{
			case USOCK_Pending:
				appSprintf( String256, "Pending" );
				break;
			case USOCK_Open:
				appSprintf( String256, "Open" );
				break;
			case USOCK_Closed:
				appSprintf( String256, "Closed" );
				break;
			default:
				appSprintf( String256, "Unknown" );
		}
		return String256;
		unguard;
	}
	void AssertValid()
	{
		guard(UTcpipConnection::AssertValid);
		check(State==USOCK_Closed || State==USOCK_Pending || USOCK_Open);
		unguard;
	}
};
IMPLEMENT_CLASS(UTcpipConnection);

/*-----------------------------------------------------------------------------
	UTcpNetDriver init and exit.
-----------------------------------------------------------------------------*/

//
// Initialize the windows sockets network driver.
//
UBOOL UTcpNetDriver::Init( UBOOL Connect, FNetworkNotify* InNotify, FURL& URL, char* Error256 )
{
	guard(UTcpNetDriver::UTcpNetDriver);
	strcpy( HostName, "" );

	// Init base.
	if( !UNetDriver::Init( Connect, InNotify, URL, Error256 ) )
		return 0;

	// Init WSA.
	if( !InitSockets(Error256) )
		return 0;

	// Get this host name.
#ifdef PLATFORM_DREAMCAST
	net_ipv4_get_stats();
	if (!net_default_dev)
	{
		// TODO: Modem
		appSprintf( Error256, "No network devices available" );
		return 0;
	}
	appStrcpy( HostName, "localhost" );
#else
	if( gethostname( HostName, 256 ) )
	{
		appSprintf( Error256, "Not Connected To The Internet" );
		return 0;
	}
#endif
	debugf( NAME_Init, "WinSock gethostname: %s", HostName );
	char Home[256];
	if( Parse(appCmdLine(),"MULTIHOME=",Home,ARRAY_COUNT(Home)) )
	{
		char *A, *B, *C, *D;
		A=Home;
		if
		(	(A=Home)!=NULL
		&&	(B=appStrchr(A,'.'))!=NULL
		&&	(C=appStrchr(B+1,'.'))!=NULL
		&&	(D=appStrchr(C+1,'.'))!=NULL )
		{
			IPBYTE(HostAddr, 1) = appAtoi(A);
			IPBYTE(HostAddr, 2) = appAtoi(B+1);
			IPBYTE(HostAddr, 3) = appAtoi(C+1);
			IPBYTE(HostAddr, 4) = appAtoi(D+1);
		}
		else appErrorf( "Invalid multihome IP address %s", Home );
	}
	else
	{
#ifdef PLATFORM_DREAMCAST
		IPBYTE(HostAddr, 1) = net_default_dev->ip_addr[0];
		IPBYTE(HostAddr, 2) = net_default_dev->ip_addr[1];
		IPBYTE(HostAddr, 3) = net_default_dev->ip_addr[2];
		IPBYTE(HostAddr, 4) = net_default_dev->ip_addr[3];
#else
		HOSTENT* HostEnt = gethostbyname( HostName ); 
		if( HostEnt==NULL || HostEnt->h_addrtype!=PF_INET )
		{
			appStrcpy( Error256, "Not Connected To The Internet" );
			return 0;
		}
		HostAddr = *(in_addr*)( *HostEnt->h_addr_list );
#endif
	}

	// Create UDP socket and enable broadcasting.
	Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( Socket == INVALID_SOCKET )
	{
		Socket = 0;
		appSprintf( Error256, "WinSock: socket failed (%s)", SocketError() );
		return 0;
	}

#ifndef PLATFORM_DREAMCAST
	INT TrueBuffer=1;
	if( setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, (char*)&TrueBuffer, sizeof(TrueBuffer) ) )
	{
		appSprintf( Error256, "WinSock: setsockopt SO_BROADCAST failed (%s)", SocketError() );
		return 0;
	}
#endif

	// Increase socket queue size, because we are polling rather than threading
	// and thus we rely on Windows Sockets to buffer a lot of data on the server.
	INT RecvSize = Connect ? 0x8000 : 0x20000;
	INT SendSize = Connect ? 0x8000 : 0x20000;
	socklen_t SizeSize=sizeof(RecvSize);
	setsockopt( Socket, SOL_SOCKET, SO_RCVBUF, (char*)&RecvSize, SizeSize );
	getsockopt( Socket, SOL_SOCKET, SO_RCVBUF, (char*)&RecvSize, &SizeSize );
	setsockopt( Socket, SOL_SOCKET, SO_SNDBUF, (char*)&SendSize, SizeSize );
	getsockopt( Socket, SOL_SOCKET, SO_SNDBUF, (char*)&SendSize, &SizeSize );
	debugf( NAME_Init, "Socket queue %i / %i", RecvSize, SendSize );

	// Bind socket to our port.
	LocalAddr.sin_family      = AF_INET;
	LocalAddr.sin_addr.s_addr = 0;
	LocalAddr.sin_port        = 0;
	if( !Connect )
	{
		// Init as a server.
		Parse( appCmdLine(), "PORT=", URL.Port );
		LocalAddr.sin_port = htons(URL.Port);
		URL.Host.Setf
		(
			"%i.%i.%i.%i",
			IPBYTE(HostAddr, 1),
			IPBYTE(HostAddr, 2),
			IPBYTE(HostAddr, 3),
			IPBYTE(HostAddr, 4)
		);
	}
    if( bind( Socket, (sockaddr*)&LocalAddr, sizeof(LocalAddr) ) )
	{
		appSprintf( Error256, "WinSock: binding to port %i failed (%s)", ntohs(LocalAddr.sin_port), SocketError() );
		return 0;
	}
	DWORD NoBlock=1;
	if( ioctlsocket( Socket, FIONBIO, &NoBlock ) )
	{
		appSprintf( Error256, "WinSock: ioctlsocket failed (%s)", SocketError() );
		return 0;
	}

	// Connect to remote.
	if( Connect )
	{
		// Init remote address.
		sockaddr_in TempAddr;
		TempAddr.sin_family           = AF_INET;
		TempAddr.sin_port             = htons(URL.Port);
		IpSetInt( TempAddr.sin_addr, 0 );

		// Create new connection.
		ServerConnection = new UTcpipConnection( this, TempAddr, USOCK_Pending, 1 );
		const char* Temp = URL.GetOption( "RATE=", NULL );
		if( Temp && appAtoi(Temp) )
			ServerConnection->ByteLimit = appAtoi( Temp );
		debugf( NAME_DevNet, "Opened socket to port %i, rate %i", URL.Port, ServerConnection->ByteLimit );

		// Crack the URL.
		const char* s = *URL.Host;
		INT i;
		for( i=0; i<4 && s!=NULL && *s>='0' && *s<='9'; i++ )
		{
			s = strchr(s,'.');
			if( s )
				s++;
		}
		if( i==4 && s==NULL )
		{
			// Get numerical address directly.
			IpSetInt( GetServerConnection()->RemoteAddr.sin_addr, inet_addr( *URL.Host ) );
		}
		else
		{
			// Create thread to resolve the address.
			debugf( NAME_DevNet, "Resolving '%s'...", *URL.Host );
			GetServerConnection()->ResolveInfo = new FResolveInfo( *URL.Host );
			appThreadSpawn(
				ResolveThreadEntry,
				(void*)GetServerConnection()->ResolveInfo,
				"ResolveThread",
				true,
				&GetServerConnection()->ResolveInfo->ThreadId
			);
		}

		// Set socket URL.
		GetServerConnection()->URL = URL;

		// Create channel zero.
		FControlChannel* NewChannel = (FControlChannel*)GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );
	}

	// Init server's connection list.
	Connections.Empty();

	// Success: link into the linked list of drivers.
	GDrivers.AddItem( this );

	// Success.
	debugf
	(
		NAME_Log,
		"WinSock: I am %s (%i.%i.%i.%i)",
		HostName,
		IPBYTE(HostAddr, 1),
		IPBYTE(HostAddr, 2),
		IPBYTE(HostAddr, 3),
		IPBYTE(HostAddr, 4)
	);
	return 1;
	unguard;
}

//
// Shut down the windows sockets network driver.
//
void UTcpNetDriver::Destroy()
{
	guard(UTcpNetDriver::Destroy);

	// Destroy server connection.
	guard(DestroyServerConnection);
	if( ServerConnection )
		delete ServerConnection;
	unguard;

	// Destroy client connections.
	guard(DestroyClientConnections);
	while( Connections.Num() )
		delete Connections( 0 );
	unguard;

	// Remove from linked list of drivers.
	GDrivers.RemoveItem( this );

	// Close the socket.
	guard(CloseSocket);
	if( Socket )
		if( closesocket( Socket ) )
			debugf( NAME_Exit, "WinSock closesocket error (%s)", SocketError() );
	unguard;

	debugf( NAME_Exit, "WinSock shut down" );
	UNetDriver::Destroy();
	unguard;
}

/*-----------------------------------------------------------------------------
	UTcpNetDriver polling.
-----------------------------------------------------------------------------*/

//
// Poll the driver and forward all incoming packets to their appropriate sockets.
// Update all socket states.
//
void UTcpNetDriver::Tick()
{
	guard(UTcpNetDriver::Tick);

	// Get new time.
	Time = appSeconds();

	// Process all incoming packets.
	BYTE Data[UNetConnection::MAX_PACKET_SIZE];
	sockaddr_in FromAddr;
	INT Count=0;
	while( 1 )
	{
		// Get data, if any.
		socklen_t FromSize = sizeof(FromAddr);
		INT Size = recvfrom( Socket, (char*)Data, sizeof(Data), 0, (sockaddr*)&FromAddr, &FromSize );

		// Handle result.
		if( Size==SOCKET_ERROR && WSAGetLastError()==WSAEWOULDBLOCK )
		{
			break;
		}
		else if( Size==SOCKET_ERROR )
		{
			static UBOOL FirstError=1;
			if( FirstError )
				debugf( "UDP recvfrom error: %s", SocketError() );
			FirstError=0;
		}
		Count++;

		// Figure out which socket it came from.
		UTcpipConnection* Connection=NULL;
		if( GetServerConnection() && IpMatches(GetServerConnection()->RemoteAddr,FromAddr) )
			Connection = GetServerConnection();
		for( INT i=0; i<Connections.Num() && !Connection; i++ )
			if( IpMatches( ((UTcpipConnection*)Connections(i))->RemoteAddr, FromAddr ) )
				Connection = (UTcpipConnection*)Connections(i);

		// If we didn't find a connection, maybe create a new one.		
		if( Connection==NULL )
		{
			// Notify the server that the connection was created.
			if( Notify->NotifyAcceptingConnection()!=ACCEPTC_Accept )
				continue;

			// Create connection.
			Connection = new UTcpipConnection( this, FromAddr, USOCK_Open, 0 );
			char Temp[256];
			appSprintf
			(
				Temp,
				"%i.%i.%i.%i",
				IPBYTE(FromAddr.sin_addr, 1),
				IPBYTE(FromAddr.sin_addr, 2),
				IPBYTE(FromAddr.sin_addr, 3),
				IPBYTE(FromAddr.sin_addr, 4)
			);
			Connection->URL.Host = Temp;//!!format
			Notify->NotifyAcceptedConnection( Connection );
			Connections.AddItem( Connection );
		}

		// Send the packet to the connection for processing.
		//warning: ReceivedPacket may destroy Connection.
		debugfSlow( NAME_DevNetTraffic, "%03i: Received %i", (INT)(appSeconds()*1000)%1000, Size );
		Connection->LastReceiveTime = Time;
		Connection->ReceivedPacket( Data, Size );
	}
	//debugf( "%i", Count );

	// Poll all sockets.
	if( GetServerConnection() )
		GetServerConnection()->Tick();
	for( INT i=0; i<Connections.Num(); i++ )
		Connections(i)->Tick();

	unguard;
}

/*-----------------------------------------------------------------------------
	UTcpNetDriver command line.
-----------------------------------------------------------------------------*/

//
// Execute a command.
//
INT UTcpNetDriver::Exec( const char* Cmd, FOutputDevice* Out )
{
	guard(UTcpNetDriver::Exec);
	if( ParseCommand(&Cmd,"TESTNET") )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,"SOCKETS") )
	{
		Out->Logf( "Internet sockets:" );
		char String[256],Other[256];
		if( GetServerConnection() )
		{
			GetServerConnection()->Describe( String );
			Out->Logf( "   %s", String );
			for( FChannelIterator It(GetServerConnection()); It; ++It )
				Out->Logf( "      Channel %i: %s", It.GetIndex(), It->Describe(Other) );
		}
		for( INT i=0; i<Connections.Num(); i++ )
		{
			Connections(i)->Describe( String );
			Out->Logf( "   %s", String );
			for( FChannelIterator It(Connections(i)); It; ++It )
				Out->Logf( "      Channel %i: %s", It.GetIndex(), It->Describe(Other) );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,"URL") )
	{
		FURL URL(NULL,Cmd,TRAVEL_Absolute);
		if( URL.Valid )
		{
			FString Str;
			URL.String( Str );
			debugf(NAME_Log,"     Protocol: %s", URL.Protocol  );
			debugf(NAME_Log,"         Host: %s", URL.Host      );
			debugf(NAME_Log,"          Map: %s", URL.Map       );
			debugf(NAME_Log,"       Portal: %s", URL.Portal    );
			debugf(NAME_Log,"         Port: %i", URL.Port      );
			debugf(NAME_Log,"   NumOptions: %i", URL.Op.Num()  );
			for( int i=0; i<URL.Op.Num(); i++ )
				debugf(NAME_Log,"     Option %i: %s", i, URL.Op(i) );
			debugf(NAME_Log," Result: '%s'", *Str );
		}
		else debugf( NAME_ExecWarning, "BAD URL" );
		return 1;
	}
	else return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
