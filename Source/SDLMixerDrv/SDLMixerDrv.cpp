#include <stdlib.h>

#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"

#include "SDLMixerDrvPrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(SDLMixerDrv);
IMPLEMENT_CLASS(USDLMixerAudioSubsystem);

/*-----------------------------------------------------------------------------
	USDLMixerAudioSubsystem implementation.
-----------------------------------------------------------------------------*/

void USDLMixerAudioSubsystem::InternalClassInitializer( UClass* Class )
{
	guardSlow(USDLMixerAudioSubsystem::InternalClassInitializer);
	new(Class, "OutputRate",         RF_Public)UIntProperty   ( CPP_PROPERTY( OutputRate         ), "Audio", CPF_Config );
	new(Class, "MusicVolume",        RF_Public)UByteProperty  ( CPP_PROPERTY( MusicVolume        ), "Audio", CPF_Config );
	new(Class, "SoundVolume",        RF_Public)UByteProperty  ( CPP_PROPERTY( SoundVolume        ), "Audio", CPF_Config );
	new(Class, "MasterVolume",       RF_Public)UByteProperty  ( CPP_PROPERTY( MasterVolume       ), "Audio", CPF_Config );
	new(Class, "AmbientFactor",      RF_Public)UFloatProperty ( CPP_PROPERTY( AmbientFactor      ), "Audio", CPF_Config );
	new(Class, "MusicInterpolation", RF_Public)UByteProperty  ( CPP_PROPERTY( MusicInterpolation ), "Audio", CPF_Config );
	unguardSlow;
}

USDLMixerAudioSubsystem::USDLMixerAudioSubsystem()
{
	OutputRate = DEFAULT_OUTPUT_RATE;
	MasterVolume = 255;
	SoundVolume = 127;
	MusicVolume = 63;
	AmbientFactor = 0.6f;
	MusicInterpolation = 0;
}

UBOOL USDLMixerAudioSubsystem::Init()
{
	guard(USDLMixerAudioSubsystem::Init)

	Viewport = NULL;

	if( OutputRate <= 0 )
		OutputRate = DEFAULT_OUTPUT_RATE;

	Mix_Init( MIX_INIT_MOD );

	if( Mix_OpenAudio( OutputRate, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048 ) < 0 )
	{
		debugf( NAME_Warning, "Could not open audio: %s", SDL_GetError() );
		return false;
	}

	const INT Chans = Mix_AllocateChannels( MAX_SOURCES + 1 );
	debugf( NAME_Log, "SDL_mixer: opened audio device: frequency=%d sources=%d", OutputRate, MAX_SOURCES + 1 );

	// Set ourselves up as the audio subsystem.
	USound::Audio = this;
	UMusic::Audio = this;

	return true;

	unguard;
}

void USDLMixerAudioSubsystem::Destroy()
{
	guard(USDLMixerAudioSubsystem::Destroy)

	USound::Audio = NULL;
	UMusic::Audio = NULL;

	Mix_CloseAudio();

	Super::Destroy();

	unguard;
}

void USDLMixerAudioSubsystem::ShutdownAfterError()
{
	guard(USDLMixerAudioSubsystem::Destroy)

	USound::Audio = NULL;
	UMusic::Audio = NULL;

	Mix_CloseAudio();

	Super::ShutdownAfterError();

	unguard;
}

void USDLMixerAudioSubsystem::PostEditChange()
{
	guard(USDLMixerAudioSubsystem::PostEditChange)

	Super::PostEditChange();

	unguard;
}

void USDLMixerAudioSubsystem::SetViewport( UViewport* InViewport )
{
	guard(USDLMixerAudioSubsystem::SetViewport)

	// Stop all sounds before viewport change.
	for( INT i = 0; i < MAX_SOURCES; ++i )
		StopVoice( i );

	// Stop and free music if the viewport has changed.
	if( InViewport != Viewport )
	{
		if( Music )
		{
			UnregisterMusic( Music );
			Music = NULL;
		}
	}

	Viewport = InViewport;

	unguard;
}

void USDLMixerAudioSubsystem::RegisterMusic( UMusic* Mus )
{
	guard(USDLMixerAudioSubsystem::RegisterMusic)

	return;

	if( Mus->Handle || !Mus->Data.Num() )
		return;

	SDL_RWops* RW =  SDL_RWFromConstMem( (void*)&Mus->Data(0), Mus->Data.Num() );
	Mix_Music* MixMus = Mix_LoadMUS_RW( RW, SDL_TRUE );
	if( !MixMus )
	{
		debugf( "Failed to load music '%s': %s", Mus->GetName(), SDL_GetError() );
		return;
	}

	Mus->Handle = (void*)MixMus;
	MusicIsLoaded = true;

	unguard;
}

void USDLMixerAudioSubsystem::UnregisterMusic( UMusic* Mus )
{
	guard(USDLMixerAudioSubsystem::UnregisterMusic)

	StopMusic();

	if( Mus && Mus->Handle )
	{
		Mix_FreeMusic( (Mix_Music*)Mus->Handle );
		Mus->Handle = NULL;
	}

	MusicIsLoaded = false;

	unguard;
}

void USDLMixerAudioSubsystem::RegisterSound( USound* Sound )
{
	guard(USDLMixerAudioSubsystem::RegisterSound)

	if( Sound->Handle || !Sound->Data.Num() )
	{
		return;
	}


	SDL_RWops* RW =  SDL_RWFromConstMem( (void*)&Sound->Data(0), Sound->Data.Num() );
	Mix_Chunk* Chunk = Mix_LoadWAV_RW( RW, SDL_TRUE );
	if( !Chunk )
	{
		debugf( "Failed to load sound '%s': %s", Sound->GetName(), SDL_GetError() );
		return;
	}

	Sound->Handle = (void*)Chunk;

	// Check if there is a loop block in the WAV data.
	Sound->Looping = false;
	const BYTE* Src = &Sound->Data(0);
	for( INT i = 0; i < Sound->Data.Num(); ++i, ++Src )
	{
		if( !memcmp( (const void*)Src, (const void*)"smpl", 4 ) )
		{
			FSampleChunk SampleChunk;
			memcpy( &SampleChunk, (FSampleChunk*)( Src + 8 ), sizeof( SampleChunk ) );
			if( SampleChunk.cSampleLoops > 0 )
				Sound->Looping = true;
			break;
		}
	}

	if( !GIsEditor )
		Sound->Data.Empty();

	unguard;
}

void USDLMixerAudioSubsystem::UnregisterSound( USound* Sound )
{
	guard(USDLMixerAudioSubsystem::UnregisterSound)

	if( Sound && Sound->Handle )
	{
		Mix_FreeChunk( (Mix_Chunk*)Sound->Handle );
		Sound->Handle = NULL;
	}

	unguard;
}

void USDLMixerAudioSubsystem::UpdateVoice( INT Num, const ENVoiceOp Op )
{
	guard(USDLMixerAudioSubsystem::UpdateVoice)

	FNVoice& Voice = Voices[Num];

	// Spatialize, if we're not too close.
	FLOAT PanL, PanR;
	const FVector Delta = Voice.Location.TransformPointBy( ListenerCoords );
	const FLOAT InnerRadSq = 0.01f * Voice.Radius * Voice.Radius;
	const FLOAT DistSq = Delta.SizeSquared();
	if( DistSq < InnerRadSq )
	{
		PanL = 250.f * Voice.Volume;
		PanR = PanL;
	}
	else
	{
		const FLOAT Pan = appAtan2( Delta.X, Abs( Delta.Z ) );
		const FLOAT Dist = appSqrt( DistSq );
		const FLOAT PanFactor = Clamp( 0.5f + Pan/(FLOAT)PI, 0.f, 1.f );
		const FLOAT VolFactor = Clamp( 1.f - Dist/Voice.Radius, 0.f, 1.f ) * Voice.Volume;
		PanL = PanFactor * 250.f;
		PanR = 250.f - PanL;
		PanL *= VolFactor;
		PanR *= VolFactor;
	}

	Mix_SetPanning( Num, PanL, PanR );

	// Play or stop if needed.
	switch( Op )
	{
		case ENVoiceOp::NVOP_Play:
			if( Voice.Sound )
			{
				if( Voice.Paused && Voice.Sound->Handle == (void*)Mix_GetChunk( Num ) )
					Mix_Resume( Num );
				else
					Mix_PlayChannel( Num, (Mix_Chunk*)Voice.Sound->Handle, Voice.Sound->Looping ? -1 : 0 );
			}
			Voice.Paused = false;
			break;
		case ENVoiceOp::NVOP_Pause:
			Mix_Pause( Num );
			Voice.Paused = true;
			break;
		case ENVoiceOp::NVOP_Stop:
			Mix_HaltChannel( Num );
			Voice.Paused = false;
			return; // nothing else to do
		default:
			break;
	}

	unguard;
}

UBOOL USDLMixerAudioSubsystem::PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch )
{
	guard(USDLMixerAudioSubsystem::PlaySound)

	if( !Viewport )
		return false;

	// Allocate a new slot if requested.
	if( SOUND_SLOT_IS( Id, SLOT_None ) )
		Id = 16 * --NextId;

	FLOAT Priority = GetVoicePriority( Location, Volume, Radius );
	FLOAT MaxPriority = Priority;
	FNVoice* Voice = NULL;
	for( INT i = 0; i < MAX_SOURCES; ++i )
	{
		FNVoice* V = &Voices[i];
		if( ( V->Id & ~1 ) == ( Id & ~1 ) )
		{
			// Skip if not interruptable.
			if( Id & 1 )
				return false;
			StopVoice( i );
			Voice = V;
			break;
		}
		else if( V->Priority <= MaxPriority )
		{
			MaxPriority = V->Priority;
			Voice = V;
		}
	}

	// If we ran out of voices or the sound is too low priority, bail.
	if( !Voice || !Sound || !Sound->Handle )
		return false;

	Voice->Id = Id;
	Voice->Location = Location;
	Voice->Velocity = Actor ? Actor->Velocity : FVector();
	Voice->Volume = Clamp( Volume, 0.f, 1.f );
	Voice->Radius = Radius;
	Voice->Pitch = Pitch;
	Voice->Priority = Priority;
	Voice->Actor = Actor;
	Voice->Sound = Sound;

	// Start the voice.
	UpdateVoice( Voice - Voices, NVOP_Play );

	return true;

	unguard;
}

void USDLMixerAudioSubsystem::NoteDestroy( AActor* Actor )
{
	guard(USDLMixerAudioSubsystem::NoteDestroy)

	check(Actor);
	check(Actor->IsValid());
	for( INT i = 0; i < MAX_SOURCES; ++i)
	{
		if( Voices[i].Actor == Actor )
		{
			if( SOUND_SLOT_IS( Voices[i].Id, SLOT_Ambient ) )
			{
				// Stop ambient sound when actor dies.
				StopVoice( i );
			}
			else
			{
				// Unbind regular sounds from actors.
				Voices[i].Actor = NULL;
			}
		}
	}

	unguard;
}

void USDLMixerAudioSubsystem::StopVoice( INT Num )
{
	guard(USDLMixerAudioSubsystem::StopVoice)

	FNVoice& Voice = Voices[Num];

	Mix_HaltChannel( Num );

	Voice.Priority = 0.f;
	Voice.Sound = NULL;
	Voice.Actor = NULL;
	Voice.Id = 0;
	Voice.Paused = false;

	unguard;
}

void USDLMixerAudioSubsystem::PlayMusic()
{
	guard(USDLMixerAudioSubsystem::PlayMusic)

	MusicIsPlaying = true;

	unguard;
}

void USDLMixerAudioSubsystem::StopMusic()
{
	guard(USDLMixerAudioSubsystem::StopMusic)

	MusicIsPlaying = false;

	unguard;
}

void USDLMixerAudioSubsystem::Update( FPointRegion Region, FCoords& Listener )
{
	guard(USDLMixerAudioSubsystem::Update)

	if( !Viewport || !Viewport->IsRealtime() )
		return;

	ListenerCoords = Listener;

	// Start new ambient sounds if needed.
	if( Viewport->Actor && Viewport->Actor->XLevel )
	{
		for( INT i = 0; i < Viewport->Actor->XLevel->Num(); i++ )
		{
			AActor* Actor = Viewport->Actor->XLevel->Actors(i);
			if( !Actor || !Actor->IsValid() )
				continue;

			const FLOAT DistSq = FDistSquared( Viewport->Actor->Location, Actor->Location );
			const FLOAT AmbRad = Square( Actor->WorldSoundRadius() );
			if( !Actor->AmbientSound || DistSq > AmbRad )
				continue;

			// See if it's already playing.
			INT Id = AMBIENT_SOUND_ID( Actor->GetIndex() );
			INT AmbientNum;
			for( AmbientNum = 0; AmbientNum < MAX_SOURCES; ++AmbientNum )
			{
				if( Voices[AmbientNum].Id == Id )
					break;
			}

			// If not, start it.
			if( AmbientNum == MAX_SOURCES )
			{
				FLOAT Vol = AmbientFactor * Actor->SoundVolume / 255.f;
				FLOAT Rad = Actor->WorldSoundRadius();
				FLOAT Pitch = Actor->SoundPitch / 64.f;
				PlaySound( Actor, Id, Actor->AmbientSound, Actor->Location, Vol, Rad, Pitch );
			}
		}
	}

	// Update active ambient sounds.
	for( INT VoiceNum = 0; VoiceNum < MAX_SOURCES; ++VoiceNum )
	{
		FNVoice& Voice = Voices[VoiceNum];
		if( !Voice.Id || !Voice.Sound || !SOUND_SLOT_IS( Voice.Id, SLOT_Ambient ) )
			continue;

		check( Voice.Actor );

		const FLOAT DistSq = FDistSquared( Viewport->Actor->Location, Voice.Actor->Location );
		const FLOAT AmbRad = Square( Voice.Actor->WorldSoundRadius() );
		if( Voice.Sound != Voice.Actor->AmbientSound || DistSq > AmbRad )
		{
			// Sound changed or went out of range.
			StopVoice( VoiceNum );
		}
		else
		{
			// Update parameters. These will be applied in the loop below.
			Voice.Radius = Voice.Actor->WorldSoundRadius();
			Voice.Pitch = Voice.Actor->SoundPitch / 64.f;
			Voice.Volume = AmbientFactor * Voice.Actor->SoundVolume / 255.f;
			if( Voice.Actor->LightType != LT_None )
				Voice.Volume *= Voice.Actor->LightBrightness / 255.f;
		}
	}

	// Update all active voices.
	for( INT VoiceNum = 0; VoiceNum < MAX_SOURCES; ++VoiceNum )
	{
		FNVoice& Voice = Voices[VoiceNum];
		if( !Voice.Id || !Voice.Sound )
			continue;

		if( !Mix_Playing( VoiceNum ) )
		{
			// Voice has finished playing.
			StopVoice( VoiceNum );
		}
		else if( !Voice.Paused )
		{
			// Voice is playing, update its location and priority.
			if( Voice.Actor && Voice.Actor->IsValid() )
			{
				Voice.Location = Voice.Actor->Location;
				Voice.Velocity = Voice.Actor->Velocity;
			}
			Voice.Priority = GetVoicePriority( Voice.Location, Voice.Volume, Voice.Radius );
			// Update AL source.
			UpdateVoice( VoiceNum );
		}
	}

	// Update music.
	FLOAT DeltaTime = appSeconds() - MusicTime;
	MusicTime += DeltaTime;
	DeltaTime = Clamp( DeltaTime, 0.f, 1.f );
	if( Viewport->Actor && Viewport->Actor->Transition != MTRAN_None )
	{
		// Track is changing.
		UBOOL MusicChanged = Music != Viewport->Actor->Song;
		if( Music )
		{
			// Already playing something, figure out if we're ready to change.
			UBOOL MusicDone = false;
			if( MusicSection == 255 )
			{
				MusicDone = true;
			}
			else if( Viewport->Actor->Transition == MTRAN_Fade )
			{
				MusicFade -= DeltaTime;
				MusicDone = ( MusicFade < -2.f / 1000.f );
			}
			else if( Viewport->Actor->Transition == MTRAN_SlowFade )
			{
				MusicFade -= DeltaTime * 0.2;
				MusicDone = ( MusicFade < 0.2f * -2.f / 1000.f );
			}
			else if( Viewport->Actor->Transition == MTRAN_FastFade )
			{
				MusicFade -= DeltaTime * 3.0;
				MusicDone = ( MusicFade < 3.0f * -2.f / 1000.f );
			}
			else
			{
				MusicDone = true;
			}

			if( MusicDone )
			{
				if( Music && MusicChanged )
					UnregisterMusic( Music );
				Music = NULL;
			}
			else
			{
				// alSourcef( MusicSource, AL_GAIN, Max(MusicFade, 0.f) * MusicVolume / 255.f );
			}
		}

		if( Music == NULL )
		{
			MusicFade = 1.f;
			// alSourcef( MusicSource, AL_GAIN, Max(MusicFade, 0.f) * MusicVolume / 255.f );
			Music = Viewport->Actor->Song;
			MusicSection = Viewport->Actor->SongSection;
			if( Music )
			{
				if( MusicChanged )
					RegisterMusic( Music );
				if( MusicSection != 255 )
					PlayMusic();
				else
					StopMusic();
			}
			Viewport->Actor->Transition = MTRAN_None;
		}
	}

	unguard;
}

UBOOL USDLMixerAudioSubsystem::Exec( const char* Cmd, FOutputDevice* Out )
{
	guard(USDLMixerAudioSubsystem::Exec)

	if( ParseCommand( &Cmd, "MusicOrder") )
	{
		if( Music && MusicIsPlaying )
		{
			INT Pos = atoi( Cmd );
			Out->Logf( "Set music position to %d", Pos );

			MusicSection = Pos;
			return true;
		}
	}
	else if( ParseCommand( &Cmd, "MusicInterp" ) )
	{
		MusicInterpolation = atoi( Cmd );
		return true;
	}

	return false;

	unguard;
}
