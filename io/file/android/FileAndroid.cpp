// FileAndroid.cpp
// 透明扩展：File::readFileContents() 在普通 fopen 失败时尝试从 AAsset 读取。
// 内部存储路径在 OneLife/android 入口处通过 chdir() 切换为 internalDataPath，
// 因此普通的相对路径写入文件能直接落到 /data/data/.../files/。

#ifdef __ANDROID__

#include "AndroidAssetGlobal.h"
#include <android/asset_manager.h>
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
    AAssetManager* gMgr = nullptr;
    char* gInternalPath = nullptr;
}

namespace minorGemsAndroid {
    void setAssetManager(AAssetManager* mgr) { gMgr = mgr; }
    AAssetManager* getAssetManager() { return gMgr; }

    void setInternalDataPath(const char* path) {
        if (gInternalPath) free(gInternalPath);
        gInternalPath = path ? strdup(path) : nullptr;
    }
    const char* getInternalDataPath() { return gInternalPath ? gInternalPath : "."; }
}

// 提供 C 接口，供 minorGems File 类回退使用
extern "C" {

// 尝试从 AAsset 读取文件。成功返回 malloc 出来的缓冲区与字节数，失败返回 nullptr。
unsigned char* minorGemsAndroid_readAsset(const char* relativePath, int* outBytes) {
    if (!gMgr || !relativePath) return nullptr;
    // 去掉开头的 "./" 或 "/"，AAsset 路径相对 assets/
    const char* p = relativePath;
    while (*p == '.' || *p == '/') p++;

    AAsset* a = AAssetManager_open(gMgr, p, AASSET_MODE_BUFFER);
    if (!a) return nullptr;

    off_t len = AAsset_getLength(a);
    unsigned char* buf = (unsigned char*) malloc(len);
    int read = AAsset_read(a, buf, len);
    AAsset_close(a);
    if (read != (int)len) { free(buf); return nullptr; }
    if (outBytes) *outBytes = (int)len;
    return buf;
}

}  // extern "C"

#endif // __ANDROID__
