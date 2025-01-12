#include <stdlib.h>
#include <ctype.h>

#include "KOSDrv.h"
#include "UnRender.h"

IMPLEMENT_CLASS( UKOSClient );

/*-----------------------------------------------------------------------------
	UKOSClient implementation.
-----------------------------------------------------------------------------*/

//
// Static init.
//
void UKOSClient::InternalClassInitializer( UClass* Class )
{
	guard(UKOSClient::InternalClassInitializer);
	if( appStricmp( Class->GetName(), "KOSClient" ) == 0 )
	{
		new(Class, "DefaultDisplay",    RF_Public)UIntProperty(CPP_PROPERTY(DefaultDisplay),     "Display",  CPF_Config );
		new(Class, "StartupFullscreen", RF_Public)UBoolProperty(CPP_PROPERTY(StartupFullscreen), "Display",  CPF_Config );
		new(Class, "UseJoystick",       RF_Public)UBoolProperty(CPP_PROPERTY(UseJoystick),       "Joystick", CPF_Config );
		new(Class, "DeadZoneXYZ",       RF_Public)UFloatProperty(CPP_PROPERTY(DeadZoneXYZ),      "Joystick", CPF_Config );
		new(Class, "DeadZoneRUV",       RF_Public)UFloatProperty(CPP_PROPERTY(DeadZoneRUV),      "Joystick", CPF_Config );
		new(Class, "ScaleXYZ",          RF_Public)UFloatProperty(CPP_PROPERTY(ScaleXYZ),         "Joystick", CPF_Config );
		new(Class, "ScaleRUV",          RF_Public)UFloatProperty(CPP_PROPERTY(ScaleRUV),         "Joystick", CPF_Config );
		new(Class, "InvertY",           RF_Public)UBoolProperty(CPP_PROPERTY(InvertY),           "Joystick", CPF_Config );
		new(Class, "InvertV",           RF_Public)UBoolProperty(CPP_PROPERTY(InvertV),           "Joystick", CPF_Config );
	}
	unguard;
}

//
// UKOSClient constructor.
//
UKOSClient::UKOSClient()
{
	guard(UKOSClient::UWindowsClient);
	DefaultDisplay = 0;
	UseJoystick = true;
	ScaleXYZ = 100.f;
	ScaleRUV = 100.f;
	DeadZoneXYZ = 0.1f;
	DeadZoneRUV = 0.1f;
	unguard;
}

//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
void UKOSClient::Init( UEngine* InEngine )
{
	guard(UKOSClient::Init);

	// Init base.
	UClient::Init( InEngine );


	unguard;
}

//
// Shut down the platform-specific viewport manager subsystem.
//
void UKOSClient::Destroy()
{
	guard(UKOSClient::Destroy);

	UClient::Destroy();

	unguard;
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occured. Not guarded.
//
void UKOSClient::ShutdownAfterError()
{
	debugf( NAME_Exit, "Executing UKOSClient::ShutdownAfterError" );

	if( Engine && Engine->Audio )
	{
		Engine->Audio->ConditionalShutdownAfterError();
	}

	for( INT i=Viewports.Num()-1; i>=0; i-- )
	{
		UKOSViewport* Viewport = (UKOSViewport*)Viewports(i);
	}

	Super::ShutdownAfterError();
}

//
// Enable or disable all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void UKOSClient::EnableViewportWindows( DWORD ShowFlags, int DoEnable )
{
	guard(UKOSClient::EnableViewportWindows);
	unguard;
}

//
// Show or hide all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void UKOSClient::ShowViewportWindows( DWORD ShowFlags, int DoShow )
{
	guard(UKOSClient::ShowViewportWindows);
	unguard;
}

//
// Configuration change.
//
void UKOSClient::PostEditChange()
{
	guard(UKOSClient::PostEditChange);
	Super::PostEditChange();
	unguard;
}

// Perform background processing.  Should be called 100 times
// per second or more for best results.
//
void UKOSClient::Poll()
{
	guard(UKOSClient::Poll);
	unguard;
}

//
// Perform timer-tick processing on all visible viewports.  This causes
// all realtime viewports, and all non-realtime viewports which have been
// updated, to be blitted.
//
void UKOSClient::Tick()
{
	guard(UKOSClient::Tick);

	// Process input and blit any viewports that need blitting.
	UKOSViewport* BestViewport = NULL;
	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UKOSViewport* Viewport = CastChecked<UKOSViewport>(Viewports(i));
		if( !Viewport->GetWindow() )
		{
			// Window was closed via close button.
			delete Viewport;
			return;
		}
		else if (	Viewport->IsRealtime()
			&&	(Viewport==FullscreenViewport || FullscreenViewport==NULL)
			&&	Viewport->SizeX && Viewport->SizeY && !Viewport->OnHold
			&&	(!BestViewport || Viewport->LastUpdateTime<BestViewport->LastUpdateTime) )
		{
			BestViewport = Viewport;
		}
		// Tick input for this viewport and see if it wants to die.
		if( Viewport->TickInput() )
		{
			delete Viewport;
			return;
		}
	}

	if( BestViewport )
		BestViewport->Repaint();

	unguard;
}

//
// Create a new viewport.
//
UViewport* UKOSClient::NewViewport( class ULevel* InLevel, const FName Name )
{
	guard(UKOSClient::NewViewport);
	return new( GObj.GetTransientPackage(), Name )UKOSViewport( InLevel, this );
	unguard;
}

//
// Return the current viewport.  Returns NULL if no viewport has focus.
//
UViewport* UKOSClient::CurrentViewport()
{
	guard(UKOSClient::CurrentViewport);

	return FullscreenViewport;

	unguard;
}

//
// Command line.
//
UBOOL UKOSClient::Exec( const char* Cmd, FOutputDevice* Out )
{
	guard(UKOSClient::Exec);

	if( ParseCommand( &Cmd, "EndFullscreen" ) )
	{
		EndFullscreen();
		return 1;
	}
	else if( ParseCommand( &Cmd, "GetRes" ) )
	{
		Out->Log( "640x480" );
		return 1;
	}
	else if( UClient::Exec( Cmd, Out ) )
	{
		return 1;
	}

	return 0;

	unguard;
}

void UKOSClient::EndFullscreen()
{
	UKOSViewport* Viewport = Cast<UKOSViewport>(FullscreenViewport);
	if( Viewport )
	{
		Viewport->EndFullscreen();
	}
	FullscreenViewport = NULL;
}

//
// Try switching to a new rendering device.
//
void UKOSClient::TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen )
{
	guard(UKOSClient::TryRenderDevice);

	// Shut down current rendering device.
	if( Viewport->RenDev )
	{
		Viewport->RenDev->Exit();
		delete Viewport->RenDev;
		Viewport->RenDev = NULL;
	}

	// Find device driver.
	UClass* RenderClass = GObj.LoadClass( URenderDevice::StaticClass, NULL, ClassName, NULL, LOAD_KeepImports, NULL );
	if( RenderClass )
	{
		Viewport->RenDev = ConstructClassObject<URenderDevice>( RenderClass );
		if( Viewport->Client->Engine->Audio && !GIsEditor )
			Viewport->Client->Engine->Audio->SetViewport( NULL );
		if( Viewport->RenDev->Init( Viewport ) )
		{
			Viewport->Actor->XLevel->DetailChange( Viewport->RenDev->HighDetailActors );
			if( Fullscreen && !Viewport->Client->FullscreenViewport )
				Viewport->MakeFullscreen( Viewport->Client->ViewportX, Viewport->Client->ViewportY, 1 );
		}
		else
		{
			debugf( NAME_Log, LocalizeError("Failed3D") );
			delete Viewport->RenDev;
			Viewport->RenDev = NULL;
		}
		if( Viewport->Client->Engine->Audio && !GIsEditor )
			Viewport->Client->Engine->Audio->SetViewport( Viewport );
	}

	unguard;
}
