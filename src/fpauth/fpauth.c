// fpauth - CS9711 fingerprint enrol/verify tool (macOS + Linux test build).
//   fpauth enroll [user] [presses]   (root on macOS; replaces the user's prints)
//   fpauth add    [user] [presses]   (append presses to existing prints)
//   fpauth verify [user] [seconds]   (exit 0 = match)
//   fpauth delete [user]
//   fpauth import user a.pgm ...     (prints from 68x118 PGM files)
//   fpauth pamtest <module.so>       (check the PAM module loads, before enabling it)
// user defaults to $SUDO_USER, then $USER.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include "fpcore.h"
#include "fpusb.h"
#include "fpflow.h"

static void say(void *ctx, const char *t) { (void)ctx; printf("%s\n", t); fflush(stdout); }

static const char *defUser(void)
{
	const char *u = getenv("SUDO_USER");
	if (!u || !*u) u = getenv("USER");
	return u ? u : "";
}

static int enroll(const char *user, int presses, int append)
{
	fp_tpl *t = calloc(1, sizeof *t);
	if (append && fp_tpl_load(user, t)) { fprintf(stderr, "no prints for %s yet\n", user); return 1; }
	if (!append) t->n = 0;
	if (t->n + presses > FP_MAXTPL) presses = FP_MAXTPL - t->n;
	if (fpu_open()) { fprintf(stderr, "fingerprint reader not found / busy\n"); return 1; }
	fp_img *im = malloc(sizeof *im);
	printf("Enrolling %d presses for '%s'. Each time: press the same finger, lift when told.\n"
	       "Move it a bit between presses (up/down/left/right, slight tilt) to cover the whole finger.\n", presses, user);
	for (int got = 0; got < presses;) {
		printf("[%d/%d] touch the sensor...\n", got + 1, presses); fflush(stdout);
		unsigned char img[FP_N];
		int c = fpu_capture(img, 30000);
		if (c == 1) { printf("timed out\n"); break; }
		if (c < 0) { printf("read error, retrying\n"); usleep(300000); continue; }
		fp_prep(img, im);
		if (im->area < FP_MINAREA) { printf("  partial touch (%d px), again\n", im->area); usleep(600000); continue; }
		float s = t->n ? fp_verify(im, t, 0) : 0;
		memcpy(t->raw[t->n], img, FP_N);
		t->img[t->n++] = *im;
		got++;
		printf("  ok (similarity to earlier presses %.2f) - lift finger\n", s); fflush(stdout);
		usleep(800000);
	}
	fpu_close();
	if (t->n == 0 || fp_tpl_save(user, t)) { fprintf(stderr, "could not save prints (run with sudo?)\n"); return 1; }
	printf("saved %d presses for '%s'\n", t->n, user);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: fpauth enroll|add|verify|delete [user] [n]  |  fpauth import user files.pgm...\n");
		return 2;
	}
	const char *cmd = argv[1], *user = argc > 2 ? argv[2] : defUser();
	if (!strcmp(cmd, "enroll") || !strcmp(cmd, "add"))
		return enroll(user, argc > 3 ? atoi(argv[3]) : 20, cmd[0] == 'a');
	if (!strcmp(cmd, "verify")) {
		float s;
		int rc = fp_auth(user, (argc > 3 ? atoi(argv[3]) : 10) * 1000, 3, say, 0, &s);
		const char *w[] = { "MATCH", "no match", "timeout", "no prints enrolled", "reader not found" };
		printf("%s (best score %.3f, threshold %.2f)\n", w[rc], s, FP_THRESHOLD);
		return rc;
	}
	if (!strcmp(cmd, "pamtest") && argc > 2) {
		void *h = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
		if (!h) { printf("LOAD FAILED: %s\n", dlerror()); return 1; }
		int ok = dlsym(h, "pam_sm_authenticate") && dlsym(h, "pam_sm_setcred");
		printf("%s\n", ok ? "module OK" : "module missing pam_sm_* symbols");
		return !ok;
	}
	if (!strcmp(cmd, "delete")) {
		char p[512];
		if (fp_tpl_path(user, p, sizeof p) || unlink(p)) { perror("delete"); return 1; }
		printf("deleted prints of '%s'\n", user);
		return 0;
	}
	if (!strcmp(cmd, "import") && argc > 3) {
		fp_tpl *t = calloc(1, sizeof *t);
		for (int i = 3; i < argc && t->n < FP_MAXTPL; i++) {
			FILE *f = fopen(argv[i], "rb");
			if (!f) continue;
			fseek(f, -FP_N, SEEK_END);
			if (fread(t->raw[t->n], 1, FP_N, f) == FP_N) t->n++;
			fclose(f);
		}
		if (fp_tpl_save(user, t)) { perror("save"); return 1; }
		printf("imported %d prints for '%s'\n", t->n, user);
		return 0;
	}
	fprintf(stderr, "unknown command\n");
	return 2;
}
