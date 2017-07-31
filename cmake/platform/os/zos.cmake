##############################################################################
#
# (c) Copyright IBM Corp. 2017
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

# Note: This file currently conflates z/OS and XLC for z/OS

set(OMR_OS_DEFINITIONS 
         J9ZOS390
         LONGLONG
         J9VM_TIERED_CODE_CACHE
         _ALL_SOURCE
         _XOPEN_SOURCE_EXTENDED
         IBM_ATOE
         _POSIX_SOURCE
         )

if(OMR_ENV_DATA64)
   list(APPEND OMR_OS_DEFINITIONS DJ9ZOS39064)
else()
   list(APPEND OMR_OS_DEFINITIONS _LARGE_FILES)
endif()


# Common flags used by most z/OS invocations. 
# 
# xplink   Link with the xplink calling convention
# convlit  Convert all string literals to a codepage
# rostring Place string literals in read only storage
# FLOAT    Use IEEE (instead of IBM Hex Format) style floats
# enum     Specifies how many bytes of storage enums occupy
# a,goff   Assemble into GOFF object files
# NOANSIALIAS Do not generate ALIAS binder control statements
# TARGET   Generate code for the target operating system
# list     Generate assembly listing
# offset   In assembly listing, show addresses as offsets of function entry points
#
# 
# NOTE: character escaping on compiler args seems broken
# 
#set(OMR_GLOBAL_FLAGS "-Wc,xplink,convlit\\\(ISO8859-1\\\),rostring,FLOAT\\\(IEEE,FOLD,AFP\\\),enum\\\(4\\\) -Wa,goff -Wc,NOANSIALIAS -Wc,TARGET\\\(zOSV1R13\\\) -W \"c,list,offset\"")
set(zos_common_flags "-Wc,xplink,convlit\\\(ISO8859-1\\\),rostring,FLOAT\\\(IEEE,FOLD,AFP\\\),enum\\\(4\\\) -Wa,goff -Wc,NOANSIALIAS -Wc,TARGET\\\(zOSV1R13\\\)")

# Append 64 bit flags.
if(OMR_ENV_DATA64)
   set(zos_common_flags "${zos_common_flags} -Wc,lp64 -Wa,SYSPARM\\\(BIT64\\\)")
endif()

set(zos_c_common_flags "${zos_c_common_flags} -qnosearch -I${CMAKE_SOURCE_DIR}/util/a2e/headers -I/usr/include")


# Using the COMPILE_LANGUAGE generator expression here should be fine as we 
# will not be doing any IDE builds for z/OS. 
set(OMR_OS_COMPILE_OPTIONS
    $<$<COMPILE_LANGUAGE:C>:  "-Wc,ARCH\\\(7\\\) -Wc,\"langlvl(extc99)\"   ${zos_c_common_flags}">
    $<$<COMPILE_LANGUAGE:CXX>:"-Wc,ARCH\\\(7\\\) -Wc,\"langlvl(extended)\" ${zos_c_common_flags} -I/usr/lpp/cbclib/include">
   )

set(OMR_EXPORT_ALL "-Wc,DLL,EXPORTALL")


# Setup things that can't be done on a per-target basis. 
macro(omr_os_global_configuration) 
   message(STATUS "Executing ZOS global configuration") 
   enable_language(ASM-ZOS)
   set(CMAKE_ASM-ZOS_FLAGS "${zos_common_flags}")

   set(CMAKE_SHARED_LINKER_FLAGS "-Wl,xplink,dll")

   #TODO below is a chunk of the original makefile which still needs to be ported
   # # This is the first option applied to the C++ linking command.
   # # It is not applied to the C linking command.
   # OMR_MK_CXXLINKFLAGS=-Wc,"langlvl(extended)" -+

   # ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
   # GLOBAL_LDFLAGS+=-Wl,xplink,dll
   # else
   # # Assume we're linking an executable
   # GLOBAL_LDFLAGS+=-Wl,xplink
   # endif
   # ifeq (1,$(OMR_ENV_DATA64))
   # OMR_MK_CXXLINKFLAGS+=-Wc,lp64
   # GLOBAL_LDFLAGS+=-Wl,lp64
   # endif

   # # always link a2e last, unless we are creating the a2e library
   # ifneq (j9a2e,$(MODULE_NAME))
   # GLOBAL_SHARED_LIBS+=j9a2e
   # endif

   #dump DLLs and exes in same dir like on windows
   set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
   #apparently above doesnt work like it does on windows. Attempt to work arround
   set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
   set(LIBRARY_OUTPUT_PATH  "${CMAKE_BINARY_DIR}")



   message(STATUS "DEBUG: RUNTIME_OUTPUT_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
   message(STATUS "DEBUG: CFLAGS=${CMAKE_C_FLAGS}")
   message(STATUS "DEBUG: EXE LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}")
   message(STATUS "DEBUG: so LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}")

endmacro()


# Setup z/OS things on a per-target basis. 
macro(omr_os_target_configuration target) 
      target_include_directories(${target} PRIVATE util/a2e/headers)
endmacro()
