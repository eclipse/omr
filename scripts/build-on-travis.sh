#!/bin/bash
###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

set -evx

### Environment Variables:
### BUILD : one of om, jitbuilder, example, example-legacy
### PLATFORM: one of default, x84, x86-64, p, 

function main
{
  set_defaults
  setup_for_platform
  build
}

function set_defaults
{
  export BUILD="${BUILD:-example}"
  export PLATFORM="${PLATFORM:-${TRAVIS}}"
  export BUILD_JOBS=2
  # Disable the core dump tests as container based builds don't allow setting
  # core_pattern and don't have apport installed.  This can be removed when
  # apport is available in apt whitelist
  export GTEST_FILTER="-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended"
  export CMAKE_GENERATOR="Ninja"
}

function setup_for_platform
{
  case "$PLATFORM" in
  linux_x86)
  linux_x86-64)
  linux_p)
  linux_z)
  da
  default)
}

function setup_for_spec
{
  case "SPEC" in
  "linux_aarch64")
    setup_for
  "osx")
    setup_for_osx
    ;;
  "linux_x86")
    setup_for_linux_x86
    ;;
  "linux_x86-64")
    setup_for_linux_x86-64
    ;;
  "*")
    ;;
  esac
}

function setup_for_osx
{
  export GTEST_FILTER="$GTEST_FILTER:*DeathTest*"
  brew install ninja
}

function setup_for_linux_aarch64
{
  #TODO:ARM64: Setup DDR Support for AArch64
  export EXTRA_CONFIGURE_ARGS="--disable-DDR"

  #TODO:ARM64: Test AArch64 with QEMU?
  export RUN_TESTS="no"

  # Get the toolchain
  wget https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/aarch64-linux-gnu/gcc-linaro-4.9.4-2017.01-x86_64_aarch64-linux-gnu.tar.xz

  tar xf gcc-linaro-4.9.4-2017.01-x86_64_aarch64-linux-gnu.tar.xz

  export PATH="`pwd`/gcc-linaro-4.9.4-2017.01-x86_64_aarch64-linux-gnu/bin:${PATH}"
}

function build
{
  case "$BUILD" in
  "example")
    build_example
    ;;
  "example-legacy")
    build_example_legacy
    ;;
  "om")
    build_om
    ;;
  "jitbuilder")
    build_jitbuilder
    ;;
  "lint")
    run_linter
    ;;
  *)
    echo "Unknown build type"
    exit 1
    ;;
  esac
}

function setup_cmake_build_dir
{
  mkdir build
  cd build
  cmake .. -G"$CMAKE_GENERATOR" -DCMAKE_BUILD_TYPE=Debug "$@"
}

function build_jitbuilder
{
  mkdir build
  cd build
  time cmake .. -DCMAKE_BUILD_TYPE=Debug -DOMR_JITBUILDER=ON -DOMR_EXAMPLE=OFF -DOMR_GC=OFF
  time cmake --build .
  time ctest --output-on-failure
}

function build_om
{
  mkdir build
  cd build
  time cmake .. -DCMAKE_BUILD_TYPE=Debug -DOMR_OM=ON -DOMR_GC=ON -DOMR_JITBUILDER=OFF -DOMR_COMPILER=OFF -DOMR_EXAMPLE=OFF -DOMR_DDR=OFF -DOMR_OMRSIG=OFF
  time cmake --build .
  time ctest --output-on-failure
}

function build_example
{
  mkdir build
  cd build
  time cmake -Wdev -G "$CMAKE_GENERATOR" $CMAKE_DEFINES -C../cmake/caches/Travis.cmake ..
  time cmake --build .
  time ctest --output-on-failure
}

function build_example_legacy
{
  time make -f run_configure.mk OMRGLUE=./example/glue SPEC="$SPEC" PLATFORM="$PLATFORM"
  time make --jobs $BUILD_JOBS
  time make test
}

function run_linter
{
  time make -f run_configure.mk OMRGLUE=./example/glue SPEC="$SPEC" PLATFORM="$PLATFORM"
  time make lint
}

main
