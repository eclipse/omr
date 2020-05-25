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

#include "arm/codegen/ConstantDataSnippet.hpp"

#include "env/FrontEnd.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "runtime/Runtime.hpp"

int32_t TR::ARMConstantDataSnippet::addConstantRequest(void              *v,
                                                  TR::DataType       type,
                                                  TR::Instruction *nibble0,
                                                  TR::Instruction *nibble1,
                                                  TR::Instruction *nibble2,
                                                  TR::Instruction *nibble3,
                                                  TR::Instruction *nibble4,
                                                  TR::Node *node,
                                                  bool isUnloadablePicSite)
   {
   TR::Compilation *comp = cg()->comp();

   intptr_t   ain, aex;

   int32_t ret = 0;

   switch(type)
      {
      case TR::Address:
         {
         ListIterator< TR::ARMConstant<intptr_t> >  aiterator(&_addressConstants);
         TR::ARMConstant<intptr_t>                 *acursor=aiterator.getFirst();

         ain = *(intptr_t *)v;
         while (acursor != NULL)
            {
             aex = acursor->getConstantValue();
             // if pointers require relocation, then not all pointers may be relocated for the same reason
             //   so be conservative and do not combine them (e.g. HCR versus profiled inlined site enablement)
             if (ain == aex &&
                 (!cg()->profiledPointersRequireRelocation() || acursor->getNode() == node))
                break;
             acursor = aiterator.getNext();
             }
         if (acursor && acursor->isUnloadablePicSite()!=isUnloadablePicSite)
            {
            TR_ASSERT(0, "Existing address constant does not have a matching unloadable state.\n" );
            acursor = NULL; // If asserts are turned off then we should just create a duplicate constant
            }
         if (acursor == NULL)
            {
            acursor = new (_cg->trHeapMemory()) TR::ARMConstant<intptr_t>(_cg, ain, node, isUnloadablePicSite);
            _addressConstants.add(acursor);
            }
            acursor->addValueRequest(nibble0, nibble1, nibble2, nibble3);
         }
         break;
      default:
         TR_ASSERT(0, "Only address constants are supported. Data type is %d.\n", type);
      }

   return(ret);
   }

bool TR::ARMConstantDataSnippet::getRequestorsFromNibble(TR::Instruction* nibble, TR::Instruction **q, bool remove)
   {
   int32_t count = cg()->comp()->target().is64Bit()?4:2;
   ListIterator< TR::ARMConstant<intptr_t> >  aiterator(&_addressConstants);
   TR::ARMConstant<intptr_t>               *acursor=aiterator.getFirst();
   while (acursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = acursor->getRequestors();
      if (requestors.size() > 0)
         {
         for (int32_t i = 0; i < requestors.size(); i+=count)
            {
            if (count == 2)
               {
               if (requestors[i] == nibble || requestors[i+1] == nibble)
                  {
                  q[0] = requestors[i];
                  q[1] = requestors[i+1];
                  q[2] = NULL;
                  q[3] = NULL;
                  if (remove)
                     {
                     requestors.remove(i+1);
                     requestors.remove(i);
                     }
                  return true;
                  }
               }
            else // count == 4
               {
               int32_t j = 0;
               if (requestors[i] == nibble || requestors[i+1] == nibble || requestors[i+2] == nibble || requestors[i+3] == nibble)
                  {
                  for (j = 0; j < count; j++)
                      q[j] = requestors[i+j];
                  if (remove)
                     {
                     for (j = count-1; j >= 0 ; j--)
                         requestors.remove(i+j);
                     }
                  return true;
                  }
               }
            }
         }
      acursor = aiterator.getNext();
      }
   return false;
   }

uint8_t *TR::ARMConstantDataSnippet::emitSnippetBody()
   {
   uint8_t   *codeCursor = cg()->getBinaryBufferCursor();
   uint8_t   *iloc1, *iloc2, *iloc3, *iloc4;
   int32_t    i, size, count;
   intptr_t  addr;
   double     dconv;
   uint64_t   i64;
   float      fconv;
   uint32_t   i32;

   TR::Compilation *comp = cg()->comp();
   setSnippetBinaryStart(codeCursor);
   count = 4;

#if 0
   ListIterator< TR::ARMConstant<double> >  diterator(&_doubleConstants);
   TR::ARMConstant<double>                 *dcursor=diterator.getFirst();
   while (dcursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = dcursor->getRequestors();
      size = requestors.size();
      if (size > 0)
         {
         *(double *)codeCursor = dcursor->getConstantValue();

         TR_ASSERT(size%count == 0, "Requestors are paired.\n");
         for (i=0; i<size; i+=count)
            {
            iloc1 = requestors[i]->getBinaryEncoding();
            iloc2 = requestors[i+1]->getBinaryEncoding();
            addr = (intptr_t)codeCursor;
            if (count==4)
               {
               iloc3 = requestors[i+2]->getBinaryEncoding();
               iloc4 = requestors[i+3]->getBinaryEncoding();

               if (!cg()->comp()->compileRelocatableCode())
                  {
                  *(int32_t *)iloc4 |= LO_VALUE(addr) & 0x0000ffff;
                  addr = cg()->hiValue(addr);
                  *(int32_t *)iloc3 |= addr & 0x0000ffff;
                  *(int32_t *)iloc2 |= (addr>>16) & 0x0000ffff;
                  *(int32_t *)iloc1 |= (addr>>32) & 0x0000ffff;
                  }
               else
                  {
                  cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(requestors[i],
                                                                                               (uint8_t *)(addr),
                                                                                               (uint8_t *)fixedSequence4,
                                                                                               TR_FixedSequenceAddress2, cg()),
                                         __FILE__,
                                         __LINE__,
                                         requestors[i]->getNode());
                  }
               }
            else
               {
               *(int32_t *)iloc1 |= cg()->hiValue(addr) & 0x0000ffff;
               *(int32_t *)iloc2 |= LO_VALUE(addr) & 0x0000ffff;
               TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
               recordInfo->data3 = orderedPairSequence1;
               cg()->addExternalRelocation(new (_cg->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(iloc1,
                                                                                                      iloc2,
                                                                                                      (uint8_t *)recordInfo,
                                                                                                      TR_AbsoluteMethodAddressOrderedPair,
                                                                                                      cg()),
                                      __FILE__,
                                      __LINE__,
                                      requestors[i]->getNode());
               }
            }

         codeCursor += 8;
         }

      dcursor = diterator.getNext();
      }

   ListIterator< TR::ARMConstant<float> >  fiterator(&_floatConstants);
   TR::ARMConstant<float>               *fcursor=fiterator.getFirst();
   while (fcursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = fcursor->getRequestors();
      size = requestors.size();
      if (size > 0)
         {
         *(float *)codeCursor = fcursor->getConstantValue();

         TR_ASSERT(size%count == 0, "Requestors are paired.\n");
         for (i=0; i<size; i+=count)
            {
            iloc1 = requestors[i]->getBinaryEncoding();
            iloc2 = requestors[i+1]->getBinaryEncoding();
            addr = (intptr_t)codeCursor;
            if (count==4)
               {
               iloc3 = requestors[i+2]->getBinaryEncoding();
               iloc4 = requestors[i+3]->getBinaryEncoding();
               if (!cg()->comp()->compileRelocatableCode())
                  {
                  *(int32_t *)iloc4 |= LO_VALUE(addr) & 0x0000ffff;
                  addr = cg()->hiValue(addr);
                  *(int32_t *)iloc3 |= addr & 0x0000ffff;
                  *(int32_t *)iloc2 |= (addr>>16) & 0x0000ffff;
                  *(int32_t *)iloc1 |= (addr>>32) & 0x0000ffff;
                  }
               else
                  {
                  cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(requestors[i],
                                                                                               (uint8_t *)(addr),
                                                                                               (uint8_t *)fixedSequence4,
                                                                                               TR_FixedSequenceAddress2,
                                                                                               cg()),
                                         __FILE__,
                                         __LINE__,
                                         requestors[i]->getNode());
                  }
               }
            else
               {
               *(int32_t *)iloc1 |= cg()->hiValue(addr) & 0x0000ffff;
               *(int32_t *)iloc2 |= LO_VALUE(addr) & 0x0000ffff;

               TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
               recordInfo->data3 = orderedPairSequence1;
               cg()->addExternalRelocation(new (_cg->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(iloc1, iloc2, (uint8_t *)recordInfo, TR_AbsoluteMethodAddressOrderedPair, cg()),
                                      __FILE__, __LINE__, requestors[i]->getNode());
               }
            }
         codeCursor += 4;
         }

      fcursor = fiterator.getNext();
      }
#endif

   ListIterator< TR::ARMConstant<intptr_t> >  aiterator(&_addressConstants);
   TR::ARMConstant<intptr_t>               *acursor=aiterator.getFirst();
   while (acursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = acursor->getRequestors();
      size = requestors.size();
      if (size > 0)
         {
         *(intptr_t *)codeCursor = acursor->getConstantValue();
         if (cg()->profiledPointersRequireRelocation())
            {
            TR::Node *node = acursor->getNode();
            if (node != NULL && node->getOpCodeValue() == TR::aconst)
               {
               TR_ExternalRelocationTargetKind kind=TR_NoRelocation;
               if (node->isClassPointerConstant())
                  kind = TR_ClassPointer;
               else if (node->isMethodPointerConstant())
                  {
                  if (node->getInlinedSiteIndex() == -1)
                     kind = TR_RamMethod;
                  else
                     kind = TR_MethodPointer;
                  }

               if (kind != TR_NoRelocation)
                  {
                  TR::Relocation *relo;
                  relo = new (cg()->trHeapMemory()) TR::ExternalRelocation(codeCursor, (uint8_t *)node, kind, cg());
                  cg()->addExternalRelocation(relo, __FILE__, __LINE__, node);
                  }
               }
            }
         else
            {
            cg()->jitAddPicToPatchOnClassRedefinition((void *)acursor->getConstantValue(), (void *)codeCursor);

            if (acursor->isUnloadablePicSite())
	       {
	       // Register an unload assumption on the lower 32bit of the class constant.
	       // The patching code thinks it's low bit tagging an instruction not a class pointer!!
               cg()->
               jitAddPicToPatchOnClassUnload((void *)acursor->getConstantValue(), (void *)(codeCursor+((cg()->comp()->target().is64Bit())?4:0)) );
               }
            }

         TR_ASSERT(size%count == 0, "Requestors are paired.\n");
         for (i=0; i<size; i+=count)
            {
            addr = (intptr_t)codeCursor;
            iloc1 = requestors[i]->getBinaryEncoding();
            iloc2 = requestors[i+1]->getBinaryEncoding();
            iloc3 = requestors[i+2]->getBinaryEncoding();
            iloc4 = requestors[i+3]->getBinaryEncoding();
            if (!cg()->comp()->compileRelocatableCode())
               {
               intParts localVal((uint32_t)addr);
               *(uint16_t *)iloc1 = ((*((uint16_t *)iloc1)) & 0xf000) | 0x400 | localVal.getByte3(); // 0x400 means rotate right by 8bit
               *(uint16_t *)iloc2 = ((*((uint16_t *)iloc2)) & 0xf000) | 0x800 | localVal.getByte2();
               *(uint16_t *)iloc3 = ((*((uint16_t *)iloc3)) & 0xf000) | 0xc00 | localVal.getByte1();
               *(uint16_t *)iloc4 = ((*((uint16_t *)iloc4)) & 0xf000) | localVal.getByte1();
               }
            else
               {
               cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(requestors[i],
                                                                                               (uint8_t *)(addr),
                                                                                               (uint8_t *)fixedSequence4,
                                                                                               TR_FixedSequenceAddress2,
                                                                                               cg()),
                                         __FILE__,
                                         __LINE__,
                                         requestors[i]->getNode());
               }
            }
         codeCursor += sizeof(intptr_t);
         }
      acursor = aiterator.getNext();
      }
   return codeCursor;
   }



uint32_t TR::ARMConstantDataSnippet::getLength()
   {
   return _addressConstants.getSize()*4;
   }


#if DEBUG
void TR::ARMConstantDataSnippet::print(TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   TR_FrontEnd *fe = cg()->comp()->fe();

   uint8_t   *codeCursor = getSnippetBinaryStart();
   uint8_t   *codeStart  = cg()->getBinaryBufferStart();

   trfprintf(outFile, "\n%08x\t\t\t\t\t; Constant Data", codeCursor-codeStart);

   ListIterator< TR::ARMConstant<intptr_t> >  aiterator(&_addressConstants);
   TR::ARMConstant<intptr_t>                 *acursor=aiterator.getFirst();
   while (acursor != NULL)
      {
      if (acursor->getRequestors().size()>0)
	 {
         trfprintf(outFile, "\n%08x %08x\t\t; %p Address", codeCursor-codeStart,
                 *(int32_t *)codeCursor, acursor->getConstantValue());
         codeCursor += 4;
	 }
      acursor = aiterator.getNext();
      }
   }
#endif
