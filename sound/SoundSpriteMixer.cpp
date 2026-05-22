#include "SoundSpriteMixer.h"
#include "audioNoClip.h"
#include "minorGems/util/SimpleVector.h"

#include <pthread.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// 内部数据结构（与 gameSDL.cpp 中 SoundSprite 等价）
// ============================================================================

namespace {

struct Sprite {
    int handle;
    int numSamples;
    int samplesPlayed;
    char noVariance;
    double samplesPlayedF;

    int16_t *samplesM;
    int16_t *samplesL;
    int16_t *samplesR;
};

static pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;

// 已注册的 sprite（不在播放线程里访问）
static SimpleVector<Sprite*> sSprites;

// 正在播放的实例（按值复制，避免播放过程中 free 释放掉数据）
static SimpleVector<Sprite> sPlaying( 100 );
static SimpleVector<double> sPlayingRates( 100 );
static SimpleVector<double> sPlayingVolumesL( 100 );
static SimpleVector<double> sPlayingVolumesR( 100 );

static int sNextHandle = 1;

// 全局参数
static double sRateMin = 1.0;
static double sRateMax = 1.0;
static double sVolumeMin = 1.0;
static double sVolumeMax = 1.0;

static float sGlobalLoudness = 1.0f;
static char sFading = false;
static float sFadeIncrementPerSample = 0.0f;
static float sLoudness = 1.0f;

static int sMaxSimultaneous = -1;

static double sMaxTotalVolume = 1.0;
static double sCompressionFraction = 0.0;
static double sNormalizeFactor = 1.0;
static NoClip sSpriteNoClip;
static NoClip sTotalNoClip;
static char sNoClipInitialized = false;

}  // namespace

// ============================================================================
// 工具
// ============================================================================

static double pickRandomVolume() {
    if (sVolumeMin == 1.0 && sVolumeMax == 1.0) return 1.0;
    double r = (double)rand() / (double)RAND_MAX;
    return sVolumeMin + r * (sVolumeMax - sVolumeMin);
}

static double pickRandomRate() {
    if (sRateMin == 1.0 && sRateMax == 1.0) return 1.0;
    double r = (double)rand() / (double)RAND_MAX;
    return sRateMin + r * (sRateMax - sRateMin);
}

// ============================================================================
// 注册 / 释放
// ============================================================================

extern "C" SoundSpriteMixerHandle SoundSpriteMixer_setMono(
        int16_t *samples, int numSamples) {
    if (samples == NULL || numSamples <= 0) return NULL;
    Sprite *s = new Sprite();
    s->handle = sNextHandle++;
    s->numSamples = numSamples;
    s->samplesPlayed = 0;
    s->samplesPlayedF = 0;
    s->noVariance = false;
    s->samplesM = new int16_t[numSamples];
    s->samplesL = NULL;
    s->samplesR = NULL;
    memcpy(s->samplesM, samples, numSamples * sizeof(int16_t));
    pthread_mutex_lock(&sMutex);
    sSprites.push_back(s);
    pthread_mutex_unlock(&sMutex);
    return (SoundSpriteMixerHandle)s;
}

extern "C" SoundSpriteMixerHandle SoundSpriteMixer_setStereo(
        int16_t *samplesL, int16_t *samplesR, int numSamples) {
    if (samplesL == NULL || samplesR == NULL || numSamples <= 0) return NULL;
    Sprite *s = new Sprite();
    s->handle = sNextHandle++;
    s->numSamples = numSamples;
    s->samplesPlayed = 0;
    s->samplesPlayedF = 0;
    s->noVariance = false;
    s->samplesM = NULL;
    s->samplesL = new int16_t[numSamples];
    s->samplesR = new int16_t[numSamples];
    memcpy(s->samplesL, samplesL, numSamples * sizeof(int16_t));
    memcpy(s->samplesR, samplesR, numSamples * sizeof(int16_t));
    pthread_mutex_lock(&sMutex);
    sSprites.push_back(s);
    pthread_mutex_unlock(&sMutex);
    return (SoundSpriteMixerHandle)s;
}

extern "C" void SoundSpriteMixer_free(SoundSpriteMixerHandle h) {
    if (h == NULL) return;
    Sprite *s = (Sprite*)h;
    pthread_mutex_lock(&sMutex);
    // 从播放列表移除
    for (int i = sPlaying.size() - 1; i >= 0; i--) {
        if (sPlaying.getElement(i)->handle == s->handle) {
            sPlaying.deleteElement(i);
            sPlayingRates.deleteElement(i);
            sPlayingVolumesL.deleteElement(i);
            sPlayingVolumesR.deleteElement(i);
        }
    }
    // 从注册表移除
    for (int i = 0; i < sSprites.size(); i++) {
        if (sSprites.getElementDirect(i)->handle == s->handle) {
            sSprites.deleteElement(i);
            break;
        }
    }
    pthread_mutex_unlock(&sMutex);
    delete [] s->samplesM;
    delete [] s->samplesL;
    delete [] s->samplesR;
    delete s;
}

// ============================================================================
// 锁
// ============================================================================

extern "C" void SoundSpriteMixer_lock()   { pthread_mutex_lock(&sMutex); }
extern "C" void SoundSpriteMixer_unlock() { pthread_mutex_unlock(&sMutex); }

// 占位实现 — Task 3 填充真混音、Task 4 填全局参数 / shutdown
extern "C" void SoundSpriteMixer_play(SoundSpriteMixerHandle, double, double) {}
extern "C" void SoundSpriteMixer_mix(uint8_t*, int, int) {}
extern "C" void SoundSpriteMixer_setMaxTotal(double, double) {}
extern "C" void SoundSpriteMixer_setMaxSimultaneous(int) {}
extern "C" void SoundSpriteMixer_setVolumeRange(double, double) {}
extern "C" void SoundSpriteMixer_setLoudness(float) {}
extern "C" void SoundSpriteMixer_fade(double, int) {}
extern "C" void SoundSpriteMixer_resume() {}
extern "C" void SoundSpriteMixer_toggleVariance(SoundSpriteMixerHandle, char) {}
extern "C" void SoundSpriteMixer_clearPlaying() {}
extern "C" void SoundSpriteMixer_shutdown() {}
