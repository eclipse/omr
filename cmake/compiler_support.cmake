###############################################################################
#
# (c) Copyright IBM Corp. 2017, 2017
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

# This file contains a number of support pieces required to build the compiler
# component. 
# 
# One of the building blocks of this support is the notion that each compiler
# you may want to build has an associated 'name'. For example jitbuilder is 
# the name of one. 

## Tools and helpers:

Find_package(Perl)

if (NOT PERL_FOUND )
   message(FATAL_ERROR "Perl not found")
endif()

set(MASM2GAS_PATH ${OMR_ROOT}/tools/compiler/scripts/masm2gas.pl CACHE INTERNAL "MASM2GAS PATH")

include(${OMR_ROOT}/cmake/AddPrefix.cmake) 

# Platform setup code!
# TODO: THis needs to be abstracted for cross platform builds

set(TR_TARGET_ARCH    x     CACHE INTERNAL "The architecture directory used for the compiler code (x, p, arm, or z)")
set(TR_TARGET_SUBARCH amd64 CACHE INTERNAL "The subarchitecture directory used for the compiler code. May be empty (i386 or amd64)")
set(TR_TARGET_BITS    64    CACHE INTERNAL  "Bitness of the target architecture")

set(TR_HOST_ARCH    x     CACHE INTERNAL "The architecture directory used for the compiler code (x, p, or z)")
set(TR_HOST_SUBARCH amd64 CACHE INTERNAL "The subarchitecture directory used for the compiler code. May be empty (i386 or amd64)")
set(TR_HOST_BITS    64    CACHE INTERNAL  "Bitness of the target architecture")

set(CMAKE_ASM-ATT_FLAGS "--64 --defsym TR_HOST_X86=1 --defsym TR_HOST_64BIT=1 --defsym BITVECTOR_64BIT=1 --defsym LINUX=1 --defsym TR_TARGET_X86=1 --defsym TR_TARGET_64BIT=1" CACHE INTERNAL "ASM FLags")

# Mark a target as consuming the compiler components. 
# 
# This supplements the compile definitions, options and 
# include directories.
# 
# Call as make_compiler_component(<target> COMPILER <compiler name>)
function(make_compiler_target TARGET_NAME) 
   cmake_parse_arguments(TARGET 
      "" # Optional Arguments
      "COMPILER" # One value arguments
      ""
      ${ARGN}
      )

   set(COMPILER_DEFINES  ${${TARGET_COMPILER}_DEFINES})
   set(COMPILER_ROOT     ${${TARGET_COMPILER}_ROOT})
   set(COMPILER_INCLUDES ${${TARGET_COMPILER}_INCLUDES})
   message("making ${TARGET_NAME} into a compiler target,for ${TARGET_COMPILER}.")
   message("Defines are ${COMPILER_DEFINES}, arch is ${TR_TARGET_ARCH} and ${TR_TARGET_SUBARCH}") 
   target_compile_features(${TARGET_NAME} PUBLIC cxx_static_assert)
   # Until all the compiler written strings are resolved, we 
   # silence the warning. 
   target_compile_options(${TARGET_NAME} PRIVATE -Wno-write-strings) 

   target_include_directories(${TARGET_NAME} BEFORE PRIVATE 
      ${COMPILER_INCLUDES}
      )

   # TODO: Extract into platform specific section.
   target_compile_definitions(${TARGET_NAME} PRIVATE
      BITVECTOR_BIT_NUMBERING_MSB
      UT_DIRECT_TRACE_REGISTRATION
      TR_HOST_64BIT
      LINUX
      TR_HOST_X86
      TR_TARGET_X86
      TR_TARGET_64BIT
      ${COMPILER_DEFINES} 
      )
endfunction(make_compiler_target)

# Filter through the provided list, and rewrite any 
# .pasm files to .asm files, and add the .asm file to the list 
# in lieu of the .pasm file. 
#
function(pasm2asm_files out_var compiler)

   set(PASM_CMD ${CMAKE_C_COMPILER}) 
   set(PASM_FLAGS -x assembler-with-cpp -E -P) 
   set(PASM_INCLUDES ${${compiler}_INCLUDES} $ENV{J9SRC}/oti)
   add_prefix(PASM_INCLUDES "-I" ${PASM_INCLUDES})

   set(result "")
   foreach(in_f ${ARGN})
      get_filename_component(extension ${in_f} EXT)
      if (extension STREQUAL ".pasm") # Requires preprocessing 
         get_filename_component(absolute_in_f ${in_f} ABSOLUTE)

         string(REGEX REPLACE ".pasm" ".asm" out_f ${in_f})

         set(out_f "${CMAKE_CURRENT_BINARY_DIR}/${out_f}")
         get_filename_component(out_dir ${out_f} DIRECTORY)
         file(MAKE_DIRECTORY ${out_dir})

         add_custom_command(OUTPUT ${out_f}
            COMMAND ${PASM_CMD} ${PASM_FLAGS} ${PASM_INCLUDES} -o ${out_f} ${absolute_in_f}
            DEPENDS ${in_f} 
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Running pasm2asm command on ${in_f} to create ${out_f}"
            VERBATIM
            )

         list(APPEND result ${out_f})
      else() 
         list(APPEND result ${in_f}) # No change required. Not masm2gas target
      endif()
   endforeach()
   set(${out_var} "${result}" PARENT_SCOPE)
endfunction()



# Filter through the provided list, and rewrite any 
# .asm files to .s files, and add the .s file to the list 
# in lieu of the .asm file. 
#
# Inspired by this post:  https://cmake.org/pipermail/cmake/2010-June/037733.html
function(masm2gas_asm_files out_var compiler)

   # This portion should be abstracted away, and the ATT_INCLUDES below should be made 
   # into target properties to avoid polluting the rest of the info 
   set(MASM2GAS_INCLUDES ${${compiler}_INCLUDES} $ENV{J9SRC}/oti)
   add_prefix(MASM2GAS_INCLUDES "-I" ${MASM2GAS_INCLUDES})
   set(MASM2GAS_FLAGS --64 )
   set(CMAKE_ASM-ATT_INCLUDES ${MASM2GAS_INCLUDES} CACHE INTERNAL "ASM Includes") 
    
   set(result "")
   foreach(in_f ${ARGN})
      get_filename_component(extension ${in_f} EXT)
      if (extension STREQUAL ".asm") # Requires masm2gas
         get_filename_component(absolute_in_f ${in_f} ABSOLUTE)

         # From the masm2gas documentation: 
         #
         # Output file names follow these rules (.x is any extension besides these two)
         # foo.asm  -> GAS: (outdir)/foo.s           DEPEND: (outdir)/foo.d

         # Note: This support doesn't handle dependencies generated by masm2gas 
         #       unfortunately.   
         string(REGEX REPLACE ".asm" ".s" out_f ${in_f})
         set(out_f "${CMAKE_CURRENT_BINARY_DIR}/${out_f}")
         get_filename_component(out_dir ${out_f} DIRECTORY)
         file(MAKE_DIRECTORY ${out_dir})

         add_custom_command(OUTPUT ${out_f}
            COMMAND ${PERL_EXECUTABLE} ${MASM2GAS_PATH} ${MASM2GAS_FLAGS} ${MASM2GAS_INCLUDES} -o${out_dir}/ ${absolute_in_f}
            DEPENDS ${in_f} 
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Running masm2gas.pl on ${in_f} to create ${out_f}"
            VERBATIM
            )

         list(APPEND result ${out_f})
      else() 
         list(APPEND result ${in_f}) # No change required. Not masm2gas target
      endif()
   endforeach()
   set(${out_var} "${result}" PARENT_SCOPE)
endfunction()



# Create an OMR Compiler component
# 
# call like this: 
#  create_omr_compiler_library(NAME <compilername>  
#                              OBJECTS  <list of objects to add to the glue> 
#                              DEFINES  <DEFINES for building library
#                              FILTER   <list of default objects to remove from the compiler library.> 
#                              INCLUDES <Additional includes for building the library> 
#                              SHARED   <True if you want a shared object, false if you want a static archive> 
# 
# FILTER exists to allow compiler subprojects to opt-out of functionality
#        that they would prefer to replace. 
# 
function(create_omr_compiler_library)
   cmake_parse_arguments(COMPILER 
      "SHARED" # Optional Arguments
      "NAME" # One value arguments
      "OBJECTS;DEFINES;FILTER;INCLUDES" # Multi value args
      ${ARGV}
      )

   if(COMPILER_SHARED)
      message("Creating shared library for ${COMPILER_NAME}")
      set(LIB_TYPE SHARED)
   else()
      message("Creating static library for ${COMPILER_NAME}")
      set(LIB_TYPE STATIC)
   endif()


   get_filename_component(abs_root ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
   # We use the cache to allow passing information about compiler targets 
   # from function to function without having to use lots of temps. 
   set(${COMPILER_NAME}_ROOT    ${abs_root}               CACHE INTERNAL "Root for compiler ${COMPILER_NAME}")
   set(${COMPILER_NAME}_DEFINES ${COMPILER_DEFINES}       CACHE INTERNAL "Defines for compiler ${COMPILER_NAME}") 
   set(${COMPILER_NAME}_INCLUDES 
      ${${COMPILER_NAME}_ROOT}/${TR_TARGET_ARCH}/${TR_TARGET_SUBARCH}
      ${${COMPILER_NAME}_ROOT}/${TR_TARGET_ARCH} 
      ${${COMPILER_NAME}_ROOT} 
      ${OMR_ROOT}/compiler/${TR_TARGET_ARCH}/${TR_TARGET_SUBARCH}
      ${OMR_ROOT}/compiler/${TR_TARGET_ARCH} 
      ${OMR_ROOT}/compiler 
      ${OMR_ROOT}
      ${COMPILER_INCLUDES}
      CACHE INTERNAL "Include header list for ${COMPILER}")

   message("${COMPILER_NAME}_ROOT = ${${COMPILER_NAME}_ROOT}") 


   # Generate a build name file. 
   set(BUILD_NAME_FILE "${CMAKE_BINARY_DIR}/${COMPILER_NAME}Name.cpp")
   add_custom_command(OUTPUT ${BUILD_NAME_FILE}  
                     COMMAND perl ${OMR_ROOT}/tools/compiler/scripts/generateVersion.pl ${COMPILER_NAME} > ${BUILD_NAME_FILE}
                     VERBATIM
                     COMMENT "Generate ${BUILD_NAME_FILE}"
                     )

   # Convert pasm files ot asm files: run this before masm2gas to ensure 
   # asm files subsequently get picked up. 
   pasm2asm_files(COMPILER_OBJECTS ${COMPILER_NAME} ${COMPILER_OBJECTS})

   # Run masm2gas on contained .asm files 
   masm2gas_asm_files(COMPILER_OBJECTS ${COMPILER_NAME} ${COMPILER_OBJECTS})

   add_library(${COMPILER_NAME} ${LIB_TYPE} 
      ${BUILD_NAME_FILE} 
      ${COMPILER_OBJECTS}
      )  

   # Populated and then will use target_sources to append to the compiler library.
   set(CORE_COMPILER_OBJECTS "")

   # Add the contents of the macro to CORE_COMPILER_OBJECTS in this scope.  
   # The library name parameter is currently ignored.
   macro(compiler_library libraryname)
      # Since CMake *copies* the enviornment from parent to child scope, 
      # a PARENT_SCOPE set ultimately is not reflected for local lookups. 
      # As a result, we need to _also_ update the local scope here if we 
      # want this to succeed when called multiple times in a scope. 
      set(CORE_COMPILER_OBJECTS ${CORE_COMPILER_OBJECTS} ${ARGN}             )
      set(CORE_COMPILER_OBJECTS ${CORE_COMPILER_OBJECTS} ${ARGN} PARENT_SCOPE)
   endmacro(compiler_library)

   # Because we want to be able to build multiple compiler 
   # components in the same tree (ie the compiler test code, as 
   # well as JitBuilder, we can't just use the plain add_subdirectory, 
   # CMake demands we specify the object target directory as well. 
   # 
   # This macro computes an appropriate target directory.
   macro(add_compiler_subdirectory dir) 
      add_subdirectory(${OMR_ROOT}/compiler/${dir}
                       ${CMAKE_CURRENT_BINARY_DIR}/compiler/${dir}_${COMPILER_NAME})
   endmacro(add_compiler_subdirectory)


   # There's a little bit of a song and dance here 
   # I wonder if it's not better to just have a 
   # unified object list. 
   add_compiler_subdirectory(ras)
   add_compiler_subdirectory(compile)
   add_compiler_subdirectory(codegen)
   add_compiler_subdirectory(control)
   add_compiler_subdirectory(env)
   add_compiler_subdirectory(infra)
   add_compiler_subdirectory(il)
   add_compiler_subdirectory(optimizer)
   add_compiler_subdirectory(runtime)
   add_compiler_subdirectory(ilgen)

   if(${TR_TARGET_ARCH} STREQUAL "x")
      add_compiler_subdirectory(x)
   endif()


   # Filter out objects requested to be removed by 
   # client project (if any): 
   foreach(object ${COMPILER_FILTER})
      get_filename_component(abs_filename ${object} ABSOLUTE)
      list(REMOVE_ITEM CORE_COMPILER_OBJECTS ${abs_filename})
   endforeach()

   # Append to the compiler sources list
   target_sources(${COMPILER_NAME} PRIVATE ${CORE_COMPILER_OBJECTS})
  
   # Set include paths and defines.  
   make_compiler_target(${COMPILER_NAME} COMPILER ${COMPILER_NAME})
endfunction(create_omr_compiler_library)
