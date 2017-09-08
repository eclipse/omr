#!/bin/bash
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

set -evx

export JOBS=2

if test "x$CMAKE_GENERATOR" = "x"; then
  export CMAKE_GENERATOR="Ninja"
fi

if test "x$BUILD_WITH_CMAKE" = "xyes"; then

  if test "x$TRAVIS_OS_NAME" = "xosx" && test "x$CMAKE_GENERATOR" = "xNinja"; then
    brew install ninja
  fi

  mkdir build
  cd build
  time cmake -Wdev -G "$CMAKE_GENERATOR" -C../cmake/caches/Travis.cmake ..
  if test "x$RUN_BUILD" != "xno"; then
    time cmake --build . -- -j $JOBS
    if test "x$RUN_TESTS" != "xno"; then
      time ctest -V
    fi
  fi
else
  # Disable ddrgen on 32 bit builds--libdwarf in 32bit is unavailable.
  if test "x$SPEC" = "xlinux_x86"; then
    export EXTRA_CONFIGURE_ARGS="--disable-DDR"
  else
    export EXTRA_CONFIGURE_ARGS="--enable-DDR"
  fi
  time make -f run_configure.mk OMRGLUE=./example/glue SPEC="$SPEC" PLATFORM="$PLATFORM"
  if test "x$RUN_BUILD" != "xno"; then
    # Normal build system
    time make --jobs $JOBS
    if test "x$RUN_TESTS" != "xno"; then
      time make --jobs $JOBS test
    fi
  fi
  if test "x$RUN_LINT" = "xyes"; then
    llvm-config --version
    clang++ --version

    # Run linter for x86 target
    time make --jobs $JOBS lint

    # Run linter for p and z targets
    export TARGET_ARCH=p
    export TARGET_BITS=64
    time make --jobs $JOBS lint

    export TARGET_ARCH=z
    export TARGET_BITS=64
    time make --jobs $JOBS lint
  fi
fi
