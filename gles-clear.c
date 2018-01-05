#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/dma-buf.h>

#include <drm_fourcc.h>
#include <gbm.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include "drm-kms.h"
#include "drm-gpu.h"

int main(int argc, char *argv[])
{
	struct drm_kms_screen_args args;
	struct drm_gpu_surface *surface;
	struct drm_kms_screen *screen;
	struct drm_kms_import import;
	unsigned int width, height;
	struct drm_kms_surface *fb;
	struct drm_gpu_buffer *bo;
	unsigned int frames = 0;
	struct drm_gpu *gpu;
	int err;

	memset(&args, 0, sizeof(args));
	args.flags = DRM_KMS_SCREEN_FULLSCREEN;
	args.format = DRM_FORMAT_XRGB8888;

	err = drm_kms_screen_open_with_args(&screen, argv[1], &args);
	if (err < 0) {
		fprintf(stderr, "failed to open screen: %d\n", err);
		return 1;
	}

	width = screen->width;
	height = screen->height;

	err = drm_gpu_open(&gpu, argv[2]);
	if (err < 0) {
		fprintf(stderr, "failed to open GPU: %d\n", err);
		return 1;
	}

	err = drm_gpu_surface_create(&surface, gpu, width, height,
				     DRM_FORMAT_XRGB8888,
				     DRM_GPU_SCANOUT | DRM_GPU_RENDER);
	if (err < 0) {
		fprintf(stderr, "failed to create GPU surface: %d\n", err);
		return 1;
	}

	drm_gpu_bind_surface(gpu, surface);

	while (true) {
		const float colors[2][4] = {
			{ 1.0, 0.0, 0.0, 1.0 },
			{ 0.0, 0.0, 1.0, 1.0 },
		};
		const float *color = colors[frames & 1];

		glViewport(0, 0, width, height);
		glClearColor(color[0], color[1], color[2], color[3]);
		glClear(GL_COLOR_BUFFER_BIT);
		eglSwapBuffers(gpu->egl.display, surface->egl.surface);

		err = drm_gpu_surface_lock(surface, &bo);
		if (err < 0) {
			fprintf(stderr, "failed to lock GPU surface: %d\n", err);
			return 1;
		}

		memset(&import, 0, sizeof(import));
		import.fd = bo->fd;
		import.width = bo->width;
		import.height = bo->height;
		import.pitch = bo->stride;
		import.format = bo->format;

		err = drm_kms_screen_import_surface(screen, &fb, &import);
		if (err < 0) {
			fprintf(stderr, "failed to import surface: %d\n", err);
			return 1;
		}

		drm_gpu_surface_unlock(surface, bo);

		err = drm_kms_screen_swap_to(screen, fb);
		if (err < 0) {
			fprintf(stderr, "failed to swap screen: %d\n", err);
			return 1;
		}

		sleep(1);
		frames++;
	}

	drm_gpu_close(gpu);
	drm_kms_screen_close(screen);

	return 0;
}
