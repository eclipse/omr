###############################################################################
# Copyright (c) 2015, 2020 IBM Corp. and others
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

# This configure.ac has been confirmed to work with Autoconf v2.63,
# and to fail with Autoconf v2.59. Autoconf v2.59 does not recognize
# AS_CASE(), AS_ECHO(), AC_MSG_ERROR().
AC_PREREQ(2.63)
AC_INIT([OMR], [0.1])

m4_include([omrmakefiles/confighelpers.m4])

AC_CANONICAL_HOST

###
### Options
###

AC_ARG_VAR([SPEC], [The SPEC to use. Sets up many configure settings without checks.])

AC_ARG_VAR([OMRGLUE], [Path to the directory that contains runtime-specific OMR compatibility code.])
AC_ARG_VAR([OMRGLUE_CPPFLAGS], [Extra CPPFLAGS to be passed to the C preprocessor when compiling OMRGLUE code.])
AC_ARG_VAR([OMRGLUE_CFLAGS], [Extra CFLAGS to be passed to the C compiler when compiling OMRGLUE.])
AC_ARG_VAR([OMRGLUE_CXXFLAGS], [Extra CXXFLAGS to be passed to the C++ compiler when compiling OMRGLUE.])
AC_ARG_VAR([OMRGLUE_INCLUDES], [Extra include directories to be passed to the C preprocessor compiler when compiling OMRGLUE.])

AC_ARG_VAR([lib_output_dir], [The output directory for libraries. (Default: $top_srcdir/lib)])
AC_ARG_VAR([exe_output_dir], [The output directory for executables. (Default: $top_srcdir)])
AC_ARG_VAR([OMR_HOST_OS], [The operating system where the package will run. One of: aix,linux,osx,win,zos])
AC_ARG_VAR([OMR_HOST_ARCH], [The architecture of the CPU where the package will run. One of: aarch64,arm,ppc,riscv,s390,x86])
AC_ARG_VAR([OMR_TARGET_DATASIZE], [Specifies whether the package will run in 32- or 64-bit mode. One of: 31,32,64])
AC_ARG_VAR([OMR_TOOLCHAIN], [The toolchain used to build the package. One of: gcc,xlc,msvc])
AC_ARG_VAR([OMR_TOOLS_CC], [The C compiler to use for building tools (can differ from CC).])
AC_ARG_VAR([OMR_TOOLS_CXX], [The C++ compiler to use for building tools (can differ from CXX).])
AC_ARG_VAR([OMR_CROSS_CONFIGURE],
	[Indicates that configure is not being run on a system that is capable of building the package.
	Compiler and feature detection tests will be invalid. All options must be configured manually.
	One of: yes,no. (Default: no)])

AC_ARG_VAR([OMR_COMPANY_NAME],
  [The company name of product integrating OMR.] (Default: ))
AC_ARG_VAR([OMR_COMPANY_COPYRIGHT],
  [The copyright of product integrating OMR.] (Default: ))
AC_ARG_VAR([OMR_PRODUCT_DESCRIPTION],
  [The description of product integrating OMR.] (Default: AC_PACKAGE_STRING))
AC_ARG_VAR([OMR_PRODUCT_VERSION],
  [The version of product integrating OMR.] (Default: AC_PACKAGE_VERSION))
AC_ARG_VAR([OMR_PRODUCT_DESCRIPTION],
  [The description of product integrating OMR.] (Default: AC_PACKAGE_STRING AC_PACKAGE_URL))

OMR_COMPANY_NAME=${OMR_COMPANY_NAME:-""}
OMR_COMPANY_COPYRIGHT=${OMR_COMPANY_COPYRIGHT:-""}
OMR_PRODUCT_DESCRIPTION=${OMR_PRODUCT_DESCRIPTION:-"AC_PACKAGE_STRING AC_PACKAGE_URL"}
OMR_PRODUCT_NAME=${OMR_PRODUCT_NAME:-"AC_PACKAGE_NAME"}
OMR_PRODUCT_VERSION=${OMR_PRODUCT_VERSION:-"AC_PACKAGE_VERSION"}

AC_ARG_ENABLE([optimized],
	AS_HELP_STRING([--enable-optimized], [Create an optimized build. (Default: yes)]),
	[],
	[enable_optimized=yes]
)
AS_IF([test "$enable_optimized" != "no"],
	[OMR_OPTIMIZE=1],
	[OMR_OPTIMIZE=0])

AC_ARG_ENABLE([warnings-as-errors],
	AS_HELP_STRING([--enable-warnings-as-errors], [Stop compilation when there is a warning. (Default: yes)]),
	[],
	[enable_warnings_as_errors=yes]
)
AS_IF([test "$enable_warnings_as_errors" != "no"],
	[OMR_WARNINGS_AS_ERRORS=1],
	[OMR_WARNINGS_AS_ERRORS=0])

AC_ARG_ENABLE([enhanced-warnings],
	AS_HELP_STRING([--enable-enhanced-warnings], [Enhanced compiler warnings. (Default: yes)]),
	[],
	[enable_enhanced_warnings=yes])
AS_IF([test "$enable_enhanced_warnings" != "no"],
	[OMR_ENHANCED_WARNINGS=1],
	[OMR_ENHANCED_WARNINGS=0])

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug], [Generate debug information during compilation. (Default: yes)]),
	[],
	[enable_debug=yes])
AS_IF([test "$enable_debug" != "no"],
	[OMR_DEBUG=1],
	[OMR_DEBUG=0])
AC_ARG_ENABLE([DDR],
	AS_HELP_STRING([[--enable-DDR]], [Enable DDR support. Requires --enable-debug. (Default: no)]),
	[],
	[enable_DDR=no])
AS_IF([test $enable_DDR = "yes" && test $enable_debug != "yes"],
	AC_MSG_ERROR([--enable-DDR requires --enable-debug.]))

AC_ARG_ENABLE([fvtest],
	AS_HELP_STRING([[--enable-fvtest]], [Build fvtests. (Default: no)]),
	[],
	[enable_fvtest=no]
)
AC_ARG_ENABLE([fvtest-agent],
	AS_HELP_STRING([[--enable-fvtest-agent]],
		[Build fvtest agents even if fvtests are not enabled.
		This setting is ignored if --enable-fvtest=yes because if fvtests are
		being built, fvtest agents must also be built. (Default: no)]),
	[],
	[enable_fvtest_agent=no]
)
# If fvtests are being built, fvtest agents must also be built.
AS_IF([test "$enable_fvtest" = "yes"], [enable_fvtest_agent=yes])

AC_ARG_ENABLE([tracegen],
	AS_HELP_STRING([[--enable-tracegen]], [Run tracegen and tracemerge. (Default: yes)]),
	[],
	[enable_tracegen=yes]
)

AC_ARG_ENABLE([native-encoding],
	AS_HELP_STRING([[--enable-native-encoding]],
		[Allow use of native character encoding where possible (currently only defined for z/OS). (Default: no)]),
)
AS_IF([test "x$enable_native_encoding" = "xyes"],
	[AC_SUBST([OMR_ALLOW_NATIVE_ENCODING], [1])],
	[AC_SUBST([OMR_ALLOW_NATIVE_ENCODING], [0])]
)

###
### Build environment configuration
###

# Cache the requested CC
BUILD_CC=$CC

# Prevent AC_PROG_CC() from defaulting CFLAGS to -g for gcc.
: ${CFLAGS=""}

# Prevent AC_PROG_CXX() from defaulting CXXFLAGS to -g -O2 for g++.
: ${CXXFLAGS=""}

AS_IF([test "x$OMR_CROSS_CONFIGURE" != "xyes"],
	[
		AC_PROG_CC()
		AC_PROG_CXX()
		AC_PROG_CPP()
	]
)

# If OMR_HOST_OS is set, use it.
# Otherwise, calculate it from the canonical host.
AC_MSG_CHECKING([OMR_HOST_OS])
AS_IF([test "x$OMR_HOST_OS" = "x"],
	[OMRCFG_CATEGORIZE_OS([OMR_HOST_OS], [host_os])]
)
AC_MSG_RESULT([$OMR_HOST_OS])

# If OMR_HOST_ARCH is set, use it.
# Otherwise, calculate it from the canonical host.
AC_MSG_CHECKING([OMR_HOST_ARCH])
AS_IF([test "x$OMR_HOST_ARCH" = "x"],
	[OMRCFG_CATEGORIZE_ARCH([OMR_HOST_ARCH], [host_cpu])]
)
AC_MSG_RESULT([$OMR_HOST_ARCH])

AS_IF([test "$cross_compiling" = "yes"],
	[OMR_CROSS_COMPILE=1],
	[OMR_CROSS_COMPILE=0])

# If not cross-configuring and OMR_TARGET_DATASIZE is not set,
# detect it with a compiler test and validate it, otherwise
# just validate it.
AC_MSG_CHECKING([OMR_TARGET_DATASIZE])
if test "x$OMR_CROSS_CONFIGURE" != "xyes"
then
	AS_IF([test "x$OMR_TARGET_DATASIZE" = "x"],
		[
		AC_CHECK_SIZEOF([void *])
		AS_IF([test "$ac_cv_sizeof_void_p" = 8],
			[OMR_TARGET_DATASIZE=64],
			[OMR_TARGET_DATASIZE=32])
		]
	)
fi
AS_CASE([$OMR_TARGET_DATASIZE],
	[31],[],
	[32],[],
	[64],[],
	[AC_MSG_ERROR([An invalid value was provided for OMR_TARGET_DATASIZE: $OMR_TARGET_DATASIZE])]
)
AC_MSG_RESULT([$OMR_TARGET_DATASIZE])

AC_MSG_CHECKING([OMR_TOOLCHAIN])
if test "x$OMR_CROSS_CONFIGURE" != "xyes"
then
	AS_IF([test "x$OMR_TOOLCHAIN" = "x"],
		[OMRCFG_CATEGORIZE_TOOLCHAIN([OMR_TOOLCHAIN])])
fi
AS_CASE([$OMR_TOOLCHAIN],
	[gcc],[],
	[xlc],[],
	[msvc],[],
	[AC_MSG_ERROR([An invalid value was provided for OMR_TOOLCHAIN: $OMR_TOOLCHAIN])]
)
AC_MSG_RESULT([$OMR_TOOLCHAIN])

# Default OMRGLUE to example/glue if it was not specified
OMRGLUE=${OMRGLUE:-"./example/glue"}

CCLINK=${CCLINK:-"\$(CC)"}
CCLINKSHARED=${CCLINKSHARED:-"\$(CCLINK)"}
CCLINKEXE=${CCLINKEXE:-"\$(CCLINK)"}

CXXLINK=${CXXLINK:-"\$(CXX)"}
CXXLINKSHARED=${CXXLINKSHARED:-"\$(CXXLINK)"}
CXXLINKEXE=${CXXLINKEXE:-"\$(CXXLINK)"}

: ${AR=ar}
: ${AS=as}
: ${OBJCOPY=objcopy}

AS_CASE([$OMR_HOST_OS],
	[win],[
		: ${objext=.$OBJEXT}
		: ${exeext=.$EXEEXT}
		: ${arlibext=.lib}
		: ${solibext=.dll}
		: ${libprefix=''}
	],
	[osx],[
		: ${objext=.$OBJEXT}
		: ${exeext=''}
		: ${arlibext=.a}
		: ${solibext=.dylib}
		: ${libprefix=lib}
	],
	[
		: ${objext=.$OBJEXT}
		: ${exeext=''}
		: ${arlibext=.a}
		: ${solibext=.so}
		: ${libprefix=lib}
	]
)

# Directories
lib_output_dir=${lib_output_dir:-"\$(top_srcdir)/lib"}
exe_output_dir=${exe_output_dir:-"\$(top_srcdir)"}

###
### Build flags
###

############### Enable/Disable auto-build-flag
# auto-build-flag command line argument controls the setting of build flags
# that are not explicitly enabled or disabled.
#
# When auto-build-flag is enabled, build flags that are not explicitly enabled
# or disabled will be set according to the outcome of the <default action>
# passed to OMRCFG_DEFINE_FLAG().  The <default action> can attempt to detect
# the proper value of the build flag using environment tests, compiler tests,
# etc.
#
# When auto-build-flag is disabled, build flags are always disabled unless they
# are explicitly enabled.
#
# By default, automatic setting of build flags is enabled.
#
AC_ARG_ENABLE([auto-build-flag],
	AS_HELP_STRING([--enable-auto-build-flag],
		[Automatically detect the value of build flags that are not explicitly
		enabled or disabled. (Default: yes)]),
	[],
	[enable_auto_build_flag=yes]
)

############### Enable a build flag
# OMRCFG_ENABLE_FLAG(<flag name>)
# #define the flag in omrcfg.inc
# Set the flag to 1 in config.mk
AC_DEFUN([OMRCFG_ENABLE_FLAG],
  [AC_SUBST([$1], [1])
   AC_DEFINE([$1], [1])])

############### Disable a build flag
# OMRCFG_DISABLE_FLAG(<flag name>)
# The flag is undefined in omrcfg.inc
# Set the flag to 0 in config.mk
AC_DEFUN([OMRCFG_DISABLE_FLAG],
  [AC_SUBST([$1], [0])])

############### Define the configurable omrcfg.h build flags
# OMRCFG_DEFINE_FLAG(<flag name>, [<value>], [<default action>], [<help message>])
# where <flag name> is the name of the flag that will be defined in omrcfg.h if
# --enable-<flag name> is passed to configure. If --enable-<flag-name> is not
# passed to configure, <default action> will run if auto-build-flag is
# enabled. $4 is an optional help message.
#
AC_DEFUN([OMRCFG_DEFINE_FLAG],
	[AC_ARG_ENABLE([$1], AS_HELP_STRING([--enable-$1], [$4]),
		[AS_IF([test "x${enableval}" = xyes],
			[OMRCFG_ENABLE_FLAG([$1],[$2])],
			[OMRCFG_DISABLE_FLAG([$1])])],
		[AS_IF([test "x${enable_auto_build_flag}" = xyes],
			[AS_IF([test "$#" -gt 2],
				[
					$3
					:
				],
				[OMRCFG_DISABLE_FLAG([$1])]
			)],
			[OMRCFG_DISABLE_FLAG([$1])]
		)]
	)]
)

############### Define a configurable build flag which defaults to on
# OMRCFG_DEFINE_FLAG_ON(<flag name>, [<value>], [<help message>])
# where <flag name> is the name of the flag that will be undefined in omrcfg.h if
# --disable-<flag name> is passed to configure. If --disable-<flag-name> is not
# passed to configure, then the flag is enabled.
# $3 is an optional help message.
AC_DEFUN([OMRCFG_DEFINE_FLAG_ON],
	[AC_ARG_ENABLE([$1], [AS_HELP_STRING([--disable-$1], [$3])],
		[AS_IF([test "x${enableval}" = xyes],
			[OMRCFG_ENABLE_FLAG([$1],[$2])],
			[OMRCFG_DISABLE_FLAG([$1])]
		)],
		[OMRCFG_ENABLE_FLAG([$1])]
	)]
)

############### Define a configurable build flag which defaults to off
# OMRCFG_DEFINE_FLAG_OFF(<flag name>, [<value>], [<help message>])
# where <flag name> is the name of the flag that will be defined in omrcfg.h if
# --enable-<flag name> is passed to configure. If --enable-<flag-name> is not
# passed to configure,the the flag is disabled.
# $3 is an optional help message.
#
AC_DEFUN([OMRCFG_DEFINE_FLAG_OFF],
	[AC_ARG_ENABLE([$1],
		AS_HELP_STRING([--enable-$1], [$3]),
		[AS_IF([test "x${enableval}" = xyes],
			[OMRCFG_ENABLE_FLAG([$1],[$2])],
			[OMRCFG_DISABLE_FLAG([$1])]
		)],
		[OMRCFG_DISABLE_FLAG([$1])]
	)]
)

############### Top-Level Components
OMRCFG_DEFINE_FLAG_ON([OMR_GC], [], [Enable building the OMR GC. (Default: yes)])
OMRCFG_DEFINE_FLAG_OFF([OMR_COMPILER], [], [Enable building the OMR Compiler. (Default: no)])
OMRCFG_DEFINE_FLAG_OFF([OMR_JITBUILDER], [], [Enable building JitBuilder. (Default: no)])
OMRCFG_DEFINE_FLAG_OFF([OMR_TEST_COMPILER], [], [Enable building the Test compiler. (Default: no)])
OMRCFG_DEFINE_FLAG_ON([OMR_PORT], [], [Enable building the Port library. (Default: yes)])
OMRCFG_DEFINE_FLAG_ON([OMR_THREAD], [], [Enable building the Thread library. (Default: yes)])
OMRCFG_DEFINE_FLAG_ON([OMR_OMRSIG], [], [Enable building the OMRSig library. (Default: yes)])
OMRCFG_DEFINE_FLAG_OFF([OMR_EXAMPLE], [], [Whether we are building OMR against the example code. (Default: no)])
OMRCFG_DEFINE_FLAG_OFF([OMR_RTTI], [], [Whether we are building OMR with RTTI. (Default: no)])

############### Flags that are always enabled
# Each flag must have a corresponding entry in omrcfg.h.in

OMRCFG_DEFINE_FLAG_ON([OMR_GC_ALLOCATION_TAX])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_BATCH_CLEAR_TLH])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_COMBINATION_SPEC])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_DEBUG_ASSERTS])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_LARGE_OBJECT_AREA])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_MINIMUM_OBJECT_SIZE])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_MODRON_STANDARD])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_NON_ZERO_TLH])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_THREAD_LOCAL_HEAP])
OMRCFG_DEFINE_FLAG_ON([OMR_INTERP_HAS_SEMAPHORES])
OMRCFG_DEFINE_FLAG_ON([OMR_RAS_TDF_TRACE])
OMRCFG_DEFINE_FLAG_ON([OMR_THR_ADAPTIVE_SPIN])
OMRCFG_DEFINE_FLAG_ON([OMR_THR_JLM])
OMRCFG_DEFINE_FLAG_ON([OMR_THR_JLM_HOLD_TIMES])
OMRCFG_DEFINE_FLAG_ON([OMR_THR_LOCK_NURSERY])

############### Flags that are always disabled
# Each flag must have a corresponding entry in omrcfg.h.in

OMRCFG_DEFINE_FLAG_OFF([OMR_THR_TRACING])

############### Flags that are conditionally enabled.
# Each flag must have a corresponding entry in omrcfg.h.in

OMRCFG_DEFINE_FLAG_OFF([OMR_GC_DYNAMIC_CLASS_UNLOADING])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_MODRON_COMPACTION])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_MODRON_CONCURRENT_MARK])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_MODRON_SCAVENGER])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_DOUBLE_MAP_ARRAYLETS])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_CONCURRENT_SCAVENGER])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_CONCURRENT_SWEEP])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_LEAF_BITS])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_OBJECT_ALLOCATION_NOTIFY])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_REALTIME])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_SCAN_OBJECT_GLUE])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_SEGREGATED_HEAP])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_VLHGC])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_ASYNC_HANDLER])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_POWER])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_ARM])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_AARCH64])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_X86])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_S390])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_RISCV])

OMRCFG_DEFINE_FLAG([OMR_ENV_DATA64],[],
	[AS_IF([test "$OMR_TARGET_DATASIZE" = 64],
		[OMRCFG_ENABLE_FLAG([OMR_ENV_DATA64])],
		[OMRCFG_DISABLE_FLAG([OMR_ENV_DATA64])]
	)]
)

AC_MSG_CHECKING([OMR_GC_POINTER_MODE])
AC_ARG_VAR([OMR_GC_POINTER_MODE], [The mode of pointers.  Must be "full", "compressed", or "mixed"])
: ${OMR_GC_POINTER_MODE="full"}
AC_MSG_RESULT([$OMR_GC_POINTER_MODE])

AS_IF([[test "x$OMR_GC_POINTER_MODE" = "xmixed"] || [test "x$OMR_GC_POINTER_MODE" = "xcompressed"]],
	[AS_IF([test "x$OMR_TARGET_DATASIZE" = "x32"],
		[AC_MSG_ERROR([\$OMR_GC_POINTER_MODE=$OMR_GC_POINTER_MODE requires 64 bit target datasize])])])

AS_CASE(["x$OMR_GC_POINTER_MODE"],
	[xfull],
		[OMRCFG_ENABLE_FLAG([OMR_GC_FULL_POINTERS])
		 OMRCFG_DISABLE_FLAG([OMR_GC_COMPRESSED_POINTERS])],
	[xcompressed],
		[OMRCFG_DISABLE_FLAG([OMR_GC_FULL_POINTERS])
		 OMRCFG_ENABLE_FLAG([OMR_GC_COMPRESSED_POINTERS])],
	[xmixed],
		[OMRCFG_ENABLE_FLAG([OMR_GC_FULL_POINTERS])
		 OMRCFG_ENABLE_FLAG([OMR_GC_COMPRESSED_POINTERS])],
	[AC_MSG_ERROR([OMR_GC_FULL_POINTERS must be set to one of \"full\", \"compressed\", or \"mixed\"])])

OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS])

OMRCFG_DEFINE_FLAG([OMR_THR_FORK_SUPPORT],[],
	[AS_IF([[test "$OMR_HOST_OS" = linux] || [test "$OMR_HOST_OS" = aix] || [test "$OMR_HOST_OS" = osx]],
		[OMRCFG_ENABLE_FLAG([OMR_THR_FORK_SUPPORT])],
		[OMRCFG_DISABLE_FLAG([OMR_THR_FORK_SUPPORT])]
	)]
)

OMRCFG_DEFINE_FLAG_OFF([OMR_THR_THREE_TIER_LOCKING])
OMRCFG_DEFINE_FLAG_OFF([OMR_THR_CUSTOM_SPIN_OPTIONS])
OMRCFG_DEFINE_FLAG_OFF([OMR_NOTIFY_POLICY_CONTROL])
OMRCFG_DEFINE_FLAG_OFF([OMR_THR_SPIN_WAKE_CONTROL])

OMRCFG_DEFINE_FLAG_OFF([OMR_THR_YIELD_ALG])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_OBJECT_MAP])

OMRCFG_DEFINE_FLAG([OMR_ENV_LITTLE_ENDIAN],[],
	[
	if test "x$OMR_CROSS_CONFIGURE" != "xyes"
	then
	AC_C_BIGENDIAN(
		[OMRCFG_DISABLE_FLAG([OMR_ENV_LITTLE_ENDIAN])],
		[OMRCFG_ENABLE_FLAG([OMR_ENV_LITTLE_ENDIAN])],
		[],
		[OMRCFG_DISABLE_FLAG([OMR_ENV_LITTLE_ENDIAN])])
	fi
	]
)

OMRCFG_DEFINE_FLAG([OMR_GC_TLH_PREFETCH_FTA],[],
	[AS_IF([[test "$OMR_HOST_ARCH" = x86] || [test "$OMR_HOST_ARCH" = x86]],
		[OMRCFG_ENABLE_FLAG([OMR_GC_TLH_PREFETCH_FTA])],
		[OMRCFG_DISABLE_FLAG([OMR_GC_TLH_PREFETCH_FTA])]
	)]
)

OMRCFG_DEFINE_FLAG_OFF([OMR_ENV_GCC])

OMRCFG_DEFINE_FLAG([OMR_PORT_NUMA_SUPPORT],[],
	[
	if test "x$OMR_CROSS_CONFIGURE" != "xyes"
	then
	# The no-op ":" prevents this if-then from being left empty
	# if AC_CHECK_HEADERS_ONCE has already been evaluated.
	:
	AC_CHECK_HEADERS_ONCE([numa.h],
		[OMRCFG_ENABLE_FLAG([OMR_PORT_NUMA_SUPPORT])],
		[OMRCFG_DISABLE_FLAG([OMR_PORT_NUMA_SUPPORT])])
	fi
	]
)

if test "x$OMR_CROSS_CONFIGURE" != "xyes"
then
	AS_IF([test "$OMR_PORT_NUMA_SUPPORT" = 1 -a "$OMR_HOST_OS" = linux],
		[AC_CHECK_HEADERS_ONCE([numa.h],
			[],
			[AC_MSG_ERROR([numa.h was not found. Please install numa-dev])])
		],
		[]
	)
fi

OMRCFG_DEFINE_FLAG_OFF([OMR_OPT_CUDA])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_ALLOCATE_TOP_DOWN])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_ZOS_CEEHDLRSUPPORT])
OMRCFG_DEFINE_FLAG_OFF([OMRPORT_OMRSIG_SUPPORT])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_SOCKET_SUPPORT])
OMRCFG_DEFINE_FLAG_OFF([OMR_THR_MCS_LOCKS])

OMRCFG_DEFINE_FLAG([OMRTHREAD_LIB_AIX],[1],
	[AS_IF([test "$OMR_HOST_OS" = aix],
		[OMRCFG_ENABLE_FLAG([OMRTHREAD_LIB_AIX], [1])],
		[OMRCFG_DISABLE_FLAG([OMRTHREAD_LIB_AIX])]
	)]
)
OMRCFG_DEFINE_FLAG([OMRTHREAD_LIB_WIN32],[1],
	[AS_IF([test "$OMR_HOST_OS" = win],
		[OMRCFG_ENABLE_FLAG([OMRTHREAD_LIB_WIN32], [1])],
		[OMRCFG_DISABLE_FLAG([OMRTHREAD_LIB_WIN32])]
	)]
)
OMRCFG_DEFINE_FLAG([OMRTHREAD_LIB_ZOS],[1],
	[AS_IF([test "$OMR_HOST_OS" = zos],
		[OMRCFG_ENABLE_FLAG([OMRTHREAD_LIB_ZOS], [1])],
		[OMRCFG_DISABLE_FLAG([OMRTHREAD_LIB_ZOS])]
	)]
)
OMRCFG_DEFINE_FLAG([OMRTHREAD_LIB_UNIX],[1],
	[AS_IF([[test "$OMR_HOST_OS" = linux] || [test "$OMR_HOST_OS" = osx]],
		[OMRCFG_ENABLE_FLAG([OMRTHREAD_LIB_UNIX], [1])],
		[OMRCFG_DISABLE_FLAG([OMRTHREAD_LIB_UNIX])]
	)]
)

AC_ARG_VAR([OMR_DIASDK_HOME],
	[The directory where the DIA SDK can be found.] (Default: ))

AS_IF([test "$enable_DDR" = yes -a "$cross_compiling" != "yes"],
	[AS_CASE($OMR_HOST_OS,
		[aix],[],
		[osx],[],
		[win],[
			# a user-supplied path wins
			DIASDK_HOME="$OMR_DIASDK_HOME"
			# otherwise if VSINSTALLDIR is set, use the DIA SDK found there
			AS_IF([test "x$DIASDK_HOME" = "x" -a "x$VSINSTALLDIR" != "x"],
				[DIASDK_HOME="$VSINSTALLDIR/DIA SDK"]
			)
			# next, look relative to the location of the Visual Studio compiler
			AS_IF([test "x$DIASDK_HOME" = "x" -a "$OMR_TOOLCHAIN" = msvc],
				[
				AC_PATH_PROG([VS_CL_PATH],[cl.exe])
				AS_IF([test "x$VS_CL_PATH" != x],
					[
					# this should yield .../VC/bin/cl.exe or .../VC/bin/amd64/cl.exe
					vc_bin_dir=$(AS_DIRNAME(["$VS_CL_PATH"]))
					diasdk_dir1="$vc_bin_dir/../../DIA SDK"
					diasdk_dir2="$vc_bin_dir/../../../DIA SDK"
					diasdk_dir3="$vc_bin_dir/../../../../../../../DIA SDK"
					AS_IF([test -f "$diasdk_dir1/include/dia2.h"], [DIASDK_HOME="$(cygpath -a -w "$diasdk_dir1")"],
						[test -f "$diasdk_dir2/include/dia2.h"], [DIASDK_HOME="$(cygpath -a -w "$diasdk_dir2")"],
						[test -f "$diasdk_dir3/include/dia2.h"], [DIASDK_HOME="$(cygpath -a -w "$diasdk_dir3")"]
					)
					]
				)
				]
			)
			# fail if none of the above
			AS_IF([test "x$DIASDK_HOME" = "x"],
				[AC_MSG_ERROR([--enable-DDR requires the DIA SDK; set VSINSTALLDIR or OMR_DIASDK_HOME])]
			)
			# use forward slashes instead of backslashes; remove any duplicate slashes
			DIASDK_HOME=$(echo "$DIASDK_HOME" | sed -e 's:\\:/:g' -e 's://:/:'g)
		],
		[
			# other host types: look for DWARF support
			if test "x$OMR_CROSS_CONFIGURE" != "xyes"
			then
				AC_CHECK_HEADERS([dwarf.h libdwarf/dwarf.h libdwarf.h libdwarf/libdwarf.h])
				AC_SEARCH_LIBS([dwarf_init], [dwarf], [], [], [])
				AC_SEARCH_LIBS([dwarf_init], [dwarf], [], [], [-lelf])
				AC_SEARCH_LIBS([elf_begin], [elf], [], [], [])
			else
				# Can't run AC_CHECK_HEADERS when cross-configuring.
				# If DDR is enabled set these flags and hope for the best,
				# otherwise the build will fail even if the headers are available.
				AC_DEFINE([HAVE_DWARF_H])
				AC_DEFINE([HAVE_LIBDWARF_H])
			fi
		]
	)]
)

OMRCFG_DEFINE_FLAG_OFF([OMR_GC_IDLE_HEAP_MANAGER])
OMRCFG_DEFINE_FLAG_OFF([OMR_VALGRIND_MEMCHECK], [], [Integrate valgrind memcheck API. (Default: no)])

###
### Variable Substitution
###
AC_SUBST(OMR_CROSS_COMPILE)

AS_IF([test "x$BUILD_CC" = "x"], [BUILD_CC="$CC"])
AS_IF([test "x$HOST_CC" = "x"], [HOST_CC="$CC"])

AC_SUBST(CC,[$BUILD_CC])
AC_SUBST(CXX)
AC_SUBST(CCLINK)
AC_SUBST(CCLINKSHARED)
AC_SUBST(CCLINKEXE)
AC_SUBST(CXXLINK)
AC_SUBST(CXXLINKSHARED)
AC_SUBST(CXXLINKEXE)

AC_SUBST(AS)
AC_SUBST(AR)
AC_SUBST(OBJCOPY)

AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(ARFLAGS)

AC_SUBST(GLOBAL_CFLAGS)
AC_SUBST(GLOBAL_CXXFLAGS)
AC_SUBST(GLOBAL_CPPFLAGS)
AC_SUBST(GLOBAL_INCLUDES)
AC_SUBST(GLOBAL_LDFLAGS)
AC_SUBST(GLOBAL_SHARED_LIBS)
AC_SUBST(GLOBAL_STATIC_LIBS)
AC_SUBST(GLOBAL_LIBPATH)
AC_SUBST(GLOBAL_ARFLAGS)

AC_SUBST(OMR_OPTIMIZE)
AC_SUBST(OMR_WARNINGS_AS_ERRORS)
AC_SUBST(OMR_ENHANCED_WARNINGS)
AC_SUBST(OMR_DEBUG)
AC_SUBST([enable_fvtest])
AC_SUBST([enable_fvtest_agent])
AC_SUBST([enable_tracegen])
AC_SUBST([enable_DDR])
AC_SUBST([DIASDK_HOME])

AC_SUBST(exeext)
AC_SUBST(solibext)
AC_SUBST(arlibext)
AC_SUBST(objext)
AC_SUBST(libprefix)

AC_SUBST(lib_output_dir)
AC_SUBST(exe_output_dir)

###
### Version strings
###

# File checks are normally disabled when cross-compiling. We temporarily set
# $cross_compiling to "no" to allow these file checks to proceed.
# The files being checked are not compiler-generated.
save_cross_compiling=$cross_compiling
cross_compiling=no

# Look for the OMR_VERSION_STRING file in:
#  (1) If the OMR_VERSION_STRING env var is not empty, use it as a full path;
#  (2) The current directory.
AS_IF([test "x$OMR_VERSION_STRING" = "x"],
	[OMR_VERSION_STRING=./OMR_VERSION_STRING])

# Set the undefined version string if the OMR_VERSION_STRING file doesn't exist.
AC_CHECK_FILE($OMR_VERSION_STRING,
	[OMR_UNDEFINED_VERSION_STRING=],
	[OMR_UNDEFINED_VERSION_STRING="#define OMR_VERSION_STRING \"<Unknown>\""])
AC_SUBST(OMR_UNDEFINED_VERSION_STRING)
AC_SUBST_FILE(OMR_VERSION_STRING)

# Look for the OMR_JIT_VERSION_STRING file in:
#  (1) If the OMR_JIT_VERSION_STRING env var is not empty, use it as a full path;
#  (2) The current directory.
AS_IF([test "x$OMR_JIT_VERSION_STRING" = "x"],
	[OMR_JIT_VERSION_STRING=./OMR_JIT_VERSION_STRING])

# Set the undefined version string if the OMR_JIT_VERSION_STRING file doesn't exist.
AC_CHECK_FILE($OMR_JIT_VERSION_STRING,
	[OMR_UNDEFINED_JIT_VERSION_STRING=],
	[OMR_UNDEFINED_JIT_VERSION_STRING="#define OMR_JIT_VERSION_STRING \"<Unknown>\""])
AC_SUBST(OMR_UNDEFINED_JIT_VERSION_STRING)
AC_SUBST_FILE(OMR_JIT_VERSION_STRING)

# Restore the value of $cross_compiling.
cross_compiling=$save_cross_compiling

AC_CONFIG_FILES([
	include_core/omrversionstrings.h:include_core/omrversionstrings.h.in
	omrmakefiles/configure.mk:omrmakefiles/configure.mk.in
	omr.rc
])

AC_CONFIG_HEADERS([include_core/omrcfg.h:include_core/omrcfg.h.in])

###
### Configure the build tools
###
# The build tools are executed on the build machine, not the host machine.
# If cross-compiling, unset CC and CXX so that configure detects the compiler
# on the build machine.
# toolconfig/configure must be run from the subdirectory so that it doesn't
# overwrite output from this configure script.
AC_CONFIG_COMMANDS([toolconfigure],
	[(cd tools && sh ./configure $OMR_HOSTCONFIG_ENV)],
	[AS_IF([test "$cross_compiling" = "yes"],
		[OMR_HOSTCONFIG_ENV="CC=$OMR_TOOLS_CC CXX=$OMR_TOOLS_CXX"])])

AC_OUTPUT
