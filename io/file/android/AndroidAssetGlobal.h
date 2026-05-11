#ifndef MINORGEMS_ANDROID_ASSET_GLOBAL_H
#define MINORGEMS_ANDROID_ASSET_GLOBAL_H

// 由 OneLife/android/jni/AndroidPlatform.cpp 在 NativeActivity 启动时设置
// FileAndroid 通过此入口找到 AAssetManager 与内部存储路径

#ifdef __ANDROID__
#include <android/asset_manager.h>

namespace minorGemsAndroid {
    void setAssetManager(AAssetManager* mgr);
    AAssetManager* getAssetManager();

    void setInternalDataPath(const char* path);
    const char* getInternalDataPath();
}
#endif

#endif
