noinst_PROGRAMS = pamtest
bin_PROGRAMS = pam_fprint_enroll
pammod_PROGRAMS = pam_fprint.so
pammoddir=/lib/security

pam_fprint_so_SOURCES = pam_fprint.c
pam_fprint_so_CFLAGS = -fPIC $(FPRINT_CFLAGS)
pam_fprint_so_LDFLAGS = -shared
pam_fprint_so_LDADD = $(PAM_LIBS) $(FPRINT_LIBS)

pamtest_SOURCES = pamtest.c
pamtest_LDADD = $(PAM_LIBS) -lpam_misc

pam_fprint_enroll_SOURCES = pam_fprint_enroll.c
pam_fprint_enroll_LDADD = $(FPRINT_LIBS)
pam_fprint_enroll_CFLAGS = $(FPRINT_CFLAGS)
