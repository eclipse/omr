/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_ALIASBUILDER_INCL
#define OMR_ALIASBUILDER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_ALIASBUILDER_CONNECTOR
#define OMR_ALIASBUILDER_CONNECTOR
namespace OMR { class AliasBuilder; }
namespace OMR { typedef OMR::AliasBuilder AliasBuilderConnector; }
#endif

#include "compile/Method.hpp"   // for vcount_t
#include "env/TRMemory.hpp"     // for Allocator, etc
#include "infra/BitVector.hpp"
#include "infra/Array.hpp"      // for TR_Array
#include "infra/List.hpp"
#include "infra/Link.hpp"

class TR_Memory;
class TR_StackMemory;
class TR_HeapMemory;
namespace TR { class AliasBuilder; }
namespace TR { class Block; }
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }

namespace OMR
{

class AliasBuilder
   {

public:

   TR_ALLOC(TR_Memory::SymbolReferenceTable)

   AliasBuilder(TR::SymbolReferenceTable *symRefTab, size_t sizeHintInElements, TR::Compilation *comp);

   void createAliasInfo();

   TR::AliasBuilder *self();

   TR::SymbolReferenceTable *symRefTab() { return _symRefTab; }
   TR::Compilation *comp() { return _compilation; }
   TR_Memory *trMemory() { return _trMemory; }
   TR_StackMemory trStackMemory() { return _trMemory; }
   TR_HeapMemory trHeapMemory() { return _trMemory; }

   TR_BitVector & addressShadowSymRefs() { return _addressShadowSymRefs; }
   TR_BitVector & intShadowSymRefs() { return _intShadowSymRefs; }
   TR_BitVector & genericIntShadowSymRefs() { return _genericIntShadowSymRefs; }
   TR_BitVector & genericIntArrayShadowSymRefs() { return _genericIntArrayShadowSymRefs; }
   TR_BitVector & genericIntNonArrayShadowSymRefs() { return _genericIntNonArrayShadowSymRefs; }
   TR_BitVector & nonIntPrimitiveShadowSymRefs() { return _nonIntPrimitiveShadowSymRefs; }
   TR_BitVector & addressStaticSymRefs() { return _addressStaticSymRefs; }
   TR_BitVector & intStaticSymRefs() { return _intStaticSymRefs; }
   TR_BitVector & nonIntPrimitiveStaticSymRefs() { return _nonIntPrimitiveStaticSymRefs; }
   TR_BitVector & methodSymRefs() { return _methodSymRefs; }
   TR_BitVector & arrayElementSymRefs() { return _arrayElementSymRefs; }
   TR_BitVector & immutableArrayElementSymRefs() { return _immutableArrayElementSymRefs; }

   TR::SymbolReference *getSymRefForAliasing(TR::Node *node, TR::Node *addrChild);

   TR_BitVector & arrayletElementSymRefs() { return _arrayletElementSymRefs; }
   TR_BitVector & unsafeSymRefNumbers() { return _unsafeSymRefNumbers; }
   TR_BitVector & unsafeArrayElementSymRefs() { return _unsafeArrayElementSymRefs; }

   TR_BitVector & gcSafePointSymRefNumbers() { return _gcSafePointSymRefNumbers; }
   TR_BitVector & cpConstantSymRefs() { return _cpConstantSymRefs; }
   TR_BitVector & cpSymRefs() { return _cpSymRefs; }

   TR_BitVector & catchLocalUseSymRefs() { return _catchLocalUseSymRefs; }

   TR_BitVector & defaultMethodDefAliases() { return _defaultMethodDefAliases; }
   TR_BitVector & defaultMethodUseAliases() { return _defaultMethodUseAliases; }
   TR_BitVector & methodsThatMayThrow() { return _methodsThatMayThrow; }
   TR_BitVector & defaultMethodDefAliasesWithoutImmutable() { return _defaultMethodDefAliasesWithoutImmutable; }
   TR_BitVector & defaultMethodDefAliasesWithoutUserField() { return _defaultMethodDefAliasesWithoutUserField; }

   // used in support of LoopAliasRefiner
   TR_BitVector & refinedNonIntPrimitiveArrayShadows() { return _refinedNonIntPrimitiveArrayShadows; }
   TR_BitVector & refinedAddressArrayShadows() { return _refinedAddressArrayShadows; }
   TR_BitVector & refinedIntArrayShadows() { return _refinedIntArrayShadows; }

   bool litPoolGenericIntShadowHasBeenCreated(){ return _litPoolGenericIntShadowHasBeenCreated; }
   void setLitPoolGenericIntShadowHasBeenCreated(){ _litPoolGenericIntShadowHasBeenCreated = true; }

   void updateSubSets(TR::SymbolReference *ref);

   TR_BitVector * methodAliases(TR::SymbolReference *);


   void setVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol *, TR_BitVector *);
   TR_BitVector * getVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol *);

   TR_BitVector & notOsrCatchLocalUseSymRefs() { return _notOsrCatchLocalUseSymRefs; }
   void setCatchLocalUseSymRefs();
   void gatherLocalUseInfo(TR::Node *, TR_BitVector&, vcount_t, bool);
   void gatherLocalUseInfo(TR::Block *, bool);
   void gatherLocalUseInfo(TR::Block *, TR_BitVector &, TR_ScratchList<TR_Pair<TR::Block, TR_BitVector> > *, vcount_t, bool);

   bool hasUseonlyAliasesOnlyDueToOSRCatchBlocks(TR::SymbolReference *symRef);

   bool conservativeGenericIntShadowAliasing();
   void setConservativeGenericIntShadowAliasing(bool b) { _conservativeGenericIntShadowAliasingRequired = b; }
   bool mutableGenericIntShadowHasBeenCreated() { return _mutableGenericIntShadowHasBeenCreated; }
   void setMutableGenericIntShadowHasBeenCreated(bool b) { _mutableGenericIntShadowHasBeenCreated = b; }

   void addNonIntPrimitiveArrayShadows(TR_BitVector *);
   void addAddressArrayShadows(TR_BitVector *);
   void addIntArrayShadows(TR_BitVector *);

protected:

   TR::Compilation *_compilation;
   TR_Memory *_trMemory;

   TR::SymbolReferenceTable *_symRefTab;

   struct CallAliases : TR_Link<CallAliases>
      {
      CallAliases(TR_BitVector * bv, TR::ResolvedMethodSymbol * m) : _callAliases(bv), _methodSymbol(m) { }
      TR_BitVector * _callAliases;
      TR::ResolvedMethodSymbol * _methodSymbol;
      };

   TR_LinkHead<CallAliases> _callAliases;

   TR_BitVector _addressShadowSymRefs;
   TR_BitVector _intShadowSymRefs;
   TR_BitVector _genericIntShadowSymRefs;
   TR_BitVector _genericIntArrayShadowSymRefs;
   TR_BitVector _genericIntNonArrayShadowSymRefs;
   TR_BitVector _nonIntPrimitiveShadowSymRefs;
   TR_BitVector _addressStaticSymRefs;
   TR_BitVector _intStaticSymRefs;
   TR_BitVector _nonIntPrimitiveStaticSymRefs;
   TR_BitVector _methodSymRefs;
   TR_BitVector _arrayElementSymRefs;
   TR_BitVector _immutableArrayElementSymRefs;

   TR_BitVector _arrayletElementSymRefs;
   TR_BitVector _unsafeSymRefNumbers;
   TR_BitVector _unsafeArrayElementSymRefs;  // subset of _unsafeSymRefNumbers
   TR_BitVector _gcSafePointSymRefNumbers;
   TR_BitVector _cpConstantSymRefs;
   TR_BitVector _cpSymRefs;

   TR_BitVector _catchLocalUseSymRefs;
   TR_BitVector _defaultMethodDefAliases;
   TR_BitVector _defaultMethodUseAliases;
   TR_BitVector _methodsThatMayThrow;

   TR_BitVector _defaultMethodDefAliasesWithoutImmutable;
   TR_BitVector _defaultMethodDefAliasesWithoutUserField;

   // used for LoopAliasRefiner
   TR_BitVector _refinedAddressArrayShadows;
   TR_BitVector _refinedIntArrayShadows;
   TR_BitVector _refinedNonIntPrimitiveArrayShadows;

   bool _litPoolGenericIntShadowHasBeenCreated;

   TR_Array<TR_BitVector *> _userFieldMethodDefAliases;

   TR_BitVector _notOsrCatchLocalUseSymRefs;

   // J9?
   bool _conservativeGenericIntShadowAliasingRequired;
   bool _mutableGenericIntShadowHasBeenCreated;

   };

}

#endif

