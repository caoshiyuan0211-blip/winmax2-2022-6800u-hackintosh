#pragma once
#define FP_W 68
#define FP_H 118
#define FP_N (FP_W * FP_H)
typedef struct { float x, y, a; int type; } fp_minu;
#define FP_MAXMINU 48
typedef struct { float v[FP_N]; unsigned char m[FP_N]; int area; int nm; fp_minu mi[FP_MAXMINU]; } fp_img;
// per alignment candidate: ncc, overlap, minutiae in overlap (probe/template), matched pairs
typedef struct { float ncc; int overlap, rot, dx, dy, np, nt, matched; } fp_cand;
#define FP_NCAND 6
typedef struct { float score; int overlap, rot, dx, dy, np, nt, matched; fp_cand c[FP_NCAND]; int nc; } fp_result;
void fp_prep(const unsigned char *img, fp_img *out);
float fp_match(const fp_img *probe, const fp_img *tmpl, fp_result *res);
int fp_minutiae(const fp_img *im, fp_minu *out, int max, unsigned char *skelOut);
