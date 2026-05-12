/*
 * AndroidLog
 *
 * 把 minorGems 的 AppLog 输出转发到 Android logcat 的 Log 实现。
 * 使用 __android_log_vprint，支持 printf 风格格式化。
 *
 * 用法：在 platformInit 时调用 AppLog::setLog(new AndroidLog())。
 */

#ifndef ANDROID_LOG_INCLUDED
#define ANDROID_LOG_INCLUDED

#ifdef __ANDROID__

#include "minorGems/util/log/Log.h"

class AndroidLog : public Log {
public:
    AndroidLog();
    virtual ~AndroidLog();

    virtual void setLoggingLevel(int inLevel);
    virtual int  getLoggingLevel();

    virtual void logString(int inLevel, const char* inFormatString, ...);
    virtual void logPrintf(int inLevel, const char* inFormatString, ...);
    virtual void logString(const char* inLoggerName, int inLevel,
                           const char* inFormatString, ...);

    virtual void logStringV(int inLevel, const char* inFormatString,
                            va_list inArgList);
    virtual void logStringV(const char* inLoggerName,
                            int inLevel, const char* inFormatString,
                            va_list inArgList);

private:
    int mLevel;
};

#endif // __ANDROID__
#endif
