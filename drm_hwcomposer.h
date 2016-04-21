/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_DRM_HWCOMPOSER_H_
#define ANDROID_DRM_HWCOMPOSER_H_

#include <stdbool.h>
#include <stdint.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include "seperate_rects.h"
#include "drmhwcgralloc.h"

#define BLEND_MASK 0xFFFF
struct hwc_import_context;

int hwc_import_init(struct hwc_import_context **ctx);
int hwc_import_destroy(struct hwc_import_context *ctx);

int hwc_import_bo_create(int fd, struct hwc_import_context *ctx,
                         buffer_handle_t buf, struct hwc_drm_bo *bo);
bool hwc_import_bo_release(int fd, struct hwc_import_context *ctx,
                           struct hwc_drm_bo *bo);

namespace android {

#if RK_DRM_HWC_DEBUG
enum LOG_LEVEL
{
    //Log level flag
    DBG_VERBOSE = 1 << 0,
    DBG_DEBUG = 1 << 1,
    DBG_INFO = 1 << 2,
    DBG_WARN = 1 << 3,
    DBG_ERROR = 1 << 4,
    DBG_FETAL = 1 << 5,
    DBG_SILENT = 1 << 6,
};
bool log_level(LOG_LEVEL log_level);
#endif

class Importer;

class UniqueFd {
 public:
  UniqueFd() = default;
  UniqueFd(int fd) : fd_(fd) {
  }
  UniqueFd(UniqueFd &&rhs) {
    fd_ = rhs.fd_;
    rhs.fd_ = -1;
  }

  UniqueFd &operator=(UniqueFd &&rhs) {
    Set(rhs.Release());
    return *this;
  }

  ~UniqueFd() {
    if (fd_ >= 0)
      close(fd_);
  }

  int Release() {
    int old_fd = fd_;
    fd_ = -1;
    return old_fd;
  }

  int Set(int fd) {
    if (fd_ >= 0)
      close(fd_);
    fd_ = fd;
    return fd_;
  }

  void Close() {
    if (fd_ >= 0)
      close(fd_);
    fd_ = -1;
  }

  int get() {
    return fd_;
  }

 private:
  int fd_ = -1;
};

struct OutputFd {
  OutputFd() = default;
  OutputFd(int *fd) : fd_(fd) {
  }
  OutputFd(OutputFd &&rhs) {
    fd_ = rhs.fd_;
    rhs.fd_ = NULL;
  }

  OutputFd &operator=(OutputFd &&rhs);

  int Set(int fd) {
    if (*fd_ >= 0)
      close(*fd_);
    *fd_ = fd;
    return fd;
  }

  int get() {
    return *fd_;
  }

 private:
  int *fd_ = NULL;
};

class DrmHwcBuffer {
 public:
  DrmHwcBuffer() = default;
  DrmHwcBuffer(const hwc_drm_bo &bo, Importer *importer)
      : bo_(bo), importer_(importer) {
  }
  DrmHwcBuffer(DrmHwcBuffer &&rhs) : bo_(rhs.bo_), importer_(rhs.importer_) {
    rhs.importer_ = NULL;
  }

  ~DrmHwcBuffer() {
    Clear();
  }

  DrmHwcBuffer &operator=(DrmHwcBuffer &&rhs) {
    Clear();
    importer_ = rhs.importer_;
    rhs.importer_ = NULL;
    bo_ = rhs.bo_;
    return *this;
  }

  operator bool() const {
    return importer_ != NULL;
  }

  const hwc_drm_bo *operator->() const;

  void Clear();

  int ImportBuffer(buffer_handle_t handle, Importer *importer);

 private:
  hwc_drm_bo bo_;
  Importer *importer_ = NULL;
};

class DrmHwcNativeHandle {
 public:
  DrmHwcNativeHandle() = default;

  DrmHwcNativeHandle(const gralloc_module_t *gralloc, native_handle_t *handle)
      : gralloc_(gralloc), handle_(handle) {
  }

  DrmHwcNativeHandle(DrmHwcNativeHandle &&rhs) {
    gralloc_ = rhs.gralloc_;
    rhs.gralloc_ = NULL;
    handle_ = rhs.handle_;
    rhs.handle_ = NULL;
  }

  ~DrmHwcNativeHandle();

  DrmHwcNativeHandle &operator=(DrmHwcNativeHandle &&rhs) {
    Clear();
    gralloc_ = rhs.gralloc_;
    rhs.gralloc_ = NULL;
    handle_ = rhs.handle_;
    rhs.handle_ = NULL;
    return *this;
  }

  int CopyBufferHandle(buffer_handle_t handle, const gralloc_module_t *gralloc);

  void Clear();

  buffer_handle_t get() const {
    return handle_;
  }

 private:
  const gralloc_module_t *gralloc_ = NULL;
  native_handle_t *handle_ = NULL;
};

template <typename T>
using DrmHwcRect = seperate_rects::Rect<T>;

enum class DrmHwcTransform : uint32_t {
  kIdentity = 0,
  kFlipH = HWC_TRANSFORM_FLIP_H,
  kFlipV = HWC_TRANSFORM_FLIP_V,
  kRotate90 = HWC_TRANSFORM_ROT_90,
  kRotate180 = HWC_TRANSFORM_ROT_180,
  kRotate270 = HWC_TRANSFORM_ROT_270,
};

enum class DrmHwcBlending : int32_t {
  kNone = HWC_BLENDING_NONE,
  kPreMult = HWC_BLENDING_PREMULT,
  kCoverage = HWC_BLENDING_COVERAGE,
};

struct DrmHwcLayer {
  buffer_handle_t sf_handle = NULL;
  int gralloc_buffer_usage = 0;
  DrmHwcBuffer buffer;
  DrmHwcNativeHandle handle;
  DrmHwcTransform transform = DrmHwcTransform::kIdentity;
  DrmHwcBlending blending = DrmHwcBlending::kNone;
  uint8_t alpha = 0xff;
  DrmHwcRect<float> source_crop;
  DrmHwcRect<int> isource_crop;
  DrmHwcRect<int> display_frame;
  std::vector<DrmHwcRect<int>> source_damage;

  UniqueFd acquire_fence;
  OutputFd release_fence;

#if 0
  bool is_yuv;
  bool is_scale;
  bool is_large;
  int h_scale_mul;
  int v_scale_mul;
  int format;
  int width;
  int height;
  int bpp;
  int group_id;

  int InitFromHwcLayer(struct hwc_context_t *ctx, hwc_layer_1_t *sf_layer, Importer *importer,
                        const gralloc_module_t *gralloc);
#else
  int InitFromHwcLayer(hwc_layer_1_t *sf_layer, Importer *importer,
                        const gralloc_module_t *gralloc);
#endif

#if RK_DRM_HWC_DEBUG
void dump_drm_layer(int index, std::ostringstream *out) const;
#endif

  buffer_handle_t get_usable_handle() const {
    return handle.get() != NULL ? handle.get() : sf_handle;
  }
};

struct DrmHwcDisplayContents {
  OutputFd retire_fence;
  std::vector<DrmHwcLayer> layers;
};
}

#endif
