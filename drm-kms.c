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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>

#include "drm-kms.h"

int drm_kms_bo_create(struct drm_kms_bo **bop, int fd, unsigned int width,
		      unsigned int height, unsigned int bpp)
{
	struct drm_mode_create_dumb arg;
	struct drm_kms_bo *bo;
	int err;

	bo = calloc(1, sizeof(*bo));
	if (!bo)
		return -ENOMEM;

	memset(&arg, 0, sizeof(arg));
	arg.width = width;
	arg.height = height;
	arg.bpp = bpp;

	err = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
	if (err < 0) {
		err = -errno;
		free(bo);
		return err;
	}

	bo->handle = arg.handle;
	bo->size = arg.size;
	bo->pitch = arg.pitch;
	bo->fd = fd;

	*bop = bo;

	return 0;
}

int drm_kms_bo_free(struct drm_kms_bo *bo)
{
	struct drm_mode_destroy_dumb arg;
	int err;

	if (bo->ptr) {
		munmap(bo->ptr, bo->size);
		bo->ptr = NULL;
	}

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->handle;

	err = drmIoctl(bo->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
	if (err < 0)
		return -errno;

	free(bo);

	return 0;
}

int drm_kms_bo_map(struct drm_kms_bo *bo)
{
	struct drm_mode_map_dumb arg;
	void *map;
	int err;

	if (bo->ptr) {
		bo->map_count++;
		return 0;
	}

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->handle;

	err = drmIoctl(bo->fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
	if (err < 0)
		return -errno;

	map = mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, bo->fd,
		   arg.offset);
	if (map == MAP_FAILED)
		return -errno;

	bo->ptr = map;

	return 0;
}

int drm_kms_bo_unmap(struct drm_kms_bo *bo)
{
	bo->map_count--;

	return 0;
}

int drm_kms_surface_create(struct drm_kms_surface **surfacep,
			   struct drm_kms_screen *screen,
			   unsigned int width, unsigned int height,
			   uint32_t format)
{
	struct drm_kms_surface *surface;
	uint32_t offset = 0;
	unsigned int bpp;
	int err;

	surface = calloc(1, sizeof(*surface));
	if (!surface)
		return -ENOMEM;

	surface->screen = screen;
	surface->width = width;
	surface->height = height;
	surface->format = format;

	switch (format) {
	case DRM_FORMAT_C8:
		bpp = 8;
		break;

	case DRM_FORMAT_XRGB8888:
		bpp = 32;
		break;

	default:
		return -EINVAL;
	}

	err = drm_kms_bo_create(&surface->bo, screen->fd, width, height, bpp);
	if (err < 0) {
		free(surface);
		return err;
	}

	err = drmModeAddFB2(screen->fd, width, height, format,
			    &surface->bo->handle, &surface->bo->pitch,
			    &offset, &surface->id, 0);
	if (err < 0) {
		drm_kms_bo_free(surface->bo);
		free(surface);
		return err;
	}

	*surfacep = surface;

	return 0;
}

int drm_kms_surface_free(struct drm_kms_surface *surface)
{
	if (!surface)
		return -EINVAL;

	drm_kms_bo_free(surface->bo);
	free(surface);

	return 0;
}

int drm_kms_surface_lock(struct drm_kms_surface *surface, void **ptr)
{
	int err;

	if (!surface || !ptr)
		return -EINVAL;

	err = drm_kms_bo_map(surface->bo);
	if (err < 0)
		return err;

	*ptr = surface->bo->ptr;

	return 0;
}

int drm_kms_surface_unlock(struct drm_kms_surface *surface)
{
	int err;

	if (!surface)
		return -EINVAL;

	err = drm_kms_bo_unmap(surface->bo);
	if (err < 0)
		return err;

	return 0;
}

static int drm_kms_screen_choose_output(struct drm_kms_screen *screen)
{
	int ret = -ENODEV;
	drmModeRes *res;
	uint32_t i;

	if (!screen)
		return -EINVAL;

	res = drmModeGetResources(screen->fd);
	if (!res)
		return -ENODEV;

	for (i = 0; i < res->count_connectors; i++) {
		drmModeConnector *connector;
		drmModeEncoder *encoder;

		connector = drmModeGetConnector(screen->fd, res->connectors[i]);
		if (!connector)
			continue;

		if (connector->connection != DRM_MODE_CONNECTED) {
			drmModeFreeConnector(connector);
			continue;
		}

		encoder = drmModeGetEncoder(screen->fd, connector->encoder_id);
		if (!encoder) {
			drmModeFreeConnector(connector);
			continue;
		}

		screen->connector = res->connectors[i];
		screen->mode = connector->modes[0];
		screen->crtc = encoder->crtc_id;

		drmModeFreeEncoder(encoder);
		drmModeFreeConnector(connector);
		ret = 0;
		break;
	}

	for (i = 0; i < res->count_crtcs; i++) {
		drmModeCrtc *crtc;

		crtc = drmModeGetCrtc(screen->fd, res->crtcs[i]);
		if (!crtc)
			continue;

		if (crtc->crtc_id == screen->crtc) {
			drmModeFreeCrtc(crtc);
			screen->pipe = i;
			break;
		}

		drmModeFreeCrtc(crtc);
	}

	drmModeFreeResources(res);
	return ret;
}

int drm_kms_screen_create_with_args(struct drm_kms_screen **screenp, int fd,
				    const struct drm_kms_screen_args *args)
{
	struct drm_kms_screen *screen;
	unsigned int i;
	int err;

	err = drmSetMaster(fd);
	if (err < 0)
		return -errno;

	screen = calloc(1, sizeof(*screen));
	if (!screen)
		return -ENOMEM;

	screen->fd = fd;

	err = drm_kms_screen_choose_output(screen);
	if (err < 0)
		return err;

	screen->original_crtc = drmModeGetCrtc(screen->fd, screen->crtc);

	if (args->flags & DRM_KMS_SCREEN_FULLSCREEN) {
		screen->width = screen->mode.hdisplay;
		screen->height = screen->mode.vdisplay;
	} else {
		screen->width = args->width;
		screen->height = args->height;
	}

	for (i = 0; i < 2; i++) {
		err = drm_kms_surface_create(&screen->fb[i], screen,
					     screen->width, screen->height,
					     args->format);
		if (err < 0) {
			fprintf(stderr, "drm_kms_surface_create_with_format() failed: %d\n", err);
			return err;
		}
	}

	drm_kms_screen_swap(screen);

	*screenp = screen;

	return 0;
}

int drm_kms_screen_create(struct drm_kms_screen **screenp, int fd)
{
	struct drm_kms_screen_args args;

	memset(&args, 0, sizeof(args));
	args.flags = DRM_KMS_SCREEN_FULLSCREEN;
	args.format = DRM_FORMAT_XRGB8888;

	return drm_kms_screen_create_with_args(screenp, fd, &args);
}

void drm_kms_screen_free(struct drm_kms_screen *screen)
{
	drmModeCrtcPtr crtc;
	unsigned int i;

	if (!screen)
		return;

	crtc = screen->original_crtc;
	drmModeSetCrtc(screen->fd, crtc->crtc_id, crtc->buffer_id, crtc->x,
		       crtc->y, &screen->connector, 1, &crtc->mode);
	drmModeFreeCrtc(crtc);

	for (i = 0; i < 2; i++)
		drm_kms_surface_free(screen->fb[i]);

	drmDropMaster(screen->fd);
	free(screen);
}

int drm_kms_screen_open_with_args(struct drm_kms_screen **screenp,
				  const char *path,
				  const struct drm_kms_screen_args *args)
{
	int fd, err;

	fd = open(path, O_RDWR);
	if (fd < 0)
		return -errno;

	err = drm_kms_screen_create_with_args(screenp, fd, args);
	if (err < 0) {
		close(fd);
		return err;
	}

	return 0;
}

void drm_kms_screen_close(struct drm_kms_screen *screen)
{
	int fd = screen->fd;

	drm_kms_screen_free(screen);
	close(fd);
}

int drm_kms_screen_swap(struct drm_kms_screen *screen)
{
	struct drm_kms_surface *fb = screen->fb[screen->current];
	int err;

	if (!screen)
		return -EINVAL;

	err = drmModeSetCrtc(screen->fd, screen->crtc, fb->id, 0, 0,
			     &screen->connector, 1, &screen->mode);
	if (err < 0)
		return -errno;

	screen->current ^= 1;

	return 0;
}

int drm_kms_screen_swap_to(struct drm_kms_screen *screen,
			   struct drm_kms_surface *surface)
{
	int err;

	if (!screen || !surface)
		return -EINVAL;

	err = drmModeSetCrtc(screen->fd, screen->crtc, surface->id, 0, 0,
			     &screen->connector, 1, &screen->mode);
	if (err < 0)
		return -errno;

	return 0;
}

int drm_kms_screen_flip(struct drm_kms_screen *screen, void *data)
{
	struct drm_kms_surface *fb = screen->fb[screen->current];
	int err;

	if (!screen)
		return -EINVAL;

	err = drmModePageFlip(screen->fd, screen->crtc, fb->id,
			      DRM_MODE_PAGE_FLIP_EVENT, data);
	if (err < 0)
		return -errno;

	screen->current ^= 1;

	return 0;
}

int drm_kms_screen_flip_to(struct drm_kms_screen *screen,
			   struct drm_kms_surface *surface, void *data)
{
	int err;

	if (!screen || !surface)
		return -EINVAL;

	err = drmModePageFlip(screen->fd, screen->crtc, surface->id,
			      DRM_MODE_PAGE_FLIP_EVENT, data);
	if (err < 0)
		return -errno;

	return 0;
}

int drm_kms_screen_import_surface(struct drm_kms_screen *screen,
				  struct drm_kms_surface **surfacep,
				  const struct drm_kms_import *args)
{
	struct drm_kms_surface *surface;
	uint32_t handle, offset = 0;
	int err;

	surface = calloc(1, sizeof(*surface));
	if (!surface)
		return -ENOMEM;

	surface->screen = screen;
	surface->width = args->width;
	surface->height = args->height;
	surface->format = args->format;

	err = drmPrimeFDToHandle(screen->fd, args->fd, &handle);
	if (err < 0) {
		free(surface);
		return -errno;
	}

	err = drmModeAddFB2(screen->fd, args->width, args->height,
			    args->format, &handle, &args->pitch, &offset,
			    &surface->id, 0);
	if (err < 0) {
		/* TODO close handle */
		free(surface);
		return -errno;
	}

	*surfacep = surface;

	return 0;
}

int drm_kms_screen_load_lut(struct drm_kms_screen *screen,
			    const struct drm_kms_lut *lut)
{
	uint16_t *buffer;
	unsigned int i;
	int err;

	buffer = calloc(sizeof(*buffer), lut->num_entries * 3);
	if (!buffer)
		return -ENOMEM;

	for (i = 0; i < lut->num_entries; i++) {
		buffer[lut->num_entries * 0 + i] = lut->entries[i].red;
		buffer[lut->num_entries * 1 + i] = lut->entries[i].green;
		buffer[lut->num_entries * 2 + i] = lut->entries[i].blue;
	}

	err = drmModeCrtcSetGamma(screen->fd, screen->crtc, lut->num_entries,
				  &buffer[lut->num_entries * 0],
				  &buffer[lut->num_entries * 1],
				  &buffer[lut->num_entries * 2]);

	free(buffer);

	return err;
}

int drm_kms_lut_load_palette(struct drm_kms_lut **lutp, const char *filename)
{
	struct drm_kms_lut_entry *entry;
	struct drm_kms_lut *lut;
	uint8_t color[3];
	unsigned int i;
	ssize_t num;
	int fd, err;

	lut = malloc(sizeof(*lut));
	if (!lut)
		return -ENOMEM;

	lut->num_entries = 256;

	lut->entries = calloc(lut->num_entries, sizeof(*entry));
	if (!lut->entries) {
		free(lut);
		return -ENOMEM;
	}

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		free(lut->entries);
		free(lut);
		return -errno;
	}

	for (i = 0; i < lut->num_entries; i++) {
		num = read(fd, color, sizeof(color));
		if (num <= 0) {
			if (num == 0)
				err = -EILSEQ;
			else
				err = -errno;

			free(lut->entries);
			free(lut);
			return err;
		}

		lut->entries[i].red = color[0] << 8;
		lut->entries[i].green = color[1] << 8;
		lut->entries[i].blue = color[2] << 8;
	}

	*lutp = lut;

	return 0;
}

void drm_kms_lut_free(struct drm_kms_lut *lut)
{
	if (!lut)
		return;

	free(lut->entries);
	free(lut);
}
