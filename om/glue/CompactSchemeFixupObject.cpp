/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "objectdescription.h"
#include "omr.h"

#include "CompactSchemeFixupObject.hpp"
#include "EnvironmentStandard.hpp"

#if defined(OMR_GC_MODRON_COMPACTION)

void
MM_CompactSchemeFixupObject::fixupObject(MM_EnvironmentStandard* env, omrobjectptr_t objectPtr)
{
#error provide an implementation to fix objects during a compact
}

void
MM_CompactSchemeFixupObject::verifyForwardingPtr(
	omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr)
{
#error provide an implementation to verify objects after a compact is complete
}

#endif /* OMR_GC_MODRON_COMPACTION */
