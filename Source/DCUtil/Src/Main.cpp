#include <stdlib.h>
#include <stdio.h>

#include "DCUtilPrivate.h"

extern CORE_API FGlobalPlatform GTempPlatform;
extern DLL_IMPORT UBOOL GTickDue;
extern "C" {HINSTANCE hInstance;}
extern "C" {char GCC_HIDDEN THIS_PACKAGE[64]="DCUtil";}

// FObjListItem.
struct FObjListItem
{
	UObject* Obj;
	INT Size;
};

static INT Compare( const FObjListItem& A, const FObjListItem& B )
{
	return A.Size - B.Size;
}

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

//
// Handle an error.
//
void FDCUtil::HandleError( const char* Exception )
{
	GIsGuarded=0;
	GIsCriticalError=1;

	debugf( NAME_Exit, "Shutting down after catching exception" );
	GObj.ShutdownAfterError();

	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	if( Exception )
		fprintf( stderr, "Fatal error: %s\n", Exception );

	abort();
}

//
// Initialize.
//
void FDCUtil::InitEngine()
{
	guard(InitEngine);

	// Platform init.
	appInit();
	GDynMem.Init( 65536 );

	// Init subsystems.
	GSceneMem.Init( 32768 );

	// Create editor engine.
	UClass* EngineClass;
	EngineClass = GObj.LoadClass( UEngine::StaticClass, NULL, "ini:Engine.Engine.EditorEngine", NULL, LOAD_NoFail | LOAD_KeepImports, NULL );

	// Init engine.
	Engine = ConstructClassObject<UEngine>( EngineClass );
	Engine->Init();

	unguard;
}

//
// Exit the engine.
//
void FDCUtil::ExitEngine()
{
	guard(ExitEngine);

	GObj.Exit();
	GMem.Exit();
	GDynMem.Exit();
	GSceneMem.Exit();
	GCache.Exit(1);

	unguard;
}

void FDCUtil::LoadPackages( const char *Dir )
{
	guard(LoadPackages);

	char Path[2048];
	appStrcpy( Path, Dir );

	if( char* Glob = appStrchr( Path, '*' ) )
	{
		char Temp[2048];
		TArray<FString> Files = appFindFiles( Path );
		*Glob = 0;
		for( INT i = 0; i < Files.Num(); ++i )
		{
			snprintf( Temp, sizeof(Temp), "%s%s", Path, *Files(i) );
			UPackage* Pkg = Cast<UPackage>( GObj.LoadPackage( nullptr, Temp, LOAD_KeepImports ) );
			if( !Pkg )
				appErrorf(  "Package '%s' does not exist", Pkg );
			LoadedPackages.Add( Temp, Pkg );
		}
	}
	else
	{
		UPackage* Pkg = Cast<UPackage>( GObj.LoadPackage( nullptr, Path, LOAD_KeepImports ) );
		if( !Pkg )
			appErrorf(  "Package '%s' does not exist", Pkg );
		LoadedPackages.Add( Path, Pkg );
	}

	unguard;
}

void FDCUtil::ParsePackageArg( const char* Arg, const char* Glob )
{
	if( !appStrcmp( Arg, "*" ) )
	{
		// Go through ALL .u and specific packages
		if( Glob )
			LoadPackages( Glob );
		LoadPackages( "../System/*.u" );
	}
	else
	{
		if( !appStrchr( Arg, '.' ) )
			appErrorf( "Specify filename with extension." );
		LoadPackages( Arg );
	}
}

void FDCUtil::ConvertTexturePkg( const FString& PkgPath, UPackage* Pkg )
{
	guard(ConvertTexturePkg);

	printf( "Converting textures in '%s'\n", Pkg->GetName() );

	// first, collect all palettes to see which ones get orphaned after we're done
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		if( !It->IsIn( Pkg ) )
			continue;
		if( It->Palette && It->Palette->IsIn( Pkg ) )
		{
			if( !( It->TextureFlags & (TF_Realtime|TF_RealtimePalette|TF_Parametric) ) )
				UnrefPalettes.AddUniqueItem( It->Palette );
		}
	}

	UBOOL Changed = false;
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		if( It->IsIn( Pkg ) )
		{
			const BYTE OldFmt = It->Format;
			const INT OldSize = It->MemUsage();
			TotalPrevSize += OldSize;
			if( FTextureConverter::AutoConvertTexture( *It ) )
			{
				const DWORD NewSize = It->MemUsage();
				Changed = true;
				printf( "- Converted '%s' from %d to %d (%d -> %d bytes)\n", It->GetName(), OldFmt, It->Format, OldSize, NewSize );
				TotalNewSize += NewSize;
			}
			if( It->Palette )
				UnrefPalettes.RemoveItem( It->Palette );
		}
	}

	if( Changed )
		ChangedPackages.Add( PkgPath, Pkg );

	unguard;
}

void FDCUtil::ConvertSoundPkg( const FString& PkgPath, UPackage* Pkg )
{
	guard(ConvertSoundPkg);

	printf( "Nuking sounds in '%s'\n", Pkg->GetName() );

	UBOOL Changed = false;
	for( TObjectIterator<USound> It; It; ++It )
	{
		// just nuke for now
		if( It->IsIn( Pkg ) && It->Data.Num() )
		{
			const DWORD Size = It->MemUsage();
			printf( "- Nuking '%s' (%u bytes)\n", It->GetName(), Size );
			TotalPrevSize += Size;
			TotalNewSize += Size - It->Data.Num();
			It->Data.Empty();
			Changed = true;
		}
	}

	if( Changed )
		ChangedPackages.Add( PkgPath, Pkg );

	unguard;
}

void FDCUtil::ConvertMusicPkg(const FString &PkgPath, UPackage *Pkg)
{
	guard(ConvertMusicPkg);

	printf( "Nuking music in '%s'\n", Pkg->GetName() );

	UBOOL Changed = false;
	for( TObjectIterator<UMusic> It; It; ++It )
	{
		// just nuke for now
		if( It->IsIn( Pkg ) && It->Data.Num() )
		{
			const DWORD Size = It->MemUsage();
			printf( "- Nuking '%s' (%u bytes)\n", It->GetName(), Size );
			TotalPrevSize += Size;
			TotalNewSize += Size - It->Data.Num();
			It->Data.Empty();
			Changed = true;
		}
	}

	if( Changed )
		ChangedPackages.Add( PkgPath, Pkg );

	unguard;
}

void FDCUtil::CommitChanges()
{
	if( UnrefPalettes.Num() )
	{
		// double check if any other packages are referencing these
		for( TObjectIterator<UTexture> It; It; ++It )
			if( It->Palette )
				UnrefPalettes.RemoveItem( It->Palette );
		// then delete remaining
		printf( "Cleaning up %d orphaned palettes\n", UnrefPalettes.Num() );
		for( INT i = 0; i < UnrefPalettes.Num(); ++i )
		{
			UnrefPalettes(i)->Colors.Empty();
			UnrefPalettes(i)->ConditionalDestroy();
			UnrefPalettes(i) = nullptr;
		}
		UnrefPalettes.Empty();
	}

	GObj.CollectGarbage( GSystem, RF_Intrinsic | RF_Standalone );

	if( ChangedPackages.Size() )
	{
		printf( "Saving %d changed packages\n", ChangedPackages.Size() );
		for( INT i = 0; i < ChangedPackages.Size(); ++i )
		{
			FString PkgName;
			UPackage* Pkg;
			ChangedPackages.GetPair( i, PkgName, Pkg );
			GObj.SavePackage( Pkg, nullptr, RF_Standalone, *PkgName );
		}
	}

	LoadedPackages.Empty();

	printf( "Total size change: %u -> %u\n", TotalPrevSize, TotalNewSize );
}

//
// Actual main function.
//
void FDCUtil::Main( )
{
	guard(Main);

	GIsRunning = 1;

	char Temp[2048] = { 0 };
	const char* Cmd = appCmdLine();
	FString PkgPath;
	UPackage* Pkg = nullptr;
	if( Parse( Cmd, "CVTUTX=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../Textures/*.utx" );
		for( INT i = 0; i < LoadedPackages.Size(); ++i )
		{
			LoadedPackages.GetPair( i, PkgPath, Pkg );
			ConvertTexturePkg( PkgPath, Pkg );
		}
		CommitChanges();
	}
	else if( Parse( Cmd, "CVTUAX=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../Sounds/*.uax" );
		for( INT i = 0; i < LoadedPackages.Size(); ++i )
		{
			LoadedPackages.GetPair( i, PkgPath, Pkg );
			ConvertSoundPkg( PkgPath, Pkg );
		}
		CommitChanges();
	}
	else if( Parse( Cmd, "CVTUMX=", Temp, sizeof( Temp ) - 1 ) )
	{
		ParsePackageArg( Temp, "../Music/*.umx" );
		for( INT i = 0; i < LoadedPackages.Size(); ++i )
		{
			LoadedPackages.GetPair( i, PkgPath, Pkg );
			ConvertMusicPkg( PkgPath, Pkg );
		}
		CommitChanges();
	}
	else if( Parse( Cmd, "LIST=", Temp, sizeof( Temp ) - 1 ) )
	{
		Pkg = Cast<UPackage>( GObj.LoadPackage( Pkg, Temp, LOAD_KeepImports ) );
		if( !Pkg )
			appThrowf( "Package '%s' does not exist", Temp );

		TArray<FObjListItem> Items;
		for( TObjectIterator<UObject> It; It; ++It )
		{
			if( It->IsIn( Pkg ) )
				Items.AddItem( { *It, It->MemUsage() } );
		}

		appSort( &Items(0), Items.Num() );

		printf( "%d objects in '%s':\n", Items.Num(), Pkg->GetName() );
		for( INT i = 0; i < Items.Num(); ++i )
			printf( "- %s: %s (%d bytes)\n", Items(i).Obj->GetPathName(), Items(i).Obj->GetClassName(), Items(i).Size );
	}
	else
	{
		printf( "Usage: dctool CVTUTX=<TEXPKG>\n" );
	}

	GIsRunning = 0;

	unguard;
}

int main( int argc, const char** argv )
{
	hInstance = NULL;
	appSetCmdLine( argc, argv );

	GIsStarted = 1;

	// Set package name.
	appStrcpy( THIS_PACKAGE, appPackage() );

	// Init mode.
	GIsServer = true;
	GIsClient = true;
	GIsEditor = true;

	appChdir( appBaseDir() );

	GExecHook = GThisExecHook;

	// Begin.
	FDCUtil DCUtil;
	try
	{
		// Start main loop.
		GIsGuarded=1;
		GSystem = &GTempPlatform;
		DCUtil.InitEngine();
		DCUtil.Main();
		DCUtil.ExitEngine();
		GIsGuarded=0;
	}
	catch( const char* Error )
	{
		// Fatal error.
		try { DCUtil.HandleError( Error ); } catch( ... ) { }
	}
	catch( ... )
	{
		// Crashed.
		try { DCUtil.HandleError( nullptr ); } catch( ... ) { }
	}

	// Shut down.
	GExecHook=NULL;
	appExit();
	GIsStarted = 0;
	return 0;
}

