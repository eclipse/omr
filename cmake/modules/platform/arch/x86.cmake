###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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

if(OMR_ENV_DATA64)
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DJ9HAMMER
		-D_AMD64_
	)
else()
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DJ9X86
		-D_X86_
	)
endif()

# Testarossa build variables. Longer term the distinction between TR and the rest
# of the OMR code should be heavily reduced. In the meantime, we keep the distinction.
list(APPEND TR_COMPILE_DEFINITIONS -DTR_HOST_X86 -DTR_TARGET_X86)

set(TR_HOST_ARCH x)

if(OMR_ENV_DATA64)
	list(APPEND TR_COMPILE_DEFINITIONS -DTR_HOST_64BIT -DTR_TARGET_64BIT -DBITVECTOR_64BIT)

	set(TR_HOST_SUBARCH amd64)
	set(TR_HOST_BITS 64)
else()
	list(APPEND TR_COMPILE_DEFINITIONS -DTR_HOST_32BIT -DTR_TARGET_32BIT)

	set(TR_HOST_SUBARCH i386)
	set(TR_HOST_BITS 32)
endif()
