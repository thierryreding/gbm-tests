#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/dma-buf.h>

#include <gbm.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

int main(int argc, char *argv[])
{
	const unsigned int width = 32, height = 32;
	const char *path = "/dev/dri/card2";
	struct gbm_surface *surface;
	struct gbm_device *device;
	void *ptr, *data = NULL;
	EGLint major, minor;
	uint32_t stride = 0;
	int fd, prime, err;
	EGLDisplay display;
	EGLContext context;
	EGLSurface window;
	unsigned int i, j;
	struct gbm_bo *bo;
	EGLConfig config;
	EGLint count;

	if (argc > 1)
		path = argv[1];

	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s: %d\n", path, errno);
		return 1;
	}

	device = gbm_create_device(fd);
	if (!device) {
		fprintf(stderr, "failed to create GBM device\n");
		return 1;
	}

	surface = gbm_surface_create(device, width, height, GBM_FORMAT_XRGB8888,
				     GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!surface) {
		fprintf(stderr, "failed to create GBM surface\n");
		return 1;
	}

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};

	display = eglGetDisplay(device);
	if (!display) {
		fprintf(stderr, "failed to create EGL display\n");
		return 1;
	}

	if (!eglInitialize(display, &major, &minor)) {
		fprintf(stderr, "failed to initialize EGL\n");
		return 1;
	}

	printf("EGL %d.%d\n", major, minor);

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind to OpenGL ES API\n");
		return 1;
	}

	if (!eglChooseConfig(display, config_attribs, &config, 1, &count)) {
		fprintf(stderr, "failed to choose EGL configuration\n");
		return 1;
	}

	context = eglCreateContext(display, config, EGL_NO_CONTEXT,
				   context_attribs);
	if (!context) {
		fprintf(stderr, "failed to create EGL context\n");
		return 1;
	}

	window = eglCreateWindowSurface(display, config, surface, NULL);
	if (!window) {
		fprintf(stderr, "failed to create EGL window\n");
		return 1;
	}

	eglMakeCurrent(display, window, window, context);

	for (i = 0; i < 1; i++) {
		glViewport(0, 0, width, height);
		glClearColor(1.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		eglSwapBuffers(display, window);
	}

	bo = gbm_surface_lock_front_buffer(surface);
	printf("stride: %u\n", gbm_bo_get_stride(bo));

	prime = gbm_bo_get_fd(bo);
	printf("fd: %d\n", prime);

	struct dma_buf_sync args;
	args.flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_START;

	err = ioctl(prime, DMA_BUF_IOCTL_SYNC, &args);
	if (err < 0)
		fprintf(stderr, "failed to sync DMA-BUF: %d\n", errno);

	ptr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, prime, 0);
	if (ptr == MAP_FAILED)
		fprintf(stderr, "failed to mmap DMA-BUF: %d\n", errno);

	/*
	ptr = gbm_bo_map(bo, 0, 0, width, height, 0, &stride, &data);
	printf("ptr: %p data: %p stride: %u\n", ptr, data, stride);
	*/

	for (j = 0; j < height; j++) {
		uint32_t *pixels = ptr + j * stride;

		for (i = 0; i < width; i++) {
			if (pixels[i] != 0xffff0000)
				printf("unexpected value %08x at %2ux%2u\n",
				       pixels[i], i, j);
		}
	}

	/*
	gbm_bo_unmap(bo, data);
	*/
	munmap(ptr, 4096);


	close(fd);

	return 0;
}
