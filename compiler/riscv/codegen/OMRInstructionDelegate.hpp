/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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

#ifndef OMR_RV_INSTRUCTIONDELEGATE_INCL
#define OMR_RV_INSTRUCTIONDELEGATE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTRUCTIONDELEGATE_CONNECTOR
#define OMR_INSTRUCTIONDELEGATE_CONNECTOR

namespace OMR {
namespace RV {
class InstructionDelegate;
}

typedef OMR::RV::InstructionDelegate InstructionDelegateConnector;
} // namespace OMR
#else
#error OMR::RV::InstructionDelegate expected to be a primary connector, but an OMR connector is already defined
#endif

#include "codegen/MemoryReference.hpp"
#include "compiler/codegen/OMRInstructionDelegate.hpp"
#include "infra/Annotations.hpp"

namespace TR {
class JtypeInstruction;
class LoadInstruction;
class StoreInstruction;
} // namespace TR

namespace OMR { namespace RV {

class OMR_EXTENSIBLE InstructionDelegate : public OMR::InstructionDelegate {
protected:
    InstructionDelegate() {}

public:
    /**
     * @brief Sets the return address to CallSnippet for Label target
     * @param[in] cg : CodeGenerator
     * @param[in] ins : instruction associated with CallSnippet
     * @param[in] cursor : instruction cursor
     */
    static void encodeBranchToLabel(TR::CodeGenerator *cg, TR::JtypeInstruction *ins, uint8_t *cursor)
    {
        // Do nothing in OMR
    }

    /**
     * @brief Determines if this instruction will throw an implicit null pointer exception and sets appropriate flags
     * @param[in] cg    : CodeGenerator
     * @param[in] instr : instruction with memory reference
     */
    static void setupImplicitNullPointerException(TR::CodeGenerator *cg, TR::LoadInstruction *instr)
    {
        // Do nothing in OMR
    }

    /**
     * @brief Determines if this instruction will throw an implicit null pointer exception and sets appropriate flags
     * @param[in] cg    : CodeGenerator
     * @param[in] instr : instruction with memory reference
     */
    static void setupImplicitNullPointerException(TR::CodeGenerator *cg, TR::StoreInstruction *instr)
    {
        // Do nothing in OMR
    }
};

}} // namespace OMR::RV

#endif
