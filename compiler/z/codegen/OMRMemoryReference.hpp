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

#ifndef OMR_Z_MEMORY_REFERENCE_INCL
#define OMR_Z_MEMORY_REFERENCE_INCL

/*
* The following #define and typedef must appear before any #includes in this file
*/
#ifndef OMR_MEMREF_CONNECTOR
#define OMR_MEMREF_CONNECTOR
namespace OMR {namespace Z { class MemoryReference; } }
namespace OMR { typedef OMR::Z::MemoryReference MemoryReferenceConnector; }
#else
#error OMR::Z::MemoryReference expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRMemoryReference.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Register.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"

class TR_StorageReference;
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class UnresolvedDataSnippet; }

#define S390MemRef_UnresolvedDataSnippet         0x01
#define MemRef_ConstantDataSnippet               0x04
#define DisplacementAdjusted                     0x10
#define PseudoLive                               0x20
#define CheckForLongDispSlot                     0x40
#define MemRefMustNotSpill                       0x80
#define MemRefUsedBefore                         0x100
#define TR_S390MemRef_Is2ndMemRef                0x200
#define TR_S390MemRef_RightAlignMemRef           0x400
/** ThrowsNullPointerException is only for Java */
#define ThrowsNullPointerException               0x800
#define TR_S390MemRef_HasTemporaryNegativeOffset 0x2000
#define TR_S390MemRef_LeftAlignMemRef            0x4000
#define TR_S390MemRef_ForceEvaluation           0x10000
#define TR_S390MemRef_ForceFolding              0x20000
#define OriginalSymRefForAliasingOnly          0x200000
/** ExposedConsantAddressing is true BCD and Aggr const nodes that have the lit pool address fully exposed as the first child
* The alternative is if litPoolOffset is just picked off of the constant node itself */
#define ExposedConstantAddressing              0x400000
#define AdjustedForLongDisplacement            0x800000
/** MemRefCreatedDuringInstructionSelection is true for inserting BPP after a call or a label, we need to load the return address from the stack,
* this happens during doInstructionSelection when we don't have stack frame, so we need to update the memory reference during binary generation */
#define MemRefCreatedDuringInstructionSelection 0x1000000
#define TR_S390MemRef_ForceFirstTimeFolding     0x2000000

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReference
{
TR::Register              *_baseRegister;

protected:

TR::Node                  *_baseNode;

private:

TR::Register              *_indexRegister;
TR::Node                  *_indexNode;

protected:

TR::SymbolReference      *_symbolReference;
TR::SymbolReference      *_originalSymbolReference;

private:

TR::Instruction           *_targetSnippetInstruction;
TR::Snippet                   *_targetSnippet;
flags32_t                 _flags;

/** \brief
  *     The offset of the memory reference relative to the entity being referenced.
  */
intptr_t _offset;

TR_StorageReference       *_storageReference;

/**
 * For memRefs that are to be right or left aligned then an overall symbol size is required.
 * Usually this size can be obtained from the current _symbolReference->getSymbol()->getSize() but in some cases
 * (such as a load from the getWCodeMainLitSymRef() static processed through populateLoadAddrTree) the _symbolReference
 * is changed to point to a shared symbol with a size with no direct relation to the current instructions use of the symbol.
 * In these cases the size from the original rootLoadOrStore node's symRef is cached in _fixedSizeForAlignment
 */
int32_t                   _fixedSizeForAlignment;
int32_t                   _leftMostByte; ///< for left alignment  : bump = getTotalSizeForAlignment() - _leftMostByte
const char                *_name;        ///< name to use in listings for when we don't have a valid symref
TR::SymbolReference      *_listingSymbolReference;

protected:

List<TR::Node>            _incrementedNodesList;

public:

TR_ALLOC(TR_Memory::MemoryReference)

MemoryReference(TR::CodeGenerator *cg);
MemoryReference(TR::Register *br, int32_t disp, TR::CodeGenerator *cg, const char *name=NULL);
MemoryReference(TR::Register *br, int32_t disp, TR::SymbolReference *symRef, TR::CodeGenerator *cg);
MemoryReference(TR::Register *br, TR::Register *ir, int32_t disp, TR::CodeGenerator *cg);

MemoryReference(int32_t disp, TR::CodeGenerator *cg, bool isConstantDataSnippet=false);

MemoryReference(TR::Node *rootLoadOrStore, TR::CodeGenerator *cg, bool canUseRX = false, TR_StorageReference *storageReference=NULL);
MemoryReference(TR::Node *addressTree, bool canUseIndex, TR::CodeGenerator *cg);
void initSnippetPointers(TR::Snippet *s, TR::CodeGenerator *cg);
MemoryReference(TR::Node *node, TR::SymbolReference *symRef, TR::CodeGenerator *cg, TR_StorageReference *storageReference=NULL);
MemoryReference(TR::Snippet *s, TR::CodeGenerator *cg, TR::Register* base, TR::Node* node);
MemoryReference(TR::Snippet *s, TR::Register* indx, int32_t disp, TR::CodeGenerator *cg);

MemoryReference(MemoryReference& mr, int32_t n, TR::CodeGenerator *cg);

/** @return true if instr needs to be adjusted for long displacement */
static bool needsAdjustDisp(TR::Instruction * instr, MemoryReference * mRef, TR::CodeGenerator * cg);
static bool canUseTargetRegAsScratchReg (TR::Instruction * instr);
static bool typeNeedsAlignment(TR::Node *node);
static bool shouldLabelForRAS(TR::SymbolReference * symRef,  TR::CodeGenerator * cg);
int32_t calcDisplacement(uint8_t * cursor, TR::Instruction * instr, TR::CodeGenerator * cg);
int32_t getRightAlignmentBump(TR::Instruction * instr, TR::CodeGenerator * cg);
int32_t getLeftAlignmentBump(TR::Instruction * instr, TR::CodeGenerator * cg);
int32_t getSizeIncreaseBump(TR::Instruction * instr, TR::CodeGenerator * cg);

/** Project specific relocations for specific instructions */
void addInstrSpecificRelocation(TR::CodeGenerator* cg, TR::Instruction* instr, int32_t disp, uint8_t * cursor) {} //J9 AOT only for now

bool setForceFoldingIfAdvantageous(TR::CodeGenerator * cg, TR::Node * addressChild);
bool tryBaseIndexDispl(TR::CodeGenerator* cg, TR::Node* loadStore, TR::Node* addressChild);

TR::Register *getBaseRegister()                {return _baseRegister;}
TR::Register *setBaseRegister(TR::Register *br, TR::CodeGenerator * cg)
   {
   TR_ASSERT(cg != NULL, "cg is null in setBaseRegister!");
   if ((_baseRegister != NULL) &&
       (_baseRegister->getRegisterPair() != NULL))
      {
      cg->stopUsingRegister(_baseRegister);
      }
   return _baseRegister = br;
   }
TR::Register *swapBaseRegister(TR::Register *br, TR::CodeGenerator * cg);

TR::Node *getBaseNode()            {return _baseNode;}
TR::Node *setBaseNode(TR::Node *bn) {return _baseNode = bn;}

TR::Register *getIndexRegister() {return _indexRegister;}
TR::Register *setIndexRegister(TR::Register *ir) {return _indexRegister = ir;}

TR::Node *getIndexNode()            {return _indexNode;}
TR::Node *setIndexNode(TR::Node *in) {return _indexNode = in;}

/** \brief
  *    Gets the offset of the memory reference relative to the entity being referenced.
  *
  * \note
  *    The offset may not represent the final displacement which will be used to encode the memory reference until the
  *    binary encoding of this memory reference is generated.
  */
intptr_t getOffset() {return _offset;}

/** \brief
  *    Sets the offset of the memory reference relative to the entity being referenced.
  */
void setOffset(intptr_t amount) {_offset = amount;}

void addToOffset(intptr_t amount) {_offset += amount;}

/**
 * An unresolved data snippet and constant data snippet are mutually exclusive for
 * a given memory reference.  Hence, they share the same pointer.
 */
TR::UnresolvedDataSnippet *getUnresolvedSnippet();

TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *s);

TR::Instruction *getTargetSnippetInstruction() {return _targetSnippetInstruction;}
TR::Instruction *setTargetSnippetInstruction(TR::Instruction *i)
   {
   return _targetSnippetInstruction = i;
   }

TR::S390ConstantDataSnippet *getConstantDataSnippet();

TR::S390ConstantDataSnippet *setConstantDataSnippet(TR::S390ConstantDataSnippet *s);

TR_StorageReference *getStorageReference()                           { return _storageReference; }
TR_StorageReference *setStorageReference(TR_StorageReference *ref)   { return _storageReference=ref; }

/**
 * Memory references created for indirect loads/stores have a fixed size set as the memRef may be set with the underlying loadaddr symRef. This loadaddr
 * symRef does not reflect the actual size of the symbol being loaded so the fixed _fixedSizeForAlignment must be used instead.
 */
int32_t getTotalSizeForAlignment();
void setFixedSizeForAlignment(int32_t size);
int32_t getFixedSizeForAlignment() { return _fixedSizeForAlignment; }

int32_t getLeftMostByte()              { return _leftMostByte; }
int32_t setLeftMostByte(int32_t byte)  { return _leftMostByte=byte; }

bool getMemRefUsedBefore()  { return _flags.testAny(MemRefUsedBefore); }
void setMemRefUsedBefore()  { _flags.set(MemRefUsedBefore); }
void resetMemRefUsedBefore()  { _flags.reset(MemRefUsedBefore); }

bool getDispAdjusted()    { return _flags.testAny(DisplacementAdjusted); }
void setDispAdjusted()    { _flags.set(DisplacementAdjusted); }

bool getPseudoLive()      { return _flags.testAny(PseudoLive); }
void setPseudoLive()      { _flags.set(PseudoLive); }

bool getCheckForLongDispSlot() { return _flags.testAny(CheckForLongDispSlot); }
void setCheckForLongDispSlot() { _flags.set(CheckForLongDispSlot); }

/** Determine whether the memory reference can cause an implicit null pointer exception */
bool getCausesImplicitNullPointerException() { return _flags.testAny(ThrowsNullPointerException); }
void setCausesImplicitNullPointerException() { _flags.set(ThrowsNullPointerException); }
void setupCausesImplicitNullPointerException(TR::CodeGenerator *cg);

void setupCheckForLongDispFlag(TR::CodeGenerator *cg);

bool is2ndMemRef()         { return _flags.testAny(TR_S390MemRef_Is2ndMemRef); }
void setIs2ndMemRef()      { _flags.set(TR_S390MemRef_Is2ndMemRef); }
void resetIs2ndMemRef()    { _flags.reset(TR_S390MemRef_Is2ndMemRef); }

bool rightAlignMemRef()       { return _flags.testAny(TR_S390MemRef_RightAlignMemRef); }
void resetRightAlignMemRef()  { _flags.reset(TR_S390MemRef_RightAlignMemRef); }
void setRightAlignMemRef()
   {
    _flags.reset(TR_S390MemRef_LeftAlignMemRef);
    _flags.set(TR_S390MemRef_RightAlignMemRef);
   }

bool leftAlignMemRef()        { return _flags.testAny(TR_S390MemRef_LeftAlignMemRef); }
void resetLeftAlignMemRef()   { _flags.reset(TR_S390MemRef_LeftAlignMemRef); }
void setLeftAlignMemRef(int32_t leftMostByte);

bool isAligned();

/** \brief
 *     Determines whether this memory reference may require long displacement (as defined by PoPs).
 *
 * \return
 *    `true` if this memory reference requires long displacement; `false` if long displacement may not be required.
 *
 * \note
 *    A return value of `false` does not imply long displacement is not required. This is because the displacement of
 *    this memory reference is not fully known until binary encoding. Only after the binary encoding of this memory
 *    reference will a return value of `false` indicate that long displacement is not required. The offset may increase
 *    by some amount during code generation, however it will never decrease. This is why a return value of `true` 
 *    always implies long displacement will be required.
 */
const bool isLongDisplacementRequired();

/** \brief
 *     Determines whether this memory reference may require huge displacement.
 *
 * \return
 *    `true` if this memory reference requires huge displacement; `false` if huge displacement may not be required.
 *
 * \note
 *    \see isLongDisplacementRequired notes section.
 */
const bool isHugeDisplacementRequired();

bool forceEvaluation()        { return _flags.testAny(TR_S390MemRef_ForceEvaluation); }
void setForceEvaluation()
   {
   _flags.reset(TR_S390MemRef_ForceFolding);
   _flags.set(TR_S390MemRef_ForceEvaluation);
   }

bool forceFolding()        { return _flags.testAny(TR_S390MemRef_ForceFolding); }
void setForceFolding()
   {
   _flags.reset(TR_S390MemRef_ForceEvaluation);
   _flags.set(TR_S390MemRef_ForceFolding);
   }

bool forceFirstTimeFolding()  { return _flags.testAny(TR_S390MemRef_ForceFirstTimeFolding); }
void setForceFirstTimeFolding()
   {
   _flags.reset(TR_S390MemRef_ForceEvaluation);
   _flags.set(TR_S390MemRef_ForceFirstTimeFolding);
   }
void resetForceFirstTimeFolding()   { _flags.reset(TR_S390MemRef_ForceFirstTimeFolding); }

bool hasTemporaryNegativeOffset()    { return _flags.testAny(TR_S390MemRef_HasTemporaryNegativeOffset); }
void addToTemporaryNegativeOffset(TR::Node *node, int32_t offset, TR::CodeGenerator *cg);
void setHasTemporaryNegativeOffset() { _flags.set(TR_S390MemRef_HasTemporaryNegativeOffset); }

bool isOriginalSymRefForAliasingOnly(){ return _flags.testAny(OriginalSymRefForAliasingOnly); }
void setIsOriginalSymRefForAliasingOnly(){ _flags.set(OriginalSymRefForAliasingOnly); }

bool isExposedConstantAddressing(){ return _flags.testAny(ExposedConstantAddressing); }
void setIsExposedConstantAddressing(){ _flags.set(ExposedConstantAddressing); }

bool isAdjustedForLongDisplacement(){ return _flags.testAny(AdjustedForLongDisplacement); }
void setAdjustedForLongDisplacement(){ _flags.set(AdjustedForLongDisplacement); }

bool wasCreatedDuringInstructionSelection() {return _flags.testAny(MemRefCreatedDuringInstructionSelection); }
void setCreatedDuringInstructionSelection() { _flags.set(MemRefCreatedDuringInstructionSelection);}

bool refsRegister(TR::Register *reg)
   {
   if (reg == _baseRegister ||
       reg == _indexRegister)
      {
      return true;
      }
   return false;
   }

bool isUnresolvedDataSnippet()  {return _flags.testAny(S390MemRef_UnresolvedDataSnippet);}
void setUnresolvedDataSnippet() {_flags.set(S390MemRef_UnresolvedDataSnippet);}

bool isConstantDataSnippet()  {return _flags.testAny(MemRef_ConstantDataSnippet);}
void setConstantDataSnippet() {_flags.set(MemRef_ConstantDataSnippet);}

bool isMemRefMustNotSpill()   {return _flags.testAny(MemRefMustNotSpill);}
void setMemRefMustNotSpill()  {_flags.set(MemRefMustNotSpill);}

bool ZeroBasePtr_EvaluateSubtree(TR::Node * subTree, TR::CodeGenerator * cg, MemoryReference * mr);

void setSymbolReference(TR::SymbolReference * ref) { _symbolReference = ref;}
TR::SymbolReference *getSymbolReference() {return _symbolReference;}

void setOriginalSymbolReference(TR::SymbolReference * ref, TR::CodeGenerator * cg);
TR::SymbolReference *getOriginalSymbolReference() {return _originalSymbolReference;}

void setListingSymbolReference(TR::SymbolReference * ref) { _listingSymbolReference = ref;}
TR::SymbolReference *getListingSymbolReference() {return _listingSymbolReference;}

void decNodeReferenceCounts(TR::CodeGenerator *cg);
void stopUsingMemRefRegister(TR::CodeGenerator *cg);

void bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg);

void populateThroughEvaluation(TR::Node * rootLoadOrStore, TR::CodeGenerator *cg);
void populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg);
TR::S390ConstantDataSnippet* createLiteralPoolSnippet(TR::Node *rootNode, TR::CodeGenerator *cg);
void populateLoadAddrTree(TR::Node* subTree, TR::CodeGenerator* cg);
void populateAloadTree(TR::Node* subTree, TR::CodeGenerator* cg, bool privateArea = false);
void populateShiftLeftTree(TR::Node* subTree, TR::CodeGenerator* cg);
void populateAddTree(TR::Node* subTree, TR::CodeGenerator* cg);

void consolidateRegisters(TR::Node *subTree, TR::CodeGenerator *cg);

/** Move the index register from a RS instruction after poplulateMemoryReference */
TR::Instruction * separateIndexRegister(TR::Node *subTree, TR::CodeGenerator *cg, bool enforce4KDisplacementLimit, TR::Instruction *preced, bool forceDueToAlignmentBump = false);

bool ignoreNegativeOffset();
bool alignmentBumpMayRequire4KFixup(TR::Node *node, TR::CodeGenerator * cg);
TR::Instruction * handleLargeOffset(TR::Node * node, TR::MemoryReference *interimMemoryReference, TR::Register *tempTargetRegister, TR::CodeGenerator * cg, TR::Instruction * preced);
void eliminateNegativeDisplacement(TR::Node * node, TR::CodeGenerator *cg);
TR::Instruction * enforceDisplacementLimit(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction * preced);
TR::Instruction * enforce4KDisplacementLimit(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction * preced, bool forceFixup = false, bool forceDueToAlignmentBump = false);
TR::Instruction * enforceSSFormatLimits(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction *preced);
TR::Instruction * enforceRSLFormatLimits(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction *preced);
TR::Instruction * enforceVRXFormatLimits(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction *preced); //revert
TR::Instruction * markAndAdjustForLongDisplacementIfNeeded(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction * preced, bool *testLongDisp = NULL);

void propagateAlignmentInfo(MemoryReference *newMemRef);

void assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);

bool doEvaluate(TR::Node * subTree, TR::CodeGenerator * cg);

TR::Snippet * getSnippet();
int32_t generateBinaryEncoding(uint8_t *modRM, TR::CodeGenerator *cg, TR::Instruction *instr);
int32_t estimateBinaryLength(int32_t currentEstimate, TR::CodeGenerator *cg, TR::Instruction *instr);
int32_t generateBinaryEncodingTouchUpForLongDisp(uint8_t *modRM, TR::CodeGenerator *cg, TR::Instruction *instr);

void blockRegisters()
   {
   if (_baseRegister)
      {
      _baseRegister->block();
      }
   if (_indexRegister)
      {
      _indexRegister->block();
      }
   }

void unblockRegisters()
   {
   if (_baseRegister)
      {
      _baseRegister->unblock();
      }
   if (_indexRegister)
      {
      _indexRegister->unblock();
      }
   }

bool usesRegister(TR::Register* reg)
   {
   if (_baseRegister && _baseRegister == reg)
      return true;
   if (_indexRegister && _indexRegister == reg)
     return true;

   return false;
   }

TR::Register* parmRegister(TR::ParameterSymbol* ps, TR::CodeGenerator *cg);

const char *getName()              {return _name; }

void tryForceFolding(TR::Node * rootLoadOrStore, TR::CodeGenerator * cg, TR_StorageReference *storageReference, TR::SymbolReference *& symRef, TR::Symbol *& symbol,
                     List<TR::Node>& nodesAlreadyEvaluatedBeforeFoldingList) {}

TR::UnresolvedDataSnippet * createUnresolvedDataSnippet(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register * tempReg, bool isStore) {return NULL;}
TR::UnresolvedDataSnippet * createUnresolvedDataSnippetForiaload(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register * tempReg, bool & isStore) {return NULL;}
void createUnresolvedSnippetWithNodeRegister(TR::Node * node, TR::CodeGenerator * cg, TR::SymbolReference * symRef, TR::Register *& writableLiteralPoolRegister) {}
void createUnresolvedDataSnippetForBaseNode(TR::CodeGenerator * cg, TR::Register * writableLiteralPoolRegister) {}

void createPatchableDataInLitpool(TR::Node * node, TR::CodeGenerator * cg, TR::Register * tempReg, TR::UnresolvedDataSnippet * uds) {}

bool symRefHasTemporaryNegativeOffset() {return false;}
void setMemRefAndGetUnresolvedData(TR::Snippet *& snippet) {}
};
}
}


///////////////////////////////////////////////////////////
// Generate Routines
///////////////////////////////////////////////////////////

TR::MemoryReference * generateS390MemoryReference(TR::CodeGenerator *cg);
TR::MemoryReference * generateS390MemoryReference(int32_t, TR::CodeGenerator *cg);
TR::MemoryReference * generateS390MemoryReference(TR::Register *, int32_t, TR::CodeGenerator *cg, const char *name=NULL);
TR::MemoryReference * generateS390MemoryReference(TR::Register *, TR::Register *, int32_t, TR::CodeGenerator *cg);
TR::MemoryReference * generateS390MemoryReference(TR::Node *, TR::CodeGenerator *, bool canUseRX = false);
TR::MemoryReference * generateS390MemoryReference(TR::MemoryReference &, int32_t, TR::CodeGenerator *cg);
TR::MemoryReference * generateS390MemoryReference(TR::Node *, TR::SymbolReference *, TR::CodeGenerator *);
TR::MemoryReference * generateS390MemoryReference(TR::Snippet *, TR::CodeGenerator *, TR::Register *, TR::Node *);
TR::MemoryReference * generateS390MemoryReference(TR::Snippet *, TR::Register *, int32_t, TR::CodeGenerator *);
TR::MemoryReference * generateS390MemoryReference(int32_t, TR::DataType, TR::CodeGenerator *, TR::Register *, TR::Node *node = NULL);
TR::MemoryReference * generateS390MemoryReference(int64_t, TR::DataType, TR::CodeGenerator *, TR::Register *, TR::Node *node = NULL);
TR::MemoryReference * generateS390MemoryReference(float  , TR::DataType, TR::CodeGenerator *, TR::Node *);
TR::MemoryReference * generateS390MemoryReference(double , TR::DataType, TR::CodeGenerator *, TR::Node *);
TR::MemoryReference * generateS390MemoryReference(TR::CodeGenerator * cg, TR::Node *, TR::Node *, int32_t disp=0, bool forSS=false);

TR::MemoryReference * generateS390ConstantAreaMemoryReference(TR::Register *br, int32_t disp, TR::Node *node, TR::CodeGenerator *cg, bool forSS);
TR::MemoryReference * generateS390ConstantAreaMemoryReference(TR::CodeGenerator *cg, TR::Node *addrNode, bool forSS);

TR::MemoryReference * generateS390ConstantDataSnippetMemRef(int32_t, TR::CodeGenerator *cg);

TR::MemoryReference * generateS390AddressConstantMemoryReference(TR::CodeGenerator *cg, TR::Node *node);

TR::MemoryReference * reuseS390MemoryReference(TR::MemoryReference *baseMR, int32_t offset, TR::Node *node, TR::CodeGenerator *cg, bool enforceSSLimits);

TR::MemoryReference * generateS390RightAlignedMemoryReference(TR::MemoryReference& baseMR, TR::Node *node, int32_t offset, TR::CodeGenerator *cg, bool enforceSSLimits=true);
TR::MemoryReference * generateS390LeftAlignedMemoryReference(TR::MemoryReference& baseMR, TR::Node *node, int32_t offset, TR::CodeGenerator *cg, int32_t leftMostByte, bool enforceSSLimits=true);
#endif
