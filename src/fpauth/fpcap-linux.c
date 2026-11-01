// CS9711 (2541:9711) fingerprint capture on Linux via usbfs (no libusb).
// Protocol from libfprint-CS9711 (ddlsmurf): cmd = EA t 00 00 00 00 t EA on EP 0x01,
// init(1) -> 8-byte status, scan(4): IN read of 8000 B is queued BEFORE the cmd and completes
// when a finger is pressed, then 24 B tail, then reset(2). Raw 34x236 -> 68x118 interleave.
// usage: fpcap-linux <out-prefix> <count>   -> writes <prefix>NN.pgm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#define W 68
#define H 118
static int fd = -1;

static int findDev(char *path)
{
	DIR *d = opendir("/sys/bus/usb/devices");
	struct dirent *e;
	char p[512], b[32];
	while (d && (e = readdir(d))) {
		snprintf(p, sizeof p, "/sys/bus/usb/devices/%s/idVendor", e->d_name);
		FILE *f = fopen(p, "r");
		if (!f) continue;
		int ok = fgets(b, sizeof b, f) && !strncmp(b, "2541", 4);
		fclose(f);
		if (!ok) continue;
		int bus = 0, dev = 0;
		snprintf(p, sizeof p, "/sys/bus/usb/devices/%s/busnum", e->d_name);
		f = fopen(p, "r"); if (f) { fscanf(f, "%d", &bus); fclose(f); }
		snprintf(p, sizeof p, "/sys/bus/usb/devices/%s/devnum", e->d_name);
		f = fopen(p, "r"); if (f) { fscanf(f, "%d", &dev); fclose(f); }
		sprintf(path, "/dev/bus/usb/%03d/%03d", bus, dev);
		closedir(d);
		return 0;
	}
	if (d) closedir(d);
	return -1;
}

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

static unsigned char raw[8000 + 24 + 64];

static int capture(unsigned char *img)
{
	static unsigned char rb[8192];
	struct usbdevfs_urb u = { .type = USBDEVFS_URB_TYPE_BULK, .endpoint = 0x81, .buffer = rb, .buffer_length = 8000 };
	if (ioctl(fd, USBDEVFS_SUBMITURB, &u) < 0) { perror("submit"); return -1; }
	if (cmd(4) < 0) { perror("scan cmd"); }
	struct usbdevfs_urb *done = 0;
	if (ioctl(fd, USBDEVFS_REAPURB, &done) < 0) { perror("reap"); return -1; }
	if (u.actual_length != 8000) { fprintf(stderr, "first block %d bytes (status %d)\n", u.actual_length, u.status); cmd(2); return -1; }
	memcpy(raw, rb, 8000);
	int n = bulk(0x81, raw + 8000, 64, 300);
	if (n != 24) fprintf(stderr, "tail %d bytes\n", n);
	cmd(2);
	unsigned char st[64];
	bulk(0x81, st, 64, 100);  // swallow any status after reset
	for (int y = 0; y < 236; y++)
		for (int x = 0; x < 34; x++)
			img[(y / 2) * W + x * 2 + y % 2] = raw[y * 34 + x];
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: %s prefix count\n", argv[0]); return 1; }
	char path[64];
	if (findDev(path)) { fprintf(stderr, "no 2541 device\n"); return 1; }
	fd = open(path, O_RDWR);
	if (fd < 0) { perror(path); return 1; }
	unsigned int ifn = 0;
	ioctl(fd, USBDEVFS_DISCONNECT, &(struct usbdevfs_ioctl){ .ifno = 0, .ioctl_code = USBDEVFS_DISCONNECT });
	if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &ifn) < 0) { perror("claim"); return 1; }
	unsigned char st[64];
	if (cmd(1) < 0) { perror("init cmd -> reset"); bulk(0x81, st, 64, 300); cmd(2); bulk(0x81, st, 64, 300); cmd(1); }
	int n = bulk(0x81, st, 64, 300);
	printf("init status (%d):", n);
	for (int i = 0; i < n; i++) printf(" %02x", st[i]);
	printf("\n");
	int cnt = atoi(argv[2]);
	unsigned char img[W * H];
	for (int i = 0; i < cnt; i++) {
		printf("[%d/%d] press finger...\n", i + 1, cnt); fflush(stdout);
		usleep(250000);
		if (capture(img)) { i--; usleep(300000); continue; }
		char fn[256];
		snprintf(fn, sizeof fn, "%s%02d.pgm", argv[1], i);
		FILE *f = fopen(fn, "wb");
		fprintf(f, "P5\n%d %d\n255\n", W, H);
		fwrite(img, 1, W * H, f);
		fclose(f);
		int mn = 255, mx = 0; long s = 0;
		for (int k = 0; k < W * H; k++) { if (img[k] < mn) mn = img[k]; if (img[k] > mx) mx = img[k]; s += img[k]; }
		printf("  saved %s (min %d max %d mean %ld) - lift finger\n", fn, mn, mx, s / (W * H)); fflush(stdout);
		usleep(800000);
	}
	ioctl(fd, USBDEVFS_RELEASEINTERFACE, &ifn);
	return 0;
}
