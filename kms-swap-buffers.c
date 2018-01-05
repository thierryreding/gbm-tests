#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <drm_fourcc.h>

#include "drm-kms.h"

int main(int argc, char *argv[])
{
	struct drm_kms_screen_args args;
	struct drm_kms_screen *screen;
	unsigned int current = 0;
	int fd, err;

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		return 1;

	memset(&args, 0, sizeof(args));
	args.flags = DRM_KMS_SCREEN_FULLSCREEN;
	args.format = DRM_FORMAT_XRGB8888;

	err = drm_kms_screen_create_with_args(&screen, fd, &args);
	if (err < 0) {
		fprintf(stderr, "failed to create KMS screen: %d\n", err);
		return 1;
	}

	while (1) {
		struct drm_kms_surface *fb = screen->fb[current];
		int color = current ? 0x00 : 0xff;
		void *buffer;

		err = drm_kms_surface_lock(fb, &buffer);
		if (err < 0)
			break;

		memset(buffer, color, fb->bo->size);
		drm_kms_surface_unlock(fb);

		drm_kms_screen_swap(screen);

		current ^= 1;
	}

	drm_kms_screen_free(screen);

	close(fd);

	return 0;
}
