AC_INIT([pam_fprint], [0.2])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/pam_fprint.c])

AC_PREREQ([2.50])
AC_PROG_CC
AM_PROG_CC_C_O

AC_CHECK_HEADER([security/pam_modules.h], ,
	[AC_MSG_ERROR([PAM headers missing])])
AC_CHECK_LIB(pam, pam_start, [PAM_LIBS="-lpam"],
	AC_MSG_ERROR([PAM library missing]))
AC_SUBST(PAM_LIBS)

PKG_CHECK_MODULES(FPRINT, "libfprint")
AC_SUBST(FPRINT_LIBS)
AC_SUBST(FPRINT_CFLAGS)

AC_CONFIG_FILES([Makefile] [src/Makefile])
AC_OUTPUT
