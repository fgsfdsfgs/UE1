#include "PSPOpenGLDrvPrivate.h"

#include <string.h>
#include <malloc.h>
#include <pspsysmem.h>

extern SceUID __psp_heap_blockid;

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(PSPOpenGLDrv);
IMPLEMENT_CLASS(UPSPOpenGLRenderDevice);

/*-----------------------------------------------------------------------------
	UPSPOpenGLRenderDevice implementation.
-----------------------------------------------------------------------------*/

// from XOpenGLDrv:
// PF_Masked requires index 0 to be transparent, but is set on the polygon instead of the texture,
// so we potentially need two copies of any palettized texture in the cache
// unlike in newer unreal versions the low cache bits are actually used, so we have use one of the
// actually unused higher bits for this purpose, thereby breaking 64-bit compatibility for now
#define MASKED_TEXTURE_TAG (1ULL << 60)

// FColor is adjusted for endianness
#define ALPHA_MASK 0xff000000

// lightmaps are 0-127
#define LIGHTMAP_SCALE 2

// and it also would be nice to overbright them
#define LIGHTMAP_OVERBRIGHT 1.4f

void UPSPOpenGLRenderDevice::InternalClassInitializer( UClass* Class )
{
	guardSlow(UPSPOpenGLRenderDevice::InternalClassInitializer);
	new(Class, "NoFiltering",         RF_Public)UBoolProperty( CPP_PROPERTY(NoFiltering),         "Options", CPF_Config );
	new(Class, "UseHwPalette",        RF_Public)UBoolProperty( CPP_PROPERTY(UseHwPalette),        "Options", CPF_Config );
	new(Class, "DetailTextures",      RF_Public)UBoolProperty( CPP_PROPERTY(DetailTextures),      "Options", CPF_Config );
	new(Class, "AutoFOV",             RF_Public)UBoolProperty( CPP_PROPERTY(AutoFOV),             "Options", CPF_Config );
	new(Class, "SwapInterval",        RF_Public)UIntProperty ( CPP_PROPERTY(SwapInterval),        "Options", CPF_Config );
	unguardSlow;
}

UPSPOpenGLRenderDevice::UPSPOpenGLRenderDevice()
{
	NoFiltering = false;
	UseHwPalette = true;
	DetailTextures = false;
	AutoFOV = true;
	CurrentBrightness = -1.f;
	SwapInterval = 1;
}

UBOOL UPSPOpenGLRenderDevice::Init( UViewport* InViewport )
{
	guard(UPSPOpenGLRenderDevice::Init)

	SupportsFogMaps = false;
	SupportsDistanceFog = false;
	NoVolumetricBlend = true;

	debugf( NAME_Log, "Got OpenGL ES CM 1.1" );

	ComposeSize = 256 * 256 * 4;
	Compose = (BYTE*)memalign( 64, ComposeSize );
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
	glEnable( GL_CULL_FACE );
	glCullFace( GL_FRONT );
	glShadeModel( GL_FLAT );
	glAlphaFunc( GL_GREATER, 0.5f );
	glDisable( GL_ALPHA_TEST );
	glDepthMask( GL_TRUE );
	glBlendFunc( GL_ONE, GL_ZERO );
	glDisable( GL_BLEND );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	CurrentPolyFlags = PF_Occlude;
	Viewport = InViewport;

	return true;
	unguard;
}

void UPSPOpenGLRenderDevice::Exit()
{
	guard(UPSPOpenGLRenderDevice::Exit);

	debugf( NAME_Log, "Shutting down OpenGL renderer" );

	Flush();

	if( Compose )
	{
		free( Compose );
		Compose = NULL;
	}

	ComposeSize = 0;

	unguard;
}

void UPSPOpenGLRenderDevice::PostEditChange()
{
	guard(UPSPOpenGLRenderDevice::PostEditChange)

	Super::PostEditChange();

	unguard;
}

void UPSPOpenGLRenderDevice::Flush()
{
	guard(UPSPOpenGLRenderDevice::Flush);

	if( BindMap.Size() )
	{
		debugf( NAME_Log, "Flushing %d textures", BindMap.Size() );
		ResetTexture();
		glFinish();
		for( INT i = 0; i < BindMap.Size(); ++i)
		{
			if( BindMap[i].Id )
				glDeleteTextures( 1, &BindMap[i].Id );
		}
		BindMap.Empty();
	}

	unguard;
}

UBOOL UPSPOpenGLRenderDevice::Exec( const char* Cmd, FOutputDevice* Out )
{
	return false;
}

void UPSPOpenGLRenderDevice::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
{
	guard(UPSPOpenGLRenderDevice::Lock);

	BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;

	glClearColor( ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W );
	glClearDepthf( 1.0f );
	glDepthFunc( GL_LEQUAL );

	SetBlend( PF_Occlude );

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
		const FLOAT Aspect = (FLOAT)Viewport->SizeX / (FLOAT)Viewport->SizeY;
		const FLOAT Fov = (FLOAT)( appAtan( appTan( 90.0 * PI / 360.0 ) * ( Aspect / ( 4.0 / 3.0 ) ) ) * 360.0 ) / PI;
		Viewport->Actor->DesiredFOV = Fov;
	}

	unguard;
}

void UPSPOpenGLRenderDevice::Unlock( UBOOL Blit )
{
	guard(UPSPOpenGLRenderDevice::Unlock);

	unguard;
}

void UPSPOpenGLRenderDevice::DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
{
	guard(UPSPOpenGLRenderDevice::DrawComplexSurface);

	check(Surface.Texture);

	SetSceneNode( Frame );

	uclock(ComplexCycles);

	DrawComplexSurfaceSingleTex(Frame, Surface, Facet);

	uunclock(ComplexCycles);

	unguard;
}

void UPSPOpenGLRenderDevice::DrawComplexSurfaceSingleTex( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
{
	const FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	const FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	FLOAT* TexCoords = VertexBuf;
	FLOAT* UV = TexCoords;

	// Draw texture.
	SetBlend( Surface.PolyFlags );
	SetTexture( *Surface.Texture, ( Surface.PolyFlags & PF_Masked ), 0.f );
	glColor4f( 1.f, 1.f, 1.f, 1.f );
	for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
	{
		glBegin( GL_TRIANGLE_FAN );
		for( INT i = 0; i < Poly->NumPts; ++i, UV += 2 )
		{
			// Cache UVs for drawing lightmaps later.
			UV[0] = ( Facet.MapCoords.XAxis | Poly->Pts[i]->Point ) - UDot;
			UV[1] = ( Facet.MapCoords.YAxis | Poly->Pts[i]->Point ) - VDot;
			glTexCoord2f( (UV[0]-TexInfo.UPan)*TexInfo.UMult, (UV[1]-TexInfo.VPan)*TexInfo.VMult );
			glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
		}
		glEnd();
	}

	// Draw lightmap.
	if( Surface.LightMap )
	{
		SetBlend( PF_Modulated );
		// TODO: This is a shitty solution sometimes
		glDepthFunc( GL_EQUAL );
		SetTexture( *Surface.LightMap, 0, -0.5 );
		UV = TexCoords;
		for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
		{
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < Poly->NumPts; ++i, UV += 2 )
			{
				glTexCoord2f( (UV[0]-TexInfo.UPan)*TexInfo.UMult, (UV[1]-TexInfo.VPan)*TexInfo.VMult );
				glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
			}
			glEnd();
		}
		glDepthFunc( GL_LEQUAL );
	}
}

void UPSPOpenGLRenderDevice::DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )
{
		guard(UPSPOpenGLRenderDevice::DrawGouraudPolygon);

		glShadeModel( GL_SMOOTH );

		SetSceneNode( Frame );
		uclock(GouraudCycles);
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
			ResetTexture();
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

		glShadeModel( GL_FLAT );

		uunclock(GouraudCycles);
		unguard;
}

void UPSPOpenGLRenderDevice::DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags )
{
	guard(UPSPOpenGLRenderDevice::DrawTile);

	SetSceneNode( Frame );
	uclock(TileCycles);
	SetBlend( PolyFlags );
	SetTexture( Texture, ( PolyFlags & PF_Masked ), 0.f );

	glDisable( GL_DEPTH_TEST );

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

	glEnable( GL_DEPTH_TEST );

	uunclock(TileCycles);
	unguard;
}

void UPSPOpenGLRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{

}

void UPSPOpenGLRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2 )
{

}

void UPSPOpenGLRenderDevice::EndFlash( )
{
	guard(UPSPOpenGLESRenderDevice::EndFlash);

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

void UPSPOpenGLRenderDevice::PushHit( const BYTE* Data, INT Count )
{

}

void UPSPOpenGLRenderDevice::PopHit( INT Count, UBOOL bForce )
{

}

void UPSPOpenGLRenderDevice::GetStats( char* Result )
{
	guard(UPSPOpenGLRenderDevice::GetStats)

	unguard;
}

void UPSPOpenGLRenderDevice::ReadPixels( FColor* Pixels )
{
	guard(UPSPOpenGLRenderDevice::ReadPixels);

	unguard;
}

void UPSPOpenGLRenderDevice::ClearZ( FSceneNode* Frame )
{
	guard(UPSPOpenGLRenderDevice::ClearZ);

	SetBlend( PF_Occlude );
	glClear( GL_DEPTH_BUFFER_BIT );

	unguard;
}

void UPSPOpenGLRenderDevice::SetSceneNode( FSceneNode* Frame )
{
	guard(UPSPOpenGLRenderDevice::SetSceneNode);

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

	if( Frame->FX != CurrentSceneNode.FX || Frame->FY != CurrentSceneNode.FY ||
			Viewport->Actor->FovAngle != CurrentSceneNode.FovAngle )
	{
		RProjZ = appTan( Viewport->Actor->FovAngle * PI / 360.0 );
		Aspect = Frame->FY / Frame->FX;
		RFX2 = 2.0f * RProjZ / Frame->FX;
		RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glFrustumf( -RProjZ, +RProjZ, -Aspect * RProjZ, +Aspect * RProjZ, 1.0f, 32768.0f );
		CurrentSceneNode.FX = Frame->FX;
		CurrentSceneNode.FY = Frame->FY;
		CurrentSceneNode.FovAngle = Viewport->Actor->FovAngle;
	}

	unguard;
}

void UPSPOpenGLRenderDevice::SetBlend( DWORD PolyFlags, UBOOL InverseOrder )
{
	guard(UPSPOpenGLRenderDevice::SetBlend);

	// Adjust PolyFlags according to Unreal's precedence rules.
	if( !(PolyFlags & (PF_Translucent|PF_Modulated)) )
		PolyFlags |= PF_Occlude;
	else if( PolyFlags & PF_Translucent )
		PolyFlags &= ~PF_Masked;

	// Detect changes in the blending modes.
	DWORD Xor = CurrentPolyFlags ^ PolyFlags;
	if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted) )
	{
		if( Xor&(PF_Translucent|PF_Modulated|PF_Highlighted) )
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
		{
			glDepthMask( (PolyFlags & PF_Occlude) != 0 );
		}
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

void UPSPOpenGLRenderDevice::ResetTexture()
{
	guard(UPSPOpenGLRenderDevice::ResetTexture);

	if( TexInfo.CurrentCacheID != 0 )
	{
		uclock(BindCycles);
		glBindTexture( GL_TEXTURE_2D, 0 );
		glDisable( GL_TEXTURE_2D );
		TexInfo.CurrentCacheID = 0;
		uunclock(BindCycles);
	}

	unguard;
}

void UPSPOpenGLRenderDevice::SetTexture( FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias )
{
	guard(UPSPOpenGLRenderDevice::SetTexture);

	// Set panning.
	FTexInfo& Tex = TexInfo;
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

	// Make current.
	uclock(BindCycles);
	Tex.CurrentCacheID = NewCacheID;
	FCachedTexture* Bind = BindMap.Find( NewCacheID );
	FCachedTexture* OldBind = Bind;
	if( !Bind )
	{
		// New texture.
		Bind = BindMap.Add( NewCacheID, FCachedTexture() );
		glGenTextures( 1, &Bind->Id );
	}

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, Bind->Id );
	uunclock(BindCycles);

	if( !OldBind || RealtimeChanged )
	{
		// Set mip filtering if there are mips.
		if( ( PolyFlags & PF_NoSmooth ) || ( NoFiltering && Info.Palette ) ) // TODO: This is set per poly, not per texture.
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		}
		else
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		}
		// New texture or it has changed, upload it.
		Info.TextureFlags &= ~TF_RealtimeChanged;
		UploadTexture( Info, ( PolyFlags & PF_Masked ) != 0, !OldBind );
	}

	unguard;
}

void UPSPOpenGLRenderDevice::EnsureComposeSize( const DWORD NewSize )
{
	if( NewSize > ComposeSize )
	{
		debugf( NAME_Warning, "Increasing compose buffer size: %u -> %u", ComposeSize, NewSize );
		free( Compose );
		Compose = (BYTE*)memalign( 64, NewSize );
		ComposeSize = NewSize;
	}
	verify( Compose );
}

void UPSPOpenGLRenderDevice::ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette, const UBOOL Masked, UBOOL NewTexture, BYTE*& UploadBuf, INT& USize, INT &VSize )
{
	// 8-bit indexed. We have to fix the alpha component since it's mostly garbage.
	UploadBuf = Mip->DataPtr;
	DWORD i;
	const BYTE* SrcPal = (const BYTE*)Palette;
	_WORD* DstPal = ComposePal;
	// only process palette when first creating the texture
	if( NewTexture )
	{
		i = 0;
		// index 0 is transparent in masked textures
		if( Masked )
		{
			*DstPal++ = 0;
			SrcPal += 4;
			++i;
		}
		// 1 alpha on the rest of the palette
		for( ; i < 256; ++i, SrcPal += 4 )
		{
			*DstPal++ = ((SrcPal[0] & 0xF8) >> 3) | ((SrcPal[1] & 0xF8) << 2) | ((SrcPal[2] & 0xF8) << 7) | 0x8000;
		}
		glColorTable( GL_TEXTURE_2D, GL_RGBA, 256, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, (const void*)ComposePal );
	}
}

void UPSPOpenGLRenderDevice::ConvertTextureMipBGRA7777( const FMipmap* Mip, BYTE*& UploadBuf, INT& USize, INT &VSize )
{
	// The PSP wants 16-bit textures to have a width that's a multiple of 8.
	USize = Max( USize, 8 );

	EnsureComposeSize( USize * VSize * 2 * 2 );
	UploadBuf = Compose;

	const BYTE* Src = (const BYTE*)Mip->DataPtr;
	const DWORD Count = Mip->USize * Mip->VSize;
	_WORD* Dst = (_WORD*)UploadBuf;

	// Convert to RGBA5551 and stretch to width=8 if necessary.
	const INT UTimes = USize / Mip->USize;
	for( DWORD i = 0; i < Count; ++i, Src += 4 )
	{
		const _WORD C = ( (Src[0] & 0x7C) >> 2) | ((Src[1] & 0x7E) << 4) | ((Src[2] & 0x7C) << 9 );
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

	// Apparently the PSP also wants 16-bit textures to have a height that's a multiple of 8.
	// Stretch to height=8 if necessary.
	VSize = Max( VSize, 8 );
	Src = UploadBuf;
	UploadBuf = (BYTE*)Dst;
	const INT VTimes = VSize / Mip->VSize;
	const INT SrcLine = USize << 1;
	for( INT i = 0; i < VSize; ++i, Src += SrcLine )
	{
		switch( VTimes )
		{
			case 8:
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				[[fallthrough]];
			case 4:
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				[[fallthrough]];
			case 2:
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				[[fallthrough]];
			default:
				appMemcpy( Dst, Src, SrcLine ); Dst += USize;
				break;
		}
	}
}

void UPSPOpenGLRenderDevice::UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture )
{
	guard(UPSPOpenGLRenderDevice::UploadTexture);

	if( !Info.Mips[0] )
	{
		debugf( NAME_Warning, "Encountered texture with invalid mips!" );
		return;
	}

	// Upload all mips.
	uclock(ImageCycles);
	// TODO: Figure out what the fuck is wrong with mip filtering (it's inverted?).
	const INT MipIndex = 0;
	{
		const FMipmap* Mip = Info.Mips[MipIndex];
		BYTE* UploadBuf;
		GLenum UploadFormat;
		GLenum InternalFormat;
		GLenum PixelType;
		if( !Mip || !Mip->DataPtr )
			return;
		INT USize = Mip->USize;
		INT VSize = Mip->VSize;
		// Convert texture if needed.
		if( Info.Palette )
		{
			UploadFormat = InternalFormat = GL_COLOR_INDEX8_EXT;
			PixelType = GL_UNSIGNED_BYTE;
			ConvertTextureMipI8( Mip, Info.Palette, Masked, NewTexture, UploadBuf, USize, VSize );
		}
		else
		{
			UploadFormat = InternalFormat = GL_RGB;
			PixelType = GL_UNSIGNED_SHORT_5_6_5;
			ConvertTextureMipBGRA7777( Mip, UploadBuf, USize, VSize );
		}
		// Upload to GL.
		if( NewTexture )
			glTexImage2D( GL_TEXTURE_2D, MipIndex, InternalFormat, USize, VSize, 0, UploadFormat, PixelType, (void*)UploadBuf );
		else
			glTexSubImage2D( GL_TEXTURE_2D, MipIndex, 0, 0, USize, VSize, UploadFormat, PixelType, (void*)UploadBuf );
	}
	uunclock(ImageCycles);

	unguard;
}
