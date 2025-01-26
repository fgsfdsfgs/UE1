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
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define WSAEWOULDBLOCK EAGAIN // EWOULDBLOCK?
#define WSAENOTSOCK ENOTSOCK
#define WSAEISCONN EISCONN
#define WSATRY_AGAIN TRY_AGAIN
#define WSAHOST_NOT_FOUND HOST_NOT_FOUND
#define WSANO_DATA NO_DATA
#define closesocket close
#define ioctlsocket ioctl
#define WSAGetLastError() errno
#define IPBYTE(A, N) ((BYTE*)&A.s_addr)[N-1]
#endif

#ifdef PLATFORM_DREAMCAST

#include <kos/net.h>
#include <ppp/ppp.h>
#include <dc/modem/modem.h>

struct linger {
	int l_onoff;
	int l_linger;
};

#define ESOCKTNOSUPPORT 44
#define ESHUTDOWN 58
#define EUSERS 68
#define EREMOTE 71
#define FIONBIO O_NONBLOCK
#undef ioctlsocket

static inline int ioctlsocket( int fd, int opt, void* val )
{
	int flags = fcntl( fd, F_GETFL, 0 );
	return fcntl( fd, F_SETFL, flags | opt );
}

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
