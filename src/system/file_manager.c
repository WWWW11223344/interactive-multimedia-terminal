/* Replaces Xie Donghong's BMP helpers. Codex, 2026-11-19: padding, bounds, top-down and errors. */
#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Explicit little endian avoids struct padding and signed-char colour conversion. */
static uint32_t read_le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Serialize the BMP header without compiler-specific packing. */
static void write_le(uint8_t *p, uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i) {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

/* An empty destination is required; limit a decoded picture to 32 MiB. */
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

/* Release a possibly empty image and invalidate its dimensions. */
void image_free(Image *image)
{
    if (image) {
        free(image->pixels);
        *image = (Image){0};
    }
}

/* Accept uncompressed 24/32-bit BMP, including row padding and negative height. */
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
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "BM", 2) != 0 || read_le(header + 14) < 40 ||
        header[26] != 1 || header[27] != 0 || read_le(header + 30) != 0) {
        goto done;
    }
    int32_t width = (int32_t)read_le(header + 18), rawHeight = (int32_t)read_le(header + 22);
    unsigned int bytes = header[28] / 8;
    uint32_t offset = read_le(header + 10);
    if (header[29] || (header[28] != 24 && header[28] != 32) || rawHeight == INT32_MIN ||
        width <= 0 || rawHeight == 0 || offset < 54 || offset < 14u + (uint64_t)read_le(header + 14)) {
        goto done;
    }
    int height = rawHeight < 0 ? -rawHeight : rawHeight;
    if (width > 4096 || height > 4096) { goto done; }
    size_t stride = ((size_t)width * bytes + 3u) & ~(size_t)3;
    if (fseek(file, 0, SEEK_END) != 0) { goto done; }
    long length = ftell(file);
    if (length < 0 || (uint64_t)offset + stride * (size_t)height > (uint64_t)length ||
        fseek(file, (long)offset, SEEK_SET) != 0 || !image_create(&image, width, height)) {
        goto done;
    }
    uint8_t row[4096 * 4];
    for (int y = 0; y < height; ++y) {
        if (fread(row, 1, stride, file) != stride) { goto done; }
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

/* Write a complete 24-bit BMP; remove partial output if any write or close fails. */
bool image_save(const char *path, const Image *image)
{
    if (!path || !image || !image->pixels || image->width <= 0 || image->height <= 0 ||
        image->width > 4096 || image->height > 4096) { return false; }
    uint8_t header[54] = {0}, row[4096 * 3 + 4];
    size_t stride = ((size_t)image->width * 3u + 3u) & ~(size_t)3;
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
