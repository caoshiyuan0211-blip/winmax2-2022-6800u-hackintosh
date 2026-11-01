#pragma once
#include "fpmatch.h"
#define FP_MW 200
#define FP_MH 260
typedef struct { int n; float sum[FP_MW * FP_MH], cnt[FP_MW * FP_MH]; } fp_mosaic;
typedef struct { float score, frac; int rot, x, y, overlap; } fp_mres;
void fp_mosaic_init(fp_mosaic *mo);
void fp_mosaic_paste(fp_mosaic *mo, const fp_img *probe, int rot, int x, int y);
float fp_mosaic_match(const fp_img *probe, const fp_mosaic *mo, float minFrac, int minOv, fp_mres *res);
int fp_mosaic_build(fp_mosaic *mo, const fp_img *imgs, int n, float addThr, int addOv, unsigned char *used);
int fp_mosaic_build_all(fp_mosaic *mos, int maxMos, const fp_img *imgs, int n, float addThr, int addOv);
