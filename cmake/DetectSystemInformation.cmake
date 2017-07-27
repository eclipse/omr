###############################################################################
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

# Translate from CMake's view of the system to the OMR view of the system. 
# Exports a number of variables indicicating platform, os, endianness, etc. 
# - OMR_ARCH_{X86,ARM,S390} # TODO: Add POWER
# - OMR_ENV_DATA{32,64}  
# - OMR_ENV_TARGET_DATASIZE (either 32 or 64) 
# - OMR_ENV_LITTLE_ENDIAN
# - OMR_HOST_OS
macro(omr_detect_system_information) 

   if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
      set(OMR_ARCH_X86 ON)
      set(OMR_ENV_DATA64 ON)
      set(OMR_TARGET_DATASIZE 64)
      set(OMR_ENV_LITTLE_ENDIAN ON)
   elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i[3-6]86")
      set(OMR_ARCH_X86 ON)
      set(OMR_ENV_DATA32 ON)
      set(OMR_TARGET_DATASIZE 32)
      set(OMR_ENV_LITTLE_ENDIAN ON)
   elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
      set(OMR_ARCH_ARM ON)
      set(OMR_ENV_DATA32 ON)
      set(OMR_TARGET_DATASIZE 32)
      set(OMR_ENV_LITTLE_ENDIAN ON)
   elseif(CMAKE_SYSTEM_NAME MATCHES "OS390")
      set(OMR_ARCH_S390 ON)
      set(OMR_ENV_DATA32 ON)
      set(OMR_TARGET_DATASIZE 32)
   else()
      message(FATAL_ERROR "Unknown processor: ${CMAKE_SYSTEM_PROCESSOR}")
   endif()

   #TODO: Detect AIX 
   if(CMAKE_SYSTEM_NAME MATCHES "Linux")
      set(OMR_HOST_OS "linux")
   elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
      set(OMR_HOST_OS "osx")
   elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
      set(OMR_HOST_OS "win")
   elseif(CMAKE_SYSTEM_NAME MATCHES "OS390")
      set(OMR_HOST_OS "zos")
   else()
      message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
   endif()

   if(OMR_HOST_OS STREQUAL "linux")
      # Linux specifics
      set(OMR_TOOLCONFIG "gnu") 
   endif()

   if(OMR_HOST_OS STREQUAL "win")
      #TODO: should probably check for MSVC
      set(OMR_WINVER "0x501")
      if(OMR_ENV_DATA64)
         set(TARGET_MACHINE AMD64)
      else()
         set(TARGET_MACHINE i386)
      endif()
      set(OMR_TOOLCONFIG "msvc") 
   endif()

   if(OMR_HOST_OS STREQUAL "zos")
      # ZOS specifics
   endif()


   if(OMR_HOST_OS STREQUAL "osx")
      # OSX specifics
   endif()

endmacro(omr_detect_system_information)
