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

# Include useful tool macros.
include(cmake/AddPrefix.cmake)
include(cmake/RemoveOption.cmake)

# Some operating systems / toolchains have requirements that can't be 
# expressed on a per-target basis. Do that setup here: 
omr_os_global_configuration()

macro(omr_set_target_flags target)

   target_compile_definitions(${target} PRIVATE
      ${OMR_OS_DEFINITIONS}
      ${OMR_ARCH_DEFINITIONS}
      )

   target_compile_options(${target} PRIVATE 
      ${OMR_OS_COMPILE_OPTIONS}
      )

   if(OMR_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE ${OMR_WARNING_AS_ERROR_FLAG}) 
   endif()

   omr_os_target_configuration(${target})

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

