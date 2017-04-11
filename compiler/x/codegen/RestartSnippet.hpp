/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef IA32RESTARTSNIPPET_INCL
#define IA32RESTARTSNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>                   // for int32_t, uint8_t, uint32_t, etc
#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "env/jittypes.h"             // for intptrj_t
#include "il/symbol/LabelSymbol.hpp"  // for LabelSymbol
#include "infra/Assert.hpp"           // for TR_ASSERT
#include "x/codegen/X86Ops.hpp"       // for TR_X86OpCode, ::JMP4, etc
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
      intptrj_t  distance    = destination - (bufferCursor + 2);

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
      intptrj_t location = label->getEstimatedCodeLocation();
      if (label->getCodeLocation() != 0)
         {
         location = label->getCodeLocation() - cg()->getBinaryBufferStart();
         }
      intptrj_t distance = location - (estimatedSnippetLocation + 2); // 2 is size of short branch
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
