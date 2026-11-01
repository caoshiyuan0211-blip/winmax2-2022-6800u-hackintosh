// CS9711 via macOS IOKit user-space USB (IOUSBLib interface plug-in). Needs root
// (sudo / authorizationhost) or a console user; no kext needed (no driver claims the device).
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include "fpusb.h"

typedef IOUSBInterfaceInterface300 **Intf;
static Intf intf;
static UInt8 pipeOut, pipeIn;
static volatile int cancelReq;

static int cmd(int type)
{
	unsigned char c[8] = { 0xea, (unsigned char)type, 0, 0, 0, 0, (unsigned char)type, 0xea };
	return (*intf)->WritePipeTO(intf, pipeOut, c, 8, 300, 300) == kIOReturnSuccess ? 0 : -1;
}

static int readIn(void *buf, UInt32 *len, UInt32 tmo)
{
	IOReturn r = (*intf)->ReadPipeTO(intf, pipeIn, buf, len, tmo, tmo);
	if (r == kIOUSBPipeStalled) (*intf)->ClearPipeStallBothEnds(intf, pipeIn);
	return r == kIOReturnSuccess ? 0 : (int)r;
}

static Intf openIntf(io_service_t svc)
{
	IOCFPlugInInterface **plug = 0;
	SInt32 score;
	Intf i = 0;
	if (IOCreatePlugInInterfaceForService(svc, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score) || !plug)
		return 0;
	(*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID300), (LPVOID *)&i);
	(*plug)->Release(plug);
	return i;
}

static int propInt(io_service_t s, CFStringRef k)
{
	int v = -1;
	CFTypeRef t = IORegistryEntryCreateCFProperty(s, k, kCFAllocatorDefault, 0);
	if (t) { if (CFGetTypeID(t) == CFNumberGetTypeID()) CFNumberGetValue(t, kCFNumberIntType, &v); CFRelease(t); }
	return v;
}

int fpu_open(void)
{
	if (intf) return 0;
	// Plain class match + manual ID check: IOUSBHostInterface only accepts certain USB
	// key combinations in a matching dict (idVendor+idProduct+bInterfaceNumber alone matches nothing).
	io_iterator_t it;
	if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOUSBHostInterface"), &it)) {
		fprintf(stderr, "fpusb: IOUSBHostInterface lookup failed\n"); return -1;
	}
	io_service_t s;
	int found = 0;
	while ((s = IOIteratorNext(it))) {
		if (!intf && propInt(s, CFSTR("idVendor")) == 0x2541 && propInt(s, CFSTR("idProduct")) == 0x9711 &&
		    propInt(s, CFSTR("bInterfaceNumber")) == 0) {
			found = 1;
			intf = openIntf(s);
		}
		IOObjectRelease(s);
	}
	IOObjectRelease(it);
	if (!found) { fprintf(stderr, "fpusb: no CS9711 (2541:9711) interface in IORegistry\n"); return -1; }
	if (!intf) { fprintf(stderr, "fpusb: CS9711 found but user-client plug-in failed\n"); return -1; }
	IOReturn r = (*intf)->USBInterfaceOpen(intf);
	if (r) r = (*intf)->USBInterfaceOpenSeize(intf);
	if (r) { fprintf(stderr, "fpusb: USBInterfaceOpen failed 0x%x\n", r); (*intf)->Release(intf); intf = 0; return -1; }
	UInt8 ne = 0;
	(*intf)->GetNumEndpoints(intf, &ne);
	for (UInt8 i = 1; i <= ne; i++) {
		UInt8 dir, num, type, ival; UInt16 mps;
		if ((*intf)->GetPipeProperties(intf, i, &dir, &num, &type, &mps, &ival)) continue;
		if (type != kUSBBulk) continue;
		if (dir == kUSBIn) pipeIn = i; else if (dir == kUSBOut) pipeOut = i;
	}
	if (!pipeIn || !pipeOut) { fprintf(stderr, "fpusb: bulk pipes not found (%d endpoints)\n", ne); fpu_close(); return -1; }
	unsigned char st[64]; UInt32 l;
	if (cmd(1)) { l = 64; readIn(st, &l, 300); cmd(2); l = 64; readIn(st, &l, 300); cmd(1); }
	l = 64; readIn(st, &l, 300);
	return 0;
}

typedef struct { unsigned char buf[8192]; UInt32 len; int rc; volatile int done; } rd;

static void *reader(void *a)
{
	rd *r = a;
	r->len = 8000;
	r->rc = readIn(r->buf, &r->len, 0);  // no timeout: completes on finger or AbortPipe
	r->done = 1;
	return 0;
}

int fpu_capture(unsigned char *img, int timeout_ms)
{
	static rd r;
	unsigned char raw[8000 + 64], st[64];
	UInt32 l;
	cancelReq = 0;
	memset(&r, 0, sizeof r);
	pthread_t th;
	if (pthread_create(&th, 0, reader, &r)) return -1;
	usleep(20000);  // let the IN request get queued before the scan command (as libfprint does)
	cmd(4);
	int waited = 0;
	while (!r.done && !cancelReq && waited < timeout_ms) { usleep(10000); waited += 10; }
	if (!r.done) {
		(*intf)->AbortPipe(intf, pipeIn);
		pthread_join(th, 0);
		cmd(2);
		l = 64; readIn(st, &l, 100);
		(*intf)->ClearPipeStallBothEnds(intf, pipeIn);
		return 1;
	}
	pthread_join(th, 0);
	int rc = -1;
	if (r.rc == 0 && r.len == 8000) {
		memcpy(raw, r.buf, 8000);
		l = 64; readIn(raw + 8000, &l, 300);
		fpu_unpack(raw, img);
		rc = 0;
	}
	cmd(2);
	l = 64; readIn(st, &l, 100);
	return rc;
}

void fpu_cancel(void) { cancelReq = 1; }

void fpu_close(void)
{
	if (!intf) return;
	(*intf)->USBInterfaceClose(intf);
	(*intf)->Release(intf);
	intf = 0; pipeIn = pipeOut = 0;
}
