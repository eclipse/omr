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

#ifndef OMR_PPCCONSTANTDATASNIPPET_INCL
#define OMR_PPCCONSTANTDATASNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_PPCCONSTANTDATASNIPPET_CONNECTOR
#define OMR_PPCCONSTANTDATASNIPPET_CONNECTOR
namespace OMR { class ConstantDataSnippet; }
namespace OMR { typedef OMR::ConstantDataSnippet ConstantDataSnippetConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/Array.hpp"
#include "infra/List.hpp"

namespace TR { class Node; }

namespace OMR
{

template <class T> class PPCConstant
   {
   TR_Array<TR::Instruction *>   _instructionPairs;
   int32_t                         _tocOffset;
   T                               _value;
   TR::Node                        *_node;
   bool                            _isUnloadablePicSite;

   public:

   TR_ALLOC(TR_Memory::PPCConstant)

   PPCConstant(TR::CodeGenerator * cg, T v, TR::Node *n=NULL, bool ps=false) : _instructionPairs(cg->trMemory()), _value(v), _node(n), _isUnloadablePicSite(ps) {};

   T getConstantValue() {return _value;}
   bool isUnloadablePicSite() {return _isUnloadablePicSite;}

   TR_Array<TR::Instruction *> &getRequestors() {return _instructionPairs;}

   void addValueRequest(TR::Instruction *n0, TR::Instruction *n1, TR::Instruction *n2, TR::Instruction *n3)
      {
      if (n0 != NULL)
         {
         _instructionPairs.add(n0);
         _instructionPairs.add(n1);
         n0->setWillBePatched();
         n1->setWillBePatched();
         }

      if (n2 != NULL)
         {
         _instructionPairs.add(n2);
         _instructionPairs.add(n3);
         n2->setWillBePatched();
         n3->setWillBePatched();
         }
      }

   int32_t getTOCOffset() {return _tocOffset;}
   void setTOCOffset(int32_t o) {_tocOffset = o;}
   TR::Node *getNode() { return _node; }
   };


class ConstantDataSnippet
   {
   List< PPCConstant<double> > _doubleConstants;
   List< PPCConstant<float> > _floatConstants;
   List< PPCConstant<intptr_t> > _addressConstants;
   uint8_t *_snippetBinaryStart;
   TR::CodeGenerator *_cg;

   public:

   TR_ALLOC(TR_Memory::PPCConstantDataSnippet)

   ConstantDataSnippet(TR::CodeGenerator *cg) : _cg(cg), _doubleConstants(cg->trMemory()),
        _floatConstants(cg->trMemory()), _addressConstants(cg->trMemory())
      {
      };

   uint8_t *getSnippetBinaryStart() {return _snippetBinaryStart;}
   uint8_t *setSnippetBinaryStart(uint8_t *p) {return _snippetBinaryStart=p;}

   int32_t addConstantRequest(void              *v,
                           TR::DataType       type,
                           TR::Instruction *nibble0,
                           TR::Instruction *nibble1,
                           TR::Instruction *nibble2,
                           TR::Instruction *nibble3,
                           TR::Node *node,
                           bool isUnloadablePicSite);

   void emitFloatingPointConstant(
      TR_Array<TR::Instruction *> &requestors,
      uint8_t *codeCursor,
      int32_t count);

   virtual void emitAddressConstant(
      PPCConstant<intptr_t> *acursor,
      TR_Array<TR::Instruction *> &requestors,
      uint8_t *codeCursor,
      int32_t count);

   bool getRequestorsFromNibble(TR::Instruction *nibble, TR::Instruction **q, bool remove);

   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength();

   TR::CodeGenerator *cg() {return _cg;}

#ifdef DEBUG
   virtual void print(TR::FILE *outFile);
#endif

   };

}

#endif
