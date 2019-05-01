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

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/Annotations.hpp"

class TR_BitVector;
class TR_FrontEnd;
class TR_HeapMemory;
class TR_Memory;
class TR_StackMemory;
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Linkage; }
namespace TR { class Machine; }
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

   inline TR::Linkage* self();

   /**
    * @return Cached CodeGenerator object
    */
   inline TR::CodeGenerator *cg();

   /**
    * @return Machine object from cached CodeGenerator
    */
   inline TR::Machine *machine();

   /**
    * @return Compilation object from cached CodeGenerator
    */
   inline TR::Compilation *comp();

   /**
    * @return FrontEnd object from cached CodeGenerator
    */
   inline TR_FrontEnd *fe();

   /**
    * @return TR_Memory object from cached CodeGenerator
    */
   inline TR_Memory *trMemory();

   /**
    * @return TR_HeapMemory object
    */
   inline TR_HeapMemory trHeapMemory();

   /**
    * @return TR_StackMemory object
    */
   inline TR_StackMemory trStackMemory();

   TR_ALLOC(TR_Memory::Linkage)

   Linkage(TR::CodeGenerator *cg) : _cg(cg) { }

   virtual void createPrologue(TR::Instruction *cursor) = 0;
   virtual void createEpilogue(TR::Instruction *cursor) = 0;

   virtual uint32_t getRightToLeft() = 0;
   virtual bool     hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void     mapStack(TR::ResolvedMethodSymbol *method) = 0;
   virtual void     mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex) = 0;

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol> &parm)
      {
      TR_UNIMPLEMENTED();
      }

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
      {
      TR_UNIMPLEMENTED();
      }

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind) = 0;

   virtual TR_RegisterKinds argumentRegisterKind(TR::Node *argumentNode);

protected:

   TR::CodeGenerator *_cg;
   };
}

#endif
