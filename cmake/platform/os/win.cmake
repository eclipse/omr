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

# Note, this file currently conflates Windows and MSVC. 

set(OMR_OS_DEFINITIONS 
   WIN32
   _CRT_SECURE_NO_WARNINGS
   CRTAPI1=_cdecl
   CRTAPI2=_cdecl
   _WIN_95
   _WIN32_WINDOWS=0x0500
   _WIN32_DCOM
   _MT
   _WINSOCKAPI_
   _WIN32_WINVER=${OMR_WINVER}
   _WIN32_WINNT=${OMR_WINVER}
   _DLL
   _HAS_EXCEPTIONS=0
   )

if(OMR_ENV_DATA64)
   list(APPEND OMR_OS_DEFINITIONS 
      WIN64
      _AMD64_=1)
else()
   list(APPEND OMR_OS_DEFINITIONS 
      _X86_)
endif()

set(OMR_OS_COMPILE_OPTIONS "/GS-")

# Setup things that can't be done on a per-target basis. 
macro(omr_os_global_configuration) 
   message(STATUS "Executing Windows global configuration") 
   #      Make sure we are building without incremental linking
   omr_remove_option(CMAKE_EXE_LINKER_FLAGS "/INCREMENTAL")
   omr_remove_option(CMAKE_SHARED_LINKER_FLAGS "/INCREMENTAL")
   set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO /NOLOGO")
   set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO /NOLOGO")
   foreach(build_type IN LISTS CMAKE_CONFIGURATION_TYPES)
      string(TOUPPER "${build_type}" build_type)
      omr_remove_option("CMAKE_EXE_LINKER_FLAGS_${build_type}" "/INCREMENTAL")
      omr_remove_option("CMAKE_SHARED_LINKER_FLAGS_${build_type}" "/INCREMENTAL")
   endforeach()

   set(common_flags "-MD -Zm400 /wd4577 /wd4091")
   #strip out exception handling flags (added by default by cmake)
   omr_remove_option(CMAKE_CXX_FLAGS "/EHsc")
   omr_remove_option(CMAKE_CXX_FLAGS "/GR")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${common_flags}")
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${common_flags}")

   message(STATUS "CFLAGS = ${CMAKE_C_FLAGS}")
   message(STATUS "CXXFLAGS = ${CMAKE_CXX_FLAGS}")

   #Hack up output dir to fix dll dependency issues on windows
   set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endmacro()
