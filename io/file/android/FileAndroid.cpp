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

    // 先尝试直接路径
    AAsset* a = AAssetManager_open(gMgr, p, AASSET_MODE_BUFFER);

    // 如果失败且路径不含 '/'，尝试 graphics/ 前缀（gameSource UI 图标）
    if (!a && strchr(p, '/') == nullptr) {
        char graphicsPath[512];
        snprintf(graphicsPath, sizeof(graphicsPath), "graphics/%s", p);
        a = AAssetManager_open(gMgr, graphicsPath, AASSET_MODE_BUFFER);
    }

    if (!a) return nullptr;

    off_t len = AAsset_getLength(a);
    unsigned char* buf = (unsigned char*) malloc(len);
    int read = AAsset_read(a, buf, len);
    AAsset_close(a);
    if (read != (int)len) { free(buf); return nullptr; }
    if (outBytes) *outBytes = (int)len;
    return buf;
}

// 检测 AAsset 中是否存在指定目录（通过 AAssetManager_openDir）
// 返回 1 表示是目录，0 表示不是
int minorGemsAndroid_isAssetDirectory(const char* relativePath) {
    if (!gMgr || !relativePath) return 0;
    const char* p = relativePath;
    while (*p == '.' || *p == '/') p++;

    AAssetDir* dir = AAssetManager_openDir(gMgr, p);
    if (!dir) return 0;

    // AAssetManager_openDir 即使路径不存在也会返回非 NULL，
    // 必须检查能否枚举出至少一个文件来判断目录是否真实存在
    const char* firstFile = AAssetDir_getNextFileName(dir);
    AAssetDir_close(dir);
    return firstFile != nullptr ? 1 : 0;
}

// 列出 AAsset 目录下所有文件名（不递归）。
// 返回文件名数组（每个 strdup 出来），调用者负责 free 每个字符串和数组本身。
// outCount 写入文件数量。失败或空目录返回 nullptr。
char** minorGemsAndroid_listAssetDirectory(const char* relativePath, int* outCount) {
    if (outCount) *outCount = 0;
    if (!gMgr || !relativePath) return nullptr;
    const char* p = relativePath;
    while (*p == '.' || *p == '/') p++;

    AAssetDir* dir = AAssetManager_openDir(gMgr, p);
    if (!dir) return nullptr;

    // 先统计数量
    int count = 0;
    const char* name;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
        count++;
    }
    AAssetDir_close(dir);

    if (count == 0) return nullptr;

    // 重新打开并复制每个文件名
    dir = AAssetManager_openDir(gMgr, p);
    if (!dir) return nullptr;

    char** names = (char**) malloc(sizeof(char*) * count);
    int i = 0;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr && i < count) {
        names[i++] = strdup(name);
    }
    AAssetDir_close(dir);

    if (outCount) *outCount = i;
    return names;
}

}  // extern "C"

#endif // __ANDROID__
