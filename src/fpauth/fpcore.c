// Template storage + parallel verification.
// File: FP_DIR/<user>.tpl, mode 0600, dir 0700: "FPA1" + u32 count + count * 8024 raw image bytes.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fpcore.h"

int fp_tpl_path(const char *user, char *out, int len)
{
	for (const char *c = user; *c; c++)
		if (*c == '/' || (*c == '.' && c == user)) return -1;
	snprintf(out, len, "%s/%s.tpl", FP_DIR, user);
	return 0;
}

int fp_tpl_load(const char *user, fp_tpl *t)
{
	char p[512];
	if (fp_tpl_path(user, p, sizeof p)) return -1;
	FILE *f = fopen(p, "rb");
	if (!f) return -1;
	char mg[4]; uint32_t n = 0;
	int ok = fread(mg, 1, 4, f) == 4 && !memcmp(mg, "FPA1", 4) && fread(&n, 4, 1, f) == 1 && n > 0 && n <= FP_MAXTPL;
	if (ok) ok = fread(t->raw, FP_N, n, f) == n;
	fclose(f);
	if (!ok) return -1;
	t->n = (int)n;
	for (int i = 0; i < t->n; i++) fp_prep(t->raw[i], &t->img[i]);
	return 0;
}

int fp_tpl_save(const char *user, const fp_tpl *t)
{
	char p[512], tmp[520];
	if (fp_tpl_path(user, p, sizeof p)) return -1;
	mkdir(FP_DIR, 0700);
	chmod(FP_DIR, 0700);
	snprintf(tmp, sizeof tmp, "%s.new", p);
	umask(077);
	FILE *f = fopen(tmp, "wb");
	if (!f) return -1;
	uint32_t n = (uint32_t)t->n;
	int ok = fwrite("FPA1", 1, 4, f) == 4 && fwrite(&n, 4, 1, f) == 1 && fwrite(t->raw, FP_N, n, f) == n;
	ok = fclose(f) == 0 && ok;
	if (!ok || rename(tmp, p)) { unlink(tmp); return -1; }
	chmod(p, 0600);
	return 0;
}

typedef struct {
	const fp_img *probe; const fp_tpl *t;
	pthread_mutex_t mu; int next, which; float best; int done;
} job;

static void *worker(void *a)
{
	job *j = a;
	for (;;) {
		pthread_mutex_lock(&j->mu);
		int i = j->done ? j->t->n : j->next++;
		pthread_mutex_unlock(&j->mu);
		if (i >= j->t->n) return 0;
		float s = fp_match(j->probe, &j->t->img[i], 0);
		pthread_mutex_lock(&j->mu);
		if (s > j->best) { j->best = s; j->which = i; }
		if (s >= FP_THRESHOLD) j->done = 1;
		pthread_mutex_unlock(&j->mu);
	}
}

float fp_verify(const fp_img *probe, const fp_tpl *t, int *which)
{
	job j = { .probe = probe, .t = t, .best = -1, .which = -1 };
	pthread_mutex_init(&j.mu, 0);
	long nc = sysconf(_SC_NPROCESSORS_ONLN);
	int nt = nc < 1 ? 1 : nc > 8 ? 8 : (int)nc;
	pthread_t th[8];
	pthread_attr_t at;
	pthread_attr_init(&at);
	pthread_attr_setstacksize(&at, 1 << 20);
	int started = 0;
	for (int i = 0; i < nt; i++)
		if (!pthread_create(&th[started], &at, worker, &j)) started++;
	if (!started) worker(&j);
	for (int i = 0; i < started; i++) pthread_join(th[i], 0);
	pthread_mutex_destroy(&j.mu);
	if (which) *which = j.which;
	return j.best;
}
