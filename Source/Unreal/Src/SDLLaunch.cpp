#ifdef PLATFORM_SDL
#include "SDL2/SDL.h"
#endif

#ifdef PLATFORM_WIN32
#include <windows.h>
#endif

#ifdef PLATFORM_DREAMCAST
#include <kos.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#ifdef DREAMCAST_USE_FATFS
extern "C" {
#include <fatfs.h>
}
#endif
KOS_INIT_FLAGS( INIT_IRQ | INIT_THD_PREEMPT | INIT_DEFAULT_ARCH );
#endif

#include <stdio.h>

#include "Engine.h"

extern CORE_API FGlobalPlatform GTempPlatform;
extern DLL_IMPORT UBOOL GTickDue;
extern "C" {HINSTANCE hInstance;}
extern "C" {char GCC_HIDDEN THIS_PACKAGE[64]="Launch";}

// FExecHook.
class FExecHook : public FExec
{
	UBOOL Exec( const char* Cmd, FOutputDevice* Out )
	{
		return 0;
	}
};

FExecHook GLocalHook;
DLL_EXPORT FExec* GThisExecHook = &GLocalHook;

#ifdef PLATFORM_DREAMCAST

// What dbgio device was active at startup
DLL_EXPORT const char* GStartupDbgDev = nullptr;

//
// Display error and lock up.
//
void FatalError( const char* Fmt, ... ) __attribute__((noreturn));
void FatalError( const char* Fmt, ... )
{
	char Msg[2048];

	va_list Args;
	va_start( Args, Fmt );
	vsnprintf( Msg, sizeof( Msg ), Fmt, Args );
	va_end( Args );

	if( !dbgio_dev_get() || appStrcmp( dbgio_dev_get(), "fb" ) != 0 )
	{
		pvr_shutdown();
		vid_init( DM_640x480, PM_RGB555 );
		pvr_init_defaults();
		dbgio_dev_select( "fb" );
	}

	printf( "%s\n\n", Msg );

	arch_stk_trace( 2 );

	while (true)
		thd_sleep( 100 );
}

//
// Handle assertion failure.
//
void HandleAssertFail( const char* File, int Line, const char* Expr, const char* Msg, const char* Func )
{
	FatalError( "ASSERTION FAILED:\nLoc: %s:%d (%s)\nExpr: %s\n%s", File, Line, Func, Expr, Msg);
}

void HandleIrqException( irq_t Code, irq_context_t* Context, void* Data )
{
	bfont_draw_str_vram_fmt( 8, 8, true, "UNHANDLED EXCEPTION 0x%08x", Code );
	bfont_draw_str_vram_fmt( 8, 32, true, "PC: %p PR: %p", (void*)Context->pc, (void*)Context->pr );
	bfont_draw_str_vram_fmt( 8, 56, true, "SR: %p R0: %p", (void*)Context->sr, (void*)Context->r[0] );

	arch_stk_trace_at( Context->r[14], 0 );

	volatile INT Dummy = 1;
	while (Dummy);
}

#endif

//
// Handle an error.
//
void HandleError( const char* Exception )
{
	GIsGuarded=0;
	GIsCriticalError=1;
	debugf( NAME_Exit, "Shutting down after catching exception" );
	GObj.ShutdownAfterError();
	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
#ifdef PLATFORM_SDL
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, LocalizeError("Critical"), GErrorHist, SDL_GetKeyboardFocus() );
#elif defined(PLATFORM_DREAMCAST)
	if( Exception )
		FatalError( "FATAL ERROR:\n%s\n\n%s", Exception, GErrorHist );
	else
		FatalError( "FATAL ERROR:\n%s", GErrorHist );
#endif
}

//
// Initialize.
//
UEngine* InitEngine()
{
	guard(InitEngine);

	// Platform init.
	appInit();

	// Init subsystems.
#ifdef PLATFORM_LOW_MEMORY
	GDynMem.Init( 32768 );
	GSceneMem.Init( 32768 );
#else
	GDynMem.Init( 65536 );
	GSceneMem.Init( 32768 );
#endif

	// First-run menu.
	UBOOL FirstRun=0;
	GetConfigBool( "FirstRun", "FirstRun", FirstRun );

	// Create the global engine object.
	UClass* EngineClass;
	if( !GIsEditor )
	{
		// Create game engine.
		EngineClass = GObj.LoadClass( UGameEngine::StaticClass, NULL, "ini:Engine.Engine.GameEngine", NULL, LOAD_NoFail | LOAD_KeepImports, NULL );
	}
	else if( ParseParam( appCmdLine(),"MAKE" ) )
	{
		// Create editor engine.
		EngineClass = GObj.LoadClass( UEngine::StaticClass, NULL, "ini:Engine.Engine.EditorEngine", NULL, LOAD_NoFail | LOAD_DisallowFiles | LOAD_KeepImports, NULL );
	}
	else
	{
		// Editor.
		EngineClass = GObj.LoadClass( UEngine::StaticClass, NULL, "ini:Engine.Engine.EditorEngine", NULL, LOAD_NoFail | LOAD_KeepImports, NULL );
	}

	// Init engine.
	UEngine* Engine = ConstructClassObject<UEngine>( EngineClass );
	Engine->Init();

#ifdef PLATFORM_DREAMCAST
	malloc_stats();
#endif

	return Engine;

	unguard;
}

//
// Unreal's main message loop.  All windows in Unreal receive messages
// somewhere below this function on the stack.
//
void MainLoop( UEngine* Engine )
{
	guard(MainLoop);

	GIsRunning = 1;
	DOUBLE OldTime = appSeconds();
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update the world.
		DOUBLE NewTime = appSeconds();
		Engine->Tick( NewTime - OldTime );
		OldTime = NewTime;

		// Enforce optional maximum tick rate.
		INT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate )
		{
			DOUBLE Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			if( Delta > 0.0 )
				appSleep( Delta );
		}
	}
	GIsRunning = 0;
	unguard;
}

//
// Exit the engine.
//
void ExitEngine( UEngine* Engine )
{
	guard(ExitEngine);

	GObj.Exit();
	GMem.Exit();
	GDynMem.Exit();
	GSceneMem.Exit();
	GCache.Exit(1);
	appDumpAllocs( &GTempPlatform );

	unguard;
}

#ifdef PLATFORM_WIN32
INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char* InCmdLine, INT nCmdShow )
#else
int main( int argc, const char** argv )
#endif
{
#ifdef PLATFORM_WIN32
	hInstance = hInInstance;
#else
	hInstance = NULL;
	// Remember arguments since we don't have GetCommandLine().
	appSetCmdLine( argc, argv );
#endif

#ifdef PLATFORM_DREAMCAST
	// Redirect dbgio to the framebuffer if we're not already using dcload.
	GStartupDbgDev = dbgio_dev_get();
	if( !GStartupDbgDev || !appStrstr( GStartupDbgDev, "dcl" ) )
		dbgio_dev_select( "fb" );
	assert_set_handler( HandleAssertFail );
	irq_set_handler( EXC_UNHANDLED_EXC, HandleIrqException, nullptr );
#ifdef DREAMCAST_USE_FATFS
	if( fs_fat_mount_sd() == 0 )
	{
		printf( "SD card found, will try to load data from there\n" );
	}
	else
	{
		// failed
		printf( "SD card not found, will default to CD\n" );
		sd_shutdown();
		fs_fat_shutdown();
	}
#endif
#endif

	GIsStarted = 1;

	// Set package name.
	appStrcpy( THIS_PACKAGE, appPackage() );

	// Init mode.
	GIsServer = 1;
	GIsClient = !ParseParam(appCmdLine(),"SERVER") && !ParseParam(appCmdLine(),"MAKE");
	GIsEditor = ParseParam(appCmdLine(),"EDITOR") || ParseParam(appCmdLine(),"MAKE");

	// Init windowing.
	printf( "base directory: %s\n", appBaseDir() );
	appChdir( appBaseDir() );

	// Init log.
	// TODO: GLog
	GExecHook = GThisExecHook;

	// Begin.
#ifndef _DEBUG
	try
	{
#endif
		// Start main loop.
		GIsGuarded=1;
		GSystem = &GTempPlatform;
		UEngine* Engine = InitEngine();
		if( !GIsRequestingExit )
			MainLoop( Engine );
		ExitEngine( Engine );
		GIsGuarded=0;
#ifndef _DEBUG
	}
	catch( const char* Error )
	{
		// Fatal error.
		try { HandleError( Error ); } catch( ... ) { }
	}
	catch( ... )
	{
		// Crashed.
		try { HandleError( nullptr ); } catch( ... ) { }
	}
#endif

	// Shut down.
	GExecHook=NULL;
	appExit();
	GIsStarted = 0;
	return 0;
}
