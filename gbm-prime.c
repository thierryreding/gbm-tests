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
	struct drm_kms_screen *screen;
	struct drm_kms_import import;
	unsigned int width, height;
	struct drm_kms_surface *fb;
	unsigned int frames = 0;
	struct gbm_device *gbm;
	int err, fd, prime;
	struct gbm_bo *bo;
	uint32_t handle;

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

	fd = open(argv[2], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "failed to open GBM device %s: %d\n", argv[2], errno);
		return 1;
	}

	gbm = gbm_create_device(fd);
	if (!gbm) {
		fprintf(stderr, "failed to create GBM device\n");
		return 1;
	}

	bo = gbm_bo_create(gbm, width, height, DRM_FORMAT_XRGB8888,
			   GBM_BO_USE_SCANOUT/* | GBM_BO_USE_LINEAR*/);
	if (!bo) {
		fprintf(stderr, "failed to create GBM buffer object\n");
		return 1;
	}

	handle = gbm_bo_get_handle(bo).u32;

	err = drmPrimeHandleToFD(fd, handle, DRM_RDWR | DRM_CLOEXEC, &prime);
	if (err < 0) {
		fprintf(stderr, "failed to get PRIME FD: %d\n", errno);
		return 1;
	}

	memset(&import, 0, sizeof(import));
	import.fd = prime;
	import.width = gbm_bo_get_width(bo);
	import.height = gbm_bo_get_height(bo);
	import.pitch = gbm_bo_get_stride(bo);
	import.format = gbm_bo_get_format(bo);

	err = drm_kms_screen_import_surface(screen, &fb, &import);
	if (err < 0) {
		fprintf(stderr, "failed to import surface: %d\n", err);
		return 1;
	}

	while (true) {
		const uint32_t colors[2] = {
			0xff0000ff,
			0x0000ffff,
		};
		unsigned int i, j;
		void *ptr;
#if 1
		size_t size;

		size = height * gbm_bo_get_stride(bo);

		if (1) {
			struct dma_buf_sync args;

			args.flags = DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_START;

			err = ioctl(prime, DMA_BUF_IOCTL_SYNC, &args);
			if (err < 0) {
				fprintf(stderr, "failed to invalidate buffer: %d\n", errno);
				return 1;
			}
		}

		ptr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, prime, 0);
		if (ptr == MAP_FAILED) {
			fprintf(stderr, "failed to mmap() DMA-BUF: %d\n", errno);
			break;
		}

		for (j = 0; j < height; j++) {
			uint32_t *pixels = ptr + j * gbm_bo_get_stride(bo);

			for (i = 0; i < width; i++)
				pixels[i] = colors[frames & 1];
		}

		munmap(ptr, size);

		if (1) {
			struct dma_buf_sync args;

			args.flags = DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_END;

			err = ioctl(prime, DMA_BUF_IOCTL_SYNC, &args);
			if (err < 0) {
				fprintf(stderr, "failed to flush buffer: %d\n", errno);
				return 1;
			}
		}
#else
		unsigned int stride = 0;
		void *data = NULL;

		ptr = gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_READ_WRITE, &stride, &data);
		if (!ptr) {
			fprintf(stderr, "failed to map GBM buffer object\n");
			return 1;
		}

		printf("stride: %u\n", stride);

		for (j = 0; j < height; j++) {
			uint32_t *pixels = ptr + j * gbm_bo_get_stride(bo);
			printf("  %p\n", pixels);

			for (i = 0; i < width; i++)
				pixels[i] = colors[frames & 1];
		}

		gbm_bo_unmap(bo, data);
#endif

		err = drm_kms_screen_swap_to(screen, fb);
		if (err < 0) {
			fprintf(stderr, "failed to swap screen: %d\n", err);
			return 1;
		}

		sleep(5);
		break;
		frames++;
	}

	drm_kms_screen_close(screen);

	return 0;
}
