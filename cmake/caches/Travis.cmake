###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

set(OMR_COMPILER ON CACHE BOOL "")
set(OMR_DDR ON CACHE BOOL "")
set(OMR_TEST_COMPILER ON CACHE BOOL "")
set(OMR_JITBUILDER ON CACHE BOOL "")

set(OMR_GC_SEGREGATED_HEAP ON CACHE BOOL "")
set(OMR_GC_MODRON_SCAVENGER ON CACHE BOOL "")
set(OMR_GC_MODRON_CONCURRENT_MARK ON CACHE BOOL "")
set(OMR_GC_VLHGC ON CACHE BOOL "")
set(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD ON CACHE BOOL "")
set(OMR_PORT_SOCKET_SUPPORT ON CACHE BOOL "")

set(OMR_NOTIFY_POLICY_CONTROL ON CACHE BOOL "")
set(OMR_THR_CUSTOM_SPIN_OPTIONS ON CACHE BOOL "")
set(OMR_THR_FORK_SUPPORT ON CACHE BOOL "")
set(OMR_THR_SPIN_WAKE_CONTROL ON CACHE BOOL "")
set(OMR_THR_THREE_TIER_LOCKING ON CACHE BOOL "")
