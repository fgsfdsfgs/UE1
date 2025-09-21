/*============================================================================
	UnSocket.cpp: Common interface for WinSock and BSD sockets.

	Revision history:
		* Created by Mike Danylchuk
============================================================================*/

#include "IpDrvPrivate.h"

/*----------------------------------------------------------------------------
	Initialization.
----------------------------------------------------------------------------*/

UBOOL InitSockets( char* Error256 )
{
	guard(InitSockets);

	// Init names.
	#define NAMES_ONLY
	#define DECLARE_NAME(name) IPDRV_##name = FName(#name,FNAME_Intrinsic);
	#include "IpDrvClasses.h"
	#undef DECLARE_NAME
	#undef NAMES_ONLY

#ifdef PLATFORM_WIN32
	// Init WSA.
	static UBOOL Tried = 0;
	if( !Tried )
	{
		Tried = 1;
		WSADATA WSAData;
		INT Code = WSAStartup( 0x0101, &WSAData );
		if( Code==0 )
		{
			GInitialized = 1;
			debugf
			(
				NAME_Init,
				"WinSock: version %i.%i (%i.%i), MaxSocks=%i, MaxUdp=%i",
				WSAData.wVersion>>8,WSAData.wVersion&255,
				WSAData.wHighVersion>>8,WSAData.wHighVersion&255,
				WSAData.iMaxSockets,WSAData.iMaxUdpDg
			);
			debugf( NAME_Init, "WinSock: %s", WSAData.szDescription );
		}
		else appSprintf( Error256, "WSAStartup failed (%s)", SocketError(Code) ); 
	}
#else
	GInitialized = 1;
#endif

	return GInitialized;
	unguard;
}

const char* SocketError( INT Code )
{
#ifdef PLATFORM_WIN32
	if( Code == -1 )
		Code = WSAGetLastError();
	switch( Code )
	{
		case WSAEINTR:				return "WSAEINTR";
		case WSAEBADF:				return "WSAEBADF";
		case WSAEACCES:				return "WSAEACCES";
		case WSAEFAULT:				return "WSAEFAULT";
		case WSAEINVAL:				return "WSAEINVAL";
		case WSAEMFILE:				return "WSAEMFILE";
		case WSAEWOULDBLOCK:		return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS:		return "WSAEINPROGRESS";
		case WSAEALREADY:			return "WSAEALREADY";
		case WSAENOTSOCK:			return "WSAENOTSOCK";
		case WSAEDESTADDRREQ:		return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE:			return "WSAEMSGSIZE";
		case WSAEPROTOTYPE:			return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT:		return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT:	return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT:	return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP:			return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT:		return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT:		return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE:			return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL:		return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN:			return "WSAENETDOWN";
		case WSAENETUNREACH:		return "WSAENETUNREACH";
		case WSAENETRESET:			return "WSAENETRESET";
		case WSAECONNABORTED:		return "WSAECONNABORTED";
		case WSAECONNRESET:			return "WSAECONNRESET";
		case WSAENOBUFS:			return "WSAENOBUFS";
		case WSAEISCONN:			return "WSAEISCONN";
		case WSAENOTCONN:			return "WSAENOTCONN";
		case WSAESHUTDOWN:			return "WSAESHUTDOWN";
		case WSAETOOMANYREFS:		return "WSAETOOMANYREFS";
		case WSAETIMEDOUT:			return "WSAETIMEDOUT";
		case WSAECONNREFUSED:		return "WSAECONNREFUSED";
		case WSAELOOP:				return "WSAELOOP";
		case WSAENAMETOOLONG:		return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN:			return "WSAEHOSTDOWN";
		case WSAEHOSTUNREACH:		return "WSAEHOSTUNREACH";
		case WSAENOTEMPTY:			return "WSAENOTEMPTY";
		case WSAEPROCLIM:			return "WSAEPROCLIM";
		case WSAEUSERS:				return "WSAEUSERS";
		case WSAEDQUOT:				return "WSAEDQUOT";
		case WSAESTALE:				return "WSAESTALE";
		case WSAEREMOTE:			return "WSAEREMOTE";
		case WSAEDISCON:			return "WSAEDISCON";
		case WSASYSNOTREADY:		return "WSASYSNOTREADY";
		case WSAVERNOTSUPPORTED:	return "WSAVERNOTSUPPORTED";
		case WSANOTINITIALISED:		return "WSANOTINITIALISED";
		case WSAHOST_NOT_FOUND:		return "WSAHOST_NOT_FOUND";
		case WSATRY_AGAIN:			return "WSATRY_AGAIN";
		case WSANO_RECOVERY:		return "WSANO_RECOVERY";
		case WSANO_DATA:			return "WSANO_DATA";
		case 0:						return "WSANO_NO_ERROR";
		default:					return "WSA_Unknown";
	}
#else
	if( Code == -1 )
		Code = errno;
	switch( Code )
	{
		case EINTR:					return "EINTR";
		case EBADF:					return "EBADF";
		case EACCES:				return "EACCES";
		case EFAULT:				return "EFAULT";
		case EINVAL:				return "EINVAL";
		case EMFILE:				return "EMFILE";
		case EWOULDBLOCK:			return "EWOULDBLOCK";
		case EINPROGRESS:			return "EINPROGRESS";
		case EALREADY:				return "EALREADY";
		case ENOTSOCK:				return "ENOTSOCK";
		case EDESTADDRREQ:			return "EDESTADDRREQ";
		case EMSGSIZE:				return "EMSGSIZE";
		case EPROTOTYPE:			return "EPROTOTYPE";
		case ENOPROTOOPT:			return "ENOPROTOOPT";
		case EPROTONOSUPPORT:		return "EPROTONOSUPPORT";
		case ESOCKTNOSUPPORT:		return "ESOCKTNOSUPPORT";
		case EOPNOTSUPP:			return "EOPNOTSUPP";
		case EPFNOSUPPORT:			return "EPFNOSUPPORT";
		case EAFNOSUPPORT:			return "EAFNOSUPPORT";
		case EADDRINUSE:			return "EADDRINUSE";
		case EADDRNOTAVAIL:			return "EADDRNOTAVAIL";
		case ENETDOWN:				return "ENETDOWN";
		case ENETUNREACH:			return "ENETUNREACH";
		case ENETRESET:				return "ENETRESET";
		case ECONNABORTED:			return "ECONNABORTED";
		case ECONNRESET:			return "ECONNRESET";
		case ENOBUFS:				return "ENOBUFS";
		case EISCONN:				return "EISCONN";
		case ENOTCONN:				return "ENOTCONN";
		case ESHUTDOWN:				return "ESHUTDOWN";
		case ETOOMANYREFS:			return "ETOOMANYREFS";
		case ETIMEDOUT:				return "ETIMEDOUT";
		case ECONNREFUSED:			return "ECONNREFUSED";
		case ELOOP:					return "ELOOP";
		case ENAMETOOLONG:			return "ENAMETOOLONG";
		case EHOSTDOWN:				return "EHOSTDOWN";
		case EHOSTUNREACH:			return "EHOSTUNREACH";
		case ENOTEMPTY:				return "ENOTEMPTY";
		case EUSERS:				return "EUSERS";
		case EDQUOT:				return "EDQUOT";
		case ESTALE:				return "ESTALE";
		case EREMOTE:				return "EREMOTE";
#ifndef PLATFORM_PSVITA
		case HOST_NOT_FOUND:		return "HOST_NOT_FOUND";
		case TRY_AGAIN:				return "TRY_AGAIN";
		case NO_RECOVERY:			return "NO_RECOVERY";
#endif
		case 0:						return "NO_ERROR";
		default:					return "Unknown";
	}
#endif
}

/*----------------------------------------------------------------------------
	Helper functions.
----------------------------------------------------------------------------*/

UBOOL IpMatches( sockaddr_in& A, sockaddr_in& B )
{
#if PLATFORM_WIN32
	return	A.sin_addr.S_un.S_addr == B.sin_addr.S_un.S_addr
	&&		A.sin_port             == B.sin_port
	&&		A.sin_family           == B.sin_family;
#else
	return	A.sin_addr.s_addr      == B.sin_addr.s_addr
	&&		A.sin_port             == B.sin_port
	&&		A.sin_family           == B.sin_family;
#endif
}

void IpGetInt( in_addr Addr, DWORD& Ip )
{
#if PLATFORM_WIN32
	Ip = Addr.S_un.S_addr;
#else
	Ip = Addr.s_addr;
#endif
}

void IpSetInt( in_addr& Addr, DWORD Ip )
{
#if PLATFORM_WIN32
	Addr.S_un.S_addr = Ip;
#else
	Addr.s_addr = Ip;
#endif
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
