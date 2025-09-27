/*=============================================================================
	UnPlat.cpp: Platform-specific routines-specific routines.
	Copyright 1997 Epic MegaGames, Inc. This software is a trade secret.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if defined(PLATFORM_SDL)
#include "SDL2/SDL.h"
#elif defined(PLATFORM_MSVC)
#pragma warning( disable : 4201 )
#include <direct.h>
#include <io.h>
#else
#error "Unsupported platform."
#endif

#ifdef PLATFORM_WIN32
#include <windows.h>
#include <commctrl.h>
#include <intrin.h>
#include <sys/utime.h>
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef UNREAL_STATIC
#include <dlfcn.h>
#endif
#include <fcntl.h>
#include <utime.h>
#include <sys/time.h>
#ifdef PLATFORM_X86
#include <cpuid.h>
#endif
#endif

#ifndef PLATFORM_MSVC
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <float.h>
#include <time.h>
#include <sys/stat.h>

#include <new>
#include <memory>

#include "Core.h"

#ifndef MAX_COMPUTERNAME_LENGTH
#define MAX_COMPUTERNAME_LENGTH 256
#endif

CORE_API FGlobalPlatform GTempPlatform;
INT GSlowTaskCount=0;
FILE* GLogFile=NULL;
char GLogFname[256]="";

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

CORE_API DWORD hWndMain=0, hWndProgressBar=0, hWndProgressText=0, hWndCallback=0;
static void UnrealAllocationErrorHandler( );

static void Recurse()
{
	guard(Recurse);
	Recurse();
	unguard;
}

/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

void USystem::InternalClassInitializer( UClass* Class )
{
	guard(USystem::InternalClassInitializer);
	if( appStricmp(Class->GetName(),"System")==0 )
	{
		(new(Class,"PurgeCacheDays",      RF_Public)UIntProperty   (CPP_PROPERTY(PurgeCacheDays    ), "Options", CPF_Config ));
		(new(Class,"Suppress",            RF_Public)UNameProperty  (CPP_PROPERTY(Suppress          ), "Options", CPF_Config ))->ArrayDim = 16;
		(new(Class,"Paths",               RF_Public)UStringProperty(CPP_PROPERTY(Paths             ), "Options", CPF_Config, 96 ))->ArrayDim = 16;
		(new(Class,"SavePath",            RF_Public)UStringProperty(CPP_PROPERTY(SavePath          ), "Options", CPF_Config, 96 ));
		(new(Class,"CachePath",           RF_Public)UStringProperty(CPP_PROPERTY(CachePath         ), "Options", CPF_Config, 96 ));
		(new(Class,"CacheExt",            RF_Public)UStringProperty(CPP_PROPERTY(CacheExt          ), "Options", CPF_Config, 32 ));
	}
	unguard;
}
UBOOL USystem::Exec( const char* Cmd, FOutputDevice* Out )
{
	return 0;
}
IMPLEMENT_CLASS(USystem);

/*-----------------------------------------------------------------------------
	FGlobalPlatform Command line.
-----------------------------------------------------------------------------*/

UBOOL FGlobalPlatform::Exec( const char* Cmd, FOutputDevice* Out )
{
	guard(FGlobalPlatform::Exec);
	const char* Str = Cmd;

	if( ParseCommand(&Str,"MEMSTAT") )
	{
#ifdef PLATFORM_WIN32
		MEMORYSTATUS B; B.dwLength = sizeof(B);
		GlobalMemoryStatus(&B);
		Out->Logf( "Memory available: Phys=%iK Pagef=%iK Virt=%iK", B.dwAvailPhys/1024, B.dwAvailPageFile/1024, B.dwAvailVirtual/1024 );
		Out->Logf( "Memory load = %i%%", B.dwMemoryLoad );
#else
		Out->Logf( "Not available on this platform." );
#endif
		return 1;
	}
	else if( ParseCommand(&Str,"EXIT") )
	{
		Out->Log( "Closing by request" );
		appRequestExit();
		return 1;
	}
	else if( ParseCommand(&Str,"APP") )
	{
		if( ParseCommand(&Str,"SET") )
		{
			Parse( Str, "PROGRESSBAR=",  hWndProgressBar );
			Parse( Str, "PROGRESSTEXT=", hWndProgressText );
			return 1;
		}
		else return 0;
	}
	else if( ParseCommand( &Cmd, "RELAUNCH" ) )
	{
		Out->Logf( "Relaunch: %s", Cmd );
#ifdef PLATFORM_WIN32
		char ThisFile[256];
		GetModuleFileName( NULL, ThisFile, ARRAY_COUNT(ThisFile) );
		ShellExecute( NULL, "open", ThisFile, Cmd, appBaseDir(), SW_SHOWNORMAL );
#endif
		appRequestExit();
		return 1;
	}
	else if( ParseCommand( &Cmd, "DEBUG" ) )
	{
		if( ParseCommand(&Cmd,"CRASH") )
		{
			appErrorf( "%s", "Unreal crashed at your request" );
			return 1;
		}
		else if( ParseCommand( &Cmd, "GPF" ) )
		{
			Out->Log("Unreal crashing with voluntary GPF");
			*(int *)NULL = 123;
			return 1;
		}
		else if( ParseCommand( &Cmd, "RECURSE" ) )
		{
			Out->Logf( "Recursing" );
			Recurse();
			return 1;
		}
		else if( ParseCommand( &Cmd, "EATMEM" ) )
		{
			Out->Log("Eating up all available memory");
			while( 1 )
			{
				void* Eat = appMalloc(65536,"EatMem");
				memset( Eat, 0, 65536 );
			}
			return 1;
		}
		else return 0;
	}
	else return 0; // Not executed.
	unguard;
}

/*-----------------------------------------------------------------------------
	Exit.
-----------------------------------------------------------------------------*/

CORE_API void appRequestExit()
{
	guard(appRequestExit);
	debugf("appRequestExit");
#if defined(PLATFORM_MSVC)
	PostQuitMessage( 0 );
#elif defined(PLATFORM_SDL)
	SDL_Event Ev;
	Ev.type = SDL_QUIT;
	Ev.quit.timestamp = SDL_GetTicks();
	SDL_PushEvent( &Ev );
#endif
	GIsRequestingExit=1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

CORE_API void ClipboardCopy( const char* Str )
{
	guard(ClipboardCopy);
#if defined(PLATFORM_MSVC)
	if( OpenClipboard(GetActiveWindow()) )
	{
		void* Data = GlobalAlloc( GMEM_DDESHARE, strlen(Str)+1 );
		strcpy( (char*)Data, Str );
		verify(EmptyClipboard());
		verify(SetClipboardData( CF_TEXT, Data ));
		verify(CloseClipboard());
	}
#elif defined(PLATFORM_SDL)
	SDL_SetClipboardText( Str );
#endif
	unguard;
}

CORE_API void ClipboardPaste( FString& Result )
{
	guard(ClipboardPasteString);
#if defined(PLATFORM_MSVC)
	if( OpenClipboard(GetActiveWindow()) )
	{
		void* V=GetClipboardData( CF_TEXT );
		if( V )
			Result = (char*)V;
		else
			Result = "";
		verify(CloseClipboard());
	}
	else
#elif defined(PLATFORM_SDL)
	if( SDL_HasClipboardText() )
	{
		char* Text = SDL_GetClipboardText();
		Result = Text;
		SDL_free( Text );
	}
	else
#endif
	{
		Result="";
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Machine state info.
-----------------------------------------------------------------------------*/

//
// Intel CPUID.
//
static void FGlobalPlatform_CPUID( int i, DWORD *A, DWORD *B, DWORD *C, DWORD *D )
{
#if !defined(PLATFORM_X86)
	*A = *B = *C = *D = 0;
#elif defined(PLATFORM_WIN32)
	int info[4] = { 0, 0, 0, 0 };
	__cpuid( info, i );
	*A = info[0];
	*B = info[1];
	*C = info[2];
	*D = info[3];
#else
	__get_cpuid( i, (unsigned int*)A, (unsigned int*)B, (unsigned int*)C, (unsigned int*)D );
#endif
}

/*-----------------------------------------------------------------------------
	Log.
-----------------------------------------------------------------------------*/

#ifndef PLATFORM_WIN32
static const char* _strdate( char* Buf )
{
	const time_t Now = time( NULL );
	strftime( Buf, 31, "%D", localtime( &Now ) );
	return Buf;
}
static const char* _strtime( char* Buf )
{
	const time_t Now = time( NULL );
	strftime( Buf, 31, "%T", localtime( &Now ) );
	return Buf;
}
#endif

//
// Close the log file.
//
void appCloseLog()
{
	guard(appCloseLog);
	if( GLogFile )
	{
		char Time[32], Date[32], Message[256];
		appSprintf( Message, "Log file closed, %s %s", _strdate(Date), _strtime(Time) );
		debugf( NAME_Log, Message );
		appFclose( GLogFile );
		GLogFile = NULL;
	}
	unguard;
}

//
// Open the log file.
// Not guarded.
//
void appOpenLog( const char* Fname )
{
	guard(appOpenLog);
	if( GLogFile )
		appCloseLog();
	if( Fname )
	{
		strcpy( GLogFname, Fname );
	}
	else
	{
		strcpy( GLogFname, appPackage() );
		strcat( GLogFname, ".log" );
		if( GIsEditor )
			strcpy( GLogFname, "Editor.log" );
		Parse( appCmdLine(), "LOG=", GLogFname, ARRAY_COUNT(GLogFname) );
	}
	GLogFile = appFopen( GLogFname, "w+t" );
	if( GLogFile )
	{
		setvbuf( GLogFile, 0, _IONBF, 4096 );
		char Time[32], Date[32], Message[256];
		appSprintf( Message, "Log file open, %s %s", _strdate(Date), _strtime(Time) );
		debugf( NAME_Log, Message );
	}
	else debugf( NAME_Log, "Failed to open log" );
	unguard;
}

/*-----------------------------------------------------------------------------
	FGlobalPlatform init/exit.
-----------------------------------------------------------------------------*/

void appInit()
{
	// Init.
	appOpenLog( NULL );
	strcpy( GErrorHist, "General protection fault!\r\n\r\nHistory: " );

	// Command line.
	debugf( NAME_Init, "Compiled: " __DATE__ " " __TIME__ );
	debugf( NAME_Init, "Command line: %s", appCmdLine() );
	debugf( NAME_Init, "Base directory: %s", appBaseDir() );

	// Parameters.
	GIsStrict = ParseParam( appCmdLine(), "STRICT" );

	// Ini.
	char Ini[MAX_INI_NAME + 1] = "";
	char Temp[MAX_INI_NAME + 1] = "";
	appStrcpy( Ini, appBaseDir() );
	if( Parse( appCmdLine(), "INI=", Temp, ARRAY_COUNT(Temp) ) )
	{
		appStrcat( Ini, Temp );
	}
	else
	{
		appStrcat( Ini, appPackage() );
		appStrcat( Ini, ".ini" );
		// Create Package.ini from Default.ini if it doesn't exist.
		if( appFSize( Ini ) <= 0 )
		{
			char DefaultIni[MAX_INI_NAME + 1];
			snprintf( DefaultIni, MAX_INI_NAME, "%sDefault.ini", appBaseDir() );
			appCopyFile( DefaultIni, Ini );
		}
	}

	FILE* F = appFopen( Ini, "at" );
	if( F )
		appFclose( F );
	else
		GSystem->Warnf( LocalizeError("IniReadOnly"), Ini );

	// Init config cache.
	GConfigCache.Init( Ini );

	// Language.
	if( GetConfigString( "Engine.Engine", "Language", Temp, ARRAY_COUNT(Temp) ) )
		SetLanguage( Temp );

	// Computer name.
	char Comp[MAX_COMPUTERNAME_LENGTH+1]="";
	DWORD Size=MAX_COMPUTERNAME_LENGTH+1;
#if defined(PLATFORM_WIN32)
	GetComputerName( Comp, &Size );
#else
	appStrcpy( Comp, "Default" );
	Size = appStrlen( Comp );
#endif
	char *d=GComputerName;
	for( char *c=Comp; *c!=0; c++ )
		if( appIsAlnum(*c) && d<GComputerName+ARRAY_COUNT(GComputerName)-1 )
			*d++ = *c;
	*d++ = 0;
	debugf( NAME_Init, "Computer: %s", GComputerName );

	// Core initialization.
	GObj.Init();
	GMem.Init( 65536 );
	GSys = new USystem;
	GObj.AddToRoot( GSys );
	for( INT i=0; i<ARRAY_COUNT(GSys->Suppress); i++ )
		if( GSys->Suppress[i]!=NAME_None )
			GSys->Suppress[i].SetFlags( RF_Suppress );

	// Randomize.
	srand( (unsigned)time( NULL ) );

#if defined(PLATFORM_WIN32)
	// Get memory.
	MEMORYSTATUS M;
	GlobalMemoryStatus(&M);
	GPhysicalMemory=M.dwTotalPhys;
	debugf( NAME_Init, "Memory total: Phys=%iK Pagef=%iK Virt=%iK", M.dwTotalPhys/1024, M.dwTotalPageFile/1024, M.dwTotalVirtual/1024 );

	// Working set.
	DWORD WsMin=0, WsMax=0;
	GetProcessWorkingSetSize( GetCurrentProcess(), &WsMin, &WsMax );
	debugf( NAME_Init, "Working set: %X / %X", WsMin, WsMax );
	//SetProcessWorkingSetSize( GetCurrentProcess(), 32*1024*1024, 64*1024*1024 );
 
	// Check Windows version.
	OSVERSIONINFO Version; Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&Version);
	if( Version.dwPlatformId == VER_PLATFORM_WIN32_NT )
	{
		debugf( NAME_Init, "Detected: Microsoft Windows NT %u.%u (Build: %u)",
			Version.dwMajorVersion,Version.dwMinorVersion,Version.dwBuildNumber);
	}
	else if( Version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
	{
		debugf( NAME_Init, "Detected: Microsoft Windows 95 %u.%u (Build: %u)",
			Version.dwMajorVersion,Version.dwMinorVersion,Version.dwBuildNumber);
	}
	else
	{
		debugf( NAME_Init, "Detected: Windows %u.%u (Build: %u)",
			Version.dwMajorVersion,Version.dwMinorVersion,Version.dwBuildNumber);
		appErrorf( "%s", "Unreal requires Windows 95 or Windows NT" );
	}

	// CPU speed.
	LARGE_INTEGER lFreq;
	QueryPerformanceFrequency(&lFreq);
	DOUBLE Frequency = (DOUBLE)(SQWORD)(((QWORD)lFreq.LowPart) + ((QWORD)lFreq.HighPart<<32));
	check(Frequency!=0);
	GSecondsPerCycle = 1.0 / Frequency;
	debugf( NAME_Init, "CPU Timer Freq=%f Hz", (FLOAT)Frequency );

	// Get CPU info.
	SYSTEM_INFO SI;
	GetSystemInfo(&SI);
	GPageSize=SI.dwPageSize;
	GProcessorCount=SI.dwNumberOfProcessors;
	debugf( NAME_Init, "CPU Page size=%i, Processors=%i", SI.dwPageSize, SI.dwNumberOfProcessors );
#elif defined(PLATFORM_SDL)
	debugf( NAME_Init, "Detected: %s", SDL_GetPlatform() );

	// CPU speed.
	DOUBLE Frequency = SDL_GetPerformanceFrequency();
	check(Frequency!=0.0);
	GSecondsPerCycle = 1.0 / Frequency;
	debugf( NAME_Init, "CPU Timer Freq=%f Hz", (FLOAT)Frequency );

	// Get CPU info.
	GPageSize = 4096; // TODO: sysconf?
	GProcessorCount = SDL_GetCPUCount();
#endif // PLATFORM_

#if __INTEL__
	// Check processor version with CPUID.
	DWORD A=0, B=0, C=0, D=0;
	FGlobalPlatform_CPUID(0,&A,&B,&C,&D);
	char Brand[13]="", *Model, FeatStr[256]="";
	Brand[ 0] = B;
	Brand[ 1] = B>>8;
	Brand[ 2] = B>>16;
	Brand[ 3] = B>>24;
	Brand[ 4] = D;
	Brand[ 5] = D>>8;
	Brand[ 6] = D>>16;
	Brand[ 7] = D>>24;
	Brand[ 8] = C;
	Brand[ 9] = C>>8;
	Brand[10] = C>>16;
	Brand[11] = C>>24;
	Brand[12] = 0;
	FGlobalPlatform_CPUID( 1, &A, &B, &C, &D );
	switch( (A>>8) & 0x000f )
	{
		case 4:  Model="486-class processor";        break;
		case 5:  Model="Pentium-class processor";    break;
		case 6:  Model="PentiumPro-class processor"; break;
		case 7:  Model="P7-class processor";         break;
		default: Model="Unknown processor";          break;
	}
	if( (D & 0x00800000) && !ParseParam(appCmdLine(),"NOMMX") )
	{
		strcat( FeatStr, "MMX " );
		GIsMMX=1;
	}
	if( D & 0x00008000 )
	{
		strcat( FeatStr, "CMov " );
		GIsPentiumPro=1;
	}
	if( D & 0x00000001 ) strcat( FeatStr, "FPU " );
	if( D & 0x00000010 ) strcat( FeatStr, "TimeStamp " );

	// Check for K6 3D instructions.
	if( !ParseParam(appCmdLine(),"NOK6") )
	{
		FGlobalPlatform_CPUID( 0x80000000, &A, &B, &C, &D );
		if( A >= 0x80000001 )
		{
			FGlobalPlatform_CPUID( 0x80000001, &A, &B, &C, &D );
			if( D & 0x80000000 )
			{
				strcat( FeatStr, "AMD-3D " );
				GIsK63D=1;
			}
		}
	}

	// Print feature.
	debugf( NAME_Init, "CPU Detected: %s (%s)", Model, Brand );
	debugf( NAME_Init, "CPU Features: %s", FeatStr );
#endif

	// FPU.
	appEnableFastMath( 0 );

	// Handle operator new allocation errors.
	std::set_new_handler( UnrealAllocationErrorHandler );

	// Remove stray Save.tmp if it's still present for some reason.
	snprintf( Temp, sizeof(Temp), "%s" PATH_SEPARATOR "Save.tmp", PATH(GSys->SavePath) );
	appUnlink( Temp );
}

//
// Set low precision mode.
//
void appEnableFastMath( UBOOL Enable )
{
#ifdef PLATFORM_WIN32
	guard(appEnableFastMath);
	_controlfp( Enable ? (_PC_24) : (_PC_64), _MCW_PC );
	unguard;
#endif
}

//
// Shut down the platform-specific subsystem.
// Not guarded.
//
void appExit()
{
	debugf( NAME_Exit, "appExit" );
	appDumpAllocs( GSystem );
	appCloseLog();
}

/*-----------------------------------------------------------------------------
	FGlobalPlatform misc.
-----------------------------------------------------------------------------*/

#ifdef UNREAL_STATIC

//
// Load a library.
//
CORE_API void* appGetDllHandle( const char* Filename )
{
	guard(appGetDllHandle);

	char Test[1024];
	const char* PackageName = Filename;
	char* Cur;

	check(Filename);

	// Get GLoadedPackage symbol name from full path.
	while( ( Cur = appStrchr( PackageName, '/' ) ) != NULL )
		PackageName = Cur + 1;
	while( ( Cur = appStrchr( PackageName, '\\' ) ) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, "GLoaded%s", PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

	return appGetStaticExport( Test );

	unguard;
}

#else

//
// Load a library.
//
CORE_API void* appGetDllHandle( const char* Filename )
{
	guard(appGetDllHandle);
	check(Filename);

#ifdef PLATFORM_WIN32
	void* Result = (void*)LoadLibrary( Filename );
	if( Result )
	{
		char Temp[256];
		strcpy( Temp, Filename );
		strcat( Temp, DLLEXT );
		Result = (void*)LoadLibrary( Filename );
	}
#else
	char Test[1024];
	const char* PackageName = Filename;
	char* Cur;
	char* Error;
	void* Result;

	// Get GLoadedPackage symbol name from full path.
	while( ( Cur = appStrchr( PackageName, '/' ) ) != NULL )
		PackageName = Cur + 1;
	while( ( Cur = appStrchr( PackageName, '\\' ) ) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, "GLoaded%s", PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

	// Check if the library was linked to the executable.
	Result = (void*)dlopen( NULL, RTLD_NOW );
	Error = dlerror();
	if( Error != NULL )
	{
		debugf( "dlerror(): %s", Error );
	}
	else
	{
		(void*)dlsym( Result, Test );
		Error = dlerror();
		if( Error == NULL )
			return Result;
	}

	// Load the new library.
	Result = (void*)dlopen( Filename, RTLD_NOW );
	if( Result == NULL )
	{
		char Temp[1024];
		snprintf( Temp, sizeof(Temp), "%s%s%s", appBaseDir(), Filename, DLLEXT );
		Result = (void*)dlopen( Temp, RTLD_NOW );
	}
#endif

	return Result;
	unguard;
}

#endif

//
// Free a library.
//
CORE_API void appFreeDllHandle( void* DllHandle )
{
	guard(appFreeDllHandle);
	check(DllHandle);

#if defined(UNREAL_STATIC)
	// nothing
#elif defined(PLATFORM_WIN32)
	FreeLibrary( (HMODULE)DllHandle );
#else
	dlclose( DllHandle );
#endif

	unguard;
}

//
// Lookup the address of a DLL function
//
CORE_API void* appGetDllExport( void* DllHandle, const char* ProcName )
{
	guard(appGetDllExport);
	check(DllHandle);
	check(ProcName);

#if defined(UNREAL_STATIC)
	return appGetStaticExport( ProcName );
#elif defined(PLATFORM_WIN32)
	return (void*)GetProcAddress( (HMODULE)DllHandle, ProcName );
#else
	return (void*)dlsym( DllHandle, ProcName );
#endif

	unguard;
}

//
// Break the debugger.
//
void appDebugBreak()
{
	guard(appDebugBreak);

#ifdef PLATFORM_WIN32
	::DebugBreak();
#else
	__builtin_trap();
#endif

	unguard;
}

/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// Get time in seconds.
//
CORE_API DOUBLE appSeconds()
{
#ifdef PLATFORM_MSVC
	static LARGE_INTEGER ret;
	QueryPerformanceCounter(&ret);
	return (DOUBLE)ret.QuadPart * GSecondsPerCycle;
#elif defined(PLATFORM_SDL)
	return (DOUBLE)SDL_GetPerformanceCounter() * GSecondsPerCycle;
#else
	return 0;
#endif
}

CORE_API DWORD appCycles()
{
#ifdef PLATFORM_MSVC
	static LARGE_INTEGER ret;
	QueryPerformanceCounter(&ret);
	return ret.LowPart;
#elif defined(PLATFORM_SDL)
	return SDL_GetPerformanceCounter();
#else
	return 0;
#endif
}

CORE_API void appSleep( FLOAT Sec )
{
#ifdef PLATFORM_MSVC
	Sleep( Sec * 1000.f );
#else
	usleep( Sec * 1000000.f );
#endif
}

//
// Return the system time.
//
CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	guard(appSystemTime);

#ifdef PLATFORM_WIN32
	SYSTEMTIME st;
	GetLocalTime (&st);
	Year		= st.wYear;
	Month		= st.wMonth;
	DayOfWeek	= st.wDayOfWeek;
	Day			= st.wDay;
	Hour		= st.wHour;
	Min			= st.wMinute;
	Sec			= st.wSecond;
	MSec		= st.wMilliseconds;
#else
	const time_t T = time( NULL );
	const struct tm *LT = localtime( &T );
	struct timeval TV; gettimeofday( &TV, NULL );
	Year = LT->tm_year + 1900;
	Month = LT->tm_mon + 1;
	DayOfWeek = LT->tm_wday;
	Day = LT->tm_mday;
	Hour = LT->tm_hour;
	Min = LT->tm_min;
	Sec = LT->tm_sec;
	MSec = (INT)( TV.tv_usec / 1000 );
#endif

	unguard;
}

/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const char* URL, const char* Parms, char* Error256 )
{
	guard(appLaunchURL);
	Error256[0]=0;
	debugf( NAME_Log, "LaunchURL %s", URL );
#ifdef PLATFORM_WIN32
	INT Code = (INT)ShellExecute(NULL,"open",URL,Parms,"",SW_SHOWNORMAL);
	if( Code<=32 )
		appSprintf( Error256, LocalizeError("UrlFailed") );
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	FGlobalPlatform file finding.
-----------------------------------------------------------------------------*/

//
// Find a file.
//
UBOOL appFindPackageFile( const char* In, const FGuid* Guid, char* Out )
{
	guard(appFindPackageFile);

	// Don't return it if it's a library.
	if( strlen(In)>4 && stricmp( In + strlen(In) - (sizeof(DLLEXT)-1), DLLEXT )==0 )
		return 0;

	// Try file as specified.
	strcpy( Out, In );
	if( appFSize( Out ) >= 0 )
		return 1;

	// Try all of the predefined paths.
	for( DWORD i=0; i<ARRAY_COUNT(GSys->Paths)+(Guid!=NULL); i++ )
	{
		// Get directory only.
		char Temp[256];
		char* Ext;
		if( i<ARRAY_COUNT(GSys->Paths) )
		{
			if( *GSys->Paths[i]==0 )
				continue;
			strcpy( Temp, PATH(GSys->Paths[i]) );
			Ext = appStrstr(Temp,"*");
			if( Ext )
				*Ext++ = 0;
			strcpy( Out, Temp );
			strcat( Out, In );
		}
		else
		{
			strcpy( Temp, PATH(GSys->CachePath) );
			strcat( Temp, PATH_SEPARATOR );
			Ext = GSys->CacheExt;
			strcpy( Out, Temp );
			strcat( Out, Guid->String(Temp) );
		}

		// Check for file.
		UBOOL Found = 0;
		Found = (appFSize(Out)>=0);
		if( !Found && Ext )
		{
			strcat( Out, Ext );
			Found = (appFSize( Out )>=0);
		}
		if( Found )
		{
			if( i==ARRAY_COUNT(GSys->Paths) )
			{
				// Update cache access time.
				_utime( Out, NULL );
			}
			return 1;
		}
	}

	// Not found.
	return 0;
	unguard;
}

//
// Clean out the file cache.
//
CORE_API void appCleanFileCache()
{
	guard(appCleanFileCache);
	char Temp[256];

	// Delete all temporary files.
	appSprintf( Temp, "%s" PATH_SEPARATOR "*.tmp", PATH(GSys->CachePath) );
	TArray<FString> Found = appFindFiles( Temp );
	for( INT i=0; i<Found.Num(); i++ )
	{
		appSprintf( Temp, "%s" PATH_SEPARATOR "%s", PATH(GSys->CachePath), *Found(i) );
		debugf( "Deleting temporary file: %s", Temp );
		unlink( Temp );
	}

	// Delete cache files that are no longer wanted.
	appSprintf( Temp, "%s" PATH_SEPARATOR "*%s", PATH(GSys->CachePath), GSys->CacheExt );
	Found = appFindFiles( Temp );
	if( GSys->PurgeCacheDays )
	{
		for( INT i=0; i<Found.Num(); i++ )
		{
			struct _stat Buf;
			appSprintf( Temp, "%s" PATH_SEPARATOR "%s", PATH(GSys->CachePath), *Found(i) );
			if( _stat(Temp,&Buf)==0 )
			{
				time_t CurrentTime, FileTime;
				FileTime = Buf.st_mtime;
				time( &CurrentTime );
				DOUBLE DiffSeconds = difftime( CurrentTime, FileTime );
				INT    DiffDays    = DiffSeconds / 60.0 / 60.0 / 24.0;
				if( DiffDays > GSys->PurgeCacheDays )
				{
					debugf( "Purging outdated file from cache: %s (%i days old)", Temp, DiffDays );
					unlink( Temp );
				}
			}
		}
	}
	unguard;
}

//
// Create a temporary file.
//
CORE_API void appCreateTempFilename( const char* Path, char* Result256 )
{
	guard(appCreateTempFilename);
	int i=rand();
	do
		appSprintf( Result256, "%s%04X.tmp", Path, i );
	while( appFSize(Result256)>0 );
	unguard;
}

//
// Move a file and overwrite the destination.
//
CORE_API UBOOL appMoveFile( const char* Src, const char* Dest )
{
	guard(appMoveFile);

	unlink( Dest );

#ifdef PLATFORM_WIN32
	//warning: MoveFileEx is broken on Windows 95 (Microsoft bug).
	UBOOL Success = MoveFile( Src, Dest )!=0;
#else
	UBOOL Success = rename( Src, Dest )==0;
#endif

	if( !Success )
		debugf( NAME_Warning, "Error moving file '%s' to '%s'", Src, Dest );

	return Success;
	unguard;
}

//
// Copy a file and overwrite the destination.
//
CORE_API UBOOL appCopyFile( const char* Src, const char* Dest )
{
	guard(appCopyFile);

	UBOOL Success = false;

#ifdef PLATFORM_WIN32
	//warning: MoveFileEx is broken on Windows 95 (Microsoft bug).
	Success = CopyFile( Src, Dest, 0 )!=0;
#else
	FILE* FSrc = appFopen( Src, "rb" );
	FILE* FDest = appFopen( Dest, "wb" );
	if( FSrc && FDest )
	{
		BYTE Buf[8192];
		INT N;
		Success = true;
		while( ( N = appFread( (void*)Buf, 1, sizeof(Buf), FSrc ) ) > 0 )
		{
			if( appFwrite( Buf, 1, N, FDest ) <= 0 )
			{
				Success = false;
				break;
			}
		}
	}
	if( FSrc ) appFclose( FSrc );
	if( FDest ) appFclose( FDest );
#endif

	if( !Success )
		debugf( NAME_Warning, "Error copying file '%s' to '%s'", Src, Dest );

	return Success;
	unguard;
}

/*-----------------------------------------------------------------------------
	FGlobalPlatform Log routines.
-----------------------------------------------------------------------------*/

//
// Print a message on the debugging log.
//
// This code is unguarded because trapped errors will just try
// to log more errors, resulting in a recursive mess.
//
void FGlobalPlatform::WriteBinary( const void* Data, INT Length, EName Event )
{
	try
	{
		FName EventName = FName(Event);
		if( !(EventName.GetFlags() & RF_Suppress) )
		{
			INT FoundIndex=0;
#if defined(_DEBUG) && defined(PLATFORM_WIN32)
			OutputDebugString( *EventName );
			OutputDebugString( ": " );
			OutputDebugString( (char*)Data );
			OutputDebugString( "\n" );
#endif
			if( GLogFile )
			{
				fwrite( *EventName, strlen(*EventName), 1, GLogFile );
				fwrite( ": ", 2, 1, GLogFile );
				fwrite( Data, strlen((char*)Data), 1, GLogFile );
				fwrite( "\n", 1, 1, GLogFile );
			}
			if( GLogHook )
			{
				GLogHook->WriteBinary( Data, Length, Event );
			}
		}
	}
	catch( ... )
	{
		// Ignore errors here to avoid infinite recursive error reporting.
	}
}

/*-----------------------------------------------------------------------------
	FGlobalPlatform windows specific routines.
-----------------------------------------------------------------------------*/

//
// Display a warning.
//
void VARARGS FGlobalPlatform::Warnf( const char* Fmt, ... )
{
	char TempStr[4096];
	GET_VARARGS(TempStr,Fmt);

	guard(Warnf);
	debugf( NAME_Warning, "%s", TempStr );
#if defined(PLATFORM_SDL)
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_WARNING, LocalizeError("Warning"), TempStr, SDL_GetKeyboardFocus() );
#elif defined(PLATFORM_WIN32)
	::MessageBox( NULL, TempStr, LocalizeError("Warning"), MB_OK|MB_TASKMODAL );
#endif
	unguard;
}

//
// Display a yes-no question.
//
UBOOL VARARGS FGlobalPlatform::YesNof( const char* Fmt, ... )
{
	char TempStr[4096];
	GET_VARARGS(TempStr,Fmt);

	guard(YesNof);
#if defined(PLATFORM_SDL)
	SDL_MessageBoxButtonData MsgBoxBtns[] =
	{
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"  },
	};
	SDL_MessageBoxData MsgBox;
	MsgBox.window = SDL_GetKeyboardFocus();
	MsgBox.flags = SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
	MsgBox.title = LocalizeError("Question");
	MsgBox.message = TempStr;
	MsgBox.buttons = MsgBoxBtns;
	MsgBox.numbuttons = 2;
	MsgBox.colorScheme = NULL;
	INT Result = 0;
	SDL_ShowMessageBox( &MsgBox, &Result );
	return Result;
#elif defined(PLATFORM_WIN32)
	return( ::MessageBox( NULL, TempStr, LocalizeError("Question"), MB_YESNO|MB_TASKMODAL ) == IDYES);
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	FGlobalPlatform error handling.
-----------------------------------------------------------------------------*/

//
// Allocation error handler.
//
static void UnrealAllocationErrorHandler( )
{
	appErrorf( LocalizeError("OutOfMemory") );
}

//
// Immediate exit.
//
CORE_API void appForceExit()
{
	exit( 1 );
}

//
// Handle a critical error.
//warning: May be called at library startup time.
//
void appError( const char* Msg )
{
#ifdef _DEBUG
	if( GIsStarted )
	{
		debugf( NAME_Critical, "appError called while debugging:" );
		debugf( NAME_Critical, Msg );
		GObj.ShutdownAfterError();
		debugf( NAME_Critical, "Breaking debugger" );
	}
#ifdef PLATFORM_WIN32
	DebugBreak(); 
#else
	__builtin_trap();
#endif
#else
	if( GIsCriticalError )
	{
		debugf( NAME_Critical, "Error reentered: %s", Msg );
	}
	else
	{
		GIsCriticalError = 1;
		debugf( NAME_Critical, "appError called:" );
		debugf( NAME_Critical, Msg );
		GObj.ShutdownAfterError();
		strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
		strncat( GErrorHist, "\r\n\r\n", ARRAY_COUNT(GErrorHist) );
		strncat( GErrorHist, LocalizeError("History"), ARRAY_COUNT(GErrorHist) );
		strncat( GErrorHist, ": ", ARRAY_COUNT(GErrorHist) );
	}
	throw( 1 );
#endif
}

//
// Global error handler with a formatted message.
//warning: May be called at library startup time.
//
void VARARGS appErrorf( const char* Fmt, ... )
{
	char TempStr[4096];
	GET_VARARGS(TempStr,Fmt);
	appError( TempStr );
}

CORE_API void VARARGS appUnwindf( const char* Fmt, ... )
{
	GIsCriticalError = 1;

	char TempStr[4096];
	GET_VARARGS( TempStr, Fmt );

	static INT Count=0;
	if( Count++ )
		strncat( GErrorHist, " <- ", ARRAY_COUNT(GErrorHist) - 1 );
	strncat( GErrorHist, TempStr, ARRAY_COUNT(GErrorHist) - 1 );

	debugf( NAME_Critical, TempStr );
}

/*-----------------------------------------------------------------------------
	Slow task and progress bar functions.
-----------------------------------------------------------------------------*/

//
// Begin a slow task, optionally bringing up a progress bar.  Nested calls may be made
// to this function, and the dialog will only go away after the last slow task ends.
//
void FGlobalPlatform::BeginSlowTask( const char* Task, UBOOL StatusWindow, UBOOL Cancelable )
{
	guard(FGlobalPlatform::BeginSlowTask);
#ifdef PLATFORM_WIN32
	if( hWndProgressBar && hWndProgressText )
	{
		SendMessage( (HWND)hWndProgressBar, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 100) );
		SendMessage( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, (LPARAM)Task );
		UpdateWindow( (HWND)hWndProgressText );
	}
#endif
	GIsSlowTask = ++GSlowTaskCount>0;
	unguard;
}

//
// End the slow task.
//
void FGlobalPlatform::EndSlowTask()
{
	guard(FGlobalPlatform::EndSlowTask);
	check(GSlowTaskCount>0);
	GIsSlowTask = --GSlowTaskCount>0;
	unguard;
}

//
// Update the progress bar.
//
UBOOL VARARGS FGlobalPlatform::StatusUpdatef( INT Numerator, INT Denominator, const char* Fmt, ... )
{
	guard(FGlobalPlatform::StatusUpdatef);

	char TempStr[4096];
	GET_VARARGS(TempStr,Fmt);

#ifdef PLATFORM_WIN32
	if( GIsSlowTask && hWndProgressBar && hWndProgressText )
	{
		SendMessage( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, (LPARAM)TempStr );
		SendMessage( (HWND)hWndProgressBar, PBM_SETPOS, (WPARAM)(Denominator ? 100*Numerator/Denominator : 0), (LPARAM)0 );
	}
#endif

	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Profile functions.
-----------------------------------------------------------------------------*/

//
// Read a string from the profile.
//
UBOOL GetConfigString
(
	const char*	Section,
	const char*	Key,
    char*		Value,
	INT			Size,
	const char*	Filename
)
{
	guard(GetConfigString);
	return GConfigCache.GetString( Section, Key, Value, Size, Filename );
	unguard;
}

CORE_API const char* GetConfigStr( const char* Section, const char* Key, const char* Filename )
{
	guard(GetConfigStr);
	static char Result[256]="";
	GetConfigString( Section, Key, Result, ARRAY_COUNT(Result), Filename );
	return Result;
	unguard;
}

//
// Read an array of strings from the profile.
//
UBOOL GetConfigStringArray
(
	const char*			Section,
	const char*			Key,
    TArray<FString>&	Value,
	const char*			Filename
)
{
	guard(GetConfigStringArray);
	char NewKey[256], NewValue[256];
	Value.Empty();
	for( INT i=0; i<32; i++ )
	{
		appSprintf( NewKey, "%s%i", Key, i );
		if( GetConfigString( Section, NewKey, NewValue, ARRAY_COUNT(NewValue), Filename ) )
			new( Value )FString( NewValue );
		else
			break;
	}
	return Value.Num()>0;
	unguard;
}

//
// Get an integer from the profile.
//
UBOOL GetConfigInt
(
	const char*	Section,
	const char*	Key,
	INT&		Value,
	const char*	Filename
)
{
	guard(GetConfigInt);
    char Text[80]; 
    if( GetConfigString( Section, Key, Text, sizeof(Text), Filename ) )
    {
		Value = appAtoi(Text);
		return 1;
    }
    return 0;
	unguard;
}

//
// Get a floating point number from the profile.
//
UBOOL GetConfigFloat
(
	const char*	Section,
	const char* Key,
	FLOAT&		Value,
	const char* Filename
)
{
	guard(GetConfigFloat);
    char Text[80]; 
    if( GetConfigString( Section, Key, Text, sizeof(Text), Filename ) )
    {
		Value = appAtof(Text);
		return 1;
    }
    return 0;
	unguard;
}

//
// Get a boolean from the profile.
//
UBOOL GetConfigBool
(
    const char*	Section,
	const char*	Key,
	UBOOL&		Value,
	const char* Filename
)
{
	guard(GetConfigBool);
    char Text[80]; 
    if( GetConfigString( Section, Key, Text, sizeof(Text), Filename ) )
    {
        if( stricmp(Text,"true")==0 )
            Value = 1;
		else
			Value = appAtoi(Text)==1;
		return 1;
    }
    return 0;
	unguard;
}

//
// Get an entire section of key-value pairs.
// Places a list of contiguous, null-terminated strings in Result, followed by a second null character.
//
UBOOL GetConfigSection
(
	const char*		Section,
	char*			Result,
	INT				Size,
	const char*		Filename
)
{
	guard(GetConfigSection);
	return GConfigCache.GetSection( Section, Result, Size, Filename );
	unguard;
}

//
// Write a string to the profile.
//
void SetConfigString
(
    const char* Section,
    const char* Key,
    const char* Value,
    const char* Filename
)
{
	guard(SetConfigString);
	GConfigCache.SetString( Section, Key, Value, Filename );
	unguard;
}	

//
// Write an integer to the profile.
//
void SetConfigInt
(
    const char* Section,
	const char* Key,
	INT			Value,
	const char* Filename
)
{
	guard(SetConfigInt);
    char Text[30];
    appSprintf( Text, "%i", Value );
    SetConfigString( Section, Key, Text, Filename );
	unguard;
}

//
// Write a float to the profile.
//
void SetConfigFloat
(
    const char*	Section,
	const char*	Key,
	FLOAT		Value,
	const char* Filename
)
{
	guard(SetConfigFloat);
    char Text[30];
    appSprintf( Text, "%f", Value );
    SetConfigString( Section, Key, Text, Filename );
	unguard;
}

//
// Write a boolean to the profile.
//
void SetConfigBool
(
	const char* Section,
	const char* Key,
	UBOOL		Value,
	const char* Filename
)
{
	guard(SetConfigBool);
    SetConfigString( Section, Key, Value ? "True" : "False", Filename );
	unguard;
}

//
// Write all currently registered configs.
//
UBOOL SaveAllConfigs()
{
	guard(SaveAllConfigs);
	return GConfigCache.SaveAllConfigs();
	unguard;
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// Create a new globally unique identifier.
//
CORE_API FGuid appCreateGuid()
{
	guard(appCreateGuid);

	FGuid Result;
#ifdef PLATFORM_WIN32
	check( CoCreateGuid( (GUID*)&Result )==S_OK );
#else
	appGetGUID( (void*)&Result.A );
#endif
	return Result;

	unguard;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

static char* CmdLine=NULL;
static char CmdLineBuf[4096] = "";

// Set command line from argc/argv.
CORE_API void appSetCmdLine( INT Argc, const char** Argv )
{
	guard(appSetCmdLine);

	CmdLine = CmdLineBuf;
	for( INT i = 1; i < Argc; ++i )
	{
		appStrncat( CmdLineBuf, Argv[i], sizeof(CmdLineBuf) - 1 );
		if( i != Argc - 1 )
			appStrncat( CmdLineBuf, " ", sizeof(CmdLineBuf) - 1 );
	}

	unguard;
}

// Get command line.
CORE_API const char* appCmdLine()
{
	guard(appCmdLine);
#ifdef PLATFORM_WIN32
	if( !CmdLine )
	{
		CmdLine = GetCommandLine();
		if( *CmdLine=='\"' )
		{
			do CmdLine++;
			while( *CmdLine && *CmdLine!='\"' );
			CmdLine++;
		}
		else
		{
			while( *CmdLine && *CmdLine!=' ' )
				CmdLine++;
		}
		while( *CmdLine==' ' )
			CmdLine++;
	}
#endif
	return CmdLine;
	unguard;
}

// Get startup directory.
CORE_API const char* appBaseDir()
{
	static char BaseDir[1024]="";

	if( !BaseDir[0] )
	{
		// Get directory this executable was launched from.
#if defined(PLATFORM_WIN32)
		GetModuleFileName( hInstance, BaseDir, ARRAY_COUNT(BaseDir) );
		INT i;
		for (i = strlen(BaseDir) - 1; i > 0; i--)
			if (BaseDir[i - 1] == '\\' || BaseDir[i - 1] == '/')
				break;
		BaseDir[i] = 0;
#elif defined(PLATFORM_PSVITA)
		if ( getcwd( BaseDir, sizeof(BaseDir) ) )
			appStrncat( BaseDir, "/", sizeof(BaseDir) - 1 );
#elif defined(PLATFORM_SDL)
		char* BasePath = SDL_GetBasePath();
		appStrncpy( BaseDir, BasePath, sizeof(BaseDir) );
		SDL_free( BasePath );
#endif
		// Fallback to CWD.
		if ( !BaseDir[0] )
			strcpy( BaseDir, "./" );
	}

	return BaseDir;
}

// Get launch package base name.
CORE_API const char* appPackage()
{
	static char AppPackage[256]="";
	if( !AppPackage[0] )
	{
		char Tmp[1024], *End=Tmp;
#ifdef PLATFORM_WIN32
		GetModuleFileName( NULL, Tmp, ARRAY_COUNT(Tmp) );
		while( appStrchr(End,'\\') )
			End = appStrchr(End,'\\')+1;
		while( appStrchr(End,'/') )
			End = appStrchr(End,'/')+1;
		if( appStrchr(End,'.') )
			*appStrchr(End,'.') = 0;
		if( appStricmp(End,"UnrealEd")==0 )
			End="Unreal";
		appStrcpy( AppPackage, End );
#else
		appStrcpy( AppPackage, "Unreal" );
#endif
	}
	return AppPackage;
}

/*-----------------------------------------------------------------------------
	Callback for some platforms to call when the process is being suspended or resumed.
-----------------------------------------------------------------------------*/

CORE_API void appHandleSuspendResume( UBOOL bIsSuspending )
{
	// When suspending, force-write all config changes.
	if( GIsStarted && GSys && bIsSuspending )
	{
		debugf( "App might be suspending, saving configs..." );
		SaveAllConfigs();
	}
}

/*-----------------------------------------------------------------------------
	Pathnames.
-----------------------------------------------------------------------------*/

// Convert pathname to Unix format.
char* appUnixPath( const char* Path )
{
	guard(appUnixPath);
	static char Results[16][1024];
	static INT Count=0;
	char* UnixPath = Results[Count++ & 15];
	char* Cur = UnixPath;
	appStrncpy( UnixPath, Path, 1024 );
	while( Cur = strchr( Cur, '\\' ) )
		*Cur = '/';
	return UnixPath;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
