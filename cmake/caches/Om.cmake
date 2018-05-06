###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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

# A cache for enabling Om in OMR.
# For more information, see :/om/README.md

set(OMR_JIT  ON CACHE BOOL "")
set(OMR_JITBUILDER ON CACHE BOOL "")
set(OMR_GC ON CACHE BOOL "")
set(OMR_OM ON CACHE BOOL "")
set(OMR_PORT ON CACHE BOOL "")
set(OMR_THREAD ON CACHE BOOL "")


# Disable (temporarily) unsupported GC policies

set(OMR_GC_SEGREGATED_HEAP OFF CACHE BOOL "")
set(OMR_GC_MODRON_SCAVENGER OFF CACHE BOOL "")
set(OMR_GC_MODRON_CONCURRENT_MARK OFF CACHE BOOL "")

# Default-on options

set(OMR_NOTIFY_POLICY_CONTROL ON CACHE BOOL "")
set(OMR_THR_CUSTOM_SPIN_OPTIONS ON CACHE BOOL "")
set(OMR_THR_FORK_SUPPORT ON CACHE BOOL "")
set(OMR_THR_SPIN_CODE_REFACTOR ON CACHE BOOL "")
set(OMR_THR_SPIN_WAKE_CONTROL ON CACHE BOOL "")
set(OMR_THR_THREE_TIER_LOCKING ON CACHE BOOL "")
set(OMR_WARNINGS_AS_ERRORS ON CACHE BOOL "")

# Disable unrelated OMR elements

set(OMR_DDR OFF CACHE BOOL "Enable DDR")
set(OMR_OMRSIG OFF CACHE BOOL "")
set(OMR_FVTEST OFF CACHE BOOL "")
set(OMR_EXAMPLE OFF CACHE BOOL "")
set(OMR_TEST_COMPILER OFF CACHE BOOL "")