#include "SDL2/SDL.h"
#include "glad.h"
#include "glm/matrix.hpp"
#include "glm/ext/matrix_clip_space.hpp"

#include "NOpenGLESDrvPrivate.h"

/*-----------------------------------------------------------------------------
	GLSL shaders.
-----------------------------------------------------------------------------*/

static const char *FragShaderGLSL {
#include "FragmentShader.glsl.inc"
};

static const char *VertShaderGLSL {
#include "VertexShader.glsl.inc"
};

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(NOpenGLESDrv);
IMPLEMENT_CLASS(UNOpenGLESRenderDevice);

/*-----------------------------------------------------------------------------
	UNOpenGLESRenderDevice implementation.
-----------------------------------------------------------------------------*/

// MV matrix that puts the coordinate system in order
static constexpr glm::mat4 MtxModelView {
	+1.f, +0.f, +0.f, +0.f,
	+0.f, -1.f, +0.f, +0.f,
	+0.f, +0.f, -1.f, +0.f,
	+0.f, +0.f, +0.f, +1.f,
};

// in floats
static constexpr DWORD AttribSizes[AT_Count] = {
	3, 2, 2, 2, 2, 4, 4
};

// from XOpenGLDrv:
// PF_Masked requires index 0 to be transparent, but is set on the polygon instead of the texture,
// so we potentially need two copies of any palettized texture in the cache
// unlike in newer unreal versions the low cache bits are actually used, so we have use one of the
// actually unused higher bits for this purpose, thereby breaking 64-bit compatibility for now
#define MASKED_TEXTURE_TAG (1ULL << 60ULL)

// FColor is adjusted for endianness
#define ALPHA_MASK 0xff000000

// lightmaps are 0-127
#define LIGHTMAP_SCALE 2

// and it also would be nice to overbright them
#define LIGHTMAP_OVERBRIGHT 1.4f

// max vertices in a single draw call
#define MAX_VERTS 32768

void UNOpenGLESRenderDevice::InternalClassInitializer( UClass* Class )
{
	guardSlow(UNOpenGLESRenderDevice::InternalClassInitializer);
	new(Class, "NoFiltering",    RF_Public)UBoolProperty( CPP_PROPERTY(NoFiltering),    "Options", CPF_Config );
	new(Class, "Overbright",     RF_Public)UBoolProperty( CPP_PROPERTY(Overbright),     "Options", CPF_Config );
	new(Class, "DetailTextures", RF_Public)UBoolProperty( CPP_PROPERTY(DetailTextures), "Options", CPF_Config );
	new(Class, "UseVAO",         RF_Public)UBoolProperty( CPP_PROPERTY(UseVAO),         "Options", CPF_Config );
	new(Class, "UseBGRA",        RF_Public)UBoolProperty( CPP_PROPERTY(UseBGRA),        "Options", CPF_Config );
	new(Class, "AutoFOV",        RF_Public)UBoolProperty( CPP_PROPERTY(AutoFOV),        "Options", CPF_Config );
	new(Class, "SwapInterval",   RF_Public)UIntProperty ( CPP_PROPERTY(SwapInterval),   "Options", CPF_Config );
	unguardSlow;
}

UNOpenGLESRenderDevice::UNOpenGLESRenderDevice()
{
	DetailTextures = true;
	Overbright = true;
	NoFiltering = false;
	UseVAO = false;
	UseBGRA = true;
	AutoFOV = true;
	CurrentBrightness = -1.f;
	SwapInterval = 1;
	DefaultFOV = 90.0f;
}

UBOOL UNOpenGLESRenderDevice::Init( UViewport* InViewport )
{
	guard(UNOpenGLESRenderDevice::Init)

	if( !gladLoadGLES2Loader( &SDL_GL_GetProcAddress ) )
	{
		debugf( NAME_Warning, "Could not load GLES2: %s", SDL_GetError() );
		return false;
	}

	debugf( NAME_Log, "Got OpenGL %s", glGetString( GL_VERSION ) );

	NoVolumetricBlend = true;
	SupportsFogMaps = true;
	SupportsDistanceFog = true;

	UpdateSwapInterval();

	ComposeSize = 256 * 256 * 4;
	Compose = (BYTE*)appMalloc( ComposeSize, "GLComposeBuf" );
	verify( Compose );

	VtxDataSize = 18 * MAX_VERTS; // should be enough for all attributes
	VtxData = (FLOAT*)appMalloc( VtxDataSize * sizeof(FLOAT), "GLVtxDataBuf" );
	verify( VtxData );
	VtxDataEnd = VtxData + VtxDataSize;
	VtxDataPtr = VtxData;

	IdxDataSize = MAX_VERTS;
	IdxData = (GLushort*)appMalloc( IdxDataSize * sizeof(GLushort), "GLIdxDataBuf" );
	verify( IdxData );
	IdxDataEnd = IdxData + IdxDataSize;
	IdxDataPtr = IdxData;
	IdxCount = 0;

	if( UseVAO )
	{
		glGenBuffers( 1, &GLBuf );
		glBindBuffer( GL_ARRAY_BUFFER, GLBuf );
		glBufferData( GL_ARRAY_BUFFER, VtxDataSize, (void*)VtxData, GL_DYNAMIC_DRAW );
	}

	if( UseBGRA )
	{
		// check if BGRA is actually supported
		if( !( GLAD_GL_APPLE_texture_format_BGRA8888 || GLAD_GL_EXT_texture_format_BGRA8888 || GLAD_GL_MESA_bgra ) )
		{
			debugf( "GLES2: BGRA8888 enabled, but not supported; disabling" );
			UseBGRA = false;
		}
		else
		{
			debugf( "GLES2: BGRA8888 supported" );
		}
	}

	// Set permanent state.
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glBlendFunc( GL_ONE, GL_ZERO );
	glEnable( GL_BLEND );
	glEnableVertexAttribArray( 0 );

	// Precache some common shaders
	static const DWORD PrecacheShaders[] = {
		SF_VtxColor,
		SF_Texture0,
		SF_Texture0 | SF_VtxColor,
		SF_Texture0 | SF_AlphaTest,
		SF_Texture0 | SF_VtxColor | SF_AlphaTest,
		SF_Texture0 | SF_VtxColor | SF_VtxFog,
		SF_Texture0 | SF_VtxColor | SF_VtxFog | SF_AlphaTest,
		SF_Texture0 | SF_Texture1 | SF_Lightmap,
		SF_Texture0 | SF_Texture1 | SF_Lightmap | SF_AlphaTest,
		SF_Texture0 | SF_Texture1 | SF_Texture2 | SF_Lightmap | SF_Fogmap,
		SF_Texture0 | SF_Texture1 | SF_Texture2 | SF_Lightmap | SF_Fogmap | SF_AlphaTest,
	};
	for( DWORD i = 0; i < ARRAY_COUNT( PrecacheShaders ); ++i )
		CreateShader( PrecacheShaders[i] );

	CurrentPolyFlags = PF_Occlude;
	CurrentShaderFlags = 0;
	CurrentBrightness = -1.f;
	Viewport = InViewport;

	return true;
	unguard;
}

void UNOpenGLESRenderDevice::Exit()
{
	guard(UNOpenGLESRenderDevice::Exit);

	debugf( NAME_Log, "Shutting down OpenGL ES2 renderer" );

	Flush();

	if( Compose )
	{
		appFree( Compose );
		Compose = NULL;
	}
	ComposeSize = 0;

	unguard;
}

void UNOpenGLESRenderDevice::PostEditChange()
{
	guard(UNOpenGLESRenderDevice::PostEditChange)

	Super::PostEditChange();

	UpdateSwapInterval();

	unguard;
}

void UNOpenGLESRenderDevice::Flush()
{
	guard(UNOpenGLESRenderDevice::Flush);

	if( TexAlloc.Num() )
	{
		debugf( NAME_Log, "Flushing %d/%d textures", TexAlloc.Num(), BindMap.Size() );
		ResetTexture( 0 );
		ResetTexture( 1 );
		ResetTexture( 2 );
		ResetTexture( 3 );
		glFinish();
		glDeleteTextures( TexAlloc.Num(), &TexAlloc(0) );
		TexAlloc.Empty();
		BindMap.Empty();
	}

	unguard;
}

UBOOL UNOpenGLESRenderDevice::Exec( const char* Cmd, FOutputDevice* Out )
{
	return false;
}

void UNOpenGLESRenderDevice::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
{
	guard(UNOpenGLESRenderDevice::Lock);

	glClearColor( ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W );
	glClearDepthf( 1.f );
	glDepthFunc( GL_LEQUAL );

	FLOAT TargetBrightness = CurrentBrightness;
	if( Viewport && Viewport->Client )
		TargetBrightness = Viewport->Client->Brightness;
	else if( CurrentBrightness < 0.f )
		TargetBrightness = 0.5f;

	if( CurrentBrightness != TargetBrightness )
	{
		CurrentBrightness = TargetBrightness;
		UniformsChanged[UF_Brightness] = true;
	}

	SetBlend( PF_Occlude );
	SetShader( CurrentShaderFlags );

	GLbitfield ClearBits = GL_DEPTH_BUFFER_BIT;
	if( RenderLockFlags & LOCKR_ClearScreen )
		ClearBits |= GL_COLOR_BUFFER_BIT;
	glClear( ClearBits );

	if( FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f) )
		ColorMod = FPlane( FlashFog.X, FlashFog.Y, FlashFog.Z, 1.f - Min( FlashScale.X * 2.f, 1.f ) );
	else
		ColorMod = FPlane( 0.f, 0.f, 0.f, 0.f );

	if( AutoFOV && Viewport && Viewport->Actor && Viewport->Actor->DesiredFOV == 90.0f )
	{
		Viewport->Actor->DesiredFOV = DefaultFOV;
	}

	unguard;
}

void UNOpenGLESRenderDevice::Unlock( UBOOL Blit )
{
	guard(UNOpenGLESRenderDevice::Unlock);

	FlushTriangles();

	glFlush();

	unguard;
}

void UNOpenGLESRenderDevice::DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
{
	guard(UNOpenGLESRenderDevice::DrawComplexSurface);

	check(Surface.Texture);

	SetSceneNode( Frame );
	SetBlend( Surface.PolyFlags );
	SetTexture( 0, *Surface.Texture, ( Surface.PolyFlags & PF_Masked ), 0.f );
	if( Surface.LightMap )
	{
		SetTexture( 1, *Surface.LightMap, 0, -0.5f );
		CurrentShaderFlags |= SF_Lightmap;
	}
	if( Surface.FogMap )
	{
		SetTexture( 2, *Surface.FogMap, 0, -0.5f );
		CurrentShaderFlags |= SF_Fogmap;
	}
	if( Surface.DetailTexture && DetailTextures )
	{
		SetTexture( 3, *Surface.DetailTexture, 0, 0.f );
		CurrentShaderFlags |= SF_Detail;
	}
	SetShader( CurrentShaderFlags );

	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;
	for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
	{
		BeginPoly();
		for( INT i = 0; i < Poly->NumPts; i++ )
		{
			FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
			AttribFloat3( &Poly->Pts[i]->Point.X );
			AttribFloat2( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
			if( Surface.LightMap )
				AttribFloat2( (U-UDot-TexInfo[1].UPan)*TexInfo[1].UMult, (V-VDot-TexInfo[1].VPan)*TexInfo[1].VMult );
			if( Surface.FogMap )
				AttribFloat2( (U-UDot-TexInfo[2].UPan)*TexInfo[2].UMult, (V-VDot-TexInfo[2].VPan)*TexInfo[2].VMult );
			if( Surface.DetailTexture && DetailTextures )
				AttribFloat2( (U-UDot-TexInfo[3].UPan)*TexInfo[3].UMult, (V-VDot-TexInfo[3].VPan)*TexInfo[3].VMult );
			PolyVertex();
		}
		EndPoly();
	}

	CurrentShaderFlags &= ~( SF_Lightmap|SF_Fogmap|SF_Detail );

	ResetTexture( 1 );
	ResetTexture( 2 );
	ResetTexture( 3 );

	unguard;
}

void UNOpenGLESRenderDevice::DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )
{
	guard(UNOpenGLESRenderDevice::DrawGouraudPolygon);

	const UBOOL IsFog = ( ( PolyFlags & ( PF_RenderFog|PF_Translucent|PF_Modulated ) ) == PF_RenderFog );
	const UBOOL IsModulated = ( PolyFlags & PF_Modulated );
	if( !IsModulated )
		CurrentShaderFlags |= SF_VtxColor;
	if( IsFog )
		CurrentShaderFlags |= SF_VtxFog;

	SetSceneNode( Frame );
	SetBlend( PolyFlags );
	SetTexture( 0, Texture, ( PolyFlags & PF_Masked ), 0 );
	SetShader( CurrentShaderFlags );

	BeginPoly();
	for( INT i=0; i<NumPts; i++ )
	{
		FTransTexture* P = Pts[i];
		AttribFloat3( &P->Point.X );
		AttribFloat2( P->U*TexInfo[0].UMult, P->V*TexInfo[0].VMult );
		if( !IsModulated )
			AttribFloat4( P->Light.X, P->Light.Y, P->Light.Z, 1.f );
		if( IsFog )
			AttribFloat4( &P->Fog.X );
		PolyVertex();
	}
	EndPoly();

	CurrentShaderFlags &= ~( SF_VtxColor|SF_VtxFog );

	unguard;
}

void UNOpenGLESRenderDevice::DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags )
{
	guard(UNOpenGLESRenderDevice::DrawTile);

	FPlane VtxColor;
	if( !( PolyFlags & PF_Modulated ) )
	{
		VtxColor.X = Light.X;
		VtxColor.Y = Light.Y;
		VtxColor.Z = Light.Z;
		VtxColor.W = 1.f;
	}
	else
	{
		VtxColor.X = 1.f;
		VtxColor.Y = 1.f;
		VtxColor.Z = 1.f;
		VtxColor.W = 1.f;
	}

	CurrentShaderFlags |= SF_VtxColor;

	SetSceneNode( Frame );
	SetBlend( PolyFlags );
	SetTexture( 0, Texture, ( PolyFlags & PF_Masked ), 0.f );
	SetShader( CurrentShaderFlags );

	BeginPoly();
		AttribFloat3( RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z );
		AttribFloat2( U * TexInfo[0].UMult, V * TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
		AttribFloat3( RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z );
		AttribFloat2( (U + UL) * TexInfo[0].UMult, V * TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
		AttribFloat3( RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z );
		AttribFloat2( (U + UL) * TexInfo[0].UMult, (V + VL) *TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
		AttribFloat3( RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z );
		AttribFloat2( U * TexInfo[0].UMult, (V + VL) * TexInfo[0].VMult );
		AttribFloat4( &VtxColor.X );
		PolyVertex();
	EndPoly();

	CurrentShaderFlags &= ~SF_VtxColor;

	unguard;
}

void UNOpenGLESRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{

}

void UNOpenGLESRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2 )
{

}

void UNOpenGLESRenderDevice::EndFlash( )
{
	guard(UNOpenGLESRenderDevice::EndFlash);

	if( ColorMod == FPlane( 0.f, 0.f, 0.f, 0.f ) )
		return;

	CurrentShaderFlags = SF_VtxColor;
	ResetTexture( 0 );
	ResetTexture( 1 );
	ResetTexture( 2 );
	ResetTexture( 3 );
	SetBlend( PF_Highlighted );
	SetShader( CurrentShaderFlags );

	const FLOAT Z = 1.f;
	const FLOAT RFX2 = RProjZ;
	const FLOAT RFY2 = RProjZ * Aspect;

	glDisable( GL_DEPTH_TEST );

	BeginPoly();
		AttribFloat3( RFX2 * -Z, RFY2 * -Z, Z );
		AttribFloat4( &ColorMod.R );
		PolyVertex();
		AttribFloat3( RFX2 * +Z, RFY2 * -Z, Z );
		AttribFloat4( &ColorMod.R );
		PolyVertex();
		AttribFloat3( RFX2 * +Z, RFY2 * +Z, Z );
		AttribFloat4( &ColorMod.R );
		PolyVertex();
		AttribFloat3( RFX2 * -Z, RFY2 * +Z, Z );
		AttribFloat4( &ColorMod.R );
		PolyVertex();
	EndPoly();

	glEnable( GL_DEPTH_TEST );

	CurrentShaderFlags &= ~SF_VtxColor;

	unguard;
}

void UNOpenGLESRenderDevice::PushHit( const BYTE* Data, INT Count )
{

}

void UNOpenGLESRenderDevice::PopHit( INT Count, UBOOL bForce )
{

}

void UNOpenGLESRenderDevice::GetStats( char* Result )
{
	guard(UNOpenGLESRenderDevice::GetStats)

	if( Result ) *Result = '\0';

	unguard;
}

void UNOpenGLESRenderDevice::ReadPixels( FColor* Pixels )
{
	guard(UNOpenGLESRenderDevice::ReadPixels);

	glPixelStorei( GL_UNPACK_ALIGNMENT, 0 );
	glReadPixels( 0, 0, Viewport->SizeX, Viewport->SizeY, GL_RGBA, GL_UNSIGNED_BYTE, (void*)Pixels );

	// Swap RGBA -> BGRA and flip vertically.
	for( INT i=0; i<Viewport->SizeY/2; i++ )
	{
		for( INT j=0; j<Viewport->SizeX; j++ )
		{
			Exchange( Pixels[j+i*Viewport->SizeX].R, Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX].B );
			Exchange( Pixels[j+i*Viewport->SizeX].G, Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX].G );
			Exchange( Pixels[j+i*Viewport->SizeX].B, Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX].R );
		}
	}

	unguard;
}

void UNOpenGLESRenderDevice::ClearZ( FSceneNode* Frame )
{
	guard(UNOpenGLESRenderDevice::ClearZ);

	FlushTriangles();
	SetBlend( PF_Occlude );
	SetShader( CurrentShaderFlags );

	glClear( GL_DEPTH_BUFFER_BIT );

	unguard;
}

void UNOpenGLESRenderDevice::UpdateUniforms()
{
	guard(UNOpenGLESRenderDevice::UpdateUniforms);

	if( UniformsChanged[UF_Mtx] )
	{
		FlushTriangles();
		glUniformMatrix4fv( ShaderInfo->Uniforms[UF_Mtx], 1, GL_FALSE, &MtxMVP[0][0] );
		UniformsChanged[UF_Mtx] = false;
	}

	if( UniformsChanged[UF_Brightness] )
	{
		FlushTriangles();
		glUniform1f( ShaderInfo->Uniforms[UF_Brightness], CurrentBrightness );
		UniformsChanged[UF_Brightness] = false;
	}

	for( INT i = UF_Texture0; i <= UF_Texture3; ++i )
	{
		if( UniformsChanged[i] && ShaderInfo->Uniforms[i] >= 0 )
		{
			glUniform1i( ShaderInfo->Uniforms[i], i - UF_Texture0 );
			UniformsChanged[i] = false;
		}
	}

	unguard;
}

GLuint UNOpenGLESRenderDevice::CompileShader( GLenum Type, const char* Text )
{
	guard(UNOpenGLESRenderDevice::CompileShader);

	GLuint Id = glCreateShader( Type );

	const char *Src[] = { Text, NULL };
	glShaderSource( Id, 1, Src, NULL );

	glCompileShader( Id );

	GLint Status = 0;
	glGetShaderiv( Id, GL_COMPILE_STATUS, &Status );
	if( !Status )
	{
		char Tmp[2048] = { 0 };
		glGetShaderInfoLog( Id, sizeof(Tmp), NULL, Tmp );
		appErrorf( "%s shader compilation failed:\n%s", ( Type == GL_FRAGMENT_SHADER ) ? "Fragment" : "Vertex", Tmp );
	}

	return Id;

	unguard;
}

UNOpenGLESRenderDevice::FCachedShader* UNOpenGLESRenderDevice::CreateShader( DWORD ShaderFlags )
{
	guard(UNOpenGLESRenderDevice::CreateShader);

	static const char* FlagNames[SF_Count] = {
		"SF_Texture0", "SF_Texture1", "SF_Texture2", "SF_Texture3",
		"SF_VtxColor", "SF_AlphaTest", "SF_Lightmap", "SF_Fogmap",
		"SF_Detail", "SF_VtxFog"
	};

	static const char* UniformNames[UF_Count] = {
		"uMtx", "uBrightness", "uTexture0", "uTexture1", "uTexture2", "uTexture3"
	};

	static const char* AttribNames[AT_Count] = {
		"aPosition", "aTexCoord0", "aTexCoord1", "aTexCoord2",
		"aTexCoord3", "aVtxColor", "aVtxFog"
	};

	static const DWORD AttribFlags[AT_Count] = {
		0, SF_Texture0, SF_Texture1, SF_Texture2, SF_Texture3, SF_VtxColor, SF_VtxFog
	};

	static const char* ShaderVersion = "#version 100\n";

	FCachedShader* NewShader = ShaderMap.Add( ShaderFlags, FCachedShader() );
	verify(NewShader);
	NewShader->Flags = ShaderFlags;

	FString VSText;
	FString FSText;

	VSText += ShaderVersion;
	FSText += ShaderVersion;

	for( DWORD Flag = 1, FlagNum = 0; Flag <= SF_Max; Flag <<= 1, ++FlagNum )
	{
		if( ShaderFlags & Flag )
		{
			VSText.Appendf( "#define %s %u\n", FlagNames[FlagNum], Flag );
			FSText.Appendf( "#define %s %u\n", FlagNames[FlagNum], Flag );
		}
	}

	if( Overbright )
		FSText.Appendf( "#define LIGHTMAP_OVERBRIGHT %f\n", LIGHTMAP_OVERBRIGHT );

	VSText += VertShaderGLSL;
	FSText += FragShaderGLSL;

	GLuint VS = CompileShader( GL_VERTEX_SHADER, *VSText );
	GLuint FS = CompileShader( GL_FRAGMENT_SHADER, *FSText );

	GLuint Prog = glCreateProgram();
	glAttachShader( Prog, VS );
	glAttachShader( Prog, FS );

	NewShader->NumFloats = 0;
	for( INT i = 0; i < AT_Count; ++i )
	{
		if( i == 0 || ( ShaderFlags & AttribFlags[i] ) )
		{
			glBindAttribLocation( Prog, i, AttribNames[i] );
			NewShader->Attribs[i] = true;
			NewShader->NumFloats += AttribSizes[i];
		}
		else
		{
			NewShader->Attribs[i] = false;
		}
	}

	glLinkProgram( Prog );

	GLint Status = 0;
	glGetProgramiv( Prog, GL_LINK_STATUS, &Status );
	if( !Status )
	{
		char Tmp[2048] = { 0 };
		glGetProgramInfoLog( Prog, sizeof(Tmp), NULL, Tmp );
		appErrorf( "Failed to link shader %08x:\n%s", ShaderFlags, Tmp );
	}

	glDeleteShader( VS );
	glDeleteShader( FS );

	NewShader->Prog = Prog;

	for( INT i = 0; i < UF_Count; ++i )
		NewShader->Uniforms[i] = glGetUniformLocation( NewShader->Prog, UniformNames[i] );

	return NewShader;
	unguard;
}

void UNOpenGLESRenderDevice::SetShader( DWORD ShaderFlags )
{
	guard(UNOpenGLESRenderDevice::SetShader);

	if( !ShaderInfo || ShaderInfo->Flags != ShaderFlags )
	{
		FlushTriangles();

		ShaderInfo = ShaderMap.Find( ShaderFlags );
		if( !ShaderInfo )
			ShaderInfo = CreateShader( ShaderFlags );
		verify( ShaderInfo );

		// TODO: probably don't do this every program change
		for( INT i = 0; i < UF_Count; ++i )
			UniformsChanged[i] = true;

		BYTE* Ptr = UseVAO ? nullptr : (BYTE*)VtxData;
		for( INT i = 0; i < AT_Count; ++i )
		{
			if( ShaderInfo->Attribs[i] )
			{
				glEnableVertexAttribArray( i );
				glVertexAttribPointer( i, AttribSizes[i], GL_FLOAT, GL_FALSE, ShaderInfo->NumFloats * sizeof(FLOAT), (void*)Ptr );
				Ptr += AttribSizes[i] * sizeof(FLOAT);
			}
			else
			{
				glDisableVertexAttribArray( i );
			}
		}

		glUseProgram( ShaderInfo->Prog );
	}

	UpdateUniforms();

	unguard;
}

void UNOpenGLESRenderDevice::SetSceneNode( FSceneNode* Frame )
{
	guard(UNOpenGLESRenderDevice::SetSceneNode);

	check(Viewport);

	if( !Frame )
	{
		// invalidate current saved data
		CurrentSceneNode.X = -1;
		CurrentSceneNode.FX = -1.f;
		CurrentSceneNode.SizeX = -1;
		return;
	}

	if( Frame->X != CurrentSceneNode.X || Frame->Y != CurrentSceneNode.Y ||
			Frame->XB != CurrentSceneNode.XB || Frame->YB != CurrentSceneNode.YB ||
			Viewport->SizeX != CurrentSceneNode.SizeX || Viewport->SizeY != CurrentSceneNode.SizeY )
	{
		FlushTriangles();
		glViewport( Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y );
		CurrentSceneNode.X = Frame->X;
		CurrentSceneNode.Y = Frame->Y;
		CurrentSceneNode.XB = Frame->XB;
		CurrentSceneNode.YB = Frame->YB;
		CurrentSceneNode.SizeX = Viewport->SizeX;
		CurrentSceneNode.SizeY = Viewport->SizeY;
		const FLOAT Aspect = (FLOAT)Viewport->SizeX / (FLOAT)Viewport->SizeY;
		DefaultFOV = (FLOAT)(appAtan(appTan(90.0 * PI / 360.0) * (Aspect / (4.0 / 3.0))) * 360.0) / PI;
		if( AutoFOV && Viewport->Actor->DesiredFOV > 90.0f )
			Viewport->Actor->DesiredFOV = 90.0f;
	}

	if( Frame->FX != CurrentSceneNode.FX || Frame->FY != CurrentSceneNode.FY ||
			Viewport->Actor->FovAngle != CurrentSceneNode.FovAngle )
	{
		RProjZ = appTan( Viewport->Actor->FovAngle * PI / 360.0 );
		Aspect = Frame->FY / Frame->FX;
		RFX2 = 2.0f * RProjZ / Frame->FX;
		RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;
		MtxProj = glm::frustum( -RProjZ, +RProjZ, -Aspect * RProjZ, +Aspect * RProjZ, 1.f, 32768.f );
		MtxMVP = MtxProj * MtxModelView;
		CurrentSceneNode.FX = Frame->FX;
		CurrentSceneNode.FY = Frame->FY;
		CurrentSceneNode.FovAngle = Viewport->Actor->FovAngle;
		UniformsChanged[UF_Mtx] = true;
	}

	unguard;
}

void UNOpenGLESRenderDevice::SetBlend( DWORD PolyFlags, UBOOL InverseOrder )
{
	guard(UNOpenGLESRenderDevice::SetBlend);

	// Adjust PolyFlags according to Unreal's precedence rules.
	if( !(PolyFlags & (PF_Translucent|PF_Modulated)) )
		PolyFlags |= PF_Occlude;
	else if( PolyFlags & PF_Translucent )
		PolyFlags &= ~PF_Masked;

	// Detect changes in the blending modes.
	DWORD Xor = CurrentPolyFlags ^ PolyFlags;
	if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted) )
	{
		FlushTriangles();
		if( Xor & (PF_Translucent|PF_Modulated|PF_Highlighted) )
		{
			glEnable( GL_BLEND );
			if( PolyFlags & PF_Translucent )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
			}
			else if( PolyFlags & PF_Modulated )
			{
				glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			}
			else if( PolyFlags & PF_Highlighted )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				glDisable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ZERO );
			}
		}
		if( Xor & PF_Invisible )
		{
			UBOOL Show = !( PolyFlags & PF_Invisible );
			glColorMask( Show, Show, Show, Show );
		}
		if( Xor & PF_Occlude )
			glDepthMask( (PolyFlags & PF_Occlude) != 0 );
		if( Xor & PF_Masked )
		{
			if( PolyFlags & PF_Masked )
				CurrentShaderFlags |= SF_AlphaTest;
			else
				CurrentShaderFlags &= ~SF_AlphaTest;
		}
	}

	CurrentPolyFlags = PolyFlags;

	unguard;
}

void UNOpenGLESRenderDevice::UpdateTextureFilter( const FTextureInfo& Info, DWORD PolyFlags )
{
	guard(UNOpenGLESRenderDevice::UpdateTextureFilter);

	// Set mip filtering if there are mips.
	if( ( PolyFlags & PF_NoSmooth ) || ( NoFiltering && Info.Palette ) ) // TODO: This is set per poly, not per texture.
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( Info.NumMips > 1 ) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( Info.NumMips > 1 ) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	// This is a light/fog map.
	if( !Info.Palette )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}

	unguard;
}

void UNOpenGLESRenderDevice::ResetTexture( INT TMU )
{
	guard(UNOpenGLESRenderDevice::ResetTexture);

	CurrentShaderFlags &= ~(1 << TMU);

	if( TexInfo[TMU].CurrentCacheID != 0 )
	{
		FlushTriangles();
		glActiveTexture( GL_TEXTURE0 + TMU );
		glBindTexture( GL_TEXTURE_2D, 0 );
		TexInfo[TMU].CurrentCacheID = 0;
	}

	unguard;
}

void UNOpenGLESRenderDevice::SetTexture( INT TMU, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias )
{
	guard(UNOpenGLESRenderDevice::SetTexture);

	CurrentShaderFlags |= 1 << TMU;

	// Set panning.
	FTexInfo& Tex = TexInfo[TMU];
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

	// Account for all the impact on scale normalization.
	Tex.UMult = 1.f / (Info.UScale * static_cast<FLOAT>(Info.USize));
	Tex.VMult = 1.f / (Info.VScale * static_cast<FLOAT>(Info.VSize));

	// Find in cache.
	QWORD NewCacheID = Info.CacheID;
	if( ( PolyFlags & PF_Masked ) && Info.Palette )
		NewCacheID |= MASKED_TEXTURE_TAG;
	UBOOL RealtimeChanged = ( Info.TextureFlags & TF_RealtimeChanged );
	if( NewCacheID == Tex.CurrentCacheID && !RealtimeChanged )
		return;

	FlushTriangles();

	// Make current.
	Tex.CurrentCacheID = NewCacheID;
	FCachedTexture* Bind = BindMap.Find( NewCacheID );
	FCachedTexture* OldBind = Bind;
	if( !Bind )
	{
		// New texture.
		Bind = BindMap.Add( NewCacheID, FCachedTexture() );
		glGenTextures( 1, &Bind->Id );
		TexAlloc.AddItem( Bind->Id );
	}

	glActiveTexture( GL_TEXTURE0 + TMU );
	glBindTexture( GL_TEXTURE_2D, Bind->Id );

	if( !OldBind || RealtimeChanged )
	{
		// New texture or it has changed, upload it.
		Info.TextureFlags &= ~TF_RealtimeChanged;
		UploadTexture( Info, ( PolyFlags & PF_Masked ), !OldBind );
		// TODO: This depends on PolyFlags, not Info.
		UpdateTextureFilter( Info, PolyFlags );
	}

	unguard;
}

void UNOpenGLESRenderDevice::UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture )
{
	guard(UNOpenGLESRenderDevice::UploadTexture);

	if( !Info.Mips[0] )
	{
		debugf( NAME_Warning, "Encountered texture with invalid mips!" );
		return;
	}

	// We're gonna be using the compose buffer, so expand it to fit.
	INT NewComposeSize = Info.Mips[0]->USize * Info.Mips[0]->VSize * 4;
	if( NewComposeSize > ComposeSize )
	{
		Compose = (BYTE*)appRealloc( Compose, NewComposeSize, "GLComposeBuf" );
		verify( Compose );
	}

	// Upload all mips.
	for( INT MipIndex = 0; MipIndex < Info.NumMips; ++MipIndex )
	{
		const FMipmap* Mip = Info.Mips[MipIndex];
		if( !Mip || !Mip->DataPtr ) break;
		BYTE* UploadBuf;
		GLenum UploadFormat;
		// Convert texture if needed.
		if( Info.Palette )
		{
			// 8-bit indexed. We have to fix the alpha component since it's mostly garbage in non-detailmaps.
			UploadBuf = Compose;
			UploadFormat = GL_RGBA;
			DWORD* Dst = (DWORD*)Compose;
			const BYTE* Src = (const BYTE*)Mip->DataPtr;
			const DWORD* Pal = (const DWORD*)Info.Palette;
			const DWORD Count = Mip->USize * Mip->VSize;
			if( Masked )
			{
				// index 0 is transparent
#if __INTEL_BYTE_ORDER__
				for( DWORD i = 0; i < Count; ++i, ++Src )
					*Dst++ = *Src ? ( Pal[*Src] | ALPHA_MASK ) : 0;
#else
				for( i = 0; i < Count; ++i, ++Src )
				{
					FColor Color = Palette[*Src];
					Color.A = *Src ? 255 : 0;
					*Dst++ = (Color.R << 24) | (Color.G << 16) | (Color.B << 8) | Color.A;
				}
#endif
			}
			else
			{
				// index 0 is whatever
#if __INTEL_BYTE_ORDER__
				for( DWORD i = 0; i < Count; ++i )
					*Dst++ = ( Pal[*Src++] | ALPHA_MASK );
#else
				for( DWORD i = 0; i < Count; ++i, ++Src )
				{
					FColor Color = Palette[*Src];
					Color.A = 255;
					*Dst++ = (Color.R << 24) | (Color.G << 16) | (Color.B << 8) | Color.A;
				}
#endif
			}
		}
		else if( UseBGRA )
		{
			// BGRA8888 (or 7777) and we can upload it as-is.
			UploadBuf = Mip->DataPtr;
			UploadFormat = GL_BGRA_EXT;
		}
		else
		{
			// BGRA8888 (or 7777), but we must swap it because it's not supported natively.
			UploadBuf = Compose;
			UploadFormat = GL_RGBA;
			BYTE* Dst = (BYTE*)Compose;
			const BYTE* Src = (const BYTE*)Mip->DataPtr;
			const DWORD Count = Mip->USize * Mip->VSize;
			for( DWORD i = 0; i < Count; ++i, Src += 4 )
			{
				*Dst++ = Src[2];
				*Dst++ = Src[1];
				*Dst++ = Src[0];
				*Dst++ = Src[3];
			}
		}
		// Upload to GL.
		if( NewTexture )
			glTexImage2D( GL_TEXTURE_2D, MipIndex, UploadFormat, Mip->USize, Mip->VSize, 0, UploadFormat, GL_UNSIGNED_BYTE, (void*)UploadBuf );
		else
			glTexSubImage2D( GL_TEXTURE_2D, MipIndex, 0, 0, Mip->USize, Mip->VSize, UploadFormat, GL_UNSIGNED_BYTE, (void*)UploadBuf );
	}

	unguard;
}

void UNOpenGLESRenderDevice::UpdateSwapInterval()
{
	guard(UNOpenGLESRenderDevice::UpdateSwapInterval);

	if( SwapInterval < -1 )
	{
		SwapInterval = -1;
	}

	if( SDL_GL_SetSwapInterval( SwapInterval ) < 0 )
	{
		debugf( NAME_Warning, "Failed to set swap interval %d: %s", SwapInterval, SDL_GetError() );
		if( SwapInterval < 0 )
		{
			// Adaptive VSync not supported, try normal VSync.
			SwapInterval = 1;
			UpdateSwapInterval();
		}
	}

	unguard;
}
