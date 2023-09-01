/// @file LogUtil.h
/// @brief C++ 代码用的日志工具类
/// @author hejunlin
/// @last_editor lq

#include <android/log.h>

// 这里设置日志输出等级
#define LOG_LEVEL LOG_VERBOSE

#define LOGN (void) 0

#ifndef NDEBUG
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOG_VERBOSE 1
#define LOG_DEBUG 2
#define LOG_INFO 3
#define LOG_WARNING 4
#define LOG_ERROR 5
#define LOG_FATAL 6

#ifndef LOG_TAG
#define LOG_TAG __FILE__
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_VERBOSE
#endif

#define LOGP(level, fmt, ...) \
        __android_log_print(level, LOG_TAG, "%s:" fmt, \
        __PRETTY_FUNCTION__, ##__VA_ARGS__)

#if LOG_VERBOSE >= LOG_LEVEL
#define LOGV(fmt, ...) LOGP(ANDROID_LOG_VERBOSE, fmt, ##__VA_ARGS__)

#else
#define LOGV(...) LOGN
#endif // LOG_VERBOSE

#if LOG_DEBUG >= LOG_LEVEL
#define LOGD(fmt, ...) LOGP(ANDROID_LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOGD(...) LOGN
#endif // LOG_DEBUG

#if LOG_INFO >= LOG_LEVEL
#define LOGI(fmt, ...) LOGP(ANDROID_LOG_INFO, fmt, ##__VA_ARGS__)
#else
#define LOGI(...) LOGN
#endif // LOG_INFO

#if LOG_WARNING >= LOG_LEVEL
#define LOGW(fmt, ...) LOGP(ANDROID_LOG_WARN, fmt, ##__VA_ARGS__)
#else
#define LOGW(...) LOGN
#endif // LOG_WARNING

#if LOG_ERROR >= LOG_LEVEL
#define LOGE(fmt, ...) LOGP(ANDROID_LOG_ERROR, fmt, ##__VA_ARGS__)
#else
#define LOGE(...) LOGN
#endif // LOG_ERROR

#if LOG_FATAL >= LOG_LEVEL
#define LOGF(fmt, ...) LOGP(ANDROID_LOG_FATAL, fmt, ##__VA_ARGS__)

// assert, 不满足条件的时候 log
#define LOGA(condition, fmt, ...) \
        if (!(condition)) { \
          __android_log_assert(condition, LOG_TAG, "(%s:%u) %s: error:%s " fmt, \
                             __FILE__, __LINE__, __PRETTY_FUNCTION__, condition, ##__VA_ARGS__); \
        }
#else
#define LOGA(...) LOGN
#define LOGF(...) LOGN
#endif // LOG_FATAL


#endif  // NDEBUG