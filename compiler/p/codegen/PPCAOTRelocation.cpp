/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "p/codegen/PPCAOTRelocation.hpp"

#include <stddef.h>                         // for NULL
#include <stdint.h>                         // for int32_t
#include "codegen/CodeGenerator.hpp"        // for CodeGenerator
#include "codegen/Instruction.hpp"          // for Instruction
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"          // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                   // for intptrj_t
#include "il/symbol/LabelSymbol.hpp"        // for LabelSymbol
#include "runtime/Runtime.hpp"              // for LO_VALUE

void TR::PPCPairedRelocation::mapRelocation(TR::CodeGenerator *cg)
   {
   if (cg->comp()->getOption(TR_AOT))
      {
      cg->addAOTRelocation(
         new (cg->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(
            getSourceInstruction()->getBinaryEncoding(),
            getSource2Instruction()->getBinaryEncoding(),
            getRelocationTarget(),
            getKind(), cg),
         __FILE__,
         __LINE__,
         getNode());
      }
   }


void TR::PPCPairedLabelAbsoluteRelocation::apply(TR::CodeGenerator *cg)
   {
   intptrj_t p = (intptrj_t)getLabel()->getCodeLocation();

   if (TR::Compiler->target.is32Bit())
      {
      _instr1->updateImmediateField(cg->hiValue(p) & 0x0000ffff);
      _instr2->updateImmediateField(LO_VALUE(p) & 0x0000ffff);
      }
   else
      {
      if (_instr4->getMemoryReference() != NULL)
	 {
         // We should add an updateImmediateField method to Mem instruction classes instead
         int32_t *cursor = (int32_t *)_instr4->getBinaryEncoding();
         *cursor |= LO_VALUE(p) & 0x0000ffff;
	 }
      else
         _instr4->updateImmediateField(LO_VALUE(p) & 0x0000ffff);
      p = cg->hiValue(p);
      _instr1->updateImmediateField((p>>32) & 0x0000ffff);
      _instr2->updateImmediateField((p>>16) & 0x0000ffff);
      _instr3->updateImmediateField(p & 0x0000ffff);
      }
   }
