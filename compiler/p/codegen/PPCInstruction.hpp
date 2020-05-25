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

#ifndef PPCINSTRUCTION_INCL
#define PPCINSTRUCTION_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "runtime/Runtime.hpp"

#include "codegen/GCStackMap.hpp"

class TR_VirtualGuardSite;
namespace TR { class SymbolReference; }

#define PPC_INSTRUCTION_LENGTH 4

namespace TR
{

class PPCAlignmentNopInstruction : public TR::Instruction
   {
   uint32_t _alignment;

   void setAlignment(uint32_t alignment)
      {
      TR_ASSERT_FATAL((alignment % PPC_INSTRUCTION_LENGTH) == 0, "Alignment must be a multiple of the nop instruction length");
      _alignment = alignment != 0 ? alignment : PPC_INSTRUCTION_LENGTH;
      }

public:
   PPCAlignmentNopInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t alignment, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, codeGen)
      {
      setAlignment(alignment);
      }

   PPCAlignmentNopInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t alignment, TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, precedingInstruction, codeGen)
      {
      setAlignment(alignment);
      }

   virtual Kind getKind() { return IsAlignmentNop; }

   uint32_t getAlignment() { return _alignment; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t getBinaryLengthLowerBound();
   };

class PPCImmInstruction : public TR::Instruction
   {
   uint32_t _sourceImmediate;
   TR_ExternalRelocationTargetKind _reloKind;
   TR::SymbolReference *_symbolReference;

public:

   //Six variants of this constructor:
   // 1. Has a specified relocation type, or it does not.
   // 2. Has a symbol reference to go with relocation type, or it does not.
   // 3. Has a specified preceding instruction, or it does not.

   //Without relocation types here.
   PPCImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm, TR::CodeGenerator *codeGen, uint32_t bf = 0)
      : TR::Instruction(op, n, codeGen), _sourceImmediate(imm), _reloKind(TR_NoRelocation), _symbolReference(NULL)
      {
      }

   PPCImmInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, uint32_t       imm,
                        TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen, uint32_t bf = 0)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _sourceImmediate(imm), _reloKind(TR_NoRelocation), _symbolReference(NULL)
      {
      }

   //With relocation types here.
   PPCImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm, TR_ExternalRelocationTargetKind relocationKind,
                        TR::CodeGenerator *codeGen, uint32_t bf = 0)
      : TR::Instruction(op, n, codeGen), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(NULL)
      {
      setNeedsAOTRelocation(true);
      }

   PPCImmInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, uint32_t imm, TR_ExternalRelocationTargetKind relocationKind,
                        TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen, uint32_t bf = 0)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(NULL)
      {
      setNeedsAOTRelocation(true);
      }

   //With relocation types and associated symbol references here.
   PPCImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm, TR_ExternalRelocationTargetKind relocationKind,
                        TR::SymbolReference *sr, TR::CodeGenerator *codeGen, uint32_t bf = 0)
      : TR::Instruction(op, n, codeGen), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(sr)
      {
      setNeedsAOTRelocation(true);
      }

   PPCImmInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, uint32_t imm, TR_ExternalRelocationTargetKind relocationKind,
                        TR::SymbolReference *sr, TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen, uint32_t bf = 0)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(sr)
      {
      setNeedsAOTRelocation(true);
      }

   virtual Kind getKind() { return IsImm; }

   uint32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t setSourceImmediate(uint32_t si) {return (_sourceImmediate = si);}

   void addMetaDataForCodeAddress(uint8_t *cursor);

   void setReloKind(TR_ExternalRelocationTargetKind reloKind) { _reloKind = reloKind; }
   TR_ExternalRelocationTargetKind getReloKind()              { return _reloKind; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   void insertImmediateField(uint32_t *instruction)
      {
      if (getOpCode().useAlternateFormatx())
         {
         // populate 4-bit U field at bit 16
         TR_ASSERT(getOpCodeValue() == TR::InstOpCode::mtfsfi, "Wrong usage of U field");
         *instruction |= (_sourceImmediate << 12) & 0xffff;
         }
      else if (getOpCode().isTMAbort())
         {
         // populate 5-bit SI field at bit 16
         *instruction |= (_sourceImmediate << 11) & 0xffff;
         }
      else
         {
         *instruction |= _sourceImmediate & 0xffff;
         }
      }

   virtual void updateImmediateField(uint32_t imm)
	 {
	 _sourceImmediate = imm;
	 insertImmediateField((uint32_t*)getBinaryEncoding());
	 }

// The following safe virtual downcast method is used under debug only
// for assertion checking
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   virtual PPCImmInstruction *getPPCImmInstruction();
#endif
   };

class PPCImm2Instruction : public PPCImmInstruction
   {
   uint32_t _sourceImmediate2;

   public:

   PPCImm2Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm, uint32_t imm2, TR::CodeGenerator *codeGen)
      : PPCImmInstruction(op, n, imm, codeGen), _sourceImmediate2(imm2)
      {
      }

   PPCImm2Instruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, uint32_t       imm, uint32_t imm2,
                        TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCImmInstruction(op, n, imm, precedingInstruction, codeGen), _sourceImmediate2(imm2)
      {
      }

   virtual Kind getKind() { return IsImm2; }

   uint32_t getSourceImmediate2()               {return _sourceImmediate2;}
   uint32_t setSourceImmediate2(uint32_t si)    {return (_sourceImmediate2 = si);}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);
   };

class PPCSrc1Instruction : public PPCImmInstruction
   {
   TR::Register *_source1Register;

   public:

   PPCSrc1Instruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::Register   *sreg,
                         uint32_t       imm, TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCImmInstruction(op, n, imm, precedingInstruction, codeGen),
        _source1Register(sreg)
      {
      useRegister(sreg);
      }

   PPCSrc1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register *sreg, uint32_t imm, TR::CodeGenerator *codeGen)
      : PPCImmInstruction(op, n, imm, codeGen), _source1Register(sreg)
      {
      useRegister(sreg);
      }

   virtual Kind getKind() { return IsSrc1; }

   TR::Register *getSource1Register()                {return _source1Register;}
   TR::Register *setSource1Register(TR::Register *sr) {return (_source1Register = sr);}

   virtual TR::Register *getSourceRegister(uint32_t i) {if (i==0) return _source1Register; return NULL;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getSource1Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getSource1Register());
      }

   };

class PPCDepInstruction : public TR::Instruction
   {
   TR::RegisterDependencyConditions *_conditions;

   public:

   PPCDepInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
      TR::RegisterDependencyConditions *cond, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, codeGen), _conditions(cond)
      {
      if( op != TR::InstOpCode::assocreg )
         cond->bookKeepingRegisterUses(this, codeGen);
      }

   PPCDepInstruction(TR::InstOpCode::Mnemonic                       op,
                        TR::Node                            *n,
                        TR::RegisterDependencyConditions *cond,
                        TR::Instruction                     *precedingInstruction, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _conditions(cond)
      {
      if( op != TR::InstOpCode::assocreg )
         cond->bookKeepingRegisterUses(this, codeGen);
      }

   virtual Kind getKind() { return IsDep; }

   virtual TR::RegisterDependencyConditions *getDependencyConditions()
      {
      return _conditions;
      }

   TR::RegisterDependencyConditions *setDependencyConditions(TR::RegisterDependencyConditions *cond)
      {
      return (_conditions = cond);
      }

   virtual TR::Register *getTargetRegister(uint32_t i)
      {
      if( getOpCodeValue() == TR::InstOpCode::assocreg ) return(NULL);
      return _conditions->getTargetRegister(i, cg());
      }

   virtual TR::Register *getSourceRegister(uint32_t i)
      {
      if( getOpCodeValue() == TR::InstOpCode::assocreg ) return(NULL);
      return _conditions->getSourceRegister(i);
      }

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);
   };

class PPCLabelInstruction : public TR::Instruction
   {
   TR::LabelSymbol *_symbol;

   public:

   PPCLabelInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::LabelSymbol *sym, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, codeGen), _symbol(sym)
      {
      if (sym!=NULL && op==TR::InstOpCode::label)
         sym->setInstruction(this);
      else if (sym)
         sym->setDirectlyTargeted();
      }

   PPCLabelInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::LabelSymbol *sym,
                          TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _symbol(sym)
      {
      if (sym!=NULL && op==TR::InstOpCode::label)
         sym->setInstruction(this);
      else if (sym)
         sym->setDirectlyTargeted();
      }

   virtual Kind getKind() { return IsLabel; }
   virtual bool isPatchBarrier() { return getOpCodeValue() == TR::InstOpCode::label && _symbol && _symbol->isTargeted() != TR_no; }

   TR::LabelSymbol *getLabelSymbol() {return _symbol;}
   TR::LabelSymbol *setLabelSymbol(TR::LabelSymbol *sym)
      {
      _symbol = sym;
      if (sym && getOpCodeValue() != TR::InstOpCode::label)
         sym->setDirectlyTargeted();
      return sym;
      }

   virtual TR::Snippet *getSnippetForGC() {return getLabelSymbol()->getSnippet();}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);
   };

class PPCDepLabelInstruction : public PPCLabelInstruction
   {
   TR::RegisterDependencyConditions *_conditions;

   public:

   PPCDepLabelInstruction(TR::InstOpCode::Mnemonic                       op,
                             TR::Node                            *n,
                             TR::LabelSymbol                      *sym,
                             TR::RegisterDependencyConditions *cond, TR::CodeGenerator *codeGen)
      : PPCLabelInstruction(op, n, sym, codeGen), _conditions(cond)
      {
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   PPCDepLabelInstruction(TR::InstOpCode::Mnemonic                        op,
                             TR::Node                             *n,
                             TR::LabelSymbol                       *sym,
                             TR::RegisterDependencyConditions  *cond,
                             TR::Instruction             *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCLabelInstruction(op, n, sym, precedingInstruction, codeGen),
        _conditions(cond)
      {
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   virtual Kind getKind() { return IsDepLabel; }

   virtual TR::RegisterDependencyConditions *getDependencyConditions()
      {
      return _conditions;
      }

   TR::RegisterDependencyConditions *setDependencyConditions(TR::RegisterDependencyConditions *cond)
      {
      return (_conditions = cond);
      }

   virtual TR::Register *getTargetRegister(uint32_t i)
      {
      return _conditions->getTargetRegister(i, cg());
      }

   virtual TR::Register *getSourceRegister(uint32_t i)
      {
      return _conditions->getSourceRegister(i);
      }

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      getDependencyConditions()->registersGoLive(state);
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      getDependencyConditions()->registersGoDead(state);
      }

   };

class PPCConditionalBranchInstruction : public PPCLabelInstruction
   {
   TR::Register    *_conditionRegister;
   int32_t         _estimatedBinaryLocation;
   bool            _likeliness;
   bool            _haveHint;
   bool            _farRelocation;
   bool            _exceptBranch;

   public:

   PPCConditionalBranchInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::LabelSymbol *sym,
                                      TR::Register    *cr, TR::CodeGenerator *codeGen, bool likeliness)
      : PPCLabelInstruction(op, n, sym, codeGen), _conditionRegister(cr),
        _estimatedBinaryLocation(0),  _farRelocation(false),_exceptBranch(false),
        _haveHint(true),  _likeliness(likeliness)
      {
      useRegister(cr);
      }

   PPCConditionalBranchInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::LabelSymbol *sym,
                                      TR::Register    *cr, TR::CodeGenerator *codeGen)
      : PPCLabelInstruction(op, n, sym, codeGen), _conditionRegister(cr),
        _estimatedBinaryLocation(0),  _farRelocation(false),_exceptBranch(false),
        _haveHint(false), _likeliness(false)
      {
      useRegister(cr);
      }

   PPCConditionalBranchInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::LabelSymbol *sym,
                                      TR::Register    *cr,
                                      TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen, bool likeliness)
      : PPCLabelInstruction(op, n, sym, precedingInstruction, codeGen),
        _conditionRegister(cr), _estimatedBinaryLocation(0),_exceptBranch(false),
        _farRelocation(false), _haveHint(true), _likeliness(likeliness)
      {
      useRegister(cr);
      }

   PPCConditionalBranchInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::LabelSymbol *sym,
                                      TR::Register    *cr,
                                      TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCLabelInstruction(op, n, sym, precedingInstruction, codeGen),
        _conditionRegister(cr), _estimatedBinaryLocation(0),_exceptBranch(false),
        _farRelocation(false), _haveHint(false), _likeliness(false)
      {
      useRegister(cr);
      }

   virtual Kind getKind() { return IsConditionalBranch; }

   void expandIntoFarBranch();

   TR::Register    *getConditionRegister()              {return _conditionRegister;}
   TR::Register    *setConditionRegister(TR::Register *cr) {return (_conditionRegister = cr);}

   virtual TR::Register *getSourceRegister(uint32_t i) {if (i==0) return _conditionRegister; return NULL;}

   int32_t getEstimatedBinaryLocation() {return _estimatedBinaryLocation;}
   int32_t setEstimatedBinaryLocation(int32_t l) {return (_estimatedBinaryLocation = l);}

   bool            getFarRelocation() {return _farRelocation;}

   bool       reverseLikeliness() {return (_likeliness = !_likeliness);}
   bool       getLikeliness()    {return _likeliness;}
   bool       haveHint()         {return _haveHint;}
   bool isExceptBranchOp() { return _exceptBranch; }
   bool setExceptBranchOp() { return _exceptBranch = true; }

   virtual PPCConditionalBranchInstruction *getPPCConditionalBranchInstruction();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void fillBinaryEncodingFields(uint32_t *cursor);
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

class PPCDepConditionalBranchInstruction : public PPCConditionalBranchInstruction
   {
   TR::RegisterDependencyConditions *_conditions;

   public:

      PPCDepConditionalBranchInstruction(
                             TR::InstOpCode::Mnemonic                        op,
                             TR::Node                             *n,
                             TR::LabelSymbol                       *sym,
                             TR::Register                         *cr,
                             TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *codeGen, bool likeliness)
      : PPCConditionalBranchInstruction(op, n, sym, cr, codeGen, likeliness), _conditions(cond)
      {
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   PPCDepConditionalBranchInstruction(
                             TR::InstOpCode::Mnemonic                        op,
                             TR::Node                             *n,
                             TR::LabelSymbol                       *sym,
                             TR::Register                         *cr,
                             TR::RegisterDependencyConditions  *cond, TR::CodeGenerator *codeGen)
      : PPCConditionalBranchInstruction(op, n, sym, cr, codeGen), _conditions(cond)
      {
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   PPCDepConditionalBranchInstruction(
                             TR::InstOpCode::Mnemonic                       op,
                             TR::Node                            *n,
                             TR::LabelSymbol                      *sym,
                             TR::Register                        *cr,
                             TR::RegisterDependencyConditions *cond,
                             TR::Instruction                     *precedingInstruction, TR::CodeGenerator *codeGen, bool likeliness)
      : PPCConditionalBranchInstruction(op, n, sym, cr, precedingInstruction, codeGen, likeliness),
        _conditions(cond)
      {
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   PPCDepConditionalBranchInstruction(
                             TR::InstOpCode::Mnemonic                       op,
                             TR::Node                            *n,
                             TR::LabelSymbol                      *sym,
                             TR::Register                        *cr,
                             TR::RegisterDependencyConditions *cond,
                             TR::Instruction                     *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCConditionalBranchInstruction(op, n, sym, cr, precedingInstruction, codeGen),
        _conditions(cond)
      {
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   virtual Kind getKind() { return IsDepConditionalBranch; }

   virtual TR::RegisterDependencyConditions *getDependencyConditions()
      {
      return _conditions;
      }

   TR::RegisterDependencyConditions *setDependencyConditions(TR::RegisterDependencyConditions *cond)
      {
      return (_conditions = cond);
      }

   virtual TR::Register *getTargetRegister(uint32_t i)
      {
      return _conditions->getTargetRegister(i, cg());
      }

   virtual TR::Register *getSourceRegister(uint32_t i)
      {
      if (i==0) return(getConditionRegister());
      return _conditions->getSourceRegister(i-1);
      }

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);
   };

class PPCAdminInstruction : public TR::Instruction
   {
   TR::Node *_fenceNode;

   public:

   PPCAdminInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Node * fenceNode, TR::CodeGenerator *codeGen) :
      TR::Instruction(op, n, codeGen), _fenceNode(fenceNode) {}

   PPCAdminInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::Node *fenceNode,
                          TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen) :
      TR::Instruction(op, n, precedingInstruction, codeGen), _fenceNode(fenceNode) {}

   virtual Kind getKind() { return IsAdmin; }

   virtual TR::Instruction *expandInstruction();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   bool isDebugFence()      {return (_fenceNode!=NULL && _fenceNode->getOpCodeValue() == TR::dbgFence); }

   TR::Node * getFenceNode() { return _fenceNode; }

   virtual void fillBinaryEncodingFields(uint32_t *cursor);
   };

class PPCDepImmSymInstruction : public PPCDepInstruction
   {

   uintptr_t           _addrImmediate;
   TR::SymbolReference *_symbolReference;
   TR::Snippet         *_snippet;

   public:

   PPCDepImmSymInstruction(TR::InstOpCode::Mnemonic                       op,
                              TR::Node                            *n,
                              uintptr_t                           imm,
                              TR::RegisterDependencyConditions *cond,
                              TR::SymbolReference                 *sr,
                              TR::Snippet                         *s, TR::CodeGenerator *codeGen)
      : PPCDepInstruction(op, n, cond, codeGen), _addrImmediate(imm), _symbolReference(sr),
        _snippet(s) {}

   PPCDepImmSymInstruction(
                              TR::InstOpCode::Mnemonic                       op,
                              TR::Node                            *n,
                              uintptr_t                           imm,
                              TR::RegisterDependencyConditions *cond,
                              TR::SymbolReference                 *sr,
                              TR::Snippet                         *s,
                              TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCDepInstruction(op, n, cond, precedingInstruction, codeGen),
        _addrImmediate(imm), _symbolReference(sr), _snippet(s) {}

   virtual Kind getKind() { return IsDepImmSym; }

   uintptr_t getAddrImmediate() {return _addrImmediate;}

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }

   TR::Snippet *getCallSnippet() { return _snippet;}
   TR::Snippet *setCallSnippet(TR::Snippet *s) {return (_snippet = s);}

   virtual TR::Snippet *getSnippetForGC() {return _snippet;}

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

class PPCTrg1Instruction : public TR::Instruction
   {
   TR::Register *_target1Register;

   public:

   PPCTrg1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *reg, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, codeGen), _target1Register(reg)
      {
      useRegister(reg);
      }

   PPCTrg1Instruction(TR::InstOpCode::Mnemonic  op, TR::Node * n, TR::Register *reg,
                        TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _target1Register(reg)
      {
      useRegister(reg);
      }

   virtual Kind getKind() { return IsTrg1; }

   TR::Register *getTrg1Register()                 {return _target1Register;}
   TR::Register *getTargetRegister()               {return _target1Register;}
   TR::Register *setTargetRegister(TR::Register *r) {return (_target1Register = r);}

   TR::Register *getPrimaryTargetRegister()        {return _target1Register;}

   virtual TR::Register *getTargetRegister(uint32_t i)       {if (i==0) return _target1Register; return NULL;}

   void insertTargetRegister(uint32_t *instruction)
      {
      TR::RealRegister *target = toRealRegister(_target1Register);
      if (isVSX())
         target->setRegisterFieldXT(instruction);
      else
         target->setRegisterFieldRT(instruction);
      }

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   void         assignRegisters(TR_RegisterKinds kindToBeAssigned, bool excludeGPR0);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      }

   };

class PPCTrg1ImmInstruction : public PPCTrg1Instruction
   {
   uint32_t _sourceImmediate;

   public:

   PPCTrg1ImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                            uint32_t     imm, TR::CodeGenerator *codeGen)
           : PPCTrg1Instruction(op, n, treg, codeGen), _sourceImmediate(imm) {};

   PPCTrg1ImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                            uint32_t       imm,
                            TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
           : PPCTrg1Instruction(op, n, treg, precedingInstruction, codeGen), _sourceImmediate(imm) {};

   virtual Kind getKind() { return IsTrg1Imm; }

   uint32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t setSourceImmediate(uint32_t si) {return (_sourceImmediate = si);}

   virtual void updateImmediateField(uint32_t imm)
         {
         _sourceImmediate = imm;
         insertImmediateField((uint32_t*)getBinaryEncoding());
         }

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   void insertImmediateField(uint32_t *instruction)
      {
      if (!isVMX())
         {
         if (getOpCodeValue() == TR::InstOpCode::addpcis)
            {
            // populate d0, d1 and d2 fields
            *instruction |= ((_sourceImmediate >> 6) & 0x3ff) << 6;
            *instruction |= ((_sourceImmediate >> 1) & 0x1f) << 16;
            *instruction |= _sourceImmediate & 0x1;
            }
         else if (getOpCodeValue() == TR::InstOpCode::setb ||
                  getOpCodeValue() == TR::InstOpCode::mcrfs)
            {
            // populate 3-bit BFA field
            *instruction |= (_sourceImmediate & 0x7) << 18;
            }
         else if (getOpCodeValue() == TR::InstOpCode::darn)
            {
            // populate 2-bit L field
            *instruction |= (_sourceImmediate & 0x3) << 16;
            }
         else
            {
            *instruction |= _sourceImmediate & 0xffff;
            }
         }
      else
         *instruction |= (_sourceImmediate & 0x1f) << 16;
      }

   void addMetaDataForCodeAddress(uint8_t *cursor);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      }

   };

class PPCSrc2Instruction : public TR::Instruction
   {
   TR::Register *_source1Register;
   TR::Register *_source2Register;

   public:

   PPCSrc2Instruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register   *s1reg,
                         TR::Register   *s2reg, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, codeGen), _source1Register(s1reg), _source2Register(s2reg)
      {
      useRegister(s1reg);
      useRegister(s2reg);
      }

   PPCSrc2Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register   *s1reg,
                         TR::Register    *s2reg,
                         TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : TR::Instruction(op, n, precedingInstruction, codeGen), _source1Register(s1reg), _source2Register(s2reg)
      {
      useRegister(s1reg);
      useRegister(s2reg);
      }

   virtual Kind getKind() { return IsSrc2; }

   TR::Register *getSource1Register()                {return _source1Register;}
   TR::Register *setSource1Register(TR::Register *sr) {return (_source1Register = sr);}
   TR::Register *getSource2Register()                {return _source2Register;}
   TR::Register *setSource2Register(TR::Register *sr) {return (_source2Register = sr);}
   virtual TR::Register *getSourceRegister(uint32_t i) {if      (i==0) return _source1Register;
                                                       else if (i==1) return _source2Register; return NULL;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getSource1Register());
      state->addVirtualRegister(getSource2Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getSource1Register());
      state->removeVirtualRegister(getSource2Register());
      }
   };

class PPCTrg1Src1Instruction : public PPCTrg1Instruction
   {
   TR::Register *_source1Register;

   public:

   PPCTrg1Src1Instruction(TR::InstOpCode::Mnemonic op,  TR::Node * n, TR::Register   *treg,
                             TR::Register   *sreg, TR::CodeGenerator *codeGen);

   PPCTrg1Src1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,  TR::Register   *treg,
                             TR::Register    *sreg,
                             TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen);

   virtual Kind getKind() { return IsTrg1Src1; }

   TR::Register *getSource1Register()                {return _source1Register;}
   TR::Register *setSource1Register(TR::Register *sr) {return (_source1Register = sr);}

   virtual TR::Register *getSourceRegister(uint32_t i) {if (i==0) return _source1Register; return NULL;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      state->addVirtualRegister(getSource1Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      state->removeVirtualRegister(getSource1Register());
      }
   };

class PPCTrg1Src1ImmInstruction : public PPCTrg1Src1Instruction
   {

   uintptr_t _source1Immediate;

   public:

   PPCTrg1Src1ImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register    *treg,
                                TR::Register    *sreg, uintptr_t imm, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1Instruction(op, n, treg, sreg, codeGen),
             _source1Immediate(imm) {};

   PPCTrg1Src1ImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register    *treg,
                                TR::Register    *sreg, uintptr_t imm,
                                TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1Instruction(op, n, treg, sreg, precedingInstruction, codeGen),
             _source1Immediate(imm) {};

   PPCTrg1Src1ImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register    *treg,
                                TR::Register *sreg, TR::Register *cr0reg, uintptr_t imm, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1Instruction(op, n, treg, sreg, codeGen),
             _source1Immediate(imm)
      {
      TR::RegisterDependencyConditions *cond = new (codeGen->trHeapMemory()) TR::RegisterDependencyConditions( 0, 1, codeGen->trMemory() );
      cond->addPostCondition(cr0reg, TR::RealRegister::cr0, DefinesDependentRegister );
      setDependencyConditions( cond );
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   PPCTrg1Src1ImmInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register    *treg,
                                TR::Register *sreg, TR::Register *cr0reg, uintptr_t imm,
                                TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1Instruction(op, n, treg, sreg, precedingInstruction, codeGen),
             _source1Immediate(imm)
      {
      TR::RegisterDependencyConditions *cond = new (codeGen->trHeapMemory()) TR::RegisterDependencyConditions( 0, 1, codeGen->trMemory() );
      cond->addPostCondition(cr0reg, TR::RealRegister::cr0, DefinesDependentRegister );
      setDependencyConditions( cond );
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   virtual Kind getKind() { return IsTrg1Src1Imm; }

   uint32_t getSourceImmediate()            {return (uint32_t)_source1Immediate;}
   uint32_t setSourceImmediate(uint32_t si) {return (_source1Immediate = si);}

   uintptr_t getSourceImmPtr()             {return _source1Immediate;}

   virtual void updateImmediateField(uint32_t imm)
      {
      _source1Immediate = imm;
      insertImmediateField((uint32_t*)getBinaryEncoding());
      }

   void insertImmediateField(uint32_t *instruction)
      {
      if (!isVMX() && !isVSX())
         *instruction |= _source1Immediate & 0xffff;
      else
	     *instruction |= (_source1Immediate & 0x3f) << 16;
      }

   void insertShiftAmount(uint32_t *instruction)
      {
      if (isDoubleWord() || getOpCodeValue() == TR::InstOpCode::extswsli)
         // The low order 5 bits of a long shift amount are in the shift field.  The high order bit
         // is in bit 30.
         {
         *instruction |= ((_source1Immediate & 0x1f) << TR::RealRegister::pos_SH) |
                         ((_source1Immediate & 0x20) >> 4);
         }
      else
         {
         *instruction |= ((_source1Immediate & 0x1f) << TR::RealRegister::pos_SH);
         }
      }

   void addMetaDataForCodeAddress(uint8_t *cursor);

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      state->addVirtualRegister(getSource1Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      state->removeVirtualRegister(getSource1Register());
      }
   };

class PPCTrg1Src1Imm2Instruction : public PPCTrg1Src1ImmInstruction
   {

   int64_t _mask;

   public:

   PPCTrg1Src1Imm2Instruction(TR::InstOpCode::Mnemonic  op,
                                 TR::Node        *n,
                                 TR::Register    *treg,
                                 TR::Register    *sreg,
                                 uint32_t       imm,
                                 uint64_t       m,
                                 TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1ImmInstruction(op, n, treg, sreg, imm, precedingInstruction, codeGen),
             _mask(m) {}

   PPCTrg1Src1Imm2Instruction(TR::InstOpCode::Mnemonic  op,
                                 TR::Node        *n,
                                 TR::Register    *treg,
                                 TR::Register    *sreg,
                                 uint32_t       imm,
                                 uint64_t       m, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1ImmInstruction(op, n, treg, sreg, imm, codeGen),
             _mask(m) {}

   PPCTrg1Src1Imm2Instruction(TR::InstOpCode::Mnemonic  op,
                                 TR::Node        *n,
                                 TR::Register    *treg,
                                 TR::Register    *sreg,
                                 TR::Register    *cr0reg,
                                 uint32_t       imm,
                                 uint64_t       m, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1ImmInstruction(op, n, treg, sreg, cr0reg, imm, codeGen),
             _mask(m) {}

   PPCTrg1Src1Imm2Instruction(TR::InstOpCode::Mnemonic  op,
                                 TR::Node        *n,
                                 TR::Register    *treg,
                                 TR::Register    *sreg,
                                 TR::Register    *cr0reg,
                                 uint32_t       imm,
                                 uint64_t       m,
                                 TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
           : PPCTrg1Src1ImmInstruction(op, n, treg, sreg, cr0reg, imm, precedingInstruction, codeGen),
             _mask(m) {}

   virtual Kind getKind() { return IsTrg1Src1Imm2; }

   int32_t getMask()                {return (_mask&0xffffffff);}
   int32_t setMask(uint32_t mi)     {return (int32_t) (_mask = (uint64_t)mi);}
   int64_t getLongMask()            {return _mask;}
   int64_t setLongMask(uint64_t mi) {return _mask = mi;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      state->addVirtualRegister(getSource1Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      state->removeVirtualRegister(getSource1Register());
      }
   };

class PPCTrg1Src2Instruction : public PPCTrg1Src1Instruction
   {
   TR::Register *_source2Register;

   public:

   PPCTrg1Src2Instruction( TR::InstOpCode::Mnemonic op,
                              TR::Node       *n,
                              TR::Register   *treg,
                              TR::Register   *s1reg,
                              TR::Register   *s2reg, TR::CodeGenerator *codeGen)
      : PPCTrg1Src1Instruction(op, n, treg, s1reg, codeGen), _source2Register(s2reg)
      {
      useRegister(s2reg);
      }

   PPCTrg1Src2Instruction( TR::InstOpCode::Mnemonic op,
                              TR::Node       *n,
                              TR::Register   *treg,
                              TR::Register   *s1reg,
                              TR::Register   *s2reg,
                             TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCTrg1Src1Instruction(op, n, treg, s1reg, precedingInstruction, codeGen),
                             _source2Register(s2reg)
      {
      useRegister(s2reg);
      }

   PPCTrg1Src2Instruction( TR::InstOpCode::Mnemonic op,
                              TR::Node       *n,
                              TR::Register   *treg,
                              TR::Register   *s1reg,
                              TR::Register   *s2reg,
                              TR::Register   *cr0reg,
                             TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCTrg1Src1Instruction(op, n, treg, s1reg, precedingInstruction, codeGen),
                             _source2Register(s2reg)
      {
      useRegister(s2reg);
      TR::RegisterDependencyConditions *cond = new (codeGen->trHeapMemory()) TR::RegisterDependencyConditions( 0, 1, codeGen->trMemory() );
      cond->addPostCondition(cr0reg, TR::RealRegister::cr0, DefinesDependentRegister );
      setDependencyConditions( cond );
      cond->bookKeepingRegisterUses(this, codeGen);
      }

   virtual Kind getKind() { return IsTrg1Src2; }

   TR::Register *getSource2Register()                {return _source2Register;}
   TR::Register *setSource2Register(TR::Register *sr) {return (_source2Register = sr);}

   virtual TR::Register *getSourceRegister(uint32_t i) {if      (i==0) return getSource1Register();
                                               else if (i==1) return _source2Register; return NULL;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      state->addVirtualRegister(getSource1Register());
      state->addVirtualRegister(getSource2Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      state->removeVirtualRegister(getSource1Register());
      state->removeVirtualRegister(getSource2Register());
      }
   };

class PPCTrg1Src2ImmInstruction : public PPCTrg1Src2Instruction
   {
   int64_t _mask;

   public:

   PPCTrg1Src2ImmInstruction( TR::InstOpCode::Mnemonic op,
                                 TR::Node       *n,
                                 TR::Register   *treg,
                                 TR::Register   *s1reg,
                                 TR::Register   *s2reg,
                                 int64_t       m, TR::CodeGenerator *codeGen)
      : PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, codeGen),
      _mask(m) {}

   PPCTrg1Src2ImmInstruction( TR::InstOpCode::Mnemonic op,
                                 TR::Node       *n,
                                 TR::Register   *treg,
                                 TR::Register   *s1reg,
                                 TR::Register   *s2reg,
                                 int64_t       m,
                                 TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, precedingInstruction, codeGen),
      _mask(m) {}

   virtual Kind getKind() { return IsTrg1Src2Imm; }

   int32_t getMask()                {return (_mask&0xffffffff);}
   int32_t setMask(uint32_t mi)     {return (uint32_t) (_mask = (uint64_t)mi);}
   int64_t getLongMask()            {return _mask;}
   int64_t setLongMask(uint64_t mi) {return _mask = mi;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);
   };

class PPCTrg1Src3Instruction : public PPCTrg1Src2Instruction
   {
   TR::Register *_source3Register;

   public:

   PPCTrg1Src3Instruction( TR::InstOpCode::Mnemonic op,
                              TR::Node       *n,
                              TR::Register   *treg,
                              TR::Register   *s1reg,
                              TR::Register   *s2reg,
                              TR::Register   *s3reg, TR::CodeGenerator *codeGen)
      : PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, codeGen), _source3Register(s3reg)
      {
      useRegister(s3reg);
      }

   PPCTrg1Src3Instruction( TR::InstOpCode::Mnemonic op,
                              TR::Node       *n,
                              TR::Register   *treg,
                              TR::Register   *s1reg,
                              TR::Register   *s2reg,
                              TR::Register   *s3reg,
                              TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen)
      : PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, precedingInstruction, codeGen),
                             _source3Register(s3reg)
      {
      useRegister(s3reg);
      }

   virtual Kind getKind() { return IsTrg1Src3; }

   TR::Register *getSource3Register()                {return _source3Register;}
   TR::Register *setSource3Register(TR::Register *sr) {return (_source3Register = sr);}

   virtual TR::Register *getSourceRegister(uint32_t i) {if      (i==0) return getSource1Register();
                                               else if (i==1) return getSource2Register();
                                               else if (i==2) return _source3Register; return NULL;}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      state->addVirtualRegister(getSource1Register());
      state->addVirtualRegister(getSource2Register());
      state->addVirtualRegister(getSource3Register());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      state->removeVirtualRegister(getSource1Register());
      state->removeVirtualRegister(getSource2Register());
      state->removeVirtualRegister(getSource3Register());
      }
   };

class PPCMemInstruction : public TR::Instruction
   {
   TR::MemoryReference *_memoryReference;

   public:

   PPCMemInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node *n,
                        TR::MemoryReference *mf, TR::CodeGenerator *cg);

   PPCMemInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node *n,
                        TR::MemoryReference *mf,
                        TR::Instruction *precedingInstruction, TR::CodeGenerator *cg);

   virtual Kind getKind() { return IsMem; }

   TR::MemoryReference *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference *setMemoryReference(TR::MemoryReference *p)
      {
      _memoryReference = p;
      return p;
      }

   virtual TR::Register *getMemoryBase()    {return getMemoryReference()->getBaseRegister();}
   virtual TR::Register *getMemoryIndex()   {return getMemoryReference()->getIndexRegister();}
   virtual int32_t      getOffset()        {return getMemoryReference()->getOffset(*TR::comp());}

   virtual void fillBinaryEncodingFields(uint32_t *cursor);
   virtual TR::Instruction *expandInstruction();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual TR::Register *getSourceRegister(uint32_t i)       {
	   switch(i){
		   case 0: return (_memoryReference->getBaseRegister() == NULL) ? _memoryReference->getIndexRegister() : _memoryReference ->getBaseRegister();
		   case 1: return _memoryReference->getIndexRegister();
	 	   default: return NULL;
	   }
   }

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getMemoryReference()->getBaseRegister());
      state->addVirtualRegister(getMemoryReference()->getIndexRegister());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getMemoryReference()->getBaseRegister());
      state->removeVirtualRegister(getMemoryReference()->getIndexRegister());
      }
   };

class PPCMemSrc1Instruction : public PPCMemInstruction
   {
   TR::Register *_sourceRegister;

   public:

   PPCMemSrc1Instruction(TR::InstOpCode::Mnemonic op,
                            TR::Node *n,
                            TR::MemoryReference *mf,
                            TR::Register *sreg, TR::CodeGenerator *codeGen);

   PPCMemSrc1Instruction(TR::InstOpCode::Mnemonic op,
                            TR::Node *n,
                            TR::MemoryReference *mf,
                            TR::Register *sreg,
                            TR::Instruction *precedingInstruction, TR::CodeGenerator *codeGen);

   virtual Kind getKind() { return IsMemSrc1; }

   TR::Register *getSourceRegister()                {return _sourceRegister;}
   TR::Register *setSourceRegister(TR::Register *sr) {return (_sourceRegister = sr);}

   virtual TR::Register *getSourceRegister(uint32_t i)
      {
      if (getOpCodeValue() == TR::InstOpCode::stmw)
         {
         return getSourceRegisterForStmw(i);
         }
      if (i==0) return _sourceRegister;
      if (i==1)
         {
         if (getMemoryReference()->getBaseRegister() != NULL)
            return getMemoryReference()->getBaseRegister();
         return getMemoryReference()->getIndexRegister();
         }
      if (i==2)
         {
         if (getMemoryReference()->getBaseRegister() != NULL)
            return getMemoryReference()->getIndexRegister();
         return NULL;
         }
      return NULL;
      }

   TR::Register *getSourceRegisterForStmw(uint32_t i);

   virtual TR::Register *getTargetRegister(uint32_t i)
     {
     if ((i==0) && isUpdate()) return getMemoryReference()->getBaseRegister();
     return NULL;
     }

   virtual TR::Snippet *getSnippetForGC() {return getMemoryReference()->getUnresolvedSnippet();}

   virtual TR::RegisterDependencyConditions *getDependencyConditions()
      {
      return getMemoryReference()->getConditions();
      }

   virtual TR::Register *getMemoryDataRegister();

   virtual void fillBinaryEncodingFields(uint32_t *cursor);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   void insertSourceRegister(uint32_t *instruction)
      {
      TR::RealRegister *source = toRealRegister(_sourceRegister);

      if (isVSX())
         source->setRegisterFieldXS(instruction);
      else
         source->setRegisterFieldRS(instruction);
      }

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getSourceRegister());
      state->addVirtualRegister(getMemoryReference()->getBaseRegister());
      state->addVirtualRegister(getMemoryReference()->getIndexRegister());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getSourceRegister());
      state->removeVirtualRegister(getMemoryReference()->getBaseRegister());
      state->removeVirtualRegister(getMemoryReference()->getIndexRegister());
      }
   };

class PPCTrg1MemInstruction : public PPCTrg1Instruction
   {
   TR::MemoryReference *_memoryReference;

   int32_t _hint;

   public:

   PPCTrg1MemInstruction(TR::InstOpCode::Mnemonic          op,
                            TR::Node                *n,
                            TR::Register            *treg,
                            TR::MemoryReference *mf, TR::CodeGenerator *codeGen, int32_t hint = PPCOpProp_NoHint);

   PPCTrg1MemInstruction(TR::InstOpCode::Mnemonic          op,
                            TR::Node                *n,
                            TR::Register            *treg,
                            TR::MemoryReference *mf,
                            TR::Instruction         *precedingInstruction, TR::CodeGenerator *codeGen, int32_t hint = PPCOpProp_NoHint);

   virtual Kind getKind() { return IsTrg1Mem; }

   TR::MemoryReference *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference *setMemoryReference(TR::MemoryReference *p)
      {
      _memoryReference = p;
      return p;
      }

   virtual TR::Register *getMemoryBase()    {return getMemoryReference()->getBaseRegister();}
   virtual TR::Register *getMemoryIndex()   {return getMemoryReference()->getIndexRegister();}
   virtual int32_t      getOffset()        {return getMemoryReference()->getOffset(*TR::comp());}

   bool encodeMutexHint();
   bool haveHint() {return getHint() != PPCOpProp_NoHint;};
   int32_t getHint() {return _hint;};

   virtual TR::Register *getSourceRegister(uint32_t i)
      {
      if (i==0)
         {
         if (getMemoryReference()->getBaseRegister() != NULL)
            return getMemoryReference()->getBaseRegister();
         return getMemoryReference()->getIndexRegister();
         }
      if (i==1)
         {
         if (getMemoryReference()->getBaseRegister() != NULL)
            return getMemoryReference()->getIndexRegister();
         return NULL;
         }
      return NULL;
      }

   virtual TR::Register *getTargetRegister()
      {
      return PPCTrg1Instruction::getTargetRegister();
      }

   virtual TR::Register *getTargetRegister(uint32_t i)
      {
      if (getOpCodeValue() == TR::InstOpCode::lmw)
         {
         return getTargetRegisterForLmw(i);
         }

      if (i==0)
         {
         return PPCTrg1Instruction::getTargetRegister();
         }
      if ((i==1) && isUpdate())
         {
         return getMemoryReference()->getBaseRegister();
         }
      else
         {
         if (isUpdate())
            i-=2;
         else
            i-=1;

         if (getMemoryReference()->getConditions())
            {
            return( getMemoryReference()->getConditions()->getTargetRegister(i, cg()) );
            }
         }
      return NULL;
      }

   TR::Register *getTargetRegisterForLmw(uint32_t i);

   virtual TR::Snippet *getSnippetForGC() {return getMemoryReference()->getUnresolvedSnippet();}

   virtual TR::RegisterDependencyConditions *getDependencyConditions()
      {
      return getMemoryReference()->getConditions();
      }

   virtual TR::Register *getMemoryDataRegister();

   virtual void fillBinaryEncodingFields(uint32_t *cursor);
   virtual TR::Instruction *expandInstruction();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->addVirtualRegister(getTargetRegister());
      state->addVirtualRegister(getMemoryReference()->getBaseRegister());
      state->addVirtualRegister(getMemoryReference()->getIndexRegister());
      }
   virtual void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      state->removeVirtualRegister(getTargetRegister());
      state->removeVirtualRegister(getMemoryReference()->getBaseRegister());
      state->removeVirtualRegister(getMemoryReference()->getIndexRegister());
      }
   };

// If we are using a register allocator that cannot handle registers being alive across basic block
// boundaries then we use PPCControlFlowInstruction as a placeholder.  It contains a summary of
// register usage information and enough other information to generate the correct instruction
// sequence.
class PPCControlFlowInstruction : public TR::Instruction
   {
   TR::RegisterDependencyConditions *_conditions;
   bool _isSourceImmediate[8];
   union {
      TR::Register *_sourceRegister;
      uint32_t _sourceImmediate;
   } _sources[8];
   TR::Register *_targetRegisters[5];
   TR::LabelSymbol *_label;
   int32_t _numSources;
   int32_t _numTargets;
   TR::InstOpCode _opCode2;
   TR::InstOpCode _opCode3;
   TR::InstOpCode _cmpOp;
   bool _useRegPairForResult;
   bool _useRegPairForCond;

   public:

   PPCControlFlowInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n,
      TR::CodeGenerator *codeGen,
      TR::RegisterDependencyConditions *deps=NULL,
      bool useRegPairForResult=false,
      bool useRegPairForCond=false)
      : TR::Instruction(op, n, codeGen), _numSources(0), _numTargets(0), _label(NULL),
        _opCode2(TR::InstOpCode::bad), _conditions(deps), _useRegPairForResult(useRegPairForResult),
        _useRegPairForCond(useRegPairForCond)
      {
      if (deps!=NULL) deps->bookKeepingRegisterUses(this, codeGen);
      }
   PPCControlFlowInstruction(TR::InstOpCode::Mnemonic  op, TR::Node * n,
      TR::Instruction *preceedingInstruction,
      TR::CodeGenerator *codeGen,
      TR::RegisterDependencyConditions *deps=NULL,
      bool useRegPairForResult=false,
      bool useRegPairForCond=false)
      : TR::Instruction(op, n, preceedingInstruction, codeGen),
        _numSources(0), _numTargets(0), _label(NULL), _opCode2(TR::InstOpCode::bad),
        _conditions(deps), _useRegPairForResult(useRegPairForResult),
         _useRegPairForCond(useRegPairForCond)
      {
      if (deps!=NULL) deps->bookKeepingRegisterUses(this, codeGen);
      }

   bool useRegPairForResult() { return _useRegPairForResult; }
   bool useRegPairForCond() { return _useRegPairForCond; }
   virtual Kind getKind() { return IsControlFlow; }

   int32_t getNumSources()                    {return _numSources;}
   bool isSourceImmediate(uint32_t i)         {return _isSourceImmediate[i];}
   virtual TR::Register *getSourceRegister(uint32_t i)  {if (i>=_numSources || _isSourceImmediate[i]) return NULL; return _sources[i]._sourceRegister;}
   TR::Register *setSourceRegister(uint32_t i, TR::Register *sr) { _isSourceImmediate[i] = false; return (_sources[i]._sourceRegister = sr); }
   TR::Register *addSourceRegister(TR::Register *sr)
      {
      int i = _numSources;
      useRegister(sr);
      _numSources++;
      _isSourceImmediate[i] = false;
      return (_sources[i]._sourceRegister = sr);
      }
   virtual uint32_t getSourceImmediate(uint32_t i)  {if (i>=_numSources || !_isSourceImmediate[i]) return 0; return _sources[i]._sourceImmediate;}
   void setSourceImmediate(uint32_t i, uint32_t val) { _isSourceImmediate[i] = true; _sources[i]._sourceImmediate = val; }
   void addSourceImmediate(uint32_t val)
      {
      int i = _numSources;
      _numSources++;
      _isSourceImmediate[i] = true;
      _sources[i]._sourceImmediate = val;
      }

   int32_t getNumTargets()                    {return _numTargets;}
   virtual TR::Register *getTargetRegister(uint32_t i)  {if (i>=_numTargets) return NULL; return _targetRegisters[i];}
   TR::Register *setTargetRegister(int32_t i, TR::Register *tr) { return (_targetRegisters[i] = tr); }
   TR::Register *addTargetRegister(TR::Register *tr)
      {
      int i = _numTargets;
      useRegister(tr);
      _numTargets++;
      return (_targetRegisters[i] = tr);
      }

   TR::LabelSymbol *getLabelSymbol()                     {return _label;}
   TR::LabelSymbol *setLabelSymbol(TR::LabelSymbol *sym)
      {
      _label = sym;
      if (sym)
         sym->setDirectlyTargeted();
      return sym;
      }

   TR::InstOpCode& getOpCode2()                      {return _opCode2;}
   TR::InstOpCode::Mnemonic getOpCode2Value()                 {return _opCode2.getOpCodeValue();}
   TR::InstOpCode::Mnemonic setOpCode2Value(TR::InstOpCode::Mnemonic op) {return (_opCode2.setOpCodeValue(op));}

   TR::InstOpCode& getOpCode3()                      {return _opCode3;}
   TR::InstOpCode::Mnemonic getOpCode3Value()                 {return _opCode3.getOpCodeValue();}
   TR::InstOpCode::Mnemonic setOpCode3Value(TR::InstOpCode::Mnemonic op) {return (_opCode3.setOpCodeValue(op));}

   TR::InstOpCode& getCmpOp()                      {return _cmpOp;}
   TR::InstOpCode::Mnemonic getCmpOpValue()                 {return _cmpOp.getOpCodeValue();}
   TR::InstOpCode::Mnemonic setCmpOpValue(TR::InstOpCode::Mnemonic op) {return (_cmpOp.setOpCodeValue(op));}

   virtual TR::RegisterDependencyConditions *getDependencyConditions() {return _conditions;}

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);

   virtual bool defsRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);
   };

#ifdef J9_PROJECT_SPECIFIC
class PPCVirtualGuardNOPInstruction : public PPCDepLabelInstruction
   {
   private:
   TR_VirtualGuardSite     *_site;

   public:
   PPCVirtualGuardNOPInstruction(TR::Node                        *node,
                                    TR_VirtualGuardSite     *site,
                                    TR::RegisterDependencyConditions *cond,
                                    TR::LabelSymbol                  *label,
                                    TR::CodeGenerator               *codeGen)
      : PPCDepLabelInstruction(TR::InstOpCode::vgdnop, node, label, cond, codeGen), _site(site) {}

   PPCVirtualGuardNOPInstruction(TR::Node                        *node,
                                    TR_VirtualGuardSite     *site,
                                    TR::RegisterDependencyConditions *cond,
                                    TR::LabelSymbol                  *label,
                                    TR::Instruction                 *precedingInstruction,
                                    TR::CodeGenerator               *codeGen)
      : PPCDepLabelInstruction(TR::InstOpCode::vgdnop, node, label, cond, precedingInstruction, codeGen), _site(site) {}

   virtual Kind getKind() { return IsVirtualGuardNOP; }

   void setSite(TR_VirtualGuardSite *site) { _site = site; }
   TR_VirtualGuardSite * getSite() { return _site; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual bool     isVirtualGuardNOPInstruction() {return true;}
   };
#endif

}

 /*************************************************************************
  * Pseudo-safe downcast functions
  *
  *************************************************************************/
inline TR::PPCImmInstruction * toPPCImmInstruction(TR::Instruction *i)
   {
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   TR_ASSERT(i->getPPCImmInstruction() != NULL, "trying to downcast to an PPCImmInstruction");
#endif
   return (TR::PPCImmInstruction *)i;
   }

#endif
