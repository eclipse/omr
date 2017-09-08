###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

macro(create_jitbuilder_test target) 
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} 
        jitbuilder
        ${CMAKE_DL_LIBS}) 
    add_test(NAME ${target}_example_as_test COMMAND ${target}) 
endmacro(create_jitbuilder_test) 

# Basic Tests: These should run properly on all platforms. 
create_jitbuilder_test(conditionals    src/Conditionals.cpp) 
create_jitbuilder_test(isSupportedType src/IsSupportedType.cpp)
create_jitbuilder_test(iterfib         src/IterativeFib.cpp)
create_jitbuilder_test(nestedloop      src/NestedLoop.cpp)
create_jitbuilder_test(pow2            src/Pow2.cpp)
create_jitbuilder_test(simple          src/Simple.cpp) 
create_jitbuilder_test(toiltype        src/ToIlType.cpp)
create_jitbuilder_test(worklist        src/Worklist.cpp)

# Additional Tests: These may not run properly on all platforms
# Opt in by setting OMR_JITBUILDER_ADDITIONAL
if(OMR_JITBUILDER_ADDITIONAL) 
    create_jitbuilder_test(call              src/Call.cpp) 
    create_jitbuilder_test(conststring       src/ConstString.cpp) 
    create_jitbuilder_test(dotproduct        src/DotProduct.cpp) 
    create_jitbuilder_test(fieldaddress      src/FieldAddress.cpp) 
    create_jitbuilder_test(linkedlist        src/LinkedList.cpp) 
    create_jitbuilder_test(localarray        src/LocalArray.cpp) 
    create_jitbuilder_test(operandarraytests src/OperandArrayTests.cpp) 
    create_jitbuilder_test(operandstacktests src/OperandStackTests.cpp) 
    create_jitbuilder_test(pointer           src/Pointer.cpp) 
    create_jitbuilder_test(recfib            src/RecursiveFib.cpp) 
    create_jitbuilder_test(structArray       src/StructArray.cpp) 
    create_jitbuilder_test(switch            src/Switch.cpp) 
    create_jitbuilder_test(union             src/Union.cpp) 
endif()

# Experimental Tests: These are still on the experimental side 
# Opt in by setting OMR_JITBUILDER_EXPERIMENTAL
if(OMR_JITBUILDER_EXPERIMENTAL) 
    create_jitbuilder_test(atomicoperations             src/AtomicOperations.cpp) 
    create_jitbuilder_test(transactionaloperations      src/TransactionalOperations.cpp) 
endif()


# Additional Tests: These may not run properly on all platforms
# Mandelbrot takes arguments for its test 
# so will require we enhance create_jitbuilder_target. 
#create_jitbuilder_target(mandelbrot   src/Mandelbrot.cpp) 
