// pam_fpauth: fingerprint (CS9711) PAM auth module for macOS, used as
//   auth sufficient /usr/local/lib/pam/pam_fpauth.so
// in /etc/pam.d/sudo_local. Success = fingerprint matched; anything else = PAM_IGNORE
// (falls through to the password). Options: timeout=<s> (default 10), tries=<n> (default 3).
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include "fpflow.h"

static void say(void *ctx, const char *text)
{
	pam_handle_t *ph = ctx;
	const struct pam_conv *conv = 0;
	if (pam_get_item(ph, PAM_CONV, (const void **)&conv) != PAM_SUCCESS || !conv || !conv->conv) return;
	struct pam_message m = { PAM_TEXT_INFO, (char *)text };
	const struct pam_message *mp = &m;
	struct pam_response *r = 0;
	conv->conv(1, &mp, &r, conv->appdata_ptr);
	if (r) { free(r->resp); free(r); }
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *ph, int flags, int argc, const char **argv)
{
	(void)flags;
	int timeout = 10, tries = 3;
	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "timeout=", 8)) timeout = atoi(argv[i] + 8);
		else if (!strncmp(argv[i], "tries=", 6)) tries = atoi(argv[i] + 6);
	}
	const char *user = 0;
	if (pam_get_user(ph, &user, 0) != PAM_SUCCESS || !user || !*user) return PAM_IGNORE;
	if (geteuid() != 0) return PAM_IGNORE;  // template store is root-only
	int rc = fp_auth(user, timeout * 1000, tries, say, ph, 0);
	return rc == FPF_OK ? PAM_SUCCESS : PAM_IGNORE;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *ph, int flags, int argc, const char **argv)
{
	(void)ph; (void)flags; (void)argc; (void)argv;
	return PAM_SUCCESS;
}
