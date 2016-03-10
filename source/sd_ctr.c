#include "include/wl_def.h"

#include <sys/types.h>
#include <3ds.h>
#include "include/fmopl.h"

#define PACKED __attribute__((packed))

typedef	struct {
	longword length;
	word priority;
} PACKED SoundCommon;

typedef	struct {
	SoundCommon common;
	byte data[1];
} PACKED PCSound;

typedef	struct {
	byte mChar, cChar, mScale, cScale, mAttack, cAttack, mSus, cSus,
		mWave, cWave, nConn, voice, mode, unused[3];
} PACKED Instrument;

typedef	struct {
	SoundCommon common;
	Instrument inst;
	byte block, data[1];
} PACKED AdLibSound;

typedef	struct {
	word length, values[1];
} PACKED MusicGroup;

boolean AdLibPresent=true, SoundBlasterPresent=true, term=false;

SDMode SoundMode;
SMMode MusicMode;
SDSMode DigiMode;

static int Volume;
static int leftchannel, rightchannel;
static int L, R;

static volatile boolean sqActive;

static word *DigiList;

static boolean SD_Started;

#define NUM_SFX 8

static volatile boolean SoundPositioned[NUM_SFX];
static volatile int SoundPlaying[NUM_SFX];
static volatile int SoundPlayPos[NUM_SFX];
static volatile int SoundPlayLen[NUM_SFX];
static volatile int SoundPage[NUM_SFX];
static volatile int SoundLen[NUM_SFX];
static volatile byte *SoundData[NUM_SFX];
static volatile fixed SoundGX[NUM_SFX];
static volatile fixed SoundGY[NUM_SFX];
static volatile int CurDigi[NUM_SFX];

static FM_OPL *OPL;
static MusicGroup *Music;
static volatile int NewMusic;
static volatile int NewAdlib;
static volatile int AdlibPlaying;
static volatile int CurAdlib;

static boolean SPHack;

#define NUM_SAMPS 512

u16* sdlbuf;
static short int musbuf[NUM_SAMPS];
static int bufpos = 0;

static int MusicLength;
static int MusicCount;
static word *MusicData;
static byte AdlibBlock;
static byte *AdlibData;
static int AdlibLength;

static void SetSoundLoc(fixed gx, fixed gy);
static boolean SD_PlayDirSound(soundnames sound, fixed gx, fixed gy);

// 0 = csnd, 1 = dsp, 2 = none
int audioservice = 2;
u32 *threadStack;
Handle streamThread;

void SD_SetVolume(int vol)
{
	Volume = vol;
}

int SD_GetVolume()
{
	return Volume;
}

void InitSoundBuff(void){
	MusicLength = 0;
	MusicCount = 0;
	MusicData = NULL;
	AdlibBlock = 0;
	AdlibData = NULL;
	AdlibLength = -1;

	OPLWrite(OPL, 0x01, 0x20); /* Set WSE=1 */
	OPLWrite(OPL, 0x08, 0x00); /* Set CSM=0 & SEL=0 */
}

ndspWaveBuf* waveBuf;

void FillSoundBuff(void)
{

	int i, j, snd;
	short int samp;
	word dat;

	AdLibSound *AdlibSnd;
	Instrument *inst;

	// start by computing next buffer worth of music
	if ((MusicMode!=smm_Off) || (SoundMode!=sdm_Off)) {
		if (NewAdlib != -1) {
			AdlibPlaying = NewAdlib;
			AdlibSnd = (AdLibSound *)audiosegs[STARTADLIBSOUNDS+AdlibPlaying];
			inst = (Instrument *)&AdlibSnd->inst;
#define alChar		0x20
#define alScale		0x40
#define alAttack	0x60
#define alSus		0x80
#define alFeedCon	0xC0
#define alWave		0xE0

			OPLWrite(OPL, 0 + alChar, 0);
			OPLWrite(OPL, 0 + alScale, 0);
			OPLWrite(OPL, 0 + alAttack, 0);
			OPLWrite(OPL, 0 + alSus, 0);
			OPLWrite(OPL, 0 + alWave, 0);
			OPLWrite(OPL, 3 + alChar, 0);
			OPLWrite(OPL, 3 + alScale, 0);
			OPLWrite(OPL, 3 + alAttack, 0);
			OPLWrite(OPL, 3 + alSus, 0);
			OPLWrite(OPL, 3 + alWave, 0);
			OPLWrite(OPL, 0xA0, 0);
			OPLWrite(OPL, 0xB0, 0);

			OPLWrite(OPL, 0 + alChar, inst->mChar);
			OPLWrite(OPL, 0 + alScale, inst->mScale);
			OPLWrite(OPL, 0 + alAttack, inst->mAttack);
			OPLWrite(OPL, 0 + alSus, inst->mSus);
			OPLWrite(OPL, 0 + alWave, inst->mWave);
			OPLWrite(OPL, 3 + alChar, inst->cChar);
			OPLWrite(OPL, 3 + alScale, inst->cScale);
			OPLWrite(OPL, 3 + alAttack, inst->cAttack);
			OPLWrite(OPL, 3 + alSus, inst->cSus);
			OPLWrite(OPL, 3 + alWave, inst->cWave);

			//OPLWrite(OPL, alFeedCon, inst->nConn);
			OPLWrite(OPL, alFeedCon, 0);

			AdlibBlock = ((AdlibSnd->block & 7) << 2) | 0x20;
			AdlibData = (byte *)&AdlibSnd->data;
			AdlibLength = AdlibSnd->common.length*5;
			//OPLWrite(OPL, 0xB0, AdlibBlock);
			NewAdlib = -1;
		}

		if (NewMusic != -1) {
			NewMusic = -1;
			MusicLength = Music->length;
			MusicData = Music->values;
			MusicCount = 0;
		}
		for (i = 0; i < 4; i++) {
			if (sqActive) {
				while (MusicCount <= 0) {
					dat = *MusicData++;
					MusicCount = *MusicData++;
					MusicLength -= 4;
					OPLWrite(OPL, dat & 0xFF, dat >> 8);
				}
				if (MusicLength <= 0) {
					NewMusic = 1;
				}
				MusicCount-=2;
			}

			if (AdlibPlaying != -1) {
				if (AdlibLength == 0) {
					//OPLWrite(OPL, 0xB0, AdlibBlock);
				} else if (AdlibLength == -1) {
					OPLWrite(OPL, 0xA0, 00);
					OPLWrite(OPL, 0xB0, AdlibBlock);
					AdlibPlaying = -1;
				} else if ((AdlibLength % 5) == 0) {
					OPLWrite(OPL, 0xA0, *AdlibData);
					OPLWrite(OPL, 0xB0, AdlibBlock & ~2);
					AdlibData++;
				}
				AdlibLength--;
				if (AdlibLength == 0) {
					//OPLWrite(OPL, 0xB0, AdlibBlock);
				} else if (AdlibLength == -1) {
					OPLWrite(OPL, 0xA0, 00);
					OPLWrite(OPL, 0xB0, AdlibBlock);
					AdlibPlaying = -1;
				} else if ((AdlibLength % 5) == 0) {
					OPLWrite(OPL, 0xA0, *AdlibData);
					OPLWrite(OPL, 0xB0, AdlibBlock & ~2);
					AdlibData++;
				}
				AdlibLength--;
			}

			YM3812UpdateOne(OPL, &musbuf[i*NUM_SAMPS/4], NUM_SAMPS/4);
		}
	}
	
	//prefill audio buffer with music
	for (i = 0; i < NUM_SAMPS*2; i++)
		sdlbuf[i] = musbuf[i/2];

	// now add in sound effects
	for (j=0; j<NUM_SFX; j++) {
		if (SoundPlaying[j] == -1)
			continue;

		// compute L/R scale based on player position and stored sound position
		if (SoundPositioned[j]) {
			SetSoundLoc(SoundGX[j], SoundGY[j]);
			L = leftchannel;
			R = rightchannel;
		}

		// now add sound effect data to buffer
		for (i = 0; i < NUM_SAMPS*2; i += 2) {
			samp = (SoundData[j][(SoundPlayPos[j] >> 16)] << 8)^0x8000;
			if (SoundPositioned[j]) {
				snd = samp*(16-L)>>5;
				sdlbuf[i+0] += snd;
				snd = samp*(16-R)>>5;
				sdlbuf[i+1] += snd;
			} else {
				snd = samp>>2;
				sdlbuf[i+0] += snd;
				sdlbuf[i+1] += snd;
			}
			SoundPlayPos[j] += 10402; // 7000 / 44100 * 65536
			if ((SoundPlayPos[j] >> 16) >= SoundPlayLen[j]) {
				SoundPlayPos[j] -= (SoundPlayLen[j] << 16);
				SoundLen[j] -= 4096;
				SoundPlayLen[j] = (SoundLen[j] < 4096) ? SoundLen[j] : 4096;
				if (SoundLen[j] <= 0) {
					SoundPlaying[j] = -1;
					CurDigi[j] = -1;
					i = NUM_SAMPS*2;
				} else {
					SoundPage[j]++;
					SoundData[j] = PM_GetSoundPage(SoundPage[j]);
				}
			}
		}
	}
	DSP_FlushDataCache(sdlbuf, NUM_SAMPS*2);
}

static const u8 mix8[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
  0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
  0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
  0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
  0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
  0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
  0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
  0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71,
  0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C,
  0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92,
  0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D,
  0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
  0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
  0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
  0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
  0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4,
  0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
  0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
  0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
  0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
};

#define SDL_MIX_MAXVOLUME 128
#define ADJUST_VOLUME(s, v)	(s = (s*v)/SDL_MIX_MAXVOLUME)

void SDL_MixAudio (u8 *dst, const u8 *src, u32 len, int volume)
{
	s16 src1, src2;
	int dst_sample;
	const int max_audioval = ((1<<(16-1))-1);
	const int min_audioval = -(1<<(16-1));

	len /= 2;
	while ( len-- ) {
		src1 = ((src[1])<<8|src[0]);
		ADJUST_VOLUME(src1, volume);
		src2 = ((dst[1])<<8|dst[0]);
		src += 2;
		dst_sample = src1+src2;
		if ( dst_sample > max_audioval ) {
			dst_sample = max_audioval;
		} else
		if ( dst_sample < min_audioval ) {
			dst_sample = min_audioval;
		}
		dst[0] = dst_sample&0xFF;
		dst_sample >>= 8;
		dst[1] = dst_sample&0xFF;
		dst += 2;
	}
}


void SoundCallBack(void *unused, u8 *stream, int len)
{
	u8 *waveptr;
	int waveleft;

	waveptr = (u8*)sdlbuf + bufpos;
	waveleft = sizeof(sdlbuf) - bufpos;

    SDL_MixAudio(stream, waveptr, len, Volume*SDL_MIX_MAXVOLUME/10);
    bufpos += len;
	if (len >= waveleft)
		bufpos = 0;

    if (bufpos==0)
      FillSoundBuff();
}

void Blah()
{
    memptr  list;
    word    *p, pg;
    int     i;

	if (DigiList!=NULL)
		free(DigiList);

    MM_GetPtr(&list,PMPageSize);
    p = PM_GetPage(ChunksInFile - 1);
    memcpy((void *)list,(void *)p,PMPageSize);

    pg = PMSoundStart;
    for (i = 0;i < PMPageSize / (sizeof(word) * 2);i++,p += 2)
    {
	    if (pg >= ChunksInFile - 1)
        	break;
        pg += (p[1] + (PMPageSize - 1)) / PMPageSize;
    }
    MM_GetPtr((memptr *)&DigiList, i * sizeof(word) * 2);
    memcpy((void *)DigiList, (void *)list, i * sizeof(word) * 2);
    MM_FreePtr(&list);
}

// createDspBlock: Create a new block for DSP service
void createDspBlock(ndspWaveBuf* waveBuf, u16 bps, u32 size, u8 loop, u32* data){
	waveBuf->data_vaddr = (void*)data;
	waveBuf->nsamples = size / bps;
	waveBuf->looping = loop;
	waveBuf->offset = 0;	
	DSP_FlushDataCache(data, size);
}

void SD_Startup()
{
	int i;

	if (SD_Started)
		return;

	Blah();
	InitDigiMap();
	OPL = OPLCreate(OPL_TYPE_YM3812, 3579545, 44100);
	
	for (i=0; i<NUM_SFX; i++) {
		SoundPlaying[i] = -1;
		CurDigi[i] = -1;
	}

	CurAdlib = -1;
	NewAdlib = -1;
	NewMusic = -1;
	AdlibPlaying = -1;
	sqActive = false;
	sdlbuf = linearAlloc(NUM_SAMPS*2);
	memset(sdlbuf, 0, NUM_SAMPS*2);
	memset(musbuf, 0, NUM_SAMPS);
	hidScanInput();
	if (ndspInit() != 0) {
		printf("ERROR: failed to init audio\n");
		SD_Started = false;
    }else{ 
		printf("Audio-service: dsp::DSP\n");
		audioservice = 1;
		ndspSetOutputMode(NDSP_OUTPUT_STEREO);
		ndspChnReset(0x08);
		ndspChnWaveBufClear(0x08);
		ndspChnSetInterp(0x08, NDSP_INTERP_NONE);
		ndspChnSetRate(0x08, (float)44100);
		ndspChnSetFormat(0x08, NDSP_FORMAT_STEREO_PCM16);
		waveBuf = (ndspWaveBuf*)calloc(1, sizeof(ndspWaveBuf));
		createDspBlock(waveBuf, 2, NUM_SAMPS * 2, 1, sdlbuf);
		ndspSetCallback((ndspCallback)&SoundCallBack, NULL);
		ndspChnWaveBufAdd(0x08, waveBuf);
		printf("Configured audio device with %d samples", NUM_SAMPS);
		InitSoundBuff();
		SD_Started = true;
	}
	
}

void SD_Shutdown()
{
	if (!SD_Started)
		return;

	SD_MusicOff();
	SD_StopSound();

	SD_Started = false;
	if (audioservice == 1){
		ndspChnWaveBufClear(0x08);
		ndspExit();
	}else if (audioservice == 0){
		term = true;
		svcSleepThread(1000000000);
		svcCloseHandle(streamThread);
		free(threadStack);
		csndExit();
	}
	linearFree(sdlbuf);
	if (audioservice == 1) free(waveBuf);
	OPLDestroy(OPL);
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_PlaySound() - plays the specified sound on the appropriate hardware
//
///////////////////////////////////////////////////////////////////////////
boolean SD_PlaySound(soundnames sound)
{
	SPHack = false;
	return SD_PlayDirSound(sound, 0, 0);
}

static boolean SD_PlayDirSound(soundnames sound, fixed gx, fixed gy)
{
	SoundCommon *s;

	s = (SoundCommon *)audiosegs[STARTADLIBSOUNDS + sound];

	if (DigiMode != sds_Off) {
	if (DigiMap[sound] != -1) {
		int i;

		for (i=0; i<NUM_SFX; i++)
			if (SoundPlaying[i] == -1)
				break;
		if (i == NUM_SFX)
			for (i=0; i<NUM_SFX; i++)
				if (s->priority >= ((SoundCommon *)audiosegs[STARTADLIBSOUNDS+CurDigi[i]])->priority)
					break;
		if (i == NUM_SFX)
			return false; // no free channels, and priority not high enough to preempt a playing sound

		SoundPage[i] = DigiList[(DigiMap[sound] * 2) + 0];
		SoundData[i] = PM_GetSoundPage(SoundPage[i]);
		SoundLen[i] = DigiList[(DigiMap[sound] * 2) + 1];
		SoundPlayLen[i] = (SoundLen[i] < 4096) ? SoundLen[i] : 4096;
		SoundPlayPos[i] = 0;
		if (SPHack) {
			SPHack = false;
			SoundGX[i] = gx;
			SoundGY[i] = gy;
			SoundPositioned[i] = true;
		} else {
			SoundPositioned[i] = false;
		}
		CurDigi[i] = sound;
		SoundPlaying[i] = DigiMap[sound];
		return true;
	}
	}

	if (SoundMode != sdm_Off) {
	if ((AdlibPlaying == -1) || (CurAdlib == -1) ||
	(s->priority >= ((SoundCommon *)audiosegs[STARTADLIBSOUNDS+CurAdlib])->priority) ) {
		CurAdlib = sound;
		NewAdlib = sound;
		return true;
	}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_SoundPlaying() - returns the sound number of first playing sound,
//		or 0 if nothing playing
//
///////////////////////////////////////////////////////////////////////////
word SD_SoundPlaying()
{
	int i;

	for (i=0; i<NUM_SFX; i++)
		if (SoundPlaying[i] != -1)
			return CurDigi[i];

	if (AdlibPlaying != -1)
		return CurAdlib;

	return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_StopSound() - if a sound is playing, stops it
//
///////////////////////////////////////////////////////////////////////////
void SD_StopSound()
{
	int i;

	for (i=0; i<NUM_SFX; i++) {
		SoundPlaying[i] = -1;
		CurDigi[i] = -1;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_WaitSoundDone() - waits until the current sound is done playing
//
///////////////////////////////////////////////////////////////////////////
void SD_WaitSoundDone()
{
	while (SD_SoundPlaying()) ;
}

/*
==========================
=
= SetSoundLoc - Given the location of an object, munges the values
=	for an approximate distance from the left and right ear, and puts
=	those values into leftchannel and rightchannel.
=
= JAB
=
==========================
*/

#define ATABLEMAX 15
static const byte righttable[ATABLEMAX][ATABLEMAX * 2] = {
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 6, 0, 0, 0, 0, 0, 1, 3, 5, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 4, 0, 0, 0, 0, 0, 2, 4, 6, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6, 4, 1, 0, 0, 0, 1, 2, 4, 6, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 6, 5, 4, 2, 1, 0, 1, 2, 3, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 5, 4, 3, 2, 2, 3, 3, 5, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 4, 4, 4, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 5, 5, 6, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}
};
static const byte lefttable[ATABLEMAX][ATABLEMAX * 2] = {
{ 8, 8, 8, 8, 8, 8, 8, 8, 5, 3, 1, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 6, 4, 2, 0, 0, 0, 0, 0, 4, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 6, 4, 2, 1, 0, 0, 0, 1, 4, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 3, 2, 1, 0, 1, 2, 4, 5, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 5, 3, 3, 2, 2, 3, 4, 5, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 5, 4, 4, 4, 4, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 6, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}
};

static void SetSoundLoc(fixed gx, fixed gy)
{
	fixed xt, yt;
	int x, y;

// translate point to view centered coordinates
//
	gx -= viewx;
	gy -= viewy;

//
// calculate newx
//
	xt = FixedByFrac(gx,viewcos);
	yt = FixedByFrac(gy,viewsin);
	x = (xt - yt) >> TILESHIFT;

//
// calculate newy
//
	xt = FixedByFrac(gx,viewsin);
	yt = FixedByFrac(gy,viewcos);
	y = (yt + xt) >> TILESHIFT;

	if (y >= ATABLEMAX)
		y = ATABLEMAX - 1;
	else if (y <= -ATABLEMAX)
		y = -ATABLEMAX;
	if (x < 0)
		x = -x;
	if (x >= ATABLEMAX)
		x = ATABLEMAX - 1;

	leftchannel  =  lefttable[x][y + ATABLEMAX];
	rightchannel = righttable[x][y + ATABLEMAX];
}

/*
==========================
=
= SetSoundLocGlobal - Sets up globalsoundx & globalsoundy and then calls
=	UpdateSoundLoc() to transform that into relative channel volumes. Those
=	values are then passed to the Sound Manager so that they'll be used for
=	the next sound played (if possible).
=
==========================
*/

void PlaySoundLocGlobal(word s, int id, fixed gx, fixed gy)
{
	SPHack = true;
	SD_PlayDirSound(s, gx, gy);
}

// tell sound driver the current player moved/turned
void UpdateSoundLoc(fixed x, fixed y, int angle)
{
	// SetSoundLoc uses viewx, viewy, viewcos, viewsin in FillBuffer
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_MusicOn() - turns on the sequencer
//
///////////////////////////////////////////////////////////////////////////
void SD_MusicOn()
{
	sqActive = true;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_MusicOff() - turns off the sequencer and any playing notes
//
///////////////////////////////////////////////////////////////////////////
void SD_MusicOff()
{
	int j;

	sqActive = false;
	OPLResetChip(OPL);

	for (j=0; j<NUM_SAMPS; j++)
		musbuf[j] = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_StartMusic() - starts playing the music pointed to
//
///////////////////////////////////////////////////////////////////////////
void SD_StartMusic(int music)
{
	music += STARTMUSIC;

	CA_CacheAudioChunk(music);

	if (MusicMode!=sdm_Off) {
		SD_MusicOff();
		SD_MusicOn();
	}

	Music = (MusicGroup *)audiosegs[music];
	NewMusic = 1;
}

void SD_SetDigiDevice(SDSMode mode)
{
	if ((mode==sds_Off) || (mode==sds_SoundBlaster)) {
		DigiMode=mode;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_SetSoundMode() - Sets which sound hardware to use for sound effects
//
///////////////////////////////////////////////////////////////////////////
boolean SD_SetSoundMode(SDMode mode)
{
	if ((mode==sdm_Off) || (mode==sdm_AdLib)) {
		SoundMode=mode;
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_SetMusicMode() - sets the device to use for background music
//
///////////////////////////////////////////////////////////////////////////
boolean SD_SetMusicMode(SMMode mode)
{
	if (mode==smm_Off) {
		MusicMode=mode;
		SD_MusicOff();
		return true;
	}
	if (mode==smm_AdLib) {
		MusicMode=mode;
		//SD_MusicOn();
		return true;
	}
	return false;
}
