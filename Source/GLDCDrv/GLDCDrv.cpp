#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"
#include <kos.h>
#include <malloc.h>

#include "GLDCDrvPrivate.h"

#define MASKED_TEXTURE_TAG (1ULL << 60)

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(GLDCDrv);
IMPLEMENT_CLASS(UGLDCRenderDevice);

/*-----------------------------------------------------------------------------
	UGLDCRenderDevice implementation.
-----------------------------------------------------------------------------*/

void UGLDCRenderDevice::InternalClassInitializer( UClass* Class )
{
	guardSlow(UGLDCRenderDevice::InternalClassInitializer);
	new(Class, "NoFiltering",  RF_Public)UBoolProperty( CPP_PROPERTY(NoFiltering),  "Options", CPF_Config );
	unguardSlow;
}

UGLDCRenderDevice::UGLDCRenderDevice()
{
	NoFiltering = false;
}

UBOOL UGLDCRenderDevice::Init( UViewport* InViewport )
{
	guard(UGLDCRenderDevice::Init)

	GLdcConfig config;
	glKosInitConfig(&config);
	config.initial_op_capacity = 512 * 3;
	config.initial_tr_capacity = 512 * 3;
	config.initial_pt_capacity = 256 * 3;
	config.initial_immediate_capacity = 256;
	glKosInitEx(&config);

	SupportsFogMaps = false; // true;
	SupportsDistanceFog = false; // true;
	NoVolumetricBlend = true;

	ComposeSize = 256 * 256 * 2;
	Compose = (BYTE*)appMalloc( ComposeSize, "GLComposeBuf" );
	verify( Compose );

	// Set modelview matrix to flip stuff into our coordinate system.
	const FLOAT Matrix[16] =
	{
		+1, +0, +0, +0,
		+0, -1, +0, +0,
		+0, +0, -1, +0,
		+0, +0, +0, +1,
	};
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glMultMatrixf( Matrix );

	// Set permanent state.
	glEnable( GL_DEPTH_TEST );
	glShadeModel( GL_SMOOTH );
	glAlphaFunc( GL_GREATER, 0.5f );
	glDisable( GL_ALPHA_TEST );
	glDepthMask( GL_TRUE );
	glBlendFunc( GL_ONE, GL_ZERO );
	glDisable( GL_BLEND );
	glClearDepth( 1.0f );

	PrintMemStats();

	CurrentPolyFlags = PF_Occlude;
	Viewport = InViewport;

	return true;
	unguard;
}

void UGLDCRenderDevice::Exit()
{
	guard(UGLDCRenderDevice::Exit);

	debugf( NAME_Log, "Shutting down OpenGL renderer" );

	Flush();

	if( Compose )
	{
		appFree( Compose );
		Compose = NULL;
	}
	ComposeSize = 0;

	unguard;
}

void UGLDCRenderDevice::Flush()
{
	guard(UGLDCRenderDevice::Flush);

	ResetTexture();

	if( BindMap.Size() )
	{
		debugf( NAME_Log, "Flushing %d textures", BindMap.Size() );
		glFinish();
		for( INT i = 0; i < BindMap.Size(); ++i )
			glDeleteTextures( 1, &BindMap[i] );
		BindMap.Empty();
	}

	glDefragmentTextureMemory_KOS();

	unguard;
}

UBOOL UGLDCRenderDevice::Exec( const char* Cmd, FOutputDevice* Out )
{
	return false;
}

void UGLDCRenderDevice::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
{
	guard(UGLDCRenderDevice::Lock);

	glClearColor( ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W );
	glDepthFunc( GL_LEQUAL );
	SetBlend( PF_Occlude );

	if( RenderLockFlags & LOCKR_ClearScreen )
		glClear( GL_COLOR_BUFFER_BIT );

	if( FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f) )
		ColorMod = FPlane( FlashFog.X, FlashFog.Y, FlashFog.Z, 1.f - Min( FlashScale.X * 2.f, 1.f ) );
	else
		ColorMod = FPlane( 0.f, 0.f, 0.f, 0.f );

	unguard;
}

void UGLDCRenderDevice::Unlock( UBOOL Blit )
{
	guard(UGLDCRenderDevice::Unlock);

	static DWORD Frame = 0;

	glKosSwapBuffers();

	++Frame;
	if( ( Frame & 0xff ) == 0 )
	{
		debugf( "Frame %d", Frame );
		PrintMemStats();
	}

	unguard;
}

void UGLDCRenderDevice::DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
{
	guard(UGLDCRenderDevice::DrawComplexSurface);

	check(Surface.Texture);

	SetSceneNode( Frame );

	// @HACK: Don't draw translucent and masked parts of the sky. Don't know how to do that yet.
	if( CurrentSceneNode.bIsSky && ( Surface.PolyFlags & (PF_Translucent|PF_Masked) ))
		return;

	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	// Draw texture.
	SetBlend( Surface.PolyFlags );
	SetTexture( *Surface.Texture, ( Surface.PolyFlags & PF_Masked ), 0.f );
	glColor4f( 1.f, 1.f, 1.f, 1.f );
	for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
	{
		glBegin( GL_TRIANGLE_FAN );
		for( INT i = 0; i < Poly->NumPts; i++ )
		{
			FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
			glTexCoord2f( (U-UDot-TexInfo.UPan)*TexInfo.UMult, (V-VDot-TexInfo.VPan)*TexInfo.VMult );
			glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
		}
		glEnd();
	}

	// Draw lightmap.
	// @HACK: Unless this is the sky. See above.
	if( Surface.LightMap && !CurrentSceneNode.bIsSky )
	{
		SetBlend( PF_Modulated );
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_EQUAL );
		SetTexture( *Surface.LightMap, 0, -0.5f );
		glColor4f( 1.f, 1.f, 1.f, 1.f );
		for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
		{
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < Poly->NumPts; i++ )
			{
				FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
				FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
				glTexCoord2f( (U-UDot-TexInfo.UPan)*TexInfo.UMult, (V-VDot-TexInfo.VPan)*TexInfo.VMult );
				glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
			}
			glEnd();
		}
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_LEQUAL );
	}

	// Draw fog.
	/*
	if( Surface.FogMap )
	{
		SetBlend( PF_Highlighted );
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_EQUAL );
		SetTexture( *Surface.FogMap, 0, -0.5f );
		for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
		{
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < Poly->NumPts; i++ )
			{
				FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
				FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
				glTexCoord2f( (U-UDot-TexInfo.UPan)*TexInfo.UMult, (V-VDot-TexInfo.VPan)*TexInfo.VMult );
				glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
			}
			glEnd();
		}
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_LEQUAL );
	}
	*/

	unguard;
}

void UGLDCRenderDevice::DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )
{
		guard(UGLDCRenderDevice::DrawGouraudPolygon);

		SetSceneNode( Frame );
		SetBlend( PolyFlags );
		SetTexture( Texture, ( PolyFlags & PF_Masked ), 0 );

		const UBOOL IsModulated = ( PolyFlags & PF_Modulated );

		if( IsModulated )
			glColor4f( 1.f, 1.f, 1.f, 1.f );

		glBegin( GL_TRIANGLE_FAN  );
		for( INT i=0; i<NumPts; i++ )
		{
			FTransTexture* P = Pts[i];
			if( !IsModulated )
				glColor4f( P->Light.X, P->Light.Y, P->Light.Z, 1.f );
			glTexCoord2f( P->U*TexInfo.UMult, P->V*TexInfo.VMult );
			glVertex3f( P->Point.X, P->Point.Y, P->Point.Z );
		}
		glEnd();

		if( (PolyFlags & (PF_RenderFog|PF_Translucent|PF_Modulated)) == PF_RenderFog )
		{
			ResetTexture( );
			SetBlend( PF_Highlighted );
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < NumPts; i++ )
			{
				FTransTexture* P = Pts[i];
				glColor4f( P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W );
				glVertex3f( P->Point.X, P->Point.Y, P->Point.Z );
			}
			glEnd();
		}

		unguard;
}

void UGLDCRenderDevice::DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags )
{
	guard(UGLDCRenderDevice::DrawTile);

	SetSceneNode( Frame );
	SetBlend( PolyFlags );
	SetTexture( Texture, ( PolyFlags & PF_Masked ), 0.f );

	if( PolyFlags & PF_Modulated )
		glColor4f( 1.f, 1.f, 1.f, 1.f );
	else
		glColor4f( Light.X, Light.Y, Light.Z, 1.f );

	glBegin( GL_TRIANGLE_FAN );
		glTexCoord2f( (U   )*TexInfo.UMult, (V   )*TexInfo.VMult );
		glVertex3f( RFX2*Z*(X   -Frame->FX2), RFY2*Z*(Y   -Frame->FY2), Z );
		glTexCoord2f( (U+UL)*TexInfo.UMult, (V   )*TexInfo.VMult );
		glVertex3f( RFX2*Z*(X+XL-Frame->FX2), RFY2*Z*(Y   -Frame->FY2), Z );
		glTexCoord2f( (U+UL)*TexInfo.UMult, (V+VL)*TexInfo.VMult );
		glVertex3f( RFX2*Z*(X+XL-Frame->FX2), RFY2*Z*(Y+YL-Frame->FY2), Z );
		glTexCoord2f( (U   )*TexInfo.UMult, (V+VL)*TexInfo.VMult );
		glVertex3f( RFX2*Z*(X   -Frame->FX2), RFY2*Z*(Y+YL-Frame->FY2), Z );
	glEnd();

	unguard;
}

void UGLDCRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{

}

void UGLDCRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2 )
{

}

void UGLDCRenderDevice::EndFlash( )
{
	guard(UGLDCESRenderDevice::EndFlash);

	if( ColorMod == FPlane( 0.f, 0.f, 0.f, 0.f ) )
		return;

	ResetTexture();
	SetBlend( PF_Highlighted );

	const FLOAT Z = 1.f;
	const FLOAT RFX2 = RProjZ;
	const FLOAT RFY2 = RProjZ * Aspect;

	glDisable( GL_DEPTH_TEST );

	glColor4fv( &ColorMod.R );
	glBegin( GL_TRIANGLE_FAN );
		glVertex3f( RFX2 * -Z, RFY2 * -Z, Z );
		glVertex3f( RFX2 * +Z, RFY2 * -Z, Z );
		glVertex3f( RFX2 * +Z, RFY2 * +Z, Z );
		glVertex3f( RFX2 * -Z, RFY2 * +Z, Z );
	glEnd();

	glEnable( GL_DEPTH_TEST );

	unguard;
}

void UGLDCRenderDevice::PushHit( const BYTE* Data, INT Count )
{

}

void UGLDCRenderDevice::PopHit( INT Count, UBOOL bForce )
{

}

void UGLDCRenderDevice::GetStats( char* Result )
{
	guard(UGLDCRenderDevice::GetStats)

	if( Result ) *Result = '\0';

	unguard;
}

void UGLDCRenderDevice::ReadPixels( FColor* Pixels )
{
	guard(UGLDCRenderDevice::ReadPixels);

	glPixelStorei( GL_UNPACK_ALIGNMENT, 0 );
	glReadPixels( 0, 0, Viewport->SizeX, Viewport->SizeY, GL_BGRA, GL_UNSIGNED_BYTE, (void*)Pixels );

	unguard;
}

void UGLDCRenderDevice::ClearZ( FSceneNode* Frame )
{
	guard(UGLDCRenderDevice::ClearZ);

	SetBlend( PF_Occlude );

	unguard;
}

void UGLDCRenderDevice::SetSceneNode( FSceneNode* Frame )
{
	guard(UGLDCRenderDevice::SetSceneNode);

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
		glViewport( Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y );
		CurrentSceneNode.X = Frame->X;
		CurrentSceneNode.Y = Frame->Y;
		CurrentSceneNode.XB = Frame->XB;
		CurrentSceneNode.YB = Frame->YB;
		CurrentSceneNode.SizeX = Viewport->SizeX;
		CurrentSceneNode.SizeY = Viewport->SizeY;
	}

	if( Frame->Level )
	{
		AZoneInfo* ZoneInfo = (AZoneInfo*)Frame->Level->GetZoneActor( Frame->ZoneNumber );
		const UBOOL bIsSky = ( ZoneInfo && ZoneInfo->SkyZone && ZoneInfo->IsA( ASkyZoneInfo::StaticClass ) );
		CurrentSceneNode.bIsSky = bIsSky;
	}

	if( Frame->FX != CurrentSceneNode.FX || Frame->FY != CurrentSceneNode.FY ||
			Viewport->Actor->FovAngle != CurrentSceneNode.FovAngle )
	{
		RProjZ = ftan( Viewport->Actor->FovAngle * (FLOAT)PI / 360.0f );
		Aspect = Frame->FY / Frame->FX;
		RFX2 = 2.0f * RProjZ / Frame->FX;
		RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glFrustum( -RProjZ, +RProjZ, -Aspect * RProjZ, +Aspect * RProjZ, 1.0f, 32768.0f );
		CurrentSceneNode.FX = Frame->FX;
		CurrentSceneNode.FY = Frame->FY;
		CurrentSceneNode.FovAngle = Viewport->Actor->FovAngle;
	}

	unguard;
}

void UGLDCRenderDevice::SetBlend( DWORD PolyFlags, UBOOL InverseOrder )
{
	guard(UGLDCRenderDevice::SetBlend);

	// Adjust PolyFlags according to Unreal's precedence rules.
	// @HACK: Unless this is the sky, in which case we want it to never occlude.
	if( !(PolyFlags & (PF_Translucent|PF_Modulated)) && !CurrentSceneNode.bIsSky )
		PolyFlags |= PF_Occlude;
	else if( PolyFlags & PF_Translucent )
		PolyFlags &= ~PF_Masked;

	// Detect changes in the blending modes.
	DWORD Xor = CurrentPolyFlags ^ PolyFlags;
	if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted) )
	{
		if( Xor&(PF_Translucent|PF_Modulated|PF_Highlighted) )
		{
			if( PolyFlags & PF_Translucent )
			{
				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
			}
			else if( PolyFlags & PF_Modulated )
			{
				glEnable( GL_BLEND );
				glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			}
			else if( PolyFlags & PF_Highlighted )
			{
				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				glBlendFunc( GL_ONE, GL_ZERO );
				glDisable( GL_BLEND );
			}
		}
		if( Xor & PF_Invisible )
		{
			const UBOOL Show = !( PolyFlags & PF_Invisible );
			glColorMask( Show, Show, Show, Show );
		}
		if( Xor & PF_Occlude )
			glDepthMask( (PolyFlags & PF_Occlude) != 0 );
		if( Xor & PF_Masked )
		{
			if( PolyFlags & PF_Masked )
				glEnable( GL_ALPHA_TEST );
			else
				glDisable( GL_ALPHA_TEST );
		}
	}

	CurrentPolyFlags = PolyFlags;

	unguard;
}

void UGLDCRenderDevice::ResetTexture( )
{
	guard(UGLDCRenderDevice::ResetTexture);

	if( TexInfo.CurrentCacheID != 0 )
	{
		glBindTexture( GL_TEXTURE_2D, 0 );
		glDisable( GL_TEXTURE_2D );
		TexInfo.CurrentCacheID = 0;
	}

	unguard;
}

void UGLDCRenderDevice::SetTexture( FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias )
{
	guard(UGLDCRenderDevice::SetTexture);

	// Set panning.
	FTexInfo& Tex = TexInfo;
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

	// Account for all the impact on scale normalization.
	//const INT USize = Max( MinTexSize, Info.USize );
	//const INT VSize = Max( MinTexSize, Info.VSize );
	Tex.UMult = 1.f / (Info.UScale * static_cast<FLOAT>(Info.USize));
	Tex.VMult = 1.f / (Info.VScale * static_cast<FLOAT>(Info.VSize));

	// Find in cache.
	const UBOOL RealtimeChanged = ( Info.TextureFlags & TF_RealtimeChanged );
	QWORD NewCacheID = Info.CacheID;
	if( PolyFlags & PF_Masked )
		NewCacheID |= MASKED_TEXTURE_TAG;
	if( NewCacheID == Tex.CurrentCacheID && !RealtimeChanged )
		return;

	// Make current.
	Tex.CurrentCacheID = NewCacheID;
	GLuint* Bind = BindMap.Find( NewCacheID );
	GLuint* OldBind = Bind;
	if( !Bind )
	{
		// New texture.
		Bind = BindMap.Add( NewCacheID, 0 );
		glGenTextures( 1, Bind );
	}

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, *Bind );

	if( !OldBind || RealtimeChanged )
	{
		// New texture or it has changed, upload it.
		Info.TextureFlags &= ~TF_RealtimeChanged;
		UploadTexture( Info, !OldBind, ( PolyFlags & PF_Masked ) != 0 );
		// Set mip filtering if there are mips.
		const INT NumMips = Min( MaxMipLevel + 1, Info.NumMips );
		if( ( PolyFlags & PF_NoSmooth ) || ( NoFiltering && Info.Palette ) ) // TODO: This is set per poly, not per texture.
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( NumMips > 1 ) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		}
		else
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( NumMips > 1 ) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		}
	}

	unguard;
}

void UGLDCRenderDevice::EnsureComposeSize( const DWORD NewSize )
{
	if( NewSize > ComposeSize )
	{
		Compose = (BYTE*)appRealloc( Compose, NewSize, "GLComposeBuf" );
		verify( Compose );
		debugf( "GL: Compose size increased %d -> %d", ComposeSize, NewSize );
		ComposeSize = NewSize;
	}
}

static inline _WORD RGB888ToARGB1555( const FColor& Col )
{
	return ( ( Col.B & 0xF8 ) >> 3 ) | ( ( Col.G & 0xF8 ) << 2 ) | ( ( Col.R & 0xF8 ) << 7 ) | 0x8000;
}

static inline _WORD BGRA7777ToRGB565( const BYTE* Src )
{
	return ( ( Src[0] & 0x7C ) >> 2 ) | ( ( Src[1] & 0x7E ) << 4 ) | ( ( Src[2] & 0x7C ) << 9 );
}

void* UGLDCRenderDevice::VerticalUpscale( const INT USize, const INT VSize, const INT VTimes )
{
	DWORD i;
	const INT SrcLine = USize << 1;
	const _WORD* Src = (_WORD*)Compose;
	_WORD* NewBase = (_WORD*)Compose + USize * VSize;
	_WORD* Dst = NewBase;

	// at this point U is already at least 8, so we can freely use memcpy4
	for( i = 0; i < VSize; ++i, Src += USize )
	{
		switch( VTimes )
		{
			case 8:
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				[[fallthrough]];
			case 4:
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				[[fallthrough]];
			case 2:
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				[[fallthrough]];
			default:
				memcpy4( Dst, Src, SrcLine ); Dst += USize;
				break;
		}
	}

	return NewBase;
}

void* UGLDCRenderDevice::ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette, const UBOOL Masked )
{
	// 8-bit indexed. We have to fix the alpha component since it's mostly garbage.
	DWORD i;
	_WORD* Dst = (_WORD*)Compose;
	const BYTE* Src = (const BYTE*)Mip->DataPtr;
	const DWORD SrcCount = Mip->USize * Mip->VSize;
	INT USize = Mip->USize;
	INT VSize = Mip->VSize;

	EnsureComposeSize( SrcCount * 2 );

	// convert palette; if texture is masked, make first entry transparent
	_WORD DstPal[NUM_PAL_COLORS];
	DstPal[0] = Masked ? 0 : RGB888ToARGB1555( Palette[0] );
	for( i = 1; i < ARRAY_COUNT( DstPal ); ++i )
		DstPal[i] = RGB888ToARGB1555( Palette[i] );

	// convert and upscale texture horizontally to width = 8 if needed
	const INT UTimes = MinTexSize / USize;
	for( i = 0; i < SrcCount; ++i, ++Src )
	{
		const _WORD C = DstPal[*Src];
		switch( UTimes )
		{
			case 8:
				*Dst++ = C;
				*Dst++ = C;
				*Dst++ = C;
				*Dst++ = C;
				[[fallthrough]];
			case 4:
				*Dst++ = C;
				*Dst++ = C;
				[[fallthrough]];
			case 2:
				*Dst++ = C;
				[[fallthrough]];
			default:
				*Dst++ = C;
				break;
		}
	}
	if( UTimes > 1 )
		USize = MinTexSize;

	// upscale texture vertically to height = 8 if needed
	const INT VTimes = MinTexSize / VSize;
	if( VTimes > 1 )
		return VerticalUpscale( USize, VSize, VTimes );

	return Compose;
}

void* UGLDCRenderDevice::ConvertTextureMipBGRA7777( const FMipmap* Mip )
{
	// BGRA8888. This is actually a BGRA7777 lightmap, so we need to scale it.
	DWORD i;
	INT USize = Mip->USize;
	INT VSize = Mip->VSize;
	_WORD* Dst = (_WORD*)Compose;
	const BYTE* Src = (const BYTE*)Mip->DataPtr;
	const DWORD Count = USize * VSize;

	EnsureComposeSize( Count * 2 );

	// convert and upscale texture horizontally to width = 8 if needed
	const INT UTimes = MinTexSize / USize;
	for( i = 0; i < Count; ++i, Src += 4 )
	{
		const _WORD C = BGRA7777ToRGB565( Src );
		switch( UTimes )
		{
			case 8:
				*Dst++ = C;
				*Dst++ = C;
				*Dst++ = C;
				*Dst++ = C;
				[[fallthrough]];
			case 4:
				*Dst++ = C;
				*Dst++ = C;
				[[fallthrough]];
			case 2:
				*Dst++ = C;
				[[fallthrough]];
			default:
				*Dst++ = C;
				break;
		}
	}
	if( UTimes > 1 )
		USize = MinTexSize;

	// upscale texture vertically to height = 8 if needed
	const INT VTimes = MinTexSize / VSize;
	if( VTimes > 1 )
		return VerticalUpscale( USize, VSize, VTimes );

	return Compose;
}

void UGLDCRenderDevice::UploadTexture( FTextureInfo& Info, const UBOOL NewTexture, const UBOOL Masked )
{
	guard(UGLDCRenderDevice::UploadTexture);

	if( !Info.Mips[0] )
	{
		debugf( NAME_Warning, "Encountered texture with invalid mips!" );
		return;
	}

	// Upload all mips.
	const INT NumMips = Min( MaxMipLevel + 1, Info.NumMips );
	for( INT MipIndex = 0; MipIndex < NumMips; ++MipIndex )
	{
		FMipmap* Mip = Info.Mips[MipIndex];
		void* UploadBuffer = (void*)Compose;
		GLenum UploadFormat;
		GLenum InternalFormat;
		GLenum ElementFormat;
		if( !Mip || !Mip->DataPtr )
			break;
		// Convert texture if needed.
		if( Info.Palette )
		{
			UploadFormat = GL_BGRA;
			InternalFormat = GL_ARGB1555_KOS;
			ElementFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
			UploadBuffer = ConvertTextureMipI8( Mip, Info.Palette, Masked );
		}
		else
		{
			UploadFormat = GL_RGB;
			InternalFormat = GL_RGB565_KOS;
			ElementFormat = GL_UNSIGNED_SHORT_5_6_5;
			UploadBuffer = ConvertTextureMipBGRA7777( Mip );
		}
		// Upload to GL.
		const INT USize = Max( MinTexSize, Mip->USize );
		const INT VSize = Max( MinTexSize, Mip->VSize );
		if( NewTexture )
			glTexImage2D( GL_TEXTURE_2D, MipIndex, InternalFormat, USize, VSize, 0, UploadFormat, ElementFormat, (void*)UploadBuffer );
		else
			glTexSubImage2D( GL_TEXTURE_2D, MipIndex, 0, 0, USize, VSize, UploadFormat, ElementFormat, (void*)UploadBuffer );
	}

	unguard;
}

void UGLDCRenderDevice::PrintMemStats() const
{
	INT FreeVRAM = 0;
	glGetIntegerv(GL_FREE_TEXTURE_MEMORY_KOS, &FreeVRAM);
	debugf( "Free VRAM = %d", FreeVRAM );
	malloc_stats();
}
