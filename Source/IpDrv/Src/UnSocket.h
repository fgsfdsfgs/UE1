/*============================================================================
	UnSocket.h: Common interface for WinSock and BSD sockets.

	Revision history:
		* Created by Mike Danylchuk
============================================================================*/

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/

#ifdef PLATFORM_WIN32
typedef int socklen_t;
#else
typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct hostent HOSTENT;
typedef struct linger LINGER;
typedef struct sockaddr* LPSOCKADDR;
typedef struct hostent* LPHOSTENT;
typedef char* LPSTR;
#endif

#ifdef PLATFORM_WIN32
#define IPBYTE(A, N) A.S_un.S_un_b.s_b##N
#else
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAENOTSOCK ENOTSOCK
#define WSAEISCONN EISCONN
#define WSATRY_AGAIN TRY_AGAIN
#define WSAHOST_NOT_FOUND HOST_NOT_FOUND
#define WSANO_DATA NO_ADDRESS
#define closesocket close
#ifdef PLATFORM_PSVITA
// this is only used for FIONBIO
#define ioctlsocket( fd, opt, arg ) setsockopt( (fd), SOL_SOCKET, SO_NONBLOCK, (const void*)(arg), sizeof(*(arg)) )
#else
#define ioctlsocket ioctl
#endif
#define WSAGetLastError() errno
#define IPBYTE(A, N) ((BYTE*)&A.s_addr)[N-1]
#endif

/*----------------------------------------------------------------------------
	Functions.
----------------------------------------------------------------------------*/

UBOOL InitSockets( char* Error256 );
const char* SocketError( INT Code=-1 );
UBOOL IpMatches( sockaddr_in& A, sockaddr_in& B );
void IpGetInt( in_addr Addr, DWORD& Ip );
void IpSetInt( in_addr& Addr, DWORD Ip );

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
