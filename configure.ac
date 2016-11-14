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

# This configure.ac has been confirmed to work with Autoconf v2.63,
# and to fail with Autoconf v2.59. Autoconf v2.59 does not recognize
# AS_CASE(), AS_ECHO(), AC_MSG_ERROR().
AC_PREREQ(2.63)
AC_INIT([OMR], [1.0])

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
AC_ARG_VAR([OMR_HOST_ARCH], [The architecture of the CPU where the package will run. One of: arm,ppc,s390,x86])
AC_ARG_VAR([OMR_TARGET_DATASIZE], [Specifies whether the package will run in 32- or 64-bit mode. One of: 31,32,64])
AC_ARG_VAR([OMR_TOOLCHAIN], [The toolchain used to build the package. One of: gcc,xlc,msvc])
AC_ARG_VAR([OMR_CROSS_CONFIGURE],
	[Indicates that configure is not being run on a system that is capable of building the package.
	Compiler and feature detection tests will be invalid. All options must be configured manually.
	One of: yes,no. (Default: no)])

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


###
### Build environment configuration
###

# Cache the requested CC
BUILD_CC=$CC

# Prevent AC_PROG_CC() from defaulting CFLAGS to -g for gcc.
: ${CFLAGS=""}

# Prevent AC_PROG_CXX() from defaulting CXXFLAGS to -g -O2 for g++.
: ${CXXFLAGS=""}

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

AS_IF([test "$OMR_CROSS_CONFIGURE" = "yes"],
# If we are cross-configuring, clear CC so that the default platform compiler can be detected.
# Warn that compiler and feature detection tests will be invalid.
# The user must configure all options manually.
  [AS_UNSET([CC])
	AS_ECHO([["## ------------------------------------------------------- ##"]])
	AS_ECHO([["## WARNING                                                 ##"]])
	AS_ECHO([["## The current machine cannot compile the target platform. ##"]])
	AS_ECHO([["## Compiler and feature detection tests will not be valid. ##"]])
	AS_ECHO([["## ------------------------------------------------------- ##"]])]
)

# If OMR_TARGET_DATASIZE is set, validate it and use it.
# Otherwise, detect it with a compiler test.
AS_IF([test "x$OMR_TARGET_DATASIZE" = "x"],
	[AC_CHECK_SIZEOF([void *])])

AC_MSG_CHECKING([OMR_TARGET_DATASIZE])
AS_IF([test "x$OMR_TARGET_DATASIZE" != "x"],
	[AS_CASE([$OMR_TARGET_DATASIZE],
		[31],[],
		[32],[],
		[64],[],
		[AC_MSG_ERROR([An invalid value was provided for OMR_TARGET_DATASIZE: $OMR_TARGET_DATASIZE])]
	)],
	[AS_IF([test "$ac_cv_sizeof_void_p" = 8],
		[OMR_TARGET_DATASIZE=64],
		[OMR_TARGET_DATASIZE=32]
	)]
) 
AC_MSG_RESULT([$OMR_TARGET_DATASIZE])

AC_MSG_CHECKING([OMR_TOOLCHAIN])
AS_IF([test "x$OMR_TOOLCHAIN" = "x"],
	[OMRCFG_CATEGORIZE_TOOLCHAIN([OMR_TOOLCHAIN])])
AS_CASE([$OMR_TOOLCHAIN],
	[gcc],[],
	[xlc],[],
	[msvc],[],
	[AC_MSG_ERROR([An invalid value was provided for OMR_TOOLCHAIN: $OMR_TOOLCHAIN])]
)
AC_MSG_RESULT([$OMR_TOOLCHAIN])

# Default OMRGLUE to example/glue if it was not specified
OMRGLUE=${OMRGLUE:-"./example/glue"}


## Default Variables

# Set program defaults if they are undefined
AC_PROG_CC()
AC_PROG_CXX()

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
# OMRCFG_ENABLE_FLAG(<flag name>, [<value>])
# #define the flag in omrcfg.h
# Set the flag to 1 in omrcfg.mk
AC_DEFUN([OMRCFG_ENABLE_FLAG], [
	AS_IF([test "x$2" = "x"],
		[AC_SUBST([$1], [["#define $1"]])],
		[AC_SUBST([$1], [["#define $1 $2"]])]
	)
	echo "$1 := 1" >> omrmakefiles/omrcfg.mk
])

############### Disable a build flag
# OMRCFG_DISABLE_FLAG(<flag name>)
# The flag is undefined in omrcfg.h
# Set the flag to 0 in omrcfg.mk
AC_DEFUN([OMRCFG_DISABLE_FLAG], [
	AC_SUBST([$1], [["#undef $1"]])
	echo "$1 := 0" >> omrmakefiles/omrcfg.mk
])

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
			[OMRCFG_DISABLE_FLAG([$1])]
		)],
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
#
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

############### Create omrcfg.mk comment
# omrcfg.mk mimicks omrcfg.h, setting each defined variable to 1
# and each undefined variable to 0. This allows usage of build flags
# from within makefiles.
# Note: this must be placed before any OMRCFG_ENABLE_FLAG or OMRCFG_DEFINE_FLAG
# macros to ensure the comments below are echo'd first into omrcfg.mk

echo "##" > omrmakefiles/omrcfg.mk
echo "## THIS IS A GENERATED FILE." >> omrmakefiles/omrcfg.mk
echo "## This file was generated by configure" >> omrmakefiles/omrcfg.mk
echo "##" >> omrmakefiles/omrcfg.mk
echo "" >> omrmakefiles/omrcfg.mk

############### Top-Level Components
OMRCFG_DEFINE_FLAG_ON([OMR_GC], [], [Enable building the OMR GC. (Default: yes)])
OMRCFG_DEFINE_FLAG_OFF([OMR_JIT], [], [Enable building the OMR JIT. (Default: no)])
OMRCFG_DEFINE_FLAG_ON([OMR_PORT], [], [Enable building the Port library. (Default: yes)])
OMRCFG_DEFINE_FLAG_ON([OMR_THREAD], [], [Enable building the Thread library. (Default: yes)])
OMRCFG_DEFINE_FLAG_ON([OMR_OMRSIG], [], [Enable building the OMRSig library. (Default: yes)])
OMRCFG_DEFINE_FLAG_OFF([OMR_EXAMPLE], [], [Whether we are building OMR against the example code. (Default: no)])

############### Flags that are always enabled
# Each flag must have a corresponding entry in omrcfg.h.in

OMRCFG_DEFINE_FLAG_ON([OMR_GC_ALLOCATION_TAX])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_ARRAYLETS])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_BATCH_CLEAR_TLH])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_COMBINATION_SPEC])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_DEBUG_ASSERTS])
OMRCFG_DEFINE_FLAG_ON([OMR_GC_HEAP_CARD_TABLE])
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
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_CONCURRENT_SCAVENGER])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_CONCURRENT_SWEEP])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_HYBRID_ARRAYLETS])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_OBJECT_ALLOCATION_NOTIFY])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_REALTIME])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_SEGREGATED_HEAP])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_STACCATO])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_VLHGC])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_ASYNC_HANDLER])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_POWER])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_ARM])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_X86])
OMRCFG_DEFINE_FLAG_OFF([OMR_ARCH_S390])

OMRCFG_DEFINE_FLAG([OMR_ENV_DATA64],[],
	[AS_IF([test "$OMR_TARGET_DATASIZE" = 64],
		[OMRCFG_ENABLE_FLAG([OMR_ENV_DATA64])],
		[OMRCFG_DISABLE_FLAG([OMR_ENV_DATA64])]
	)]
)

OMRCFG_DEFINE_FLAG_OFF([OMR_GC_COMPRESSED_POINTERS])
OMRCFG_DEFINE_FLAG_OFF([OMR_INTERP_COMPRESSED_OBJECT_HEADER])
OMRCFG_DEFINE_FLAG_OFF([OMR_INTERP_SMALL_MONITOR_SLOT])

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

OMRCFG_DEFINE_FLAG_OFF([OMR_THR_YIELD_ALG])
OMRCFG_DEFINE_FLAG_OFF([OMR_GC_OBJECT_MAP])
OMRCFG_DEFINE_FLAG([OMR_ENV_LITTLE_ENDIAN],[],
	[AC_C_BIGENDIAN(
		[OMRCFG_DISABLE_FLAG([OMR_ENV_LITTLE_ENDIAN])],
		[OMRCFG_ENABLE_FLAG([OMR_ENV_LITTLE_ENDIAN])],
		[],
		[OMRCFG_DISABLE_FLAG([OMR_ENV_LITTLE_ENDIAN])]
	)]
)

OMRCFG_DEFINE_FLAG([OMR_GC_TLH_PREFETCH_FTA],[],
	[AS_IF([[test "$OMR_HOST_ARCH" = x86] || [test "$OMR_HOST_ARCH" = x86]],
		[OMRCFG_ENABLE_FLAG([OMR_GC_TLH_PREFETCH_FTA])],
		[OMRCFG_DISABLE_FLAG([OMR_GC_TLH_PREFETCH_FTA])]
	)]
)

OMRCFG_DEFINE_FLAG_OFF([OMR_ENV_GCC])

OMRCFG_DEFINE_FLAG([OMR_PORT_NUMA_SUPPORT],[],
	[AC_CHECK_HEADERS([numa.h],
		[OMRCFG_ENABLE_FLAG([OMR_PORT_NUMA_SUPPORT])],
		[OMRCFG_DISABLE_FLAG([OMR_PORT_NUMA_SUPPORT])]
	)]
)

OMRCFG_DEFINE_FLAG_OFF([OMR_OPT_CUDA])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_ALLOCATE_TOP_DOWN])
OMRCFG_DEFINE_FLAG_OFF([OMR_PORT_ZOS_CEEHDLRSUPPORT])

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

AS_IF([test "$cross_compiling" != "yes"],
	[# Look for libdwarf.h
	AC_CHECK_HEADER([libdwarf.h], [],
		[AC_CHECK_HEADER([/usr/include/libdwarf/libdwarf.h],
			[OMR_LIBDWARF_IPATH="/usr/include/libdwarf"])])

	# Look for libdwarf.so
	# If the shared lib isn't found, assume we're linking the static lib,
	# so elf must be linked explicitly.
	AC_CHECK_FILE([/usr/lib/libdwarf.so],
		[OMR_LIBDWARF_SHARED_LIBS="dwarf"],
		[OMR_LIBDWARF_STATIC_LIBS="dwarf elf"])]
)

###
### Variable Substitution
###
AC_SUBST(OMR_CROSS_COMPILE)

AS_IF([test "x$BUILD_CC" = "x"], [BUILD_CC=$CC])
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

AC_SUBST(exeext)
AC_SUBST(solibext)
AC_SUBST(arlibext)
AC_SUBST(objext)
AC_SUBST(libprefix)

AC_SUBST(lib_output_dir)
AC_SUBST(exe_output_dir)


AC_SUBST(OMR_LIBDWARF_IPATH)
AC_SUBST(OMR_LIBDWARF_SHARED_LIBS)
AC_SUBST(OMR_LIBDWARF_STATIC_LIBS)

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
	include_core/omrcfg.h:include_core/omrcfg.h.in
	omrmakefiles/configure.mk:omrmakefiles/configure.mk.in
])

###
### Configure the build tools
###
# The build tools are executed on the build machine, not the host machine.
# If cross-compiling, unset CC and CXX so that configure detects the compiler
# on the build machine.
# toolconfig/configure must be run from the subdirectory so that it doesn't
# overwrite output from this configure script.
AC_CONFIG_COMMANDS([toolconfigure],
	[(cd toolconfig && sh ./configure $OMR_HOSTCONFIG_ENV)],
	[AS_IF([test "$cross_compiling" = "yes"],
		[OMR_HOSTCONFIG_ENV="CC= CXX="])])

AC_OUTPUT
