// CS9711 via Linux usbfs (test backend; the macOS backend is fpusb_mac.c).
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include "fpusb.h"

static int fd = -1;
static volatile int cancelReq;

static int bulk(int ep, void *buf, int len, int tmo)
{
	struct usbdevfs_bulktransfer t = { .ep = ep, .len = len, .timeout = tmo, .data = buf };
	return ioctl(fd, USBDEVFS_BULK, &t);
}

static int cmd(int type)
{
	unsigned char c[8] = { 0xea, type, 0, 0, 0, 0, type, 0xea };
	return bulk(0x01, c, 8, 300);
}

int fpu_open(void)
{
	DIR *d = opendir("/sys/bus/usb/devices");
	struct dirent *e;
	char p[512], b[32], path[64] = "";
	while (d && (e = readdir(d))) {
		snprintf(p, sizeof p, "/sys/bus/usb/devices/%s/idVendor", e->d_name);
		FILE *f = fopen(p, "r");
		if (!f) continue;
		int ok = fgets(b, sizeof b, f) && !strncmp(b, "2541", 4);
		fclose(f);
		if (!ok) continue;
		int bus = 0, dev = 0;
		snprintf(p, sizeof p, "/sys/bus/usb/devices/%s/busnum", e->d_name);
		if ((f = fopen(p, "r"))) { if (fscanf(f, "%d", &bus) != 1) bus = 0; fclose(f); }
		snprintf(p, sizeof p, "/sys/bus/usb/devices/%s/devnum", e->d_name);
		if ((f = fopen(p, "r"))) { if (fscanf(f, "%d", &dev) != 1) dev = 0; fclose(f); }
		snprintf(path, sizeof path, "/dev/bus/usb/%03d/%03d", bus, dev);
		break;
	}
	if (d) closedir(d);
	if (!path[0]) return -1;
	if ((fd = open(path, O_RDWR)) < 0) return -1;
	unsigned int ifn = 0;
	if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &ifn) < 0) { close(fd); fd = -1; return -1; }
	unsigned char st[64];
	if (cmd(1) < 0) { bulk(0x81, st, 64, 300); cmd(2); bulk(0x81, st, 64, 300); cmd(1); }
	bulk(0x81, st, 64, 300);
	return 0;
}

int fpu_capture(unsigned char *img, int timeout_ms)
{
	static unsigned char rb[8192], raw[8000 + 64];
	cancelReq = 0;
	struct usbdevfs_urb u = { .type = USBDEVFS_URB_TYPE_BULK, .endpoint = 0x81, .buffer = rb, .buffer_length = 8000 };
	if (ioctl(fd, USBDEVFS_SUBMITURB, &u) < 0) return -1;
	cmd(4);
	struct usbdevfs_urb *done = 0;
	int waited = 0, rc = -1;
	for (;;) {
		if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &done) == 0) break;
		if (cancelReq || waited >= timeout_ms) {
			ioctl(fd, USBDEVFS_DISCARDURB, &u);
			ioctl(fd, USBDEVFS_REAPURB, &done);
			cmd(2);
			unsigned char st[64];
			bulk(0x81, st, 64, 100);
			return 1;
		}
		usleep(10000); waited += 10;
	}
	if (u.actual_length == 8000) {
		memcpy(raw, rb, 8000);
		bulk(0x81, raw + 8000, 64, 300);
		fpu_unpack(raw, img);
		rc = 0;
	}
	cmd(2);
	unsigned char st[64];
	bulk(0x81, st, 64, 100);
	return rc;
}

void fpu_cancel(void) { cancelReq = 1; }

void fpu_close(void)
{
	if (fd < 0) return;
	unsigned int ifn = 0;
	ioctl(fd, USBDEVFS_RELEASEINTERFACE, &ifn);
	close(fd);
	fd = -1;
}
