#ifdef __ANDROID__

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "OneLifeAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// minorGems 游戏音频接口（实际签名来自 game/game.h）
typedef uint8_t Uint8;
extern "C" void getSoundSamples(Uint8* inBuffer, int inLengthToFillInBytes);
extern "C" int getSampleRate();

namespace {
    SLObjectItf engineObj = nullptr;
    SLEngineItf engineItf = nullptr;
    SLObjectItf mixObj = nullptr;
    SLObjectItf playerObj = nullptr;
    SLPlayItf playItf = nullptr;
    SLAndroidSimpleBufferQueueItf bqItf = nullptr;

    constexpr int kBufferFrames = 2048;  // 立体声帧数
    constexpr int kBufferSize = kBufferFrames * 2 * sizeof(int16_t);  // 字节数
    uint8_t buffers[2][kBufferSize];  // 双缓冲
    int currentBuffer = 0;

    void fillAndEnqueue() {
        uint8_t* buf = buffers[currentBuffer];
        getSoundSamples(buf, kBufferSize);  // 游戏侧填充立体声 PCM
        (*bqItf)->Enqueue(bqItf, buf, kBufferSize);
        currentBuffer ^= 1;
    }

    void bqCallback(SLAndroidSimpleBufferQueueItf, void*) {
        fillAndEnqueue();
    }
}

extern "C" int minorGemsAndroid_audioStart() {
    SLresult r = slCreateEngine(&engineObj, 0, nullptr, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) { LOGE("slCreateEngine failed"); return -1; }
    (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
    (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engineItf);

    (*engineItf)->CreateOutputMix(engineItf, &mixObj, 0, nullptr, nullptr);
    (*mixObj)->Realize(mixObj, SL_BOOLEAN_FALSE);

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
    (*engineItf)->CreateAudioPlayer(engineItf, &playerObj, &src, &sink, 1, ids, req);
    (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
    (*playerObj)->GetInterface(playerObj, SL_IID_PLAY, &playItf);
    (*playerObj)->GetInterface(playerObj, SL_IID_BUFFERQUEUE, &bqItf);
    (*bqItf)->RegisterCallback(bqItf, bqCallback, nullptr);
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

    fillAndEnqueue();
    fillAndEnqueue();
    LOGI("OpenSL ES started: %d Hz stereo", rate);
    return 0;
}

extern "C" void minorGemsAndroid_audioStop() {
    if (playerObj) { (*playerObj)->Destroy(playerObj); playerObj = nullptr; }
    if (mixObj)    { (*mixObj)->Destroy(mixObj); mixObj = nullptr; }
    if (engineObj) { (*engineObj)->Destroy(engineObj); engineObj = nullptr; }
}

#endif // __ANDROID__
