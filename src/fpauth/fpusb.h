#pragma once
// CS9711 USB access. Backends: fpusb_linux.c (usbfs), fpusb_mac.c (IOKit IOUSBLib).
// fpu_open: find + claim the reader, send init. 0 = ok.
// fpu_capture: wait up to timeout_ms for a finger, fill img (68x118). 0 = ok, 1 = timeout, -1 = error.
// fpu_cancel: from another thread, abort a pending fpu_capture.
int fpu_open(void);
int fpu_capture(unsigned char *img, int timeout_ms);
void fpu_cancel(void);
void fpu_close(void);

// shared protocol helper: raw 34x236 sensor order -> 68x118 image
static inline void fpu_unpack(const unsigned char *raw, unsigned char *img)
{
	for (int y = 0; y < 236; y++)
		for (int x = 0; x < 34; x++)
			img[(y / 2) * 68 + x * 2 + y % 2] = raw[y * 34 + x];
}
