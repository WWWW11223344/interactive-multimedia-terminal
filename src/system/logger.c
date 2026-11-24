/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   logger.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-24
Description: 应用运行日志的实现。每条日志同时写到文件与标准错误，格式为
             「MM-DD HH:MM:SS [级别] 正文」。文件采用追加方式，超过 LOG_MAX_BYTES
             后截断重写并写一条轮转提示，保证占用空间恒定。
Others:      文件句柄在 log_open 时打开一次，之后每次写入用 fprintf 追加并 fflush，
             这样进程被信号终止时也已经落盘，不会丢最后几条。写入失败（分区满、
             介质只读）只把 errno 打到标准错误，不改变程序控制流——日志是辅助设施，
             不能因为它不可用就让相册或游戏崩掉。时间戳用 localtime，单线程程序无需
             加锁；返回值判空是为了兼容某些平台在时区未配置时返回空指针。
Function List:
  1. log_open       打开日志文件并写启动分隔记录
  2. log_close      关闭日志文件
  3. log_write      格式化并按级别输出一条日志
  4. log_set_level  设置最低输出级别
  5. log_level_name 级别到标签的映射
  6. rotate_if_needed 文件超过上限时截断重写
History:
  1. Date: 2026-11-24   Author: 汪贤云
     Modification: 初版，建立分级日志与文件大小轮转
******************************************************************************/
#include "logger.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 单条日志正文的最大长度。超出部分被截断，防止一次异常输入写进整段内存 */
#define LOG_LINE_CAPACITY 512
/* 日志文件上限，64KiB。板端分区紧张，日志只记关键动作，这个量足够存数轮演示 */
#define LOG_MAX_BYTES (64u * 1024u)

/* 当前日志文件句柄。NULL 表示未打开或打开失败，此时仅写标准错误 */
static FILE *g_logFile;
/* 当前日志文件的路径。轮转时要用它重新以截断方式打开 */
static char g_logPath[512];
/* 当前文件已写入的字节数，用于判断是否触及上限 */
static unsigned long g_logBytes;
/* 最低输出级别，默认 INFO，即不输出 DEBUG */
static LogLevel g_minLevel = LOG_INFO;

/* 级别到标签的固定映射。放在数组里避免每处 if/else 重复 */
static const char *const LEVEL_NAMES[] = {"ERROR", "WARN", "INFO", "DEBUG"};

const char *log_level_name(LogLevel level)
{
    /* 越界的级别返回 UNKNOWN，不让非法值流出到文件里 */
    if (level < LOG_ERROR || level > LOG_DEBUG) { return "UNKNOWN"; }
    return LEVEL_NAMES[level];
}

void log_set_level(LogLevel level)
{
    g_minLevel = level;
}

/* 写入前的轮转检查：文件超过上限就截断重写，并留下一条可辨认的提示。
   必须在写入新内容之前调用，否则可能刚写完就超限。 */
static void rotate_if_needed(const char *path, unsigned long incoming)
{
    if (!g_logFile || g_logBytes + incoming <= LOG_MAX_BYTES) { return; }

    fclose(g_logFile);
    g_logFile = fopen(path, "w");
    if (!g_logFile) {
        g_logBytes = 0;
        return;
    }
    /* 轮转提示直接写文件，不走 log_write，避免递归触发检查 */
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char stamp[16] = "";

    if (local) { strftime(stamp, sizeof(stamp), "%m-%d %H:%M:%S", local); }
    int written = fprintf(g_logFile, "%s [%-5s] log rotated, size limit %u bytes\n",
        stamp, log_level_name(LOG_WARN), LOG_MAX_BYTES);
    fflush(g_logFile);
    g_logBytes = written > 0 ? (unsigned long)written : 0;
}

bool log_open(const char *path)
{
    /* 重复打开时先收掉旧句柄，避免文件描述符泄漏 */
    log_close();
    if (!path || !*path) { return false; }

    snprintf(g_logPath, sizeof(g_logPath), "%s", path);
    g_logFile = fopen(path, "a");
    if (!g_logFile) {
        fprintf(stderr, "logger: cannot open %s: %s\n", path, strerror(errno));
        return false;
    }

    /* 追加模式下需要知道已有内容的大小：先定位到末尾读取长度 */
    if (fseek(g_logFile, 0, SEEK_END) == 0) {
        long length = ftell(g_logFile);
        g_logBytes = length > 0 ? (unsigned long)length : 0;
    } else {
        g_logBytes = 0;
    }

    /* 已有文件本身超限（上一版遗留）时先轮转，再写启动记录 */
    rotate_if_needed(path, 0);
    log_write(LOG_INFO, "========== log opened ==========");
    return true;
}

void log_close(void)
{
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = NULL;
    }
    g_logBytes = 0;
}

void log_write(LogLevel level, const char *format, ...)
{
    char body[LOG_LINE_CAPACITY];
    char stamp[16] = "";
    va_list args;

    /* 严重度低于最低级别的条目直接丢弃。级别数值越小越严重，
       因此 level 大于 g_minLevel（例如默认 INFO 下的 DEBUG）才过滤 */
    if (level > g_minLevel) { return; }

    /* 正文在栈上格式化，限制单行长度 */
    va_start(args, format);
    vsnprintf(body, sizeof(body), format ? format : "(null)", args);
    va_end(args);

    /* 时间戳取本机时间；时区未配置时 localtime 可能返回空，此时留空而非崩溃 */
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local) { strftime(stamp, sizeof(stamp), "%m-%d %H:%M:%S", local); }

    /* 先按最坏情况估算写入长度做轮转，再真正写。估算偏大只会提前轮转，不会漏记 */
    unsigned long incoming = (unsigned long)(strlen(stamp) + strlen(log_level_name(level)) + strlen(body) + 16);
    rotate_if_needed(g_logPath, incoming);

    /* 文件与标准错误用同一份格式化结果，保证两边内容一致 */
    /* 级别标签统一占 5 字符宽，保证 ERROR/WARN/INFO/DEBUG 四行对齐 */
    if (g_logFile) {
        int written = fprintf(g_logFile, "%s [%-5s] %s\n", stamp, log_level_name(level), body);
        if (written > 0) { g_logBytes += (unsigned long)written; }
        fflush(g_logFile);
    }
    fprintf(stderr, "%s [%-5s] %s\n", stamp, log_level_name(level), body);
}
