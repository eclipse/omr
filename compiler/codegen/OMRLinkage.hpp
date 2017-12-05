/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef OMR_LINKAGE_INCL
#define OMR_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { class Linkage; }
namespace OMR { typedef OMR::Linkage LinkageConnector; }
#endif

#include "infra/List.hpp"
#include "il/symbol/ParameterSymbol.hpp"

#include <stddef.h>                            // for NULL
#include <stdint.h>                            // for uint32_t, uint8_t, etc
#include "codegen/RegisterConstants.hpp"       // for TR_RegisterKinds, etc
#include "env/TRMemory.hpp"                    // for TR_Memory, etc
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/Annotations.hpp"               // for OMR_EXTENSIBLE

class TR_BitVector;
namespace TR { class AutomaticSymbol; }
namespace TR { class Compilation; }
namespace TR { class Linkage; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
template <class T> class List;

namespace OMR
{
class OMR_EXTENSIBLE Linkage
   {
   public:

   TR::Linkage* self();

   TR_ALLOC(TR_Memory::Linkage)

   Linkage() { }

   virtual void createPrologue(TR::Instruction *cursor) = 0;
   virtual void createEpilogue(TR::Instruction *cursor) = 0;

   virtual uint32_t getRightToLeft() = 0;
   virtual bool     hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void     mapStack(TR::ResolvedMethodSymbol *method) = 0;
   virtual void     mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex) = 0;

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol> &parm)
      {
      TR_ASSERT(0, "setParameterLinkageRegisterIndex has to be implemented for this linkage\n");
      }

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind) = 0;
   virtual TR_RegisterKinds argumentRegisterKind(TR::Node *argumentNode);

   virtual bool useCachedStaticAreaAddresses(TR::Compilation *c) { return false; }

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
      {
      TR_ASSERT(0, "setParameterLinkageRegisterIndex(2) has to be implemented for this linkage\n");
      }

   virtual  TR_BitVector * getKilledRegisters(TR::Node *node)
      {
      return NULL;
      }
   virtual  TR_BitVector * getSavedRegisters(TR::Node *node, int32_t entryId = 0)
      {
      return NULL;
      }

   virtual bool isAddressOfStaticSymRef(TR::SymbolReference *) { return false; }

   virtual bool mapPreservedRegistersToStackOffsets(int32_t *mapRegsToStack, int32_t &numPreserved, TR_BitVector *&) { return false; }
   virtual TR::Instruction *savePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset)    {return NULL; }
   virtual TR::Instruction *restorePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset) {return NULL; }
   virtual int32_t getRegisterSaveSize() { return 0; }
   virtual TR::Instruction *composeSavesRestores(TR::Instruction *start, int32_t firstReg, int32_t lastReg, int32_t offset, int32_t numRegs, bool doSaves) { return NULL; }

   virtual bool isSpecialNonVolatileArgumentRegister(int8_t) { return false; }

   };
}

#endif
