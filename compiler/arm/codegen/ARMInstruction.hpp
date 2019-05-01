/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef ARMINSTRUCTION_INCL
#define ARMINSTRUCTION_INCL

#include "codegen/Instruction.hpp"
#include "codegen/ARMOps.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/GCStackMap.hpp"
#include "codegen/ARMConditionCode.hpp"
#include "codegen/ARMOperand2.hpp"
#include "infra/Bit.hpp"

class TR_VirtualGuardSite;

namespace TR { class ARMDepImmInstruction; }
namespace TR { class ARMImmInstruction; }
namespace TR { class ARMConditionalBranchInstruction; }

#define ARM_INSTRUCTION_LENGTH 4

inline bool constantIsUnsignedImmed8(int32_t intValue)
	{
   if(abs(intValue) < 256)
      return true;
   else
      return false;
   }

inline bool constantIsImmed8r(int32_t intValue)
   {
   return intValue < 128;
   }

bool constantIsImmed8r(int32_t intValue, uint32_t *base, uint32_t *rotate);

inline bool constantIsImmed8r(int32_t intValue, uint32_t *base, uint32_t *rotate, bool *negated)
   {
   uint32_t bitPattern = (uint32_t) intValue;

   *negated = false;
   if(intValue < 0)
      {
      bitPattern ^= 0xFFFFFFFF;
      *negated = true;
      }
   return constantIsImmed8r(bitPattern, base, rotate);
   }

inline bool constantIsImmed10(int32_t intValue)
   {
   if(abs(intValue) < 1024)
      {
      return true;
      }
   return false;
   }

inline bool constantIsImmed12(int32_t intValue)
   {
   if(abs(intValue) < 4096)
      {
      return true;
      }
   return false;
   }

inline bool isSinglePrecision(TR::RealRegister::RegNum registerNumber)
   {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   return ((registerNumber >= TR::RealRegister::FirstFSR) && (registerNumber <= TR::RealRegister::LastFSR));
#else
   return false;
#endif
   }

namespace TR {

class ARMImmInstruction : public TR::Instruction
   {
   uint32_t _sourceImmediate;
   TR_ExternalRelocationTargetKind _reloKind;
   TR::SymbolReference *_symbolReference;

   public:

   ARMImmInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::Instruction(node, cg) {}

   ARMImmInstruction(TR_ARMOpCodes op, TR::Node *node, uint32_t imm, TR::CodeGenerator *cg)
      : TR::Instruction(node, cg), _sourceImmediate(imm), _reloKind(TR_NoRelocation), _symbolReference(NULL)
      {
      setOpCodeValue(op);
      }

   ARMImmInstruction(TR_ARMOpCodes                       op,
                     TR::Node                            *node,
                     TR::RegisterDependencyConditions *cond,
                     uint32_t                            imm,
                     TR::CodeGenerator                   *cg)
      : TR::Instruction(op, node, cond, cg), _sourceImmediate(imm), _reloKind(TR_NoRelocation), _symbolReference(NULL)
      {
      }

   ARMImmInstruction(TR::Instruction   *precedingInstruction,
                     TR_ARMOpCodes     op,
                     TR::Node          *node,
                     uint32_t          imm,
                     TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg), _sourceImmediate(imm), _reloKind(TR_NoRelocation), _symbolReference(NULL)
      {
      }

   ARMImmInstruction(TR::Instruction                     *precedingInstruction,
                     TR_ARMOpCodes                       op,
                     TR::Node                            *node,
                     TR::RegisterDependencyConditions *cond,
                     uint32_t                            imm,
                     TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cond, cg), _sourceImmediate(imm), _reloKind(TR_NoRelocation), _symbolReference(NULL)
      {
      }

   ARMImmInstruction(TR_ARMOpCodes     op,
                     TR::Node          *node,
                     uint32_t          imm,
                     TR_ExternalRelocationTargetKind relocationKind,
                     TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(NULL)
      {
      setNeedsAOTRelocation(true);
      }

   ARMImmInstruction(TR::Instruction   *precedingInstruction,
                     TR_ARMOpCodes     op,
                     TR::Node          *node,
                     uint32_t          imm,
                     TR_ExternalRelocationTargetKind relocationKind,
                     TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(NULL)
      {
      setNeedsAOTRelocation(true);
      }

   ARMImmInstruction(TR_ARMOpCodes     op,
                     TR::Node          *node,
                     uint32_t          imm,
                     TR_ExternalRelocationTargetKind relocationKind,
                     TR::SymbolReference *sr,
                     TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(sr)
      {
      setNeedsAOTRelocation(true);
      }

   ARMImmInstruction(TR::Instruction   *precedingInstruction,
                     TR_ARMOpCodes     op,
                     TR::Node          *node,
                     uint32_t          imm,
                     TR_ExternalRelocationTargetKind relocationKind,
                     TR::SymbolReference *sr,
                     TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg), _sourceImmediate(imm), _reloKind(relocationKind), _symbolReference(sr)
      {
      setNeedsAOTRelocation(true);
      }

   virtual Kind getKind() { return IsImm; }

   uint32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t setSourceImmediate(uint32_t si) {return (_sourceImmediate = si);}
   void setReloKind(TR_ExternalRelocationTargetKind reloKind) { _reloKind = reloKind; }
   TR_ExternalRelocationTargetKind getReloKind()              { return _reloKind; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }

   virtual uint8_t *generateBinaryEncoding();

   void insertImmediateField(uint32_t *instruction)
      {
      *instruction |= _sourceImmediate & 0xffff;
      }

// The following safe virtual downcast method is used under debug only
// for assertion checking
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   virtual TR::ARMImmInstruction *getARMImmInstruction();
#endif
   };


class ARMLabelInstruction : public TR::Instruction
   {
   TR::LabelSymbol *_symbol;
   TR::Register    *_t1reg;
   TR::Register    *_s1reg;

   public:

   ARMLabelInstruction(TR::Node *node, TR::CodeGenerator *cg)
      : TR::Instruction(node, cg), _symbol(NULL), _t1reg(NULL), _s1reg(NULL) {}

   ARMLabelInstruction(TR_ARMOpCodes op, TR::Node *node, TR::LabelSymbol *sym, TR::CodeGenerator *cg,
                       TR::Register *t1reg = NULL, TR::Register *s1reg = NULL)
      : TR::Instruction(node, cg), _symbol(sym), _t1reg(t1reg), _s1reg(s1reg)
      {
      setOpCodeValue(op);
      if (t1reg) t1reg->incTotalUseCount();
      if (s1reg) s1reg->incTotalUseCount();
      if (sym!=NULL && op==ARMOp_label)
         sym->setInstruction(this);
      }

   ARMLabelInstruction(TR_ARMOpCodes                       op,
                       TR::Node                            *node,
                       TR::RegisterDependencyConditions *cond,
                       TR::LabelSymbol                     *sym,
                       TR::CodeGenerator                   *cg)
      : TR::Instruction(op, node, cond, cg), _symbol(sym), _t1reg(NULL), _s1reg(NULL)
      {
      if (sym!=NULL && op==ARMOp_label)
         sym->setInstruction(this);
      }

   ARMLabelInstruction(TR::Instruction   *precedingInstruction,
                       TR_ARMOpCodes     op,
                       TR::Node          *node,
                       TR::LabelSymbol   *sym,
                       TR::CodeGenerator *cg,
                       TR::Register      *t1reg = NULL,
                       TR::Register      *s1reg = NULL)
      : TR::Instruction(precedingInstruction, op, node, cg), _symbol(sym), _t1reg(t1reg), _s1reg(s1reg)
      {
      if (t1reg) t1reg->incTotalUseCount();
      if (s1reg) s1reg->incTotalUseCount();
      if (sym!=NULL && op==ARMOp_label)
         sym->setInstruction(this);
      }

   ARMLabelInstruction(TR::Instruction                     *precedingInstruction,
                       TR_ARMOpCodes                       op,
                       TR::Node                            *node,
                       TR::RegisterDependencyConditions *cond,
                       TR::LabelSymbol                     *sym,
                       TR::CodeGenerator                   *cg)
      : TR::Instruction(precedingInstruction, op, node, cond, cg), _symbol(sym), _t1reg(NULL), _s1reg(NULL)
      {
      if (sym!=NULL && op==ARMOp_label)
         sym->setInstruction(this);
      }

   virtual Kind getKind() { return IsLabel; }

   void insertTargetRegister(uint32_t *instruction)
      {
      toRealRegister(_t1reg)->setRegisterFieldRD(instruction);
      }

   void insertSource1Register(uint32_t *instruction)
      {
      toRealRegister(_s1reg)->setRegisterFieldRN(instruction);
      }

   TR::Register *getTarget1Register()                   {return _t1reg;}
   TR::Register *setTarget1Register(TR::Register *treg1) {return (_t1reg = treg1);}

   TR::Register *getSource1Register()                   {return _s1reg;}
   TR::Register *setSource1Register(TR::Register *sreg1) {return (_s1reg = sreg1);}

   TR::LabelSymbol *getLabelSymbol() {return _symbol;}
   TR::LabelSymbol *setLabelSymbol(TR::LabelSymbol *sym) {return (_symbol=sym);}

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };


class ARMConditionalBranchInstruction : public TR::ARMLabelInstruction
   {
   int32_t             _estimatedBinaryLocation;
   bool                _farRelocation;

   public:

   ARMConditionalBranchInstruction(TR::Node *node, TR::CodeGenerator *cg)
      : TR::ARMLabelInstruction(node, cg),
        _estimatedBinaryLocation(0),
        _farRelocation(false)
      {
      setConditionCode(ARMConditionCodeIllegal);
      }

   ARMConditionalBranchInstruction(TR_ARMOpCodes        op,
                                   TR::Node             *node,
                                   TR::LabelSymbol      *sym,
                                   TR_ARMConditionCode  cc,
                                   TR::CodeGenerator    *cg)
      : TR::ARMLabelInstruction(op, node, sym, cg),
        _estimatedBinaryLocation(0),
        _farRelocation(false)
      {
      setConditionCode(cc);
      }

   ARMConditionalBranchInstruction(TR_ARMOpCodes                       op,
                                   TR::Node                            *node,
                                   TR::RegisterDependencyConditions *cond,
                                   TR::LabelSymbol                     *sym,
                                   TR_ARMConditionCode                 cc,
                                   TR::CodeGenerator                   *cg)
      : TR::ARMLabelInstruction(op, node, cond, sym, cg),
        _estimatedBinaryLocation(0),
        _farRelocation(false)
      {
      setConditionCode(cc);
      }

   ARMConditionalBranchInstruction(TR::Instruction      *precedingInstruction,
                                   TR_ARMOpCodes        op,
                                   TR::Node             *node,
                                   TR::LabelSymbol      *sym,
                                   TR_ARMConditionCode  cc,
                                   TR::CodeGenerator    *cg)

      : TR::ARMLabelInstruction(precedingInstruction, op, node, sym, cg),
        _estimatedBinaryLocation(0),
        _farRelocation(false)
      {
      setConditionCode(cc);
      }

   ARMConditionalBranchInstruction(TR::Instruction                     *precedingInstruction,
                                   TR_ARMOpCodes                       op,
                                   TR::Node                            *node,
                                   TR::RegisterDependencyConditions *cond,
                                   TR::LabelSymbol                     *sym,
                                   TR_ARMConditionCode                 cc,
                                   TR::CodeGenerator                   *cg)
      : TR::ARMLabelInstruction(precedingInstruction, op, node, cond, sym, cg),
        _estimatedBinaryLocation(0),
        _farRelocation(false)
      {
      setConditionCode(cc);
      }

   virtual Kind getKind() { return IsConditionalBranch; }

   virtual TR::Snippet *getSnippetForGC() {return getLabelSymbol()->getSnippet();}

   int32_t getEstimatedBinaryLocation() {return _estimatedBinaryLocation;}
   int32_t setEstimatedBinaryLocation(int32_t l) {return (_estimatedBinaryLocation = l);}

   bool getFarRelocation() {return _farRelocation;}
   bool setFarRelocation(bool b) {return (_farRelocation = b);}

   virtual TR::ARMConditionalBranchInstruction *getARMConditionalBranchInstruction();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

#ifdef J9_PROJECT_SPECIFIC
class ARMVirtualGuardNOPInstruction : public TR::ARMLabelInstruction
   {
   private:
   TR_VirtualGuardSite     *_site;

   public:

   ARMVirtualGuardNOPInstruction(TR::Node                         *node,
                                 TR_VirtualGuardSite              *site,
                                 TR::RegisterDependencyConditions *cond,
                                 TR::LabelSymbol                  *sym,
                                 TR::CodeGenerator                *cg)
      : TR::ARMLabelInstruction(ARMOp_vgdnop, node, cond, sym, cg),
        _site(site)
      {
      }

   ARMVirtualGuardNOPInstruction(TR::Node                         *node,
                                 TR_VirtualGuardSite              *site,
                                 TR::RegisterDependencyConditions *cond,
                                 TR::LabelSymbol                  *sym,
                                 TR::Instruction                *precedingInstruction,
                                 TR::CodeGenerator                *cg)
      : TR::ARMLabelInstruction(precedingInstruction, ARMOp_vgdnop, node, cond, sym, cg),
        _site(site)
      {
      }

   virtual Kind getKind() { return IsVirtualGuardNOP; }

   void setSite(TR_VirtualGuardSite *site) { _site = site; }
   TR_VirtualGuardSite * getSite() { return _site; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual bool     isVirtualGuardNOPInstruction() {return true;}
   };
#endif

class ARMAdminInstruction : public TR::Instruction
   {
   TR::Node *_fenceNode;

   public:

   ARMAdminInstruction(TR_ARMOpCodes     op,
                       TR::Node          *node,
                       TR::Node          *fenceNode,
                       TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _fenceNode(fenceNode) {}

   ARMAdminInstruction(TR_ARMOpCodes                       op,
                       TR::Node                            *node,
                       TR::Node                            *fenceNode,
                       TR::RegisterDependencyConditions *cond,
                       TR::CodeGenerator                   *cg)
      : TR::Instruction(op, node, cond, cg), _fenceNode(fenceNode) {}

   ARMAdminInstruction(TR::Instruction   *precedingInstruction,
                       TR_ARMOpCodes     op,
                       TR::Node          *node,
                       TR::Node          *fenceNode,
                       TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg), _fenceNode(fenceNode) {}

   ARMAdminInstruction(TR::Instruction                     *precedingInstruction,
                       TR_ARMOpCodes                       op,
                       TR::Node                            *node,
                       TR::Node                            *fenceNode,
                       TR::RegisterDependencyConditions *cond,
                       TR::CodeGenerator                   *cg)
      : TR::Instruction(precedingInstruction, op, node, cond, cg), _fenceNode(fenceNode) {}

   TR::Node *getFenceNode() { return _fenceNode; }

   virtual Kind getKind() { return IsAdmin; }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual uint8_t *generateBinaryEncoding();
   };


class ARMImmSymInstruction : public TR::ARMImmInstruction
   {
   TR::SymbolReference *_symbolReference;
   TR::Snippet         *_snippet;

   public:

   ARMImmSymInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMImmInstruction(node, cg), _symbolReference(NULL), _snippet(NULL) {}

   ARMImmSymInstruction(TR_ARMOpCodes                          op,
                        TR::Node                            *node,
                        uint32_t                            imm,
                        TR::RegisterDependencyConditions *cond,
                        TR::SymbolReference                 *sr,
                        TR::CodeGenerator                   *cg,
                        TR::Snippet                         *s=NULL,
                        TR_ARMConditionCode                cc=ARMConditionCodeAL);

   ARMImmSymInstruction(TR::Instruction                        *precedingInstruction,
                        TR_ARMOpCodes                       op,
                        TR::Node                            *node,
                        uint32_t                            imm,
                        TR::RegisterDependencyConditions *cond,
                        TR::SymbolReference                 *sr,
                        TR::CodeGenerator                   *cg,
                        TR::Snippet                         *s=NULL,
                        TR_ARMConditionCode                cc=ARMConditionCodeAL);

   virtual Kind getKind() { return IsImmSym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return (_symbolReference = sr);
      }

   TR::Snippet *getCallSnippet() { return _snippet;}
   TR::Snippet *setCallSnippet(TR::Snippet *s) {return (_snippet = s);}

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

class ARMTrg1Src2Instruction : public TR::Instruction
   {
   TR::Register    *_target1Register;
   TR::Register    *_source1Register;
   TR_ARMOperand2 *_source2Operand;

   public:

   ARMTrg1Src2Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::Instruction(node, cg) {}

   ARMTrg1Src2Instruction(TR_ARMOpCodes op, TR::Node *node, TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _target1Register(0), _source1Register(0), _source2Operand(0)
      {
      }

   ARMTrg1Src2Instruction(TR::Instruction   *precedingInstruction,
                          TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg), _target1Register(0), _source1Register(0), _source2Operand(0)
      {
      }

   ARMTrg1Src2Instruction(TR_ARMOpCodes                       op,
                          TR::Node                            *node,
                          TR::RegisterDependencyConditions *cond,
                          TR::CodeGenerator                   *cg)
      : TR::Instruction(op, node, cond, cg), _target1Register(0), _source1Register(0), _source2Operand(0)
      {
      }

   ARMTrg1Src2Instruction(TR_ARMOpCodes    op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR::Register      *s1reg,
                          TR_ARMOperand2   *s2op,
                          TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _target1Register(treg), _source1Register(s1reg), _source2Operand(s2op)
      {
      treg->incTotalUseCount();
      s1reg->incTotalUseCount();
      s2op->incTotalUseCount();
      }

   ARMTrg1Src2Instruction(TR::Instruction   *precedingInstruction,
                          TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR::Register      *s1reg,
                          TR_ARMOperand2   *s2op,
                          TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg),  _target1Register(treg), _source1Register(s1reg), _source2Operand(s2op)
      {
      treg->incTotalUseCount();
      s1reg->incTotalUseCount();
      s2op->incTotalUseCount();
      }

// The following are for convenience so one doesn't need to build a
// TR_ARMOperand2 by hand each time
   ARMTrg1Src2Instruction(TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR::Register      *s1reg,
                          TR::Register      *s2reg,
                          TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _target1Register(treg), _source1Register(s1reg)
      {
      TR_ARMOperand2 *s2op = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, s2reg);
      setSource2Operand(s2op);
      treg->incTotalUseCount();
      s1reg->incTotalUseCount();
      s2op->incTotalUseCount();
      }

   ARMTrg1Src2Instruction(TR::Instruction   *precedingInstruction,
                          TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR::Register      *s1reg,
                          TR::Register      *s2reg,
                          TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg), _target1Register(treg), _source1Register(s1reg)
      {
      TR_ARMOperand2 *s2op = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, s2reg);
      setSource2Operand(s2op);
      treg->incTotalUseCount();
      s1reg->incTotalUseCount();
      s2op->incTotalUseCount();
      }

   virtual Kind getKind() { return IsTrg1Src2; }

   TR::Register *getTarget1Register()                   {return _target1Register;}
   TR::Register *setTarget1Register(TR::Register *treg1) {return (_target1Register = treg1);}

   TR::Register *getSource1Register()                   {return _source1Register;}
   TR::Register *setSource1Register(TR::Register *sreg1) {return (_source1Register = sreg1);}

   TR_ARMOperand2 *getSource2Operand()                    {return _source2Operand;}
   TR_ARMOperand2 *setSource2Operand(TR_ARMOperand2 *sop) {return (_source2Operand = sop);}

   void insertTargetRegister(uint32_t *instruction, TR_Memory * m)
      {
      TR::Register *treg = getTarget1Register();
      TR::RealRegister *realtreg = 0;
      if(treg)
         {
         realtreg = toRealRegister(treg);
         }
      else
         {  // no target -> SBZ (should be zero)
         realtreg = new (m->trHeapMemory()) TR::RealRegister(cg());
         realtreg->setRegisterNumber(TR::RealRegister::NoReg);
         }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      if(isSinglePrecision(realtreg->getRegisterNumber()))
         {
         realtreg->setRegisterFieldSD(instruction);
         }
      else
         {
         realtreg->setRegisterFieldRD(instruction);
         }
#else
      realtreg->setRegisterFieldRD(instruction);
#endif
      }

   void insertSource1Register(uint32_t *instruction)
      {
      TR::Register *sreg = getSource1Register();
      if(sreg)  // can be null in trg1src1 subclass
         {
         TR::RealRegister *source1 = toRealRegister(sreg);
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         if(isSinglePrecision(source1->getRegisterNumber()))
            {
            source1->setRegisterFieldSN(instruction);
            }
         else
            {
             source1->setRegisterFieldRN(instruction);
            }
#else
         source1->setRegisterFieldRN(instruction);
#endif
         }
      }

   void insertSource2Operand(uint32_t *instruction)
      {
      getSource2Operand()->setBinaryEncoding(instruction);
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);
   };

class ARMLoadStartPCInstruction : public TR::ARMTrg1Src2Instruction
   {
   TR::SymbolReference           *_symbolReference;

   public:
   ARMLoadStartPCInstruction(TR::Node          *node,
                             TR::Register      *treg,
                             TR::SymbolReference *symRef,
                             TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(ARMOp_sub, node, treg, cg->machine()->getRealRegister(TR::RealRegister::gr15),
         new (cg->trHeapMemory()) TR_ARMOperand2(0xde, 24), cg), /* The value 0xde does not mean anything. It will be replaced in the binary encoding phase. */
        _symbolReference(symRef)
      {
      }

   ARMLoadStartPCInstruction(TR::Instruction   *precedingInstruction,
                             TR::Node          *node,
                             TR::Register      *treg,
                             TR::SymbolReference *symRef,
                             TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(precedingInstruction, ARMOp_sub, node, treg, cg->machine()->getRealRegister(TR::RealRegister::gr15),
         new (cg->trHeapMemory()) TR_ARMOperand2(0xde, 0), cg),  /* The value 0xde does not mean anything. It will be replaced in the binary encoding phase. */
        _symbolReference(symRef)
      {
      }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   virtual uint8_t *generateBinaryEncoding();
   };

class ARMSrc2Instruction : public TR::ARMTrg1Src2Instruction
   {

   public:

   ARMSrc2Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMTrg1Src2Instruction(node, cg) {}

   ARMSrc2Instruction(TR_ARMOpCodes     op,
                      TR::Node          *node,
                      TR::Register      *s1reg,
                      TR_ARMOperand2   *s2op,
                      TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(op, node, cg)
      {
      setSource1Register(s1reg);
      setSource2Operand(s2op);
      s1reg->incTotalUseCount();
      s2op->incTotalUseCount();
      }

   ARMSrc2Instruction(TR::Instruction   *precedingInstruction,
                      TR_ARMOpCodes     op,
                      TR::Node          *node,
                      TR::Register      *s1reg,
                      TR_ARMOperand2   *s2op,
                      TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(precedingInstruction, op, node, cg)
      {
      setSource1Register(s1reg);
      setSource2Operand(s2op);
      s1reg->incTotalUseCount();
      s2op->incTotalUseCount();
      }

   virtual Kind getKind() { return IsSrc2; }
   };


class ARMTrg1Src1Instruction : public TR::ARMTrg1Src2Instruction
   {

   public:

   ARMTrg1Src1Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMTrg1Src2Instruction(node, cg) {}

   ARMTrg1Src1Instruction(TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR_ARMOperand2   *sop,
                          TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(op, node, cg)
      {
      setTarget1Register(treg);
      setSource2Operand(sop);
      treg->incTotalUseCount();
      sop->incTotalUseCount();
      }

   ARMTrg1Src1Instruction(TR_ARMOpCodes                       op,
                          TR::Node                            *node,
                          TR::RegisterDependencyConditions *cond,
                          TR::Register                        *treg,
                          TR_ARMOperand2                     *sop,
                          TR::CodeGenerator                   *cg)
      : TR::ARMTrg1Src2Instruction(op, node, cond, cg)
      {
      setTarget1Register(treg);
      setSource2Operand(sop);
      treg->incTotalUseCount();
      sop->incTotalUseCount();
      }

   ARMTrg1Src1Instruction(TR::Instruction   *precedingInstruction,
                          TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR_ARMOperand2   *sop,
                          TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(precedingInstruction, op, node, cg)
      {
      setTarget1Register(treg);
      setSource2Operand(sop);
      treg->incTotalUseCount();
      sop->incTotalUseCount();
      }

   // The following are for convenience so one doesn't need to build a
   // TR_ARMOperand2 by hand each time
   ARMTrg1Src1Instruction(TR::Instruction   *precedingInstruction,
                          TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR::Register      *sreg,
                          TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(precedingInstruction, op, node, cg)
      {
      TR_ARMOperand2 *sop = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, sreg);
      setTarget1Register(treg);
      setSource2Operand(sop);
      treg->incTotalUseCount();
      sop->incTotalUseCount();
      }

   ARMTrg1Src1Instruction(TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *treg,
                          TR::Register      *sreg,
                          TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(op, node, cg)
      {
      TR_ARMOperand2 *sop = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, sreg);
      setTarget1Register(treg);
      setSource2Operand(sop);
      treg->incTotalUseCount();
      sop->incTotalUseCount();
      }

   ARMTrg1Src1Instruction(TR_ARMOpCodes                       op,
                          TR::Node                            *node,
                          TR::RegisterDependencyConditions *cond,
                          TR::Register                        *treg,
                          TR::Register                        *sreg,
                          TR::CodeGenerator                   *cg)
      : TR::ARMTrg1Src2Instruction(op, node, cond, cg)
      {
      TR_ARMOperand2 *sop = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, sreg);
      setTarget1Register(treg);
      setSource2Operand(sop);
      treg->incTotalUseCount();
      sop->incTotalUseCount();
      }

   virtual Kind getKind() { return IsTrg1Src1; }
   };


class ARMTrg1Instruction : public TR::Instruction
   {
   TR::Register *_target1Register;

   public:

   ARMTrg1Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::Instruction(node, cg) {}


   ARMTrg1Instruction(TR_ARMOpCodes     op,
                      TR::Node          *node,
                      TR::Register      *treg,
                      TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg),
        _target1Register(treg)
      {
      treg->incTotalUseCount();
      }

   ARMTrg1Instruction(TR::Instruction   *precedingInstruction,
                      TR_ARMOpCodes     op,
                      TR::Node          *node,
                      TR::Register      *treg,
                      TR::CodeGenerator *cg)
      : TR::Instruction(precedingInstruction, op, node, cg),
        _target1Register(treg)
      {
      treg->incTotalUseCount();
      }

   virtual Kind getKind() { return IsTrg1; }

   virtual uint8_t *generateBinaryEncoding();

   TR::Register *getTargetRegister() {return _target1Register;}
   TR::Register *setTargetRegister(TR::Register *treg) {return _target1Register = treg;}

   void insertTargetRegister(uint32_t *instruction)
      {
      TR::RealRegister *target = toRealRegister(_target1Register);
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      if(isSinglePrecision(target->getRegisterNumber()))
         {
         target->setRegisterFieldSD(instruction);
         }
      else
         {
         target->setRegisterFieldRD(instruction);
         }
#else
      target->setRegisterFieldRD(instruction);
#endif
      }

   bool refsRegister(TR::Register *reg);
   bool defsRegister(TR::Register *reg);
   bool usesRegister(TR::Register *reg);

   bool defsRealRegister(TR::Register *reg);
   void assignRegisters(TR_RegisterKinds kindToBeAssigned);
   };

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
class ARMTrg2Src1Instruction : public TR::ARMTrg1Src2Instruction
   {
   // For fmrrd
   TR::Register *_target2Register;

   public:

   ARMTrg2Src1Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMTrg1Src2Instruction(node, cg), _target2Register(0) {}

   ARMTrg2Src1Instruction(TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *t1reg,
                          TR::Register      *t2reg,
                          TR::Register      *sreg,
                          TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(op, node, cg), _target2Register(t2reg)
      {
      setTarget1Register(t1reg);
      setSource1Register(sreg);
      t1reg->incTotalUseCount();
      t2reg->incTotalUseCount();
      sreg->incTotalUseCount();
      }

   ARMTrg2Src1Instruction(TR::Instruction   *precedingInstruction,
                          TR_ARMOpCodes     op,
                          TR::Node          *node,
                          TR::Register      *t1reg,
                          TR::Register      *t2reg,
                          TR::Register      *sreg,
                          TR::CodeGenerator *cg)
      : TR::ARMTrg1Src2Instruction(precedingInstruction, op, node, cg), _target2Register(t2reg)
      {
      setTarget1Register(t1reg);
      setSource1Register(sreg);
      t1reg->incTotalUseCount();
      t2reg->incTotalUseCount();
      sreg->incTotalUseCount();
      }
   virtual Kind getKind() { return IsTrg2Src1; }

   TR::Register *getTarget2Register()                   {return _target2Register;}
   TR::Register *setTarget2Register(TR::Register *treg2) {return (_target2Register = treg2);}

   void insertTarget1Register(uint32_t *instruction)
      {
      TR::RealRegister *target = toRealRegister(getTarget1Register());
      target->setRegisterFieldRD(instruction);
      }

   void insertTarget2Register(uint32_t *instruction)
      {
      TR::RealRegister *target = toRealRegister(_target2Register);
      target->setRegisterFieldRN(instruction);
      }

   void insertSourceRegister(uint32_t *instruction)
      {
      TR::RealRegister *target = toRealRegister(getSource1Register());
      target->setRegisterFieldRM(instruction);
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);
   };
#endif

class ARMMulInstruction : public TR::Instruction
   {
   TR::Register    *_targetLoRegister;  // for mul only targetLoRegister is used
   TR::Register    *_targetHiRegister;  // for mull instructions only, NULL for mul instructions
   TR::Register    *_source1Register;
   TR::Register    *_source2Register;

   public:

   ARMMulInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::Instruction(node, cg) {}

   ARMMulInstruction(TR_ARMOpCodes     op,
                     TR::Node          *node,
                     TR::CodeGenerator *cg)
         : TR::Instruction(op, node, cg),
           _targetLoRegister(0),
           _targetHiRegister(0),
           _source1Register(0),
           _source2Register(0)
         {
         }

   ARMMulInstruction(TR::Instruction   *precedingInstruction,
                     TR_ARMOpCodes     op,
                     TR::Node          *node,
                     TR::CodeGenerator *cg)
         : TR::Instruction(precedingInstruction, op, node, cg),
           _targetLoRegister(0),
           _targetHiRegister(0),
           _source1Register(0),
           _source2Register(0)
         {
         }

   ARMMulInstruction(TR_ARMOpCodes                       op,
                     TR::Node                            *node,
                     TR::RegisterDependencyConditions *cond,
                     TR::CodeGenerator                   *cg)
         : TR::Instruction(op, node, cond, cg),
           _targetLoRegister(0),
           _targetHiRegister(0),
           _source1Register(0),
           _source2Register(0)
         {
         }

   ARMMulInstruction(TR_ARMOpCodes     op,
                     TR::Node          *node,
                     TR::Register      *tregLo,
                     TR::Register      *s1reg,
                     TR::Register      *s2reg,
                     TR::CodeGenerator *cg)
         : TR::Instruction(op, node, cg),
           _targetLoRegister(tregLo),
           _targetHiRegister(0),
           _source1Register(s1reg),
           _source2Register(s2reg)
         {
         tregLo->incTotalUseCount();
         s1reg->incTotalUseCount();
         s2reg->incTotalUseCount();
         }

   ARMMulInstruction(TR::Instruction   *precedingInstruction,
                     TR_ARMOpCodes     op,
                     TR::Node          *node,
                     TR::Register      *tregLo,
                     TR::Register      *s1reg,
                     TR::Register      *s2reg,
                     TR::CodeGenerator *cg)
         : TR::Instruction(precedingInstruction, op, node, cg),
           _targetLoRegister(tregLo),
           _targetHiRegister(0),
           _source1Register(s1reg),
           _source2Register(s2reg)
         {
         tregLo->incTotalUseCount();
         s1reg->incTotalUseCount();
         s2reg->incTotalUseCount();
         }
// below 2 for mull instructions
   ARMMulInstruction(TR_ARMOpCodes       op,
                     TR::Node          *node,
                     TR::Register      *tregHi,
                     TR::Register      *tregLo,
                     TR::Register      *s1reg,
                     TR::Register      *s2reg,
                     TR::CodeGenerator *cg)
         : TR::Instruction(op, node, cg),
           _targetLoRegister(tregLo),
           _targetHiRegister(tregHi),
           _source1Register(s1reg),
           _source2Register(s2reg)
         {
         tregLo->incTotalUseCount();
         tregHi->incTotalUseCount();
         s1reg->incTotalUseCount();
         s2reg->incTotalUseCount();
         }

   ARMMulInstruction(TR::Instruction   *precedingInstruction,
                     TR_ARMOpCodes     op,
                     TR::Node          *node,
                     TR::Register      *tregHi,
                     TR::Register      *tregLo,
                     TR::Register      *s1reg,
                     TR::Register      *s2reg,
                     TR::CodeGenerator *cg)
         : TR::Instruction(precedingInstruction, op, node, cg),
           _targetLoRegister(tregLo),
           _targetHiRegister(tregHi),
           _source1Register(s1reg),
           _source2Register(s2reg)
         {
         tregLo->incTotalUseCount();
         tregHi->incTotalUseCount();
         s1reg->incTotalUseCount();
         s2reg->incTotalUseCount();
         }

   virtual Kind getKind() { return IsMul; }

   TR::Register *getTargetLoRegister()                   {return _targetLoRegister;}
   TR::Register *setTargetLoRegister(TR::Register *tregLo) {return (_targetLoRegister = tregLo);}

   TR::Register *getTargetHiRegister()                   {return _targetHiRegister;}
   TR::Register *setTargetHiRegister(TR::Register *tregHi) {return (_targetHiRegister = tregHi);}

   TR::Register *getSource1Register()                   {return _source1Register;}
   TR::Register *setSource1Register(TR::Register *sreg1) {return (_source1Register = sreg1);}

   TR::Register *getSource2Register()                   {return _source2Register;}
   TR::Register *setSource2Register(TR::Register *sreg2) {return (_source2Register = sreg2);}

   void insertTargetRegister(uint32_t *instruction)
      {
      // for mul, book reads as "Rd" but it's in the "Rn" slot
      // for mull, "RdHi" is in "Rn" slot and "RdLo" is "Rd" slot
      // (as referenced by all other instructions)

      //toARMRealRegister(getTargetLoRegister())->setRegisterFieldRN(instruction);
      if(getTargetHiRegister())  // for mull
         {
         toRealRegister(getTargetLoRegister())->setRegisterFieldRD(instruction);
         toRealRegister(getTargetHiRegister())->setRegisterFieldRN(instruction);
         }
      else // for mul
         {
         toRealRegister(getTargetLoRegister())->setRegisterFieldRN(instruction);
         }
      }

   void insertSource1Register(uint32_t *instruction)
      {
      toRealRegister(getSource1Register())->setRegisterFieldRM(instruction);
      }

   void insertSource2Register(uint32_t *instruction)
      {
      toRealRegister(getSource2Register())->setRegisterFieldRS(instruction);
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);
   };

// superclass of both ldr and str
class ARMMemInstruction : public TR::ARMTrg1Instruction
   {
   TR::MemoryReference *_memoryReference;
   bool                   _memoryRead;

   public:

   ARMMemInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMTrg1Instruction(node, cg) {}

   ARMMemInstruction(TR_ARMOpCodes          op,
                     TR::Node               *node,
                     TR::Register           *treg,
                     TR::MemoryReference *mf,
                     bool                   memRead,
                     TR::CodeGenerator *cg)
         : TR::ARMTrg1Instruction(op, node, treg, cg), _memoryReference(mf), _memoryRead(memRead)
      {
      mf->incRegisterTotalUseCounts(cg);
      }

   ARMMemInstruction(TR::Instruction         *precedingInstruction,
                     TR_ARMOpCodes          op,
                     TR::Node                *node,
                     TR::Register            *treg,
                     TR::MemoryReference  *mf,
                     bool                   memRead,
                     TR::CodeGenerator *cg)
         : TR::ARMTrg1Instruction(precedingInstruction, op, node, treg, cg), _memoryReference(mf), _memoryRead(memRead)
      {
      mf->incRegisterTotalUseCounts(cg);
      }

   virtual Kind getKind() { return IsMem; }

   TR::MemoryReference *getMemoryReference() {return _memoryReference;}
   TR::MemoryReference *setMemoryReference(TR::MemoryReference *p)
      {
      _memoryReference = p;
      return p;
      }

   virtual TR::Snippet *getSnippetForGC() {return getMemoryReference()->getUnresolvedSnippet();}

   virtual TR::Register *getMemoryDataRegister();

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);
   };

class ARMMemSrc1Instruction : public TR::ARMMemInstruction
   {
   public:

   ARMMemSrc1Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMMemInstruction(node, cg) {}

   ARMMemSrc1Instruction(TR_ARMOpCodes         op,
                         TR::Node               *node,
                         TR::MemoryReference *mf,
                         TR::Register           *sreg,
                         TR::CodeGenerator *cg)
         : TR::ARMMemInstruction(op, node, sreg, mf, true, cg)
      {
      }

   ARMMemSrc1Instruction(TR::Instruction        *precedingInstruction,
                         TR_ARMOpCodes         op,
                         TR::Node               *node,
                         TR::MemoryReference *mf,
                         TR::Register           *sreg,
                         TR::CodeGenerator *cg)
         : TR::ARMMemInstruction(precedingInstruction, op, node, sreg, mf, true, cg)
      {
      }

   virtual Kind getKind() { return IsMemSrc1; }

   TR::Register *getSourceRegister()                {return getTargetRegister();}
   TR::Register *setSourceRegister(TR::Register *sr) {return setTargetRegister(sr);}

   void insertSourceRegister(uint32_t *instruction) {insertTargetRegister(instruction);}

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);
   };


class ARMTrg1MemInstruction : public TR::ARMMemInstruction
   {
   public:


   ARMTrg1MemInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMMemInstruction(node, cg) {}

   ARMTrg1MemInstruction(TR_ARMOpCodes          op,
                         TR::Node               *node,
                         TR::Register           *sreg,
                         TR::MemoryReference *mf,
                         TR::CodeGenerator      *cg)
      : TR::ARMMemInstruction(op, node, sreg, mf, false, cg) {}

   ARMTrg1MemInstruction(TR::Instruction        *precedingInstruction,
                         TR_ARMOpCodes          op,
                         TR::Node               *node,
                         TR::Register           *sreg,
                         TR::MemoryReference *mf,
                         TR::CodeGenerator      *cg)
      : TR::ARMMemInstruction(precedingInstruction, op, node, sreg, mf, false, cg) {}

   virtual Kind getKind() { return IsTrg1Mem; }

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);
   };

class ARMTrg1MemSrc1Instruction : public TR::ARMMemInstruction
   {
   TR::Register *_source1Register;

   public:

   ARMTrg1MemSrc1Instruction(TR::Node *node, TR::CodeGenerator *cg) : TR::ARMMemInstruction(node, cg) {}

   ARMTrg1MemSrc1Instruction(TR_ARMOpCodes          op,
                             TR::Node               *node,
                             TR::Register           *treg,
                             TR::MemoryReference *mf,
                             TR::Register           *sreg,
                             TR::CodeGenerator      *cg)
      : TR::ARMMemInstruction(op, node, treg, mf, false, cg), _source1Register(sreg) {}

   ARMTrg1MemSrc1Instruction(TR::Instruction        *precedingInstruction,
                             TR_ARMOpCodes          op,
                             TR::Node               *node,
                             TR::Register           *treg,
                             TR::MemoryReference *mf,
                             TR::Register           *sreg,
                             TR::CodeGenerator      *cg)
      : TR::ARMMemInstruction(precedingInstruction, op, node, treg, mf, false, cg), _source1Register(sreg) {}

   virtual Kind getKind() { return IsTrg1MemSrc1; }

   TR::Register *getSourceRegister()                   {return _source1Register;}
   TR::Register *setSourceRegister(TR::Register *sreg1) {return (_source1Register = sreg1);}

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   virtual bool usesRegister(TR::Register *reg);

   virtual uint8_t *generateBinaryEncoding();
   void insertSourceRegister(uint32_t *instruction)
      {
      toRealRegister(getSourceRegister())->setRegisterFieldRM(instruction);
      }
   };

// If we are using a register allocator that cannot handle registers being alive across basic block
// boundaries then we use TR::ARMControlFlowInstruction as a placeholder.  It contains a summary of
// register usage information and enough other information to generate the correct instruction
// sequence.
class ARMControlFlowInstruction : public TR::Instruction
   {
   TR::Register *_sourceRegisters[8];
   TR::Register *_targetRegisters[5];
   int32_t _numSources;
   int32_t _numTargets;
   TR::LabelSymbol *_label;
   TR_ARMOpCode _opCode2;
   TR_ARMOpCode _opCode3;
   TR_ARMOpCode _cmpOp;

   public:

   ARMControlFlowInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::Instruction(node, cg) {}
   ARMControlFlowInstruction(TR_ARMOpCodes  op, TR::Node *node, TR::CodeGenerator *cg)
      : TR::Instruction(op, node, cg), _numSources(0), _numTargets(0), _label(NULL), _opCode2(ARMOp_bad)
      {
      }

   ARMControlFlowInstruction(TR_ARMOpCodes  op, TR::Node *node, TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg)
      : TR::Instruction(op, node, deps, cg), _numSources(0), _numTargets(0), _label(NULL), _opCode2(ARMOp_bad)
      {
      }

   virtual Kind getKind() { return IsControlFlow; }

   int32_t getNumSources()                    {return _numSources;}
   TR::Register *getSourceRegister(int32_t i)  {return _sourceRegisters[i];}
   TR::Register *setSourceRegister(int32_t i, TR::Register *sr)
      {
      return (_sourceRegisters[i] = sr);
      }

   TR::Register *addSourceRegister(TR::Register *sr)
      {
      int i = _numSources;
      sr->incTotalUseCount();
      _numSources++;
      return (_sourceRegisters[i] = sr);
      }

   int32_t getNumTargets()                    {return _numTargets;}
   TR::Register *getTargetRegister(int32_t i)  {return _targetRegisters[i];}
   TR::Register *setTargetRegister(int32_t i, TR::Register *tr)
      {
      return (_targetRegisters[i] = tr);
      }
   TR::Register *addTargetRegister(TR::Register *tr)
      {
      int i = _numTargets;
      tr->incTotalUseCount();
      _numTargets++;
      return (_targetRegisters[i] = tr);
      }

   TR::LabelSymbol *getLabelSymbol()                     {return _label;}
   TR::LabelSymbol *setLabelSymbol(TR::LabelSymbol *sym)  {return (_label = sym);}

   TR_ARMOpCode& getOpCode2()                      {return _opCode2;}
   TR_ARMOpCodes getOpCode2Value()                 {return _opCode2.getOpCodeValue();}
   TR_ARMOpCodes setOpCode2Value(TR_ARMOpCodes op) {return (_opCode2.setOpCodeValue(op));}

   TR_ARMOpCode& getOpCode3()                      {return _opCode3;}
   TR_ARMOpCodes getOpCode3Value()                 {return _opCode3.getOpCodeValue();}
   TR_ARMOpCodes setOpCode3Value(TR_ARMOpCodes op) {return (_opCode3.setOpCodeValue(op));}

   TR_ARMOpCode& getCmpOp()                      {return _cmpOp;}
   TR_ARMOpCodes getCmpOpValue()                 {return _cmpOp.getOpCodeValue();}
   TR_ARMOpCodes setCmpOpValue(TR_ARMOpCodes op) {return (_cmpOp.setOpCodeValue(op));}

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual uint8_t *generateBinaryEncoding();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);
   };

class ARMMultipleMoveInstruction : public TR::Instruction
   {
   TR::Register    *_memoryBaseRegister;
   uint16_t       _registerList;
   bool           _writeBack;
   bool           _increment;
   bool           _preIndex;

   public:

   ARMMultipleMoveInstruction(TR::Node *node, TR::CodeGenerator *cg) : TR::Instruction(node, cg) {}

   ARMMultipleMoveInstruction(TR_ARMOpCodes op, TR::Node *node, TR::CodeGenerator *cg)
         : TR::Instruction(op, node, cg), _memoryBaseRegister(0), _registerList(0), _writeBack(false), _increment(false), _preIndex(false)
         {
         }

   ARMMultipleMoveInstruction(TR::Instruction * precedingInstruction,
                              TR_ARMOpCodes    op,
                              TR::Node *node,
                              TR::CodeGenerator *cg)
         : TR::Instruction(precedingInstruction, op, node, cg), _memoryBaseRegister(0), _registerList(0), _writeBack(false), _increment(false), _preIndex(false)
         {
         }

   ARMMultipleMoveInstruction(TR_ARMOpCodes                        op,
                              TR::Node                            * node,
                              TR::RegisterDependencyConditions * cond,
                              TR::CodeGenerator *cg)
         : TR::Instruction(op, node, cond, cg), _memoryBaseRegister(0), _registerList(0), _writeBack(false), _increment(false), _preIndex(false)
         {
         }

   ARMMultipleMoveInstruction(TR_ARMOpCodes    op,
                              TR::Node        * node,
                              TR::Register    * mbr,
                              uint16_t         rl,
                              TR::CodeGenerator *cg)
         : TR::Instruction(op, node, cg), _memoryBaseRegister(mbr), _registerList(rl), _writeBack(false), _increment(false), _preIndex(false)
         {
         mbr->incTotalUseCount();
         }

   ARMMultipleMoveInstruction(TR::Instruction * precedingInstruction,
                              TR_ARMOpCodes    op,
                              TR::Node        * node,
                              TR::Register    * mbr,
                              uint16_t         rl,
                              TR::CodeGenerator *cg)
         : TR::Instruction(precedingInstruction, op, node, cg), _memoryBaseRegister(mbr), _registerList(rl), _writeBack(false), _increment(false), _preIndex(false)
         {
         mbr->incTotalUseCount();
         }

   virtual Kind getKind() { return IsMultipleMove; }

   TR::Register *getMemoryBaseRegister()                 {return _memoryBaseRegister;}
   TR::Register *setMemoryBaseRegister(TR::Register *mbr) {return (_memoryBaseRegister = mbr);}

   uint16_t getRegisterList()            {return _registerList;}
   uint16_t setRegisterList(uint16_t rl) {return (_registerList = rl);}

   bool setWriteBack()     {return (_writeBack = true);}
   bool isWriteBack()      {return _writeBack;}

   bool setIncrement()     {return (_increment = true);}
   bool isIncrement()      {return _increment;}
   bool setPreIndex()      {return (_preIndex = true);}
   bool isPreIndex()       {return _preIndex;}

   void insertMemoryBaseRegister(uint32_t *instruction, TR_Memory * m)
      {
      TR::Register *treg = getMemoryBaseRegister();
      TR::RealRegister *realtreg = 0;
      if(treg)
         {
         realtreg = toRealRegister(treg);
         }
      else
         {  // no target -> SBZ (should be zero)
         realtreg = new (m->trHeapMemory()) TR::RealRegister(cg());
         realtreg->setRegisterNumber(TR::RealRegister::NoReg);
         }
      realtreg->setRegisterFieldRN(instruction);
      }

   void insertRegisterList(uint32_t *instruction)
      {
      *instruction |= (uint32_t) getRegisterList();
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);

   virtual bool defsRealRegister(TR::Register *reg);
   };

}

typedef TR::ARMImmInstruction TR_ARMImmInstruction;
typedef TR::ARMLabelInstruction TR_ARMLabelInstruction;
typedef TR::ARMConditionalBranchInstruction TR_ARMConditionalBranchInstruction;
typedef TR::ARMVirtualGuardNOPInstruction TR_ARMVirtualGuardNOPInstruction;
typedef TR::ARMAdminInstruction TR_ARMAdminInstruction;
typedef TR::ARMImmSymInstruction TR_ARMImmSymInstruction;
typedef TR::ARMTrg1Src2Instruction TR_ARMTrg1Src2Instruction;
typedef TR::ARMLoadStartPCInstruction TR_ARMLoadStartPCInstruction;
typedef TR::ARMSrc2Instruction TR_ARMSrc2Instruction;
typedef TR::ARMTrg1Src1Instruction TR_ARMTrg1Src1Instruction;
typedef TR::ARMTrg1Instruction TR_ARMTrg1Instruction;
typedef TR::ARMTrg2Src1Instruction TR_ARMTrg2Src1Instruction;
typedef TR::ARMMulInstruction TR_ARMMulInstruction;
typedef TR::ARMMemInstruction TR_ARMMemInstruction;
typedef TR::ARMMemSrc1Instruction TR_ARMMemSrc1Instruction;
typedef TR::ARMTrg1MemInstruction TR_ARMTrg1MemInstruction;
typedef TR::ARMTrg1MemSrc1Instruction TR_ARMTrg1MemSrc1Instruction;
typedef TR::ARMControlFlowInstruction TR_ARMControlFlowInstruction;
typedef TR::ARMMultipleMoveInstruction TR_ARMMultipleMoveInstruction;

#endif
