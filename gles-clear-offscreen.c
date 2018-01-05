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
	unsigned int width = 8, height = 4, i, j;
	struct drm_gpu_surface *surface;
	struct drm_gpu_buffer *bo;
	struct drm_gpu *gpu;
	uint32_t stride;
	void *ptr;
	int err;

	err = drm_gpu_open(&gpu, argv[1]);
	if (err < 0) {
		fprintf(stderr, "failed to open GPU: %d\n", err);
		return 1;
	}

	err = drm_gpu_surface_create(&surface, gpu, width, height,
				     DRM_FORMAT_ARGB8888, DRM_GPU_RENDER);
	if (err < 0) {
		fprintf(stderr, "failed to create GPU surface: %d\n", err);
		return 1;
	}

	drm_gpu_bind_surface(gpu, surface);

	glViewport(0, 0, width, height);
	glClearColor(1.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(gpu->egl.display, surface->egl.surface);

	err = drm_gpu_surface_lock(surface, &bo);
	if (err < 0) {
		fprintf(stderr, "failed to lock GPU surface: %d\n", err);
		return 1;
	}

	err = drm_gpu_buffer_map(bo, &ptr, &stride);
	if (err < 0) {
		fprintf(stderr, "failed to map GPU buffer: %d\n", err);
		return 1;
	}

	printf("stride: %u\n", stride);

	for (j = 0; j < height; j++) {
		uint32_t *pixels = ptr + j * stride;

		for (i = 0; i < width; i++) {
			printf(" %08x", pixels[i]);
		}

		printf("\n");
	}

	drm_gpu_buffer_unmap(bo);

	drm_gpu_surface_unlock(surface, bo);
	drm_gpu_close(gpu);

	return 0;
}
