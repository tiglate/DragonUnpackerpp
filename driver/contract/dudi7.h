/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * DUDI 7: Dragon UnPACKer Driver Interface (C edition).
 * The rules and the order of the calls are in DUDI7.md. */
#ifndef DUDI7_H
#define DUDI7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DUDI7_CONTRACT_VERSION 1

#define DUDI7_OK               0
#define DUDI7_ERR_GENERIC     -1
#define DUDI7_ERR_VERSION     -2
#define DUDI7_ERR_BADARG      -3
#define DUDI7_ERR_NOTFORMAT   -4
#define DUDI7_ERR_IO          -5
#define DUDI7_ERR_UNSUPPORTED -6
#define DUDI7_ERR_CANCELLED   -7
#define DUDI7_ERR_BUFFER      -8

typedef void    (*dudi7_progress_cb)(void *ctx, int32_t percent);
typedef int32_t (*dudi7_language_cb)(void *ctx, const char *id, char *buf, int32_t buflen);
typedef void    (*dudi7_message_cb)(void *ctx, const char *title, const char *msg);
typedef int64_t (*dudi7_write_cb)(void *ctx, const void *buf, int64_t len);

typedef struct {
    uint32_t struct_size;
    uint32_t host_version;
    void *context;
    dudi7_progress_cb progress;
    dudi7_language_cb language;
    dudi7_message_cb message;
} dudi7_host;

typedef struct {
    uint32_t struct_size;
    int32_t version_id;
    const char *name;
    const char *author;
    const char *version;
    const char *comment;
    int32_t format_count;
} dudi7_driver_info;

typedef struct {
    uint32_t struct_size;
    const char *extensions;
    const char *name;
} dudi7_format_info;

typedef struct {
    uint32_t struct_size;
    int32_t extract_internal;
    int64_t entry_count;
    const char *format_id;
    const char *dir_separator;
} dudi7_archive_info;

typedef struct {
    uint32_t struct_size;
    const char *name;
    int64_t offset;
    int64_t size;
    int64_t data_x;
    int64_t data_y;
} dudi7_entry;

int32_t Dudi7_Version(int32_t host_version, int32_t *plugin_version);
int32_t Dudi7_Init(const dudi7_host *host);
int32_t Dudi7_Shutdown(void);
int32_t Dudi7_GetDriverInfo(dudi7_driver_info *info);
int32_t Dudi7_GetFormatInfo(int32_t index, dudi7_format_info *info);
int32_t Dudi7_IsFormat(const char *path, int32_t deeper, int32_t *supported);
int32_t Dudi7_Open(const char *path, int32_t deeper, void **archive);
int32_t Dudi7_GetArchiveInfo(void *archive, dudi7_archive_info *info);
int32_t Dudi7_GetEntry(void *archive, int64_t index, dudi7_entry *entry);
int32_t Dudi7_Extract(void *archive, int64_t index, dudi7_write_cb write, void *ctx);
int32_t Dudi7_Close(void *archive);
int32_t Dudi7_GetLastError(void *archive, char *buf, int32_t buflen);

#ifdef __cplusplus
}
#endif

#endif
