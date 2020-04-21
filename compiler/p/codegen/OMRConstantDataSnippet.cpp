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

#include "codegen/ConstantDataSnippet.hpp"

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
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/Runtime.hpp"

int32_t OMR::ConstantDataSnippet::addConstantRequest(void              *v,
                                                  TR::DataType       type,
                                                  TR::Instruction *nibble0,
                                                  TR::Instruction *nibble1,
                                                  TR::Instruction *nibble2,
                                                  TR::Instruction *nibble3,
                                                  TR::Node *node,
                                                  bool isUnloadablePicSite)
   {
   TR::Compilation *comp = cg()->comp();
   union {
      float       fvalue;
      int32_t     ivalue;
   } fin, fex;

   union {
      double      dvalue;
      int64_t     lvalue;
   } din, dex;

   intptr_t   ain, aex;

   int32_t ret = PTOC_FULL_INDEX;

   switch(type)
      {
      case TR::Float:
         {
         ListIterator< PPCConstant<float> >  fiterator(&_floatConstants);
         PPCConstant<float>                 *fcursor=fiterator.getFirst();

         fin.fvalue = *(float *)v;
         while (fcursor != NULL)
            {
            fex.fvalue = fcursor->getConstantValue();
            if (fin.ivalue == fex.ivalue)
               break;
            fcursor = fiterator.getNext();
            }
         if (fcursor == NULL)
            {
            fcursor = new (_cg->trHeapMemory()) PPCConstant<float>(_cg, fin.fvalue);
            _floatConstants.add(fcursor);
            if (comp->target().is64Bit())
               {
               ret = TR_PPCTableOfConstants::lookUp(fin.fvalue, _cg);
               }
            fcursor->setTOCOffset(ret);
            }
         ret = fcursor->getTOCOffset();
         if (comp->target().is32Bit() || ret==PTOC_FULL_INDEX)
            fcursor->addValueRequest(nibble0, nibble1, nibble2, nibble3);
         }
         break;

      case TR::Double:
         {
         ListIterator< PPCConstant<double> > diterator(&_doubleConstants);
         PPCConstant<double>                *dcursor=diterator.getFirst();

         din.dvalue = *(double *)v;
         while (dcursor != NULL)
            {
            dex.dvalue = dcursor->getConstantValue();
            if (din.lvalue == dex.lvalue)
               break;
            dcursor = diterator.getNext();
            }
         if (dcursor == NULL)
            {
            dcursor = new (_cg->trHeapMemory()) PPCConstant<double>(_cg, din.dvalue);
            _doubleConstants.add(dcursor);
            if (comp->target().is64Bit())
               {
               ret = TR_PPCTableOfConstants::lookUp(din.dvalue, _cg);
               }
            dcursor->setTOCOffset(ret);
            }
         ret = dcursor->getTOCOffset();
         if (comp->target().is32Bit() || ret==PTOC_FULL_INDEX)
            dcursor->addValueRequest(nibble0, nibble1, nibble2, nibble3);
         }
         break;

      case TR::Address:
         {
         ListIterator< PPCConstant<intptr_t> >  aiterator(&_addressConstants);
         PPCConstant<intptr_t>                 *acursor=aiterator.getFirst();

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
            acursor = new (_cg->trHeapMemory()) PPCConstant<intptr_t>(_cg, ain, node, isUnloadablePicSite);
            _addressConstants.add(acursor);
            }
            acursor->addValueRequest(nibble0, nibble1, nibble2, nibble3);
         }
         break;

      default:
         TR_ASSERT(0, "Only float and address constants are supported. Data type is %s.\n", type.toString());
      }

   return(ret);
   }


bool OMR::ConstantDataSnippet::getRequestorsFromNibble(TR::Instruction* nibble, TR::Instruction **q, bool remove)
   {
   ListIterator< PPCConstant<double> >  diterator(&_doubleConstants);
   PPCConstant<double>                 *dcursor=diterator.getFirst();
   int32_t count = cg()->comp()->target().is64Bit()?4:2;

   while (dcursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = dcursor->getRequestors();
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
               int32_t j;
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
      dcursor = diterator.getNext();
      }
   ListIterator< PPCConstant<float> >  fiterator(&_floatConstants);
   PPCConstant<float>               *fcursor=fiterator.getFirst();
   while (fcursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = fcursor->getRequestors();
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
      fcursor = fiterator.getNext();
      }
   ListIterator< PPCConstant<intptr_t> >  aiterator(&_addressConstants);
   PPCConstant<intptr_t>               *acursor=aiterator.getFirst();
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


void
OMR::ConstantDataSnippet::emitFloatingPointConstant(
      TR_Array<TR::Instruction *> &requestors,
      uint8_t *codeCursor,
      int32_t count)
   {
   uint8_t *iloc1, *iloc2, *iloc3, *iloc4;
   intptr_t addr;
   int32_t i;
   int32_t size = requestors.size();
   TR::Compilation *comp = cg()->comp();

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

         if (cg()->canEmitDataForExternallyRelocatableInstructions())
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

   }


void
OMR::ConstantDataSnippet::emitAddressConstant(
      PPCConstant<intptr_t> *acursor,
      TR_Array<TR::Instruction *> &requestors,
      uint8_t *codeCursor,
      int32_t count)
   {
   uint8_t *iloc1, *iloc2, *iloc3, *iloc4;
   intptr_t addr;
   int32_t i;
   int32_t size = requestors.size();
   TR::Compilation *comp = cg()->comp();

   if (cg()->profiledPointersRequireRelocation())
      {
      TR::Node *node = acursor->getNode();
      if (node != NULL && node->getOpCodeValue() == TR::aconst)
         {
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR::SymbolType type;

            if (node->isClassPointerConstant())
               type = TR::SymbolType::typeClass;
            else if (node->isMethodPointerConstant())
               type = TR::SymbolType::typeMethod;
            else
               TR_ASSERT_FATAL(false, "Unable to relocate node %p", node);

            TR::Relocation *relo = new (cg()->trHeapMemory()) TR::ExternalRelocation(codeCursor, (uint8_t *)node->getAddress(), (uint8_t*)type, TR_SymbolFromManager, cg());
            cg()->addExternalRelocation(relo, __FILE__, __LINE__, node);
            }
         else
            {
            TR_ExternalRelocationTargetKind kind=TR_NoRelocation;
            if (node->isClassPointerConstant())
               kind = TR_ClassPointer;
            else if (node->isMethodPointerConstant())
               kind = (node->getInlinedSiteIndex() == -1) ? TR_RamMethod : TR_MethodPointer;

            if (kind != TR_NoRelocation)
               {
               TR::Relocation *relo = new (cg()->trHeapMemory()) TR::ExternalRelocation(codeCursor, (uint8_t *)node, kind, cg());
               cg()->addExternalRelocation(relo, __FILE__, __LINE__, node);
               }
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
      iloc1 = requestors[i]->getBinaryEncoding();
      iloc2 = requestors[i+1]->getBinaryEncoding();
      addr = (intptr_t)codeCursor;
      // if it's the start PC, don't use the constant, but grab it from the symbol:
      TR::Symbol *sym = requestors[i]->getNode()->getSymbol();
      TR::StaticSymbol *staticSym = sym ? sym->getStaticSymbol() : NULL;
      if (staticSym && staticSym->isStartPC())
         addr = (intptr_t) staticSym->getStaticAddress();
      if (count==4)
         {
         iloc3 = requestors[i+2]->getBinaryEncoding();
         iloc4 = requestors[i+3]->getBinaryEncoding();
         if (cg()->canEmitDataForExternallyRelocatableInstructions())
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
         *(int32_t *)iloc1 |= _cg->hiValue(addr) & 0x0000ffff;
         *(int32_t *)iloc2 |= LO_VALUE(addr) & 0x0000ffff;

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data3 = orderedPairSequence1;
         cg()->addExternalRelocation(new (_cg->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(iloc1, iloc2, (uint8_t *)recordInfo, TR_AbsoluteMethodAddressOrderedPair, cg()),
                                __FILE__, __LINE__, requestors[i]->getNode());
         }
      }

   }


uint8_t *OMR::ConstantDataSnippet::emitSnippetBody()
   {
   uint8_t *codeCursor = cg()->getBinaryBufferCursor();
   int32_t size, count;

   setSnippetBinaryStart(codeCursor);
   count = cg()->comp()->target().is64Bit()?4:2;

   // Align cursor to 8 bytes alignment
   codeCursor = (uint8_t *)((intptr_t)(codeCursor+7) & ~7);

   // Emit order is double, address, float. In this order we can ensure the 8 bytes alignment of double and address.

   ListIterator< PPCConstant<double> > diterator(&_doubleConstants);
   PPCConstant<double> *dcursor=diterator.getFirst();
   while (dcursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = dcursor->getRequestors();
      size = requestors.size();
      if (size > 0)
         {
         *(double *)codeCursor = dcursor->getConstantValue();
         emitFloatingPointConstant(requestors, codeCursor, count);
         codeCursor += 8;
         }

      dcursor = diterator.getNext();
      }

   ListIterator< PPCConstant<intptr_t> > aiterator(&_addressConstants);
   PPCConstant<intptr_t> *acursor=aiterator.getFirst();
   while (acursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = acursor->getRequestors();
      size = requestors.size();
      if (size > 0)
         {
         *(intptr_t *)codeCursor = acursor->getConstantValue();
         emitAddressConstant(acursor, requestors, codeCursor, count);
         codeCursor += sizeof(intptr_t);
         }
      acursor = aiterator.getNext();
      }

   ListIterator< PPCConstant<float> > fiterator(&_floatConstants);
   PPCConstant<float> *fcursor=fiterator.getFirst();
   while (fcursor != NULL)
      {
      TR_Array<TR::Instruction *> &requestors = fcursor->getRequestors();
      size = requestors.size();
      if (size > 0)
         {
         *(float *)codeCursor = fcursor->getConstantValue();
         emitFloatingPointConstant(requestors, codeCursor, count);
         codeCursor += 4;
         }

      fcursor = fiterator.getNext();
      }

   return codeCursor;
   }



uint32_t OMR::ConstantDataSnippet::getLength()
   {
      return _doubleConstants.getSize()*8 + _floatConstants.getSize()*4 + _addressConstants.getSize()*sizeof(intptr_t) + 4;
   }


#if DEBUG
void OMR::ConstantDataSnippet::print(TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   TR_FrontEnd *fe = cg()->comp()->fe();

   uint8_t   *codeCursor = getSnippetBinaryStart();
   uint8_t   *codeStart  = cg()->getBinaryBufferStart();

   trfprintf(outFile, "\n%08x\t\t\t\t\t; Constant Data", codeCursor-codeStart);

   ListIterator< PPCConstant<double> >  diterator(&_doubleConstants);
   PPCConstant<double>                 *dcursor=diterator.getFirst();
   while (dcursor != NULL)
      {
      if (cg()->comp()->target().is32Bit() || dcursor->getRequestors().size()>0)
         {
         trfprintf(outFile, "\n%08x %08x %08x\t\t; %16f Double", codeCursor-codeStart,
                 *(int32_t *)codeCursor, *(int32_t *)(codeCursor+4), dcursor->getConstantValue());
         codeCursor += 8;
         }
      dcursor = diterator.getNext();
      }

   ListIterator< PPCConstant<float> >  fiterator(&_floatConstants);
   PPCConstant<float>                 *fcursor=fiterator.getFirst();
   while (fcursor != NULL)
      {
      if (cg()->comp()->target().is32Bit() || fcursor->getRequestors().size()>0)
         {
         trfprintf(outFile, "\n%08x %08x\t\t; %16f Float", codeCursor-codeStart,
                 *(int32_t *)codeCursor, fcursor->getConstantValue());
         codeCursor += 4;
         }
      fcursor = fiterator.getNext();
      }

   ListIterator< PPCConstant<intptr_t> >  aiterator(&_addressConstants);
   PPCConstant<intptr_t>                 *acursor=aiterator.getFirst();
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
