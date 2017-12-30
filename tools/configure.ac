###############################################################################
# Copyright (c) 2015, 2016 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

# Configures the build environment for build tools.
# For cross-compiled packages, the build tools execute on the build
# system, not the host/target system.

# This configure.ac has been confirmed to work with Autoconf v2.63,
# and to fail with Autoconf v2.59. Autoconf v2.59 does not recognize
# AS_CASE(), AS_ECHO(), AC_MSG_ERROR().
AC_PREREQ(2.63)
AC_INIT([OMR], [1.0])
m4_include([../omrmakefiles/confighelpers.m4])

AC_CANONICAL_BUILD

# Categorize the Build OS
AC_MSG_CHECKING([OMR_BUILD_OS])
OMRCFG_CATEGORIZE_OS([OMR_BUILD_OS], [build_os])
AC_MSG_RESULT([$OMR_BUILD_OS])

# Categorize the Build architecture
AC_MSG_CHECKING([OMR_BUILD_ARCH])
OMRCFG_CATEGORIZE_ARCH([OMR_BUILD_ARCH], [build_cpu])
AC_MSG_RESULT([$OMR_BUILD_ARCH])

AC_PROG_CC()
AC_PROG_CXX()

AC_CHECK_SIZEOF([void *])
AC_MSG_CHECKING([OMR_BUILD_DATASIZE])
AS_IF([test "$ac_cv_sizeof_void_p" = 8],
	[OMR_BUILD_DATASIZE=64],
	[OMR_BUILD_DATASIZE=32])
AC_MSG_RESULT([$OMR_BUILD_DATASIZE])

AC_MSG_CHECKING([OMR_ENV_DATA64])
AS_IF([test "$OMR_BUILD_DATASIZE" = 64],
	  [OMR_ENV_DATA64=1],
	  [OMR_ENV_DATA64=0])
AC_MSG_RESULT([$OMR_ENV_DATA64])

AC_MSG_CHECKING([OMR_BUILD_TOOLCHAIN])
OMRCFG_CATEGORIZE_TOOLCHAIN([OMR_BUILD_TOOLCHAIN])
AC_MSG_RESULT([$OMR_BUILD_TOOLCHAIN])

###
### Variable Substitution
###

AC_SUBST(OMR_ENV_DATA64)
AC_SUBST(OMR_BUILD_OS)
AC_SUBST(OMR_BUILD_ARCH)
AC_SUBST(OMR_BUILD_TOOLCHAIN)
AC_SUBST(OMR_BUILD_DATASIZE)
AC_SUBST(OMR_BUILD_CC,[$CC])
AC_SUBST(OMR_BUILD_CXX,[$CXX])

AC_CONFIG_FILES([toolconfigure.mk])
AC_CONFIG_HEADERS([toolconfig.h])

AC_OUTPUT
