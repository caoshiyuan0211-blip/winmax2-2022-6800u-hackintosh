#pragma once
enum { FPF_OK = 0, FPF_NOMATCH, FPF_TIMEOUT, FPF_NOTPL, FPF_NODEV };
typedef void (*fp_msg_fn)(void *ctx, const char *text);
int fp_auth(const char *user, int timeout_ms, int maxTries, fp_msg_fn msg, void *ctx, float *scoreOut);
