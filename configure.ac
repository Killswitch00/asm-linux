AC_INIT([Arma Server Monitor], [0.6],[killswitch@zeus-community.net],
	[armaservermonitor], [https://github.com/Killswitch00/asm-linux])
AC_PREREQ([2.59])

AC_CONFIG_SRCDIR([src/asm.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign -Wall -Werror])

# Checks for programs
m4_pattern_allow([AM_PROG_AR])
m4_ifdef([AM_PROG_AR],[AM_PROG_AR],)
LT_INIT([dlopen disable-static])
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for required libraries
PKG_CHECK_MODULES([GLIB],[glib-2.0])

# The 64-bit Arma 3 extension must be named asmdll_x64.so
AC_CANONICAL_HOST
AS_CASE([$host_cpu],
    [i?86], [ASMDLL_NAME=asmdll],
    [x86_64], [ASMDLL_NAME=asmdll_x64],
    [AC_MSG_ERROR("Unsupported CPU type $host_cpu")]
)
AC_SUBST(ASMDLL_NAME)

# Finalize
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
