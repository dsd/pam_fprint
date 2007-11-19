/*
 * pam_fprint: PAM module for fingerprint authentication through libfprint
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#include <fprint.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>

static int send_info_msg(pam_handle_t *pamh, char *msg)
{
	const struct pam_message mymsg = {
		.msg_style = PAM_TEXT_INFO,
		.msg = msg,
	};
	const struct pam_message *msgp = &mymsg;
    const struct pam_conv *pc;
	struct pam_response *resp;
	int r;

    r = pam_get_item(pamh, PAM_CONV, (const void **) &pc);
	if (r != PAM_SUCCESS)
		return;

	if (!pc || !pc->conv)
		return;

	return pc->conv(1, &msgp, &resp, pc->appdata_ptr);
}

static int send_err_msg(pam_handle_t *pamh, char *msg)
{
	const struct pam_message mymsg = {
		.msg_style = PAM_ERROR_MSG,
		.msg = msg,
	};
	const struct pam_message *msgp = &mymsg;
    const struct pam_conv *pc;
	struct pam_response *resp;
	int r;

    r = pam_get_item(pamh, PAM_CONV, (const void **) &pc);
	if (r != PAM_SUCCESS)
		return;

	if (!pc || !pc->conv)
		return;

	return pc->conv(1, &msgp, &resp, pc->appdata_ptr);
}

static const char *fingerstr(enum fp_finger finger)
{
	const char *names[] = {
		[LEFT_THUMB] = "left thumb",
		[LEFT_INDEX] = "left index",
		[LEFT_MIDDLE] = "left middle",
		[LEFT_RING] = "left ring",
		[LEFT_LITTLE] = "left little",
		[RIGHT_THUMB] = "right thumb",
		[RIGHT_INDEX] = "right index",
		[RIGHT_MIDDLE] = "right middle",
		[RIGHT_RING] = "right ring",
		[RIGHT_LITTLE] = "right little",
	};
	if (finger < LEFT_THUMB || finger > RIGHT_LITTLE)
		return "UNKNOWN";
	return names[finger];
}

static int find_dev_and_print(struct fp_dscv_dev **ddevs,
	struct fp_dscv_print **prints, struct fp_dscv_dev **_ddev,
	struct fp_dscv_print **_print)
{
	int i = 0;
	struct fp_dscv_print *print;
	struct fp_dscv_dev *ddev;

	while (print = prints[i++]) {
		ddev = fp_dscv_dev_for_dscv_print(ddevs, print);
		if (ddev) {
			*_ddev = ddev;
			*_print = print;
			return 0;
		}
	}
	return 1;
}

static int do_verify(pam_handle_t *pamh, struct fp_dev *dev,
	struct fp_print_data *data, enum fp_finger finger)
{
	int max_tries = 5;
	const char *driver_name = fp_driver_get_full_name(fp_dev_get_driver(dev));
	const char *fstr = fingerstr(finger);

	do {
		int r;
		char msg[128];

		snprintf(msg, sizeof(msg), "Scan %s finger on %s", fstr, driver_name);
		msg[sizeof(msg) - 1] = 0;
		send_info_msg(pamh, msg);

		r = fp_verify_finger(dev, data);
		if (r < 0) {
			snprintf(msg, sizeof(msg), "Fingerprint verification error %d", r);
			msg[sizeof(msg) - 1] = 0;
			send_err_msg(pamh, msg);
			return PAM_AUTHINFO_UNAVAIL;
		}
		switch (r) {
		case FP_VERIFY_NO_MATCH:
			return PAM_AUTH_ERR;
		case FP_VERIFY_MATCH:
			return PAM_SUCCESS;
		case FP_VERIFY_RETRY:
			send_info_msg(pamh, "Scan didn't quite work. Please try again.");
			break;
		case FP_VERIFY_RETRY_TOO_SHORT:
			send_info_msg(pamh, "Swipe was too short, please try again.");
			break;
		case FP_VERIFY_RETRY_CENTER_FINGER:
			send_info_msg(pamh, "Please center your finger on the sensor and "
				"try again.");
			break;
		case FP_VERIFY_RETRY_REMOVE_FINGER:
			send_info_msg(pamh, "Please remove finger from the sensor and try "
				"again.");
			break;
		}
	} while (max_tries--);

	send_err_msg(pamh, "Too many failed scans, giving up.");
	return PAM_AUTHINFO_UNAVAIL;
}

static int do_auth(pam_handle_t *pamh)
{
	int r;
	struct fp_dscv_dev **ddevs;
	struct fp_dscv_print **prints;
	struct fp_dscv_dev *ddev;
	struct fp_dscv_print *print;
	struct fp_dev *dev;
	struct fp_print_data *data;
	enum fp_finger finger;

	r = fp_init();
	if (r < 0)
		return PAM_AUTHINFO_UNAVAIL;

	ddevs = fp_discover_devs();
	if (!ddevs)
		return PAM_AUTHINFO_UNAVAIL;

	prints = fp_discover_prints();
	if (!prints) {
		fp_dscv_devs_free(ddevs);
		return PAM_AUTHINFO_UNAVAIL;
	}

	r = find_dev_and_print(ddevs, prints, &ddev, &print);
	if (r) {
		fp_dscv_prints_free(prints);
		fp_dscv_devs_free(ddevs);
		send_info_msg(pamh, "Could not locate any suitable fingerprints "
			"matched with available hardware.");
		return PAM_AUTHINFO_UNAVAIL;
	}

	dev = fp_dev_open(ddev);
	fp_dscv_devs_free(ddevs);
	if (!dev) {
		fp_dscv_prints_free(prints);
		return PAM_AUTHINFO_UNAVAIL;
	}

	finger = fp_dscv_print_get_finger(print);

	r = fp_print_data_from_dscv_print(print, &data);
	fp_dscv_prints_free(prints);
	if (r) {
		fp_dev_close(dev);
		return PAM_AUTHINFO_UNAVAIL;
	}

	r = do_verify(pamh, dev, data, finger);
	fp_print_data_free(data);
	fp_dev_close(dev);
	return r;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
	const char **argv)
{
	const char *rhost = NULL;
	FILE *fd;
	char buf[5];
	const char *username;
	char *homedir;
	struct passwd *passwd;
	int r;

	pam_get_item(pamh, PAM_RHOST, (const void **)(const void*) &rhost);
	if (rhost != NULL && strlen(rhost) > 0) {
		/* remote login (e.g. over SSH) */
		return PAM_AUTHINFO_UNAVAIL;
	}

	r = pam_get_user(pamh, &username, NULL);
	if (r != PAM_SUCCESS)
		return PAM_AUTHINFO_UNAVAIL;

	passwd = getpwnam(username);
	if (!passwd)
		return PAM_AUTHINFO_UNAVAIL;

	homedir = strdup(passwd->pw_dir);

	/* a bit of a hack to make libfprint use the right home dir */
	r = setenv("HOME", homedir, 1);
	if (r < 0) {
		free(homedir);
		return PAM_AUTHINFO_UNAVAIL;
	}

	r = do_auth(pamh);
	free(homedir);
	return r;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
	const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc,
	const char **argv)
{
	return PAM_SUCCESS;
}

