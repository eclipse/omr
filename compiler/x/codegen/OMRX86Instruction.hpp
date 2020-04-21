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

#ifndef OMR_X86INSTRUCTION_INCL
#define OMR_X86INSTRUCTION_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/X86Ops.hpp"
#include "env/CompilerEnv.hpp"

namespace TR { class LabelRelocation; }
class TR_VirtualGuardSite;
namespace TR { class X86RegMemInstruction; }
namespace TR { class X86RegRegInstruction; }
namespace TR { class UnresolvedDataSnippet; }
namespace TR { class SymbolReference; }

enum TR_X86MemoryBarrierKinds
   {
   NoFence              = 0x00,
   kLoadFence           = 0x01,
   kStoreFence          = 0x02,
   kMemoryFence         = kLoadFence | kStoreFence,
   LockOR               = 0x04,
   NeedsExplicitBarrier = LockOR | kMemoryFence,
   LockPrefix           = 0x08
   };

extern int32_t memoryBarrierRequired(TR_X86OpCode &op, TR::MemoryReference *mr, TR::CodeGenerator *cg, bool onlyAskingAboutFences);
extern int32_t estimateMemoryBarrierBinaryLength(int32_t barrier, TR::CodeGenerator *cg);
extern void padUnresolvedReferenceInstruction(TR::Instruction *instr, TR::MemoryReference *mr, TR::CodeGenerator *cg);
extern void insertUnresolvedReferenceInstructionMemoryBarrier(TR::CodeGenerator *cg, int32_t barrier, TR::Instruction *inst, TR::MemoryReference *mr, TR::Register *srcReg = NULL, TR::MemoryReference *anotherMr = NULL);


struct TR_AtomicRegion
   {
   uint8_t _start, _length;

   // Note: Lists of TR_AtomicRegions are null-terminated; ie. they end with
   // an entry having zero length.

   uint8_t getStart()  const { return _start;  }
   uint8_t getLength() const { return _length; }
   };


namespace TR
{

class X86PaddingInstruction : public TR::Instruction
   {
   uint8_t              _length;
   TR_PaddingProperties _properties;

   public:

   X86PaddingInstruction(uint8_t length, TR::Node *node, TR::CodeGenerator *cg):
      TR::Instruction(node, BADIA32Op, cg),
      _length(length),
      _properties(TR_NoOpPadding)
      {}

   X86PaddingInstruction(uint8_t length, TR_PaddingProperties properties, TR::Node *node, TR::CodeGenerator *cg):
      TR::Instruction(node, BADIA32Op, cg),
      _length(length),
      _properties(properties)
      {}

   X86PaddingInstruction(TR::Instruction *precedingInstruction, uint8_t length, TR::CodeGenerator *cg):
      TR::Instruction(BADIA32Op, precedingInstruction, cg),
      _length(length),
      _properties(TR_NoOpPadding)
      {}

   X86PaddingInstruction(TR::Instruction *precedingInstruction, uint8_t length, TR_PaddingProperties properties, TR::CodeGenerator *cg):
      TR::Instruction(BADIA32Op, precedingInstruction, cg),
      _length(length),
      _properties(properties)
      {}

   uint8_t              getLength()    { return _length;     }
   TR_PaddingProperties getProperties(){ return _properties; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual char *description() { return "X86RegMem"; }

   virtual Kind getKind() { return IsPadding; }

   };


class X86PaddingSnippetInstruction : public TR::X86PaddingInstruction
   {
   TR::UnresolvedDataSnippet *_unresolvedSnippet;

   public:

   X86PaddingSnippetInstruction(uint8_t length, TR::Node *node, TR::CodeGenerator *cg):
      TR::X86PaddingInstruction(length, node, cg),
      _unresolvedSnippet(NULL)
      {}

   X86PaddingSnippetInstruction(uint8_t length, TR_PaddingProperties properties, TR::Node *node, TR::CodeGenerator *cg):
      TR::X86PaddingInstruction(length, properties, node, cg),
      _unresolvedSnippet(NULL)
      {}

   X86PaddingSnippetInstruction(TR::Instruction *precedingInstruction, uint8_t length, TR::CodeGenerator *cg):
      TR::X86PaddingInstruction(precedingInstruction, length, cg),
      _unresolvedSnippet(NULL)
      {}

   X86PaddingSnippetInstruction(TR::Instruction *precedingInstruction, uint8_t length, TR_PaddingProperties properties, TR::CodeGenerator *cg):
      TR::X86PaddingInstruction(precedingInstruction, length, properties, cg),
      _unresolvedSnippet(NULL)
      {}

   virtual char *description() { return "PaddingSnippetInstruction"; }

   virtual TR::Snippet *getSnippetForGC();

   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *us)
      {
      return (_unresolvedSnippet = us);
      }
   };


class X86BoundaryAvoidanceInstruction : public TR::Instruction
   {
   // Inserts NOPs to ensure that none of the atomicRegions in the adjacent
   // targetCode cross a boundary (as specified by boundarySpacing).

   public:

   X86BoundaryAvoidanceInstruction(const TR_AtomicRegion *atomicRegions,
                                      uint8_t boundarySpacing,
                                      uint8_t maxPadding,
                                      TR::Instruction *targetCode,
                                      TR::CodeGenerator *cg)
      : TR::Instruction(BADIA32Op, targetCode->getPrev(), cg),
      _sizeOfProtectiveNop(0), _atomicRegions(atomicRegions), _boundarySpacing(boundarySpacing), _maxPadding(maxPadding), _targetCode(targetCode), _minPaddingLength(0)
      {
      setNode(targetCode->getNode());
      }

   X86BoundaryAvoidanceInstruction(int32_t sizeOfProtectiveNop,
                                      const TR_AtomicRegion *atomicRegions,
                                      uint8_t boundarySpacing,
                                      uint8_t maxPadding,
                                      TR::Instruction *targetCode,
                                      TR::CodeGenerator *cg)
      : TR::Instruction(BADIA32Op, targetCode->getPrev(), cg),
      _sizeOfProtectiveNop(sizeOfProtectiveNop), _atomicRegions(atomicRegions),
      _boundarySpacing(boundarySpacing), _maxPadding(maxPadding), _targetCode(targetCode),
      _minPaddingLength(0)
      {
      setNode(targetCode->getNode());
      }

   X86BoundaryAvoidanceInstruction(TR::Instruction *precedingInstruction,
                                      const TR_AtomicRegion *atomicRegions,
                                      uint8_t boundarySpacing,
                                      uint8_t maxPadding,
                                      TR::CodeGenerator *cg)
      : TR::Instruction(BADIA32Op, precedingInstruction, cg),
      _atomicRegions(atomicRegions), _boundarySpacing(boundarySpacing), _maxPadding(maxPadding), _targetCode(NULL),
      _sizeOfProtectiveNop(0), _minPaddingLength(0)
      {
      // Note: this constructor is usually not the right one to use, because
      // when you want to make something patchable, you usually know what
      // you're patching.  That's why there's no generateXXX function for this.
      // However, there are unusual cases where we don't care what we're
      // patching, so we provide this extra constructor.
      //
      // Notice that the order of the arguments is crucial to avoid calling the
      // wrong constructor.
      }

   const TR_AtomicRegion *getAtomicRegions()   { return _atomicRegions;   }
   uint8_t                getBoundarySpacing() { return _boundarySpacing; }
   uint8_t                getMaxPadding()      { return _maxPadding;      }
   TR::Instruction     *getTargetCode()      { return _targetCode;   }
   int32_t                getSizeOfProtectiveNop() { return _sizeOfProtectiveNop; }

   virtual void     assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t *generateBinaryEncoding();
   virtual OMR::X86::EnlargementResult  enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement);

   virtual char *description() { return "X86BoundaryAvoidance"; }

   virtual Kind getKind() { return IsBoundaryAvoidance; }

   static TR_AtomicRegion unresolvedAtomicRegions[]; // For patching unresolved references

   protected:

   virtual int32_t betterPadLength(int32_t oldPadLength, const TR_AtomicRegion *unaccommodatedRegion, int32_t unaccommodatedRegionStart);

   private:

   const TR_AtomicRegion *_atomicRegions;
   uint8_t                _boundarySpacing;
   uint8_t                _maxPadding;
   TR::Instruction     *_targetCode; // if NULL, _next will be padded instead
   int32_t                _sizeOfProtectiveNop;
   uint8_t                _minPaddingLength;
   };


class X86PatchableCodeAlignmentInstruction : public TR::X86BoundaryAvoidanceInstruction
   {

   public:

   // Note: we use cg->getInstructionPatchAlignmentBoundary() as the max padding,
   // even though that is 1 more byte than we should ever actually need.  The reason is
   // that we don't want the inherited logic to stop silently when it reaches this limit;
   // rather, we want it to try to add more padding, and trip on an assertion inside
   // the betterPadLength function so that we are alerted to the problem.

   X86PatchableCodeAlignmentInstruction(const TR_AtomicRegion *atomicRegions,
                                           TR::Instruction *patchableCode,
                                           TR::CodeGenerator *cg)
      : TR::X86BoundaryAvoidanceInstruction(atomicRegions, cg->getInstructionPatchAlignmentBoundary(), cg->getInstructionPatchAlignmentBoundary(), patchableCode, cg)
      {}

   X86PatchableCodeAlignmentInstruction(const TR_AtomicRegion *atomicRegions,
                                           TR::Instruction *patchableCode,
                                           int32_t sizeOfProtectiveNop,
                                           TR::CodeGenerator *cg)
      : TR::X86BoundaryAvoidanceInstruction(sizeOfProtectiveNop, atomicRegions, cg->getInstructionPatchAlignmentBoundary(), cg->getInstructionPatchAlignmentBoundary(), patchableCode, cg)
      {}


   X86PatchableCodeAlignmentInstruction(TR::Instruction *precedingInstruction,
                                           const TR_AtomicRegion *atomicRegions,
                                           TR::CodeGenerator *cg)
      : TR::X86BoundaryAvoidanceInstruction(precedingInstruction, atomicRegions, cg->getInstructionPatchAlignmentBoundary(), cg->getInstructionPatchAlignmentBoundary(), cg)
      {
      // Note: this constructor is usually not the right one to use.  See the
      // note in the corresponding TR::X86BoundaryAvoidanceInstruction
      // constructor for more information (search for "generateXXX").
      }

   TR::Instruction *getPatchableCode(){ return getTargetCode(); }

   virtual char *description() { return "PatchableCodeAlignment"; }

   virtual Kind getKind() { return IsPatchableCodeAlignment; }
   virtual int32_t betterPadLength(int32_t oldPadLength, const TR_AtomicRegion *unaccommodatedRegion, int32_t unaccommodatedRegionStart);

   // A few handy atomic region descriptors

   static TR_AtomicRegion spinLoopAtomicRegions[]; // For any patching done using a 2-byte self-loop
   static TR_AtomicRegion CALLImm4AtomicRegions[]; // For patching the displacement of a 5-byte call instruction
   };


class X86LabelInstruction : public TR::Instruction
   {
   TR::LabelSymbol *_symbol;
   TR::X86LabelInstruction *_outlinedInstructionBranch;
   bool _needToClearFPStack;
   uint8_t _reloType;
   bool _permitShortening;
   void initialize(TR::LabelSymbol *sym, bool b);

   public:

   X86LabelInstruction(TR_X86OpCodes op, TR::Node * node, TR::LabelSymbol *sym, TR::CodeGenerator *cg, bool b = false);
   X86LabelInstruction(TR::Instruction *precedingInstruction, TR_X86OpCodes op, TR::LabelSymbol *sym, TR::CodeGenerator *cg, bool b = false);
   X86LabelInstruction(TR_X86OpCodes op, TR::Node *node, TR::LabelSymbol *sym, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, bool b = false);
   X86LabelInstruction(TR::Instruction *precedingInstruction, TR_X86OpCodes op, TR::LabelSymbol *sym, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, bool b = false);

   void prohibitShortening() { _permitShortening = false; }

   virtual char *description() { return "X86LabelInstruction"; }
   virtual bool isPatchBarrier() { return getOpCodeValue() == LABEL && _symbol && _symbol->isTargeted() != TR_no; }

   uint8_t    getReloType() {return _reloType; };
   void       setReloType(uint8_t rt) { _reloType = rt;};


   virtual Kind getKind() { return IsLabel; }

   TR::LabelSymbol *getLabelSymbol()                    {return _symbol;}
   TR::LabelSymbol *setLabelSymbol(TR::LabelSymbol *sym) {return (_symbol = sym);}

   bool getNeedToClearFPStack()             {return _needToClearFPStack;}
   void setNeedToClearFPStack(bool b)       {_needToClearFPStack = b;}

   virtual TR::Snippet *getSnippetForGC();
   virtual uint8_t    *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual void        assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t     getBinaryLengthLowerBound();
   virtual OMR::X86::EnlargementResult  enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement);

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   virtual TR::X86LabelInstruction  *getX86LabelInstruction();

   void assignOutlinedInstructions(TR_RegisterKinds kindsToBeAssigned, TR::X86LabelInstruction *labelInstruction);
   void addPostDepsToOutlinedInstructionsBranch();

   void setOutlinedInstructionBranch(TR::X86LabelInstruction *li) {_outlinedInstructionBranch = li;}

   };


class X86AlignmentInstruction : public TR::Instruction
   {
   uint8_t _boundary, _margin, _minPaddingLength;

   public:

   virtual char *description() { return "X86Alignment"; }

   virtual Kind getKind() { return IsAlignment; }

   uint8_t getBoundary() { return _boundary; }
   uint8_t getMargin()   { return _margin; }

   // The 8 constructor permutations:
   // - node vs. preceding instruction
   // - with vs. without margin
   // - with vs. without dependencies

   X86AlignmentInstruction(TR::Node * node, uint8_t boundary, TR::CodeGenerator *cg)
      : TR::Instruction(node, BADIA32Op, cg),
      _boundary(boundary),
      _margin(0),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Node * node, uint8_t boundary, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg)
      : TR::Instruction(cond, node, BADIA32Op, cg),
      _boundary(boundary),
      _margin(0),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Node * node, uint8_t boundary, uint8_t margin, TR::CodeGenerator *cg)
      : TR::Instruction(node, BADIA32Op, cg),
      _boundary(boundary),
      _margin(margin),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Node * node, uint8_t boundary, uint8_t margin, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg)
      : TR::Instruction(cond, node, BADIA32Op, cg),
      _boundary(boundary),
      _margin(margin),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Instruction *precedingInstruction, uint8_t boundary, TR::CodeGenerator *cg)
      : TR::Instruction(BADIA32Op, precedingInstruction, cg),
      _boundary(boundary),
      _margin(0),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Instruction *precedingInstruction, uint8_t boundary, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg)
      : TR::Instruction(cond, BADIA32Op, precedingInstruction, cg),
      _boundary(boundary),
      _margin(0),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Instruction *precedingInstruction, uint8_t boundary, uint8_t margin, TR::CodeGenerator *cg)
      : TR::Instruction(BADIA32Op, precedingInstruction, cg),
      _boundary(boundary),
      _margin(margin),
      _minPaddingLength(0)
      {}

   X86AlignmentInstruction(TR::Instruction *precedingInstruction, uint8_t boundary, uint8_t margin, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
      : TR::Instruction(cond, BADIA32Op, precedingInstruction, cg),
      _boundary(boundary),
      _margin(margin),
      _minPaddingLength(0)
      {}

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t *generateBinaryEncoding();
   virtual OMR::X86::EnlargementResult  enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement);
   };


class X86FenceInstruction : public TR::Instruction
   {
   TR::Node * _fenceNode; // todo: replace uses of this with TR::Instruction::_node

   public:

   X86FenceInstruction(TR_X86OpCodes op,
                          TR::Node *,
                          TR::Node *n,
                          TR::CodeGenerator *cg);

   X86FenceInstruction(TR::Instruction *precedingInstruction,
                          TR_X86OpCodes op,
                          TR::Node *n,
                          TR::CodeGenerator *cg);

   virtual char *description() { return "X86Fence"; }

   virtual Kind getKind() { return IsFence; }

   TR::Node * getFenceNode() { return _fenceNode; }

   virtual uint8_t *generateBinaryEncoding();

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   };


#ifdef J9_PROJECT_SPECIFIC
class X86VirtualGuardNOPInstruction : public TR::X86LabelInstruction
   {
   private:
   TR_VirtualGuardSite *_site;

   // These fields are set after binary encoding
   TR::RealRegister::RegNum _register;
   int32_t _nopSize;

   public:
   X86VirtualGuardNOPInstruction(TR_X86OpCodes op,
                                    TR::Node *node,
                                    TR_VirtualGuardSite *site,
                                    TR::RegisterDependencyConditions *cond,
                                    TR::CodeGenerator *cg,
                                    TR::LabelSymbol *label = 0)
      : TR::X86LabelInstruction(op, node, label, cond, cg), _site(site), _nopSize(0), _register(TR::RealRegister::NoReg) {}

   X86VirtualGuardNOPInstruction(TR::Instruction *precedingInstruction,
                                    TR_X86OpCodes op,
                                    TR::Node *node,
                                    TR_VirtualGuardSite *site,
                                    TR::RegisterDependencyConditions *cond,
                                    TR::CodeGenerator *cg,
                                    TR::LabelSymbol *label = 0)
      : TR::X86LabelInstruction(precedingInstruction, op, label, cond, cg), _site(site), _nopSize(0), _register(TR::RealRegister::NoReg) { setNode(node); }

   virtual char *description() { return "X86VirtualGuardNOP"; }

   virtual Kind getKind() { return IsVirtualGuardNOP; }

   void setSite(TR_VirtualGuardSite *site) { _site = site; }
   TR_VirtualGuardSite * getSite() { return _site; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual bool isVirtualGuardNOPInstruction() {return true;}

   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);
   virtual bool refsRegister(TR::Register *reg);
   };
#endif

class X86ImmInstruction : public TR::Instruction
   {
   int32_t _sourceImmediate;
   int32_t _adjustsFramePointerBy; // TODO: Rename me.  Calls don't adjust the VFP per se; they adjust the stack pointer
   int32_t _reloKind;

   public:

   X86ImmInstruction(TR::Node * node, TR_X86OpCodes op, TR::CodeGenerator *cg, int32_t reloKind=TR_NoRelocation)
      : TR::Instruction(node, op, cg),
      _sourceImmediate(0),
      _adjustsFramePointerBy(0),
      _reloKind(reloKind)
      {
      }

   X86ImmInstruction(int32_t imm, TR::Node * node, TR_X86OpCodes op, TR::CodeGenerator *cg, int32_t reloKind=TR_NoRelocation)
      : TR::Instruction(node, op, cg),
      _sourceImmediate(imm),
      _adjustsFramePointerBy(0),
      _reloKind(reloKind)
      {
      }

   X86ImmInstruction(int32_t imm,
                        TR_X86OpCodes op,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg,
                        int32_t reloKind=TR_NoRelocation)
      : TR::Instruction(op, precedingInstruction, cg),
      _sourceImmediate(imm),
      _adjustsFramePointerBy(0),
      _reloKind(reloKind)
      {}

   X86ImmInstruction(TR::RegisterDependencyConditions *cond,
                        int32_t                             imm,
                        TR::Node                            *node,
                        TR_X86OpCodes                       op,
                        TR::CodeGenerator                   *cg,
                        int32_t                             reloKind=TR_NoRelocation)
      : TR::Instruction(cond, node, op, cg),
      _sourceImmediate(imm),
      _adjustsFramePointerBy(0),
      _reloKind(reloKind)
      {}

   X86ImmInstruction(TR::RegisterDependencyConditions *cond,
                        int32_t imm,
                        TR_X86OpCodes op,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg,
                        int32_t reloKind=TR_NoRelocation)
      : TR::Instruction(cond, op, precedingInstruction, cg),
      _sourceImmediate(imm),
      _adjustsFramePointerBy(0),
      _reloKind(reloKind)
      {
      if (cond && cg->enableRegisterAssociations())
         cond->createRegisterAssociationDirective(this, cg);
      }

   X86ImmInstruction(TR_X86OpCodes op,
                        TR::Node *node,
                        int32_t imm,
                        TR::CodeGenerator *cg,
                        int32_t reloKind=TR_NoRelocation);

   X86ImmInstruction(TR::Instruction *precedingInstruction,
                        TR_X86OpCodes op,
                        int32_t imm,
                        TR::CodeGenerator *cg,
                        int32_t reloKind=TR_NoRelocation);

   X86ImmInstruction(TR_X86OpCodes op,
                        TR::Node *node,
                        int32_t imm,
                        TR::RegisterDependencyConditions *cond,
                        TR::CodeGenerator *cg,
                        int32_t reloKind=TR_NoRelocation);

   X86ImmInstruction(TR::Instruction *precedingInstruction,
                        TR_X86OpCodes op,
                        int32_t imm,
                        TR::RegisterDependencyConditions *cond,
                        TR::CodeGenerator *cg,
                        int32_t reloKind=TR_NoRelocation);

   virtual char *description() { return "X86Imm"; }

   virtual Kind getKind() { return IsImm; }

   int32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t getSourceImmediateAsAddress()  {return (uint32_t)_sourceImmediate;}
   int32_t setSourceImmediate(int32_t si) {return (_sourceImmediate = si);}

   void setAdjustsFramePointerBy(int32_t a) {_adjustsFramePointerBy = a;}
   int32_t getAdjustsFramePointerBy()       {return _adjustsFramePointerBy;}

   int32_t getReloKind()                   {return _reloKind;}
   void setReloKind(int32_t reloKind)  {_reloKind = reloKind;}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   // The following safe virtual downcast method is used under debug only
   // for assertion checking.
   //
   virtual X86ImmInstruction  *getX86ImmInstruction();
#endif

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg){ adjustVFPStateForCall(state, _adjustsFramePointerBy, cg); }
   };


class X86ImmSnippetInstruction : public TR::X86ImmInstruction
   {
   TR::UnresolvedDataSnippet *_unresolvedSnippet;

   public:

   X86ImmSnippetInstruction(TR_X86OpCodes op,
                               TR::Node *node,
                               int32_t imm,
                               TR::UnresolvedDataSnippet *us,
                               TR::CodeGenerator *cg);

   X86ImmSnippetInstruction(TR::Instruction *precedingInstruction,
                               TR_X86OpCodes op,
                               int32_t imm,
                               TR::UnresolvedDataSnippet *us,
                               TR::CodeGenerator *cg);

   virtual char *description() { return "X86ImmSnippet"; }

   virtual Kind getKind() { return IsImmSnippet; }

   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *us)
      {
      return (_unresolvedSnippet = us);
      }

   virtual TR::Snippet *getSnippetForGC();
   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   };


class X86ImmSymInstruction : public TR::X86ImmInstruction
   {
   TR::SymbolReference *_symbolReference;

   public:

   X86ImmSymInstruction(TR_X86OpCodes op,
                           TR::Node *node,
                           int32_t imm,
                           TR::SymbolReference *sr,
                           TR::CodeGenerator *cg);

   X86ImmSymInstruction(TR::Instruction *precedingInstruction,
                           TR_X86OpCodes op,
                           int32_t imm,
                           TR::SymbolReference *sr,
                           TR::CodeGenerator *cg);

   X86ImmSymInstruction(TR_X86OpCodes op,
                           TR::Node *node,
                           int32_t imm,
                           TR::SymbolReference *sr,
                           TR::RegisterDependencyConditions *cond,
                           TR::CodeGenerator *cg);

   X86ImmSymInstruction(TR::Instruction *precedingInstruction,
                           TR_X86OpCodes op,
                           int32_t imm,
                           TR::SymbolReference *sr,
                           TR::RegisterDependencyConditions *cond,
                           TR::CodeGenerator *cg);

   virtual char *description() { return "X86ImmSym"; }

   virtual Kind getKind() { return IsImmSym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }

   void addMetaDataForCodeAddress(uint8_t *cursor);

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   };


class X86RegInstruction : public TR::Instruction
   {
   TR::Register *_targetRegister;

   public:

   X86RegInstruction(TR::Register      *reg,
                         TR::Node          *node,
                         TR_X86OpCodes    op,
                         TR::CodeGenerator *cg)
      : TR::Instruction(node, op, cg), _targetRegister(reg)
      {
      TR::Compilation *comp = cg->comp();
      useRegister(reg);
      getOpCode().trackUpperBitsOnReg(reg, cg);

      // Check the live discardable register list to see if this is the first
      // instruction that kills the rematerializable range of a register.
      //
      if (cg->enableRematerialisation() &&
          reg->isDiscardable() &&
          getOpCode().modifiesTarget())
         {
         TR::ClobberingInstruction *clob = new (cg->trHeapMemory()) TR::ClobberingInstruction(this, cg->trMemory());
         clob->addClobberedRegister(reg);
         cg->addClobberingInstruction(clob);
         cg->removeLiveDiscardableRegister(reg);
         cg->clobberLiveDependentDiscardableRegisters(clob, reg);

         if (debug("dumpRemat"))
            {
            diagnostic("---> Clobbering %s discardable register %s at instruction %p\n",
                        reg->getRematerializationInfo()->toString(comp), reg->getRegisterName(comp), this);
            }
         }
      }

   X86RegInstruction(TR::Register *reg,
                        TR_X86OpCodes op,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg)
      : TR::Instruction(op, precedingInstruction, cg), _targetRegister(reg)
      {
      useRegister(reg);
      getOpCode().trackUpperBitsOnReg(reg, cg);
      }

   X86RegInstruction(TR::RegisterDependencyConditions  *cond,
                         TR::Register                         *reg,
                         TR::Node                             *node,
                         TR_X86OpCodes                       op,
                         TR::CodeGenerator                    *cg)
      : TR::Instruction(cond, node, op, cg), _targetRegister(reg)
      {
      TR::Compilation *comp = cg->comp();
      useRegister(reg);
      getOpCode().trackUpperBitsOnReg(reg, cg);

      // Check the live discardable register list to see if this is the first
      // instruction that kills the rematerializable range of a register.
      //
      if (cg->enableRematerialisation() &&
          reg->isDiscardable() &&
          getOpCode().modifiesTarget())
         {
         TR::ClobberingInstruction *clob = new (cg->trHeapMemory()) TR::ClobberingInstruction(this, cg->trMemory());
         clob->addClobberedRegister(reg);
         cg->addClobberingInstruction(clob);
         cg->removeLiveDiscardableRegister(reg);
         cg->clobberLiveDependentDiscardableRegisters(clob, reg);

         if (debug("dumpRemat"))
            {
            diagnostic("---> Clobbering %s discardable register %s at instruction %p\n",
                        reg->getRematerializationInfo()->toString(comp), reg->getRegisterName(comp), this);
            }
         }
      }

   X86RegInstruction(TR::RegisterDependencyConditions *cond,
                        TR::Register *reg,
                        TR_X86OpCodes op,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg)
      : TR::Instruction(cond, op, precedingInstruction, cg), _targetRegister(reg)
      {
      useRegister(reg);
      getOpCode().trackUpperBitsOnReg(reg, cg);
      }

   X86RegInstruction(TR_X86OpCodes op, TR::Node * node, TR::Register *reg, TR::CodeGenerator *cg);

   X86RegInstruction(TR::Instruction *precedingInstruction,
                        TR_X86OpCodes op,
                        TR::Register *reg,
                        TR::CodeGenerator *cg);

   X86RegInstruction(TR_X86OpCodes op,
                        TR::Node *node,
                        TR::Register *reg,
                        TR::RegisterDependencyConditions *cond,
                        TR::CodeGenerator *cg);

   X86RegInstruction(TR::Instruction *precedingInstruction,
                        TR_X86OpCodes op,
                        TR::Register *reg,
                        TR::RegisterDependencyConditions *cond,
                        TR::CodeGenerator *cg);

   virtual char *description() { return "X86Reg"; }

   virtual Kind getKind() { return IsReg; }

   virtual TR::X86RegInstruction  *getX86RegInstruction();

   virtual TR::X86RegRegInstruction  *getIA32RegRegInstruction() {return NULL;}

   virtual TR::X86RegMemInstruction  *getIA32RegMemInstruction() {return NULL;}

   virtual TR::Register *getTargetRegister()               {return _targetRegister;}
   TR::Register *setTargetRegister(TR::Register *r) {return (_targetRegister = r);}

   void applyTargetRegisterToModRMByte(uint8_t *modRM)
      {
      TR::RealRegister *target = toRealRegister(_targetRegister);
      if (getOpCode().hasTargetRegisterInModRM())
         {
         target->setRMRegisterFieldInModRM(modRM);
         }
      else if (getOpCode().hasTargetRegisterInOpcode())
         {
         target->setRegisterFieldInOpcode(modRM);
         }
      else
         {
         // If not in RM field and not in opcode, then must have a reg field in ModRM byte
         //
         target->setRegisterFieldInModRM(modRM);
         }
      }
#if defined(TR_TARGET_64BIT)
   virtual uint8_t rexBits()
      {
      return operandSizeRexBits() | targetRegisterRexBits();
      }

   uint8_t targetRegisterRexBits()
      {
      // Determine where the 4th register bit (if any) should go
      //
      uint8_t rxbBitmask;
      if (getOpCode().hasTargetRegisterInModRM())
         rxbBitmask = TR::RealRegister::REX_B;
      else if (getOpCode().hasTargetRegisterInOpcode())
         rxbBitmask = TR::RealRegister::REX_B;
      else // If not in RM field and not in opcode, then must have a reg field in ModRM byte
         rxbBitmask = TR::RealRegister::REX_R;
      // Add the appropriate bits to the Rex prefix byte
      //
      TR::RealRegister *target = toRealRegister(_targetRegister);
      return target->rexBits(rxbBitmask, getOpCode().hasByteTarget() ? true : false);
      }
#endif
   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual OMR::X86::EnlargementResult  enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement);
   virtual void     assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool     refsRegister(TR::Register *reg);
   virtual bool     defsRegister(TR::Register *reg);
   virtual bool     usesRegister(TR::Register *reg);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 1; };
#endif

   };


class X86RegRegInstruction : public TR::X86RegInstruction
   {
   TR::Register *_sourceRegister;

   public:

   X86RegRegInstruction(TR::Register      *sreg,
                            TR::Register      *treg,
                            TR::Node          *node,
                            TR_X86OpCodes    op,
                            TR::CodeGenerator *cg)
      : TR::X86RegInstruction(treg, node, op, cg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86RegRegInstruction(TR::Register *sreg,
                           TR::Register *treg,
                           TR_X86OpCodes op,
                           TR::Instruction *precedingInstruction,
                           TR::CodeGenerator *cg)
      : TR::X86RegInstruction(treg, op, precedingInstruction, cg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86RegRegInstruction(TR::RegisterDependencyConditions  *cond,
                            TR::Register                         *sreg,
                            TR::Register                         *treg,
                            TR::Node                             *node,
                            TR_X86OpCodes                       op,
                            TR::CodeGenerator                    *cg)
      : TR::X86RegInstruction(cond, treg, node, op, cg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86RegRegInstruction(TR::RegisterDependencyConditions *cond,
                           TR::Register *sreg,
                           TR::Register *treg,
                           TR_X86OpCodes op,
                           TR::Instruction *precedingInstruction,
                           TR::CodeGenerator *cg)
      : TR::X86RegInstruction(cond, treg, op, precedingInstruction, cg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86RegRegInstruction(TR_X86OpCodes    op,
                            TR::Node          *node,
                            TR::Register      *treg,
                            TR::Register      *sreg,
                            TR::CodeGenerator *cg);

   X86RegRegInstruction(TR::Instruction *precedingInstruction,
                           TR_X86OpCodes op,
                           TR::Register *treg,
                           TR::Register *sreg,
                           TR::CodeGenerator *cg);

   X86RegRegInstruction(TR_X86OpCodes                       op,
                            TR::Node                             *node,
                            TR::Register                         *treg,
                            TR::Register                         *sreg,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

   X86RegRegInstruction(TR::Instruction *precedingInstruction,
                            TR_X86OpCodes                       op,
                            TR::Register                         *treg,
                            TR::Register                         *sreg,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

   virtual char *description() { return "X86RegReg"; }

   virtual Kind getKind() { return IsRegReg; }

   virtual TR::X86RegRegInstruction  *getIA32RegRegInstruction() {return this;}

   virtual TR::Register *getSourceRegister()                {return _sourceRegister;}
   TR::Register *setSourceRegister(TR::Register *sr) {return (_sourceRegister = sr);}

   void applySourceRegisterToModRMByte(uint8_t *modRM)
      {
      TR::RealRegister *source = toRealRegister(_sourceRegister);
      if (getOpCode().hasSourceRegisterInModRM())
         {
         source->setRMRegisterFieldInModRM(modRM);
         }
      else
         {
         // If not in RM field, then must be register field in ModRM byte
         //
         source->setRegisterFieldInModRM(modRM);
         }
      }

#if defined(TR_TARGET_64BIT)
   virtual uint8_t rexBits()
      {
      return operandSizeRexBits() | targetRegisterRexBits() | sourceRegisterRexBits();
      }

   uint8_t sourceRegisterRexBits()
      {
      // Determine where the 4th register bit (if any) should go
      //
      uint8_t rxbBitmask;
      if (getOpCode().hasSourceRegisterInModRM())
         rxbBitmask = TR::RealRegister::REX_B;
      else // If not in RM field, then must have a reg field in ModRM byte
         rxbBitmask = TR::RealRegister::REX_R;
      // Add the appropriate bits to the Rex prefix byte
      //
      TR::RealRegister *source = toRealRegister(_sourceRegister);
      return source->rexBits(rxbBitmask, getOpCode().hasByteSource() ? true : false);
      }
#endif

   virtual uint8_t* generateOperand(uint8_t* cursor);

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 2; }
#endif
   };


class X86RegImmInstruction : public TR::X86RegInstruction
   {
   int32_t _sourceImmediate;
   int32_t _reloKind;

   public:

   X86RegImmInstruction(int32_t           imm,
                           TR::Register      *treg,
                           TR::Node          *node,
                           TR_X86OpCodes     op,
                           TR::CodeGenerator *cg,
                           int32_t           reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(treg, node, op, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   X86RegImmInstruction(int32_t           imm,
                           TR::Register      *treg,
                           TR_X86OpCodes     op,
                           TR::Instruction *precedingInstruction,
                           TR::CodeGenerator *cg,
                           int32_t           reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(treg, op, precedingInstruction, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   X86RegImmInstruction(TR_X86OpCodes    op,
                           TR::Node          *node,
                           TR::Register      *treg,
                           int32_t           imm,
                           TR::CodeGenerator *cg,
                           int32_t           reloKind=TR_NoRelocation);

   X86RegImmInstruction(TR::Instruction   *precedingInstruction,
                           TR_X86OpCodes    op,
                           TR::Register      *treg,
                           int32_t           imm,
                           TR::CodeGenerator *cg,
                           int32_t           reloKind=TR_NoRelocation);

   X86RegImmInstruction(TR_X86OpCodes                       op,
                           TR::Node                             *node,
                           TR::Register                         *treg,
                           int32_t                              imm,
                           TR::RegisterDependencyConditions  *cond,
                           TR::CodeGenerator                    *cg,
                           int32_t                              reloKind=TR_NoRelocation);

   X86RegImmInstruction(TR::Instruction                      *precedingInstruction,
                           TR_X86OpCodes                       op,
                           TR::Register                         *treg,
                           int32_t                              imm,
                           TR::RegisterDependencyConditions  *cond,
                           TR::CodeGenerator                    *cg,
                           int32_t                              reloKind=TR_NoRelocation);

   virtual char *description() { return "X86RegImm"; }

   virtual Kind getKind() { return IsRegImm; }

   int32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t getSourceImmediateAsAddress()  {return (uint32_t)_sourceImmediate;}
   int32_t setSourceImmediate(int32_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg);

   int32_t getReloKind()               { return _reloKind;     }
   void setReloKind(int32_t reloKind)  { _reloKind = reloKind; }
   };


class X86RegImmSymInstruction : public TR::X86RegImmInstruction
   {
   private:
   TR::SymbolReference *_symbolReference;
   void autoSetReloKind();

   public:

   X86RegImmSymInstruction(TR_X86OpCodes          op,
                              TR::Node                *node,
                              TR::Register            *reg,
                              int32_t                 imm,
                              TR::SymbolReference     *sr,
                              TR::CodeGenerator       *cg);

   X86RegImmSymInstruction(TR::Instruction         *precedingInstruction,
                              TR_X86OpCodes          op,
                              TR::Register            *reg,
                              int32_t                 imm,
                              TR::SymbolReference     *sr,
                              TR::CodeGenerator       *cg);

   virtual char *description() { return "X86RegImmSym"; }

   virtual Kind getKind() { return IsRegImmSym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }


   virtual uint8_t* generateOperand(uint8_t* cursor);

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);
   };


class X86RegRegImmInstruction : public TR::X86RegRegInstruction
   {
   int32_t _sourceImmediate;

   public:

   X86RegRegImmInstruction(TR_X86OpCodes     op,
                              TR::Node          *node,
                              TR::Register      *treg,
                              TR::Register      *sreg,
                              int32_t           imm,
                              TR::CodeGenerator *cg);

   X86RegRegImmInstruction(TR::Instruction   *precedingInstruction,
                              TR_X86OpCodes     op,
                              TR::Register      *treg,
                              TR::Register      *sreg,
                              int32_t           imm,
                              TR::CodeGenerator *cg);

   virtual char *description() { return "X86RegRegImm"; }

   virtual Kind getKind() { return IsRegRegImm; }

   int32_t getSourceImmediate()           {return _sourceImmediate;}
   uint32_t getSourceImmediateAsAddress()  {return (uint32_t)_sourceImmediate;}
   int32_t setSourceImmediate(int32_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 2; }
#endif

   };


class X86RegRegRegInstruction : public TR::X86RegRegInstruction
   {
   TR::Register *_source2ndRegister;

   public:

   X86RegRegRegInstruction(TR::RegisterDependencyConditions  *cond,
                               TR::Register                         *srreg,
                               TR::Register                         *slreg,
                               TR::Register                         *treg,
                               TR::Node                             *node,
                               TR_X86OpCodes                       op,
                               TR::CodeGenerator                    *cg)
      : TR::X86RegRegInstruction(cond, srreg, treg, node, op, cg), _source2ndRegister(slreg)
      {
      useRegister(slreg);
      }

   X86RegRegRegInstruction(TR::RegisterDependencyConditions  *cond,
                               TR::Register                         *srreg,
                               TR::Register                         *slreg,
                               TR::Register                         *treg,
                               TR_X86OpCodes                       op,
                               TR::Instruction                      *precedingInstruction,
                               TR::CodeGenerator                    *cg)
      : TR::X86RegRegInstruction(cond, srreg, treg, op, precedingInstruction, cg), _source2ndRegister(slreg)
      {
      useRegister(slreg);
      }

   X86RegRegRegInstruction(TR::Register      *srreg,
                               TR::Register      *slreg,
                               TR::Register      *treg,
                               TR::Node          *node,
                               TR_X86OpCodes    op,
                               TR::CodeGenerator *cg)
      : TR::X86RegRegInstruction(srreg, treg, node, op, cg), _source2ndRegister(slreg)
      {
      useRegister(slreg);
      }

   X86RegRegRegInstruction(TR::Register      *srreg,
                               TR::Register      *slreg,
                               TR::Register      *treg,
                               TR_X86OpCodes   op,
                               TR::Instruction   *precedingInstruction,
                               TR::CodeGenerator *cg)
      : TR::X86RegRegInstruction(srreg, treg, op, precedingInstruction, cg), _source2ndRegister(slreg)
      {
      useRegister(slreg);
      }

   X86RegRegRegInstruction(TR_X86OpCodes    op,
                               TR::Node          *node,
                               TR::Register      *treg,
                               TR::Register      *slreg,
                               TR::Register      *srreg,
                               TR::CodeGenerator *cg);

   X86RegRegRegInstruction(TR::Instruction   *precedingInstruction,
                               TR_X86OpCodes    op,
                               TR::Register      *treg,
                               TR::Register      *slreg,
                               TR::Register      *srreg,
                               TR::CodeGenerator *cg);

   X86RegRegRegInstruction(TR_X86OpCodes                       op,
                               TR::Node                             *node,
                               TR::Register                         *treg,
                               TR::Register                         *slreg,
                               TR::Register                         *srreg,
                               TR::RegisterDependencyConditions  *cond,
                               TR::CodeGenerator                    *cg);

   X86RegRegRegInstruction(TR::Instruction                      *precedingInstruction,
                               TR_X86OpCodes                       op,
                               TR::Register                         *treg,
                               TR::Register                         *slreg,
                               TR::Register                         *srreg,
                               TR::RegisterDependencyConditions  *cond,
                               TR::CodeGenerator                    *cg);

   virtual char *description() { return "X86RegRegReg"; }

   virtual Kind getKind() { return IsRegRegReg; }

   /** \brief
   *    Getter of the second source register
   *
   *  \return
   *    The second source register
   */
   virtual TR::Register *getSource2ndRegister()         {return _source2ndRegister;}
   /** \brief
   *    Setter of the second source register
   *
   *  \param sr
   *    The new second source register
   *
   *  \return
   *    The second source register
   */
   TR::Register *setSource2ndRegister(TR::Register *sr) {return (_source2ndRegister = sr);}

   /** \brief
   *    Fill operand bytes
   *
   *  \param cursor
   *    The address to the first operand byte
   *
   *  \return
   *    The address after last operand byte
   */
   virtual uint8_t* generateOperand(uint8_t* cursor);

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   /** \brief
   *    Fill vvvv field in a VEX prefix
   *
   *  \param opcodeByte
   *    The address of VEX prefix byte containing vvvv field
   */
   void applySource2ndRegisterToVEX(uint8_t *vex)
      {
      TR::RealRegister *source = toRealRegister(_source2ndRegister);
      source->setRegisterFieldInVEX(vex);
      }

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 3; }
#endif
   };


class X86MemInstruction : public TR::Instruction
   {
   TR::MemoryReference  *_memoryReference;

   public:

   X86MemInstruction(TR::Node * node, TR_X86OpCodes op, TR::CodeGenerator *cg, TR::Register *srcReg = NULL)
      : TR::Instruction(node, op, cg), _memoryReference(NULL)
      {
      }

   X86MemInstruction(TR::MemoryReference  *mr,
                         TR::Node                *node,
                         TR_X86OpCodes          op,
                         TR::CodeGenerator       *cg,
                         TR::Register *srcReg = NULL)
      : TR::Instruction(node, op, cg), _memoryReference(mr)
      {
      mr->useRegisters(this, cg);
      if (mr->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, mr, cg);
         }

      if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport))
         {
         int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

         if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr, srcReg);
         }

      // Find out if this instruction clobbers the memory reference associated with
      // a live discardable register.
      //
      if (cg->enableRematerialisation() &&
          getOpCode().modifiesTarget() &&
          !cg->getLiveDiscardableRegisters().empty())
         {
         cg->clobberLiveDiscardableRegisters(this, mr);
         }
      }

   X86MemInstruction(TR::MemoryReference  *mr,
                         TR_X86OpCodes          op,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg,
                         TR::Register *srcReg = NULL)
      : TR::Instruction(op, precedingInstruction, cg), _memoryReference(mr)
      {
      mr->useRegisters(this, cg);
      if (mr->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, mr, cg);
         }

      if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport))
         {
         int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

         if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr, srcReg);
         }
      }

   X86MemInstruction(TR::RegisterDependencyConditions  *cond,
                         TR::MemoryReference               *mr,
                         TR::Node                             *node,
                         TR_X86OpCodes                       op,
                         TR::CodeGenerator                    *cg,
                         TR::Register *srcReg = NULL)
      : TR::Instruction(cond, node, op, cg), _memoryReference(mr)
      {
      mr->useRegisters(this, cg);
      if (mr->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, mr, cg);
         }

      if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport))
         {
         int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

         if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr);
         }

      // Find out if this instruction clobbers the memory reference associated with
      // a live discardable register.
      //
      if (cg->enableRematerialisation() &&
          getOpCode().modifiesTarget() &&
          !cg->getLiveDiscardableRegisters().empty())
         {
         cg->clobberLiveDiscardableRegisters(this, mr);
         }
      }

   X86MemInstruction(TR::RegisterDependencyConditions  *cond,
                         TR::MemoryReference               *mr,
                         TR_X86OpCodes                       op,
                         TR::Instruction                      *precedingInstruction,
                         TR::CodeGenerator                    *cg,
                         TR::Register *srcReg = NULL)
      : TR::Instruction(cond, op, precedingInstruction, cg), _memoryReference(mr)
      {
      mr->useRegisters(this, cg);
      if (mr->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, mr, cg);
         }

      if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport) && cg->comp()->target().is32Bit())
         {
         int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

         if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr);
         }
      }

   X86MemInstruction(TR_X86OpCodes          op,
                         TR::Node                *node,
                         TR::MemoryReference  *mr,
                         TR::CodeGenerator       *cg,
                         TR::Register            *sreg=NULL);

   X86MemInstruction(TR::Instruction         *precedingInstruction,
                         TR_X86OpCodes          op,
                         TR::MemoryReference  *mr,
                         TR::CodeGenerator       *cg,
                         TR::Register            *sreg=NULL);

   X86MemInstruction(TR_X86OpCodes                       op,
                         TR::Node                             *node,
                         TR::MemoryReference               *mr,
                         TR::RegisterDependencyConditions  *cond,
                         TR::CodeGenerator                    *cg,
                         TR::Register                         *sreg=NULL );

   virtual char *description() { return "X86Mem"; }

   virtual Kind getKind() { return IsMem; }

   virtual TR::MemoryReference  *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference  *setMemoryReference(TR::MemoryReference  *p, TR::CodeGenerator *cg)
      {
      _memoryReference = p;
      if (p->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, p, cg);
         }

      if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport) && cg->comp()->target().is32Bit())
         {
         int32_t barrier = memoryBarrierRequired(this->getOpCode(), p, cg, true);

         if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, p);
         }

      return p;
      }

   virtual bool     needsLockPrefix();
   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual OMR::X86::EnlargementResult enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement);

   virtual TR::Snippet *getSnippetForGC();
   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return _memoryReference->getNumMRReferencedGPRegisters(); }
#endif
#if defined(TR_TARGET_64BIT)
   virtual uint8_t rexBits()
      {
      return operandSizeRexBits() | _memoryReference->rexBits();
      }
#endif
   };


class X86MemTableInstruction : public TR::X86MemInstruction
   {
   TR::LabelRelocation **_relocations;
   ncount_t _numRelocations, _capacity;

   public:

   X86MemTableInstruction(TR_X86OpCodes op, TR::Node *node, TR::MemoryReference *mr, ncount_t numEntries, TR::CodeGenerator *cg):
      TR::X86MemInstruction(op, node, mr, cg),_numRelocations(0),_capacity(numEntries)
      {
      _relocations = (TR::LabelRelocation**)cg->trMemory()->allocateHeapMemory(numEntries * sizeof(_relocations[0]));
      }

   X86MemTableInstruction(TR_X86OpCodes op, TR::Node *node, TR::MemoryReference *mr, ncount_t numEntries, TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg):
      TR::X86MemInstruction(op, node, mr, deps, cg),_numRelocations(0),_capacity(numEntries)
      {
      _relocations = (TR::LabelRelocation**)cg->trMemory()->allocateHeapMemory(numEntries * sizeof(_relocations[0]));
      }

   virtual char *description() { return "X86MemTable"; }

   virtual Kind getKind() { return IsMemTable; }

   ncount_t getNumRelocations()    { return _numRelocations; }
   ncount_t getRelocationCapacity(){ return _capacity; }

   void addRelocation(TR::LabelRelocation *r)
      {
      TR_ASSERT(_numRelocations < _capacity, "Can't add another relocation to a X86MemTableInstruction that is already full");
      _relocations[_numRelocations++] = r;
      }

   TR::LabelRelocation *getRelocation(ncount_t index)
      {
      TR_ASSERT(0 <= index && index < _numRelocations, "X86MemTableInstruction::getRelocation - index %d is out of range (0-%d)", index, _numRelocations);
      return _relocations[index];
      }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);

   };


class X86CallMemInstruction : public TR::X86MemInstruction
   {
   int32_t _adjustsFramePointerBy; // TODO: Rename me.  Calls don't affect the VFP per se; they affect the stack pointer

   public:

   // TODO: roll this entire class into the TR::X86MemInstruction  class.
   //
   X86CallMemInstruction(TR_X86OpCodes                       op,
                             TR::Node                             *node,
                             TR::MemoryReference               *mr,
                             TR::RegisterDependencyConditions  *cond,
                             TR::CodeGenerator                    *cg);

   X86CallMemInstruction(TR::Instruction                      *precedingInstruction,
                             TR_X86OpCodes                       op,
                             TR::MemoryReference               *mr,
                             TR::RegisterDependencyConditions  *cond,
                             TR::CodeGenerator                    *cg);

   X86CallMemInstruction(TR_X86OpCodes                       op,
                             TR::Node                             *node,
                             TR::MemoryReference               *mr,
                             TR::CodeGenerator                    *cg);

   virtual char *description() { return "X86CallMem"; }

   virtual Kind getKind() { return IsCallMem; }

   void setAdjustsFramePointerBy(int32_t a) {_adjustsFramePointerBy = a;}
   int32_t getAdjustsFramePointerBy()       {return _adjustsFramePointerBy;}

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg){ adjustVFPStateForCall(state, _adjustsFramePointerBy, cg); }
   };


class X86MemImmInstruction : public TR::X86MemInstruction
   {
   int32_t _sourceImmediate;
   int32_t _reloKind;

   public:

   X86MemImmInstruction(int32_t                imm,
                           TR::MemoryReference  *mr,
                           TR::Node                *node,
                           TR_X86OpCodes          op,
                           TR::CodeGenerator       *cg,
                           int32_t                reloKind = TR_NoRelocation)
      : TR::X86MemInstruction(mr, node, op, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   X86MemImmInstruction(int32_t                 imm,
                           TR::MemoryReference  *mr,
                           TR_X86OpCodes           op,
                           TR::Instruction         *precedingInstruction,
                           TR::CodeGenerator       *cg,
                           int32_t                reloKind = TR_NoRelocation)
      : TR::X86MemInstruction(mr, op, precedingInstruction, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   X86MemImmInstruction(TR_X86OpCodes          op,
                           TR::Node                *node,
                           TR::MemoryReference  *mr,
                           int32_t                 imm,
                           TR::CodeGenerator       *cg,
                           int32_t                reloKind = TR_NoRelocation);

   X86MemImmInstruction(TR::Instruction         *precedingInstruction,
                           TR_X86OpCodes           op,
                           TR::MemoryReference  *mr,
                           int32_t                 imm,
                           TR::CodeGenerator       *cg,
                           int32_t                reloKind = TR_NoRelocation);

   virtual char *description() { return "X86MemImm"; }

   virtual Kind getKind() { return IsMemImm; }

   int32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t getSourceImmediateAsAddress()  {return (uint32_t)_sourceImmediate;}
   int32_t setSourceImmediate(int32_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);
   };


class X86MemImmSymInstruction : public TR::X86MemImmInstruction
   {
   TR::SymbolReference *_symbolReference;

   public:

   X86MemImmSymInstruction(TR_X86OpCodes           op,
                              TR::Node                *node,
                              TR::MemoryReference  *mr,
                              int32_t                 imm,
                              TR::SymbolReference     *sr,
                              TR::CodeGenerator       *cg);

   X86MemImmSymInstruction(TR::Instruction         *precedingInstruction,
                              TR_X86OpCodes           op,
                              TR::MemoryReference  *mr,
                              int32_t                 imm,
                              TR::SymbolReference     *sr,
                              TR::CodeGenerator       *cg);

   virtual char *description() { return "X86MemImmSym"; }

   virtual Kind getKind() { return IsMemImmSym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);
   };


class X86MemRegInstruction : public TR::X86MemInstruction
   {
   TR::Register *_sourceRegister;

   public:

   X86MemRegInstruction(TR::Register            *sreg,
                            TR::MemoryReference  *mr,
                            TR::Node                *node,
                            TR_X86OpCodes          op,
                            TR::CodeGenerator       *cg)
      : TR::X86MemInstruction(mr, node, op, cg, sreg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86MemRegInstruction(TR::Register            *sreg,
                            TR::MemoryReference  *mr,
                            TR_X86OpCodes          op,
                            TR::Instruction         *precedingInstruction,
                            TR::CodeGenerator       *cg)
      : TR::X86MemInstruction(mr, op, precedingInstruction, cg, sreg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86MemRegInstruction(TR::RegisterDependencyConditions  *cond,
                            TR::Register                         *sreg,
                            TR::MemoryReference               *mr,
                            TR::Node                             *node,
                            TR_X86OpCodes                       op,
                            TR::CodeGenerator                    *cg)
      : TR::X86MemInstruction(cond, mr, node, op, cg, sreg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86MemRegInstruction(TR::RegisterDependencyConditions  *cond,
                            TR::Register                         *sreg,
                            TR::MemoryReference               *mr,
                            TR_X86OpCodes                       op,
                            TR::Instruction                      *precedingInstruction,
                            TR::CodeGenerator                    *cg)
      : TR::X86MemInstruction(cond, mr, op, precedingInstruction, cg, sreg), _sourceRegister(sreg)
      {
      useRegister(sreg);
      }

   X86MemRegInstruction(TR_X86OpCodes          op,
                            TR::Node                *node,
                            TR::MemoryReference  *mr,
                            TR::Register            *sreg,
                            TR::CodeGenerator       *cg);

   X86MemRegInstruction(TR::Instruction         *precedingInstruction,
                            TR_X86OpCodes          op,
                            TR::MemoryReference  *mr,
                            TR::Register            *sreg,
                            TR::CodeGenerator       *cg);

   X86MemRegInstruction(TR_X86OpCodes                       op,
                            TR::Node                             *node,
                            TR::MemoryReference               *mr,
                            TR::Register                         *sreg,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

   X86MemRegInstruction(TR::Instruction                      *precedingInstruction,
                            TR_X86OpCodes                       op,
                            TR::MemoryReference               *mr,
                            TR::Register                         *sreg,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

   virtual char *description() { return "X86MemReg"; }

   virtual Kind getKind() { return IsMemReg; }

   virtual TR::Register *getSourceRegister()                {return _sourceRegister;}
   TR::Register *setSourceRegister(TR::Register *sr) {return (_sourceRegister = sr);}

   virtual uint8_t* generateOperand(uint8_t* cursor);

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 1 + getMemoryReference()->getNumMRReferencedGPRegisters(); }
#endif
#if defined(TR_TARGET_64BIT)
   virtual uint8_t rexBits()
      {
      return
           operandSizeRexBits()
         | getMemoryReference()->rexBits()
         | toRealRegister(_sourceRegister)->rexBits(TR::RealRegister::REX_R, getOpCode().hasByteSource() ? true : false)
         ;
      }
#endif
   };


class X86MemRegImmInstruction : public TR::X86MemRegInstruction
   {
   int32_t _sourceImmediate;

   public:

   X86MemRegImmInstruction(TR_X86OpCodes          op,
                              TR::Node               *node,
                              TR::MemoryReference *mr,
                              TR::Register           *sreg,
                              int32_t                imm,
                              TR::CodeGenerator      *cg);

   X86MemRegImmInstruction(TR::Instruction        *precedingInstruction,
                              TR_X86OpCodes          op,
                              TR::MemoryReference *mr,
                              TR::Register           *sreg,
                              int32_t                imm,
                              TR::CodeGenerator      *cg);

   virtual char *description() { return "X86MemRegImm"; }

   virtual Kind getKind() { return IsMemRegImm; }

   int32_t getSourceImmediate() {return _sourceImmediate;}
   uint32_t getSourceImmediateAsAddress()  {return (uint32_t)_sourceImmediate;}
   int32_t setSourceImmediate(int32_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   };


class X86RegMemInstruction : public TR::X86RegInstruction
   {
   protected:

   TR::MemoryReference *_memoryReference;

   public:

   X86RegMemInstruction(TR::MemoryReference *mr,
                           TR::Register *treg,
                           TR::Node *node,
                           TR_X86OpCodes op,
                           TR::CodeGenerator *cg)
       : TR::X86RegInstruction(treg, node, op, cg), _memoryReference(mr)
      {
      mr->useRegisters(this, cg);
      if (mr->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, mr, cg);
         }

      // Find out if this instruction clobbers the memory reference associated with
      // a live discardable register.
      //
      if (cg->enableRematerialisation() &&
         (op == LEA2RegMem || op ==  LEA4RegMem || op == LEA8RegMem) &&
         !cg->getLiveDiscardableRegisters().empty())
         {
         cg->clobberLiveDiscardableRegisters(this, mr);
         }
      }

// check constructor op order
   X86RegMemInstruction(TR::MemoryReference  *mr,
                            TR::Register            *treg,
                            TR_X86OpCodes          op,
                            TR::Instruction         *precedingInstruction,
                            TR::CodeGenerator       *cg)
      : TR::X86RegInstruction(treg, op, precedingInstruction, cg),
        _memoryReference(mr)
      {
      mr->useRegisters(this, cg);
      if (mr->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, mr, cg);
         }
      }

   X86RegMemInstruction(TR_X86OpCodes          op,
                            TR::Node                *node,
                            TR::Register            *treg,
                            TR::MemoryReference  *mr,
                            TR::CodeGenerator       *cg);

   X86RegMemInstruction(TR::Instruction         *precedingInstruction,
                            TR_X86OpCodes          op,
                            TR::Register            *treg,
                            TR::MemoryReference  *mr,
                            TR::CodeGenerator       *cg);

   X86RegMemInstruction(TR_X86OpCodes                       op,
                            TR::Node                             *node,
                            TR::Register                         *treg,
                            TR::MemoryReference               *mr,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

   X86RegMemInstruction(TR::Instruction                      *precedingInstruction,
                            TR_X86OpCodes                       op,
                            TR::Register                         *treg,
                            TR::MemoryReference               *mr,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

   virtual char *description() { return "X86RegMem"; }

   virtual Kind getKind() { return IsRegMem; }

   virtual X86RegMemInstruction  *getIA32RegMemInstruction() {return this;}

   virtual TR::MemoryReference  *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference  *setMemoryReference(TR::MemoryReference  *p, TR::CodeGenerator *cg)
      {
      _memoryReference = p;
      if (p->getUnresolvedDataSnippet() != NULL)
         {
         padUnresolvedReferenceInstruction(this, p, cg);
         }

      if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport) && cg->comp()->target().is32Bit())
         {
         int32_t barrier = memoryBarrierRequired(this->getOpCode(), p, cg, true);

         if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, p);
        }
      return p;
      }

   virtual bool     needsLockPrefix();
   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual OMR::X86::EnlargementResult enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement);

   virtual void        assignRegisters(TR_RegisterKinds kindsToBeAssigned);

   virtual TR::Snippet *getSnippetForGC();
   virtual bool        refsRegister(TR::Register *reg);
   virtual bool        usesRegister(TR::Register *reg);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 1 + _memoryReference->getNumMRReferencedGPRegisters(); }
#endif
#if defined(TR_TARGET_64BIT)
   virtual uint8_t rexBits()
      {
      return
           operandSizeRexBits()
         | toRealRegister(getTargetRegister())->rexBits(TR::RealRegister::REX_R, getOpCode().hasByteTarget() ? true : false)
         | getMemoryReference()->rexBits()
         ;
      }
#endif
   };


class X86RegMemImmInstruction : public TR::X86RegMemInstruction
   {
   int32_t _sourceImmediate;

   public:

   X86RegMemImmInstruction(TR_X86OpCodes          op,
                              TR::Node               *node,
                              TR::Register           *treg,
                              TR::MemoryReference *mr,
                              int32_t                imm,
                              TR::CodeGenerator      *cg);

   X86RegMemImmInstruction(TR::Instruction        *precedingInstruction,
                              TR_X86OpCodes          op,
                              TR::Register           *treg,
                              TR::MemoryReference *mr,
                              int32_t                imm,
                              TR::CodeGenerator      *cg);

   virtual char *description() { return "X86RegMemImm"; }

   virtual Kind getKind() { return IsRegMemImm; }

   int32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t getSourceImmediateAsAddress()  {return (uint32_t)_sourceImmediate;}
   int32_t setSourceImmediate(int32_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   };


class X86RegRegMemInstruction : public TR::X86RegMemInstruction
   {
   TR::Register *_source2ndRegister;

   public:

   X86RegRegMemInstruction(TR_X86OpCodes        op,
                           TR::Node            *node,
                           TR::Register        *slreg,
                           TR::Register        *srreg,
                           TR::MemoryReference *mr,
                           TR::CodeGenerator   *cg);
   X86RegRegMemInstruction(TR_X86OpCodes                     op,
                           TR::Node                         *node,
                           TR::Register                     *slreg,
                           TR::Register                     *srreg,
                           TR::MemoryReference              *mr,
                           TR::RegisterDependencyConditions *cond,
                           TR::CodeGenerator                *cg);

   virtual char *description() { return "X86RegRegMem"; }

   virtual Kind getKind() { return IsRegRegMem; }

   /** \brief
   *    Getter of the second source register
   *
   *  \return
   *    The second source register
   */
   virtual TR::Register *getSource2ndRegister()          {return _source2ndRegister;}
   /** \brief
   *    Setter of the second source register
   *
   *  \param sr
   *    The new second source register
   *
   *  \return
   *    The second source register
   */
   TR::Register *setSource2ndRegister(TR::Register *srr) {return (_source2ndRegister = srr);}

   /** \brief
   *    Fill operand bytes
   *
   *  \param cursor
   *    The address to the first operand byte
   *
   *  \return
   *    The address after last operand byte
   */
   virtual uint8_t* generateOperand(uint8_t* cursor);

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   /** \brief
   *    Fill vvvv field in a VEX prefix
   *
   *  \param opcodeByte
   *    The address of VEX prefix byte containing vvvv field
   */
   void applySource2ndRegisterToVEX(uint8_t *vex)
      {
      TR::RealRegister *source = toRealRegister(_source2ndRegister);
      source->setRegisterFieldInVEX(vex);
      }

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 2 + getMemoryReference()->getNumMRReferencedGPRegisters(); }
#endif
   };


class X86FPRegInstruction : public TR::X86RegInstruction
   {
   public:

   X86FPRegInstruction(TR_X86OpCodes    op,
                           TR::Node          *node,
                           TR::Register      *reg,
                           TR::CodeGenerator *cg);

   X86FPRegInstruction(TR::Instruction   *precedingInstruction,
                           TR_X86OpCodes    op,
                           TR::Register      *reg,
                           TR::CodeGenerator *cg);

   virtual Kind getKind() { return IsFPReg; }

   void applyTargetRegisterToOpCode(uint8_t *opCode)
      {
      TR::RealRegister *target = toRealRegister(getTargetRegister());
      target->setRegisterFieldInOpcode(opCode);
      }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 0; }
   virtual uint32_t getNumOperandReferencedFPRegisters() { return 1; }
#endif
   };


class AMD64RegImm64Instruction : public TR::X86RegInstruction
   {
   uint64_t _sourceImmediate;
   int32_t _reloKind;

   public:

   AMD64RegImm64Instruction(uint64_t          imm,
                               TR::Register      *treg,
                               TR::Node          *node,
                               TR_X86OpCodes     op,
                               TR::CodeGenerator *cg,
                               int32_t           reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(treg, node, op, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   AMD64RegImm64Instruction(uint64_t          imm,
                               TR::Register      *treg,
                               TR_X86OpCodes     op,
                               TR::Instruction   *precedingInstruction,
                               TR::CodeGenerator *cg,
                               int32_t           reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(treg, op, precedingInstruction, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   AMD64RegImm64Instruction(TR_X86OpCodes     op,
                               TR::Node          *node,
                               TR::Register      *treg,
                               uint64_t          imm,
                               TR::CodeGenerator *cg,
                               int32_t           reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(treg, node, op, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   AMD64RegImm64Instruction(TR::Instruction   *precedingInstruction,
                               TR_X86OpCodes     op,
                               TR::Register      *treg,
                               uint64_t          imm,
                               TR::CodeGenerator *cg,
                               int32_t           reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(treg, op, precedingInstruction, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   AMD64RegImm64Instruction(TR_X86OpCodes                        op,
                               TR::Node                             *node,
                               TR::Register                         *treg,
                               uint64_t                             imm,
                               TR::RegisterDependencyConditions  *cond,
                               TR::CodeGenerator                    *cg,
                               int32_t                              reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(cond, treg, node, op, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   AMD64RegImm64Instruction(TR::Instruction                      *precedingInstruction,
                               TR_X86OpCodes                        op,
                               TR::Register                         *treg,
                               uint64_t                             imm,
                               TR::RegisterDependencyConditions  *cond,
                               TR::CodeGenerator                    *cg,
                               int32_t                              reloKind=TR_NoRelocation)
      : TR::X86RegInstruction(cond, treg, op, precedingInstruction, cg), _sourceImmediate(imm), _reloKind(reloKind) {}

   virtual char *description() { return "AMD64RegImm64"; }

   virtual Kind getKind() { return IsRegImm64; }

   uint64_t getSourceImmediate()            {return _sourceImmediate;}
   uint64_t setSourceImmediate(uint64_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);

   int32_t getReloKind()               { return _reloKind;     }
   void setReloKind(int32_t reloKind)  { _reloKind = reloKind; }
   };


class AMD64RegImm64SymInstruction : public TR::AMD64RegImm64Instruction
   {
   private:

   TR::SymbolReference *_symbolReference;
   void autoSetReloKind();

   public:

   AMD64RegImm64SymInstruction(TR_X86OpCodes           op,
                                  TR::Node                *node,
                                  TR::Register            *reg,
                                  uint64_t                imm,
                                  TR::SymbolReference     *sr,
                                  TR::CodeGenerator       *cg);

   AMD64RegImm64SymInstruction(TR::Instruction         *precedingInstruction,
                                  TR_X86OpCodes           op,
                                  TR::Register            *reg,
                                  uint64_t                imm,
                                  TR::SymbolReference     *sr,
                                  TR::CodeGenerator       *cg);

   virtual char *description() { return "AMD64RegImm64Sym"; }

   virtual Kind getKind() { return IsRegImm64Sym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr) {return (_symbolReference = sr);}

   virtual uint8_t* generateOperand(uint8_t* cursor);

   virtual void addMetaDataForCodeAddress(uint8_t *cursor);
   };


class AMD64Imm64Instruction : public TR::Instruction
   {
   uint64_t _sourceImmediate;

   public:

   AMD64Imm64Instruction(uint64_t imm, TR::Node * node, TR_X86OpCodes op, TR::CodeGenerator *cg)
      : TR::Instruction(node, op, cg),
      _sourceImmediate(imm) {}

   AMD64Imm64Instruction(uint64_t          imm,
                            TR_X86OpCodes     op,
                            TR::Instruction   *precedingInstruction,
                            TR::CodeGenerator *cg)
      : TR::Instruction(op, precedingInstruction, cg),
      _sourceImmediate(imm) {}

   AMD64Imm64Instruction(TR::RegisterDependencyConditions *cond,
                            uint64_t                            imm,
                            TR::Node                            *node,
                            TR_X86OpCodes                       op,
                            TR::CodeGenerator                   *cg)
      : TR::Instruction(cond, node, op, cg),
      _sourceImmediate(imm) {}

   AMD64Imm64Instruction(TR::RegisterDependencyConditions *cond,
                            uint64_t                            imm,
                            TR_X86OpCodes                       op,
                            TR::Instruction                     *precedingInstruction,
                            TR::CodeGenerator                   *cg)
      : TR::Instruction(cond, op, precedingInstruction, cg),
      _sourceImmediate(imm)
      {
      if (cond)
         cond->createRegisterAssociationDirective(this, cg);
      }

   AMD64Imm64Instruction(TR_X86OpCodes     op,
                            TR::Node          *node,
                            uint64_t          imm,
                            TR::CodeGenerator *cg)
      : TR::Instruction(node, op, cg), _sourceImmediate(imm) {}

   AMD64Imm64Instruction(TR::Instruction   *precedingInstruction,
                            TR_X86OpCodes     op,
                            uint64_t          imm,
                            TR::CodeGenerator *cg)
      : TR::Instruction(op, precedingInstruction, cg), _sourceImmediate(imm) {}

   AMD64Imm64Instruction(TR_X86OpCodes                        op,
                            TR::Node *                            node,
                            uint64_t                             imm,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg)
      : TR::Instruction(cond, node, op, cg), _sourceImmediate(imm) {}

   AMD64Imm64Instruction(TR::Instruction                      *precedingInstruction,
                            TR_X86OpCodes                        op,
                            uint64_t                             imm,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg)
   : TR::Instruction(cond, op, precedingInstruction, cg), _sourceImmediate(imm)
      {
      if (cond)
         cond->createRegisterAssociationDirective(this, cg);
      }

   virtual char *description() { return "AMD64Imm64"; }

   virtual Kind getKind() { return IsImm64; }

   uint64_t getSourceImmediate()            {return _sourceImmediate;}
   uint64_t setSourceImmediate(uint64_t si) {return (_sourceImmediate = si);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t  getBinaryLengthLowerBound();
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);
   };


class AMD64Imm64SymInstruction : public TR::AMD64Imm64Instruction
   {
   TR::SymbolReference *_symbolReference;

   public:

   AMD64Imm64SymInstruction(TR_X86OpCodes       op,
                               TR::Node            *node,
                               uint64_t            imm,
                               TR::SymbolReference *sr,
                               TR::CodeGenerator   *cg)
      : TR::AMD64Imm64Instruction(imm, node, op, cg), _symbolReference(sr) {}

   AMD64Imm64SymInstruction(TR::Instruction     *precedingInstruction,
                               TR_X86OpCodes       op,
                               uint64_t            imm,
                               TR::SymbolReference *sr,
                               TR::CodeGenerator   *cg)
      : TR::AMD64Imm64Instruction(imm, op, precedingInstruction, cg), _symbolReference(sr) {}

   AMD64Imm64SymInstruction(TR_X86OpCodes                        op,
                               TR::Node                             *node,
                               uint64_t                             imm,
                               TR::SymbolReference                  *sr,
                               TR::RegisterDependencyConditions  *cond,
                               TR::CodeGenerator                    *cg)
      : TR::AMD64Imm64Instruction(cond, imm, node, op, cg), _symbolReference(sr) {}

   AMD64Imm64SymInstruction(TR::Instruction                      *precedingInstruction,
                              TR_X86OpCodes                        op,
                              uint64_t                             imm,
                              TR::SymbolReference                  *sr,
                              TR::RegisterDependencyConditions  *cond,
                              TR::CodeGenerator                    *cg)
      : TR::AMD64Imm64Instruction(cond, imm, op, precedingInstruction, cg), _symbolReference(sr) {}

   virtual char *description() { return "AMD64Imm64Sym"; }

   virtual Kind getKind() { return IsImm64Sym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr) {return (_symbolReference = sr);}

   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual void addMetaDataForCodeAddress(uint8_t *cursor);
   };


class X86FPRegRegInstruction : public TR::X86RegRegInstruction
   {

   public:

   X86FPRegRegInstruction(TR::Register      *sreg,
                              TR::Register      *treg,
                              TR::Node          *node,
                              TR_X86OpCodes    op,
                              TR::CodeGenerator *cg)
      : TR::X86RegRegInstruction(sreg, treg, node, op, cg) {}

   X86FPRegRegInstruction(TR::Register      *sreg,
                              TR::Register      *treg,
                              TR_X86OpCodes    op,
                              TR::Instruction   *precedingInstruction,
                              TR::CodeGenerator *cg)
      : TR::X86RegRegInstruction(sreg, treg, op, precedingInstruction, cg) {}

   X86FPRegRegInstruction(TR_X86OpCodes    op,
                              TR::Node          *node,
                              TR::Register      *treg,
                              TR::Register      *sreg,
                              TR::CodeGenerator *cg);

   X86FPRegRegInstruction(TR::Instruction   *precedingInstruction,
                              TR_X86OpCodes    op,
                              TR::Register      *treg,
                              TR::Register      *sreg,
                              TR::CodeGenerator *cg);

   X86FPRegRegInstruction(TR_X86OpCodes op,
                             TR::Node       *node,
                             TR::Register   *treg,
                             TR::Register   *sreg,
                             TR::RegisterDependencyConditions  *cond,
                             TR::CodeGenerator *cg);

   virtual char *description() { return "X86FPRegReg"; }

   virtual Kind getKind() { return IsRegRegReg; }

   enum EassignmentResults
      {
      kSourceCanBePopped = 0x01,  // source operand can be popped by this instruction
      kTargetCanBePopped = 0x02,  // target operand can be popped by this instruction
      kSourceOnFPStack   = 0x04,  // source operand is on the FP stack
      kTargetOnFPStack   = 0x08   // target operand is on the FP stack
      };

   uint32_t assignTargetSourceRegisters();

   void applyRegistersToOpCode(uint8_t *opCode, TR::Machine * machine)
      {

      // At least one of source and target will be in ST0.
      TR::RealRegister *reg = toRealRegister(getTargetRegister());
      if (reg->getRegisterNumber() != TR::RealRegister::st0)
         {
         reg->setRegisterFieldInOpcode(opCode);
         }
      else
         {
         reg = toRealRegister(getSourceRegister());
         if (reg->getRegisterNumber() != TR::RealRegister::st0)
            {
            reg->setRegisterFieldInOpcode(opCode);
            }
         }
      }

   void applySourceRegisterToOpCode(uint8_t *opCode, TR::Machine * machine)
      {

      TR::RealRegister *reg = toRealRegister(getSourceRegister());
      if (reg->getRegisterNumber() != TR::RealRegister::st0)
         {
         reg->setRegisterFieldInOpcode(opCode);
         }
      }

   void applyTargetRegisterToOpCode(uint8_t *opCode, TR::Machine * machine)
      {

      TR::RealRegister *reg = toRealRegister(getTargetRegister());
      if (reg->getRegisterNumber() != TR::RealRegister::st0)
         {
         reg->setRegisterFieldInOpcode(opCode);
         }
      }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned) {}
   virtual uint8_t* generateOperand(uint8_t* cursor);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return 0; }
   virtual uint32_t getNumOperandReferencedFPRegisters() { return 2; }
#endif
   };


class X86FPST0ST1RegRegInstruction : public TR::X86FPRegRegInstruction
   {
   public:

   X86FPST0ST1RegRegInstruction(TR_X86OpCodes    op,
                                    TR::Node          *node,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::CodeGenerator *cg);

   X86FPST0ST1RegRegInstruction(TR_X86OpCodes    op,
                                    TR::Node          *node,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::RegisterDependencyConditions  *cond,
                                    TR::CodeGenerator *cg);

   X86FPST0ST1RegRegInstruction(TR::Instruction   *precedingInstruction,
                                    TR_X86OpCodes    op,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::CodeGenerator *cg);

   virtual char *description() { return "X86FPST0ST1RegReg"; }

   virtual Kind getKind() { return IsFPST0ST1RegReg; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);
   };


class X86FPST0STiRegRegInstruction : public TR::X86FPRegRegInstruction
   {
   public:

   X86FPST0STiRegRegInstruction(TR_X86OpCodes    op,
                                    TR::Node          *node,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::CodeGenerator *cg);

   X86FPST0STiRegRegInstruction(TR::Instruction   *precedingInstruction,
                                    TR_X86OpCodes    op,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::CodeGenerator *cg);

   virtual char *description() { return "X86FPST0STiRegReg"; }

   virtual Kind getKind() { return IsFPST0STiRegReg; }


   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);
   };


class X86FPSTiST0RegRegInstruction : public TR::X86FPRegRegInstruction
   {
   public:

   X86FPSTiST0RegRegInstruction(TR_X86OpCodes    op,
                                    TR::Node          *node,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::CodeGenerator *cg, bool forcePop = false);

   X86FPSTiST0RegRegInstruction(TR::Instruction   *precedingInstruction,
                                    TR_X86OpCodes    op,
                                    TR::Register      *treg,
                                    TR::Register      *sreg,
                                    TR::CodeGenerator *cg, bool forcePop = false);

   virtual char *description() { return "X86FPSTiST0RegReg"; }

   virtual Kind getKind() { return IsFPSTiST0RegReg; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);

   bool _forcePop;
   };


class X86FPArithmeticRegRegInstruction : public TR::X86FPRegRegInstruction
   {
   public:

   X86FPArithmeticRegRegInstruction(TR_X86OpCodes    op,
                                        TR::Node          *node,
                                        TR::Register      *treg,
                                        TR::Register      *sreg,
                                        TR::CodeGenerator *cg);

   X86FPArithmeticRegRegInstruction(TR::Instruction   *precedingInstruction,
                                        TR_X86OpCodes    op,
                                        TR::Register      *treg,
                                        TR::Register      *sreg,
                                        TR::CodeGenerator *cg);

   virtual char *description() { return "X86FPArithmeticRegReg"; }

   virtual Kind getKind() { return IsFPArithmeticRegReg; }

   void applyDestinationBitToOpCode(uint8_t *opCode, TR::Machine * machine)
      {
      TR::RealRegister *reg = toRealRegister(getTargetRegister());
      if (reg->getRegisterNumber() != TR::RealRegister::st0)
         {
         *opCode |= 0x04;
         }
      }

   void applyDirectionBitToOpCode(uint8_t *opCode, TR::Machine * machine)
      {
      uint8_t reverse, destination;

      TR::RealRegister *reg = toRealRegister(getTargetRegister());
      destination = (reg->getRegisterNumber() != TR::RealRegister::st0) ? 1 : 0;
      reverse = (this->getOpCode().sourceOpTarget()) ? 1 : 0;

      if (destination ^ reverse)
         {
         *opCode |= 0x08;
         }
      }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);
   };


class X86FPCompareRegRegInstruction : public TR::X86FPRegRegInstruction
   {
   public:

   X86FPCompareRegRegInstruction(TR_X86OpCodes    op,
                                     TR::Node          *node,
                                     TR::Register      *treg,
                                     TR::Register      *sreg,
                                     TR::CodeGenerator *cg);

   X86FPCompareRegRegInstruction(TR::Instruction   *precedingInstruction,
                                     TR_X86OpCodes    op,
                                     TR::Register      *treg,
                                     TR::Register      *sreg,
                                     TR::CodeGenerator *cg);

   virtual char *description() { return "X86FPCompareRegReg"; }

   virtual Kind getKind() { return IsFPCompareRegReg; }

   bool swapOperands();

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);
   };


class X86FPCompareEvalInstruction : public TR::Instruction
   {
   TR::Register    *_accRegister;
   uint8_t         _actionType;

   public:

   X86FPCompareEvalInstruction(TR_X86OpCodes    op,
                                   TR::Node          *node,
                                   TR::Register      *accRegister,
                                   TR::CodeGenerator *cg);

   X86FPCompareEvalInstruction(TR_X86OpCodes                       op,
                                   TR::Node                             *node,
                                   TR::Register                         *accRegister,
                                   TR::RegisterDependencyConditions  *cond,
                                   TR::CodeGenerator                    *cg);

   virtual char *description() { return "X86FPCompareEval"; }

   virtual Kind getKind() { return IsFPCompareEval; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   };


class X86FPRemainderRegRegInstruction : public TR::X86FPST0ST1RegRegInstruction
   {
   TR::Register *_accRegister;

   public:

   X86FPRemainderRegRegInstruction(TR_X86OpCodes     op,
                                      TR::Node          *node,
                                      TR::Register      *treg,
                                      TR::Register      *sreg,
                                      TR::CodeGenerator *cg);

   X86FPRemainderRegRegInstruction(TR_X86OpCodes                        op,
                                      TR::Node                             *node,
                                      TR::Register                         *treg,
                                      TR::Register                         *sreg,
                                      TR::Register                         *accReg,
                                      TR::RegisterDependencyConditions  *cond,
                                      TR::CodeGenerator                    *cg);

   X86FPRemainderRegRegInstruction(TR::Instruction   *precedingInstruction,
                                      TR_X86OpCodes     op,
                                      TR::Register      *treg,
                                      TR::Register      *sreg,
                                      TR::CodeGenerator *cg);

   virtual char *description() { return "X86FPRemainderRegReg"; }

   virtual Kind getKind() { return IsFPRemainderRegReg; }
   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   };


class X86FPMemRegInstruction : public TR::X86MemRegInstruction
   {

   public:

   X86FPMemRegInstruction(TR_X86OpCodes          op,
                              TR::Node                *node,
                              TR::MemoryReference  *mr,
                              TR::Register            *sreg,
                              TR::CodeGenerator       *cg);

   X86FPMemRegInstruction(TR::Instruction         *precedingInstruction,
                              TR_X86OpCodes          op,
                              TR::MemoryReference  *mr,
                              TR::Register            *sreg,
                              TR::CodeGenerator       *cg);

   virtual char *description() { return "X86FPMemReg"; }

   virtual Kind getKind() { return IsFPMemReg; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return getMemoryReference()->getNumMRReferencedGPRegisters(); }
   virtual uint32_t getNumOperandReferencedFPRegisters() { return 1; }
#endif
   };


class X86FPRegMemInstruction : public TR::X86RegMemInstruction
   {
   public:

   X86FPRegMemInstruction(TR_X86OpCodes          op,
                              TR::Node                *node,
                              TR::Register            *treg,
                              TR::MemoryReference  *mr,
                              TR::CodeGenerator       *cg);

   X86FPRegMemInstruction(TR::Instruction         *precedingInstruction,
                              TR_X86OpCodes          op,
                              TR::Register            *treg,
                              TR::MemoryReference  *mr,
                              TR::CodeGenerator       *cg);

   virtual char *description() { return "X86FPRegMem"; }

   virtual Kind getKind() { return IsFPRegMem; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual uint8_t* generateOperand(uint8_t* cursor);
   virtual uint8_t  getBinaryLengthLowerBound();

#ifdef DEBUG
   virtual uint32_t getNumOperandReferencedGPRegisters() { return getMemoryReference()->getNumMRReferencedGPRegisters(); }
   virtual uint32_t getNumOperandReferencedFPRegisters() { return 1; }
#endif
   };


class X86VFPSaveInstruction : public TR::Instruction
   {
   TR_VFPState _savedState;

   public:

   X86VFPSaveInstruction(TR::Instruction *precedingInstruction, TR::CodeGenerator *cg) :
      TR::Instruction(AdjustFramePtr, precedingInstruction, cg) {}

   X86VFPSaveInstruction(TR::Node *node, TR::CodeGenerator *cg) :
      TR::Instruction(node, AdjustFramePtr, cg) {}

   virtual char *description() { return "X86VFPSave"; }

   virtual Kind getKind() { return IsVFPSave; }

   const TR_VFPState &getSavedState(){ return _savedState; }

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
      {
      _savedState = cg->vfpState();
      }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate){ return currentEstimate; }

   };


class X86VFPRestoreInstruction : public TR::Instruction
   {
   TR::X86VFPSaveInstruction *_saveInstruction;

   public:

   X86VFPRestoreInstruction(TR::Instruction *precedingInstruction, TR::X86VFPSaveInstruction  *saveInstruction, TR::CodeGenerator *cg) :
      _saveInstruction(saveInstruction),
      TR::Instruction(AdjustFramePtr, precedingInstruction, cg) {}

   X86VFPRestoreInstruction(TR::X86VFPSaveInstruction  *saveInstruction, TR::Node *node, TR::CodeGenerator *cg) :
      _saveInstruction(saveInstruction),
      TR::Instruction(node, AdjustFramePtr, cg) {}

   virtual char *description() { return "X86VFPRestore"; }

   virtual Kind getKind() { return IsVFPRestore; }

   TR::X86VFPSaveInstruction  *getSaveInstruction() { return _saveInstruction; }

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
      {
      cg->setVFPState(_saveInstruction->getSavedState());
      }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate) { return currentEstimate; }

   };


class X86VFPDedicateInstruction : public TR::X86RegMemInstruction
   {
   // Ideally, we'd use multiple inheritance to get protected
   // TR::X86RegMemInstruction  and public TR::X86VFPSaveInstruction.  Since
   // the latter's functionality is quite simple, we just duplicate it here.

   TR_VFPState  _savedState;

   TR::MemoryReference *memref(TR::CodeGenerator * cg)
      {
      // This instruction translates to LEA rxx, [vfp+0], so this function
      // returns [vfp+0].
      //
      TR::Machine *machine = cg->machine();
      return generateX86MemoryReference(machine->getRealRegister(TR::RealRegister::vfp), 0, cg);
      }

   public:

   X86VFPDedicateInstruction(TR::Instruction *precedingInstruction, TR::RealRegister *framePointerReg, TR::CodeGenerator *cg):
      TR::X86RegMemInstruction(precedingInstruction, LEARegMem(), framePointerReg, memref(cg), cg){}

   X86VFPDedicateInstruction(TR::RealRegister *framePointerReg, TR::Node *node, TR::CodeGenerator *cg):
      TR::X86RegMemInstruction(LEARegMem(), node, framePointerReg, memref(cg), cg){}

   X86VFPDedicateInstruction(TR::Instruction *precedingInstruction, TR::RealRegister *framePointerReg, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg):
      TR::X86RegMemInstruction(precedingInstruction, LEARegMem(), framePointerReg, memref(cg), cond, cg){}

   X86VFPDedicateInstruction(TR::RealRegister *framePointerReg, TR::Node *node, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg):
      TR::X86RegMemInstruction(LEARegMem(), node, framePointerReg, memref(cg), cond, cg){}

   virtual char *description() { return "X86VFPDedicate"; }

   virtual Kind getKind() { return IsVFPDedicate; }

   const TR_VFPState &getSavedState() { return _savedState; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
      {
      _savedState = cg->vfpState();
      cg->initializeVFPState(toRealRegister(getTargetRegister())->getRegisterNumber(), 0);
      }

   };


class X86VFPReleaseInstruction : public TR::Instruction
   {
   // Ideally, TR::X86VFPDedicateInstruction  would inherit TR::X86VFPSaveInstruction,
   // and then this could inherit TR::X86VFPRestoreInstruction, but instead we
   // must duplicate the latter's functionality here.

   TR::X86VFPDedicateInstruction  *_dedicateInstruction;

   public:

   X86VFPReleaseInstruction(TR::Instruction *precedingInstruction, TR::X86VFPDedicateInstruction  *dedicateInstruction, TR::CodeGenerator *cg):
      _dedicateInstruction(dedicateInstruction),
      TR::Instruction(AdjustFramePtr, precedingInstruction, cg){}

   X86VFPReleaseInstruction(TR::X86VFPDedicateInstruction  *dedicateInstruction, TR::Node *node, TR::CodeGenerator *cg):
      _dedicateInstruction(dedicateInstruction),
      TR::Instruction(node, AdjustFramePtr, cg){}

   virtual char *description() { return "X86VFPRelease"; }

   virtual Kind getKind() { return IsVFPRelease; }

   TR::X86VFPDedicateInstruction *getDedicateInstruction() { return _dedicateInstruction; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
      {
      cg->setVFPState(_dedicateInstruction->getSavedState());
      }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate) { return currentEstimate; }

   };


/**
 * Calls are normally responsible for maintaining VFP state.  However, Polymorphic
 * Inline Caches (PICs) can have multiple call instructions in the same internal
 * control flow region.  As a consequence, if the VFP adjustment must be attached
 * to a call then it must go on exactly one of them, which is sometimes hard to
 * achieve in the code.  Furthermore, with the adjustment on one of the calls, the
 * VFP offset must be incorrect for at least part of the internal control flow
 * region, though this doesn't matter because we have tight control over what is
 * done in there anyway.
 *
 * Given that VFP tracking isn't perfect inside internal control flow anyway, it
 * seems simpler to offer a dedicated "VFP adjustment instruction" that we can
 * place at the appropriate point in the internal control flow region to make the
 * adjustment happen there.  Such an instruction is not safe in general, because
 * we can't control whether a spill comes before or after such an instruction, and
 * only one of the two possibilities can be correct; however, there are no spills
 * inside internal control flow anyway.
 *
 * Thus, it seems that an explicit VFP adjustment instruction is simpler than what
 * we have now, and no less correct.
 */
class X86VFPCallCleanupInstruction : public TR::Instruction
   {
   int32_t _stackPointerAdjustment;

   public:

   X86VFPCallCleanupInstruction(TR::Instruction *precedingInstruction, int32_t adjustment, TR::CodeGenerator *cg):
      _stackPointerAdjustment(adjustment),
      TR::Instruction(AdjustFramePtr, precedingInstruction, cg) {}

   X86VFPCallCleanupInstruction(int32_t adjustment, TR::Node *node, TR::CodeGenerator *cg):
      _stackPointerAdjustment(adjustment),
      TR::Instruction(node, AdjustFramePtr, cg) {}

   virtual char *description() { return "X86VFPCallCleanup"; }

   virtual Kind getKind() { return IsVFPCallCleanup; }

   int32_t getStackPointerAdjustment(){ return _stackPointerAdjustment; }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);

   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
      {
      if (state->_register == TR::RealRegister::esp)
         state->_displacement += _stackPointerAdjustment;
      }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate) { return currentEstimate; }

   };

}


//////////////////////////////////////////////////////////////////////////
// Pseudo-safe downcast functions
//////////////////////////////////////////////////////////////////////////

inline TR::X86ImmInstruction  * toIA32ImmInstruction(TR::Instruction *i)
   {
   TR_ASSERT(i->getX86ImmInstruction() != NULL,
          "trying to downcast to an IA32ImmInstruction");
   return (TR::X86ImmInstruction  *)i;
   }


//////////////////////////////////////////////////////////////////////////
// Generate Routines
//////////////////////////////////////////////////////////////////////////


TR::X86MemInstruction  * generateMemInstruction(TR::Instruction *, TR_X86OpCodes op, TR::MemoryReference  * mr, TR::CodeGenerator *cg);
TR::X86RegInstruction  * generateRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86RegInstruction  * generateRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::CodeGenerator *cg);
TR::X86RegInstruction  * generateRegInstruction(TR::Instruction *prev, TR_X86OpCodes op, TR::Register * reg1, TR::CodeGenerator *cg);
TR::X86RegInstruction  * generateRegInstruction(TR::Instruction *prev, TR_X86OpCodes op, TR::Register * reg1, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg);

TR::X86MemInstruction  * generateMemInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, TR::CodeGenerator *cg);

TR::X86MemInstruction  * generateMemInstruction(TR_X86OpCodes                       op,
                                               TR::Node                             *node,
                                               TR::MemoryReference               *mr,
                                               TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg);

TR::X86MemTableInstruction * generateMemTableInstruction(TR_X86OpCodes op, TR::Node *node, TR::MemoryReference *mr, ncount_t numEntries, TR::CodeGenerator *cg);
TR::X86MemTableInstruction * generateMemTableInstruction(TR_X86OpCodes op, TR::Node *node, TR::MemoryReference *mr, ncount_t numEntries, TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg);

TR::X86RegImmInstruction  * generateRegImmInstruction(TR::Instruction *, TR_X86OpCodes op, TR::Register * reg1, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind=TR_NoRelocation);
TR::X86RegMemInstruction  * generateRegMemInstruction(TR::Instruction *, TR_X86OpCodes op, TR::Register * reg1, TR::MemoryReference  * mr, TR::CodeGenerator *cg);
TR::X86RegRegInstruction  * generateRegRegInstruction(TR::Instruction *, TR_X86OpCodes op, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);

TR::Instruction  * generateInstruction(TR_X86OpCodes, TR::Node *, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::Instruction  * generateInstruction(TR_X86OpCodes op, TR::Node * node, TR::CodeGenerator *cg);
TR::Instruction  * generateInstruction(TR::Instruction *prev, TR_X86OpCodes op, TR::CodeGenerator *cg);

TR::X86ImmInstruction  * generateImmInstruction(TR_X86OpCodes op, TR::Node * node, int32_t imm, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86ImmInstruction  * generateImmInstruction(TR_X86OpCodes op, TR::Node * node, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind=TR_NoRelocation);
TR::X86ImmInstruction  * generateImmInstruction(TR::Instruction *prev, TR_X86OpCodes op, int32_t imm, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86ImmInstruction  * generateImmInstruction(TR::Instruction *prev, TR_X86OpCodes op, int32_t imm, TR::CodeGenerator *cg);

TR::X86RegInstruction  * generateRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86RegInstruction  * generateRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::CodeGenerator *cg);

TR::X86MemImmSymInstruction  * generateMemImmSymInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, int32_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg);

TR::X86PaddingInstruction  * generatePaddingInstruction(uint8_t length, TR::Node * node, TR::CodeGenerator *cg);
TR::X86PaddingInstruction  * generatePaddingInstruction(TR::Instruction *precedingInstruction, uint8_t length, TR::CodeGenerator *cg);

TR::X86PaddingSnippetInstruction * generatePaddingSnippetInstruction(uint8_t length, TR::Node * node, TR::CodeGenerator *cg);

TR::X86AlignmentInstruction  * generateAlignmentInstruction(TR::Node * node, uint8_t boundary, TR::CodeGenerator *cg);
TR::X86AlignmentInstruction  * generateAlignmentInstruction(TR::Node * node, uint8_t boundary, uint8_t margin, TR::CodeGenerator *cg);
TR::X86AlignmentInstruction  * generateAlignmentInstruction(TR::Instruction *precedingInstruction, uint8_t boundary, TR::CodeGenerator *cg);
TR::X86AlignmentInstruction  * generateAlignmentInstruction(TR::Instruction *precedingInstruction, uint8_t boundary, uint8_t margin, TR::CodeGenerator *cg);

TR::X86LabelInstruction  * generateLabelInstruction(TR_X86OpCodes op, TR::Node *, TR::LabelSymbol *sym, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86LabelInstruction  * generateLabelInstruction(TR::Instruction *prev, TR_X86OpCodes op, TR::LabelSymbol *sym, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86LabelInstruction  * generateLabelInstruction(TR_X86OpCodes op, TR::Node *node, TR::LabelSymbol *sym, TR::CodeGenerator *cg);
TR::X86LabelInstruction  * generateLabelInstruction(TR::Instruction *i, TR_X86OpCodes op, TR::LabelSymbol *sym, TR::CodeGenerator *cg);
TR::X86LabelInstruction  * generateLabelInstruction(TR_X86OpCodes op, TR::Node *, TR::LabelSymbol *sym, TR::Node * glRegDep, List<TR::Register> *popRegs, bool evaluateGlRegDeps, TR::CodeGenerator *cg);
inline TR::X86LabelInstruction  * generateLabelInstruction(TR_X86OpCodes op, TR::Node *node, TR::LabelSymbol *sym, TR::Node * glRegDep, List<TR::Register> *popRegs, TR::CodeGenerator *cg)
   { return generateLabelInstruction(op, node, sym, glRegDep, popRegs, true, cg); }
inline TR::X86LabelInstruction  * generateLabelInstruction(TR_X86OpCodes op, TR::Node *node, TR::LabelSymbol *sym, TR::Node * glRegDep, TR::CodeGenerator *cg)
   { return generateLabelInstruction(op, node, sym, glRegDep, 0, true, cg); }
TR::X86LabelInstruction  * generateLongLabelInstruction(TR_X86OpCodes op, TR::Node *, TR::LabelSymbol *sym, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86LabelInstruction  * generateLongLabelInstruction(TR_X86OpCodes op, TR::Node *, TR::LabelSymbol *sym, TR::CodeGenerator *cg);

TR::X86LabelInstruction  * generateJumpInstruction(TR_X86OpCodes op, TR::Node * jumpNode, TR::CodeGenerator *cg, bool needsVMThreadRegister = false, bool evaluateGlRegDeps = true);

TR::X86LabelInstruction  * generateConditionalJumpInstruction(TR_X86OpCodes op, TR::Node * ifNode, TR::CodeGenerator *cg, bool needsVMThreadRegister = false);

TR::X86FenceInstruction  * generateFenceInstruction(TR_X86OpCodes op, TR::Node *, TR::Node *, TR::CodeGenerator *cg);

#ifdef J9_PROJECT_SPECIFIC
TR::X86VirtualGuardNOPInstruction  * generateVirtualGuardNOPInstruction(TR::Node *, TR_VirtualGuardSite *site, TR::RegisterDependencyConditions  *deps, TR::CodeGenerator *cg);
TR::X86VirtualGuardNOPInstruction  * generateVirtualGuardNOPInstruction(TR::Node *, TR_VirtualGuardSite *site, TR::RegisterDependencyConditions  *deps, TR::LabelSymbol *symbol, TR::CodeGenerator *cg);
TR::X86VirtualGuardNOPInstruction  * generateVirtualGuardNOPInstruction(TR::Instruction *i, TR::Node *, TR_VirtualGuardSite *site, TR::RegisterDependencyConditions  *deps, TR::LabelSymbol *symbol, TR::CodeGenerator *cg);
#endif

TR::X86RegImmInstruction  * generateRegImmInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, int32_t imm, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);
TR::X86RegImmInstruction  * generateRegImmInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind=TR_NoRelocation);

TR::X86RegImmSymInstruction  *generateRegImmSymInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, int32_t imm, TR::SymbolReference *, TR::CodeGenerator *cg);

TR::X86RegMemInstruction  * generateRegMemInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::MemoryReference  * mr, TR::RegisterDependencyConditions  *deps, TR::CodeGenerator *cg);
TR::X86RegMemInstruction  * generateRegMemInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::MemoryReference  * mr, TR::CodeGenerator *cg);
TR::X86RegRegInstruction  * generateRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::RegisterDependencyConditions  *deps, TR::CodeGenerator *cg);
TR::X86RegRegInstruction  * generateRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);
TR::X86MemImmInstruction  * generateMemImmInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind = TR_NoRelocation);
TR::X86MemImmInstruction  * generateMemImmInstruction(TR::Instruction *, TR_X86OpCodes op, TR::MemoryReference  * mr, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind = TR_NoRelocation);
TR::X86MemRegInstruction  * generateMemRegInstruction(TR::Instruction *precedingInstruction, TR_X86OpCodes op, TR::MemoryReference  *mr, TR::Register *sreg, TR::CodeGenerator *cg);
TR::X86MemRegInstruction  * generateMemRegInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, TR::Register * reg1, TR::RegisterDependencyConditions  *deps, TR::CodeGenerator *cg);
TR::X86MemRegInstruction  * generateMemRegInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, TR::Register * reg1, TR::CodeGenerator *cg);
TR::X86ImmSymInstruction  * generateImmSymInstruction(TR_X86OpCodes op, TR::Node *, int32_t imm, TR::SymbolReference *, TR::RegisterDependencyConditions  *, TR::CodeGenerator *cg);
TR::X86ImmSymInstruction  * generateImmSymInstruction(TR_X86OpCodes op, TR::Node *, int32_t imm, TR::SymbolReference *, TR::CodeGenerator *cg);
TR::X86ImmSymInstruction  * generateImmSymInstruction(TR::Instruction *prev, TR_X86OpCodes op, int32_t imm, TR::SymbolReference *, TR::RegisterDependencyConditions  *, TR::CodeGenerator *cg);
TR::X86ImmSymInstruction  * generateImmSymInstruction(TR::Instruction *prev, TR_X86OpCodes op, int32_t imm, TR::SymbolReference *, TR::CodeGenerator *cg);

TR::X86RegRegRegInstruction  * generateRegRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::Register * reg3, TR::RegisterDependencyConditions  *deps, TR::CodeGenerator *cg);
TR::X86RegRegRegInstruction  * generateRegRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::Register * reg3, TR::CodeGenerator *cg);
TR::X86RegRegMemInstruction  * generateRegRegMemInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::MemoryReference  * mr, TR::RegisterDependencyConditions  *deps, TR::CodeGenerator *cg);
TR::X86RegRegMemInstruction  * generateRegRegMemInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::MemoryReference  * mr, TR::CodeGenerator *cg);

TR::X86ImmSnippetInstruction  * generateImmSnippetInstruction(TR_X86OpCodes op, TR::Node *, int32_t imm, TR::UnresolvedDataSnippet *, TR::CodeGenerator *cg);

TR::X86RegMemImmInstruction  * generateRegMemImmInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::MemoryReference  * mr, int32_t imm, TR::CodeGenerator *cg);
TR::X86RegRegImmInstruction  * generateRegRegImmInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, int32_t imm, TR::CodeGenerator *cg);

TR::X86CallMemInstruction  * generateCallMemInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, TR::RegisterDependencyConditions  *, TR::CodeGenerator *cg);
TR::X86CallMemInstruction  * generateCallMemInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, TR::CodeGenerator *cg);
TR::X86CallMemInstruction  * generateCallMemInstruction(TR::Instruction *, TR_X86OpCodes op, TR::MemoryReference *mr, TR::RegisterDependencyConditions *, TR::CodeGenerator *cg);

TR::X86ImmSymInstruction  * generateHelperCallInstruction(TR::Node *, TR_RuntimeHelper, TR::RegisterDependencyConditions  *, TR::CodeGenerator *cg);
TR::X86ImmSymInstruction  * generateHelperCallInstruction(TR::Instruction *, TR_RuntimeHelper, TR::CodeGenerator *cg);

TR::AMD64RegImm64Instruction * generateRegImm64Instruction(TR_X86OpCodes op, TR::Node *node, TR::Register *treg, uint64_t imm, TR::CodeGenerator *cg, int32_t reloKind = TR_NoRelocation);

TR::AMD64RegImm64Instruction *
generateRegImm64Instruction(TR::Instruction   *precedingInstruction,
                            TR_X86OpCodes     op,
                            TR::Register      *treg,
                            uint64_t          imm,
                            TR::CodeGenerator *cg,
                            int32_t           reloKind=TR_NoRelocation);

TR::AMD64RegImm64Instruction *
generateRegImm64Instruction(TR_X86OpCodes                        op,
                            TR::Node                             *node,
                            TR::Register                         *treg,
                            uint64_t                             imm,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg,
                            int32_t                              reloKind=TR_NoRelocation);

TR::AMD64RegImm64Instruction *
generateRegImm64Instruction(TR::Instruction                      *precedingInstruction,
                            TR_X86OpCodes                        op,
                            TR::Register                         *treg,
                            uint64_t                             imm,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg,
                            int32_t                              reloKind = TR_NoRelocation);

TR::AMD64Imm64Instruction *
generateImm64Instruction(TR_X86OpCodes     op,
                         TR::Node          *node,
                         uint64_t          imm,
                         TR::CodeGenerator *cg);

TR::AMD64Imm64Instruction *
generateImm64Instruction(TR::Instruction   *precedingInstruction,
                         TR_X86OpCodes     op,
                         uint64_t          imm,
                         TR::CodeGenerator *cg);

TR::AMD64Imm64Instruction *
generateImm64Instruction(TR_X86OpCodes                        op,
                         TR::Node *                            node,
                         uint64_t                             imm,
                         TR::RegisterDependencyConditions  *cond,
                         TR::CodeGenerator                    *cg);

TR::AMD64Imm64Instruction *
generateImm64Instruction(TR::Instruction                      *precedingInstruction,
                         TR_X86OpCodes                        op,
                         uint64_t                             imm,
                         TR::RegisterDependencyConditions  *cond,
                         TR::CodeGenerator                    *cg);

TR::AMD64RegImm64SymInstruction *
generateRegImm64SymInstruction(TR_X86OpCodes       op,
                               TR::Node            *node,
                               TR::Register        *reg,
                               uint64_t            imm,
                               TR::SymbolReference *sr,
                               TR::CodeGenerator   *cg);

TR::AMD64RegImm64SymInstruction *
generateRegImm64SymInstruction(TR::Instruction     *precedingInstruction,
                               TR_X86OpCodes       op,
                               TR::Register        *reg,
                               uint64_t            imm,
                               TR::SymbolReference *sr,
                               TR::CodeGenerator   *cg);

TR::AMD64Imm64SymInstruction *
generateImm64SymInstruction(TR_X86OpCodes       op,
                            TR::Node            *node,
                            uint64_t            imm,
                            TR::SymbolReference *sr,
                            TR::CodeGenerator   *cg);

TR::AMD64Imm64SymInstruction *
generateImm64SymInstruction(TR::Instruction     *precedingInstruction,
                           TR_X86OpCodes       op,
                           uint64_t            imm,
                           TR::SymbolReference *sr,
                           TR::CodeGenerator   *cg);

TR::AMD64Imm64SymInstruction *
generateImm64SymInstruction(TR_X86OpCodes                        op,
                            TR::Node                             *node,
                            uint64_t                             imm,
                            TR::SymbolReference                  *sr,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);

TR::AMD64Imm64SymInstruction *
generateImm64SymInstruction(TR::Instruction                      *precedingInstruction,
                            TR_X86OpCodes                        op,
                            uint64_t                             imm,
                            TR::SymbolReference                  *sr,
                            TR::RegisterDependencyConditions  *cond,
                            TR::CodeGenerator                    *cg);



namespace TR { typedef TR::Instruction X86FPReturnInstruction; }
namespace TR { typedef TR::X86ImmInstruction X86FPReturnImmInstruction; }


TR::X86FPRegInstruction  * generateFPRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::CodeGenerator *cg);

TR::X86FPST0ST1RegRegInstruction  * generateFPST0ST1RegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);
TR::X86FPST0STiRegRegInstruction  * generateFPST0STiRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);
TR::X86FPSTiST0RegRegInstruction  * generateFPSTiST0RegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg, bool forcePop = false);

TR::X86FPArithmeticRegRegInstruction  * generateFPArithmeticRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);

TR::X86FPCompareRegRegInstruction  * generateFPCompareRegRegInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);

TR::X86FPCompareEvalInstruction  * generateFPCompareEvalInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * accRegister, TR::CodeGenerator *cg);

TR::X86FPCompareEvalInstruction  * generateFPCompareEvalInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * accRegister, TR::RegisterDependencyConditions  * cond, TR::CodeGenerator *cg);

TR::X86FPRemainderRegRegInstruction  * generateFPRemainderRegRegInstruction( TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::CodeGenerator *cg);
TR::X86FPRemainderRegRegInstruction  * generateFPRemainderRegRegInstruction( TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::Register * reg2, TR::Register *accReg, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg);

TR::X86FPMemRegInstruction  * generateFPMemRegInstruction(TR_X86OpCodes op, TR::Node *, TR::MemoryReference  * mr, TR::Register * reg1, TR::CodeGenerator *cg);
TR::X86FPRegMemInstruction  * generateFPRegMemInstruction(TR_X86OpCodes op, TR::Node *, TR::Register * reg1, TR::MemoryReference  * mr, TR::CodeGenerator *cg);

TR::X86FPReturnInstruction  * generateFPReturnInstruction(TR_X86OpCodes op, TR::Node *, TR::RegisterDependencyConditions  *, TR::CodeGenerator *cg);

TR::X86FPReturnImmInstruction  * generateFPReturnImmInstruction(TR_X86OpCodes cp, TR::Node *, int32_t imm, TR::RegisterDependencyConditions  *, TR::CodeGenerator *cg);

TR::X86PatchableCodeAlignmentInstruction  *
generatePatchableCodeAlignmentInstruction(const TR_AtomicRegion *atomicRegions, TR::Instruction *patchableCode, TR::CodeGenerator *cg);
TR::X86PatchableCodeAlignmentInstruction *
generatePatchableCodeAlignmentInstructionWithProtectiveNop(const TR_AtomicRegion *atomicRegions, TR::Instruction *patchableCode, int32_t protectiveNopSize, TR::CodeGenerator *cg);
TR::X86BoundaryAvoidanceInstruction  *
generateBoundaryAvoidanceInstruction(const TR_AtomicRegion *atomicRegions, uint8_t boundarySpacing, uint8_t maxPadding, TR::Instruction *targetCode, TR::CodeGenerator *cg);
TR::X86PatchableCodeAlignmentInstruction *
generatePatchableCodeAlignmentInstruction(TR::Instruction *prev, const TR_AtomicRegion *atomicRegions, TR::CodeGenerator *cg);
TR::X86PatchableCodeAlignmentInstruction  *
generatePatchableCodeAlignmentInstruction(const TR_AtomicRegion *atomicRegions, TR::Instruction *patchableCode, TR::CodeGenerator *cg);

TR::X86VFPSaveInstruction  *generateVFPSaveInstruction(TR::Instruction *precedingInstruction, TR::CodeGenerator *cg);
TR::X86VFPSaveInstruction  *generateVFPSaveInstruction(TR::Node *node, TR::CodeGenerator *cg);

TR::X86VFPRestoreInstruction  *generateVFPRestoreInstruction(TR::Instruction *precedingInstruction, TR::X86VFPSaveInstruction  *saveInstruction, TR::CodeGenerator *cg);
TR::X86VFPRestoreInstruction  *generateVFPRestoreInstruction(TR::X86VFPSaveInstruction  *saveInstruction, TR::Node *node, TR::CodeGenerator *cg);

TR::X86VFPDedicateInstruction  *generateVFPDedicateInstruction(TR::Instruction *precedingInstruction, TR::RealRegister *framePointerReg, TR::CodeGenerator *cg);
TR::X86VFPDedicateInstruction  *generateVFPDedicateInstruction(TR::RealRegister *framePointerReg, TR::Node *node, TR::CodeGenerator *cg);
TR::X86VFPDedicateInstruction  *generateVFPDedicateInstruction(TR::Instruction *precedingInstruction, TR::RealRegister *framePointerReg, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg);
TR::X86VFPDedicateInstruction  *generateVFPDedicateInstruction(TR::RealRegister *framePointerReg, TR::Node *node, TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *cg);

TR::X86VFPReleaseInstruction  *generateVFPReleaseInstruction(TR::Instruction *precedingInstruction, TR::X86VFPDedicateInstruction  *dedicateInstruction, TR::CodeGenerator *cg);
TR::X86VFPReleaseInstruction  *generateVFPReleaseInstruction(TR::X86VFPDedicateInstruction  *dedicateInstruction, TR::Node *node, TR::CodeGenerator *cg);

TR::X86VFPCallCleanupInstruction *generateVFPCallCleanupInstruction(TR::Instruction *precedingInstruction, int32_t adjustment, TR::CodeGenerator *cg);
TR::X86VFPCallCleanupInstruction *generateVFPCallCleanupInstruction(int32_t adjustment, TR::Node *node, TR::CodeGenerator *cg);


//////////////////////////////////////////////////////////////////////////
// Miscellaneous helpers
//////////////////////////////////////////////////////////////////////////

TR::RealRegister *assign8BitGPRegister(TR::Instruction *instr, TR::Register *virtReg, TR::CodeGenerator *cg);

TR::RealRegister *assignGPRegister(TR::Instruction   *instr,
                                  TR::Register      *virtReg,
                                  TR_RegisterSizes  requestedRegSize,
                                  TR::CodeGenerator *cg);

TR_X86OpCodes getBranchOrSetOpCodeForFPComparison(TR::ILOpCodes cmpOp, bool useFCOMIInstructions);
#endif
