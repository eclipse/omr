###############################################################################
# Copyright (c) 2015, 2019 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

############### Categorize the OS
# OMRCFG_CATEGORIZE_OS([<omr os variable>], [<canonical os>])
AC_DEFUN([OMRCFG_CATEGORIZE_OS],
	[AS_CASE([$$2],
		[*aix*],[$1=aix],
		[*os400*],[$1=aix],
		[*linux*],[$1=linux],
		[*darwin*],[$1=osx],
		[*cygwin*],[$1=win],
		[*mingw*],[$1=win],
		[*mks*],[$1=win],
		[*msys*],[$1=win],
		[*mvs*],[$1=zos],
		[*openedition*],[$1=zos],
		[*zvmoe*],[$1=zos],
		[AC_MSG_ERROR([Unable to derive $1 from $2: $$2])]
	)]
)

############### Categorize the Architecture
# OMRCFG_CATEGORIZE_ARCH([<omr arch variable>], [<canonical cpu>])
AC_DEFUN([OMRCFG_CATEGORIZE_ARCH],
	[AS_CASE([$$2],
		[*arm*],[$1=arm],
		[aarch64],[$1=aarch64],
		[powerpc*],[$1=ppc],
		[i370],[$1=s390],
		[s390],[$1=s390],
		[s390x],[$1=s390],
		[i386],[$1=x86],
		[i486],[$1=x86],
		[i586],[$1=x86],
		[i686],[$1=x86],
		[ia64],[$1=x86],
		[x86_64],[$1=x86],
		[riscv64],[$1=riscv],
		[AC_MSG_ERROR([Unable to derive $1 from $2: $$2])]
	)]
)

############### Categorize the Toolchain
# You must detect the CC compiler using AC_PROG_CC() before running this.
#
# OMRCFG_CATEGORIZE_TOOLCHAIN([<omr toolchain variable>]])
AC_DEFUN([OMRCFG_CATEGORIZE_TOOLCHAIN],
	[AC_LANG([C])
	AS_IF([test "x$$1" = "x"],
		AS_IF([test "$GCC" = "yes"], [$1=gcc]))
	AS_IF([test "x$OMR_BUILD_TOOLCHAIN" = "x"],
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifndef _MSC_VER
#error not an msvc compiler
#endif]])], [$1=msvc]))
	AS_IF([test "x$$1" = "x"],
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if !(defined(__xlC__) || (defined(__MVS__) && defined(__COMPILER_VER__)))
#error not an xlc compiler
#endif]])], [$1=xlc]))
	AS_IF([test "x$$1" = "x"],
		AC_MSG_ERROR([Unrecognized toolchain])
	)]
)
