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


###
### Platform flags
### TODO: arch flags. Defaulting to x86-64

message(STATUS "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM}")
message(STATUS "CMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}")

include(cmake/DetectSystemInformation.cmake)
omr_detect_system_information()

# Pickup OS info 
include(cmake/platform/os/${OMR_HOST_OS}.cmake OPTIONAL)

# Pickup Arch Info
include(cmake/platform/arch/${OMR_HOST_ARCH}.cmake OPTIONAL) 

# Pickup toolconfig based on platform. 
include(cmake/platform/toolcfg/${OMR_TOOLCONFIG}.cmake OPTIONAL)

# Verify toolconfig!
include(cmake/platform/toolcfg/verify.cmake)

include(cmake/AddPrefix.cmake)

# Remove a specified option from a variable
macro(omr_remove_option var opt)
   string( REGEX REPLACE
      "(^| )${opt}($| )"
      ""
      ${var}
      "${${var}}"
      )
endmacro(omr_remove_option)

# Some operating systems / toolchains have requirements that can't be 
# expressed on a per-target basis. Do that setup here: 
omr_os_global_configuration()

macro(omr_set_target_flags target)

   add_prefix(OMR_OS_DEFINITIONS_PREFIXED   ${OMR_C_DEFINITION_PREFIX} ${OMR_OS_DEFINITIONS})
   add_prefix(OMR_ARCH_DEFINITIONS_PREFIXED ${OMR_C_DEFINITION_PREFIX} ${OMR_ARCH_DEFINITIONS})

   target_compile_definitions(${target} PRIVATE
      ${OMR_OS_DEFINITIONS_PREFIXED}
      ${OMR_ARCH_DEFINITIONS_PREFIXED}
      )

   target_compile_options(${target} PRIVATE 
      ${OMR_OS_COMPILE_OPTIONS}
      )

   if(OMR_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE ${OMR_WARNING_AS_ERROR_FLAG}) 
   endif()

   if(OMR_HOST_OS STREQUAL "win")

      # we want to disable C4091, and C4577
      # C4577 is a bogus warning about specifying noexcept when exceptions are disabled
      # C4091 is caused by broken windows sdk (https://connect.microsoft.com/VisualStudio/feedback/details/1302025/warning-c4091-in-sdk-7-1a-shlobj-h-1051-dbghelp-h-1054-3056)
      set(linker_common "-subsystem:console -machine:${TARGET_MACHINE}")

      if(NOT OMR_ENV_DATA64)
         set(linker_common "${linker_common} /SAFESEH")
      endif()

      target_link_libraries(${target} PRIVATE "${linker_common} /LARGEADDRESSAWARE wsetargv.obj")
      if(OMR_ENV_DATA64)
         #TODO: makefile has this but it seems to break linker
         #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
      endif()

      if(OMR_ENV_DATA64)
         target_link_libraries(${target} PRIVATE "-entry:_DllMainCRTStartup")
      else()
         target_link_libraries(${traget} PRIVATE "-entry:_DllMainCRTStartup@12")
      endif()

   endif()

   # FIXME BEFORE DELIVERY: HAVEN'T TARGETized ZOS
   if(OMR_HOST_OS STREQUAL "zos")
      target_include_directories(${target} PRIVATE util/a2e/headers)
   endif()

endmacro(omr_set_target_flags)
###
### Flags we aren't using
###

# TODO: SPEC

# TODO: OMR_HOST_ARCH
# TODO: OMR_TARGET_DATASIZE
# TODO: OMR_TOOLCHAIN
# TODO: OMR_CROSS_CONFIGURE
# TODO: OMR_RTTI

