#pragma once
#include "fpmatch.h"
#define FP_THRESHOLD 0.88f   // logs/fp 10-31: wrong fingers max 0.851 (31 presses), own finger ~80% per press
#define FP_MAXTPL 32
#define FP_MINAREA 4500      // reject captures with less valid skin (of 8024 px)
#ifdef __APPLE__
#define FP_DIR "/var/db/fpauth"
#else
#define FP_DIR "/tmp/fpauth-test"
#endif
typedef struct { int n; unsigned char raw[FP_MAXTPL][FP_N]; fp_img img[FP_MAXTPL]; } fp_tpl;
int fp_tpl_path(const char *user, char *out, int len);
int fp_tpl_load(const char *user, fp_tpl *t);       // 0 ok (preps all images)
int fp_tpl_save(const char *user, const fp_tpl *t);  // root-only file
// best score of probe vs all templates (parallel, stops early once >= FP_THRESHOLD)
float fp_verify(const fp_img *probe, const fp_tpl *t, int *which);
