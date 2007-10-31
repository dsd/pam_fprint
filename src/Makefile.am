noinst_PROGRAMS = pamtest
pammod_PROGRAMS = pam_fprint.so
pammoddir=/lib/security

pam_fprint_so_SOURCES = pam_fprint.c
pam_fprint_so_CFLAGS = -fPIC $(FPRINT_CFLAGS)
pam_fprint_so_LDFLAGS = -shared
pam_fprint_so_LDADD = $(PAM_LIBS) $(FPRINT_LIBS)

pamtest_SOURCES = pamtest.c
pamtest_LDADD = $(PAM_LIBS) -lpam_misc

