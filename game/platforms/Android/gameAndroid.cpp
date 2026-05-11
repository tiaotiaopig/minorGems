#ifdef __ANDROID__

// gameAndroid.cpp
// 提供 minorGems 游戏框架在 Android 上的"主循环 tick"实现。
// 与桌面 SDL 不同，Android 由 NativeActivity 驱动事件循环，
// 这里只对外暴露 init/tick/term 三个钩子，由 OneLife/android/jni/android_main.cpp 调用。

#include <android/log.h>

// minorGems 游戏侧接口（实际签名来自 game/game.h）
extern "C" {
    void initFrameDrawer(int inWidth, int inHeight, int inTargetFrameRate,
                         const char* inCustomRecordedGameData,
                         char inPlayingBack);
    void freeFrameDrawer();
    void drawFrame(char inUpdate);
    void initDrawString(int inWidth, int inHeight);
    void freeDrawString();
}

namespace minorGemsAndroid {

void platformInit(int width, int height, int targetFrameRate) {
    __android_log_print(ANDROID_LOG_INFO, "OneLife",
        "platformInit %dx%d @%dfps", width, height, targetFrameRate);

    // 初始化字符串绘制（可能在 initFrameDrawer 之前被调用以显示加载消息）
    initDrawString(width, height);

    // 初始化帧绘制器（空字符串表示无录制数据，false 表示非回放模式）
    initFrameDrawer(width, height, targetFrameRate, "", false);
}

void platformTick() {
    // true 表示更新游戏逻辑（非暂停状态）
    drawFrame(true);
}

void platformShutdown() {
    freeFrameDrawer();
    freeDrawString();
}

}  // namespace minorGemsAndroid

#endif // __ANDROID__
