#ifdef __ANDROID__

#include "AndroidLog.h"

#include <android/log.h>
#include <stdarg.h>
#include <stdio.h>

namespace {

// 把 minorGems Log level 映射到 Android log priority
android_LogPriority toAndroidPriority(int inLevel) {
    if (inLevel <= Log::CRITICAL_ERROR_LEVEL) return ANDROID_LOG_FATAL;
    if (inLevel <= Log::ERROR_LEVEL)          return ANDROID_LOG_ERROR;
    if (inLevel <= Log::WARNING_LEVEL)        return ANDROID_LOG_WARN;
    if (inLevel <= Log::INFO_LEVEL)           return ANDROID_LOG_INFO;
    if (inLevel <= Log::DETAIL_LEVEL)         return ANDROID_LOG_DEBUG;
    return ANDROID_LOG_VERBOSE;
}

}  // anon ns

AndroidLog::AndroidLog()
    : mLevel(Log::INFO_LEVEL) {}

AndroidLog::~AndroidLog() {}

void AndroidLog::setLoggingLevel(int inLevel) { mLevel = inLevel; }
int  AndroidLog::getLoggingLevel() { return mLevel; }

void AndroidLog::logString(int inLevel, const char* inFormatString, ...) {
    if (inLevel > mLevel) return;
    va_list args;
    va_start(args, inFormatString);
    __android_log_vprint(toAndroidPriority(inLevel), "OneLifeGame",
                         inFormatString, args);
    va_end(args);
}

void AndroidLog::logPrintf(int inLevel, const char* inFormatString, ...) {
    if (inLevel > mLevel) return;
    va_list args;
    va_start(args, inFormatString);
    __android_log_vprint(toAndroidPriority(inLevel), "OneLifeGame",
                         inFormatString, args);
    va_end(args);
}

void AndroidLog::logString(const char* inLoggerName, int inLevel,
                           const char* inFormatString, ...) {
    if (inLevel > mLevel) return;
    char tag[64];
    snprintf(tag, sizeof(tag), "OneLife:%s", inLoggerName ? inLoggerName : "");
    va_list args;
    va_start(args, inFormatString);
    __android_log_vprint(toAndroidPriority(inLevel), tag,
                         inFormatString, args);
    va_end(args);
}

void AndroidLog::logStringV(int inLevel, const char* inFormatString,
                            va_list inArgList) {
    if (inLevel > mLevel) return;
    __android_log_vprint(toAndroidPriority(inLevel), "OneLifeGame",
                         inFormatString, inArgList);
}

void AndroidLog::logStringV(const char* inLoggerName,
                            int inLevel, const char* inFormatString,
                            va_list inArgList) {
    if (inLevel > mLevel) return;
    char tag[64];
    snprintf(tag, sizeof(tag), "OneLife:%s", inLoggerName ? inLoggerName : "");
    __android_log_vprint(toAndroidPriority(inLevel), tag,
                         inFormatString, inArgList);
}

#endif // __ANDROID__
