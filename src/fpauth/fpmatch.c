// Small-area fingerprint matcher for CS9711 68x118 images (portable C, no deps).
// Preprocess: local mean/std normalisation + validity mask (finger present, not saturated).
// Match: rotation search + translation search with masked normalised cross-correlation.
// Coarse pass on 2x-decimated images (every position), refine top candidates at full res.
// Inner loops are branch-free (masked values are 0, mask is a float 0/1) so they vectorise.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "fpmatch.h"

#define R 5          // local window radius for normalisation
#define MINOV 1800   // min overlapping pixels (full res, of 8024)
#define RSTEP 4      // coarse rotation step (deg)
#define RMAX 32      // rotation range +-RMAX
#define NROT (2 * RMAX / RSTEP + 1)
#define TOPK 6
#define HW (FP_W / 2)
#define HH (FP_H / 2)

typedef struct { int w, h; float v[FP_N], m[FP_N], vv[FP_N]; } plane;

void fp_prep(const unsigned char *img, fp_img *o)
{
	float *in = malloc(sizeof(float) * FP_N);
	for (int i = 0; i < FP_N; i++) in[i] = img[i];
	for (int y = 0; y < FP_H; y++)
		for (int x = 0; x < FP_W; x++) {
			float s = 0, s2 = 0; int n = 0;
			for (int dy = -R; dy <= R; dy++) {
				int yy = y + dy; if (yy < 0 || yy >= FP_H) continue;
				for (int dx = -R; dx <= R; dx++) {
					int xx = x + dx; if (xx < 0 || xx >= FP_W) continue;
					float v = in[yy * FP_W + xx]; s += v; s2 += v * v; n++;
				}
			}
			float m = s / n, sd = sqrtf(fmaxf(s2 / n - m * m, 0));
			int i = y * FP_W + x;
			o->m[i] = sd > 18 && m < 200;  // background = bright/flat, dropout = flat
			o->v[i] = o->m[i] ? (in[i] - m) / (sd + 1) : 0;
		}
	unsigned char t[FP_N];
	memcpy(t, o->m, FP_N);
	for (int y = 0; y < FP_H; y++)
		for (int x = 0; x < FP_W; x++) {
			int i = y * FP_W + x;
			if (!t[i]) continue;
			if (x == 0 || y == 0 || x == FP_W - 1 || y == FP_H - 1 || !t[i - 1] || !t[i + 1] || !t[i - FP_W] || !t[i + FP_W])
				o->m[i] = 0, o->v[i] = 0;
		}
	o->area = 0;
	for (int i = 0; i < FP_N; i++) o->area += o->m[i];
	free(in);
	o->nm = fp_minutiae(o, o->mi, FP_MAXMINU, 0);
}

static void toPlane(const fp_img *p, plane *o)
{
	o->w = FP_W; o->h = FP_H;
	for (int i = 0; i < FP_N; i++) { o->m[i] = p->m[i]; o->v[i] = p->m[i] ? p->v[i] : 0; o->vv[i] = o->v[i] * o->v[i]; }
}

// 2x decimation (2x2 average; valid only if all 4 valid)
static void half(const plane *p, plane *o)
{
	o->w = HW; o->h = HH;
	for (int y = 0; y < HH; y++)
		for (int x = 0; x < HW; x++) {
			int j = 2 * y * FP_W + 2 * x, i = y * HW + x;
			float ok = p->m[j] * p->m[j + 1] * p->m[j + FP_W] * p->m[j + FP_W + 1];
			o->m[i] = ok;
			o->v[i] = ok * 0.25f * (p->v[j] + p->v[j + 1] + p->v[j + FP_W] + p->v[j + FP_W + 1]);
			o->vv[i] = o->v[i] * o->v[i];
		}
}

static void rot(const fp_img *p, float deg, plane *o)
{
	float a = deg * (float)M_PI / 180.f, c = cosf(a), s = sinf(a), cx = (FP_W - 1) / 2.f, cy = (FP_H - 1) / 2.f;
	o->w = FP_W; o->h = FP_H;
	for (int y = 0; y < FP_H; y++)
		for (int x = 0; x < FP_W; x++) {
			float sx = c * (x - cx) + s * (y - cy) + cx, sy = -s * (x - cx) + c * (y - cy) + cy;
			int x0 = (int)floorf(sx), y0 = (int)floorf(sy), i = y * FP_W + x;
			o->v[i] = o->m[i] = o->vv[i] = 0;
			if (x0 < 0 || y0 < 0 || x0 + 1 >= FP_W || y0 + 1 >= FP_H) continue;
			int j = y0 * FP_W + x0;
			if (!(p->m[j] && p->m[j + 1] && p->m[j + FP_W] && p->m[j + FP_W + 1])) continue;
			float fx = sx - x0, fy = sy - y0;
			o->v[i] = (p->v[j] * (1 - fx) + p->v[j + 1] * fx) * (1 - fy) + (p->v[j + FP_W] * (1 - fx) + p->v[j + FP_W + 1] * fx) * fy;
			o->m[i] = 1;
			o->vv[i] = o->v[i] * o->v[i];
		}
}

// masked NCC of a shifted by (dx,dy) over b (same plane size); returns overlap count in *ov
static float ncc(const plane *a, const plane *b, int dx, int dy, float *ov)
{
	int w = a->w, h = a->h;
	int y0 = dy > 0 ? dy : 0, y1 = h + (dy < 0 ? dy : 0);
	int x0 = dx > 0 ? dx : 0, x1 = w + (dx < 0 ? dx : 0);
	float sab = 0, saa = 0, sbb = 0, sa = 0, sb = 0, n = 0;
	for (int y = y0; y < y1; y++) {
		const float *av = a->v + (y - dy) * w - dx, *am = a->m + (y - dy) * w - dx, *aq = a->vv + (y - dy) * w - dx;
		const float *bv = b->v + y * w, *bm = b->m + y * w, *bq = b->vv + y * w;
		for (int x = x0; x < x1; x++) {
			sab += av[x] * bv[x]; n += am[x] * bm[x];
			sa += av[x] * bm[x]; sb += am[x] * bv[x];
			saa += aq[x] * bm[x]; sbb += am[x] * bq[x];
		}
	}
	*ov = n;
	if (n < 8) return -1;
	float ca = saa - sa * sa / n, cb = sbb - sb * sb / n;
	if (ca <= 0 || cb <= 0) return -1;
	return (sab - sa * sb / n) / sqrtf(ca * cb);
}

typedef struct { float s; int r, dx, dy; } cand;

// is (x,y) well inside the valid area of plane p (radius 3)?
static int inside(const float *m, float x, float y)
{
	int xi = (int)lrintf(x), yi = (int)lrintf(y);
	if (xi < 3 || yi < 3 || xi >= FP_W - 3 || yi >= FP_H - 3) return 0;
	for (int dy = -3; dy <= 3; dy += 3)
		for (int dx = -3; dx <= 3; dx += 3)
			if (!m[(yi + dy) * FP_W + xi + dx]) return 0;
	return 1;
}

static float angDiff(float a, float b)  // orientations mod pi
{
	float d = fmodf(fabsf(a - b), (float)M_PI);
	return d > (float)M_PI / 2 ? (float)M_PI - d : d;
}

// minutiae agreement for probe rotated by deg then shifted (dx,dy) onto template.
// P = rotated probe plane (for the template-minutia visibility test), T = template plane.
static void minuCheck(const fp_img *probe, const fp_img *tmpl, const plane *P, const plane *T, float deg, int dx, int dy, fp_cand *c)
{
	float a = deg * (float)M_PI / 180.f, co = cosf(a), si = sinf(a), cx = (FP_W - 1) / 2.f, cy = (FP_H - 1) / 2.f;
	float px[FP_MAXMINU], py[FP_MAXMINU], pa[FP_MAXMINU];
	int pin[FP_MAXMINU], np = 0, nt = 0, matched = 0;
	unsigned char used[FP_MAXMINU] = { 0 };
	for (int i = 0; i < probe->nm; i++) {
		float x = co * (probe->mi[i].x - cx) - si * (probe->mi[i].y - cy) + cx;
		float y = si * (probe->mi[i].x - cx) + co * (probe->mi[i].y - cy) + cy;
		px[i] = x + dx; py[i] = y + dy; pa[i] = probe->mi[i].a + a;
		pin[i] = inside(T->m, px[i], py[i]);
		np += pin[i];
	}
	for (int j = 0; j < tmpl->nm; j++) {
		const fp_minu *t = &tmpl->mi[j];
		if (!inside(P->m, t->x - dx, t->y - dy)) continue;
		nt++;
		int best = -1; float bd = 1e9f;
		for (int i = 0; i < probe->nm; i++) {
			if (used[i]) continue;
			float ex = px[i] - t->x, ey = py[i] - t->y, d = ex * ex + ey * ey;
			if (d < 5 * 5 && angDiff(pa[i], t->a) < 0.5f && d < bd) { bd = d; best = i; }
		}
		if (best >= 0) { used[best] = 1; matched++; }
	}
	c->np = np; c->nt = nt; c->matched = matched;
}

static void push(cand *top, cand c)
{
	if (c.s <= top[TOPK - 1].s) return;
	int k = TOPK - 1;
	while (k > 0 && top[k - 1].s < c.s) { top[k] = top[k - 1]; k--; }
	top[k] = c;
}

float fp_match(const fp_img *probe, const fp_img *tmpl, fp_result *res)
{
	plane *buf = malloc(4 * sizeof(plane));
	if (!buf) return -1;
	plane *T = buf, *Th = buf + 1, *P = buf + 2, *Ph = buf + 3;
	cand top[TOPK];
	for (int k = 0; k < TOPK; k++) top[k].s = -2;
	toPlane(tmpl, T);
	half(T, Th);
	for (int r = -RMAX; r <= RMAX; r += RSTEP) {
		rot(probe, (float)r, P);
		half(P, Ph);
		for (int dy = -(HH - 12); dy <= HH - 12; dy++)
			for (int dx = -(HW - 8); dx <= HW - 8; dx++) {
				float ov, s = ncc(Ph, Th, dx, dy, &ov);
				if (ov * 4 >= MINOV) push(top, (cand){ s, r, dx * 2, dy * 2 });
			}
	}
	cand bc = { -1, 0, 0, 0 }; float bo = 0;
	fp_result rr; memset(&rr, 0, sizeof rr);
	for (int k = 0; k < TOPK && top[k].s > -1; k++) {
		cand kb = { -1, 0, 0, 0 }; float ko = 0;
		for (int dr = -2; dr <= 2; dr++) {
			rot(probe, (float)(top[k].r + dr), P);
			for (int dy = -2; dy <= 2; dy++)
				for (int dx = -2; dx <= 2; dx++) {
					float ov, s = ncc(P, T, top[k].dx + dx, top[k].dy + dy, &ov);
					if (ov >= MINOV && s > kb.s) { kb = (cand){ s, top[k].r + dr, top[k].dx + dx, top[k].dy + dy }; ko = ov; }
				}
		}
		if (kb.s <= -1) continue;
		rot(probe, (float)kb.r, P);
		fp_cand *c = &rr.c[rr.nc++];
		*c = (fp_cand){ kb.s, (int)ko, kb.r, kb.dx, kb.dy, 0, 0, 0 };
		minuCheck(probe, tmpl, P, T, (float)kb.r, kb.dx, kb.dy, c);
		if (kb.s > bc.s) { bc = kb; bo = ko; }
	}
	free(buf);
	rr.score = bc.s; rr.overlap = (int)bo; rr.rot = bc.r; rr.dx = bc.dx; rr.dy = bc.dy;
	if (res) *res = rr;
	return bc.s;
}
