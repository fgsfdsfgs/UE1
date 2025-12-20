/*=============================================================================
	UnThread.cpp: Platform-specific multithreading routines.
	Copyright 2024 fgsfds
=============================================================================*/

#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "CorePrivate.h"

#if defined(PLATFORM_PSVITA) || defined(PLATFORM_PSP)
#define pthread_setname_np(t, n) do { } while(0)
#endif

CORE_API UTHREAD appThreadSpawn( THREAD_FUNC Func, void* Arg, const char* Name, UBOOL bDetach, DWORD* OutThreadId )
{
	guard(appThreadSpawn);
	check(Func);

#ifdef PLATFORM_WIN32
	return (UTHREAD)CreateThread( NULL, 0, Func, Arg, 0, OutThreadId );
#else
	static DWORD ThreadId = 0;
	pthread_attr_t Attr;
	pthread_t* Thread = (pthread_t*)appMalloc( sizeof(pthread_t), Name );
	check(Thread);
	appMemset( (void*)Thread, 0, sizeof(*Thread) );

	pthread_attr_init( &Attr );
	if( bDetach )
		pthread_attr_setdetachstate( &Attr, PTHREAD_CREATE_DETACHED );

	if( pthread_create( Thread, &Attr, Func, Arg ) != 0 )
	{
		appFree( (void*)Thread );
		Thread = nullptr;
	}
	else
	{
		pthread_setname_np( *Thread, Name );
		if( OutThreadId )
			*OutThreadId = ++ThreadId;
	}

	return (UTHREAD)Thread;
#endif

	unguard;
}

CORE_API THREAD_RET appThreadJoin( UTHREAD Thread )
{
	guard(appThreadJoin);
	check(Thread);

#ifdef PLATFORM_WIN32
	DWORD Code = 0;
	WaitForSingleObjectEx( (void*)Thread, INFINITE, false );
	GetExitCodeThread( (void*)Thread, &Code );
	return (THREAD_RET)Code;
#else
	void* Result = nullptr;
	pthread_join( *(pthread_t*)Thread, &Result );
	appFree( (void*)Thread );
	return (THREAD_RET)Result;
#endif

	unguard;
}

CORE_API UMUTEX appMutexCreate( const char* Name )
{
	guard(appMutexCreate);

#ifdef PLATFORM_WIN32
	LPCRITICAL_SECTION CS = (LPCRITICAL_SECTION)appMalloc( sizeof(CRITICAL_SECTION), Name );
	check(CS);
	// CS is recursive, so we should be good.
	// Set the spin count to 2000 to maybe slightly improve performance on multi-processor systems.
	InitializeCriticalSectionAndSpinCount( CS, 2000 );
	return (UMUTEX)CS;
#else
	pthread_mutexattr_t Attr;
	pthread_mutex_t* Mutex = (pthread_mutex_t*)appMalloc( sizeof(pthread_mutex_t), Name );
	check(Mutex);
	appMemset( (void*)Mutex, 0, sizeof(*Mutex) );
	pthread_mutexattr_init( &Attr );
	pthread_mutexattr_settype( &Attr, PTHREAD_MUTEX_RECURSIVE );
	if( pthread_mutex_init( Mutex, &Attr ) != 0)
	{
		appFree( (void*)Mutex );
		Mutex = nullptr;
	}
	return (UMUTEX)Mutex;
#endif

	unguard;
}

CORE_API UBOOL appMutexLock( UMUTEX Mutex )
{
	check(Mutex);

#ifdef PLATFORM_WIN32
	EnterCriticalSection( (LPCRITICAL_SECTION)Mutex );
	return true;
#else
	return pthread_mutex_lock( (pthread_mutex_t*)Mutex ) == 0;
#endif
}

CORE_API UBOOL appMutexUnlock( UMUTEX Mutex )
{
	check(Mutex);

#ifdef PLATFORM_WIN32
	LeaveCriticalSection( (LPCRITICAL_SECTION)Mutex );
	return true;
#else
	return pthread_mutex_unlock( (pthread_mutex_t*)Mutex ) == 0;
#endif
}

CORE_API void appMutexFree( UMUTEX Mutex )
{
	guard(appMutexFree);
	check(Mutex);

#ifdef PLATFORM_WIN32
	DeleteCriticalSection( (LPCRITICAL_SECTION)Mutex );
#else
	pthread_mutex_destroy( (pthread_mutex_t*)Mutex );
#endif
	appFree( (void*)Mutex );

	unguard;
}
