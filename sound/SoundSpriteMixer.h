#ifndef SOUND_SPRITE_MIXER_INCLUDED
#define SOUND_SPRITE_MIXER_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* SoundSpriteMixerHandle;

// 注册一个 sprite。samples 由调用方拥有，函数内部 memcpy。
// 返回不透明句柄；NULL 表示分配失败。
SoundSpriteMixerHandle SoundSpriteMixer_setMono(
    int16_t *samples, int numSamples);
SoundSpriteMixerHandle SoundSpriteMixer_setStereo(
    int16_t *samplesL, int16_t *samplesR, int numSamples);

// 释放 sprite。先从播放列表中清除，再释放注册表中的 PCM 数据。
void SoundSpriteMixer_free(SoundSpriteMixerHandle h);

// 触发播放。volumeTweak ∈ [0,1]，stereoPos ∈ [0,1]（0=左, 1=右）。
void SoundSpriteMixer_play(SoundSpriteMixerHandle h,
                            double volumeTweak, double stereoPos);

// 混音入口。在音频回调线程调用。
// inBuffer 已包含 musicPlayer2 写入的背景音乐 PCM；
// 函数把所有正在播放的 sprite 叠加进去。
void SoundSpriteMixer_mix(uint8_t *inBuffer, int lengthBytes, int sampleRate);

// 全局参数
void SoundSpriteMixer_setMaxTotal(double maxVolume, double compressFraction);
void SoundSpriteMixer_setMaxSimultaneous(int maxCount);
void SoundSpriteMixer_setVolumeRange(double minV, double maxV);
void SoundSpriteMixer_setLoudness(float globalLoudness);
void SoundSpriteMixer_fade(double seconds, int sampleRate);
void SoundSpriteMixer_resume();
void SoundSpriteMixer_toggleVariance(SoundSpriteMixerHandle h, char noVariance);

// 显式锁（musicPlayer2 用于切换 musicOGGReady）
void SoundSpriteMixer_lock();
void SoundSpriteMixer_unlock();

// 关闭：清空播放列表，但保留注册的 sprite。
// 用于 OpenSL audioStop 时立即静音，audioStart 时无需重新加载 sprite。
void SoundSpriteMixer_clearPlaying();

// 完全关闭：清空播放列表 + 注册表 + 释放所有 PCM 数据。
// 用于游戏完全退出时调用。
void SoundSpriteMixer_shutdown();

#ifdef __cplusplus
}
#endif

#endif
