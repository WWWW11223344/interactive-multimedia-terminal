/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   logger.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-24
Description: 应用运行日志模块的对外接口。日志用于记录程序的生命周期、页面切换与
             关键业务动作，与 platform_touch_log 记录的触摸原始事件互补：后者是
             诊断面板硬件用的，本模块记录的是「用户在程序里做了什么」。
             模块只依赖 C 标准库，不碰屏幕、字体与设备节点，因此主机测试、
             Windows 预览与板端实现共用同一套代码。
Others:      日志同时写入文件与标准错误：文件供离线回看，标准错误在板端就是串口，
             SecureCRT 里能实时看到。文件大小有硬上限，超过即轮转，避免诊断性输出
             长期运行后把板子分区写满——这块板曾经出现过空间不足。
Function List:
  1. log_open       打开日志文件并写一条启动分隔记录
  2. log_close      关闭日志文件
  3. log_write      按级别写入一条带时间戳的日志
  4. log_set_level  设置最低输出级别
  5. log_level_name 把级别枚举翻译成大写标签
History:
  1. Date: 2026-11-24   Author: 汪贤云
     Modification: 初版，建立分级日志与文件大小轮转
******************************************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdbool.h>

/* 告诉编译器 log_write 接受 printf 风格的格式串与可变参数。
   这样调用处的格式与实参会被静态检查，函数内部把 format 传给 vsnprintf 时
   也不会触发 -Wformat-nonliteral。MSVC 不支持这个属性，定义为空即可。 */
#if defined(__GNUC__) || defined(__clang__)
#define LOG_PRINTF_ATTR(formatIndex, argsIndex) \
    __attribute__((format(printf, formatIndex, argsIndex)))
#else
#define LOG_PRINTF_ATTR(formatIndex, argsIndex)
#endif

/* 日志级别。数值越小越严重，log_set_level 之下（数值更大）的条目被丢弃 */
typedef enum {
    LOG_ERROR = 0,   /* 错误：功能失败，需要人工介入 */
    LOG_WARN,        /* 警告：异常但程序仍可继续 */
    LOG_INFO,        /* 信息：正常的关键动作，默认级别 */
    LOG_DEBUG        /* 调试：细节较多，仅在排查时开启 */
} LogLevel;

/* 打开日志文件。path 为文件路径，父目录须已存在；重复调用会先关闭上一次的句柄。
   返回：打开成功返回真；失败时日志仍会写标准错误，只是不落盘。 */
bool log_open(const char *path);

/* 关闭日志文件。程序退出前调用一次，未打开过也可安全调用。 */
void log_close(void);

/* 写入一条日志。level 为级别，format 与可变参数按 printf 规则解释。
   时间戳取本机时间，格式为 MM-DD HH:MM:SS；文件写入失败不会中断程序。 */
void log_write(LogLevel level, const char *format, ...) LOG_PRINTF_ATTR(2, 3);

/* 设置最低输出级别，默认 LOG_INFO。传 LOG_DEBUG 可打开全部调试输出。 */
void log_set_level(LogLevel level);

/* 把级别翻译成 4 个大写字母的标签，供测试与格式化使用 */
const char *log_level_name(LogLevel level);

#endif
