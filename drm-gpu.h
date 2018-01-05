/*
 * Copyright (C) 2018 Thierry Reding
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef DRM_GPU_H
#define DRM_GPU_H 1

#include <gbm.h>

#include <EGL/egl.h>

#define DRM_GPU_SCANOUT (1 << 0)
#define DRM_GPU_RENDER  (1 << 1)

struct drm_gpu_buffer {
	struct gbm_bo *bo;

	int fd;
	unsigned int width;
	unsigned int height;
	unsigned int stride;
	uint32_t format;

	void *map_data;
	void *ptr;
};

int drm_gpu_buffer_map(struct drm_gpu_buffer *bo, void **ptrp,
		       uint32_t *stridep);
void drm_gpu_buffer_unmap(struct drm_gpu_buffer *bo);

struct drm_gpu_surface {
	struct drm_gpu *gpu;
	unsigned int width;
	unsigned int height;
	uint32_t format;

	struct {
		struct gbm_surface *surface;
	} gbm;

	struct {
		EGLSurface surface;
	} egl;
};

struct drm_gpu {
	struct gbm_device *device;
	int fd;

	struct {
		EGLDisplay display;
		EGLConfig config;
		EGLContext context;
	} egl;
};

int drm_gpu_create(struct drm_gpu **gpup, int fd);
void drm_gpu_free(struct drm_gpu *gpu);

int drm_gpu_open(struct drm_gpu **gpup, const char *path);
void drm_gpu_close(struct drm_gpu *gpu);

void drm_gpu_bind_surface(struct drm_gpu *gpu, struct drm_gpu_surface *surface);

int drm_gpu_surface_create(struct drm_gpu_surface **surfacep,
			   struct drm_gpu *gpu, unsigned int width,
			   unsigned int height, uint32_t format,
			   unsigned long flags);
void drm_gpu_surface_free(struct drm_gpu_surface *surface);

int drm_gpu_surface_lock(struct drm_gpu_surface *surface,
			 struct drm_gpu_buffer **bop);
void drm_gpu_surface_unlock(struct drm_gpu_surface *surface,
			    struct drm_gpu_buffer *bo);

#endif /* DRM_GPU_H */
