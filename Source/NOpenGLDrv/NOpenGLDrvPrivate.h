/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include "glad.h"
#include "RenderPrivate.h"

/*------------------------------------------------------------------------------------
	OpenGL rendering private definitions.
------------------------------------------------------------------------------------*/

//
// Fixed function OpenGL renderer based on OpenGLDrv and XOpenGLDrv.
//
class DLL_EXPORT UNOpenGLRenderDevice : public URenderDevice
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT(UNOpenGLRenderDevice, URenderDevice, CLASS_Config)

	// texture, lightmap, detail, fogmap
	static constexpr INT MaxTexUnits = 4;

	// Options.
	UBOOL NoFiltering;
	UBOOL UseHwPalette;
	UBOOL UseBGRA;
	UBOOL DetailTextures;
	UBOOL UseMultiTexture;
	UBOOL AutoFOV;
	UBOOL UseWindowBrightness;
	INT SwapInterval;

	// All currently cached textures.
	struct FCachedTexture
	{
		GLuint Id;
		INT BaseMip;
		INT MaxLevel;
	};
	TMap<QWORD, FCachedTexture> BindMap;
	TArray<GLuint> TexAlloc;

	struct FTexInfo
	{
		QWORD CurrentCacheID;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
	} TexInfo[MaxTexUnits];

	// Texture upload buffer;
	BYTE* Compose;
	DWORD ComposeSize;

	// Timing.
	INT BindCycles, ImageCycles, ComplexCycles, GouraudCycles, TileCycles;

	// Current state.
	FLOAT CurrentBrightness;
	DWORD CurrentPolyFlags;
	FLOAT RProjZ, Aspect;
	FLOAT RFX2, RFY2;
	FPlane ColorMod;
	FLOAT DefaultFOV;

	struct FCachedSceneNode
	{
		FLOAT FovAngle;
		FLOAT FX, FY;
		INT X, Y;
		INT XB, YB;
		INT SizeX, SizeY;
	} CurrentSceneNode;

	// Constructors.
	UNOpenGLRenderDevice();
	static void InternalClassInitializer( UClass* Class );

	// URenderDevice interface.
	virtual UBOOL Init( UViewport* InViewport ) override;
	virtual void Exit() override;
	virtual void PostEditChange() override;
	virtual void Flush() override;
	virtual UBOOL Exec( const char* Cmd, FOutputDevice* Out ) override;
	virtual void Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize ) override;
	virtual void Unlock( UBOOL Blit ) override;
	virtual void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet ) override;
	virtual void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer ) override;
	virtual void DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags ) override;
	virtual void EndFlash() override;
	virtual void GetStats( char* Result ) override;
	virtual void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 ) override;
	virtual void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2 ) override;
	virtual void PushHit( const BYTE* Data, INT Count ) override;
	virtual void PopHit( INT Count, UBOOL bForce ) override;
	virtual void ReadPixels( FColor* Pixels ) override;
	virtual void ClearZ( FSceneNode* Frame ) override;

	// UNOpenGLRenderDevice interface.
	void SetSceneNode( FSceneNode* Frame );
	void SetBlend( DWORD PolyFlags, UBOOL InverseOrder = false );
	void SetTexture( INT TMU, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias );
	void ResetTexture( INT TMU );
	void UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture );
	void EnsureComposeSize( const DWORD NewSize );
	void ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette, const UBOOL Masked, BYTE*& UploadBuf, GLenum& UploadFormat, GLenum& InternalFormat );
	void ConvertTextureMipBGRA7777( const FMipmap* Mip, BYTE*& UploadBuf, GLenum& UploadFormat, GLenum& InternalFormat );
	void UpdateSwapInterval();

	void DrawComplexSurfaceMultiTex( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet );
	void DrawComplexSurfaceSingleTex( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet );
};
