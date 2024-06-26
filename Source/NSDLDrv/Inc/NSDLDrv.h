#include "Engine.h"
#include "SDL2/SDL.h"

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#ifdef NSDLDRV_EXPORTS
#define NSDLDRV_API DLL_EXPORT
#else
#define NSDLDRV_API DLL_IMPORT
#endif


/*-----------------------------------------------------------------------------
	UNSDLViewport.
-----------------------------------------------------------------------------*/

extern NSDLDRV_API UBOOL GTickDue;

//
// A SDL2 viewport.
//
class NSDLDRV_API UNSDLViewport : public UViewport
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT( UNSDLViewport, UViewport, CLASS_Transient )
	NO_DEFAULT_CONSTRUCTOR( UNSDLViewport )

	// Constructors.
	UNSDLViewport( ULevel* InLevel, class UNSDLClient* InClient );
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

	// UNSDLViewport interface.
	void SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile );
	void EndFullscreen();
	UBOOL TickInput(); // returns true if the viewport has requested death

private:
	// Static variables.
	static BYTE KeyMap[SDL_NUM_SCANCODES]; // SDL_Scancode -> EInputKey map
	static const BYTE MouseButtonMap[6]; // SDL_BUTTON_ -> EInputKey map

	// Variables.
	class UNSDLClient* Client;
	SDL_Window* hWnd;
	SDL_Renderer* SDLRen; // for accelerated SoftDrv
	SDL_Texture* SDLTex; // for use with the above renderer
	DWORD SDLTexFormat;
	SDL_GLContext GLCtx; // for OpenGLDrv
	UBOOL Destroyed;
	INT DisplayIndex;
	SDL_Rect DisplaySize;

	// Info saved during captures and fullscreen sessions.
	INT SavedX, SavedY;

	// UNSDLViewport private methods.
	static void InitKeyMap();
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
};

/*-----------------------------------------------------------------------------
	UNSDLClient.
-----------------------------------------------------------------------------*/

//
// SDL2 implementation of the client.
//
class NSDLDRV_API UNSDLClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT( UNSDLClient, UClient, CLASS_Transient|CLASS_Config )

	// Configuration.
	INT DefaultDisplay;
	UBOOL StartupFullscreen;
	UBOOL UseJoystick;
	UBOOL DeadZoneXYZ;
	UBOOL DeadZoneRUV;
	UBOOL InvertVertical;
	FLOAT ScaleXYZ;
	FLOAT ScaleRUV;

	// Constructors.
	UNSDLClient();
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

	// UNSDLClient interface.
	void TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen );
	const TArray<SDL_Rect>& GetDisplayResolutions();
	inline SDL_GameController* GetController() { return Controller; }
	inline const SDL_DisplayMode& GetDefaultDisplayMode() const { return DefaultDisplayMode; }

private:
	// Variables.
	SDL_GameController* Controller;
	SDL_DisplayMode DefaultDisplayMode;
	TArray<SDL_Rect> DisplayResolutions;
};
