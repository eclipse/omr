/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef IA32RESTARTSNIPPET_INCL
#define IA32RESTARTSNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "infra/Assert.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86Ops_inlines.hpp"

namespace TR { class Node; }

namespace TR {

class X86RestartSnippet  : public TR::Snippet
   {
   TR::LabelSymbol *_restartLabel;
   bool            _forceLongRestartJump;

   public:

   X86RestartSnippet(TR::CodeGenerator *cg,
                     TR::Node * n,
                     TR::LabelSymbol *restartlab,
                     TR::LabelSymbol *snippetlab,
                     bool            isGCSafePoint)
      : TR::Snippet(cg, n, snippetlab, isGCSafePoint),
        _restartLabel(restartlab), _forceLongRestartJump(false) {}

   virtual Kind getKind() { return IsRestart; }

   TR::LabelSymbol *getRestartLabel()                  {return _restartLabel;}
   TR::LabelSymbol *setRestartLabel(TR::LabelSymbol *l) {return (_restartLabel = l);}

   void setForceLongRestartJump() {_forceLongRestartJump = true;}
   bool getForceLongRestartJump() {return _forceLongRestartJump;}

   uint8_t *genRestartJump(TR_X86OpCodes branchOp, uint8_t *bufferCursor, TR::LabelSymbol *label)
      {
      TR_X86OpCode  opcode(branchOp);

      uint8_t *destination = label->getCodeLocation();
      intptr_t  distance    = destination - (bufferCursor + 2);

      TR_ASSERT((branchOp >= JA4) && (branchOp <= JMP4),
             "opcode must be a long branch for conditional restart in a restart snippet\n");

      if (getForceLongRestartJump())
         {
          bufferCursor = opcode.binary(bufferCursor);
          *(int32_t *)bufferCursor = (int32_t)(destination - (bufferCursor + 4));
          bufferCursor += 4;
         }
      else
         {
         if (distance >= -128 && distance <= 127)
            {
            opcode.convertLongBranchToShort();
            bufferCursor = opcode.binary(bufferCursor);
            *bufferCursor = (int8_t)(destination - (bufferCursor + 1));
            bufferCursor++;
            }
         else
            {
            bufferCursor = opcode.binary(bufferCursor);
            *(int32_t *)bufferCursor = (int32_t)(destination - (bufferCursor + 4));
            bufferCursor += 4;
            }
         }
      return bufferCursor;
      }

   uint8_t *genRestartJump(uint8_t *bufferCursor, TR::LabelSymbol *label)
      {
      return genRestartJump(JMP4, bufferCursor, label);
      }

   uint8_t *genRestartJump(uint8_t *bufferCursor)
      {
      return genRestartJump(bufferCursor, _restartLabel);
      }

   uint32_t estimateRestartJumpLength(TR_X86OpCodes  branchOp,
                                      int32_t         estimatedSnippetLocation,
                                      TR::LabelSymbol *label)
      {
      intptr_t location = label->getEstimatedCodeLocation();
      if (label->getCodeLocation() != 0)
         {
         location = label->getCodeLocation() - cg()->getBinaryBufferStart();
         }
      intptr_t distance = location - (estimatedSnippetLocation + 2); // 2 is size of short branch
      if (distance >= -128 && distance <= 127 && !getForceLongRestartJump())
         {
         return 2;
         }
      // long branch required
      if (branchOp == JMP4)
         return 5;
      else
         return 6;
      }

   uint32_t estimateRestartJumpLength(int32_t estimatedSnippetLocation, TR::LabelSymbol *label)
      {
      return estimateRestartJumpLength(JMP4, estimatedSnippetLocation, label);
      }

   uint32_t estimateRestartJumpLength(int32_t estimatedSnippetLocation)
      {
      return estimateRestartJumpLength(estimatedSnippetLocation, _restartLabel);
      }

   uint32_t estimateRestartJumpLength(TR_X86OpCodes branchOp, int32_t estimatedSnippetLocation)
      {
      return estimateRestartJumpLength(branchOp, estimatedSnippetLocation, _restartLabel);
      }

   };

}

#endif
