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
#include <stdlib.h>
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
		return PAM_CONV_ERR;

	if (!pc || !pc->conv)
		return PAM_CONV_ERR;

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
		return PAM_CONV_ERR;

	if (!pc || !pc->conv)
		return PAM_CONV_ERR;

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


static struct fp_print_data **find_dev_and_prints(struct fp_dscv_dev **ddevs,
	struct fp_dscv_print **prints, struct fp_dscv_dev **_ddev, enum fp_finger **fingers)
{
	int i = 0, j = 0, err;
	struct fp_dscv_print *print;
	struct fp_dscv_dev *ddev = NULL;
	uint16_t driver_id, driver_id_cur;
	size_t prints_count = 0;
	struct fp_print_data **gallery;

	/* TODO: add device selection */
	while (print = prints[i++]) {
		if (!ddev) {
			ddev = fp_dscv_dev_for_dscv_print(ddevs, print);
			driver_id = fp_dscv_print_get_driver_id(print);
			*_ddev = ddev;
		}
		if (ddev)
		{
		    driver_id_cur = fp_dscv_print_get_driver_id(print);
		    if (driver_id_cur == driver_id) {
			    prints_count++;
		    }
		}
	}
	
	if (prints_count == 0) {
	    return NULL;
	}
	
	gallery = malloc(sizeof(*gallery) * (prints_count + 1));
	if (gallery == NULL) {
	    return NULL;
	}
	gallery[prints_count] = NULL;
	*fingers = malloc(sizeof(*fingers) * (prints_count));
	if (*fingers == NULL) {
	    free(gallery);
	    return NULL;
	}
	
	i = 0, j = 0;
	while (print = prints[i++]) {
		driver_id_cur = fp_dscv_print_get_driver_id(print);
		if (driver_id_cur == driver_id) {
			err = fp_print_data_from_dscv_print(print, & (gallery[j]));
			if (err != 0) {
			    gallery[j] = NULL;
			    break;
			}
			(*fingers)[j] = fp_dscv_print_get_finger(print);
			j++;
		}
	}
	
	return gallery;
}

static int do_identify(pam_handle_t *pamh, struct fp_dev *dev,
	struct fp_print_data **gallery, enum fp_finger *fingers)
{
	int max_tries = 5;
	size_t offset;
	const char *driver_name = fp_driver_get_full_name(fp_dev_get_driver(dev));
	const char *fstr = fingerstr(fingers[0]);
	
	do {
		int r;
		char msg[128];

		
		if (fp_dev_supports_identification(dev)) {
		    snprintf(msg, sizeof(msg), "Scan finger on %s", driver_name);
		    msg[sizeof(msg) - 1] = 0;
		    send_info_msg(pamh, msg);
		    r = fp_identify_finger(dev, gallery, &offset);
		    
		}
		else {
		    snprintf(msg, sizeof(msg), "Scan %s finger on %s", fstr, driver_name);
		    msg[sizeof(msg) - 1] = 0;
		    send_info_msg(pamh, msg);
		    r = fp_verify_finger(dev, gallery[0]);
		}
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
	struct fp_print_data **gallery, **gallery_iter;
	enum fp_finger *fingers;

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
	
	gallery = find_dev_and_prints(ddevs, prints, &ddev, &fingers);
	if (!gallery) {
		fp_dscv_prints_free(prints);
		fp_dscv_devs_free(ddevs);
		send_info_msg(pamh, "Could not locate any suitable fingerprints "
			"matched with available hardware.");
		return PAM_AUTHINFO_UNAVAIL;
	}

	dev = fp_dev_open(ddev);
	fp_dscv_devs_free(ddevs);
	fp_dscv_prints_free(prints);
	if (!dev) {
		gallery_iter = gallery;
		while (*gallery_iter) {
		    fp_print_data_free(*gallery_iter);
		    gallery_iter++;
		}
		free(gallery);
		free(fingers);	
		return PAM_AUTHINFO_UNAVAIL;
	}

	r = do_identify(pamh, dev, gallery, fingers);
	
	gallery_iter = gallery;
	while (*gallery_iter)
	{
	    fp_print_data_free(*gallery_iter);
	    gallery_iter++;
	}
	free(gallery);
	free(fingers);
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

