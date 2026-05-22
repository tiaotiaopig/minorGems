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

// ============================================================================
// 播放
// ============================================================================

extern "C" void SoundSpriteMixer_play(SoundSpriteMixerHandle h,
                                        double volumeTweak,
                                        double stereoPos) {
    if (h == NULL) return;
    Sprite *src = (Sprite*)h;
    pthread_mutex_lock(&sMutex);

    if (sFading && sGlobalLoudness == 0.0f) {
        pthread_mutex_unlock(&sMutex);
        return;
    }
    if (sMaxSimultaneous != -1 && sPlaying.size() >= sMaxSimultaneous) {
        pthread_mutex_unlock(&sMutex);
        return;
    }

    // 长音(>5秒)同时只允许一个实例
    int sampleRate = 22050;
    if (src->numSamples / sampleRate > 5) {
        for (int i = 0; i < sPlaying.size(); i++) {
            if (sPlaying.getElement(i)->handle == src->handle) {
                pthread_mutex_unlock(&sMutex);
                return;
            }
        }
    }

    double volume = volumeTweak;
    if (!src->noVariance) volume *= pickRandomVolume();
    double rate = src->noVariance ? 1.0 : pickRandomRate();

    // 等功率立体声分布
    double p = M_PI * stereoPos * 0.5;
    double rightVolume = volume * sin(p);
    double leftVolume  = volume * cos(p);

    // 复制 Sprite 头到 playing 列表（数据指针共用，由 sSprites 持有）
    Sprite copy = *src;
    copy.samplesPlayed = 0;
    copy.samplesPlayedF = 0;
    sPlaying.push_back(copy);
    sPlayingRates.push_back(rate);
    sPlayingVolumesL.push_back(leftVolume);
    sPlayingVolumesR.push_back(rightVolume);

    pthread_mutex_unlock(&sMutex);
}

// ============================================================================
// 全局参数
// ============================================================================

extern "C" void SoundSpriteMixer_setMaxTotal(double maxVolume,
                                              double compressFraction) {
    pthread_mutex_lock(&sMutex);
    sMaxTotalVolume = maxVolume;
    sCompressionFraction = compressFraction;
    sNormalizeFactor = 1.0 / (1.0 - compressFraction);
    int sr = 22050;
    sSpriteNoClip = resetAudioNoClip(
        (1.0 - compressFraction) * maxVolume * 32767,
        sr / 2, sr / 2);
    sTotalNoClip = resetAudioNoClip(32767, sr / 2, sr / 2);
    sNoClipInitialized = true;
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_setMaxSimultaneous(int n) {
    pthread_mutex_lock(&sMutex);
    sMaxSimultaneous = n;
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_setVolumeRange(double minV, double maxV) {
    pthread_mutex_lock(&sMutex);
    sVolumeMin = minV;
    sVolumeMax = maxV;
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_setLoudness(float v) {
    pthread_mutex_lock(&sMutex);
    sLoudness = v;
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_fade(double seconds, int sampleRate) {
    pthread_mutex_lock(&sMutex);
    sFading = true;
    if (seconds > 0) {
        sFadeIncrementPerSample = 1.0f / (seconds * sampleRate);
    } else {
        sFadeIncrementPerSample = 1.0f;  // 立即静音
    }
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_resume() {
    pthread_mutex_lock(&sMutex);
    sFading = false;
    sGlobalLoudness = 1.0f;
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_toggleVariance(SoundSpriteMixerHandle h,
                                                  char noVariance) {
    if (h == NULL) return;
    Sprite *s = (Sprite*)h;
    pthread_mutex_lock(&sMutex);
    s->noVariance = noVariance;
    pthread_mutex_unlock(&sMutex);
}

// ============================================================================
// 混音核心
// ============================================================================

extern "C" void SoundSpriteMixer_mix(uint8_t *inBuffer, int lengthBytes,
                                      int sampleRate) {
    int numSamples = lengthBytes / 4;  // stereo 16-bit
    if (numSamples <= 0) return;

    pthread_mutex_lock(&sMutex);

    if (sPlaying.size() == 0 && sLoudness == 1.0f) {
        // fast path：没 sprite 在播且无 loudness 缩放
        pthread_mutex_unlock(&sMutex);
        return;
    }

    if (!sNoClipInitialized) {
        sSpriteNoClip = resetAudioNoClip(0.5 * 32767,
                                          sampleRate / 2, sampleRate / 2);
        sTotalNoClip = resetAudioNoClip(32767,
                                          sampleRate / 2, sampleRate / 2);
        sNoClipInitialized = true;
    }

    // 静态扩容缓冲（避免每帧 malloc）
    static double *bufL = NULL;
    static double *bufR = NULL;
    static int bufSize = 0;
    if (numSamples > bufSize) {
        delete [] bufL;
        delete [] bufR;
        bufL = new double[numSamples];
        bufR = new double[numSamples];
        bufSize = numSamples;
    }

    if (sPlaying.size() > 0) {
        for (int i = 0; i < numSamples; i++) {
            bufL[i] = 0.0;
            bufR[i] = 0.0;
        }

        for (int i = 0; i < sPlaying.size(); i++) {
            Sprite *s = sPlaying.getElement(i);
            double rate = sPlayingRates.getElementDirect(i);
            double volL = sPlayingVolumesL.getElementDirect(i);
            double volR = sPlayingVolumesR.getElementDirect(i);

            int16_t *L, *R;
            if (s->samplesM != NULL) { L = R = s->samplesM; }
            else { L = s->samplesL; R = s->samplesR; }

            int filled = 0;
            if (rate == 1.0) {
                int played = s->samplesPlayed;
                while (filled < numSamples && played < s->numSamples) {
                    bufL[filled] += volL * L[played];
                    bufR[filled] += volR * R[played];
                    filled++;
                    played++;
                }
                s->samplesPlayed = played;
            } else {
                double playedF = s->samplesPlayedF;
                while (filled < numSamples && playedF < s->numSamples - 1) {
                    int a = (int)floor(playedF);
                    int b = (int)ceil(playedF);
                    double bw = playedF - a;
                    double aw = 1 - bw;
                    bufL[filled] += volL * (L[a] * aw + L[b] * bw);
                    bufR[filled] += volR * (R[a] * aw + R[b] * bw);
                    filled++;
                    playedF += rate;
                }
                s->samplesPlayedF = playedF;
            }
        }

        audioNoClip(&sSpriteNoClip, bufL, bufR, numSamples);
        if (sNormalizeFactor != 1.0) {
            for (int i = 0; i < numSamples; i++) {
                bufL[i] *= sNormalizeFactor;
                bufR[i] *= sNormalizeFactor;
            }
        }

        // 与 inBuffer 中已有的（音乐）混合
        int p = 0;
        for (int i = 0; i < numSamples; i++) {
            int16_t lIn = (int16_t)((inBuffer[p+1] << 8) | inBuffer[p]);
            int16_t rIn = (int16_t)((inBuffer[p+3] << 8) | inBuffer[p+2]);

            bufL[i] *= sGlobalLoudness;
            bufR[i] *= sGlobalLoudness;

            if (sFading) {
                sGlobalLoudness -= sFadeIncrementPerSample;
                if (sGlobalLoudness < 0.0f) sGlobalLoudness = 0.0f;
            }

            bufL[i] += lIn;
            bufR[i] += rIn;
            p += 4;
        }

        audioNoClip(&sTotalNoClip, bufL, bufR, numSamples);

        // 写回
        p = 0;
        for (int i = 0; i < numSamples; i++) {
            int16_t lOut = (int16_t)lrint(bufL[i]);
            int16_t rOut = (int16_t)lrint(bufR[i]);
            inBuffer[p++] = (uint8_t)(lOut & 0xFF);
            inBuffer[p++] = (uint8_t)((lOut >> 8) & 0xFF);
            inBuffer[p++] = (uint8_t)(rOut & 0xFF);
            inBuffer[p++] = (uint8_t)((rOut >> 8) & 0xFF);
        }

        // 清理已播放完的实例
        for (int i = sPlaying.size() - 1; i >= 0; i--) {
            Sprite *s = sPlaying.getElement(i);
            if (sGlobalLoudness == 0 ||
                s->samplesPlayed >= s->numSamples ||
                s->samplesPlayedF >= s->numSamples - 1) {
                sPlaying.deleteElement(i);
                sPlayingRates.deleteElement(i);
                sPlayingVolumesL.deleteElement(i);
                sPlayingVolumesR.deleteElement(i);
            }
        }
    }

    // 全局 loudness 缩放（不在 sPlaying 路径里时也需要）
    if (sLoudness != 1.0f) {
        int p = 0;
        for (int i = 0; i < numSamples; i++) {
            int16_t l = (int16_t)((inBuffer[p+1] << 8) | inBuffer[p]);
            int16_t r = (int16_t)((inBuffer[p+3] << 8) | inBuffer[p+2]);
            l = (int16_t)(l * sLoudness);
            r = (int16_t)(r * sLoudness);
            inBuffer[p++] = (uint8_t)(l & 0xFF);
            inBuffer[p++] = (uint8_t)((l >> 8) & 0xFF);
            inBuffer[p++] = (uint8_t)(r & 0xFF);
            inBuffer[p++] = (uint8_t)((r >> 8) & 0xFF);
        }
    }

    pthread_mutex_unlock(&sMutex);
}

// ============================================================================
// 关闭
// ============================================================================

extern "C" void SoundSpriteMixer_clearPlaying() {
    pthread_mutex_lock(&sMutex);
    sPlaying.deleteAll();
    sPlayingRates.deleteAll();
    sPlayingVolumesL.deleteAll();
    sPlayingVolumesR.deleteAll();
    pthread_mutex_unlock(&sMutex);
}

extern "C" void SoundSpriteMixer_shutdown() {
    pthread_mutex_lock(&sMutex);
    sPlaying.deleteAll();
    sPlayingRates.deleteAll();
    sPlayingVolumesL.deleteAll();
    sPlayingVolumesR.deleteAll();
    for (int i = 0; i < sSprites.size(); i++) {
        Sprite *s = sSprites.getElementDirect(i);
        delete [] s->samplesM;
        delete [] s->samplesL;
        delete [] s->samplesR;
        delete s;
    }
    sSprites.deleteAll();
    pthread_mutex_unlock(&sMutex);
}
