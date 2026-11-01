// Enrolment mosaic: stitch enrolled presses into one large normalised image of the finger, so a probe
// can be required to overlap (almost) completely. Small-overlap matches are what let other fingers
// through (parallel ridge patches correlate well); full-overlap NCC separates fingers much better.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "fpmosaic.h"

typedef struct { int w, h; float *v, *m, *vv; } gplane;

static void gpAlloc(gplane *p, int w, int h)
{
	p->w = w; p->h = h;
	p->v = calloc((size_t)w * h, sizeof(float));
	p->m = calloc((size_t)w * h, sizeof(float));
	p->vv = calloc((size_t)w * h, sizeof(float));
}

static void gpFree(gplane *p) { free(p->v); free(p->m); free(p->vv); }

// probe rotated by deg about its centre (bilinear) into a FP_W x FP_H plane
static void gpRot(const fp_img *p, float deg, gplane *o)
{
	float a = deg * (float)M_PI / 180.f, c = cosf(a), s = sinf(a), cx = (FP_W - 1) / 2.f, cy = (FP_H - 1) / 2.f;
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

static void gpHalf(const gplane *p, gplane *o)
{
	for (int y = 0; y < o->h; y++)
		for (int x = 0; x < o->w; x++) {
			int j = 2 * y * p->w + 2 * x, i = y * o->w + x;
			float ok = p->m[j] * p->m[j + 1] * p->m[j + p->w] * p->m[j + p->w + 1];
			o->m[i] = ok;
			o->v[i] = ok * 0.25f * (p->v[j] + p->v[j + 1] + p->v[j + p->w] + p->v[j + p->w + 1]);
			o->vv[i] = o->v[i] * o->v[i];
		}
}

// NCC of small plane a placed at (ox,oy) inside big plane b (a fully inside b's canvas)
static float gpNcc(const gplane *a, const gplane *b, int ox, int oy, float *ov, float *aValid)
{
	float sab = 0, saa = 0, sbb = 0, sa = 0, sb = 0, n = 0, na = 0;
	for (int y = 0; y < a->h; y++) {
		const float *av = a->v + y * a->w, *am = a->m + y * a->w, *aq = a->vv + y * a->w;
		const float *bv = b->v + (y + oy) * b->w + ox, *bm = b->m + (y + oy) * b->w + ox, *bq = b->vv + (y + oy) * b->w + ox;
		for (int x = 0; x < a->w; x++) {
			sab += av[x] * bv[x]; n += am[x] * bm[x]; na += am[x];
			sa += av[x] * bm[x]; sb += am[x] * bv[x];
			saa += aq[x] * bm[x]; sbb += am[x] * bq[x];
		}
	}
	*ov = n; if (aValid) *aValid = na;
	if (n < 16) return -1;
	float ca = saa - sa * sa / n, cb = sbb - sb * sb / n;
	if (ca <= 0 || cb <= 0) return -1;
	return (sab - sa * sb / n) / sqrtf(ca * cb);
}

#define RMAX 32
#define RSTEP 4
#define TOPK 6

typedef struct { float s; int r, x, y; } cand;

static void push(cand *top, cand c)
{
	if (c.s <= top[TOPK - 1].s) return;
	int k = TOPK - 1;
	while (k > 0 && top[k - 1].s < c.s) { top[k] = top[k - 1]; k--; }
	top[k] = c;
}

// mosaic plane view (normalised values = sum/cnt)
static void mosView(const fp_mosaic *mo, gplane *o)
{
	for (int i = 0; i < FP_MW * FP_MH; i++) {
		float c = mo->cnt[i];
		o->m[i] = c > 0;
		o->v[i] = c > 0 ? mo->sum[i] / c : 0;
		o->vv[i] = o->v[i] * o->v[i];
	}
}

float fp_mosaic_match(const fp_img *probe, const fp_mosaic *mo, float minFrac, int minOv, fp_mres *res)
{
	gplane M, Mh, P, Ph;
	gpAlloc(&M, FP_MW, FP_MH); gpAlloc(&Mh, FP_MW / 2, FP_MH / 2);
	gpAlloc(&P, FP_W, FP_H); gpAlloc(&Ph, FP_W / 2, FP_H / 2);
	mosView(mo, &M);
	gpHalf(&M, &Mh);
	cand top[TOPK];
	for (int k = 0; k < TOPK; k++) top[k].s = -2;
	for (int r = -RMAX; r <= RMAX; r += RSTEP) {
		gpRot(probe, (float)r, &P);
		gpHalf(&P, &Ph);
		for (int y = 0; y + Ph.h <= Mh.h; y++)
			for (int x = 0; x + Ph.w <= Mh.w; x++) {
				float ov, na, s = gpNcc(&Ph, &Mh, x, y, &ov, &na);
				if (na > 0 && ov >= minFrac * na && ov * 4 >= minOv) push(top, (cand){ s, r, x * 2, y * 2 });
			}
	}
	cand bc = { -1, 0, 0, 0 }; float bov = 0, bna = 0;
	for (int k = 0; k < TOPK && top[k].s > -1; k++)
		for (int dr = -2; dr <= 2; dr++) {
			gpRot(probe, (float)(top[k].r + dr), &P);
			for (int dy = -2; dy <= 2; dy++)
				for (int dx = -2; dx <= 2; dx++) {
					int x = top[k].x + dx, y = top[k].y + dy;
					if (x < 0 || y < 0 || x + FP_W > FP_MW || y + FP_H > FP_MH) continue;
					float ov, na, s = gpNcc(&P, &M, x, y, &ov, &na);
					if (na > 0 && ov >= minFrac * na && ov >= minOv && s > bc.s) { bc = (cand){ s, top[k].r + dr, x, y }; bov = ov; bna = na; }
				}
		}
	gpFree(&M); gpFree(&Mh); gpFree(&P); gpFree(&Ph);
	if (res) { res->score = bc.s; res->rot = bc.r; res->x = bc.x; res->y = bc.y; res->overlap = (int)bov; res->frac = bna > 0 ? bov / bna : 0; }
	return bc.s;
}

void fp_mosaic_init(fp_mosaic *mo)
{
	memset(mo, 0, sizeof *mo);
}

void fp_mosaic_paste(fp_mosaic *mo, const fp_img *probe, int rot, int x, int y)
{
	gplane P;
	gpAlloc(&P, FP_W, FP_H);
	gpRot(probe, (float)rot, &P);
	for (int yy = 0; yy < FP_H; yy++)
		for (int xx = 0; xx < FP_W; xx++) {
			int i = yy * FP_W + xx, j = (yy + y) * FP_MW + xx + x;
			if (!P.m[i]) continue;
			mo->sum[j] += P.v[i]; mo->cnt[j] += 1;
		}
	gpFree(&P);
	mo->n++;
}

// Build one mosaic from the presses not yet marked in used[]: seed = first unused press, then repeatedly
// add the unused press that registers best (ncc >= addThr with >= addOv overlap). Marks used presses.
int fp_mosaic_build(fp_mosaic *mo, const fp_img *imgs, int n, float addThr, int addOv, unsigned char *used)
{
	fp_mosaic_init(mo);
	int seed = -1;
	for (int i = 0; i < n && seed < 0; i++) if (!used[i]) seed = i;
	if (seed < 0) return 0;
	fp_mosaic_paste(mo, &imgs[seed], 0, (FP_MW - FP_W) / 2, (FP_MH - FP_H) / 2);
	used[seed] = 1;
	for (int added = 1; added;) {
		added = 0;
		int bi = -1; fp_mres br = { .score = -2 };
		for (int i = 0; i < n; i++) {
			if (used[i]) continue;
			fp_mres r;
			float s = fp_mosaic_match(&imgs[i], mo, 0.0f, addOv, &r);
			if (s >= addThr && s > br.score) { bi = i; br = r; }
		}
		if (bi >= 0) {
			fp_mosaic_paste(mo, &imgs[bi], br.rot, br.x, br.y);
			used[bi] = 1;
			added = 1;
		}
	}
	return mo->n;
}

// All presses -> as many mosaics as needed (each press in exactly one). Returns mosaic count.
int fp_mosaic_build_all(fp_mosaic *mos, int maxMos, const fp_img *imgs, int n, float addThr, int addOv)
{
	unsigned char used[256] = { 0 };
	int k = 0;
	while (k < maxMos && fp_mosaic_build(&mos[k], imgs, n, addThr, addOv, used)) k++;
	return k;
}
