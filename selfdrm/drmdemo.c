#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <cutils/log.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "png.h"

#include <linux/videodev2.h>

#define SKY_LOG(str) ALOGE( \
    "sky log FuncName::%s Line::%d Msg::%s Errorinfo::%s \n", \
    __func__, __LINE__, str, strerror(errno)) 



struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t *vaddr;
	uint32_t fb_id;
};

void open_device();
void modeset_create_fb(int fd, struct buffer_object *bo);
void modeset_destroy_fb(int fd, struct buffer_object *bo);
char* drm_device_name = "rcar-du"; // dev/dri/card0
char* device_name = "dev/dri/card0";

struct buffer_object buf;

static void sleep_ms(unsigned int secs)

{
    struct timeval tval;

    tval.tv_sec=secs/1000;

    tval.tv_usec=(secs*1000)%1000000;

    select(0,NULL,NULL,NULL,&tval);

}

int main(int argc, char **argv)
{
    SKY_LOG("start main");
    open_device();
    
    return 0;
}

void modeset_create_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb map = { };
    create.width = bo->width;
	create.height = bo->height;
    create.bpp = 32;

    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    bo->pitch = create.pitch;
    bo->size = create.size;
    bo->handle = create.handle;
    drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
			   bo->handle, &bo->fb_id);
    
    map.handle = create.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    bo->vaddr = mmap(NULL, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);
    memset(bo->vaddr, 0xff, bo->size);
}

void open_device()
{
    printf("sky open_device:: %s :: \n", strerror(errno));
    SKY_LOG("open_device");

    //drmopen_fd = drmOpen(drm_device_name, NULL);

    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;

    uint32_t conn_id;  //connector id
	uint32_t crtc_id;  //crtc id
	uint32_t plane_id; //
    
    int fd;
    SKY_LOG("open start");
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    SKY_LOG("open end");
    res = drmModeGetResources(fd);

    conn_id = res->connectors[0];
    crtc_id = res->crtcs[0];

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    plane_res = drmModeGetPlaneResources(fd);
    plane_res->planes[0];

    conn = drmModeGetConnector(fd, conn_id);
    buf.width = conn->modes[0].hdisplay;
    buf.height = conn->modes[0].vdisplay;
    SKY_LOG("modeset_create_fb start");
    modeset_create_fb(fd, &buf);
    SKY_LOG("modeset_create_fb end");
    drmModeSetCrtc(fd, crtc_id, buf.fb_id,
			0, 0, &conn_id, 1, &conn->modes[0]);

    // int result = drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0,
	// 	0, 0, 1280, 720,
	// 	0, 0, 1280 << 16, 720 << 16);

    int result = drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0,
			50, 50, 320, 320,
			0, 0, 320 << 16, 320 << 16);

    printf("sky drmModeSetPlane result:: %d::  \n", result);

    sleep_ms(5 * 1000);

    modeset_destroy_fb(fd, &buf);

	drmModeFreeConnector(conn);
	drmModeFreePlaneResources(plane_res);
	drmModeFreeResources(res);

	close(fd);

}

void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);

	munmap(bo->vaddr, bo->size);

	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}








