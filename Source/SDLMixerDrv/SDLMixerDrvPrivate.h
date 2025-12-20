/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"
#include "Engine.h"

/*------------------------------------------------------------------------------------
	OpenAL audio subsystem private definitions.
------------------------------------------------------------------------------------*/

#define MAX_SOURCES 31
#define MUSIC_SOURCE (MAX_SOURCES)

#define SOUND_SLOT_IS( Id, Slot ) ( ( (Id) & 14 ) == (Slot) * 2 )
#define AMBIENT_SOUND_ID( ActorIndex ) ( (ActorIndex) * 16 + SLOT_Ambient * 2 )

#define DEFAULT_OUTPUT_RATE 22050

class DLL_EXPORT USDLMixerAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT(USDLMixerAudioSubsystem, UAudioSubsystem, CLASS_Config)

	// Options
	INT OutputRate;
	BYTE MasterVolume;
	BYTE SoundVolume;
	BYTE MusicVolume;
	FLOAT AmbientFactor;
	BYTE MusicInterpolation;

	// Constructors.
	static void InternalClassInitializer( UClass* Class );
	USDLMixerAudioSubsystem();

	// UObject interface.
	virtual void Destroy() override;
	virtual void PostEditChange() override;
	virtual void ShutdownAfterError() override;

	// UAudioSubsystem interface.
	virtual UBOOL Init() override;
	virtual void SetViewport( UViewport* Viewport ) override;
	virtual UBOOL Exec( const char* Cmd, FOutputDevice* Out = GSystem ) override;
	virtual void Update( FPointRegion Region, FCoords& Listener ) override;
	virtual void RegisterMusic( UMusic* Music ) override;
	virtual void RegisterSound( USound* Music ) override;
	virtual void UnregisterSound( USound* Sound ) override;
	virtual void UnregisterMusic( UMusic* Music ) override;
	virtual UBOOL PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch ) override;
	virtual void NoteDestroy( AActor* Actor );
	virtual UBOOL GetLowQualitySetting() override { return false; };

	// Internals.
private:
	UViewport* Viewport;
	INT NextId;
	FCoords ListenerCoords;

	UMusic* Music;
	FLOAT MusicFade;
	FLOAT MusicTime;
	BYTE MusicSection;
	UBOOL MusicIsPlaying = false;
	UBOOL MusicIsLoaded = false;

	enum ENVoiceOp
	{
		NVOP_None,
		NVOP_Play,
		NVOP_Stop,
		NVOP_Pause,
	};

	struct FNVoice
	{
		AActor* Actor;
		INT Id;
		USound* Sound;
		FVector Location;
		FVector Velocity;
		FLOAT Volume;
		FLOAT Radius;
		FLOAT Pitch;
		FLOAT Priority;
		UBOOL Paused;
	} Voices[MAX_SOURCES];

	void UpdateVoice( INT Num, const ENVoiceOp Op = NVOP_None );
	void StopVoice( INT Num );
	void PlayMusic();
	void StopMusic();

	inline FLOAT GetVoicePriority( const FVector& Location, FLOAT Volume, FLOAT Radius )
	{
		if( Radius && Viewport->Actor )
			return Volume * ( 1.f - (Location - Viewport->Actor->Location).Size() / Radius );
		else
			return Volume;
	}
};
