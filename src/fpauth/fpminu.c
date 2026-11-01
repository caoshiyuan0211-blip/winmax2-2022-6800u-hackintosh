// Minutiae extraction for small CS9711 images: smooth -> binarise (ridge = v < 0) ->
// Zhang-Suen thinning -> crossing number (1 = ending, 3 = bifurcation) -> drop points near
// the mask edge and clustered pairs (breaks / bridges). Angle = local ridge orientation (mod 180).
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "fpmatch.h"

#define EDGE 6      // min distance (px) to invalid area / border
#define CLUSTER 6   // two minutiae closer than this = artefact, drop both

static float orient(const float *s, const unsigned char *m, int x, int y)
{
	float gxx = 0, gyy = 0, gxy = 0;
	for (int dy = -4; dy <= 4; dy++)
		for (int dx = -4; dx <= 4; dx++) {
			int xx = x + dx, yy = y + dy;
			if (xx < 1 || yy < 1 || xx >= FP_W - 1 || yy >= FP_H - 1) continue;
			int i = yy * FP_W + xx;
			if (!m[i]) continue;
			float gx = s[i + 1] - s[i - 1], gy = s[i + FP_W] - s[i - FP_W];
			gxx += gx * gx; gyy += gy * gy; gxy += gx * gy;
		}
	// ridge direction = perpendicular to dominant gradient
	return 0.5f * atan2f(2 * gxy, gxx - gyy) + (float)M_PI / 2;
}

static void thin(unsigned char *b)
{
	unsigned char *del = malloc(FP_N);
	for (int changed = 1, it = 0; changed && it < 40; it++) {
		changed = 0;
		for (int pass = 0; pass < 2; pass++) {
			memset(del, 0, FP_N);
			for (int y = 1; y < FP_H - 1; y++)
				for (int x = 1; x < FP_W - 1; x++) {
					int i = y * FP_W + x;
					if (!b[i]) continue;
					int p[9] = { b[i - FP_W], b[i - FP_W + 1], b[i + 1], b[i + FP_W + 1], b[i + FP_W], b[i + FP_W - 1], b[i - 1], b[i - FP_W - 1] };
					p[8] = p[0];
					int B = 0, A = 0;
					for (int k = 0; k < 8; k++) { B += p[k]; A += !p[k] && p[k + 1]; }
					if (B < 2 || B > 6 || A != 1) continue;
					if (pass == 0 ? (p[0] && p[2] && p[4]) || (p[2] && p[4] && p[6]) : (p[0] && p[2] && p[6]) || (p[0] && p[4] && p[6])) continue;
					del[i] = 1;
				}
			for (int i = 0; i < FP_N; i++) if (del[i]) { b[i] = 0; changed = 1; }
		}
	}
	free(del);
}

int fp_minutiae(const fp_img *im, fp_minu *out, int max, unsigned char *skelOut)
{
	float *s = malloc(sizeof(float) * FP_N);
	unsigned char *b = malloc(FP_N), *ok = malloc(FP_N);
	static const float k[3] = { 0.25f, 0.5f, 0.25f };
	for (int y = 0; y < FP_H; y++)
		for (int x = 0; x < FP_W; x++) {
			float acc = 0, w = 0;
			for (int dy = -1; dy <= 1; dy++)
				for (int dx = -1; dx <= 1; dx++) {
					int xx = x + dx, yy = y + dy;
					if (xx < 0 || yy < 0 || xx >= FP_W || yy >= FP_H || !im->m[yy * FP_W + xx]) continue;
					acc += k[dx + 1] * k[dy + 1] * im->v[yy * FP_W + xx]; w += k[dx + 1] * k[dy + 1];
				}
			s[y * FP_W + x] = w > 0 ? acc / w : 0;
		}
	for (int i = 0; i < FP_N; i++) b[i] = im->m[i] && s[i] < 0;
	thin(b);
	// "ok" = all pixels within EDGE are valid skin
	for (int y = 0; y < FP_H; y++)
		for (int x = 0; x < FP_W; x++) {
			int good = x >= EDGE && y >= EDGE && x < FP_W - EDGE && y < FP_H - EDGE;
			for (int dy = -EDGE; good && dy <= EDGE; dy += 2)
				for (int dx = -EDGE; good && dx <= EDGE; dx += 2)
					good = im->m[(y + dy) * FP_W + x + dx];
			ok[y * FP_W + x] = good;
		}
	fp_minu tmp[256];
	int n = 0;
	for (int y = 1; y < FP_H - 1 && n < 256; y++)
		for (int x = 1; x < FP_W - 1 && n < 256; x++) {
			int i = y * FP_W + x;
			if (!b[i] || !ok[i]) continue;
			int p[9] = { b[i - FP_W], b[i - FP_W + 1], b[i + 1], b[i + FP_W + 1], b[i + FP_W], b[i + FP_W - 1], b[i - 1], b[i - FP_W - 1] };
			p[8] = p[0];
			int cn = 0;
			for (int q = 0; q < 8; q++) cn += p[q] != p[q + 1];
			cn /= 2;
			if (cn != 1 && cn != 3) continue;
			tmp[n++] = (fp_minu){ (float)x, (float)y, orient(s, im->m, x, y), cn };
		}
	unsigned char drop[256] = { 0 };
	for (int a = 0; a < n; a++)
		for (int c = a + 1; c < n; c++) {
			float dx = tmp[a].x - tmp[c].x, dy = tmp[a].y - tmp[c].y;
			if (dx * dx + dy * dy < CLUSTER * CLUSTER) drop[a] = drop[c] = 1;
		}
	int m = 0;
	for (int a = 0; a < n && m < max; a++) if (!drop[a]) out[m++] = tmp[a];
	if (skelOut) memcpy(skelOut, b, FP_N);
	free(s); free(b); free(ok);
	return m;
}
