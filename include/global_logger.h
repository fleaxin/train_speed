// global_zlog.h
#ifndef GLOBAL_ZLOG_H
#define GLOBAL_ZLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zlog.h>

// 声明全局句柄
extern zlog_category_t *global_zlog_category;

// 宏定义包装
#define LOG_DEBUG(format, ...) \
    zlog_debug(global_zlog_category, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) \
    zlog_info(global_zlog_category, format, ##__VA_ARGS__)

#define LOG_WARN(format, ...) \
    zlog_warn(global_zlog_category, format, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    zlog_error(global_zlog_category, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // GLOBAL_ZLOG_H