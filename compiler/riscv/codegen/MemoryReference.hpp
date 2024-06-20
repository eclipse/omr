/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#ifndef TR_MEMORYREFERENCE_INCL
#define TR_MEMORYREFERENCE_INCL

#include "codegen/OMRMemoryReference.hpp"

#include <stdint.h>

namespace TR { class SymbolReference; }
namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class Register; }

namespace TR
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReferenceConnector
   {
   private:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::CodeGenerator *cg) :
      OMR::MemoryReferenceConnector(cg) {}

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Register *br,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(br, cg) {}

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] disp : displacement
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Register *br,
      int32_t disp,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(br, disp, cg) {}

   /**
    * @brief Constructor
    * @param[in] rootLoadOrStore : load or store node
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(rootLoadOrStore, cg) {}

   /**
    * @brief Constructor
    * @param[in] node : node
    * @param[in] symRef : symbol reference
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Node *node,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(node, symRef, cg) {}

   public:

   static TR::MemoryReference *create(TR::CodeGenerator *cg);
   static TR::MemoryReference *createWithIndexReg(TR::CodeGenerator *cg, TR::Register *baseReg, TR::Register *indexReg, uint8_t scale = 0);
   static TR::MemoryReference *createWithDisplacement(TR::CodeGenerator *cg, TR::Register *baseReg, int64_t displacement);
   static TR::MemoryReference *createWithRootLoadOrStore(TR::CodeGenerator *cg, TR::Node *rootLoadOrStore);
   static TR::MemoryReference *createWithSymRef(TR::CodeGenerator *cg, TR::Node *node, TR::SymbolReference *symRef);
   };

} // TR

#endif
