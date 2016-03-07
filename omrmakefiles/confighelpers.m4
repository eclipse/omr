###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
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
