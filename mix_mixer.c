#ifndef MIX_MIXER_H
#define MIX_MIXER_H


#include "SDL2/SDL.h"
#include "physfs.h"

// TODO(Joey): Make this into a fuction that is passed at or before initiation
#ifndef MIX_MIXER_ASSERT
	#define MIX_MIXER_ASSERT(arg) SDL_assert_release(arg)
#endif

#ifndef MIX_MIXER_HEADER_ONLY
#define TODO_LOGGING(Name) printf("TODO(%s): Logging %s@%d\n", #Name, __FUNCTION__, __LINE__)

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DRFLAC_ASSERT(expression)        MIX_MIXER_ASSERT(expression)
#define DRFLAC_MALLOC(sz)                SDL_malloc((sz))
#define DRFLAC_REALLOC(p, sz)            SDL_realloc((p), (sz))
#define DRFLAC_FREE(p)                   SDL_free((p))
#define DRFLAC_COPY_MEMORY(dst, src, sz) SDL_memcpy((dst), (src), (sz))
#define DRFLAC_ZERO_MEMORY(p, sz)        SDL_memset((p), 0, (sz))
#include "dr_flac.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DRWAV_ASSERT(expression)        MIX_MIXER_ASSERT(expression)
#define DRWAV_MALLOC(sz)                SDL_malloc((sz))
#define DRWAV_REALLOC(p, sz)            SDL_realloc((p), (sz))
#define DRWAV_FREE(p)                   SDL_free((p))
#define DRWAV_COPY_MEMORY(dst, src, sz) SDL_memcpy((dst), (src), (sz))
#define DRWAV_ZERO_MEMORY(p, sz)        SDL_memset((p), 0, (sz))
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DRMP3_ASSERT(expression)        MIX_MIXER_ASSERT(expression)
#define DRMP3_MALLOC(sz)                SDL_malloc((sz))
#define DRMP3_REALLOC(p, sz)            SDL_realloc((p), (sz))
#define DRMP3_FREE(p)                   SDL_free((p))
#define DRMP3_COPY_MEMORY(dst, src, sz) SDL_memcpy((dst), (src), (sz))
#define DRMP3_ZERO_MEMORY(p, sz)        SDL_memset((p), 0, (sz))
#include "dr_mp3.h"

#define STBV_ASSERT(x) MIX_MIXER_ASSERT(x)
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis_physfs.c"
#endif //MIX_MIXER_HEADER_ONLY

//TODO(Joey): get rid of this
#define STS_MIXER_IMPLEMENTATION
#include "sts_mixer.h"


#ifdef __cplusplus
extern "C" {
#endif

enum
{
	MixSoundType_Invalid = 0,
	MixSoundType_Flac,
	MixSoundType_Wav,
	MixSoundType_Mp3,
	MixSoundType_Vorbis,
};

typedef struct
{
	int Type;
	int Frequency;
	int MaxSamples;
	int Channels;
	SDL_AudioFormat Format;
	PHYSFS_File *File;
	union
	{
		void *Data;
#ifndef MIX_MIXER_HEADER_ONLY
		drflac *Flac;
		drwav *Wav;
		drmp3 *Mp3;
		stb_vorbis *Vorbis;
#endif
	};
} audio_stream;

typedef struct mix_mixer mix_mixer;

//TODO(Joey): Make it so you can check if song is initiated
typedef struct
{
	mix_mixer *Mixer;
	sts_mixer_sample_t Sample;

#ifdef __cplusplus
	int Play(float, float, float);
	void Destroy();
#endif
} mix_sound;

//TODO(Joey): Make it so you can check if song is initiated
typedef struct
{
	mix_mixer *Mixer;
	sts_mixer_stream_t Stream;
	audio_stream *AudioStream;
	void *TempBuffer;
	Sint32 TempBufferSize;

#ifdef __cplusplus
	int Play(float);
	void Destroy();
#endif
} mix_song;

struct mix_mixer
{
	SDL_AudioDeviceID DeviceID;
	SDL_AudioSpec Spec;
	sts_mixer_t Mixer;

#ifdef __cplusplus
	int Init(Uint32, SDL_AudioFormat);
	int InitSpec(SDL_AudioSpec *);
	mix_sound *LoadSound(const char *);
	mix_song  *LoadSong(const char *);
#endif
};

int mix_mixer_Init(mix_mixer *, Uint32, SDL_AudioFormat);
int mix_mixer_InitSpec(mix_mixer *, SDL_AudioSpec *);
mix_sound *mix_mixer_LoadSound(mix_mixer *, const char *);
mix_song *mix_mixer_LoadSong(mix_mixer *, const char *);

int mix_sound_Play(mix_sound *, float, float, float);
void mix_sound_Destroy(mix_sound *);

int mix_song_Play(mix_song *, float);
void mix_song_Destroy(mix_song *);

#ifdef __cplusplus
}
#endif

#ifndef MIX_MIXER_HEADER_ONLY

static int
mix__SDLToSTSSampleFormat(SDL_AudioFormat Format)
{
	switch (Format)
	{
		case AUDIO_S8:     return STS_MIXER_SAMPLE_FORMAT_8;
		case AUDIO_S16SYS: return STS_MIXER_SAMPLE_FORMAT_16;
		case AUDIO_S32SYS: return STS_MIXER_SAMPLE_FORMAT_32;
		case AUDIO_F32SYS: return STS_MIXER_SAMPLE_FORMAT_FLOAT;
		default: return STS_MIXER_SAMPLE_FORMAT_NONE;
	}
}

static SDL_AudioFormat
mix__STSToSDLSampleFormat(int Format)
{
	switch (Format)
	{
		case STS_MIXER_SAMPLE_FORMAT_8:     return AUDIO_S8;
		case STS_MIXER_SAMPLE_FORMAT_16:    return AUDIO_S16SYS;
		case STS_MIXER_SAMPLE_FORMAT_32:    return AUDIO_S32SYS;
		case STS_MIXER_SAMPLE_FORMAT_FLOAT: return AUDIO_F32SYS;
		default: return 0;
	}
}

static int
mix__MixInit(mix_mixer *Mixer)
{
	SDL_AudioSpec Have;
	Mixer->DeviceID = SDL_OpenAudioDevice(NULL, 0, &Mixer->Spec, &Have, 0);
	int STSFormat = mix__SDLToSTSSampleFormat(Mixer->Spec.format);
	sts_mixer_init(&Mixer->Mixer, Mixer->Spec.freq, STSFormat);

	return 1;
}

static float
mix__Lerp(float A, float B, float T)
{
	float Result = (1.0f - T)*A + T*B;
	return Result;
}

typedef struct
{
	Uint32 Mask;
	float Gain;
} channel_modifier;

//TODO(Joey): More asserts and stuff
static int
mix__Resample(SDL_AudioFormat SrcFormat, int SrcChannels,
              int SrcSize, const void *SrcBuffer,
              SDL_AudioFormat DstFormat, int DstChannels,
              int DstSize, void *DstBuffer)
{
	const channel_modifier _mix_ChannelModifier[8][8][8] = {
		{
			{{0x01, 1.0f}},
			{{0x01, 0.5f}, {0x01, 0.5f}},
			{{0x01, 0.5f}, {0x01, 0.5f}, {0x01, 1.0f}},
			{{0x01, 0.5f}, {0x01, 0.5f}, {0x01, 0.5f}, {0x01, 0.5f}},
			{{0x01, 0.5f}, {0x01, 0.5f}, {0x01, 1.0f}, {0x01, 0.5f}, {0x01, 0.5f}},
			{{0x01, 0.5f}, {0x01, 0.5f}, {0x01, 1.0f}, {0x01, 1.0f}, {0x01, 0.5f}, {0x01, 0.5f}},
			{{0x01, 0.5f}, {0x01, 0.5f}, {0x01, 1.0f}, {0x01, 1.0f}, {0x01, 1.0f}, {0x01, 0.5f}, {0x01, 0.5f}},
			{{0x01, 0.5f}, {0x01, 0.5f}, {0x01, 1.0f}, {0x01, 1.0f}, {0x01, 0.5f}, {0x01, 0.5f}, {0x01, 0.5f}, {0x01, 0.5f}},
		},
		{
			{{0x03, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}, {0x03, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}, {0x01, 1.0f}, {0x02, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}, {0x03, 1.0f}, {0x01, 1.0f}, {0x02, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}, {0x03, 1.0f}, {0x03, 1.0f}, {0x01, 1.0f}, {0x02, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}, {0x03, 1.0f}, {0x03, 1.0f}, {0x03, 1.0f}, {0x01, 1.0f}, {0x02, 1.0f}},
			{{0x01, 1.0f}, {0x02, 1.0f}, {0x03, 1.0f}, {0x03, 1.0f}, {0x01, 1.0f}, {0x02, 1.0f}, {0x01, 1.0f}, {0x02, 1.0f}},
		},
	};

	if ((SrcChannels < 0 && SrcChannels >= 8) ||
	    (DstChannels < 0 && DstChannels >= 2)) // Expand to more output channels
	{
		TODO_LOGGING(Joey);
		return 0;
	}
	if ((SrcFormat != AUDIO_S8)     &&
	    (SrcFormat != AUDIO_S16SYS) &&
	    (SrcFormat != AUDIO_S32SYS) &&
	    (SrcFormat != AUDIO_F32SYS))
	{
		TODO_LOGGING(Joey);
		return 0;
	}
	if (DstFormat != AUDIO_F32SYS)
	{
		TODO_LOGGING(Joey);
		return 0;
	}
	const int SrcFormatSize = SDL_AUDIO_BITSIZE(SrcFormat)>>3;
	const int DstFormatSize = SDL_AUDIO_BITSIZE(DstFormat)>>3;
	const int SrcSamples = SrcSize/SrcFormatSize/SrcChannels;
	if (SrcSamples*SrcFormatSize*SrcChannels != SrcSize)
	{
		TODO_LOGGING(Joey);
		return 0;
	}
	const int DstSamples = DstSize/DstFormatSize/DstChannels;
	if (DstSamples*DstFormatSize*DstChannels != DstSize)
	{
		TODO_LOGGING(Joey);
		return 0;
	}

	const channel_modifier *ChannelModifier =_mix_ChannelModifier[DstChannels - 1][SrcChannels- 1];
	const float Multiplicand = 1.0f/(float)((1<<(SDL_AUDIO_BITSIZE(SrcFormat)-1))-1);
	const Uint32 NegativeBit = 1<<(SDL_AUDIO_BITSIZE(SrcFormat) - 1);
	const Uint64 NegetiveMask = 0xFFFFFFFFLL>>SDL_AUDIO_BITSIZE(SrcFormat)<<SDL_AUDIO_BITSIZE(SrcFormat);
	for (int DstIndex = 0;
	     DstIndex < DstSamples;
	     ++DstIndex)
	{
		float t = (float)DstIndex*(float)(SrcSamples - 1)/(float)(DstSamples - 1);
		int SrcIndex = (int)t;
		int NextIndex = (int)SDL_ceilf((float)(DstIndex + 1)*(float)(SrcSamples-1)/(float)(DstSamples-1));

		t = SDL_fmodf(t, 1.0f);
		void *Dst = ((Sint8 *)DstBuffer) + DstIndex*DstFormatSize*DstChannels;
		SDL_memset(Dst, '\0', DstFormatSize*DstChannels);
		void *Src[2];
		Src[0] = ((Sint8 *)SrcBuffer) + SrcIndex*SrcFormatSize*SrcChannels;
		Src[1] = (SrcIndex == (SrcSamples - 1)? Src[0]:
		          (Sint8 *)SrcBuffer + NextIndex*SrcFormatSize*SrcChannels);
		for (int SrcChannelIndex = 0;
		     SrcChannelIndex < SrcChannels;
		     ++SrcChannelIndex)
		{
			const channel_modifier Modifier = ChannelModifier[SrcChannelIndex];
			float TempFloat = 0.0f;
			if (SDL_AUDIO_ISFLOAT(SrcFormat))
			{
				float Temp[2];
				SDL_memcpy(&Temp[0], ((Sint8 *)Src[0] + SrcChannelIndex*SrcFormatSize), SrcFormatSize);
				SDL_memcpy(&Temp[1], ((Sint8 *)Src[1] + SrcChannelIndex*SrcFormatSize), SrcFormatSize);
				TempFloat = mix__Lerp(Temp[0], Temp[1], t);
			}
			else
			{
				Sint32 Temp[2] = {};
				SDL_memcpy(&Temp[0], ((Sint8 *)Src[0] + SrcChannelIndex*SrcFormatSize), SrcFormatSize);
				SDL_memcpy(&Temp[1], ((Sint8 *)Src[1] + SrcChannelIndex*SrcFormatSize), SrcFormatSize);
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				Temp[0] = SDL_Swap32(Temp[0]);
				Temp[1] = SDL_Swap32(Temp[1]);
	#endif
				Temp[0] |= (Temp[0] & NegativeBit)? NegetiveMask: 0;
				Temp[1] |= (Temp[1] & NegativeBit)? NegetiveMask: 0;
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				Temp[0] = SDL_Swap32(Temp[0]);
				Temp[1] = SDL_Swap32(Temp[1]);
	#endif
				TempFloat = mix__Lerp((float)Temp[0], (float)Temp[1], t)*Multiplicand;
			}
			for (int DstChannelIndex = 0;
			     DstChannelIndex < DstChannels;
			     ++DstChannelIndex)
			{
				const Uint32 Mask = 1 << DstChannelIndex;
				if (Modifier.Mask & Mask)
				{
					((float *)Dst)[DstChannelIndex] += TempFloat*Modifier.Gain;
				}
			}
		}
	}
	return 1;
}

#define MIX_MIXER_CHARS_TO_U32(A, B, C, D) SDL_FOURCC((D), (C), (B), (A))

static int
mix__Type_Test(PHYSFS_File *File)
{
	PHYSFS_sint32 Buffer;
	PHYSFS_readSBE32(File, &Buffer);
	int Result;
	switch(Buffer)
	{
		case MIX_MIXER_CHARS_TO_U32('f', 'L', 'a', 'C'):
		{
			return Result = MixSoundType_Flac;
		} break;
		case MIX_MIXER_CHARS_TO_U32('R', 'I', 'F', 'F'):
		{
			PHYSFS_seek(File, 8);
			PHYSFS_readSBE32(File, &Buffer);
			if (Buffer == MIX_MIXER_CHARS_TO_U32('W', 'A', 'V', 'E'))
				Result = MixSoundType_Wav;
			else
			{
				TODO_LOGGING(Joey);
				Result = MixSoundType_Invalid;
			}
		} break;
		case MIX_MIXER_CHARS_TO_U32('O', 'g', 'g', 'S'):
		{
			Result = MixSoundType_Vorbis;
		} break;
		default:
		{
			if ((Buffer&MIX_MIXER_CHARS_TO_U32('\xff', '\xff', '\xff', '\0')) == MIX_MIXER_CHARS_TO_U32('I', 'D', '3', '\0') ||
			    (Buffer&MIX_MIXER_CHARS_TO_U32('\xff', '\xff', '\0', '\0')) == MIX_MIXER_CHARS_TO_U32('\xFF', '\xFB', '\0', '\0'))
			{
				Result = MixSoundType_Mp3;
			}
			else
			{
				TODO_LOGGING(Joey);
				Result = MixSoundType_Invalid;
			}
		} break;
	}

	// Lets rewind to the start!
	PHYSFS_seek(File, 0);
	return Result;
}

static void
mix__audio_callback(void* userdata, Uint8 *stream, int len)
{
	mix_mixer *Mixer = (mix_mixer *)userdata;
	sts_mixer_mix_audio(&Mixer->Mixer, stream, len/(sizeof(float)*2));
}

#define DR_ON_READ(prefix) static size_t                                           \
mix__PhysFS_Dr##prefix##_on_read(void *UserData, void *Buffer, size_t BytesToRead) \
{                                                                                  \
	PHYSFS_File *File = (PHYSFS_File *)UserData;                                   \
	size_t BytesRead = PHYSFS_readBytes(File, Buffer, BytesToRead);                \
	return BytesRead;                                                              \
}

DR_ON_READ(flac)
DR_ON_READ(wav)
DR_ON_READ(mp3)

#define DR_ON_SEEK(prefix) static dr##prefix##_bool32                                         \
mix__PhysFS_Dr##prefix##_on_seek(void* UserData, int Offset, dr##prefix##_seek_origin Origin) \
{                                                                                             \
	PHYSFS_File *File = (PHYSFS_File *)UserData;                                              \
	PHYSFS_uint64 Cursor = 0;                                                                 \
	if (Origin == dr##prefix##_seek_origin_current)                                           \
	{                                                                                         \
		Cursor = PHYSFS_tell(File);                                                           \
		if (Cursor == -1)                                                                     \
		{                                                                                     \
			TODO_LOGGING(Joey);                                                               \
			return 0;                                                                         \
		}                                                                                     \
	}                                                                                         \
	if (!PHYSFS_seek(File, Cursor+Offset))                                                    \
	{                                                                                         \
		TODO_LOGGING(Joey);                                                                   \
		return 0;                                                                             \
	}                                                                                         \
	return 1;                                                                                 \
}

DR_ON_SEEK(flac)
DR_ON_SEEK(wav)
DR_ON_SEEK(mp3)


static void
mix__CloseAudioStream(audio_stream *Stream)
{
	if (Stream != NULL)
	{
		if (Stream->File != NULL)
		{
			PHYSFS_close(Stream->File);
		}
		if (Stream->Data != NULL)
		{
			switch (Stream->Type)
			{
				case MixSoundType_Flac:
				{
					drflac_close(Stream->Flac);
				} break;
				case MixSoundType_Wav:
				{
					drwav_close(Stream->Wav);
				} break;
				case MixSoundType_Mp3:
				{
					drmp3_uninit(Stream->Mp3);
					SDL_free(Stream->Mp3);
				} break;
				case MixSoundType_Vorbis:
				{
					stb_vorbis_close(Stream->Vorbis);
				} break;
				default:
				{
					TODO_LOGGING(Joey);
				} break;
			}
		}
		SDL_free(Stream);
	}
}

static audio_stream *
mix__OpenAudioStream(const char *FileName)
{
	if (!PHYSFS_exists(FileName))
	{
		TODO_LOGGING(Joey);
		return NULL;
	}

	audio_stream *Result = (audio_stream *)SDL_malloc(sizeof(audio_stream));
	if (Result == NULL)
	{
		TODO_LOGGING(Joey);
		return NULL;
	}
	SDL_memset(Result, '\0', sizeof(Result));

	Result->File = PHYSFS_openRead(FileName);
	if (Result->File == NULL)
	{
		TODO_LOGGING(Joey);
		SDL_free(Result);
		return NULL;
	}

	Result->Type = mix__Type_Test(Result->File);
	switch (Result->Type)
	{
		case MixSoundType_Flac:
		{
			Result->Flac = drflac_open_relaxed(mix__PhysFS_Drflac_on_read,
			                           mix__PhysFS_Drflac_on_seek, 0,
			                           (void *)Result->File);
			if (Result->Flac == NULL)
			{
				TODO_LOGGING(Joey);
				mix__CloseAudioStream(Result);
				return NULL;
			}
			Result->Frequency = Result->Flac->sampleRate;
			Result->MaxSamples = Result->Flac->totalSampleCount;
			Result->Channels = Result->Flac->channels;
			Result->Format = AUDIO_S32SYS;
		} break;
		case MixSoundType_Wav:
		{
			Result->Wav = drwav_open(mix__PhysFS_Drwav_on_read,
			                         mix__PhysFS_Drwav_on_seek,
			                         (void *)Result->File);
			if (Result->Wav == NULL)
			{
				TODO_LOGGING(Joey);
				mix__CloseAudioStream(Result);
				return NULL;
			}
			Result->Frequency = Result->Wav->sampleRate;
			Result->MaxSamples = Result->Wav->totalSampleCount;
			Result->Channels = Result->Wav->channels;
			Result->Format = AUDIO_S32SYS;
		} break;
		case MixSoundType_Mp3:
		{
			Result->Mp3 = (drmp3 *)SDL_malloc(sizeof(drmp3));
			int DidInit = drmp3_init(Result->Mp3,
			                         mix__PhysFS_Drmp3_on_read,
			                         mix__PhysFS_Drmp3_on_seek,
			                         (void *)Result->File,
			                         NULL);
			if (!DidInit)
			{
				TODO_LOGGING(Joey);
				mix__CloseAudioStream(Result);
				return NULL;
			}
			Result->Frequency = Result->Mp3->sampleRate;
			Result->MaxSamples = -1;
			Result->Channels = Result->Mp3->channels;
			Result->Format = AUDIO_F32SYS;
		} break;
		case MixSoundType_Vorbis:
		{
			// FILE *file = fopen("../data/01 Hints Followed By Guesses.ogg", "rb");
			int Error = 0;
			Result->Vorbis = stb_vorbis_open_file(Result->File, FALSE, &Error, NULL);
			if (Result->Vorbis == NULL)
			{
				printf("Error: %d, file: %s\n", Error, FileName);
				TODO_LOGGING(Joey);
				mix__CloseAudioStream(Result);
				return NULL;
			}
			Result->Frequency = Result->Vorbis->sample_rate;
			Result->MaxSamples = stb_vorbis_stream_length_in_samples(Result->Vorbis)*Result->Vorbis->channels;
			Result->Channels = Result->Vorbis->channels;
			Result->Format = AUDIO_F32SYS;
		} break;
		default:
		{
			TODO_LOGGING(Joey);
			mix__CloseAudioStream(Result);
			return NULL;
		} break;
	}
	return Result;
}

static int
mix__ReadAudioStream(audio_stream *Stream, int SamplesCount, void *Buffer)
{
	int SamplesRead = -1;
	switch (Stream->Type)
	{
		case MixSoundType_Flac:
		{
			SamplesRead = drflac_read_s32(Stream->Flac, SamplesCount, (Sint32 *)Buffer);
		} break;
		case MixSoundType_Wav:
		{
			SamplesRead = drwav_read_s32(Stream->Wav, SamplesCount, (Sint32 *)Buffer);
		} break;
		case MixSoundType_Mp3:
		{
			Uint64 FramesToRead = SamplesCount/Stream->Channels;
			SamplesRead = drmp3_read_f32(Stream->Mp3, FramesToRead, (float *)Buffer);
			SamplesRead *= Stream->Channels;
		} break;
		case MixSoundType_Vorbis:
		{
			Uint32 FramesToRead = SamplesCount;
			SamplesRead = stb_vorbis_get_samples_float_interleaved(Stream->Vorbis, Stream->Channels, (float *)Buffer, FramesToRead);
			SamplesRead *= Stream->Channels;
			int Error = stb_vorbis_get_error(Stream->Vorbis);
			if (Error != VORBIS__no_error)
			{
				TODO_LOGGING(Joey);
				printf("Error %d\n", Error);
			}
		} break;
	}
	return SamplesRead;
}

static void
mix__FreeAudioBuffer(audio_stream *Stream, void *Buffer)
{
	switch (Stream->Type)
	{
		case MixSoundType_Flac:
		case MixSoundType_Wav:
		case MixSoundType_Vorbis:
		{
			SDL_free(Buffer);
		} break;
		case MixSoundType_Mp3:
		{
			drmp3_free(Buffer);
		} break;
		default:
		{
			TODO_LOGGING(Joey);
		} break;
	}
}

static Sint32
mix__ReadAllAndCloseAudioStream(mix_mixer *Mixer, audio_stream *Stream,
                                void **Buffer)
{
	Sint64 SamplesRead = -1;
	if (Stream == NULL)
	{
		return -1;
	}
	switch (Stream->Type)
	{
		case MixSoundType_Flac:
		case MixSoundType_Wav:
		case MixSoundType_Vorbis:
		{
			MIX_MIXER_ASSERT(Stream->MaxSamples > 0);
			int SrcFormatSize = SDL_AUDIO_BITSIZE(Stream->Format)>>3;
			Uint64 BufferSize = Stream->MaxSamples*Stream->Channels*SrcFormatSize;
			*Buffer = SDL_malloc(BufferSize);
			if (Buffer == NULL)
			{
				TODO_LOGGING(Joey);
				return -1;
			}
			SDL_memset(*Buffer, '\0', BufferSize);

			SamplesRead = mix__ReadAudioStream(Stream, Stream->MaxSamples, *Buffer);
			if (SamplesRead < 0)
			{
				TODO_LOGGING(Joey);
				SDL_free(*Buffer);
				return -1;
			}
		} break;
		case MixSoundType_Mp3:
		{
			drmp3_uint64 FramesRead = 0;
			*Buffer = drmp3__full_decode_and_close_f32(Stream->Mp3, NULL, &FramesRead);
			SamplesRead = FramesRead*Stream->Channels;
		} break;
		default:
		{
			TODO_LOGGING(Joey);
		}
	}
	MIX_MIXER_ASSERT(SamplesRead <= 0xFFFFFFFFLL);
	return (Uint32)SamplesRead;
}

static int
mix__SeekAudioStream(audio_stream *Stream, Uint64 SampleIndex)
{
	int WasSuccess = 0;
	switch (Stream->Type)
	{
		case MixSoundType_Flac:
		{
			WasSuccess = drflac_seek_to_sample(Stream->Flac, SampleIndex);
		} break;
		case MixSoundType_Wav:
		{
			WasSuccess = drwav_seek_to_sample(Stream->Wav, SampleIndex);
		} break;
		case MixSoundType_Mp3:
		{
			int FrameToSeek = SampleIndex/Stream->Mp3->channels;
			WasSuccess = drmp3_seek_to_frame(Stream->Mp3, FrameToSeek);
		} break;
		case MixSoundType_Vorbis:
		{
			WasSuccess = stb_vorbis_seek(Stream->Vorbis, SampleIndex);
		} break;
	}
	return WasSuccess;
}

static void
mix__RefillStream(sts_mixer_sample_t* Sample, void* UserData)
{
	mix_song *Song = (mix_song *)UserData;
	MIX_MIXER_ASSERT((Song->AudioStream->Channels > 0) &&
	                 (Song->AudioStream->Channels <= 8));

	const int SamplesToRead = Song->TempBufferSize/sizeof(float)/2;
	const int SamplesRead = mix__ReadAudioStream(Song->AudioStream, SamplesToRead, (float *)Song->TempBuffer);
	const int BytesRead = SamplesRead*sizeof(float);
	const int SamplesToWrite = (int)SDL_ceilf((float)Song->Stream.sample.length*(float)SamplesRead/(float)SamplesToRead);
	const int BytesToWrite = (SamplesToWrite/Song->AudioStream->Channels*
	                          Song->AudioStream->Channels*sizeof(float));
	MIX_MIXER_ASSERT(SamplesToWrite <= Song->Stream.sample.length);
	mix__Resample(Song->AudioStream->Format, Song->AudioStream->Channels,
	              BytesRead, Song->TempBuffer,
	              Song->Mixer->Spec.format, 2,
	              BytesToWrite, Song->Stream.sample.data);
	// TODO(Joey): Throw this into a while loop or something.
	if (SamplesToWrite < Song->Stream.sample.length)
	{
		mix__SeekAudioStream(Song->AudioStream, 0);
	}
}


int
mix_mixer_Init(mix_mixer *Mixer, Uint32 Frequency, SDL_AudioFormat AudioFormat)
{
	SDL_zerop(Mixer);

	Mixer->Spec.freq     = Frequency;
	Mixer->Spec.format   = AudioFormat;
	Mixer->Spec.channels = 2;
	Mixer->Spec.samples  = 4096;
	Mixer->Spec.callback = mix__audio_callback;
	Mixer->Spec.userdata = Mixer;

	int Result = mix__MixInit(Mixer);
	return Result;
}

int
mix_mixer_InitSpec(mix_mixer *Mixer, SDL_AudioSpec *Spec)
{
	SDL_zerop(Mixer);

	Mixer->Spec = *Spec;

	int Result = mix__MixInit(Mixer);
	return Result;
}

mix_sound *
mix_mixer_LoadSound(mix_mixer *Mixer, const char *FileName)
{
	audio_stream *Stream = mix__OpenAudioStream(FileName);
	if (Stream == NULL)
	{
		TODO_LOGGING(Joey);
		return NULL;
	}

	mix_sound *Result = (mix_sound *)SDL_malloc(sizeof(mix_sound));
	if (Result == NULL)
	{
		TODO_LOGGING(Joey);
		mix__CloseAudioStream(Stream);
		return NULL;
	}
	SDL_memset(Result, '\0', sizeof(mix_sound));

	Result->Mixer = Mixer;

	void *Buffer;
	Uint32 SamplesRead = mix__ReadAllAndCloseAudioStream(Mixer, Stream, &Buffer);

	if (Buffer == NULL)
	{
		TODO_LOGGING(Joey);
		mix__CloseAudioStream(Stream);
		SDL_free(Result);
		return NULL;
	}

	int SrcFormatSize = SDL_AUDIO_BITSIZE(Stream->Format)>>3;
	int DstFormatSize = SDL_AUDIO_BITSIZE(Mixer->Spec.format)>>3;

	Uint32 SrcSize = SamplesRead*SrcFormatSize;

	Uint32 DstSize = (Uint32)SDL_ceilf((float)SamplesRead*(float)Mixer->Mixer.frequency/(float)Stream->Frequency);
	DstSize = DstSize/Stream->Channels*DstFormatSize/SrcFormatSize*DstFormatSize;

	Result->Sample.frequency = Mixer->Mixer.frequency;
	Result->Sample.audio_format = Mixer->Mixer.audio_format;
	Result->Sample.length = DstSize/DstFormatSize;
	Result->Sample.data = SDL_malloc(DstSize);
	mix__Resample(Stream->Format, Stream->Channels,
	              SrcSize, Buffer,
	              Mixer->Spec.format, 1,
	              DstSize, Result->Sample.data);
	mix__FreeAudioBuffer(Stream, Buffer);

	return Result;
}

mix_song *
mix_mixer_LoadSong(mix_mixer *Mixer, const char *FileName)
{
	audio_stream *Stream = mix__OpenAudioStream(FileName);
	if (Stream == NULL)
	{
		TODO_LOGGING(Joey);
		return NULL;
	}

	mix_song *Result = (mix_song *)SDL_malloc(sizeof(mix_song));
	if (Result == NULL)
	{
		TODO_LOGGING(Joey);
		mix__CloseAudioStream(Stream);
		return NULL;
	}
	SDL_memset(Result, '\0', sizeof(mix_song));

	Result->Mixer = Mixer;
	Result->AudioStream = Stream;

	int SamplesSize = Mixer->Spec.samples;
	Result->TempBufferSize = (Sint32)SDL_ceilf((float)SamplesSize*(float)Stream->Frequency/(float)Mixer->Mixer.frequency);
	Result->TempBufferSize = Result->TempBufferSize*Stream->Channels*sizeof(float);
	Result->TempBuffer = SDL_malloc(Result->TempBufferSize);
	if (Result->TempBuffer == NULL)
	{
		TODO_LOGGING(Joey);
		mix__CloseAudioStream(Stream);
		SDL_free(Result);
		return NULL;
	}
	SDL_memset(Result->TempBuffer, '\0', Result->TempBufferSize);


	// Dest Buffer Size in samples
	int DstSize = SamplesSize*sizeof(float);
	Result->Stream.userdata = Result;
	Result->Stream.callback = mix__RefillStream;
	Result->Stream.sample.frequency = Mixer->Mixer.frequency;
	Result->Stream.sample.audio_format = Mixer->Mixer.audio_format;
	Result->Stream.sample.length = DstSize/sizeof(float);
	Result->Stream.sample.data = SDL_malloc(DstSize);

	mix__RefillStream(&Result->Stream.sample, Result);
	return Result;
}

int
mix_sound_Play(mix_sound *Sound, float Gain, float Pitch, float Pan)
{
	//TODO(Joey): Make it so you can check if song is initiated
	if (Sound == NULL)
	{
		TODO_LOGGING(Joey);
		return -1;
	}
	int Result = sts_mixer_play_sample(&Sound->Mixer->Mixer, &Sound->Sample, Gain, Pitch, Pan);
	return Result;
}


void
mix_sound_Destroy(mix_sound *Sound)
{
	SDL_free(Sound->Sample.data);
	SDL_free(Sound);
}


int
mix_song_Play(mix_song *Song, float Gain)
{
	//TODO(Joey): Make it so you can check if song is initiated
	if (Song == NULL)
	{
		TODO_LOGGING(Joey);
		return -1;
	}
	int Result = sts_mixer_play_stream(&Song->Mixer->Mixer, &Song->Stream, Gain);
	return Result;
}

void
mix_song_Destroy(mix_song *Song)
{
	SDL_free(Song->Stream.sample.data);
	SDL_free(Song->TempBuffer);
	mix__CloseAudioStream(Song->AudioStream);
	SDL_free(Song);
}

#endif // MIX_MIXER_HEADER_ONLY

#ifdef __cplusplus
int
mix_mixer::Init(Uint32 Frequency, SDL_AudioFormat AudioFormat)
{
	int Result = mix_mixer_Init(this, Frequency, AudioFormat);
	return Result;
}

int
mix_mixer::InitSpec(SDL_AudioSpec *Spec)
{
	int Result = mix_mixer_InitSpec(this, Spec);
	return Result;
}

mix_sound *
mix_mixer::LoadSound(const char *FileName)
{
	mix_sound *Result = mix_mixer_LoadSound(this, FileName);
	return Result;
}

mix_song *
mix_mixer::LoadSong(const char *FileName)
{
	mix_song *Result = mix_mixer_LoadSong(this, FileName);
	return Result;
}


int
mix_sound::Play(float Gain, float Pitch, float Pan)
{
	int Result = mix_sound_Play(this, Gain, Pitch, Pan);
	return Result;
}

void
mix_sound::Destroy()
{
	mix_sound_Destroy(this);
}


int
mix_song::Play(float Gain)
{
	int Result = mix_song_Play(this, Gain);
	return Result;
}

void
mix_song::Destroy()
{
	mix_song_Destroy(this);
}

#endif // __cplusplus

#endif // MIX_MIXER_H