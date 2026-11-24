/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   test_logger.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-24
Description: 应用运行日志模块的单元测试。无设备、无屏幕，只验证四件事：级别标签
             映射、按级别过滤、日志落盘、文件超过上限后自动轮转。测试把日志写到
             当前目录下的临时文件，结束时删除，不污染 data/app.log。
Others:      编译期定义 NDEBUG 被 CMake 的 -UNDEBUG 抵消，因此 assert 始终生效。
Function List:
  1. read_all      把临时文件完整读入缓冲区
  2. main          依次执行四组检查
History:
  1. Date: 2026-11-24   Author: 汪贤云
     Modification: 初版
******************************************************************************/
#include "logger.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 测试用的临时日志路径。工作目录由 ctest 设为源码根目录 */
#define TEST_LOG "test-logger-tmp.log"

/* 把文件完整读入内存，返回字节数与缓冲区。调用方负责 free。
   读不到文件时返回 NULL，字节数置 0。 */
static char *read_all(const char *path, long *size)
{
    FILE *file = fopen(path, "rb");
    char *buffer;

    *size = 0;
    if (!file) { return NULL; }
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return NULL; }
    buffer = malloc((size_t)length + 1);
    if (!buffer) { fclose(file); return NULL; }
    if (length > 0 && fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);
    buffer[length] = '\0';
    *size = length;
    return buffer;
}

int main(void)
{
    /* 一、级别标签映射，含越界保护 */
    assert(strcmp(log_level_name(LOG_ERROR), "ERROR") == 0);
    assert(strcmp(log_level_name(LOG_WARN), "WARN") == 0);
    assert(strcmp(log_level_name(LOG_INFO), "INFO") == 0);
    assert(strcmp(log_level_name(LOG_DEBUG), "DEBUG") == 0);
    assert(strcmp(log_level_name((LogLevel)99), "UNKNOWN") == 0);

    /* 二、正常写入：各级别都落盘，正文与标签都在 */
    remove(TEST_LOG);
    assert(log_open(TEST_LOG));
    log_set_level(LOG_DEBUG);
    log_write(LOG_ERROR, "error marker %d", 7);
    log_write(LOG_WARN, "warn marker %d", 8);
    log_write(LOG_INFO, "info marker %d", 9);
    log_write(LOG_DEBUG, "debug marker %d", 10);
    log_close();

    long size = 0;
    char *text = read_all(TEST_LOG, &size);
    assert(text != NULL);
    assert(strstr(text, "[ERROR] error marker 7") != NULL);
    assert(strstr(text, "[WARN ] warn marker 8") != NULL);
    assert(strstr(text, "[INFO ] info marker 9") != NULL);
    assert(strstr(text, "[DEBUG] debug marker 10") != NULL);
    assert(strstr(text, "log opened") != NULL);
    free(text);

    /* 三、级别过滤：设为 WARN 后 INFO 与 DEBUG 不应再落盘 */
    remove(TEST_LOG);
    assert(log_open(TEST_LOG));
    log_set_level(LOG_WARN);
    log_write(LOG_INFO, "should be filtered");
    log_write(LOG_DEBUG, "also filtered");
    log_write(LOG_WARN, "kept warning");
    log_close();
    log_set_level(LOG_INFO);

    text = read_all(TEST_LOG, &size);
    assert(text != NULL);
    assert(strstr(text, "should be filtered") == NULL);
    assert(strstr(text, "also filtered") == NULL);
    assert(strstr(text, "kept warning") != NULL);
    free(text);

    /* 四、大小轮转：先造一个明显超限的文件，打开时应被截断并写轮转提示。
       直接造大文件比循环写日志快得多，也避免几百行输出刷屏。 */
    FILE *big = fopen(TEST_LOG, "wb");
    assert(big != NULL);
    char block[1024];
    memset(block, 'x', sizeof(block));
    for (int i = 0; i < 80; ++i) { assert(fwrite(block, 1, sizeof(block), big) == sizeof(block)); }
    fclose(big);

    assert(log_open(TEST_LOG));
    log_close();
    text = read_all(TEST_LOG, &size);
    assert(text != NULL);
    /* 80 KiB 远超 64 KiB 上限，轮转后文件应远小于原值 */
    assert(size < 80 * 1024);
    assert(strstr(text, "log rotated") != NULL);
    free(text);

    remove(TEST_LOG);
    puts("logger tests passed");
    return 0;
}
