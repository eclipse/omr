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

#ifndef OMR_SYMBOLREFERENCE_INCL
#define OMR_SYMBOLREFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SYMBOLREFERENCE_CONNECTOR
#define OMR_SYMBOLREFERENCE_CONNECTOR
namespace OMR { class SymbolReference; }
namespace OMR { typedef OMR::SymbolReference SymbolReferenceConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Symbol.hpp"
#include "infra/Annotations.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"

typedef enum {
   UseDefAliasSet,
   UseOnlyAliasSet
} AliasSetInterface;

class TR_Debug;
class TR_ResolvedMethod;
class TR_UseDefAliasSetInterface;
class TR_UseOnlyAliasSetInterface;
namespace TR { class Register; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
template <class T> class TR_Array;
template <AliasSetInterface> class TR_AliasSetInterface ;
typedef TR::SparseBitVector SharedSparseBitVector;

// Extra symbol reference info for allocation nodes.
struct TR_ExtraInfoForNew
   {
   TR_ALLOC(TR_Memory::ExtraInfoForNew)

   TR_BitVector *zeroInitSlots;
   int32_t       numZeroInitSlots;
   };

namespace OMR
{
/**
 * A SymbolReference contains information about a Symbol that is context
 * dependent.
 *
 * At construction, the vast majority of SymbolReferences are also linked into
 * the Symbol, and this will allow them to contribute to aliasing computations.
 *
 * \see Symbol
 */
class OMR_EXTENSIBLE SymbolReference
   {

public:
   TR_ALLOC(TR_Memory::SymbolReference)

   TR::SymbolReference* self();

   void init(TR::SymbolReferenceTable * symRefTab,
             uint32_t                   refNumber,
             TR::Symbol *               sym = 0,
             intptr_t                  offset = 0,
             mcount_t                   owningMethodIndex = JITTED_METHOD_INDEX,
             int32_t                    cpIndex = -1,
             int32_t                    unresolvedIndex = 0,
             bool                       checkNoNamedShadow = true);

   SymbolReference() {}

   SymbolReference(TR::SymbolReferenceTable * symRefTab);

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol * symbol,
                   intptr_t offset = 0);

   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   int32_t refNumber,
                   TR::Symbol *ps,
                   intptr_t offset = 0);

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReferenceTable::CommonNonhelperSymbol number,
                   TR::Symbol *ps,
                   intptr_t offset = 0);

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::Symbol *sym,
                   mcount_t owningMethodIndex,
                   int32_t cpIndex,
                   int32_t unresolvedIndex = 0,
                   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);

   SymbolReference(TR::SymbolReferenceTable *symRefTab,
                   TR::SymbolReference& sr,
                   intptr_t offset,
                   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);

   void copyFlags(TR::SymbolReference * sr);

   static TR::SymbolReference *create(TR::SymbolReferenceTable *symRefTab, TR::Symbol *sym, TR::KnownObjectTable::Index koi);

   void * getMethodAddress();

   TR_ResolvedMethod *        getOwningMethod(TR::Compilation * c);
   TR::ResolvedMethodSymbol * getOwningMethodSymbol(TR::Compilation *c);

   bool maybeVolatile();

   // New CS2 Interface
   TR_UseOnlyAliasSetInterface getUseonlyAliases();
   TR_UseDefAliasSetInterface  getUseDefAliases(bool isDirectCall = false,
                                                  bool includeGCSafePoint = false);
   TR_UseDefAliasSetInterface  getUseDefAliasesIncludingGCSafePoint(bool isDirectCall = false);

   bool isUnresolvedFieldInCP(TR::Compilation *c);
   bool isUnresolvedMethodInCP(TR::Compilation *c);

   bool reallySharesSymbol(TR::Compilation * c);
   bool sharesSymbol( bool includingGCSafePoint = false);

   bool isThisPointer();
   bool isTemporary(TR::Compilation *c);

   virtual const char *getName(TR_Debug *debug);

   /**
    * Alias functions
    */

   void copyAliasSets(TR::SymbolReference *, TR::SymbolReferenceTable *symRefTab);

   void setLiteralPoolAliases(TR_BitVector *, TR::SymbolReferenceTable *symRefTab);
   void setSharedShadowAliases(TR_BitVector *, TR::SymbolReferenceTable *symRefTab);
   void setSharedStaticAliases(TR_BitVector *, TR::SymbolReferenceTable *symRefTab);

   bool canKill(TR::SymbolReference *);
   bool willUse(TR::SymbolReference *, TR::SymbolReferenceTable *symRefTab);

   bool storeCanBeRemoved();

   // Return the signature of the symbol's type if applicable. Note, the
   // signature's storage may have been created on the stack!
   const char * getTypeSignature(int32_t & len, TR_AllocationKind = stackAlloc, bool *isFixed = NULL);

   void copyRefNumIfPossible(TR::SymbolReference *sr, TR::SymbolReferenceTable *symRefTab);

   void makeIndependent(TR::SymbolReferenceTable *, TR::SymbolReference *);

   void           setIndependentSymRefs(TR_BitVector *bv);
   TR_BitVector * getIndependentSymRefs();

   /**
    * Alias functions end
    */

   /**
    * Field functions
    */

   TR::Symbol *         getSymbol()                            { return _symbol; }
   TR::Symbol *         setSymbol(TR::Symbol *ps)              { return (_symbol = ps); }

   TR_ExtraInfoForNew * getExtraInfo()                         { return _extraInfo; }
   void                 setExtraInfo(TR_ExtraInfoForNew *info) { _extraInfo = info; }

   // Offset from the underlying symbol. Should the symbol have an offset, these
   // offsets are later summed together
   intptr_t            getOffset()                            { return _offset; }
   void                 setOffset(intptr_t o)                 { _offset = o; }
   void                 addToOffset(intptr_t o)               { _offset += o; }

   uint32_t             getSize()                              { return _size; }
   void                 setSize(uint32_t size)                 { _size = size; }

   mcount_t             getOwningMethodIndex()                 { return _owningMethodIndex; }
   void                 setOwningMethodIndex(mcount_t i)       { _owningMethodIndex = i; }

   void                 setCPIndex(int32_t i)                  { _cpIndex = i; }
   int32_t              getCPIndex()                           { return _cpIndex; }

   int32_t              getUnresolvedIndex()                   { return _unresolvedIndex; }

   int32_t              getReferenceNumber()                   { return _referenceNumber; }
   int32_t              setReferenceNumber(int32_t n)          { return (_referenceNumber = n); }

   TR::KnownObjectTable::Index getKnownObjectIndex();
   bool                        hasKnownObjectIndex();
   uintptr_t *                getKnownObjectReferenceLocation(TR::Compilation *comp);

   /// Resolved final class that is not an array returns TRUE
   virtual void setAliasedTo(TR::SymbolReference *other, bool symmetric = true);
   virtual void setAliasedTo(TR_BitVector &bv, TR::SymbolReferenceTable *symRefTab, bool symmetric);

   void         setEmptyUseDefAliases(TR::SymbolReferenceTable *symRefTab);
   void         setUseDefAliases(TR_BitVector * bv);

   /**
    * Field functions end
    */

   /**
    * Flag functions
    */

   void setUnresolved()                         { _flags.set(Unresolved); }
   bool isUnresolved()                          { return _flags.testAny(Unresolved); }

   void setCanGCandReturn()                     { _flags.set(CanGCandReturn); }
   bool canGCandReturn()                        { return _flags.testAny(CanGCandReturn); }
   void setCanGCandExcept()                     { _flags.set(CanGCandExcept); }
   bool canGCandExcept()                        { return _flags.testAny(CanGCandExcept); }
   bool canCauseGC();

   void setReallySharesSymbol()                 { _flags.set(ReallySharesSymbol); }
   bool reallySharesSymbol()                    { return _flags.testAny(ReallySharesSymbol); }

   void setStackAllocatedArrayAccess()          { _flags.set(StackAllocatedArrayAccess); }
   bool stackAllocatedArrayAccess()             { return _flags.testAny(StackAllocatedArrayAccess); }

   void setSideEffectInfo()                     { _flags.set(SideEffectInfo); }
   bool isSideEffectInfo()                      { return _flags.testAny(SideEffectInfo); }

   void setLiteralPoolAddress()                 { _flags.set(LiteralPoolAddress); }
   bool isLiteralPoolAddress()                  { return _flags.testAny(LiteralPoolAddress); }
   void setFromLiteralPool()                    { _flags.set(FromLiteralPool); }
   bool isFromLiteralPool()                     { return _flags.testAny(FromLiteralPool); }
   bool isLitPoolReference();

   void setOverriddenBitAddress()               { _flags.set(OverriddenBitAddress); }
   bool isOverriddenBitAddress()                { return _flags.testAny(OverriddenBitAddress); }

   void setInitMethod()                         { _flags.set(InitMethod); }
   bool isInitMethod()                          { return _flags.testAny(InitMethod); }

   void setIsTempVariableSizeSymRef()           { _flags.set(TempVariableSizeSymRef); }
   bool isTempVariableSizeSymRef()              { return _flags.testAny(TempVariableSizeSymRef); }

   void setIsAdjunct()                          { _flags.set(Adjunct); }
   bool isAdjunct()                             { return _flags.testAny(Adjunct); }

   void setIsDual()                             { _flags.set(Dual); }
   bool isDual()                                { return _flags.testAny(Dual); }

   void setHasTemporaryNegativeOffset()         { _flags.set(TemporaryNegativeOffset); }
   void resetHasTemporaryNegativeOffset()       { _flags.reset(TemporaryNegativeOffset); }
   bool hasTemporaryNegativeOffset()            { return _flags.testAny(TemporaryNegativeOffset); }

   void setHoldsMonitoredObjectForSyncMethod()  { _flags.set(HoldsMonitoredObjectForSyncMethod); }
   bool holdsMonitoredObjectForSyncMethod()     { return _flags.testAny(HoldsMonitoredObjectForSyncMethod); }

   void setHasBeenAccessedAtRuntime(TR_YesNoMaybe ynm = TR_yes);
   TR_YesNoMaybe hasBeenAccessedAtRuntime();

   /**
    * Flag functions end
    */


   /*
    * ReferenceNumber functions
    */

   bool isOSRInductionHelper();

   TR::SymbolReference *getOriginalUnimprovedSymRef(TR::Compilation *comp)
      {
      return comp->getSymRefTab()->getOriginalUnimprovedSymRef(self());
      }

protected:

   /**
    * Create a symbol reference, however, don't check named shadows
    */
   SymbolReference(TR::SymbolReferenceTable * symRefTab,
                   TR::Symbol *               symbol,
                   intptr_t                  offset,
                   const char *               name);

   friend class ::TR_Debug;

   template <AliasSetInterface>
   friend class ::TR_AliasSetInterface;

   TR_BitVector *getUseonlyAliasesBV(TR::SymbolReferenceTable *symRefTab);
   TR_BitVector *getUseDefAliasesBV( bool isDirectCall = false, bool gcSafe = false);

   enum // flags
      {
      Unresolved                        = 0x00000001,
      CanGCandReturn                    = 0x00000002,
      CanGCandExcept                    = 0x00000004,
      ReallySharesSymbol                = 0x00000008,
      StackAllocatedArrayAccess         = 0x00000010,
      SideEffectInfo                    = 0x00000020,
      LiteralPoolAddress                = 0x00000040,
      FromLiteralPool                   = 0x00000080,
      OverriddenBitAddress              = 0x00000100,
      InitMethod                        = 0x00000200, ///< J9
      TempVariableSizeSymRef            = 0x00000400,
      Adjunct                           = 0x00000800, ///< auto symbol represents the adjunct part of the dual symbol
      Dual                              = 0x00001000, ///< auto symbol represents a dual symbol consisting of two parts
      TemporaryNegativeOffset           = 0x00002000,
      HoldsMonitoredObjectForSyncMethod = 0x00004000,
      AccessedAtRuntimeBase             = 0x10000000,
      AccessedAtRuntimeMask             = 0x30000000
      };

   TR::Symbol *                _symbol;                ///< Pointer to the symbol being referenced

   TR_ExtraInfoForNew *        _extraInfo;             ///< Extra info pointer used for some SymbolReferences

   intptr_t                   _offset;                ///< Offset of reference from base address

   uint32_t                    _size;                  ///< Byte size of this SymbolReference

   mcount_t                    _owningMethodIndex;     ///< Used to lookup the owning method symbol.

   int32_t                     _cpIndex : 18;
   uint32_t                    _unresolvedIndex : 14;  ///< Used in value propagation

   uint32_t                    _referenceNumber;       ///< Unique number identifying this SymbolReference in the symbol table

   flags32_t                   _flags;

   TR::KnownObjectTable::Index _knownObjectIndex;

   union
      {
      TR_BitVector *           _useDefAliases;
      TR_BitVector *           _independentSymRefs;
      };

   };
}


char * classNameToSignature(const char *, int32_t & len, TR::Compilation *, TR_AllocationKind = stackAlloc);


class TR_SymRefIterator : public TR_BitVectorIterator
   {

public:
   TR_SymRefIterator(TR_BitVector& bv, TR::SymbolReferenceTable *symRefTab) :
      TR_BitVectorIterator(bv), _symRefTab(symRefTab) { }

   TR::SymbolReference * getNext() { return hasMoreElements() ? _symRefTab->getSymRef(getNextElement()) : 0; }

private:
   TR::SymbolReferenceTable *_symRefTab;

   };

#endif
