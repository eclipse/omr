#!/bin/bash
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

set -evx

export JOBS=4

if test "x$BUILD_WITH_CMAKE" = "xyes"; then
  mkdir build
  cd build
  time cmake -Wdev -GNinja -C../cmake/caches/Travis.cmake ..
  if test "x$RUN_BUILD" != "xno"; then
    time ninja -j$JOBS  
    if test "x$RUN_TESTS" != "xno"; then
      time ctest -V --parallel $JOBS
    fi
  fi
else
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
