#include "image.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>
#include <webp/decode.h>

static unsigned char *read_all(const char *path, size_t *size_out) {
    *size_out = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    unsigned char *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *size_out = (size_t)n;
    return buf;
}

static cairo_surface_t *load_webp(const unsigned char *data, size_t size) {
    int w = 0, h = 0;
    unsigned char *rgba = WebPDecodeRGBA(data, size, &w, &h);
    if (!rgba || w <= 0 || h <= 0) { WebPFree(rgba); return NULL; }

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        WebPFree(rgba); cairo_surface_destroy(surface); return NULL;
    }
    unsigned char *dst = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    for (int y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)(dst + y * stride);
        for (int x = 0; x < w; x++) {
            const unsigned char *p = rgba + ((size_t)y * w + x) * 4;
            unsigned a = p[3];
            unsigned r = (p[0] * a + 127) / 255;
            unsigned g = (p[1] * a + 127) / 255;
            unsigned b = (p[2] * a + 127) / 255;
            row[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    cairo_surface_mark_dirty(surface);
    WebPFree(rgba);
    return surface;
}

static cairo_surface_t *load_jpeg(const unsigned char *data, size_t size) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, size);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) { jpeg_destroy_decompress(&cinfo); return NULL; }
    jpeg_start_decompress(&cinfo);
    int w = (int)cinfo.output_width, h = (int)cinfo.output_height;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        jpeg_finish_decompress(&cinfo); jpeg_destroy_decompress(&cinfo); cairo_surface_destroy(surface); return NULL;
    }
    unsigned char *dst = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    unsigned char *scan = malloc((size_t)w * cinfo.output_components);
    if (!scan) { cairo_surface_destroy(surface); jpeg_finish_decompress(&cinfo); jpeg_destroy_decompress(&cinfo); return NULL; }
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rowptr = scan;
        jpeg_read_scanlines(&cinfo, &rowptr, 1);
        int y = (int)cinfo.output_scanline - 1;
        uint32_t *row = (uint32_t *)(dst + y * stride);
        for (int x = 0; x < w; x++) {
            unsigned r, g, b;
            if (cinfo.output_components == 1) r = g = b = scan[x];
            else { r = scan[x * cinfo.output_components]; g = scan[x * cinfo.output_components + 1]; b = scan[x * cinfo.output_components + 2]; }
            row[x] = 0xff000000u | (r << 16) | (g << 8) | b;
        }
    }
    free(scan);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    cairo_surface_mark_dirty(surface);
    return surface;
}

cairo_surface_t *chathud_image_load(const char *path) {
    if (!path || !*path) return NULL;
    size_t size = 0;
    unsigned char *data = read_all(path, &size);
    if (!data) return NULL;

    cairo_surface_t *surface = NULL;
    if (size >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) {
        free(data);
        surface = cairo_image_surface_create_from_png(path);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            return NULL;
        }
        return surface;
    }
    if (size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) {
        surface = load_webp(data, size);
    } else if (size >= 2 && data[0] == 0xff && data[1] == 0xd8) {
        surface = load_jpeg(data, size);
    }
    free(data);
    return surface;
}
