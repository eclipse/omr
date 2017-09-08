#! /usr/bin/sh
###############################################################################
# Copyright (c) 2016, 2016 IBM Corp. and others
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

# This script builds OMRChecker using the default c++ compiler.
# It then runs some tests on the build. If one of these fails,
# a new build is attempted  with g++ explicitly set as the c++
# compiler. The tests are then run once again. If a failure is
# again detected, a non-zero value is returned.
#
# This script was created to help avoid problems when
# running OMRChecker as part of an automated build system.
#
# On certain platforms, when OMRChecker is compiled with an old
# version of clang (e.g. clang-3.4), it fails to catch errors.
# The current fix is to compile OMRChecker with gcc instead.
# This script can be used to avoid accidently running
# a malfunctioning build of OMRChecker whil.

echo "Attempting to build OMRChecker normally."

make cleanall
make OMRCHECKER_DIR=$PWD
python test.py --checker $PWD/OMRChecker.so

if [ $? -eq 0 ]; then
   exit 0
fi

echo "Build failed. Attempting to rebuild OMRChecker with g++."

make cleanall
make OMRCHECKER_DIR=$PWD CXX=g++
python test.py --checker $PWD/OMRChecker.so

exit $?
