#ifdef __ANDROID__

// gameAndroid.cpp
// 提供 minorGems 游戏框架在 Android 上的"主循环 tick"实现。
// 与桌面 SDL 不同，Android 由 NativeActivity 驱动事件循环，
// 这里只对外暴露 init/tick/term 三个钩子，由 OneLife/android/jni/android_main.cpp 调用。
//
// 同时实现 game.h 要求的 4 个 socket 函数：
//   openSocketConnection / sendToSocket / readFromSocket / closeSocket
// 通过 minorGems SocketClient + Socket 实现，无录像回放逻辑。

#include <android/log.h>

#include "minorGems/network/SocketClient.h"
#include "minorGems/network/Socket.h"
#include "minorGems/network/HostAddress.h"
#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SimpleVector.h"
#include "minorGems/util/log/AppLog.h"
#include "minorGems/util/log/AndroidLog.h"

// minorGems 游戏侧接口（实际签名来自 game/game.h，C++ 链接）
void initFrameDrawer(int inWidth, int inHeight, int inTargetFrameRate,
                     const char* inCustomRecordedGameData,
                     char inPlayingBack);
void freeFrameDrawer();
void drawFrame(char inUpdate);
void initDrawString(int inWidth, int inHeight);
void freeDrawString();

namespace minorGemsAndroid {

void platformInit(int width, int height, int targetFrameRate) {
    // 把 AppLog 转发到 Android logcat（Tag: OneLifeGame / OneLife:<loggerName>）
    AppLog::setLog(new AndroidLog());
    AppLog::setLoggingLevel(Log::TRACE_LEVEL);  // 最详细级别，让 gameSource 的输出都能看到

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


// ─── Socket API ──────────────────────────────────────────────────────────────
// 实现 game.h 要求的 4 个 C 风格 socket 函数。
// 与 gameSDL.cpp 的实现相同逻辑，但去掉了录像回放（isPlayingBack）分支。

namespace {

struct SocketConnectionRecord {
    int handle;
    Socket* sock;
};

SimpleVector<SocketConnectionRecord> gSocketRecords;
int gNextSocketHandle = 0;

// 根据 handle 查找 Socket 指针，未找到返回 NULL
static Socket* getSocketByHandle(int inHandle) {
    for (int i = 0; i < gSocketRecords.size(); ++i) {
        SocketConnectionRecord* r = gSocketRecords.getElement(i);
        if (r->handle == inHandle) {
            return r->sock;
        }
    }
    __android_log_print(ANDROID_LOG_ERROR, "OneLife",
        "getSocketByHandle: handle %d not found", inHandle);
    return NULL;
}

}  // anonymous namespace


// 建立 TCP 连接，返回 handle（>=0），失败返回 -1。
// timeout=0 表示非阻塞 connect（立即返回未连接的 socket，
// 调用方通过 sock->isConnected() 轮询连接状态）。
extern "C" int openSocketConnection(const char* inNumericalAddress, int inPort) {
    SocketConnectionRecord r;
    r.handle = gNextSocketHandle++;

    HostAddress address(stringDuplicate(inNumericalAddress), inPort);

    char timedOut = false;
    // timeout=0：非阻塞 connect，与 gameSDL.cpp 保持一致
    r.sock = SocketClient::connectToServer(&address, 0, &timedOut);

    if (r.sock != NULL) {
        gSocketRecords.push_back(r);
        return r.handle;
    }

    return -1;
}


// 非阻塞发送。返回实际发送字节数（可能为 0），-1 表示错误。
extern "C" int sendToSocket(int inHandle,
                             unsigned char* inData, int inDataLength) {
    Socket* sock = getSocketByHandle(inHandle);
    if (sock == NULL) return -1;

    int numSent = 0;

    if (sock->isConnected()) {
        // inAllowedToBlock=false, inAllowDelay=false → 非阻塞、立即发送
        numSent = sock->send(inData, inDataLength, false, false);

        if (numSent == -2) {
            // 发送会阻塞，视为本次发出 0 字节（调用方下次重试）
            numSent = 0;
        }
    }

    if (numSent == -1) {
        return -1;
    }

    return numSent;
}


// 非阻塞读取。返回读到字节数（可能为 0），-1 表示连接错误。
// timeout=0 → 立即返回；若无数据则 receive 返回 -2，映射为 0。
extern "C" int readFromSocket(int inHandle,
                               unsigned char* inDataBuffer, int inBytesToRead) {
    Socket* sock = getSocketByHandle(inHandle);
    if (sock == NULL) return -1;

    int numRead = 0;

    if (sock->isConnected()) {
        // timeout=0：非阻塞模式，无数据时返回 -2
        numRead = sock->receive(inDataBuffer, inBytesToRead, 0);

        if (numRead == -2) {
            // 超时（无数据），视为读到 0 字节
            numRead = 0;
        }
    }

    return numRead;
}


// 关闭并释放 socket。
extern "C" void closeSocket(int inHandle) {
    for (int i = 0; i < gSocketRecords.size(); ++i) {
        SocketConnectionRecord* r = gSocketRecords.getElement(i);
        if (r->handle == inHandle) {
            delete r->sock;
            gSocketRecords.deleteElement(i);
            return;
        }
    }
    __android_log_print(ANDROID_LOG_ERROR, "OneLife",
        "closeSocket: handle %d not found", inHandle);
}


#endif // __ANDROID__
