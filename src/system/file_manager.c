/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   file_manager.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 图像文件读写模块。替代谢东宏原 BMP 辅助函数，负责解读与生成 24/32 位
             未压缩 BMP，并处理行对齐填充、自顶向下与自底向上两种存储方向、边界
             检查以及错误清理。画板作品保存与相册缩略图加载都经由这里。
Others:      显式按小端逐字节读写，避免结构体对齐填充与有符号 char 造成的颜色转换
             问题。解码后的单幅图像限制在 32MiB 以内，防止小内存板一次分配过多。
Function List:
  1. read_le        按小端顺序读取 32 位无符号整数
  2. write_le       按小端顺序写出 32 位无符号整数
  3. image_create   分配并初始化空图像缓冲
  4. image_free     释放图像缓冲并使尺寸失效
  5. image_load     读取未压缩 24/32 位 BMP
  6. image_save     写出完整的 24 位 BMP，失败时清理残留文件
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 替代谢东宏的 BMP 辅助函数。Codex，2026-11-19：补齐行对齐、边界检查、上下
   方向处理与错误处理。 */
#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* 显式小端读取，避免结构体填充以及有符号 char 带来的颜色转换问题。 */
static uint32_t read_le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* 逐字节写出 BMP 头字段，不依赖编译器特定的 packing 方式。 */
static void write_le(uint8_t *p, uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i) {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

/* 目标图像必须为空；解码后单幅图像上限 32MiB。 */
bool image_create(Image *image, int width, int height)
{
    if (!image || width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
        (size_t)width * (size_t)height > 8u * 1024u * 1024u) {
        return false;
    }
    uint32_t *pixels = calloc((size_t)width * (size_t)height, sizeof(*pixels));
    if (!pixels) {
        return false;
    }
    *image = (Image){width, height, pixels};
    return true;
}

/* 释放可能为空的图像，并把尺寸一并清零，防止悬空引用。 */
void image_free(Image *image)
{
    if (image) {
        free(image->pixels);
        *image = (Image){0};
    }
}

/* 接受未压缩的 24/32 位 BMP，兼容行对齐填充与负高度（自顶向下存储）。 */
bool image_load(const char *path, Image *out)
{
    uint8_t header[54];
    Image image = {0};
    FILE *file = path ? fopen(path, "rb") : NULL;
    bool ok = false;
    if (!out || !file) {
        if (file) { fclose(file); }
        return false;
    }
    *out = (Image){0};
    /* 文件头校验：BM 标识、DIB 头至少 40 字节、未压缩、无调色板。 */
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "BM", 2) != 0 || read_le(header + 14) < 40 ||
        header[26] != 1 || header[27] != 0 || read_le(header + 30) != 0) {
        goto done;
    }
    int32_t width = (int32_t)read_le(header + 18), rawHeight = (int32_t)read_le(header + 22);
    unsigned int bytes = header[28] / 8;
    uint32_t offset = read_le(header + 10);
    /* 位深只认 24/32，高度不能是最负值，像素数据起点必须越过文件头与 DIB 头。 */
    if (header[29] || (header[28] != 24 && header[28] != 32) || rawHeight == INT32_MIN ||
        width <= 0 || rawHeight == 0 || offset < 54 || offset < 14u + (uint64_t)read_le(header + 14)) {
        goto done;
    }
    /* 负高度表示自顶向下，取绝对值；行步长按 4 字节对齐。 */
    int height = rawHeight < 0 ? -rawHeight : rawHeight;
    if (width > 4096 || height > 4096) { goto done; }
    size_t stride = ((size_t)width * bytes + 3u) & ~(size_t)3;
    /* 文件大小必须覆盖整个像素区，避免读到截断的作品。 */
    if (fseek(file, 0, SEEK_END) != 0) { goto done; }
    long length = ftell(file);
    if (length < 0 || (uint64_t)offset + stride * (size_t)height > (uint64_t)length ||
        fseek(file, (long)offset, SEEK_SET) != 0 || !image_create(&image, width, height)) {
        goto done;
    }
    uint8_t row[4096 * 4];
    for (int y = 0; y < height; ++y) {
        if (fread(row, 1, stride, file) != stride) { goto done; }
        /* BMP 默认自底向上存行，负高度时才自顶向下；BGR 转存为 RGB。 */
        int destY = rawHeight < 0 ? y : height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const uint8_t *p = row + (size_t)x * bytes;
            image.pixels[(size_t)destY * (size_t)width + (size_t)x] =
                (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
        }
    }
    *out = image;
    image = (Image){0};
    ok = true;
done:
    image_free(&image);
    fclose(file);
    return ok;
}

/* 写出完整的 24 位 BMP；写入或关闭失败时删除残留文件，避免留下残缺作品。 */
bool image_save(const char *path, const Image *image)
{
    if (!path || !image || !image->pixels || image->width <= 0 || image->height <= 0 ||
        image->width > 4096 || image->height > 4096) { return false; }
    uint8_t header[54] = {0}, row[4096 * 3 + 4];
    size_t stride = ((size_t)image->width * 3u + 3u) & ~(size_t)3;
    /* 填充 BMP 文件头与信息头，行按 4 字节对齐，自底向上写行。 */
    header[0] = 'B'; header[1] = 'M'; header[26] = 1; header[28] = 24;
    write_le(header + 2, (uint32_t)(54 + stride * (size_t)image->height));
    write_le(header + 10, 54); write_le(header + 14, 40);
    write_le(header + 18, (uint32_t)image->width); write_le(header + 22, (uint32_t)image->height);
    write_le(header + 34, (uint32_t)(stride * (size_t)image->height));
    FILE *file = fopen(path, "wb");
    if (!file) { return false; }
    bool ok = fwrite(header, 1, 54, file) == 54;
    memset(row, 0, stride);
    for (int y = image->height - 1; ok && y >= 0; --y) {
        for (int x = 0; x < image->width; ++x) {
            uint32_t rgb = image->pixels[(size_t)y * (size_t)image->width + (size_t)x];
            row[3 * x] = (uint8_t)rgb; row[3 * x + 1] = (uint8_t)(rgb >> 8);
            row[3 * x + 2] = (uint8_t)(rgb >> 16);
        }
        ok = fwrite(row, 1, stride, file) == stride;
    }
    if (fclose(file) != 0) { ok = false; }
    if (!ok) { remove(path); }
    return ok;
}
