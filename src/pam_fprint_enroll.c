/*
 * pam_fprint_enroll: Enrolls finger and saves the result to disk
 * Copyright (C) 2007 Vasily Khoruzhick <anarsoul@gmail.com>
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 *
 * Based on code from libfprint example 
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
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <libfprint/fprint.h>

static const char *finger_names[] = {
	[LEFT_THUMB] = "Left Thumb",
	[LEFT_INDEX] = "Left Index Finger",
	[LEFT_MIDDLE] = "Left Middle Finger",
	[LEFT_RING] = "Left Ring Finger",
	[LEFT_LITTLE] = "Left Little Finger",
	[RIGHT_THUMB] = "Right Thumb",
	[RIGHT_INDEX] = "Right Index Finger",
	[RIGHT_MIDDLE] = "Right Middle Finger",
	[RIGHT_RING] = "Right Ring Finger",
	[RIGHT_LITTLE] = "Right Little Finger",
};


static struct fp_dscv_dev *discover_device(struct fp_dscv_dev **discovered_devs)
{
	struct fp_dscv_dev *ddev = NULL;
	int i;

	for (i = 0; ddev = discovered_devs[i]; i++) {
		struct fp_driver *drv = fp_dscv_dev_get_driver(ddev);
		printf("Found device claimed by %s driver\n",
			fp_driver_get_full_name(drv));
		return ddev;
	}

	return ddev;
}

static struct fp_print_data *enroll(struct fp_dev *dev, enum fp_finger finger)
{
	struct fp_print_data *enrolled_print = NULL;
	int r;	

	printf("You will need to successfully scan your %s %d times to "
		"complete the process.\n", finger_names[finger], fp_dev_get_nr_enroll_stages(dev));

	do {
		sleep(1);
		printf("\nScan your finger now.\n");

		r = fp_enroll_finger_img(dev, &enrolled_print, NULL);
		
		if (r < 0) {
			printf("Enroll failed with error %d\n", r);
			return NULL;
		}

		switch (r) {
		case FP_ENROLL_COMPLETE:
			printf("Enroll complete!\n");
			break;
		case FP_ENROLL_FAIL:
			printf("Enroll failed, something wen't wrong :(\n");
			return NULL;
		case FP_ENROLL_PASS:
			printf("Enroll stage passed. Yay!\n");
			break;
		case FP_ENROLL_RETRY:
			printf("Didn't quite catch that. Please try again.\n");
			break;
		case FP_ENROLL_RETRY_TOO_SHORT:
			printf("Your swipe was too short, please try again.\n");
			break;
		case FP_ENROLL_RETRY_CENTER_FINGER:
			printf("Didn't catch that, please center your finger on the "
				"sensor and try again.\n");
			break;
		case FP_ENROLL_RETRY_REMOVE_FINGER:
			printf("Scan failed, please remove your finger and then try "
				"again.\n");
			break;
		}
	} while (r != FP_ENROLL_COMPLETE);

	if (!enrolled_print) {
		fprintf(stderr, "Enroll complete but no print?\n");
		return NULL;
	}

	printf("Enrollment completed!\n\n");
	return enrolled_print;
}

int main(int argc, char *argv[])
{
	int r = 1, i;
	int next_option;
	enum fp_finger finger = RIGHT_INDEX;

	struct fp_dscv_dev *ddev;
	struct fp_dscv_dev **discovered_devs;
	struct fp_dev *dev;
	struct fp_print_data *data;
	
	const char * const short_options = "hf:";
	const struct option long_options[] = {
		{ "help", 0, NULL, 'h'},
		{ "enroll-finger", 1, NULL, 'f'},
		{ NULL, 0, NULL, 0}
	};
	
	do {
		next_option = getopt_long(argc, argv, short_options, long_options,
			NULL);
		switch (next_option) {
		case 'h':
			/* Printing usage */
			printf("Usage: %s options\n", argv[0]);
			printf("	-h	--help			Display this usage information.\n"
			   "	-f	--enroll-finger index	Enroll finger with index.\n\n");
			printf("	Valid indexes are:\n");
			for (i = LEFT_THUMB; i <= RIGHT_LITTLE; i++) {
				printf("	%d - %s\n", i, finger_names[i]);
			}
			exit(1);
			   
			break;
		case 'f':
			sscanf(optarg, "%d", &finger);
			if (finger < LEFT_THUMB || finger > RIGHT_LITTLE) {
				printf("%s: Invalid finger index.\n", argv[0]);
				printf("%s: Valid indexes are:\n", argv[0]);
				for (i = LEFT_THUMB; i <= RIGHT_LITTLE; i++) {
					printf("%s: %d - %s\n", argv[0], i, finger_names[i]);
				}
				exit(1);
			}
			break;
		case -1:	/* Done with options. */
			break;
		default:	/* Unexpected option */
			exit(1);
		}
	} while (next_option != -1);

	printf("This program will enroll your finger, "
		"unconditionally overwriting any selected print that was enrolled "
		"previously. If you want to continue, press enter, otherwise hit "
		"Ctrl+C\n");
	getchar();

	r = fp_init();
	if (r < 0) {
		fprintf(stderr, "Failed to initialize libfprint\n");
		exit(1);
	}

	discovered_devs = fp_discover_devs();
	if (!discovered_devs) {
		fprintf(stderr, "Could not discover devices\n");
		goto out;
	}

	ddev = discover_device(discovered_devs);
	if (!ddev) {
		fprintf(stderr, "No devices detected.\n");
		goto out;
	}

	dev = fp_dev_open(ddev);
	fp_dscv_devs_free(discovered_devs);
	if (!dev) {
		fprintf(stderr, "Could not open device.\n");
		goto out;
	}

	printf("Opened device. It's now time to enroll your finger.\n\n");
	data = enroll(dev, finger);
	if (!data)
		goto out_close;

	r = fp_print_data_save(data, finger);
	if (r < 0)
		fprintf(stderr, "Data save failed, code %d\n", r);

	fp_print_data_free(data);
out_close:
	fp_dev_close(dev);
out:
	fp_exit();
	return r;
}
