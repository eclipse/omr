/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef PPCAOTRELOCATION_INCL
#define PPCAOTRELOCATION_INCL

#include <stdint.h>
#include "codegen/Relocation.hpp"
#include "env/TRMemory.hpp"
#include "runtime/Runtime.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class PPCRelocation
   {
   public:
   TR_ALLOC(TR_Memory::PPCRelocation)

   PPCRelocation(TR::Instruction *src,
                 uint8_t           *trg,
       TR_ExternalRelocationTargetKind k):
      _srcInstruction(src), _relTarget(trg), _kind(k)
      {}

   TR::Instruction *getSourceInstruction() {return _srcInstruction;}
   void setSourceInstruction(TR::Instruction *i) {_srcInstruction = i;}

   uint8_t *getRelocationTarget() {return _relTarget;}
   void setRelocationTarget(uint8_t *t) {_relTarget = t;}

   TR_ExternalRelocationTargetKind getKind() {return _kind;}
   void setKind(TR_ExternalRelocationTargetKind k) {_kind = k;}

   virtual void mapRelocation(TR::CodeGenerator *cg) = 0;

   private:
   TR::Instruction               *_srcInstruction;
   uint8_t                         *_relTarget;
   TR_ExternalRelocationTargetKind  _kind;
   };

class PPCPairedRelocation: public TR::PPCRelocation
   {
   public:
   PPCPairedRelocation(TR::Instruction *src1,
                       TR::Instruction *src2,
                       uint8_t           *trg,
                       TR_ExternalRelocationTargetKind k,
                       TR::Node *node) :
      TR::PPCRelocation(src1, trg, k), _src2Instruction(src2), _node(node)
      {}

   TR::Instruction *getSource2Instruction() {return _src2Instruction;}
   void setSource2Instruction(TR::Instruction *src) {_src2Instruction = src;}
   TR::Node* getNode(){return _node;}
   virtual void mapRelocation(TR::CodeGenerator *cg);

   private:
   TR::Instruction               *_src2Instruction;
   TR::Node                         *_node;
   };

// This class has nothing to do with AOT, but for the lack of more appropriate locations
// this class is defined here.
//
// PPC Specific Relocation: update the immediate fields of a pair of ppc instructions
// with the absolute address of a label (extended to 64bit as well).  For example:
//   lis  gr3, addr_hi
//   addi gr3, gr3, addr_lo
//
class PPCPairedLabelAbsoluteRelocation : public TR::LabelRelocation
   {
   public:
   PPCPairedLabelAbsoluteRelocation(TR::Instruction *src1,
                                    TR::Instruction *src2,
                                    TR::Instruction *src3,
                                    TR::Instruction *src4,
                                    TR::LabelSymbol *label)
      : TR::LabelRelocation(0, label), _instr1(src1), _instr2(src2), _instr3(src3), _instr4(src4) {}
   virtual void apply(TR::CodeGenerator *cg);

   private:
   TR::Instruction *_instr1;
   TR::Instruction *_instr2;
   TR::Instruction *_instr3;
   TR::Instruction *_instr4;
   };

}

typedef TR::PPCRelocation TR_PPCRelocation;
typedef TR::PPCPairedRelocation TR_PPCPairedRelocation;
typedef TR::PPCPairedLabelAbsoluteRelocation TR_PPCPairedLabelAbsoluteRelocation;

#endif
