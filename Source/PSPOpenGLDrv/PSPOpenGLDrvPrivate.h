/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include "RenderPrivate.h"

#include <GLES/egl.h>
#include <GLES/gl.h>
#include <GL/gl.h>

/*------------------------------------------------------------------------------------
	OpenGL rendering private definitions.
------------------------------------------------------------------------------------*/

//
// PSPGL version of NOpenGLDrv.
//
class DLL_EXPORT UPSPOpenGLRenderDevice : public URenderDevice
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT(UPSPOpenGLRenderDevice, URenderDevice, CLASS_Config)

	// Options.
	UBOOL NoFiltering;
	UBOOL UseHwPalette;
	UBOOL DetailTextures;
	UBOOL AutoFOV;
	INT SwapInterval;

	// All currently cached textures.
	struct FCachedTexture
	{
		GLuint Id;
	};
	TMap<QWORD, FCachedTexture> BindMap;

	struct FTexInfo
	{
		QWORD CurrentCacheID;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
	} TexInfo;

	// Temp vertex buffer.
	FLOAT VertexBuf[16384] GCC_ALIGN(64);

	// Texture upload buffer;
	_WORD ComposePal[256] GCC_ALIGN(64);
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

	struct FCachedSceneNode
	{
		FLOAT FovAngle;
		FLOAT FX, FY;
		INT X, Y;
		INT XB, YB;
		INT SizeX, SizeY;
	} CurrentSceneNode;

	// Constructors.
	UPSPOpenGLRenderDevice();
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

	// UPSPOpenGLRenderDevice interface.
	void SetSceneNode( FSceneNode* Frame );
	void SetBlend( DWORD PolyFlags, UBOOL InverseOrder = false );
	void SetTexture( FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias );
	void ResetTexture();
	void UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture );
	void EnsureComposeSize( const DWORD NewSize );
	void ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette, const UBOOL Masked, UBOOL NewTexture, BYTE*& UploadBuf, INT& USize, INT &VSize );
	void ConvertTextureMipBGRA7777( const FMipmap* Mip, BYTE*& UploadBuf, INT& USize, INT &VSize );

	void DrawComplexSurfaceSingleTex( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet );
};
