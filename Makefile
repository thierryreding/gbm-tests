sysroot = /srv/nfs/tegra210

PKG_CONFIG_LIBDIR=$(sysroot)/usr/lib/pkgconfig
PKG_CONFIG_SYSROOT_DIR=$(sysroot)
export PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR

CROSS_COMPILE=~/pbs-stage1/bin/aarch64-unknown-linux-gnu-
EXTRA_CFLAGS=--sysroot $(sysroot)

env = \
	PKG_CONFIG_LIBDIR=$(sysroot)/usr/lib/pkgconfig \
	PKG_CONFIG_SYSROOT_DIR=$(sysroot)

DRM_CFLAGS = $(shell $(env) pkg-config --cflags libdrm)
DRM_LIBS = $(shell $(env) pkg-config --libs libdrm)

CC = $(CROSS_COMPILE)gcc
CFLAGS = -O0 -ggdb -Wall -Werror $(EXTRA_CFLAGS) $(DRM_CFLAGS)
LIBS = -lpng -lgbm -lEGL -lGLESv2 $(DRM_LIBS)

drm-kms-objs = \
	drm-kms.o

drm-gpu-objs = \
	drm-gpu.o

all: kms-swap-buffers gles-clear gles-clear-offscreen

clean:
	rm -f kms-swap-buffers kms-swap-buffers.o
	rm -f gles-clear gles-clear.o
	rm -f common.o $(drm-kms-objs) $(drm-gpu-objs)

gles-clear: gles-clear.o common.o $(drm-kms-objs) $(drm-gpu-objs)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

gles-clear-offscreen: gles-clear-offscreen.o common.o $(drm-kms-objs) $(drm-gpu-objs)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

kms-swap-buffers: kms-swap-buffers.o $(drm-kms-objs)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
