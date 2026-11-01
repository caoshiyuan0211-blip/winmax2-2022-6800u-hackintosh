// Shared capture/verify loop used by the fpauth CLI and pam_fpauth.
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "fpcore.h"
#include "fpusb.h"
#include "fpflow.h"

static long nowMs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000;
}

// returns FPF_OK, FPF_NOMATCH (tries used up), FPF_TIMEOUT, FPF_NOTPL, FPF_NODEV
int fp_auth(const char *user, int timeout_ms, int maxTries, fp_msg_fn msg, void *ctx, float *scoreOut)
{
	fp_tpl *t = malloc(sizeof *t);
	if (!t) return FPF_NODEV;
	if (fp_tpl_load(user, t)) { free(t); return FPF_NOTPL; }
	if (fpu_open()) { free(t); return FPF_NODEV; }
	fp_img *probe = malloc(sizeof *probe);
	unsigned char img[FP_N];
	long end = nowMs() + timeout_ms;
	int rc = FPF_TIMEOUT, tries = 0;
	float best = -1;
	if (msg) msg(ctx, "Touch the fingerprint sensor (or wait to type your password)");
	while (probe) {
		long left = end - nowMs();
		if (left <= 0) break;
		int c = fpu_capture(img, (int)left);
		if (c == 1) break;
		if (c < 0) { rc = FPF_NODEV; break; }
		fp_prep(img, probe);
		const char *sv = getenv("FPAUTH_SAVE");  // debug: save every probe as PGM
		if (sv) {
			static int seq;
			char fn[600];
			snprintf(fn, sizeof fn, "%s/probe%ld-%d.pgm", sv, (long)time(0), seq++);
			FILE *f = fopen(fn, "wb");
			if (f) { fprintf(f, "P5\n%d %d\n255\n", FP_W, FP_H); fwrite(img, 1, FP_N, f); fclose(f); }
		}
		if (probe->area < FP_MINAREA) { if (msg) msg(ctx, "Partial touch, cover the sensor fully and try again"); continue; }
		float s = fp_verify(probe, t, 0);
		if (s > best) best = s;
		if (s >= FP_THRESHOLD) { rc = FPF_OK; break; }
		if (++tries >= maxTries) { rc = FPF_NOMATCH; break; }
		if (msg) msg(ctx, "Fingerprint not recognised, try again");
	}
	fpu_close();
	free(probe);
	free(t);
	if (scoreOut) *scoreOut = best;
	return rc;
}
