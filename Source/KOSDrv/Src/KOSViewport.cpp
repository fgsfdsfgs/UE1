#include <string.h>
#include <ctype.h>
#include <kos.h>
#include "GL/glkos.h"

#include "KOSDrv.h"
#include "UnRender.h"

IMPLEMENT_CLASS( UKOSViewport );

/*-----------------------------------------------------------------------------
	UKOSViewport implementation.
-----------------------------------------------------------------------------*/

const BYTE UKOSViewport::JoyBtnMap[MAX_JOY_BTNS] =
{
	/* CONT_C           */ IK_None,   // IK_Joy5,
	/* CONT_B           */ IK_Space,  // IK_Joy2,
	/* CONT_A           */ IK_Enter,  // IK_Joy1,
	/* CONT_START       */ IK_Escape, // IK_Joy7,
	/* CONT_DPAD_UP     */ IK_Up,     // IK_JoyPovUp,
	/* CONT_DPAD_DOWN   */ IK_Down,   // IK_JoyPovDown,
	/* CONT_DPAD_LEFT   */ IK_Left,   // IK_JoyPovLeft,
	/* CONT_DPAD_RIGHT  */ IK_Right,  // IK_JoyPovRight,
	/* CONT_Z           */ IK_Joy6,
	/* CONT_Y           */ IK_Shift,  // IK_Joy4,
	/* CONT_X           */ IK_Ctrl,   // IK_Joy3,
	/* CONT_D           */ IK_Joy8,
	/* CONT_DPAD2_UP    */ IK_Joy9,
	/* CONT_DPAD2_DOWN  */ IK_Joy10,
	/* CONT_DPAD2_LEFT  */ IK_Joy11,
	/* CONT_DPAD2_RIGHT */ IK_Joy12,
};

BYTE UKOSViewport::KeyMap[MAX_KBD_KEYS];

//
// Scancode -> EInputKey translation map.
//
void UKOSViewport::InitKeyMap()
{
	#define INIT_KEY_RANGE( AStart, AEnd, BStart, BEnd ) \
		for( DWORD Key = AStart; Key <= AEnd; ++Key ) KeyMap[Key] = BStart + ( Key - AStart )

	appMemset( KeyMap, 0, sizeof( KeyMap ) );

	INIT_KEY_RANGE( 0x04, 0x1d, IK_A, IK_Z );
	INIT_KEY_RANGE( 0x1e, 0x26, IK_1, IK_9 );
	INIT_KEY_RANGE( 0x3a, 0x45, IK_F1, IK_F12 );

	KeyMap[0x27] = IK_0;
	KeyMap[0x28] = IK_Enter;
	KeyMap[0x29] = IK_Escape;
	KeyMap[0x2a] = IK_Backspace;
	KeyMap[0x2b] = IK_Tab;
	KeyMap[0x2c] = IK_Space;
	KeyMap[0x35] = IK_Tilde;
	KeyMap[0x4f] = IK_Right;
	KeyMap[0x50] = IK_Left;
	KeyMap[0x51] = IK_Down;
	KeyMap[0x52] = IK_Up;

	#undef INIT_KEY_RANGE
}

//
// Static init.
//
void UKOSViewport::InternalClassInitializer( UClass* Class )
{
	guard(UKOSViewport::InternalClassInitializer);

	InitKeyMap();

	unguard;
}

//
// Constructor.
//
UKOSViewport::UKOSViewport( ULevel* InLevel, UKOSClient* InClient )
:	UViewport( InLevel, InClient )
,	Client( InClient )
{
	guard(UKOSViewport::UKOSViewport);

	ColorBytes = 2;
	Caps = 0;

	// Init input.
	if( GIsEditor )
		Input->Init( this, GSystem );

	Destroyed = false;
	QuitRequested = false;

	unguard;
}

// UObject interface.
void UKOSViewport::Destroy()
{
	guard(UKOSViewport::Destroy);

	if( Client->FullscreenViewport == this )
	{
		Client->FullscreenViewport = NULL;
	}
	UViewport::Destroy();

	unguard;
}

//
// Set the mouse cursor according to Unreal or UnrealEd's mode, or to
// an hourglass if a slow task is active. Not implemented.
//
void UKOSViewport::SetModeCursor()
{
	guard(UKOSViewport::SetModeCursor);
	unguard;
}

//
// Update user viewport interface.
//
void UKOSViewport::UpdateWindow()
{
	guard(UKOSViewport::UpdateViewportWindow);

	unguard;
}

//
// Open a viewport window.
//
void UKOSViewport::OpenWindow( void* InParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY )
{
	guard(UKOSViewport::OpenWindow);
	check(Actor);
	check(!OnHold);
	UBOOL DoRepaint=0, DoSetActive=0;
	UBOOL NoHard=ParseParam( appCmdLine(), "nohard" );
	NewX = Align(NewX,4);

	// User window of launcher if no parent window was specified.
	if( !InParentWindow )
	{
		QWORD ParentPtr;
		Parse( appCmdLine(), "HWND=", ParentPtr );
		InParentWindow = (void*)ParentPtr;
	}

	if( Temporary )
	{
		// Create in-memory data.
		ColorBytes = 2;
		ScreenPointer = (BYTE*)appMalloc( 2 * NewX * NewY, "TemporaryViewportData" );	
		debugf( NAME_Log, "Opened temporary viewport" );
	}
	else
	{
		ColorBytes = 2;
		if( NewX <= 320 )
		{
			vid_set_mode( DM_320x240, PM_RGB555 );
			NewX = 320;
			NewY = 240;
		}
		else if( NewX <= 640 )
		{
			vid_set_mode( DM_640x480, PM_RGB555 );
			NewX = 640;
			NewY = 480;
		}
		else
		{
			vid_set_mode( DM_768x480, PM_RGB555 );
			NewX = 768;
			NewY = 480;
		}
	}

	SizeX = NewX;
	SizeY = NewY;

	if( !RenDev && Temporary )
		Client->TryRenderDevice( this, "SoftDrv.SoftwareRenderDevice", 0 );
	if( !RenDev && !GIsEditor && !NoHard )
		Client->TryRenderDevice( this, "ini:Engine.Engine.GameRenderDevice", Client->StartupFullscreen );
	if( !RenDev )
		Client->TryRenderDevice( this, "ini:Engine.Engine.WindowedRenderDevice", 0 );
	check(RenDev);

	if( !Temporary )
		UpdateWindow();
	if( DoRepaint )
		Repaint();

	unguard;
}

//
// Close a viewport window.  Assumes that the viewport has been opened with
// OpenViewportWindow.  Does not affect the viewport's object, only the
// platform-specific information associated with it.
//
void UKOSViewport::CloseWindow()
{
	guard(UKOSViewport::CloseWindow);


	unguard;
}

//
// Lock the viewport window and set the approprite Screen and RealScreen fields
// of Viewport.  Returns 1 if locked successfully, 0 if failed.  Note that a
// lock failing is not a critical error; it's a sign that a DirectDraw mode
// has ended or the user has closed a viewport window.
//
UBOOL UKOSViewport::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize )
{
	guard(UKOSViewport::LockWindow);
	uclock(Client->DrawCycles);

	// Success.
	uunclock(Client->DrawCycles);

	return UViewport::Lock( FlashScale, FlashFog, ScreenClear, RenderLockFlags, HitData, HitSize );

	unguard;
}

//
// Unlock the viewport window.  If Blit=1, blits the viewport's frame buffer.
//
void UKOSViewport::Unlock( UBOOL Blit )
{
	guard(UKOSViewport::Unlock);

	Client->DrawCycles=0;
	uclock(Client->DrawCycles);

	// Unlock base.
	UViewport::Unlock( Blit );

	uunclock(Client->DrawCycles);

	unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void UKOSViewport::MakeCurrent()
{
	guard(UKOSViewport::MakeCurrent);
	Current = 1;
	for( INT i=0; i<Client->Viewports.Num(); i++ )
	{
		UViewport* OldViewport = Client->Viewports(i);
		if( OldViewport->Current && OldViewport != this )
		{
			OldViewport->Current = 0;
			OldViewport->UpdateWindow();
		}
	}
	UpdateWindow();
	unguard;
}

//
// Repaint the viewport.
//
void UKOSViewport::Repaint()
{
	guard(UKOSViewport::Repaint);
	if( !OnHold && RenDev && SizeX && SizeY )
		Client->Engine->Draw( this, 0 );
	unguard;
}

//
// Set the client size (viewport view size) of a viewport.
//
void UKOSViewport::SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile )
{
	guard(UKOSViewport::SetClientSize);


	SizeX = NewX;
	SizeY = NewY;

	// Optionally save this size in the profile.
	if( UpdateProfile )
	{
		Client->ViewportX = NewX;
		Client->ViewportY = NewY;
		Client->SaveConfig();
	}

	unguard;
}

//
// Return the viewport's window.
//
void* UKOSViewport::GetWindow()
{
	return (void*)-1;
}

//
// Try to make this viewport fullscreen, matching the fullscreen
// mode of the nearest x-size to the current window. If already in
// fullscreen, returns to non-fullscreen.
//
void UKOSViewport::MakeFullscreen( INT NewX, INT NewY, UBOOL UpdateProfile )
{
	guard(UKOSViewport::MakeFullscreen);

	// If someone else is fullscreen, stop them.
	if( Client->FullscreenViewport )
		Client->EndFullscreen();

	// Save this window.
	SavedX = SizeX;
	SavedY = SizeY;

	// Fullscreen rendering. For now no borderless.
	Client->FullscreenViewport = this;
	SetClientSize( NewX, NewY, false );

	if( UpdateProfile )
	{
		Client->ViewportX = NewX;
		Client->ViewportY = NewY;
		Client->SaveConfig();
	}

	unguard;
}

//
//
//
void UKOSViewport::EndFullscreen()
{
	guard(UKOSViewport::EndFullscreen);

	SetClientSize( SavedX, SavedY, false );

	unguard;
}

//
// Update input for viewport.
//
void UKOSViewport::UpdateInput( UBOOL Reset )
{
	guard(UKOSViewport::UpdateInput);


	unguard;
}

//
// If the cursor is currently being captured, stop capturing, clipping, and 
// hiding it, and move its position back to where it was when it was initially
// captured.
//
void UKOSViewport::SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL OnlyFocus )
{
	guard(UKOSViewport::SetMouseCapture);

	unguard;
}

UBOOL UKOSViewport::CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta )
{
	guard(UWindowsViewport::CauseInputEvent);

	// Route to engine if a valid key
	if( iKey > 0 )
		return Client->Engine->InputEvent( this, (EInputKey)iKey, Action, Delta );
	else
		return 0;

	unguard;
}

void UKOSViewport::TickJoystick( maple_device_t* Dev, const FLOAT DeltaTime )
{
	cont_state_t* State = (cont_state_t*)maple_dev_status( Dev );
	if( !State )
		return;

	// Emit button events
	JoyStatePrev = JoyState;
	JoyState = State->buttons;
	const DWORD Xor = JoyState ^ JoyStatePrev;
	for( DWORD Bit = 0; Bit < MAX_JOY_BTNS; ++Bit )
	{
		const DWORD Mask = ( 1U << Bit );
		if( Xor & Mask )
		{
			if( JoyState & Mask )
				CauseInputEvent( JoyBtnMap[Bit], IST_Press );
			else
				CauseInputEvent( JoyBtnMap[Bit], IST_Release );
		}
	}

	// Reset sequence
	if( ( JoyState & CONT_RESET_BUTTONS ) == CONT_RESET_BUTTONS )
		QuitRequested = true;

	// TODO: Stick/triggers
}

void UKOSViewport::TickKeyboard( maple_device_t* Dev, const FLOAT DeltaTime )
{
	kbd_state_t* State = (kbd_state_t*)maple_dev_status( Dev );
	if( !State )
		return;

	// Emit key events for the regular input system
	appMemcpy( KeyStatePrev, KeyState, sizeof( KeyState ) );
	appMemcpy( KeyState, State->matrix, sizeof( KeyState ) );
	for( INT i = 0; i < MAX_KBD_KEYS; ++i )
	{
		if( KeyMap[i] )
		{
			if( KeyState[i] && !KeyStatePrev[i] )
				CauseInputEvent( KeyMap[i], IST_Press );
			else if( !KeyState[i] && KeyStatePrev[i ])
				CauseInputEvent( KeyMap[i], IST_Release );
		}
	}

	// Emit text input
	const INT Chr = kbd_queue_pop( Dev, true );
	if( Chr > 0 ) {
		if( isprint( Chr ) || Chr == '\r' )
			Client->Engine->Key( this, (EInputKey)Chr );
	}
}

UBOOL UKOSViewport::TickInput()
{
	guard(UKOSViewport::TickInput);

	maple_device_t* Dev;
	const FLOAT CurTime = appSeconds();
	const FLOAT DeltaTime = CurTime - InputUpdateTime;

	// Check keyboard
	Dev = maple_enum_type( 0, MAPLE_FUNC_KEYBOARD );
	if ( Dev )
		TickKeyboard( Dev, DeltaTime );

	// Check joystick
	Dev = maple_enum_type( 0, MAPLE_FUNC_CONTROLLER );
	if( Dev )
		TickJoystick( Dev, DeltaTime );

	InputUpdateTime = CurTime;

	return QuitRequested;

	unguard;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

UBOOL UKOSViewport::Exec( const char* Cmd, FOutputDevice* Out )
{
	guard(UKOSViewport::Exec);
	if( UViewport::Exec( Cmd, Out ) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd, "ToggleFullscreen") )
	{
		// Toggle fullscreen.
		if( Client->FullscreenViewport )
			Client->EndFullscreen();
		else if( !(Actor->ShowFlags & SHOW_ChildWindow) )
			Client->TryRenderDevice( this, "ini:Engine.Engine.GameRenderDevice", 1 );
		return 1;
	}
	else if( ParseCommand(&Cmd, "GetCurrentRes") )
	{
		Out->Logf( "%ix%i", SizeX, SizeY );
		return 1;
	}
	else if( ParseCommand(&Cmd, "SetRes") )
	{
		INT X=appAtoi(Cmd), Y=appAtoi(appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : "");
		if( X && Y )
		{
			if( Client->FullscreenViewport )
				MakeFullscreen( X, Y, 1 );
			else
				SetClientSize( X, Y, 1 );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd, "Preferences") )
	{
		if( Client->FullscreenViewport )
			Client->EndFullscreen();
		return 1;
	}
	else return 0;
	unguard;
}
