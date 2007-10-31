/***
  This file is part of pam_dotfile.

  pam_dotfile is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  pam_dotfile is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with pam_dotfile; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
***/

#include <stdio.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

int main(int argc, char*argv[]) {
    static struct pam_conv pc = { misc_conv, NULL };
    pam_handle_t *ph = NULL;
    int r, ret;
    char *username, *procname, *service;

    if ((procname = strchr(argv[0], '/')))
        procname++;
    else
        procname = argv[0];

    if (argc <= 1 || argc > 3) {
        fprintf(stderr, "Usage: %s [<service>] [<username>]\n", procname);
        exit(1);
    }

    service = (argc >= 2) ? argv[1] : procname;
    username = (argc == 3) ? argv[2] : NULL;

    if (username)
        printf("Trying to authenticate <%s> for service <%s>.\n", username, service);
    else
        printf("Trying to authenticate for service <%s>.\n", service);
    
    if ((r = pam_start(service, username, &pc, &ph)) != PAM_SUCCESS) {
        fprintf(stderr, "Failure starting pam: %s\n", pam_strerror(ph, r));
        return 1;
    }

    if ((r = pam_authenticate(ph, 0)) != PAM_SUCCESS) {
        fprintf(stderr, "Failed to authenticate: %s\n", pam_strerror(ph, r));
        ret = 1;
    } else {
        printf("Authentication successful.\n");
        ret = 0;
    }

    if ((r = pam_end(ph, r)) != PAM_SUCCESS)
        fprintf(stderr, "Failure shutting down pam: %s\n", pam_strerror(ph, r));

    return ret;
}
