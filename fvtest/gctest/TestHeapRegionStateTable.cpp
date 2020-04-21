/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "HeapRegionStateTable.hpp"
#include "gcTestHelpers.hpp"

#include <Forge.hpp>

#include <gtest/gtest.h>

using namespace OMR::GC;

TEST(TestHeapRegionStateTable, HeapRegionStateTable)
{
    uintptr_t heapBase = 0x100;
    uintptr_t regionShift = 1;
    uintptr_t regionCount = 3;

    Forge forge;
    ASSERT_TRUE(forge.initialize(gcTestEnv->getPortLibrary()));

    HeapRegionStateTable table;
    ASSERT_TRUE(table.initialize(&forge, heapBase, regionShift, regionCount));

    EXPECT_EQ(table.getIndex((void *)0x100), 0);
    EXPECT_EQ(table.getIndex((void *)0x101), 0);
    EXPECT_EQ(table.getIndex((void *)0x102), 1);
    EXPECT_EQ(table.getIndex((void *)0x103), 1);
    EXPECT_EQ(table.getIndex((void *)0x104), 2);
    EXPECT_EQ(table.getIndex((void *)0x105), 2);

    EXPECT_EQ(table.getRegionState((void *)0x101), HEAP_REGION_STATE_NONE);
    EXPECT_EQ(table.getRegionState((void *)0x102), HEAP_REGION_STATE_NONE);
    EXPECT_EQ(table.getRegionState((void *)0x103), HEAP_REGION_STATE_NONE);
    EXPECT_EQ(table.getRegionState((void *)0x104), HEAP_REGION_STATE_NONE);
    
    table.setRegionState((void *)0x103, HEAP_REGION_STATE_COPY_FORWARD);

    EXPECT_EQ(table.getRegionState((void *)0x101), HEAP_REGION_STATE_NONE);
    EXPECT_EQ(table.getRegionState((void *)0x102), HEAP_REGION_STATE_COPY_FORWARD);
    EXPECT_EQ(table.getRegionState((void *)0x103), HEAP_REGION_STATE_COPY_FORWARD);
    EXPECT_EQ(table.getRegionState((void *)0x104), HEAP_REGION_STATE_NONE);

    table.tearDown(&forge);
    forge.tearDown();
}
