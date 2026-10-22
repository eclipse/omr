/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/InstOpCode.hpp"

const char *BranchConditionToNameMap[TR::InstOpCode::S390NumBranchConditions]
    = { "NOPR", "NOP", "VGNOP", "BRO", "BRH", "BRP", "BRL", "BRM", "BRNE", "BRNZ", "BRE", "BRZ", "BRNH", "BRNL", "BRNM",
          "BRNP", "BRNO", "B", "BR", "BRU", "BRUL", "BC", "BCR", "BE", "BER", "BH", "BHR", "BL", "BLR", "BM", "BMR",
          "BNE", "BNER", "BNH", "BNHR", "BNL", "BNLR", "BNM", "BNMR", "BNO", "BNOR", "BNP", "BNPR", "BNZ", "BNZR", "BO",
          "BOR", "BP", "BPR", "BRC", "BZ", "BZR", "MASK0", "MASK1", "MASK2", "MASK3", "MASK4", "MASK5", "MASK6",
          "MASK7", "MASK8", "MASK9", "MASK10", "MASK11", "MASK12", "MASK13", "MASK14", "MASK15" };
