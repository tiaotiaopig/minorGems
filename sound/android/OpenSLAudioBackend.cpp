#ifdef __ANDROID__

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../SoundSpriteMixer.h"

#define LOG_TAG "OneLifeAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// minorGems 游戏音频接口（实际签名来自 game/game.h，C++ 链接）
typedef uint8_t Uint8;
void getSoundSamples(Uint8* inBuffer, int inLengthToFillInBytes);
int getSampleRate();

namespace {
    SLObjectItf engineObj = nullptr;
    SLEngineItf engineItf = nullptr;
    SLObjectItf mixObj = nullptr;
    SLObjectItf playerObj = nullptr;
    SLPlayItf playItf = nullptr;
    SLAndroidSimpleBufferQueueItf bqItf = nullptr;

    constexpr int kBufferFrames = 1024;  // 立体声帧数（46ms @22050Hz）
    constexpr int kBufferSize = kBufferFrames * 2 * sizeof(int16_t);  // 字节数
    uint8_t buffers[2][kBufferSize];  // 双缓冲
    int currentBuffer = 0;

    void fillAndEnqueue() {
        uint8_t* buf = buffers[currentBuffer];
        getSoundSamples(buf, kBufferSize);  // 背景音乐
        SoundSpriteMixer_mix(buf, kBufferSize, getSampleRate());  // 叠加音效
        (*bqItf)->Enqueue(bqItf, buf, kBufferSize);
        currentBuffer ^= 1;
    }

    void bqCallback(SLAndroidSimpleBufferQueueItf, void*) {
        fillAndEnqueue();
    }
}

extern "C" int minorGemsAndroid_audioStart() {
    LOGI("Initializing OpenSL ES audio...");

    SLresult r = slCreateEngine(&engineObj, 0, nullptr, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("slCreateEngine failed (result=%d) — audio disabled", (int)r);
        return -1;
    }

    r = (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("Engine Realize failed (result=%d) — audio disabled", (int)r);
        (*engineObj)->Destroy(engineObj);
        engineObj = nullptr;
        return -1;
    }

    r = (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engineItf);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("GetInterface(ENGINE) failed (result=%d) — audio disabled", (int)r);
        (*engineObj)->Destroy(engineObj);
        engineObj = nullptr;
        return -1;
    }

    r = (*engineItf)->CreateOutputMix(engineItf, &mixObj, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("CreateOutputMix failed (result=%d) — audio disabled", (int)r);
        (*engineObj)->Destroy(engineObj);
        engineObj = nullptr;
        return -1;
    }

    r = (*mixObj)->Realize(mixObj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("OutputMix Realize failed (result=%d) — audio disabled", (int)r);
        (*mixObj)->Destroy(mixObj);
        (*engineObj)->Destroy(engineObj);
        mixObj = nullptr;
        engineObj = nullptr;
        return -1;
    }

    int rate = getSampleRate();
    SLDataLocator_AndroidSimpleBufferQueue locBQ = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
    SLDataFormat_PCM fmt = {
        SL_DATAFORMAT_PCM, 2, (SLuint32)(rate * 1000),
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource src = { &locBQ, &fmt };

    SLDataLocator_OutputMix locMix = { SL_DATALOCATOR_OUTPUTMIX, mixObj };
    SLDataSink sink = { &locMix, nullptr };

    const SLInterfaceID ids[] = { SL_IID_BUFFERQUEUE };
    const SLboolean req[] = { SL_BOOLEAN_TRUE };
    r = (*engineItf)->CreateAudioPlayer(engineItf, &playerObj, &src, &sink, 1, ids, req);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("CreateAudioPlayer failed (result=%d) — audio disabled", (int)r);
        (*mixObj)->Destroy(mixObj);
        (*engineObj)->Destroy(engineObj);
        mixObj = nullptr;
        engineObj = nullptr;
        return -1;
    }

    r = (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        LOGE("AudioPlayer Realize failed (result=%d) — audio disabled", (int)r);
        (*playerObj)->Destroy(playerObj);
        (*mixObj)->Destroy(mixObj);
        (*engineObj)->Destroy(engineObj);
        playerObj = nullptr;
        mixObj = nullptr;
        engineObj = nullptr;
        return -1;
    }

    (*playerObj)->GetInterface(playerObj, SL_IID_PLAY, &playItf);
    (*playerObj)->GetInterface(playerObj, SL_IID_BUFFERQUEUE, &bqItf);
    (*bqItf)->RegisterCallback(bqItf, bqCallback, nullptr);
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

    fillAndEnqueue();
    fillAndEnqueue();
    LOGI("✓ OpenSL ES started: %d Hz stereo", rate);
    return 0;
}

extern "C" void minorGemsAndroid_audioStop() {
    SoundSpriteMixer_clearPlaying();
    if (playerObj) { (*playerObj)->Destroy(playerObj); playerObj = nullptr; }
    if (mixObj)    { (*mixObj)->Destroy(mixObj); mixObj = nullptr; }
    if (engineObj) { (*engineObj)->Destroy(engineObj); engineObj = nullptr; }
}

#endif // __ANDROID__
