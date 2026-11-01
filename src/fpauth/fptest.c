// Offline evaluation: fptest a00.pgm ... -- b00.pgm ...
// genuine = each a_i vs all other a_j (best), impostor = each b vs all a (best).
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "fpmatch.h"

static int load(const char *f, unsigned char *img)
{
	FILE *fp = fopen(f, "rb");
	if (!fp) return -1;
	fseek(fp, -FP_N, SEEK_END);
	int n = fread(img, 1, FP_N, fp);
	fclose(fp);
	return n == FP_N ? 0 : -1;
}

static fp_img A[64], B[64];

int main(int argc, char **argv)
{
	int na = 0, nb = 0, side = 0;
	unsigned char img[FP_N];
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--")) { side = 1; continue; }
		if (load(argv[i], img)) { fprintf(stderr, "bad %s\n", argv[i]); continue; }
		fp_prep(img, side ? &B[nb++] : &A[na++]);
	}
	clock_t c0 = clock(); int pairs = 0;
	printf("genuine (a_i vs other a, best / per-template):\n");
	for (int i = 0; i < na; i++) {
		float best = -1; int bj = -1; fp_result r, br = { 0 };
		printf("a%02d area %4d:", i, A[i].area);
		for (int j = 0; j < na; j++) {
			if (j == i) continue;
			float s = fp_match(&A[i], &A[j], &r); pairs++;
			printf(" %3.0f", s * 100);
			if (s > best) best = s, bj = j, br = r;
		}
		printf("  BEST %.3f (a%02d rot %d d %d,%d ov %d)\n", best, bj, br.rot, br.dx, br.dy, br.overlap);
	}
	printf("impostor (b vs all a):\n");
	for (int i = 0; i < nb; i++) {
		float best = -1; int bj = -1; fp_result r, br = { 0 };
		printf("b%02d area %4d:", i, B[i].area);
		for (int j = 0; j < na; j++) {
			float s = fp_match(&B[i], &A[j], &r); pairs++;
			printf(" %3.0f", s * 100);
			if (s > best) best = s, bj = j, br = r;
		}
		printf("  BEST %.3f (a%02d rot %d d %d,%d ov %d)\n", best, bj, br.rot, br.dx, br.dy, br.overlap);
	}
	printf("%.1f ms/pair\n", (double)(clock() - c0) / CLOCKS_PER_SEC * 1000 / pairs);
	return 0;
}
