/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#ifndef OMR_ILINJECTOR_INCL
#define OMR_ILINJECTOR_INCL


#ifndef TR_ILINJECTOR_DEFINED
#define TR_ILINJECTOR_DEFINED
#define PUT_OMR_ILINJECTOR_INTO_TR
#endif

#include <stdint.h>
#include "env/jittypes.h"
#include "il/ILOpCodes.hpp"
#include "ilgen/IlGen.hpp"

#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)

namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class Compilation; }
namespace TR { class FrontEnd; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }

// This macro reduces dependencies for this header file to be used with libjit.a
#ifndef TR_ALLOC
#define TR_ALLOC(x)
#endif

namespace OMR
{

class IlGenerator : public TR_IlGenerator
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   IlGenerator();
   virtual ~IlGenerator() { };

   virtual void initialize(TR::IlGeneratorMethodDetails * details,
                           TR::ResolvedMethodSymbol     * methodSymbol,
                           TR::FrontEnd                 * fe,
                           TR::SymbolReferenceTable     * symRefTab);

   bool                           genIL();
   virtual TR::Block                * getCurrentBlock() = 0;
   virtual TR::Block                * block(int32_t num) = 0;
   virtual int32_t                    numBlocks() const = 0;
   virtual TR::ResolvedMethodSymbol * methodSymbol() const { return _methodSymbol; }
   virtual int32_t currentByteCodeIndex() { return -1; }

   // Many tests should just need to define their own version of this function
   virtual bool                   injectIL() = 0;

   TR::Compilation              * comp()             const { return _comp; }
   TR::IlGeneratorMethodDetails * details()          const { return _details; }
   TR::FrontEnd                 * fe()               const { return _fe; }
   TR::SymbolReferenceTable     * symRefTab()              { return _symRefTab; }
   TR::CFG                      * cfg();

   TR::Node                     * parameter(int32_t slot, TR::DataType dt);
   TR::SymbolReference          * newTemp(TR::DataType dt);
   TR::Node                     * iconst(int32_t value);
   TR::Node                     * lconst(int64_t value);
   TR::Node                     * bconst(int8_t value);
   TR::Node                     * sconst(int16_t value);
   TR::Node                     * aconst(uintptrj_t value);
   TR::Node                     * dconst(double value);
   TR::Node                     * fconst(float value);
   TR::Node                     * staticAddress(void *address);
   void                           storeToTemp(TR::SymbolReference *tempSymRef, TR::Node *value);
   TR::Node                     * loadTemp(TR::SymbolReference *tempSymRef);

   TR::TreeTop                  * genTreeTop(TR::Node *n);
   
   //TR::Node                     * createWithoutSymRef(TR::ILOpCodes opCode, uint16_t numArgs, ...);

private:

protected:
   // data
   //
   TR::Compilation              * _comp;
   TR::FrontEnd                 * _fe;
   TR::SymbolReferenceTable     * _symRefTab;

   TR::IlGeneratorMethodDetails * _details;
   TR::ResolvedMethodSymbol     * _methodSymbol;
   };

} // namespace OMR


#ifdef PUT_OMR_ILINJECTOR_INTO_TR

namespace TR
{
   class IlGenerator : public OMR::IlGenerator
      {
      public:
		  IlGenerator()
            : OMR::IlGenerator()
            { }
		  virtual TR::Block                * getCurrentBlock() = 0;
		  virtual TR::Block                * block(int32_t num) = 0;
		  virtual int32_t                    numBlocks() const = 0;

      };

} // namespace TR

#endif // defined(PUT_OMR_ILINJECTOR_INTO_TR)

#endif // !defined(OMR_ILINJECTOR_INCL)
