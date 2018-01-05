/*
 * Copyright (C) 2016 Thierry Reding
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

#ifndef DRM_KMS_H
#define DRM_KMS_H 1

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

struct drm_kms_bo {
	int fd;
	uint32_t handle;
	uint32_t size;
	void *ptr;
	int map_count;
	uint32_t pitch;
};

int drm_kms_bo_create(struct drm_kms_bo **bop, int fd, unsigned int width,
		      unsigned int height, unsigned int bpp);
int drm_kms_bo_free(struct drm_kms_bo *bo);
int drm_kms_bo_map(struct drm_kms_bo *bo);
int drm_kms_bo_unmap(struct drm_kms_bo *bo);

struct drm_kms_screen;

struct drm_kms_surface {
	struct drm_kms_screen *screen;
	struct drm_kms_bo *bo;
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
	uint32_t format;
	uint32_t id;
};

int drm_kms_surface_create(struct drm_kms_surface **surfacep,
			   struct drm_kms_screen *screen, unsigned int width,
			   unsigned int height, unsigned int bpp);
int drm_kms_surface_free(struct drm_kms_surface *surface);
int drm_kms_surface_lock(struct drm_kms_surface *surface, void **ptr);
int drm_kms_surface_unlock(struct drm_kms_surface *surface);

#define DRM_KMS_SCREEN_FULLSCREEN (1 << 0)

struct drm_kms_screen_args {
	unsigned int width;
	unsigned int height;
	uint32_t format;
	unsigned long flags;
};

struct drm_kms_screen {
	drmModeCrtcPtr original_crtc;
	drmModeModeInfo mode;
	uint32_t connector;
	uint32_t crtc;
	unsigned int pipe;
	unsigned int width;
	unsigned int height;
	struct drm_kms_surface *fb[2];
	unsigned int current;
	int fd;
};

int drm_kms_screen_create(struct drm_kms_screen **screenp, int fd);
int drm_kms_screen_create_with_args(struct drm_kms_screen **screenp, int fd,
				    const struct drm_kms_screen_args *args);
void drm_kms_screen_free(struct drm_kms_screen *screen);
int drm_kms_screen_open_with_args(struct drm_kms_screen **screenp,
				  const char *path,
				  const struct drm_kms_screen_args *args);
void drm_kms_screen_close(struct drm_kms_screen *screen);
int drm_kms_screen_swap(struct drm_kms_screen *screen);
int drm_kms_screen_swap_to(struct drm_kms_screen *screen,
			   struct drm_kms_surface *surface);
int drm_kms_screen_flip(struct drm_kms_screen *screen, void *data);
int drm_kms_screen_flip_to(struct drm_kms_screen *screen,
			   struct drm_kms_surface *surface, void *data);

struct drm_kms_import {
	int fd; /* DMA-BUF */
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	uint32_t format;
};

int drm_kms_screen_import_surface(struct drm_kms_screen *screen,
				  struct drm_kms_surface **surfacep,
				  const struct drm_kms_import *args);

struct drm_kms_lut_entry {
	uint16_t red;
	uint16_t green;
	uint16_t blue;
};

struct drm_kms_lut {
	struct drm_kms_lut_entry *entries;
	unsigned int num_entries;
};

int drm_kms_lut_load_palette(struct drm_kms_lut **lutp, const char *filename);
void drm_kms_lut_free(struct drm_kms_lut *lut);

int drm_kms_screen_load_lut(struct drm_kms_screen *screen,
			    const struct drm_kms_lut *lut);

#endif /* DRM_KMS_H */
