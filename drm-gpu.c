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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>

#include "drm-gpu.h"

static int drm_gpu_init(struct drm_gpu *gpu)
{
	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLint major, minor, count;

	gpu->egl.display = eglGetDisplay(gpu->device);
	if (gpu->egl.display == EGL_NO_DISPLAY) {
		fprintf(stderr, "failed to get EGL display\n");
		return -EINVAL;
	}

	if (!eglInitialize(gpu->egl.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize EGL\n");
		return -EINVAL;
	}

	printf("EGL %d.%d\n", major, minor);
	printf("EGL Version: %s\n", eglQueryString(gpu->egl.display, EGL_VERSION));
	printf("EGL Vendor: %s\n", eglQueryString(gpu->egl.display, EGL_VENDOR));
	printf("EGL Extensions: %s\n", eglQueryString(gpu->egl.display, EGL_EXTENSIONS));

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind OpenGL ES API\n");
		return -EINVAL;
	}

	if (!eglChooseConfig(gpu->egl.display, config_attribs,
			     &gpu->egl.config, 1, &count) ||
	    count != 1) {
		fprintf(stderr, "failed to choose EGL configuration\n");
		return -EINVAL;
	}

	gpu->egl.context = eglCreateContext(gpu->egl.display, gpu->egl.config,
					    EGL_NO_CONTEXT, context_attribs);
	if (gpu->egl.context == EGL_NO_CONTEXT) {
		fprintf(stderr, "failed to create EGL context\n");
		return -EINVAL;
	}

	return 0;
}

int drm_gpu_create(struct drm_gpu **gpup, int fd)
{
	struct drm_gpu *gpu;
	int err;

	gpu = calloc(1, sizeof(*gpu));
	if (!gpu)
		return -ENOMEM;

	gpu->fd = fd;

	gpu->device = gbm_create_device(fd);
	if (!gpu->device) {
		free(gpu);
		return -ENOMEM;
	}

	err = drm_gpu_init(gpu);
	if (err < 0) {
		gbm_device_destroy(gpu->device);
		free(gpu);
		return err;
	}

	*gpup = gpu;

	return 0;
}

void drm_gpu_free(struct drm_gpu *gpu)
{
	gbm_device_destroy(gpu->device);
	free(gpu);
}

int drm_gpu_open(struct drm_gpu **gpup, const char *path)
{
	int fd, err;

	fd = open(path, O_RDWR);
	if (fd < 0)
		return -errno;

	err = drm_gpu_create(gpup, fd);
	if (err < 0) {
		close(fd);
		return err;
	}

	return 0;
}

void drm_gpu_close(struct drm_gpu *gpu)
{
	int fd = gpu->fd;

	drm_gpu_free(gpu);
	close(fd);
}

void drm_gpu_bind_surface(struct drm_gpu *gpu, struct drm_gpu_surface *surface)
{
	eglMakeCurrent(gpu->egl.display, surface->egl.surface,
		       surface->egl.surface, gpu->egl.context);
}

static inline const char *drm_format_name(char *buffer, uint32_t format)
{
	buffer[0] = (format >>  0) & 0xff;
	buffer[1] = (format >>  8) & 0xff;
	buffer[2] = (format >> 16) & 0xff;
	buffer[3] = (format >> 24) & 0xff;
	buffer[4] = '\0';

	return buffer;
}

int drm_gpu_surface_create(struct drm_gpu_surface **surfacep,
			   struct drm_gpu *gpu, unsigned int width,
			   unsigned int height, uint32_t format,
			   unsigned long flags)
{
	uint32_t gbm_format, gbm_flags = 0;
	struct drm_gpu_surface *surface;
	char name[5];

	switch (format) {
	case DRM_FORMAT_XRGB8888:
		gbm_format = GBM_FORMAT_XRGB8888;
		break;

	case DRM_FORMAT_ARGB8888:
		gbm_format = GBM_FORMAT_ARGB8888;
		break;

	default:
		fprintf(stderr, "unsupported format: %s\n",
			drm_format_name(name, format));
		return -EINVAL;
	}

	if (flags & DRM_GPU_SCANOUT)
		gbm_flags |= GBM_BO_USE_SCANOUT;

	if (flags & DRM_GPU_RENDER)
		gbm_flags |= GBM_BO_USE_RENDERING;

	surface = calloc(1, sizeof(*surface));
	if (!surface)
		return -ENOMEM;

	surface->gpu = gpu;
	surface->width = width;
	surface->height = height;
	surface->format = format;

	surface->gbm.surface = gbm_surface_create(gpu->device, width, height,
						  gbm_format, gbm_flags);
	if (!surface->gbm.surface) {
		fprintf(stderr, "failed to create GBM surface\n");
		free(surface);
		return -ENOMEM;
	}

	surface->egl.surface = eglCreateWindowSurface(gpu->egl.display,
						      gpu->egl.config,
						      surface->gbm.surface,
						      NULL);
	if (surface->egl.surface == EGL_NO_SURFACE) {
		fprintf(stderr, "failed to create EGL surface\n");
		gbm_surface_destroy(surface->gbm.surface);
		free(surface);
		return -EINVAL;
	}

	*surfacep = surface;

	return 0;
}

void drm_gpu_surface_free(struct drm_gpu_surface *surface)
{
	free(surface);
}

int drm_gpu_surface_lock(struct drm_gpu_surface *surface,
			 struct drm_gpu_buffer **bop)
{
	struct drm_gpu_buffer *bo;
	uint32_t handle;
	int err;

	bo = calloc(1, sizeof(*bo));
	if (!bo)
		return -ENOMEM;

	bo->bo = gbm_surface_lock_front_buffer(surface->gbm.surface);
	if (!bo->bo) {
		free(bo);
		return -EINVAL;
	}

	handle = gbm_bo_get_handle(bo->bo).u32;

	err = drmPrimeHandleToFD(surface->gpu->fd, handle, 0, &bo->fd);
	if (err < 0)
		return -errno;

	bo->width = gbm_bo_get_width(bo->bo);
	bo->height = gbm_bo_get_height(bo->bo);
	bo->stride = gbm_bo_get_stride(bo->bo);
	bo->format = surface->format;

	*bop = bo;

	return 0;
}

void drm_gpu_surface_unlock(struct drm_gpu_surface *surface, struct drm_gpu_buffer *bo)
{
	close(bo->fd);
	gbm_surface_release_buffer(surface->gbm.surface, bo->bo);
}

int drm_gpu_buffer_map(struct drm_gpu_buffer *bo, void **ptrp,
		       uint32_t *stridep)
{
	uint32_t stride;

	bo->ptr = gbm_bo_map(bo->bo, 0, 0, bo->width, bo->height,
			     GBM_BO_TRANSFER_READ_WRITE, &stride,
			     &bo->map_data);
	if (!bo->ptr)
		return -ENOMEM;

	*stridep = stride;
	*ptrp = bo->ptr;

	return 0;
}

void drm_gpu_buffer_unmap(struct drm_gpu_buffer *bo)
{
	gbm_bo_unmap(bo->bo, bo->map_data);
	bo->map_data = NULL;
	bo->ptr = NULL;
}
