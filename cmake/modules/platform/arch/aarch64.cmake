###############################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
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

list(APPEND OMR_PLATFORM_DEFINITIONS
	-DAARCH64
	-DJ9AARCH64
	-DAARCH64GNU
	-DFIXUP_UNALIGNED
	-march=armv8-a+simd
	-Wno-unused-but-set-variable
)

set(TR_HOST_ARCH aarch64)
set(TR_HOST_BITS 64)

list(APPEND TR_COMPILE_DEFINITIONS TR_HOST_ARM64 TR_TARGET_ARM64 TR_HOST_64BIT TR_TARGET_64BIT BITVECTOR_64BIT)
