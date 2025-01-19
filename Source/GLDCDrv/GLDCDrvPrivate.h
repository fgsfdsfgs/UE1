/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include "GL/gl.h"
#include "RenderPrivate.h"

/*------------------------------------------------------------------------------------
	OpenGL rendering private definitions.
------------------------------------------------------------------------------------*/

//
// Fixed function OpenGL renderer for the Dreamcast using GLdc.
//
class DLL_EXPORT UGLDCRenderDevice : public URenderDevice
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT(UGLDCRenderDevice, URenderDevice, CLASS_Config)

	static constexpr INT MaxMipLevel = 0;
	static constexpr INT MinTexSize = 8;

	// Options.
	UBOOL NoFiltering;

	// All currently cached textures (Ptr -> GL ID).
	struct FTexBind
	{
		GLuint Tex;
		BYTE LastType;
	};
	TMap<QWORD, FTexBind> BindMap;

	struct FTexInfo
	{
		QWORD CurrentCacheID;
		FTexBind* CurrentBind;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
		UBOOL bIsTile;
	} TexInfo;

	// Texture upload buffer;
	BYTE* Compose;
	DWORD ComposeSize;

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
		UBOOL bIsSky;
	} CurrentSceneNode;

	// Constructors.
	UGLDCRenderDevice();
	static void InternalClassInitializer( UClass* Class );

	// URenderDevice interface.
	virtual UBOOL Init( UViewport* InViewport ) override;
	virtual void Exit() override;
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

	// UGLDCRenderDevice interface.
	void SetSceneNode( FSceneNode* Frame );
	void SetBlend( DWORD PolyFlags, UBOOL InverseOrder = false );
	void SetTexture( FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias );
	void ResetTexture( );
	void UploadTexture( FTextureInfo& Info, const UBOOL NewTexture );
	void EnsureComposeSize( const DWORD NewSize );
	void* ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette );
	void* ConvertTextureMipBGRA7777( const FMipmap* Mip );
	void* VerticalUpscale( const INT USize, const INT VSize, const INT VTimes );
	void PrintMemStats() const;
};
