#include <kos.h>
#include "Engine.h"

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#ifdef KOSDRV_EXPORTS
#define KOSDRV_API DLL_EXPORT
#else
#define KOSDRV_API DLL_IMPORT
#endif

#define MAX_JOY_BTNS 16


/*-----------------------------------------------------------------------------
	UKOSViewport.
-----------------------------------------------------------------------------*/

extern KOSDRV_API UBOOL GTickDue;

//
// A SDL2 viewport.
//
class KOSDRV_API UKOSViewport : public UViewport
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT( UKOSViewport, UViewport, CLASS_Transient )
	NO_DEFAULT_CONSTRUCTOR( UKOSViewport )

	// Constructors.
	UKOSViewport( ULevel* InLevel, class UKOSClient* InClient );
	static void InternalClassInitializer( UClass* Class );

	// UObject interface.
	virtual void Destroy() override;

	// UViewport interface.
	virtual UBOOL Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData=NULL, INT* HitSize=0 ) override;
	virtual void Unlock( UBOOL Blit ) override;
	virtual UBOOL Exec( const char* Cmd, FOutputDevice* Out ) override;
	virtual void Repaint() override;
	virtual void SetModeCursor() override;
	virtual void UpdateWindow() override;
	virtual void OpenWindow( void* ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY ) override;
	virtual void CloseWindow() override;
	virtual void UpdateInput( UBOOL Reset ) override;
	virtual void MakeCurrent() override;
	virtual void MakeFullscreen( INT NewX, INT NewY, UBOOL UpdateProfile ) override;
	virtual void* GetWindow() override;
	virtual void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly ) override;

	// UKOSViewport interface.
	void SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile );
	void EndFullscreen();
	void TickJoystick( maple_device_t* Dev, const FLOAT DeltaTime );
	void TickKeyboard( maple_device_t* Dev, const FLOAT DeltaTime );
	UBOOL TickInput(); // returns true if the viewport has requested death

private:
	// Static variables.
	static BYTE KeyMap[MAX_KBD_KEYS]; // DC keycode -> EInputKey map
	static const BYTE JoyBtnMap[MAX_JOY_BTNS]; // DC joystick button bit -> EInputKey map

	// Variables.
	BYTE KeyState[MAX_KBD_KEYS]; // Current keys held
	BYTE KeyStatePrev[MAX_KBD_KEYS]; // Previous keys held
	DWORD JoyState;
	DWORD JoyStatePrev;
	class UKOSClient* Client;
	UBOOL Destroyed;
	UBOOL QuitRequested;
	FLOAT InputUpdateTime;

	// Info saved during captures and fullscreen sessions.
	INT SavedX, SavedY;

	// UKOSViewport private methods.
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
	static void InitKeyMap();
};

/*-----------------------------------------------------------------------------
	UKOSClient.
-----------------------------------------------------------------------------*/

//
// SDL2 implementation of the client.
//
class KOSDRV_API UKOSClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT( UKOSClient, UClient, CLASS_Transient|CLASS_Config )

	// Configuration.
	INT DefaultDisplay;
	UBOOL StartupFullscreen;
	UBOOL UseJoystick;
	UBOOL InvertY;
	UBOOL InvertV;
	FLOAT ScaleXYZ;
	FLOAT ScaleRUV;
	FLOAT DeadZoneXYZ;
	FLOAT DeadZoneRUV;

	// Constructors.
	UKOSClient();
	static void InternalClassInitializer( UClass* Class );

	// UObject interface.
	virtual void Destroy() override;
	virtual void PostEditChange() override;
	virtual void ShutdownAfterError() override;

	// UClient interface.
	virtual void Init( UEngine* InEngine ) override;
	virtual void ShowViewportWindows( DWORD ShowFlags, int DoShow ) override;
	virtual void EnableViewportWindows( DWORD ShowFlags, int DoEnable ) override;
	virtual void Poll() override;
	virtual UViewport* CurrentViewport() override;
	virtual UBOOL Exec( const char* Cmd, FOutputDevice* Out=GSystem ) override;
	virtual void Tick() override;
	virtual UViewport* NewViewport( class ULevel* InLevel, const FName Name ) override;
	virtual void EndFullscreen() override;

	// UKOSClient interface.
	void TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen );
};
