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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef S390INSTRUCTION_INCL
#define S390INSTRUCTION_INCL

#include <stdint.h>                           // for uint8_t, int32_t, etc
#include <string.h>                           // for NULL, strcpy, strlen
#include "codegen/CodeGenerator.hpp"          // for CodeGenerator
#include "codegen/InstOpCode.hpp"             // for InstOpCode, etc
#include "codegen/Instruction.hpp"            // for Instruction, etc
#include "codegen/MemoryReference.hpp"        // for MemoryReference
#include "codegen/RealRegister.hpp"           // for RealRegister, etc
#include "codegen/Register.hpp"               // for Register
#include "codegen/RegisterConstants.hpp"      // for TR_RegisterKinds, etc
#include "codegen/RegisterPair.hpp"           // for RegisterPair
#include "codegen/Snippet.hpp"                // for Snippet
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"            // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                     // for uintptrj_t, intptrj_t
#include "il/symbol/LabelSymbol.hpp"          // for LabelSymbol
#include "infra/Assert.hpp"                   // for TR_ASSERT
#include "infra/Flags.hpp"                    // for flags8_t, flags32_t
#include "ras/Debug.hpp"                      // for TR_DebugBase

#include "codegen/RegisterDependency.hpp"

class TR_AsmData;
class TR_VirtualGuardSite;
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }

// Instrumentation flags
//
#define EXCHREG    0x0001
#define MOVEREG    0x0002
#define CLOBREG    0x0004
#define PARMREG    0x0008
#define USERREG    0x0010
#define PAIRREG    0x0100
#define GLBLREG    0x0200
#define DEPSREG    0x0400


////////////////////////////////////////////////////////////////////////////////
// TR::S390Instruction Class Definition
////////////////////////////////////////////////////////////////////////////////

/**
 * Pseudo-safe downcast function.
 */
inline uint32_t *toS390Cursor(uint8_t *i)
   {
   return (uint32_t *)i;
   }

namespace TR {

////////////////////////////////////////////////////////////////////////////////
// TR::S390LabeledInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390LabeledInstruction : public TR::Instruction
   {
   TR::LabelSymbol *_symbol;
   TR::Snippet     *_snippet;

   public:

   S390LabeledInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _symbol(sym), _snippet(NULL)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg), _symbol(sym), _snippet(NULL)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _symbol(sym), _snippet(NULL)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::RegisterDependencyConditions * cond,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _symbol(sym), _snippet(NULL)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::CodeGenerator  *cg)
      : TR::Instruction(op, n, cg), _symbol(NULL), _snippet(s)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator  *cg)
      : TR::Instruction(op, n, cond, cg), _symbol(NULL), _snippet(s)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::Instruction    *precedingInstruction,
                           TR::CodeGenerator  *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _symbol(NULL), _snippet(s)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   S390LabeledInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::RegisterDependencyConditions * cond,
                           TR::Instruction    *precedingInstruction,
                           TR::CodeGenerator  *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _symbol(NULL), _snippet(s)
      {
         if ( !cg->comp()->getOption(TR_EnableEBBCCInfo) )
            clearCCInfo();
         else if (isLabel() || isCall()) //otherwise clearCCInfo for real Label instruction or call only
            clearCCInfo();
      }

   TR::LabelSymbol *getLabelSymbol()
      {
         return _symbol;
      }

   TR::LabelSymbol *setLabelSymbol(TR::LabelSymbol *sym) {return _symbol = sym;}

   TR::Snippet     *getCallSnippet()                    {return _snippet;}

   virtual char *description() { return "S390Instruction"; }
   virtual Kind getKind()=0;
   virtual uint8_t *generateBinaryEncoding()=0;
   virtual int32_t estimateBinaryLength(int32_t currentEstimate)=0;

   virtual bool isNopCandidate();  // Check to determine whether we should NOP to align instruction.
   };

////////////////////////////////////////////////////////////////////////////////
// S390BranchInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390BranchInstruction : public TR::S390LabeledInstruction
   {
   TR::InstOpCode::S390BranchCondition _branchCondition;

   public:

   S390BranchInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cond, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::RegisterDependencyConditions * cond,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cond, precedingInstruction, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, cond, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::Instruction    *precedingInstruction,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, precedingInstruction, cg),
        _branchCondition(branchCondition)
      {}

   S390BranchInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::InstOpCode::S390BranchCondition branchCondition,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::RegisterDependencyConditions * cond,
                           TR::Instruction    *precedingInstruction,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, cond, precedingInstruction, cg),
        _branchCondition(branchCondition)
      {}

   virtual char *description() { return "S390Branch"; }
   virtual Kind getKind() { return IsBranch; }

   virtual TR::Snippet *getSnippetForGC()
      {
      if (getLabelSymbol() != NULL)
         return getLabelSymbol()->getSnippet();
      return NULL;
      }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   void assignRegistersAndDependencies(TR_RegisterKinds kindToBeAssigned);
   TR::InstOpCode::S390BranchCondition getBranchCondition()  {return _branchCondition;}
   TR::InstOpCode::S390BranchCondition setBranchCondition(TR::InstOpCode::S390BranchCondition branchCondition) {return _branchCondition = branchCondition;}
   uint8_t getMask() {return getMaskForBranchCondition(getBranchCondition()) << 4;}
   };

////////////////////////////////////////////////////////////////////////////////
// For virtual guard nop instruction
////////////////////////////////////////////////////////////////////////////////

#ifdef J9_PROJECT_SPECIFIC
class S390VirtualGuardNOPInstruction : public TR::S390BranchInstruction
   {
   private:
   TR_VirtualGuardSite     *_site;

   public:
   S390VirtualGuardNOPInstruction(TR::Node                            *node,
                                    TR_VirtualGuardSite            *site,
                                    TR::RegisterDependencyConditions *cond,
                                    TR::LabelSymbol                       *label,
                                    TR::CodeGenerator                    *cg)
      : S390BranchInstruction(TR::InstOpCode::BRC, TR::InstOpCode::COND_VGNOP, node, label, cond, cg), _site(site) {}

   S390VirtualGuardNOPInstruction(TR::Node                            *node,
                                    TR_VirtualGuardSite            *site,
                                    TR::RegisterDependencyConditions *cond,
                                    TR::LabelSymbol                       *label,
                                    TR::Instruction                      *precedingInstruction,
                                    TR::CodeGenerator                    *cg)
      : S390BranchInstruction(TR::InstOpCode::BRC, TR::InstOpCode::COND_VGNOP, node, label, cond, precedingInstruction, cg), _site(site) {}

   virtual char *description() { return "S390VirtualGuardNOP"; }
   virtual Kind getKind() { return IsVirtualGuardNOP; }

   void setSite(TR_VirtualGuardSite *site) { _site = site; }
   TR_VirtualGuardSite * getSite() { return _site; }

   virtual uint8_t *generateBinaryEncoding();
   virtual bool     isVirtualGuardNOPInstruction() {return true;}
   };
#endif

////////////////////////////////////////////////////////////////////////////////
// S390BranchOnCountInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390BranchOnCountInstruction : public TR::S390LabeledInstruction
   {
   #define br_targidx 0

   public:
   S390BranchOnCountInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cg)
      {useTargetRegister(targetReg);}

   S390BranchOnCountInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *targetReg,
                           TR::RegisterDependencyConditions * cond,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cond, cg)
      {useTargetRegister(targetReg); }

   S390BranchOnCountInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg)
         {useTargetRegister(targetReg); }

   S390BranchOnCountInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *targetReg,
                           TR::RegisterDependencyConditions * cond,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg)
      {useTargetRegister(targetReg);}

   virtual char *description() { return "S390BranchOnCount"; }
   virtual Kind getKind() { return IsBranchOnCount; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual bool refsRegister(TR::Register *reg);
   };

////////////////////////////////////////////////////////////////////////////////
// S390BranchOnCountInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390BranchOnIndexInstruction : public TR::S390LabeledInstruction
   {
   #define br_srcidx 0
   #define br_targidx 0

   public:
   S390BranchOnIndexInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *sourceReg,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cg)
      {useTargetRegister(targetReg); useSourceRegister(sourceReg); }

   S390BranchOnIndexInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *sourceReg,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg)
      {useTargetRegister(targetReg); useSourceRegister(sourceReg); }

   S390BranchOnIndexInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::RegisterPair  *sourceReg,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cg)
      {useTargetRegister(targetReg); useSourceRegister(sourceReg);}

   S390BranchOnIndexInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::RegisterPair  *sourceReg,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg)
      {useTargetRegister(targetReg); useSourceRegister(sourceReg);}

   virtual char *description() { return "S390BranchOnIndex"; }
   virtual Kind getKind() { return IsBranchOnIndex; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual bool refsRegister(TR::Register *reg);
   };

////////////////////////////////////////////////////////////////////////////////
// S390LabelInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390LabelInstruction : public TR::S390LabeledInstruction
   {
   flags8_t _flags;

   enum
      {
      doPrint                         = 0x01,
      skipForLabelTargetNOPs          = 0x02,
      estimateDoneForLabelTargetNOPs  = 0x04,
      // AVAILABLE                    = 0x08
      };

   public:

   S390LabelInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cg), _alignment(0), _handle(0), _flags(0)
      {
      if (op==TR::InstOpCode::LABEL)
         sym->setInstruction(this);
      cg->getNextAvailableBlockIndex();
      }

   S390LabelInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cg), _alignment(0), _handle(0), _flags(0)
      {
      if (op==TR::InstOpCode::LABEL)
         sym->setInstruction(this);
      cg->getNextAvailableBlockIndex();
      }

   S390LabelInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::Register      *targetReg,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg), _alignment(0), _handle(0), _flags(0)
      {
      if (op==TR::InstOpCode::LABEL)
         sym->setInstruction(this);
      cg->getNextAvailableBlockIndex();
      }

   S390LabelInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cond, cg), _alignment(0), _handle(0), _flags(0)
      {
      if (op==TR::InstOpCode::LABEL)
         sym->setInstruction(this);
      cg->getNextAvailableBlockIndex();
      }

   S390LabelInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, precedingInstruction, cg), _alignment(0), _handle(0), _flags(0)
      {
      if (op==TR::InstOpCode::LABEL)
         sym->setInstruction(this);
      cg->getNextAvailableBlockIndex();
      }

   S390LabelInstruction(TR::InstOpCode::Mnemonic    op,
                           TR::Node          *n,
                           TR::LabelSymbol    *sym,
                           TR::RegisterDependencyConditions * cond,
                           TR::Instruction   *precedingInstruction,
                           TR::CodeGenerator *cg)
      : S390LabeledInstruction(op, n, sym, cond, precedingInstruction, cg), _alignment(0), _handle(0), _flags(0)
      {
      if (op==TR::InstOpCode::LABEL)
         sym->setInstruction(this);
      cg->getNextAvailableBlockIndex();
      }

   S390LabelInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, cg), _alignment(0), _handle(0), _flags(0)
      {}

   S390LabelInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, cond, cg), _alignment(0), _handle(0), _flags(0)
      {}

   S390LabelInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::Instruction    *precedingInstruction,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, precedingInstruction, cg), _alignment(0), _handle(0), _flags(0)
      {}

   S390LabelInstruction(TR::InstOpCode::Mnemonic     op,
                           TR::Node           *n,
                           TR::Snippet        *s,
                           TR::RegisterDependencyConditions * cond,
                           TR::Instruction    *precedingInstruction,
                           TR::CodeGenerator  *cg)
      : S390LabeledInstruction(op, n, s, cond, precedingInstruction, cg), _alignment(0), _handle(0), _flags(0)
      {}

   virtual char *description() { return "S390LabelInstruction"; }
   virtual Kind getKind() { return IsLabel; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   void assignRegistersAndDependencies(TR_RegisterKinds kindToBeAssigned);

   void preservedForListing()    { _flags.set(doPrint);}
   bool isPreservedForListing()  {return _flags.testAny(doPrint); }

   void setSkipForLabelTargetNOPs()    { _flags.set(skipForLabelTargetNOPs);}
   bool isSkipForLabelTargetNOPs()     {return _flags.testAny(skipForLabelTargetNOPs); }

   void setEstimateDoneForLabelTargetNOPs()     { _flags.set(estimateDoneForLabelTargetNOPs);}
   bool wasEstimateDoneForLabelTargetNOPs()     {return _flags.testAny(estimateDoneForLabelTargetNOPs); }

   bool considerForLabelTargetNOPs(bool inEncodingPhase);

   uint16_t getAlignment()          {return _alignment;}
   uint16_t setAlignment(uint16_t alignment) {return _alignment = alignment;}

   protected:
      uint16_t _alignment;
      int32_t _handle;
   };

////////////////////////////////////////////////////////////////////////////////
// S390PseudoInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390PseudoInstruction : public TR::Instruction
   {
   TR::Node *_fenceNode;

   uint64_t _callDescValue; ///< Call Descriptor for XPLINK on zOS 31 JNI Calls.

   uint8_t _padbytes;
   TR::LabelSymbol *_callDescLabel;
   bool _shouldBeginNewLine;

   // Following is used when ASM encoding is determined at compile time
   // (e.g. for C/C++ when doing inlined asm and producing object file)
   uint8_t *_asmDataEncoding;       ///< binary encoding values
   int32_t  _asmDataEncodingLength; ///< binary encoding length

   public:

   S390PseudoInstruction(TR::InstOpCode::Mnemonic op,
                           TR::Node *n,
                           TR::Node * fenceNode,
                           TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg),
        _fenceNode(fenceNode),
        _callDescValue(0),
        _padbytes(0),
        _callDescLabel(NULL),
        _asmDataEncoding(NULL),
        _asmDataEncodingLength(0),
        _shouldBeginNewLine(false){}

   S390PseudoInstruction(TR::InstOpCode::Mnemonic op,
                           TR::Node *n,
                           TR::Node * fenceNode,
                           TR::RegisterDependencyConditions * cond,
                           TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg),
        _fenceNode(fenceNode),
        _callDescValue(0),
        _padbytes(0),
        _callDescLabel(NULL),
        _asmDataEncoding(NULL),
        _asmDataEncodingLength(0),
        _shouldBeginNewLine(false){}

   S390PseudoInstruction(TR::InstOpCode::Mnemonic  op,
                            TR::Node * n,
                            TR::Node *fenceNode,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg),
        _fenceNode(fenceNode),
        _callDescValue(0),
        _padbytes(0),
        _callDescLabel(NULL),
        _asmDataEncoding(NULL),
        _asmDataEncodingLength(0),
        _shouldBeginNewLine(false){}

   S390PseudoInstruction(TR::InstOpCode::Mnemonic  op,
                            TR::Node * n,
                            TR::Node *fenceNode,
                            TR::RegisterDependencyConditions * cond,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg),
        _fenceNode(fenceNode),
        _callDescValue(0),
        _padbytes(0),
        _callDescLabel(NULL),
        _asmDataEncoding(NULL),
        _asmDataEncodingLength(0),
        _shouldBeginNewLine(false){}

   virtual char *description() { return "S390PseudoInstruction"; }
   virtual Kind getKind() { return IsPseudo; }

   TR::Node * getFenceNode() { return _fenceNode; }

   //   TR::Register  *getTargetRegister() { return (_targetRegSize!=0) ? (targetRegBase())[0] : NULL;}

   void setShouldBeginNewLine(bool sbnl) { _shouldBeginNewLine = sbnl; }
   bool shouldBeginNewLine() { return _shouldBeginNewLine; }

   uint8_t *getASMDataEncoding() { return _asmDataEncoding; }
   void setASMDataEncoding(uint8_t *encoding) { _asmDataEncoding = encoding; }
   uint32_t  getASMDataEncodingLength() { return _asmDataEncodingLength; }
   void setASMDataEncodingLength(int32_t encodingLength) { _asmDataEncodingLength = encodingLength; }

   virtual uint8_t *generateBinaryEncoding();

   uint64_t setCallDescValue(uint64_t cdv, TR_Memory * m)
      {
      if (!_callDescLabel)
         {
         _callDescLabel = TR::LabelSymbol::create(m->trHeapMemory());
         }
      return _callDescValue = cdv;
      }
   uint64_t getCallDescValue() { return _callDescValue; }

   TR::LabelSymbol * setCallDescLabel(TR::LabelSymbol * ls) { return _callDescLabel = ls; }
   TR::LabelSymbol * getCallDescLabel() { return _callDescLabel; }

   uint8_t getNumPadBytes() { return _padbytes; }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

/**
 * Extends from S390PseudoInstruction to exploit the property of register assignment ignoring
 * these types or treating them specially
 */
class S390DebugCounterBumpInstruction : public S390PseudoInstruction
   {
private:
   /** Contains address information necessary for LGRL during binary encoding */
   TR::Snippet * _counterSnippet;

   /** A free real register for DCB to use during binary encoding */
   TR::RealRegister * _assignableReg;

   /** Specifies amount to increment the counter */
   int32_t _delta;

public:
   S390DebugCounterBumpInstruction(TR::InstOpCode::Mnemonic op,
                                      TR::Node *n,
                                      TR::Snippet* counterSnip,
                                      TR::CodeGenerator *cg,
                                      int32_t delta)
      : S390PseudoInstruction(op, n, NULL, cg),
        _counterSnippet(counterSnip),
        _assignableReg(NULL),
        _delta(delta){}

   S390DebugCounterBumpInstruction(TR::InstOpCode::Mnemonic op,
                                      TR::Node *n,
                                      TR::Snippet* counterSnip,
                                      TR::CodeGenerator *cg,
                                      int32_t delta,
                                      TR::Instruction *precedingInstruction)
      : S390PseudoInstruction(op, n, NULL, precedingInstruction, cg),
        _counterSnippet(counterSnip),
        _assignableReg(NULL),
        _delta(delta){}

   /**
   * A real register must only be assigned for DCB to use if it is guaranteed to be free at the cursor position where DCB is inserted
   * @param ar Real Register to be assigned
   */
   void setAssignableReg(TR::RealRegister * ar){ _assignableReg = ar; }

   /**
   * @return Assigned Real Register
   */
   TR::RealRegister * getAssignableReg(){ return _assignableReg; }

   /**
   * @return The snippet that holds the debugCounter's counter address in persistent memory
   */
   TR::Snippet * getCounterSnippet(){ return _counterSnippet; }

   /**
   * @return The integer amount to increment the counter
   */
   int32_t getDelta(){ return _delta; }

   virtual uint8_t *generateBinaryEncoding();
   };

////////////////////////////////////////////////////////////////////////////////
// S390AnnotationInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390AnnotationInstruction : public TR::Instruction
   {
public:

   S390AnnotationInstruction(TR::InstOpCode::Mnemonic    op,
                                TR::Node          *n,
                                int16_t regionNum,
                                int32_t statementNum,
                                int32_t flags,
                                bool printNumber,
                                char * annotation,
                                TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg),
        _annotation(annotation),
        _flags(flags),
        _shouldPrintNumber(printNumber)
      {
      setRegionNumber(regionNum);
      }

   S390AnnotationInstruction(TR::InstOpCode::Mnemonic    op,
                                TR::Node          *n,
                                int16_t regionNum,
                                int32_t statementNum,
                                int32_t flags,
                                bool printNumber,
                                char * annotation,
                                TR::Instruction   *precedingInstruction,
                                TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg),
        _annotation(annotation),
        _flags(flags),
        _shouldPrintNumber(printNumber)
      {
      setRegionNumber(regionNum);
      }

   S390AnnotationInstruction(TR::InstOpCode::Mnemonic    op,
                                TR::Node          *n,
                                int16_t regionNum,
                                int32_t statementNum,
                                int32_t flags,
                                bool printNumber,
                                char * annotation,
                                TR::RegisterDependencyConditions *cond,
                                TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg),
        _annotation(annotation),
        _flags(flags),
        _shouldPrintNumber(printNumber)
      {
      setRegionNumber(regionNum);
      }

   S390AnnotationInstruction(TR::InstOpCode::Mnemonic    op,
                                TR::Node          *n,
                                int16_t regionNum,
                                int32_t statementNum,
                                int32_t flags,
                                bool printNumber,
                                char * annotation,
                                TR::RegisterDependencyConditions *cond,
                                TR::Instruction   *precedingInstruction,
                                TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg),
        _annotation(annotation),
        _flags(flags),
        _shouldPrintNumber(printNumber)
      {
      setRegionNumber(regionNum);
      }

   virtual char *description() { return "S390AnnotInstruction"; }
   virtual Kind getKind()      { return IsAnnot; }
   char *getAnnotation()       { return _annotation; }
   int32_t getFlags()          { return _flags; }
   bool shouldPrintNumber()    { return _shouldPrintNumber; }

private:
   char *_annotation;
   int32_t _flags;
   bool _shouldPrintNumber;

   };

////////////////////////////////////////////////////////////////////////////////
// S390ImmInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390ImmInstruction : public TR::Instruction
   {
   uint32_t _sourceImmediate;

   public:

   S390ImmInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         uint32_t          imm,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _sourceImmediate(imm)
      {}

   S390ImmInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         uint32_t          imm,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _sourceImmediate(imm)
      {}

   S390ImmInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         uint32_t          imm,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg), _sourceImmediate(imm)
      {}

   S390ImmInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         uint32_t          imm,
                         TR::RegisterDependencyConditions *cond,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _sourceImmediate(imm)
      {}

   virtual char *description() { return "S390ImmInstruction"; }
   virtual Kind getKind() { return IsImm; }

   uint32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t setSourceImmediate(uint32_t si) {return _sourceImmediate = si;}

   virtual uint8_t *generateBinaryEncoding();


// The following safe virtual downcast method is used under debug only
// for assertion checking
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   virtual S390ImmInstruction *getS390ImmInstruction();
#endif
   };

////////////////////////////////////////////////////////////////////////////////
// S390Imm2Instruction (2-byte) Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390Imm2Instruction : public TR::Instruction
   {
   uint16_t _sourceImmediate;

   public:

   S390Imm2Instruction(TR::InstOpCode::Mnemonic    op,
                          TR::Node          *n,
                          uint16_t          imm,
                          TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _sourceImmediate(imm)
      { setEstimatedBinaryLength(2); }

   S390Imm2Instruction(TR::InstOpCode::Mnemonic    op,
                          TR::Node          *n,
                          uint16_t          imm,
                          TR::Instruction   *precedingInstruction,
                          TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _sourceImmediate(imm)
      { setEstimatedBinaryLength(2); }

   S390Imm2Instruction(TR::InstOpCode::Mnemonic    op,
                          TR::Node          *n,
                          uint16_t          imm,
                          TR::RegisterDependencyConditions *cond,
                          TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg), _sourceImmediate(imm)
      { setEstimatedBinaryLength(2); }

   S390Imm2Instruction(TR::InstOpCode::Mnemonic    op,
                          TR::Node          *n,
                          uint16_t          imm,
                          TR::RegisterDependencyConditions *cond,
                          TR::Instruction   *precedingInstruction,
                          TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _sourceImmediate(imm)
      { setEstimatedBinaryLength(2); }

   virtual char *description() { return "S390Imm2Instruction"; }
   virtual Kind getKind() { return IsImm2Byte; }

   uint16_t getSourceImmediate()            {return _sourceImmediate;}
   uint16_t setSourceImmediate(uint16_t si) {return _sourceImmediate = si;}

   virtual uint8_t *generateBinaryEncoding();
   };

////////////////////////////////////////////////////////////////////////////////
// S390ImmSnippetInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390ImmSnippetInstruction : public TR::S390ImmInstruction
   {
   TR::UnresolvedDataSnippet *_unresolvedSnippet;
   TR::Snippet                   *_snippet;

   public:

   S390ImmSnippetInstruction(TR::InstOpCode::Mnemonic                     op,
                                TR::Node                            *n,
                                uint32_t                           imm,
                                TR::UnresolvedDataSnippet       *us,
                                TR::Snippet                         *s,
                                TR::CodeGenerator                   *cg)
      : S390ImmInstruction(op, n,imm, cg), _unresolvedSnippet(us),
        _snippet(s) {}

   S390ImmSnippetInstruction(
                              TR::InstOpCode::Mnemonic                     op,
                              TR::Node                            *n,
                              uint32_t                           imm,
                              TR::UnresolvedDataSnippet       *us,
                              TR::Snippet                         *s,
                              TR::Instruction *precedingInstruction,
                              TR::CodeGenerator                   *cg)
      : S390ImmInstruction(op, n, imm, precedingInstruction, cg),
        _unresolvedSnippet(us), _snippet(s) {}

   virtual char *description() { return "S390ImmSnippetInstruction"; }
   virtual Kind getKind() { return IsImmSnippet; }

   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *us)
      {
      return _unresolvedSnippet = us;
      }

   virtual uint8_t *generateBinaryEncoding();


   };

////////////////////////////////////////////////////////////////////////////////
// S390ImmSymInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390ImmSymInstruction : public TR::S390ImmInstruction
   {
   TR::SymbolReference *_symbolReference;

   public:

   S390ImmSymInstruction(TR::InstOpCode::Mnemonic      op,
                            TR::Node            *node,
                            uint32_t            imm,
                            TR::SymbolReference *sr,
                            TR::CodeGenerator   *cg)
      : S390ImmInstruction(op, node, imm, cg), _symbolReference(sr)
      {}

   S390ImmSymInstruction(TR::InstOpCode::Mnemonic      op,
                            TR::Node            *node,
                            uint32_t            imm,
                            TR::SymbolReference *sr,
                            TR::Instruction     *precedingInstruction,
                            TR::CodeGenerator   *cg)
      : S390ImmInstruction(op, node, imm, precedingInstruction, cg), _symbolReference(sr)
      {}

   S390ImmSymInstruction(TR::InstOpCode::Mnemonic                       op,
                            TR::Node                             *node,
                            uint32_t                             imm,
                            TR::SymbolReference                  *sr,
                            TR::RegisterDependencyConditions *cond,
                            TR::CodeGenerator                    *cg)
      : S390ImmInstruction(op, node, imm, cond, cg), _symbolReference(sr)
      {}

   S390ImmSymInstruction(TR::InstOpCode::Mnemonic                       op,
                            TR::Node                             *node,
                            uint32_t                             imm,
                            TR::SymbolReference                  *sr,
                            TR::RegisterDependencyConditions *cond,
                            TR::Instruction                      *precedingInstruction,
                            TR::CodeGenerator                    *cg)
     : S390ImmInstruction(op, node, imm, cond, precedingInstruction, cg), _symbolReference(sr)
     {}
   virtual char *description() { return "S390ImmSymInstruction"; }
   virtual Kind getKind() { return IsImmSym; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      {
      return _symbolReference = sr;
      }

   virtual uint8_t *generateBinaryEncoding();
   };


/**
 * S390RegInstruction Class Definition
 *
 *
*/

class S390RegInstruction : public TR::Instruction
   {
   protected:

   int8_t _firstConstant;

   /** Flag identifying pair or single */
   bool _targetPairFlag;

   TR::InstOpCode::S390BranchCondition _branchCondition;

   public:

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::Register      *reg,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(-1)
      {
         checkRegForGPR0Disable(op, reg);
         _targetPairFlag=reg->getRegisterPair()?true:false;
         if(!getOpCode().setsOperand1())
           useSourceRegister(reg);
         else
         useTargetRegister(reg);

         // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
         // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

         // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
         // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
         TR_ASSERT( (!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                  || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(-1), _targetPairFlag(false)
      {
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(-1), _targetPairFlag(false)
      {
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         int8_t           firstConstant,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg),  _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(firstConstant), _targetPairFlag(false)
      {
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         int8_t           firstConstant,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg),  _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(firstConstant), _targetPairFlag(false)
      {
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::Register      *reg,
                         TR::RegisterDependencyConditions * cond,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg), _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(-1)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;
      if (!getOpCode().setsOperand1())
         useSourceRegister(reg);
      else
         useTargetRegister(reg);

      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT((!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::Register      *reg,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(-1)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;

      if (!getOpCode().setsOperand1())
         useSourceRegister(reg);
      else
         useTargetRegister(reg);
      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT((!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::Register      *reg,
                         TR::RegisterDependencyConditions * cond,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _branchCondition(TR::InstOpCode::COND_NOP), _firstConstant(-1)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;
      useTargetRegister(reg);

      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT((!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::InstOpCode::S390BranchCondition brCond,
                         TR::Register      *reg,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _branchCondition(brCond), _firstConstant(0)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;
      if (!getOpCode().setsOperand1())
         useSourceRegister(reg);
      else
         useTargetRegister(reg);

      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT((!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::InstOpCode::S390BranchCondition brCond,
                         TR::Register      *reg,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _branchCondition(brCond), _firstConstant(0)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;
      if (!getOpCode().setsOperand1())
         useSourceRegister(reg);
      else
         useTargetRegister(reg);

      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT( (!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
               || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
               "OpCode [%s] %s use Register Pair for Target.\n",
               cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
               (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::InstOpCode::S390BranchCondition brCond,
                         TR::Register      *reg,
                         TR::RegisterDependencyConditions * cond,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, cg), _branchCondition(brCond), _firstConstant(0)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;
      if (!getOpCode().setsOperand1())
         useSourceRegister(reg);
      else
         useTargetRegister(reg);

      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT((!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   S390RegInstruction(TR::InstOpCode::Mnemonic    op,
                         TR::Node          *n,
                         TR::InstOpCode::S390BranchCondition brCond,
                         TR::Register      *reg,
                         TR::RegisterDependencyConditions * cond,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _branchCondition(brCond), _firstConstant(0)
      {
      checkRegForGPR0Disable(op, reg);
      _targetPairFlag=reg->getRegisterPair()?true:false;
      if (!getOpCode().setsOperand1())
         useSourceRegister(reg);
      else
         useTargetRegister(reg);

      // ShouldUseRegPairForTarget returns true for those instructions that require consequtive even-odd register pairs.
      // CanUseRegPairForTarget returns true for all the above instructions + STM/LTM (which can potentially take a register pair range).

      // So, if we find a register pair passed in, we check to make sure the instruction CAN use a register pair.
      // If we do not find a register pair, we check to make sure the instruction SHOULD NOT use a register pair.
      TR_ASSERT((!_targetPairFlag && !getOpCode().shouldUseRegPairForTarget())
                || (_targetPairFlag && getOpCode().canUseRegPairForTarget()) ,
                "OpCode [%s] %s use Register Pair for Target.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)",
                (_targetPairFlag)?"cannot":"should");
      }

   virtual char *description() { return "S390RegInstruction"; }
   virtual Kind getKind() { return IsReg; }
   virtual bool isRegInstruction() { return true; }

   void blockTargetRegister()
      {
      if (isTargetPair())
         {
         getFirstRegister()->block();
         getLastRegister()->block();
         }
      else
         getRegisterOperand(1)->block();
      }

   int8_t getFirstConstant() { return _firstConstant; }

   void unblockTargetRegister()
      {
      if (isTargetPair())
         {
         getFirstRegister()->unblock();
         getLastRegister()->unblock();
         }
      else
         getRegisterOperand(1)->unblock();
      }

   // 'r' Could be a pair or not
   //   TR::Register  *getTargetRegister()               { return (_targetRegSize != 0) ? tgtRegArrElem(0) : NULL; }
   //   TR::Register  *setTargetRegister(TR::Register *r) {assume0(_targetRegSize != 0); return (targetRegBase())[0] = r;}

   /**
    * Given that instruction expresses a register range (eg. LM or STM) the first and last register of the range
    * is fetched from first operand as a register pair or from the first two operands
    */
   TR::Register *getFirstRegister()                {return _targetPairFlag? getRegisterOperand(1)->getHighOrder() : getRegisterOperand(1);}
   TR::Register *getLastRegister()                 {return _targetPairFlag? getRegisterOperand(1)->getLowOrder():NULL;}

   bool isTargetPair() { return _targetPairFlag; }

   bool matchesTargetRegister(TR::Register* reg)
      {
      TR::RealRegister * realReg = NULL;
      TR::RealRegister * targetReg1 = NULL;
      TR::RealRegister * targetReg2 = NULL;
      bool enableHighWordRA = cg()->supportsHighWordFacility() && !cg()->comp()->getOption(TR_DisableHighWordRA) &&
                              reg->getKind() != TR_FPR && reg->getKind() != TR_VRF;

      if (enableHighWordRA && reg->getRealRegister())
         {
         realReg = toRealRegister(reg);
         if (realReg->isHighWordRegister())
            {
            // Highword aliasing low word regs
            realReg = realReg->getLowWordRegister();
            }
         }
      if (isTargetPair())
         {
         // if we are matching real regs
         if (enableHighWordRA && getFirstRegister()->getRealRegister())
            {
            // reg pairs do not use HPRs
            targetReg1 = (TR::RealRegister *)getFirstRegister();
            targetReg2 = toRealRegister(getLastRegister());
            return realReg == targetReg1 || realReg == targetReg2;
            }
         // if we are matching virt regs

         return reg == getFirstRegister() || reg == getLastRegister();
         }
      else if (getRegisterOperand(1))
         {
         // if we are matching real regs
         if (enableHighWordRA && getRegisterOperand(1)->getRealRegister())
            {
            targetReg1 = ((TR::RealRegister *)getRegisterOperand(1))->getLowWordRegister();
            return realReg == targetReg1;
            }

         if (reg->getRealRegister() && getRegisterOperand(1)->getRealRegister() &&
            TR::RealRegister::isAR(((TR::RealRegister *)getRegisterOperand(1))->getRegisterNumber()) &&
             GPRmatchesAR(
             toRealRegister(reg),
             (TR::RealRegister *)getRegisterOperand(1))
             )
            return true;

         // if we are matching virt regs
         return reg == getRegisterOperand(1);
         }
      return false;
      }

   bool GPRmatchesAR(TR::RealRegister* gprReg, TR::RealRegister* arReg)
      {
      TR_ASSERT(gprReg, "expecting valid gpr");
      TR_ASSERT(arReg,  "expecting valid ar");
      if (gprReg->getRegisterNumber()==arReg->getRegisterNumber()-TR::RealRegister::FirstAR+1)
         return true;
      return false;
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual void assignRegistersNoDependencies(TR_RegisterKinds kindToBeAssigned);

   virtual bool refsRegister(TR::Register *reg);

   TR::InstOpCode::S390BranchCondition getBranchCondition()  {return _branchCondition;}
   TR::InstOpCode::S390BranchCondition setBranchCondition(TR::InstOpCode::S390BranchCondition branchCondition) {return _branchCondition = branchCondition;}
   uint8_t getMask() {return getMaskForBranchCondition(getBranchCondition()) << 4;}
   };


////////////////////////////////////////////////////////////////////////////////
// S390RRInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RRInstruction : public TR::S390RegInstruction
   {
   flags32_t    _flagsRR;
   int8_t _secondConstant;

   public:

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, cg), _flagsRR(0), _secondConstant(-1)
      {
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, cg), _flagsRR(0), _secondConstant(-1)
      {
      checkRegForGPR0Disable(op, sreg);
      if (!getOpCode().setsOperand2())
         useSourceRegister(sreg);
      else
         useTargetRegister(sreg);
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::RegisterDependencyConditions * cond,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, cond, cg), _flagsRR(0), _secondConstant(-1)
      {
      checkRegForGPR0Disable(op, sreg);
      if (!getOpCode().setsOperand2())
         useSourceRegister(sreg);
      else
         useTargetRegister(sreg);
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, precedingInstruction, cg), _flagsRR(0), _secondConstant(-1)
      {
      checkRegForGPR0Disable(op, sreg);
      if (!getOpCode().setsOperand2())
         useSourceRegister(sreg);
      else
         useTargetRegister(sreg);
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        int8_t                secondConstant,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, precedingInstruction, cg), _flagsRR(0), _secondConstant(secondConstant)
      {
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        int8_t                secondConstant,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, cg), _flagsRR(0), _secondConstant(secondConstant)
      {
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t                firstConstant,
                        int8_t                secondConstant,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, firstConstant, precedingInstruction, cg), _flagsRR(0), _secondConstant(secondConstant)
      {
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t                firstConstant,
                        int8_t                secondConstant,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, firstConstant, cg), _flagsRR(0), _secondConstant(secondConstant)
      {
      }


   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t                firstConstant,
                        TR::Register           *sreg,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, firstConstant, precedingInstruction, cg), _flagsRR(0), _secondConstant(-1)
      {
      checkRegForGPR0Disable(op,sreg);
      if (!getOpCode().setsOperand2())
         useSourceRegister(sreg);
      else
         useTargetRegister(sreg);
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t                firstConstant,
                        TR::Register           *sreg,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, firstConstant, cg),  _flagsRR(0), _secondConstant(-1)
      {
      checkRegForGPR0Disable(op,sreg);
      if (!getOpCode().setsOperand2())
         useSourceRegister(sreg);
      else
         useTargetRegister(sreg);
      }


   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::RegisterDependencyConditions * cond,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, cond, precedingInstruction, cg), _flagsRR(0), _secondConstant(-1)
      {
      checkRegForGPR0Disable(op, sreg);
      if (!getOpCode().setsOperand2())
         useSourceRegister(sreg);
      else
         useTargetRegister(sreg);
      }

   S390RRInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RegInstruction(op, n, treg, precedingInstruction, cg), _flagsRR(0), _secondConstant(-1)
      {
      }

   virtual char *description() { return "S390RRInstruction"; }
   virtual Kind getKind() { return IsRR; }

   //   TR::Register  *getSourceRegister()                {return (_sourceRegSize!=0) ? (sourceRegBase())[0] : NULL;  }
   //   TR::Register  *setSourceRegister(TR::Register *sr) { (sourceRegBase())[0] = sr; return sr; }

   int8_t getSecondConstant()                       { return _secondConstant; }

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);

   // Flags getter & setters
   //
   // Note: these functions are not used anymore, will do addInstructionComment with the annotation instead.
   // Will leave _flagsRR for now until next change, but these flags won't be used in tr.dev.

/*   void setClobberEval() { _flagsRR.set(CLOBREG, true); }
   bool getClobberEval() { return _flagsRR.testAny(CLOBREG); }

   void setRegExch()     { _flagsRR.set(EXCHREG, true); }
   bool getRegExch()     { return _flagsRR.testAny(EXCHREG); }

   void setRegMove()     { _flagsRR.set(MOVEREG, true); }
   bool getRegMove()     { return _flagsRR.testAny(MOVEREG); }

   void setRegPair()     { _flagsRR.set(PAIRREG, true); }
   bool getRegPair()     { return _flagsRR.testAny(PAIRREG); }

   void setRegDep()      { _flagsRR.set(DEPSREG, true); }
   bool getRegDep()      { return _flagsRR.testAny(DEPSREG); }

   void setRegPrm()      { _flagsRR.set(PARMREG, true); }
   bool getRegPrm()      { return _flagsRR.testAny(PARMREG); }

   void setRegUsr()      { _flagsRR.set(USERREG, true); }
   bool getRegUsr()      { return _flagsRR.testAny(USERREG); }
*/
   };

////////////////////////////////////////////////////////////////////////////////
// S390TranslateInstruction Class Definition
// This currently includes TROO, TROT, TRTO, TRTT
////////////////////////////////////////////////////////////////////////////////
class S390TranslateInstruction : public TR::Instruction
   {
   private:

   #define tr_srcidx  2
   #define tr_tblidx  3
   #define tr_termidx 4
   #define tr_targidx 1

   uint8_t _mask; ///< Mask for ETF-2
   bool _isMaskPresent;
   public:

   /**
    * There is no version -without- dependency conditions since at a minimum
    * real register R0 and real register R1 will need to have been assigned to
    * _termCharRegister and _tableRegister respectively
    */
   S390TranslateInstruction(TR::InstOpCode::Mnemonic         op,
                               TR::Node               *n,
                               TR::Register           *treg,
                               TR::Register           *sreg,
                               TR::Register           *tableReg,
                               TR::Register           *termCharReg,
                               TR::RegisterDependencyConditions * cond,
                               TR::CodeGenerator      *cg)
      : TR::Instruction(op, n, cond, cg), _isMaskPresent(false)
      {
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      useSourceRegister(sreg);
      useSourceRegister(tableReg);
      useSourceRegister(termCharReg);
      }

   /** Add mask for ETF-2 TRxx instructions - RRE format */
   S390TranslateInstruction(TR::InstOpCode::Mnemonic         op,
                               TR::Node               *n,
                               TR::Register           *treg,
                               TR::Register           *sreg,
                               TR::Register           *tableReg,
                               TR::Register           *termCharReg,
                               TR::RegisterDependencyConditions * cond,
                               TR::CodeGenerator      *cg,
                               uint8_t                mask)
      : TR::Instruction(op, n, cond, cg), _mask(mask), _isMaskPresent(true)
      {
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      useSourceRegister(sreg);
      useSourceRegister(tableReg);
      useSourceRegister(termCharReg);
      }

   S390TranslateInstruction(TR::InstOpCode::Mnemonic         op,
                               TR::Node               *n,
                               TR::Register           *treg,
                               TR::Register           *sreg,
                               TR::Register           *tableReg,
                               TR::Register           *termCharReg,
                               TR::RegisterDependencyConditions * cond,
                               TR::Instruction        *precedingInstruction,
                               TR::CodeGenerator      *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _isMaskPresent(false)
      {
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      useSourceRegister(sreg);
      useSourceRegister(tableReg);
      useSourceRegister(termCharReg);
      }

   virtual char *description() { return "S390TranslateInstruction"; }
   virtual Kind getKind() { return IsRRE; }

   TR::Register *getTableRegister()                             {return getRegisterOperand(tr_tblidx); }
   //   TR::Register *setTableRegister(TR::Register* tableReg)        {return (sourceRegBase())[tr_tblidx] =  tableReg;}

   TR::Register *getTermCharRegister()                          {return getRegisterOperand(tr_termidx); }
   // TR::Register *setTermCharRegister(TR::Register* termCharReg)  {return (sourceRegBase())[tr_termidx] = termCharReg;}

   // TR::Register *getSourceRegister()                {return (sourceRegBase())[tr_srcidx]; }
   // TR::Register *setSourceRegister(TR::Register *sr) {return (sourceRegBase())[tr_srcidx] = sr;}

   //   TR::Register *getTargetRegister()                { return (targetRegBase())[tr_targidx];}
   //   TR::Register *setTargetRegister(TR::Register *sr) { return (targetRegBase())[tr_targidx]=sr;}

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);

   bool isMaskPresent()          {return _isMaskPresent;}
   uint8_t getMask()             {return _mask;}
   uint8_t setMask(uint8_t mask) {return _mask = mask;}

   };

////////////////////////////////////////////////////////////////////////////////
// S390RRFInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RRFInstruction : public TR::S390RRInstruction
   {
   bool _isMask3Present, _isMask4Present,  _isSourceReg2Present;
   bool _encodeAsRRD;
   uint8_t _mask3, _mask4;
   public:

   S390RRFInstruction(bool encodeAsRRD,
                        TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cg), _encodeAsRRD(encodeAsRRD),
        _isSourceReg2Present(true), _isMask3Present(false), _isMask4Present(false)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(bool encodeAsRRD,
                        TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, precedingInstruction, cg), _encodeAsRRD(encodeAsRRD),
       _isSourceReg2Present(true), _isMask3Present(false),_isMask4Present(false)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cg), _encodeAsRRD(false),
        _isSourceReg2Present(true), _isMask3Present(false), _isMask4Present(false)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic        op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        uint8_t                 mask,
                        bool                   isMask3,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cg), _encodeAsRRD(false),
        _isSourceReg2Present(false), _isMask3Present(isMask3),_isMask4Present(!isMask3)
      {
      if (isMask3)
         _mask3 = mask;
      else
         _mask4 = mask;
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic        op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        uint8_t                 mask,
                        bool                   isMask3,
                        TR::RegisterDependencyConditions * cond,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, cg), _encodeAsRRD(false),
        _isSourceReg2Present(false), _isMask3Present(isMask3),_isMask4Present(!isMask3)
      {
      if (isMask3)
         _mask3 = mask;
      else
         _mask4 = mask;
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic        op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        uint8_t                 mask,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cg), _encodeAsRRD(false),
        _isSourceReg2Present(true), _isMask3Present(false),_isMask4Present(true),_mask4(mask)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        TR::RegisterDependencyConditions * cond,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, cg), _encodeAsRRD(false),
        _isSourceReg2Present(true), _isMask3Present(false),_isMask4Present(false)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, precedingInstruction, cg), _encodeAsRRD(false),
       _isSourceReg2Present(true), _isMask3Present(false),_isMask4Present(false)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        TR::Register           *sreg2,
                        TR::RegisterDependencyConditions * cond,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, precedingInstruction, cg), _encodeAsRRD(false),
        _isSourceReg2Present(true), _isMask3Present(false),_isMask4Present(false)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRFInstruction(TR::InstOpCode::Mnemonic        op,
                        TR::Node               *n,
                        TR::Register           *treg,
                        TR::Register           *sreg,
                        uint8_t                 mask3,
                        uint8_t                 mask4,
                        TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cg),
        _mask3(mask3),_mask4(mask4), _encodeAsRRD(false),
        _isSourceReg2Present(false), _isMask3Present(true),_isMask4Present(true)
      {
      }

   virtual char *description() { return "S390RRFInstruction"; }
   virtual Kind getKind()
      {
      if (_encodeAsRRD)
         return IsRRD;

      if (_isMask3Present)
         {
         if (_isMask4Present)
            return IsRRF5;          // M3, M4, R1, R2
         else
            return IsRRF2;          // M3,  ., R1, R2
         }
      else if(_isMask4Present)
         {
         if (_isSourceReg2Present)
            return IsRRF3;          // R3, M4, R1, R2
         else
            return IsRRF4;          // .,  M4, R1, R2
         }
      else
         return IsRRF;              // R1, .,  R3, R2
      }

   // TR::Register *getSourceRegister2()                {return (_sourceRegSize==2) ? (sourceRegBase())[1] : NULL; }
   // TR::Register *setSourceRegister2(TR::Register *sr) {return (sourceRegBase())[1]=sr;}

   bool isSourceRegister2Present()                  {return _isSourceReg2Present;}

   bool isMask3Present()          {return _isMask3Present;}
   bool isMask4Present()          {return _isMask4Present;}
   bool encodeAsRRD()             {return _encodeAsRRD; }
   uint8_t getMask()              {TR_ASSERT(0, "RRF: getMask() is obsolete, use getMask3(..) or getMask4(..)\n"); return 0;}
   uint8_t setMask(uint8_t mask)  {TR_ASSERT(0, "RRF: setMask() is obsolete, use setMask3(..) or setMask4(..)\n"); return 0;}
   uint8_t getMask3()             {return _mask3;}
   uint8_t setMask3(uint8_t mask) {return _mask3 = mask;}
   uint8_t getMask4()             {return _mask4;}
   uint8_t setMask4(uint8_t mask) {return _mask4 = mask;}

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);
   };

////////////////////////////////////////////////////////////////////////////////
// S390RRRInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RRRInstruction : public TR::S390RRInstruction
   {
   #define rrr_srcidx 1
   public:

   S390RRRInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         TR::Register           *treg,
                         TR::Register           *sreg,
                         TR::Register           *sreg2,
                         TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cg)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRRInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         TR::Register           *treg,
                         TR::Register           *sreg,
                         TR::Register           *sreg2,
                         TR::RegisterDependencyConditions * cond,
                         TR::Instruction        *precedingInstruction,
                         TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, precedingInstruction, cg)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   S390RRRInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         TR::Register           *treg,
                         TR::Register           *sreg,
                         TR::Register           *sreg2,
                         TR::Instruction        *precedingInstruction,
                         TR::CodeGenerator      *cg)
      : S390RRInstruction(op, n, treg, sreg, precedingInstruction, cg)
      {
      if (!getOpCode().setsOperand3())
         useSourceRegister(sreg2);
      else
         useTargetRegister(sreg2);
      }

   virtual char *description() { return "S390RRRInstruction"; }
   virtual Kind getKind()
      {
      return IsRRR;
      }

   // TR::Register *getSourceRegister2()                {return (sourceRegBase())[rrr_srcidx];}
   // TR::Register *setSourceRegister2(TR::Register *sr) {return (sourceRegBase())[rrr_srcidx]=sr;}

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);

   };


////////////////////////////////////////////////////////////////////////////////
// S390RIInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RIInstruction : public TR::S390RegInstruction
   {
   union
      {
      int32_t _sourceImmediate;
      char*   _namedDataField;
      };

   bool _isImm;

   public:

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, cg), _isImm(false), _sourceImmediate(0) {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, precedingInstruction, cg), _isImm(false), _sourceImmediate(0) {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register  *treg,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, cg), _isImm(false), _sourceImmediate(0) {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register  *treg,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg), _isImm(false), _sourceImmediate(0) {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register  *treg,
                            char *data,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, cg), _namedDataField(data), _isImm(false) {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register  *treg,
                            char *data,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg), _namedDataField(data), _isImm(false)  {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register  *treg,
                            int32_t     imm,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, cg), _sourceImmediate(imm), _isImm(true) {};

   S390RIInstruction(TR::InstOpCode::Mnemonic op, TR::Node *n, TR::Register  *treg,
                            int32_t       imm,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg), _sourceImmediate(imm), _isImm(true) {};

   virtual char *description() { return "S390RIInstruction"; }
   virtual Kind getKind() { return IsRI; }

   int32_t getSourceImmediate()            {return _sourceImmediate;}
   int32_t setSourceImmediate(int32_t si)  {return _sourceImmediate = si;}

   char* getDataField()                    {return _namedDataField;}
   bool  isImm()                           {return _isImm;}

   virtual uint8_t *generateBinaryEncoding();
   };

////////////////////////////////////////////////////////////////////////////////
// S390RILInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RILInstruction : public TR::Instruction
   {
   uint32_t             _mask;
   void                *_targetPtr;
   TR::Snippet         *_targetSnippet;
   TR::Symbol          *_targetSymbol;
   TR::LabelSymbol     *_targetLabel;
   flags8_t             _flagsRIL;
   TR::SymbolReference *_symbolReference;

   /** _flagsRIL */
   enum
      {
      isLiteralPoolAddressFlag        = 0x01,
      isImmediateOffsetInBytesFlag    = 0x02
      };

   /** Use to store immediate value where the immediate is not used as a relative offset in address calculation.*/
   uint32_t  _sourceImmediate;

   public:

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         uint32_t           imm,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff),  _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(imm)
      {
      TR_ASSERT_FATAL(getOpCode().isExtendedImmediate(), "Incorrect S390RILInstruction constructor used.\n"
                                                       "%s expects an address as the immediate but this constructor is for integer immediates.", cg->getDebug()->getOpCodeName(&this->getOpCode()));
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         int32_t            imm,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff),  _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(imm)
      {
      TR_ASSERT_FATAL(getOpCode().isExtendedImmediate(), "Incorrect S390RILInstruction constructor used.\n"
                                                       "%s expects an address as the immediate but this constructor is for integer immediates.", cg->getDebug()->getOpCodeName(&this->getOpCode()));
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         void              *targetPtr,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff),  _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      TR_ASSERT_FATAL(!getOpCode().isExtendedImmediate(), "Incorrect S390RILInstruction constructor used.\n"
                                                        "%s expects an integer as the immediate but this constructor is for address immediates.", cg->getDebug()->getOpCodeName(&this->getOpCode()));
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }


   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         void                *targetPtr,
                         TR::SymbolReference *sr,
                         TR::CodeGenerator   *cg)
      : TR::Instruction(op, n, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         void                *targetPtr,
                         TR::SymbolReference *sr,
                         TR::Instruction     *precedingInstruction,
                         TR::CodeGenerator   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         uint32_t           imm,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(imm)
      {
      TR_ASSERT_FATAL(getOpCode().isExtendedImmediate(), "Incorrect S390RILInstruction constructor used.\n"
                                                       "%s expects an adddress as the immediate but this constructor is for integer immediates.", cg->getDebug()->getOpCodeName(&this->getOpCode()));
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         int32_t            imm,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(imm)
      {
      TR_ASSERT_FATAL(getOpCode().isExtendedImmediate(), "Incorrect S390RILInstruction constructor used.\n"
                                                       "%s expects an address as the immediate but this constructor is for integer immediates.", cg->getDebug()->getOpCodeName(&this->getOpCode()));
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         void              *targetPtr,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      TR_ASSERT_FATAL(!getOpCode().isExtendedImmediate(), "Incorrect S390RILInstruction constructor used.\n"
                                                        "%s expects an address as the immediate but this constructor is for integer immediates.", cg->getDebug()->getOpCodeName(&this->getOpCode()));
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   /** For TR::InstOpCode::BRCL */
   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Snippet       *ts,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(ts), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask,
                         void              *targetPtr,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(mask), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask,
                         TR::Snippet       *ts,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(ts), _targetSymbol(NULL), _flagsRIL(0), _mask(mask), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Snippet       *ts,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(NULL), _targetSnippet(ts), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask,
                         void              *targetPtr,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(mask), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      }

   /** For TR::InstOpCode::BRASL */
   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Snippet                      *ts,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _targetPtr(NULL), _targetSnippet(ts), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL) ,_symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Snippet                      *ts,
                         TR::RegisterDependencyConditions *cond,
                         TR::SymbolReference              *sr,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _targetPtr(NULL), _targetSnippet(ts), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask,
                         TR::Snippet                      *ts,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _targetPtr(NULL), _targetSnippet(ts), _targetSymbol(NULL), _flagsRIL(0), _mask(mask), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         void                             *targetPtr,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _targetPtr(targetPtr), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Symbol                       *sym,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Symbol        *sym,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      useTargetRegister(treg);
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Symbol          *sym,
                         TR::SymbolReference *sr,
                         TR::CodeGenerator   *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Symbol                       *sym,
                         TR::SymbolReference              *sr,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::Symbol          *sym,
                         TR::SymbolReference *sr,
                         TR::Instruction     *precedingInstruction,
                         TR::CodeGenerator   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(0xffffffff), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   /** For PFDRL */
   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask,
                         TR::Symbol          *sym,
                         TR::SymbolReference *sr,
                         TR::CodeGenerator   *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(mask), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t mask,
                         TR::Symbol          *sym,
                         TR::SymbolReference *sr,
                         TR::Instruction     *precedingInstruction,
                         TR::CodeGenerator   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(sym), _flagsRIL(0), _mask(mask), _targetLabel(NULL), _symbolReference(sr), _sourceImmediate(0)
      {
      if (sym->isLabel())
         _targetLabel = sym->castToLabelSymbol();
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::LabelSymbol   *label,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(label), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   S390RILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Register  *treg,
                         TR::LabelSymbol   *label,
                         TR::Instruction   *precedingInstruction,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _targetPtr(NULL), _targetSnippet(NULL), _targetSymbol(NULL), _flagsRIL(0), _mask(0xffffffff), _targetLabel(label), _symbolReference(NULL), _sourceImmediate(0)
      {
      checkRegForGPR0Disable(op,treg);
      if (!getOpCode().setsOperand1())
         useSourceRegister(treg);
      else
         useTargetRegister(treg);
      }

   virtual char *description() { return "S390RILInstruction"; }
   virtual Kind getKind() { return IsRIL; }

   bool isLiteralPoolAddress() {return _flagsRIL.testAny(isLiteralPoolAddressFlag); }
   void setIsLiteralPoolAddress() { _flagsRIL.set(isLiteralPoolAddressFlag);}

   bool isImmediateOffsetInBytes() {return _flagsRIL.testAny(isImmediateOffsetInBytesFlag); }
   void setIsImmediateOffsetInBytes() { _flagsRIL.set(isImmediateOffsetInBytesFlag);}

   uintptrj_t getTargetPtr()
      { return  reinterpret_cast<uintptrj_t>(_targetPtr); }
   uintptrj_t setTargetPtr(uintptrj_t tp)
      { TR_ASSERT(!isImmediateOffsetInBytes(), "Immediate Offset already set on RIL instruction."); _targetPtr = reinterpret_cast<void*>(tp); return tp; }
   uintptrj_t getImmediateOffsetInBytes()
      { TR_ASSERT(isImmediateOffsetInBytes(), "Immediate Offset not set for RIL Instruction."); return reinterpret_cast<uintptrj_t>(_targetPtr); }
   uintptrj_t setImmediateOffsetInBytes(uintptrj_t tp)
      { setIsImmediateOffsetInBytes(); _targetPtr = reinterpret_cast<void*>(tp); return tp; }
   TR::Snippet *getTargetSnippet()
      { return _targetSnippet; }
   TR::Snippet *setTargetSnippet(TR::Snippet *ts)
      { return _targetSnippet = ts; }
   TR::Symbol *getTargetSymbol()
      { return _targetSymbol; }
   TR::LabelSymbol *getTargetLabel()
      { return _targetLabel; }
   TR::Symbol *setTargetSymbol(TR::Symbol *sym)
      { return _targetSymbol = sym; }
   uint32_t   getMask()
      { return _mask; }
   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *sr)
      { return _symbolReference = sr; }

   //   TR::Register  *getTargetRegister()               { return (_targetRegSize!=0) ? (targetRegBase())[0] : NULL;}
   //   TR::Register  *setTargetRegister(TR::Register *r) { assume0(_targetRegSize!=0);  (targetRegBase())[0] = r; return r; }
   bool matchesTargetRegister(TR::Register* reg)
      {
      TR::RealRegister * realReg = NULL;
      TR::RealRegister * targetReg = NULL;

      bool enableHighWordRA = cg()->supportsHighWordFacility() && !cg()->comp()->getOption(TR_DisableHighWordRA) &&
                              reg->getKind() != TR_FPR && reg->getKind() != TR_VRF;

      if (enableHighWordRA && reg->getRealRegister())
         {
         realReg = (TR::RealRegister *)reg;
         if (realReg->isHighWordRegister())
            {
            // Highword aliasing low word regs
            realReg = realReg->getLowWordRegister();
            }
         }

      // if we are matching real regs
      if (enableHighWordRA && getRegisterOperand(1) && getRegisterOperand(1)->getRealRegister())
         {
         targetReg = ((TR::RealRegister *)getRegisterOperand(1))->getLowWordRegister();
         return realReg == targetReg;
         }
      // if we are matching virt regs
      return reg == getRegisterOperand(1);
      }

   bool refsRegister(TR::Register *reg);

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t *generateBinaryEncoding();

   // Get value from extended immediate instructions
   int32_t getSourceImmediate()            {return _sourceImmediate;}
   int32_t setSourceImmediate(int32_t si)  {return _sourceImmediate = si;}

   int32_t adjustCallOffsetWithTrampoline(int32_t offset, uint8_t * currentInst);
   };

////////////////////////////////////////////////////////////////////////////////
// S390RSInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RSInstruction : public TR::S390RegInstruction
   {
   protected:

   int16_t                 _sourceImmediate;
   int8_t                  _maskImmediate;
   bool                    _hasMask;
   int8_t                  _idx;
   Kind                    _kind;


   public:

   /**
    * RS instruction with  R1,D2(0) format
    * e.g. SLL   R1,12(0) - shifting R1 with constant value 12 and the value is < 4K
    * in this case, base register doesn't need to be set
    */
   S390RSInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        uint32_t          imm,
                        TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, cg), _sourceImmediate(imm), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {
      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
               "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      };

   S390RSInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        uint32_t          imm,
                        TR::Instruction   *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg), _kind(IsRS),
             _sourceImmediate(imm), _maskImmediate(0), _idx(-1), _hasMask(false)
      {
      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
               "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      };

   S390RSInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        uint32_t          imm,
                        TR::RegisterDependencyConditions *_conditions,
                        TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, _conditions, cg), _sourceImmediate(imm), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)

      {
      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
               "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      };

   S390RSInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        uint32_t          imm,
                        TR::RegisterDependencyConditions *_conditions,
                        TR::Instruction   *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, _conditions, precedingInstruction, cg), _kind(IsRS),
             _sourceImmediate(imm), _maskImmediate(0), _idx(-1), _hasMask(false)
      {
      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
               "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      };

   // RS instruction with  R1,D2(B2) format
   S390RSInstruction(TR::InstOpCode::Mnemonic           op,
                        TR::Node                 *n,
                        TR::Register             *treg,
                        TR::MemoryReference  *mf,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, treg, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);

      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
               "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      }

   S390RSInstruction(TR::InstOpCode::Mnemonic           op,
                        TR::Node                 *n,
                        TR::Register             *treg,
                        TR::MemoryReference  *mf,
                        TR::Instruction          *precedingInstruction,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg),
             _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);

      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
                "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      }

   /** Used for ICM instruction */
   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::Register             *treg,
                        uint32_t                 mask,
                        TR::MemoryReference      *mf,
                        TR::Instruction          *precedingInstruction,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg),
             _sourceImmediate(0), _maskImmediate(mask), _idx(-1), _hasMask(true), _kind(IsRS)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);

      // 1 Register is specified - If it is not a register pair, make sure instruction doesn't take a range (i.e. STM).
      TR_ASSERT(treg->getRegisterPair() || !getOpCode().usesRegRangeForTarget(),
               "OpCode [%s] requires a register range.\n",
                cg->getDebug()? cg->getDebug()->getOpCodeName(&this->getOpCode()) : "(unknown)");
      }

   /** Used for ICM instruction */
   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::Register             *treg,
                        uint32_t                 mask,
                        TR::MemoryReference      *mf,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, treg, cg),
             _sourceImmediate(0), _maskImmediate(mask), _idx(-1), _hasMask(true), _kind(IsRS)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }


   /**
    * Used for range of registers STM R1,R2 (R2!=R1+1)
    * also for SLLG, SLAG, SRLG for 64bit codegen
    */
   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::Register             *freg,
                        TR::Register             *lreg,
                        TR::MemoryReference      *mf,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, freg, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {
      if (getOpCode().setsOperand2())
         useTargetRegister(lreg);
      else
         useSourceRegister(lreg);
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);

      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::Register             *freg,
                        TR::Register             *lreg,
                        TR::MemoryReference      *mf,
                        TR::Instruction          *precedingInstruction,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, freg, precedingInstruction, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {

      if (getOpCode().setsOperand2())
         useTargetRegister(lreg);
      else
         useSourceRegister(lreg);

      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);

      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   /** Used specifically for consecutive even-odd pairs (STM,LM) */
   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::RegisterPair         *regp,
                        TR::MemoryReference      *mf,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, regp, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
     {
     useSourceMemoryReference(mf);
     setupThrowsImplicitNullPointerException(n,mf);
     if (mf->getUnresolvedSnippet() != NULL)
        (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
     }

   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::RegisterPair         *regp,
                        TR::MemoryReference      *mf,
                        TR::Instruction          *precedingInstruction,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, regp, precedingInstruction, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   /** Used specifically for source/target consecutive even-odd pairs (CLCLE,MVCLE)*/
   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::RegisterPair         *regp,
                        TR::RegisterPair         *regp2,
                        TR::MemoryReference      *mf,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, regp, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)

     {
     if (getOpCode().setsOperand2())
        useTargetRegister(regp2);
     else
        useSourceRegister(regp2);
     useSourceMemoryReference(mf);
     setupThrowsImplicitNullPointerException(n,mf);
     if (mf->getUnresolvedSnippet() != NULL)
        mf->getUnresolvedSnippet()->setDataReferenceInstruction(this);
     }

   S390RSInstruction(TR::InstOpCode::Mnemonic op,
                        TR::Node                 *n,
                        TR::RegisterPair         *regp,
                        TR::RegisterPair         *regp2,
                        TR::MemoryReference      *mf,
                        TR::Instruction          *precedingInstruction,
                        TR::CodeGenerator        *cg)
           : S390RegInstruction(op, n, regp, precedingInstruction, cg), _sourceImmediate(0), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
      {
      if (getOpCode().setsOperand2())
         useTargetRegister(regp2);
      else
         useSourceRegister(regp2);
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   /** For 64bit code-gen SLLG, SLAG, SRLG */
   S390RSInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Register      *sreg,
                        uint32_t          imm,
                        TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, cg), _sourceImmediate(imm), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
        {
        if (getOpCode().setsOperand2())
           useTargetRegister(sreg);
        else
           useSourceRegister(sreg);
        };

   S390RSInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Register      *sreg,
                        uint32_t          imm,
                        TR::Instruction   *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390RegInstruction(op, n, treg, precedingInstruction, cg),
             _sourceImmediate(imm), _maskImmediate(0), _idx(-1), _hasMask(false), _kind(IsRS)
        {
        if (getOpCode().setsOperand2())
           useTargetRegister(sreg);
        else
           useSourceRegister(sreg);
        };

   virtual char *description() { return "S390RSInstruction"; }
   virtual Kind getKind()                   {return _kind;}
   virtual void setKind(Kind kind)          { _kind = kind; }
   uint32_t getSourceImmediate()            {return _sourceImmediate;}
   uint32_t setSourceImmediate(uint32_t si) {return _sourceImmediate = si;}
   uint32_t getMaskImmediate()              {return _maskImmediate;}
   uint32_t setMaskImmediate(uint32_t mi)   {return _maskImmediate = mi;}
   bool     hasMask()                       {return _hasMask;}

   TR::Register* getFirstRegister() { return isTargetPair()? S390RegInstruction::getFirstRegister() : getRegisterOperand(1); }
   TR::Register* getLastRegister()  { return isTargetPair()? S390RegInstruction::getLastRegister()  : getRegisterOperand(2); }

   TR::Register* getSecondRegister() {return getRegisterOperand(2); }


   virtual TR::MemoryReference* getMemoryReference()  { return (_sourceMemSize!=0) ? (sourceMemBase())[0] : NULL; }

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   void generateAdditionalSourceRegisters(TR::Register *, TR::Register *);
   };


////////////////////////////////////////////////////////////////////////////////
// S390RSWithImplicitPairStoresInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////

/**
 * @todo complex source <-> target copies
 */
class S390RSWithImplicitPairStoresInstruction : public TR::S390RSInstruction
   {
   public:


   /** Used specifically for source/target consecutive even-odd pairs (CLCLE,MVCLE)*/
   S390RSWithImplicitPairStoresInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::RegisterPair  *regp,
                        TR::RegisterPair  *regp2,
                        TR::MemoryReference *mf,
                        TR::CodeGenerator *cg)
           : S390RSInstruction(op, n, regp, regp2, mf, cg)
      {
      }

   S390RSWithImplicitPairStoresInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::RegisterPair  *regp,
                        TR::RegisterPair  *regp2,
                        TR::MemoryReference *mf,
                        TR::Instruction   *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390RSInstruction(op, n, regp, regp2, mf, precedingInstruction, cg)
      {
      }

   void swap_operands(int i,int j) { void *op1 = _operands[i]; _operands[i]=_operands[j]; _operands[j]=op1; }


   };


////////////////////////////////////////////////////////////////////////////////
// S390RSYInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RSYInstruction : public TR::S390RSInstruction
   {
   public:

   S390RSYInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        uint32_t          mask,
                        TR::MemoryReference *mf,
                        TR::CodeGenerator *cg)
      : S390RSInstruction(op, n, treg, mask, mf, cg)
      {
      }

   S390RSYInstruction(TR::InstOpCode::Mnemonic    op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        uint32_t          mask,
                        TR::MemoryReference *mf,
                        TR::Instruction   *preced,
                        TR::CodeGenerator *cg)
      : S390RSInstruction(op, n, treg, mask, mf, preced, cg)
      {
      }

   virtual char *description() { return "S390RSYInstruction"; }
   virtual Kind getKind() { return IsRSY; }

   //virtual uint8_t *generateBinaryEncoding();

   //virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };


////////////////////////////////////////////////////////////////////////////////
// S390RRSInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RRSInstruction : public TR::S390RRInstruction
   {
   TR::MemoryReference * _branchDestination;
   TR::InstOpCode::S390BranchCondition _branchCondition;

   Kind _kind;

   public:

   /** Construct a new RRS instruction with no preceding instruction */
   S390RRSInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         TR::MemoryReference * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::CodeGenerator * cg)
           : S390RRInstruction(op, n, targetRegister, sourceRegister, cg),
             _kind(IsRRS),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition)
   {
   setupThrowsImplicitNullPointerException(n,branchDestination);
   }

   /** Construct a new RRS instruction with preceding instruction */
   S390RRSInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         TR::MemoryReference * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator * cg)
           : S390RRInstruction(op, n, targetRegister, sourceRegister, precedingInstruction, cg),
             _kind(IsRRS),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition)
   {
   setupThrowsImplicitNullPointerException(n,branchDestination);
   }

   virtual char *description() { return "S390RRSInstruction"; }
   virtual Kind getKind() { return IsRRS; }

   /** Get branch condition information */
   virtual TR::InstOpCode::S390BranchCondition getBranchCondition()  {return _branchCondition;}
   virtual TR::InstOpCode::S390BranchCondition setBranchCondition(TR::InstOpCode::S390BranchCondition branchCondition) {return _branchCondition = branchCondition;}
   uint8_t getMask() {return getMaskForBranchCondition(getBranchCondition()) << 4;}

   /** Get branch destination information */
   virtual TR::MemoryReference * getBranchDestinationLabel() { return _branchDestination; }

   virtual uint8_t * generateBinaryEncoding();
   };


////////////////////////////////////////////////////////////////////////////////
// S390RREInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RREInstruction : public TR::S390RRInstruction
   {
   public:

   S390RREInstruction(TR::InstOpCode::Mnemonic  op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::CodeGenerator *cg)
      : S390RRInstruction(op, n, treg, cg)
      {
      }

   S390RREInstruction(TR::InstOpCode::Mnemonic  op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Register      *sreg,
                        TR::CodeGenerator *cg)
      : S390RRInstruction(op, n, treg, sreg, cg)
      {
      }

   S390RREInstruction(TR::InstOpCode::Mnemonic  op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Register      *sreg,
                        TR::RegisterDependencyConditions * cond,
                        TR::CodeGenerator *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, cg)
      {
      }

   S390RREInstruction(TR::InstOpCode::Mnemonic  op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Register      *sreg,
                        TR::RegisterDependencyConditions * cond,
                        TR::Instruction   *preced,
                        TR::CodeGenerator *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, preced, cg)
      {
      }

   S390RREInstruction(TR::InstOpCode::Mnemonic  op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Register      *sreg,
                        TR::Instruction   *preced,
                        TR::CodeGenerator *cg)
      : S390RRInstruction(op, n, treg, sreg, preced, cg)
      {
      }

   S390RREInstruction(TR::InstOpCode::Mnemonic  op,
                        TR::Node          *n,
                        TR::Register      *treg,
                        TR::Instruction   *preced,
                        TR::CodeGenerator *cg)
      : S390RRInstruction(op, n, treg,  preced, cg)
      {
      }

   virtual char *description() { return "S390RREInstruction"; }
   virtual Kind getKind() { return IsRRE; }

   //virtual uint8_t *generateBinaryEncoding();

   //virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

////////////////////////////////////////////////////////////////////////////////
// S390IEInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390IEInstruction : public TR::Instruction
   {
   uint8_t                  _immediate1;
   uint8_t                  _immediate2;
   public:

   S390IEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node *n,
                         uint8_t im1,
                         uint8_t im2,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _immediate1(im1), _immediate2(im2)
           {

           }

   S390IEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         uint8_t im1,
                         uint8_t im2,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _immediate1(im1), _immediate2(im2)
           {

           }

   uint8_t getImmediateField1() { return _immediate1; }
   uint8_t getImmediateField2() { return _immediate2; }

   virtual char *description() { return "S390IEInstruction"; }
   virtual Kind getKind() { return IsIE; }

   virtual uint8_t *generateBinaryEncoding();
   };

////////////////////////////////////////////////////////////////////////////////
// S390RIEInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
/**
 * There are 3 forms of RIE instructions.  one which compares Reg-Reg, one
 * which compares Reg-Imm8, and one whicn compares Reg-Imm16.
 *
 * Note that there is no distinction between source and target register for
 * a compare, this is just following convention.
 */
class S390RIEInstruction : public TR::S390RegInstruction
   {

   TR::InstOpCode::S390BranchCondition _branchCondition;

   // in the cases where we have an immediate to compare against a register, it
   // will be in this member
   int8_t  _sourceImmediate8;
   int8_t  _sourceImmediate8One;
   int8_t  _sourceImmediate8Two;

   // in the case where the caller has a 16-bit immediate to compare against,
   // it will be in this member
   int16_t _sourceImmediate16;

   /**
    * If the user only knows that they are branching to a label, we'll store
    * that in this member
    */
   TR::LabelSymbol * _branchDestination;

   Kind _kind;

   public:
   typedef enum RIEForms { RIE_RR, RIE_RI8, RIE_RI16A, RIE_RI16G, RIE_RRI16, RIE_IMM } RIEForm;


   private:
   /** This member will determine which form  of RIE you have based on how we get constructed */
   RIEForm _instructionFormat;
   TR::InstOpCode _extendedHighWordOpCode; ///< for zG highword rotate instructions

   public:


   /** Construct a Reg-Reg form RIE with no preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         TR::LabelSymbol * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, cg),
             _kind(IsRIE),
             _instructionFormat(RIE_RR),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      useSourceRegister(sourceRegister);
      }

   /** Construct a Reg-Reg form RIE with preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         TR::LabelSymbol * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, precedingInstruction, cg),
             _kind(IsRIE),
             _instructionFormat(RIE_RR),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      useSourceRegister(sourceRegister);
      }

   /** Construct a Reg-Imm8 form RIE with no preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         int8_t sourceImmediate,
                         TR::LabelSymbol * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, cg),
             _kind(IsRIE),
             _instructionFormat(RIE_RI8),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition),
             _sourceImmediate8(sourceImmediate)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      }


   /** Construct a Reg-Imm8 form RIE with preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         int8_t sourceImmediate,
                         TR::LabelSymbol * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, precedingInstruction, cg),
              _kind(IsRIE),
              _instructionFormat(RIE_RI8),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition),
             _sourceImmediate8(sourceImmediate)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      }

   /** Construct a Reg-Reg-Imm8-Imm8-Imm8 form RIE with no preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         int8_t sourceImmediateOne,
                         int8_t sourceImmediateTwo,
                         int8_t sourceImmediate,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, cg),
             _kind(IsRIE),
             _instructionFormat(RIE_IMM),
             _extendedHighWordOpCode(TR::InstOpCode::BAD),
             _branchDestination(0),
             _branchCondition(TR::InstOpCode::COND_NOPR),
             _sourceImmediate8(sourceImmediate),
             _sourceImmediate8One(sourceImmediateOne),
             _sourceImmediate8Two(sourceImmediateTwo)
      {
      useSourceRegister(sourceRegister);
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      if (op == TR::InstOpCode::RISBG || op == TR::InstOpCode::RISBGN)
         {
         TR_ASSERT((sourceImmediateOne & 0xC0) == 0, "Bits 0-1 in the I3 field for %s must be 0", getOpCodeValue() == TR::InstOpCode::RISBG ? "RISBG" : "RISBGN");
         TR_ASSERT((sourceImmediateTwo & 0x40) == 0, "Bit 1 in the I4 field for %s must be 0", getOpCodeValue() == TR::InstOpCode::RISBG ? "RISBG" : "RISBGN");

         if (cg->supportsHighWordFacility() && !cg->comp()->getOption(TR_DisableHighWordRA) &&
             sourceImmediateTwo & 0x80)
            {
            (S390RegInstruction::getRegisterOperand(1))->setIs64BitReg(true);
            }
         }
      }


   /** Construct a Reg-Reg-Imm8-Imm8-Imm8 form RIE with preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         int8_t sourceImmediateOne,
                         int8_t sourceImmediateTwo,
                         int8_t sourceImmediate,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, precedingInstruction, cg),
             _kind(IsRIE),
             _instructionFormat(RIE_IMM),
             _extendedHighWordOpCode(TR::InstOpCode::BAD),
             _branchDestination(0),
             _branchCondition(TR::InstOpCode::COND_NOPR),
             _sourceImmediate8(sourceImmediate),
             _sourceImmediate8One(sourceImmediateOne),
             _sourceImmediate8Two(sourceImmediateTwo)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      useSourceRegister(sourceRegister);
      if ((op == TR::InstOpCode::RISBG || op == TR::InstOpCode::RISBGN) &&
          cg->supportsHighWordFacility() && !cg->comp()->getOption(TR_DisableHighWordRA) &&
          sourceImmediateTwo & 0x80) // if the zero bit is set, target reg will be 64bit
         {
         (S390RegInstruction::getRegisterOperand(1))->setIs64BitReg(true);
         }
      }

   /** Construct a Reg-Imm16 form RIE with no preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         int16_t sourceImmediate,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, cg),
              _kind(IsRIE),
              _instructionFormat(RIE_RI16A),
             _branchDestination(0),
             _branchCondition(branchCondition),
             _sourceImmediate16(sourceImmediate)
      {
      // Note that _targetRegister is registered for use via the S390RegInstruction constructor call
      if (op == TR::InstOpCode::LOCGHI || op == TR::InstOpCode::LOCHI || op == TR::InstOpCode::LOCHHI)
         _instructionFormat = RIE_RI16G;
      }

   /** Construct a Reg-Imm16 form RIE with preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         int16_t sourceImmediate,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator * cg)
           : S390RegInstruction(op, n, targetRegister, precedingInstruction, cg),
              _kind(IsRIE),
              _instructionFormat(RIE_RI16A),
             _branchDestination(0),
             _branchCondition(branchCondition),
             _sourceImmediate16(sourceImmediate)
      {
      if (isTrap())
         TR_ASSERT((sourceImmediate >> 8) != 0x00B9, "ASSERTION FAILURE: should not generate RIE trap instruction with imm field = B9XX!\n");

      // Note that _targetRegister is registered for use via the S390RegInstruction constructor call
      if (op == TR::InstOpCode::LOCGHI || op == TR::InstOpCode::LOCHI || op == TR::InstOpCode::LOCHHI)
         _instructionFormat = RIE_RI16G;
      }

      /** Construct a Reg-Reg-Imm16 form RIE with preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         int16_t sourceImmediate,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator * cg)
      : S390RegInstruction(op, n, targetRegister, precedingInstruction, cg),
        _kind(IsRIE),
        _instructionFormat(RIE_RRI16),
        _extendedHighWordOpCode(TR::InstOpCode::BAD),
        _branchDestination(0),
        _branchCondition(TR::InstOpCode::COND_NOPR),
        _sourceImmediate8(0),
        _sourceImmediate16(sourceImmediate)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      useSourceRegister(sourceRegister);
      }

   /** Construct a Reg-Reg-Imm16 form RIE with no preceding instruction */
   S390RIEInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         TR::Register * sourceRegister,
                         int16_t sourceImmediate,
                         TR::CodeGenerator * cg)
      : S390RegInstruction(op, n, targetRegister, cg),
        _kind(IsRIE),
        _instructionFormat(RIE_RRI16),
        _extendedHighWordOpCode(TR::InstOpCode::BAD),
        _branchDestination(0),
        _branchCondition(TR::InstOpCode::COND_NOPR),
        _sourceImmediate8(0),
        _sourceImmediate16(sourceImmediate)
      {
      // note that _targetRegister is registered for use via the
      // S390RegInstruction constructor call
      useSourceRegister(sourceRegister);
      }

   uint8_t *splitIntoCompareAndLongBranch(void);
   void splitIntoCompareAndBranch(TR::Instruction *insertBranchAfterThis);


   /** Determine the form of this instruction */
   virtual char *description() { return "S390RIEInstruction"; }
   virtual Kind getKind() { return IsRIE; }
   virtual RIEForm getRieForm() { return _instructionFormat; }

   /** For zGryphon highword rotate instructions, extended mnemonics */
   virtual void setExtendedHighWordOpCode(TR::InstOpCode op) {_extendedHighWordOpCode = op;}
   virtual TR::InstOpCode& getExtendedHighWordOpCode() { return _extendedHighWordOpCode;}

   // get register information
   //virtual TR::Register * getSourceRegister() { return (_sourceRegSize!=0) ? (sourceRegBase())[0] : NULL; }
   //   virtual TR::Register * getTargetRegister() { return S390RegInstruction::getTargetRegister(); }

   // set register informtion
   //   virtual void setSourceRegister(TR::Register * reg) { assume0(_sourceRegSize==1); (sourceRegBase())[0] = reg; }

   /** Get immediate value information */
   int8_t getSourceImmediate8() { return _sourceImmediate8; }
   int8_t getSourceImmediate8One() { return _sourceImmediate8One; }
   int8_t getSourceImmediate8Two() { return _sourceImmediate8Two; }
   int16_t getSourceImmediate16() { return _sourceImmediate16; }

   /** Set immediate value information */
   void setSourceImmediate8(int8_t i) {_sourceImmediate8 = i;}
   void setSourceImmediate8One(int8_t i) {_sourceImmediate8One = i;}
   void setSourceImmediate8Two(int8_t i) {_sourceImmediate8Two = i;}
   void setSourceImmediate16(int16_t i) {_sourceImmediate16 = i;}

   /** Get branch condition information */
   virtual TR::InstOpCode::S390BranchCondition getBranchCondition()  {return _branchCondition;}
   virtual TR::InstOpCode::S390BranchCondition setBranchCondition(TR::InstOpCode::S390BranchCondition branchCondition) {return _branchCondition = branchCondition;}
   uint8_t getMask() {return getMaskForBranchCondition(getBranchCondition()) << 4;}

   /** Get branch destination information */
   virtual TR::LabelSymbol * getBranchDestinationLabel() { return _branchDestination; }
   void setBranchDestinationLabel(TR::LabelSymbol *l) { _branchDestination = l; }

   virtual uint8_t * generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   virtual TR::LabelSymbol *getLabelSymbol()
      {
      return _branchDestination;
      }

   virtual TR::Snippet *getSnippetForGC()
      {
      if (getLabelSymbol() != NULL)
         return getLabelSymbol()->getSnippet();
      return NULL;
      }

   };


////////////////////////////////////////////////////////////////////////////////
// S390SMIInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390SMIInstruction : public TR::Instruction
   {
   private:
   uint8_t _mask;
   TR::LabelSymbol * _branchInstruction;

   public:

   S390SMIInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node                 *n,
                         uint8_t                  mask,
                         TR::LabelSymbol          *inst,
                         TR::MemoryReference      *mf3,
                         TR::CodeGenerator        *cg)
      : TR::Instruction(op, n, cg), _mask(mask),  _branchInstruction(inst)
      {
      useSourceMemoryReference(mf3);
      }

   S390SMIInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node                 *n,
                         uint8_t                  mask,
                         TR::LabelSymbol          *inst,
                         TR::MemoryReference      *mf3,
                         TR::Instruction          *precedingInstruction,
                         TR::CodeGenerator        *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _mask(mask), _branchInstruction(inst)
      {
      useSourceMemoryReference(mf3);
      }

   S390SMIInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node                         * n,
                         uint8_t                          mask,
                         TR::LabelSymbol                  *inst,
                         TR::MemoryReference              *mf3,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, cg), _mask(mask), _branchInstruction(inst)
      {
      useSourceMemoryReference(mf3);
      }

   S390SMIInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node                         *n,
                         uint8_t                          mask,
                         TR::LabelSymbol                  *inst,
                         TR::MemoryReference              *mf3,
                         TR::RegisterDependencyConditions *cond,
                         TR::Instruction                  *precedingInstruction,
                         TR::CodeGenerator                *cg)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _mask(mask), _branchInstruction(inst)
      {
      useSourceMemoryReference(mf3);
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   uint8_t getMask()             {return _mask;}
   uint8_t setMask(uint8_t mask) {return _mask = mask;}

   virtual char *description() { return "S390SMIInstruction"; }
   virtual Kind getKind() { return IsSMI; }
   virtual TR::LabelSymbol *getLabelSymbol()
      {
      return _branchInstruction;
      }

   virtual TR::MemoryReference* getMemoryReference() { return (_sourceMemSize!=0) ? (sourceMemBase())[0] : NULL; }
   };

////////////////////////////////////////////////////////////////////////////////
// S390MIIInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390MIIInstruction : public TR::Instruction
   {
   private:
   uint8_t _mask;
   TR::LabelSymbol * _branchInstruction;
   TR::SymbolReference * _callSymRef;
   public:

   S390MIIInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         uint8_t                mask,
                         TR::LabelSymbol          *inst2,
                         TR::SymbolReference         *inst3,
                         TR::CodeGenerator       *cg)
   : TR::Instruction(op, n, cg), _mask(mask), _callSymRef(inst3), _branchInstruction(inst2)
      {
      }
   S390MIIInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         uint8_t                mask,
                         TR::LabelSymbol          *inst2,
                         TR::SymbolReference         *inst3,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator       *cg)
   : TR::Instruction(op, n, precedingInstruction, cg), _mask(mask), _callSymRef(inst3), _branchInstruction(inst2)
      {
      }
   S390MIIInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         uint8_t                mask,
                         TR::LabelSymbol          *inst2,
                         TR::SymbolReference         *inst3,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator       *cg)
   : TR::Instruction(op, n, cond, cg), _mask(mask), _callSymRef(inst3), _branchInstruction(inst2)
      {
      }
   S390MIIInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         uint8_t                mask,
                         TR::LabelSymbol          *inst2,
                         TR::SymbolReference         *inst3,
                         TR::RegisterDependencyConditions *cond,
                         TR::Instruction        *precedingInstruction,
                         TR::CodeGenerator       *cg)
   : TR::Instruction(op, n, cond, precedingInstruction, cg), _mask(mask), _callSymRef(inst3), _branchInstruction(inst2)
      {
      }

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   uint8_t getMask()             {return _mask;}
   uint8_t setMask(uint8_t mask) {return _mask = mask;}

   virtual char *description() { return "S390MIIInstruction"; }
   virtual Kind getKind() { return IsMII; }
   virtual TR::LabelSymbol *getLabelSymbol()
      {
      return _branchInstruction;
      }

   virtual TR::SymbolReference * getSymRef() {return _callSymRef;}
   };


////////////////////////////////////////////////////////////////////////////////
// S390RISInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RISInstruction : public TR::S390RIInstruction
   {
   TR::MemoryReference * _branchDestination;
   TR::InstOpCode::S390BranchCondition _branchCondition;
   Kind _kind;

   public:

   /** Construct a new RIS instruction with no preceding instruction */
   S390RISInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         int8_t sourceImmediate,
                         TR::MemoryReference * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::CodeGenerator *cg)
           : S390RIInstruction(op, n, targetRegister, sourceImmediate, cg),
             _kind(IsRIS),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition)
      {
      setupThrowsImplicitNullPointerException(n,branchDestination);
      }

   /** Construct a new RIS instruction with preceding instruction */
   S390RISInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Register * targetRegister,
                         int8_t sourceImmediate,
                         TR::MemoryReference * branchDestination,
                         TR::InstOpCode::S390BranchCondition branchCondition,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390RIInstruction(op, n, targetRegister, sourceImmediate, precedingInstruction, cg),
             _kind(IsRIS),
             _branchDestination(branchDestination),
             _branchCondition(branchCondition)
      {
      setupThrowsImplicitNullPointerException(n,branchDestination);
      }

   virtual char *description() { return "S390RISInstruction"; }
   virtual Kind getKind() { return IsRIS; }

   /** Get branch condition information */
   virtual TR::InstOpCode::S390BranchCondition getBranchCondition()  {return _branchCondition;}
   virtual TR::InstOpCode::S390BranchCondition setBranchCondition(TR::InstOpCode::S390BranchCondition branchCondition) {return _branchCondition = branchCondition;}
   uint8_t getMask() {return getMaskForBranchCondition(getBranchCondition()) << 4;}

   /** Get branch destination information */
   virtual TR::MemoryReference * getBranchDestinationLabel() { return _branchDestination; }

   virtual uint8_t * generateBinaryEncoding();
   };


////////////////////////////////////////////////////////////////////////////////
// S390MemInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390MemInstruction : public TR::Instruction
   {
   int8_t _memAccessMode;
   int8_t _constantField;
   TR::MemoryReference *_memref;

   public:

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        TR::MemoryReference *mf,
                        TR::CodeGenerator       *cg,
                        bool use = true)
      : TR::Instruction(op, n, cg), _memAccessMode(-1), _constantField(-1), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         TR::MemoryReference *mf,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator       *cg,
                         bool use = true)
      : TR::Instruction(op, n, cond, cg), _memAccessMode(-1), _constantField(-1), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         int8_t                memAccessMode,
                         TR::MemoryReference *mf,
                         TR::CodeGenerator       *cg,
                         bool use = true)
      : TR::Instruction(op, n, cg), _memAccessMode(memAccessMode), _constantField(-1), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t               constantField,
                        int8_t                memAccessMode,
                        TR::MemoryReference *mf,
                        TR::CodeGenerator       *cg,
                        bool use = true)
      : TR::Instruction(op, n, cg), _memAccessMode(memAccessMode), _constantField(constantField), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         TR::MemoryReference *mf,
                         TR::Instruction        *precedingInstruction,
                         TR::CodeGenerator      *cg,
                         bool use = true)
      : TR::Instruction(op, n, precedingInstruction, cg), _memAccessMode(-1), _constantField(-1), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                         TR::Node               *n,
                         TR::MemoryReference *mf,
                         TR::RegisterDependencyConditions *cond,
                         TR::Instruction        *precedingInstruction,
                         TR::CodeGenerator      *cg,
                         bool use = true)
      : TR::Instruction(op, n, cond, precedingInstruction, cg), _memAccessMode(-1), _constantField(-1), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t                memAccessMode,
                        TR::MemoryReference *mf,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg,
                        bool use = true)
      : TR::Instruction(op, n, precedingInstruction, cg), _memAccessMode(memAccessMode), _constantField(-1), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390MemInstruction(TR::InstOpCode::Mnemonic         op,
                        TR::Node               *n,
                        int8_t                constantField,
                        int8_t                memAccessMode,
                        TR::MemoryReference *mf,
                        TR::Instruction        *precedingInstruction,
                        TR::CodeGenerator      *cg,
                        bool use = true)
      : TR::Instruction(op, n, precedingInstruction, cg), _memAccessMode(memAccessMode), _constantField(constantField), _memref(mf)
      {
      if (use)
         useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   virtual char *description() { return "S390MemInstruction"; }
   virtual Kind getKind() { return IsMem; }

   virtual TR::MemoryReference *getMemoryReference() { return _memref; }

   int16_t getMemAccessMode() {return _memAccessMode;}
   int16_t getConstantField() {return _constantField;}
   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);
   };

////////////////////////////////////////////////////////////////////////////////
// S390SIInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390SIInstruction : public TR::S390MemInstruction
   {
   uint8_t _sourceImmediate;
   Kind     _kind;

   public:


   S390SIInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference *mf,
                        uint8_t     imm,
                        TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf, cg, false), _sourceImmediate(imm), _kind(IsSI)
      {
      useSourceMemoryReference(mf); // need to call this *after* S390SIInstruction constructor
      }

   S390SIInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference *mf,
                        uint8_t       imm,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf, precedingInstruction, cg, false), _sourceImmediate(imm), _kind(IsSI)
      {
      useSourceMemoryReference(mf);
      };

   virtual char *description() { return "S390SIInstruction"; }
   virtual Kind getKind()                   { return _kind; }
   virtual void setKind(Kind kind)          { _kind = kind; }

   uint8_t getSourceImmediate() { return _sourceImmediate; }
   uint8_t setSourceImmediate(uint8_t si) { return _sourceImmediate = si; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

////////////////////////////////////////////////////////////////////////////////
// S390SIYInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390SIYInstruction : public TR::S390SIInstruction
   {
   public:

   S390SIYInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference *mf,
                         uint8_t        imm,
                         TR::CodeGenerator *cg)
           : S390SIInstruction(op, n, mf, imm, cg) {};

   S390SIYInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference *mf,
                         uint8_t        imm,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390SIInstruction(op, n, mf, imm, precedingInstruction, cg) {};

   virtual char *description() { return "S390SIYInstruction"; }
   virtual Kind getKind() { return IsSIY; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   };

////////////////////////////////////////////////////////////////////////////////
// S390SILInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390SILInstruction : public TR::S390MemInstruction
   {
   uint16_t _sourceImmediate;

   public:

   S390SILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::MemoryReference *mf,
                         uint16_t imm,
                         TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf, cg, false), _sourceImmediate(imm)
      {
      if (op != TR::InstOpCode::TBEGINC)
         useSourceMemoryReference(mf);
      }

   S390SILInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::MemoryReference *mf,
                         uint16_t imm,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf, precedingInstruction, cg, false), _sourceImmediate(imm)
      {
      if (op != TR::InstOpCode::TBEGINC)
         useSourceMemoryReference(mf);
      }

   virtual char *description() { return "S390SILInstruction"; }
   virtual Kind getKind() { return IsSIL; }

   uint16_t getSourceImmediate() { return _sourceImmediate; }
   uint16_t setSourceImmediate(uint16_t si) { return _sourceImmediate = si; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

////////////////////////////////////////////////////////////////////////////////
// S390SInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390SInstruction : public TR::S390MemInstruction
   {
   public:

   S390SInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference *mf,
                       TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf, cg) {};

   S390SInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n, TR::MemoryReference *mf,
                       TR::Instruction *precedingInstruction,
                       TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf, precedingInstruction, cg) {};

   virtual char *description() { return "S390SInstruction"; }
   virtual Kind getKind() { return IsS; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };


/**
 * S390RSLInstruction Class Definition
 *    ________ _______ _________________________________
 *   |Op Code |   L1  |    |  B1 |   D1  |      | 'C0'  |
 *   |________|_______|____|_____|_______|______|_______|
 *   0         8     12    16     20     32      40     47
 *
 * First memory reference operand is inherited from base class 390MemInstruction
 */
class S390RSLInstruction : public TR::S390MemInstruction
   {

   uint16_t _len; ///< length field

   public:

   S390RSLInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint16_t     len,
                         TR::MemoryReference *mf1,
                         TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf1, cg), _len(len)
      {
      }

   S390RSLInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                        uint16_t       len,
                        TR::MemoryReference *mf1,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf1, precedingInstruction, cg), _len(len)
      {
      }

   virtual char *description() { return "S390RSLInstruction"; }
   virtual Kind getKind() { return IsRSL; }

   uint16_t getLen()             {return _len;}
   uint16_t setLen(uint16_t len) {return _len = len;}

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

/**
 * S390RSLInstruction Class Definition
 *    ________ _______________________________________________
 *   | op     |     L1     |  B2 |   D2  |  R1 |  M3 |    op  |
 *   |________|____________|_____|_______|_____|_____|________|
 *   0         8           16    20     32     36    40      47
 *
 *
 * RSLb is actually very different in encoding and use then RSL so creating a new class
 */
class S390RSLbInstruction : public TR::S390RegInstruction
   {
   uint16_t _length;
   uint8_t _mask;

   public:
   S390RSLbInstruction(TR::InstOpCode::Mnemonic op,
                          TR::Node * n,
                          TR::Register *reg,
                          uint16_t length,
                          TR::MemoryReference *mf,
                          uint8_t mask,
                          TR::CodeGenerator *cg)
            : S390RegInstruction(op, n, reg, cg), _length(length), _mask(mask)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390RSLbInstruction(TR::InstOpCode::Mnemonic op,
                          TR::Node * n,
                          TR::Register *reg,
                          int16_t length,
                          TR::MemoryReference *mf,
                          int8_t mask,
                          TR::Instruction * preced,
                          TR::CodeGenerator *cg)
            : S390RegInstruction(op, n, reg, preced, cg), _length(length), _mask(mask)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   virtual char *description() { return "S390RSLbInstruction"; }
   virtual Kind getKind() { return IsRSLb; }

   uint16_t getLen()             {return _length;}
   uint16_t setLen(uint16_t len) {return _length = len;}

   uint8_t getMask()             {return _mask;}
   uint8_t setMask(uint8_t mask) {return _mask = mask;}

   virtual TR::MemoryReference* getMemoryReference()  { return (_sourceMemSize!=0) ? (sourceMemBase())[0] : NULL; }

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

/**
 * Common base class for instructions with two memory references (except SSF as this is also like an RX format instruction)
 */
class S390MemMemInstruction : public TR::S390MemInstruction
   {
   public:

   S390MemMemInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                            TR::MemoryReference *mf1,
                            TR::MemoryReference *mf2,
                            TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf1, cg)
      {
      if (mf2)
         {
         useTargetMemoryReference(mf2, mf1);
         mf2->setIs2ndMemRef();
         }
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   S390MemMemInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                            TR::MemoryReference *mf1,
                            TR::MemoryReference *mf2,
                            TR::Instruction *precedingInstruction,
                            TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf1, precedingInstruction, cg)
      {
      if (mf2)
         {
         useTargetMemoryReference(mf2, mf1);
         mf2->setIs2ndMemRef();
         }
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   S390MemMemInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                            TR::MemoryReference *mf1,
                            TR::MemoryReference *mf2,
                            TR::RegisterDependencyConditions *cond,
                            TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf1, cond, cg)
      {
      if (mf2)
         {
         useTargetMemoryReference(mf2, mf1);
         mf2->setIs2ndMemRef();
         }
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   S390MemMemInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::RegisterDependencyConditions *cond,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390MemInstruction(op, n, mf1, cond, precedingInstruction, cg)
      {
      if (mf2)
         {
         useTargetMemoryReference(mf2, mf1);
         mf2->setIs2ndMemRef();
         }
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   virtual char *description() { return "S390MemMemInstruction"; }
   virtual Kind getKind() { return IsMemMem; }

   virtual TR::MemoryReference *getMemoryReference2() { return (_targetMemSize!=0) ? (targetMemBase())[0] : NULL;}

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

/**
 * S390SS1Instruction Class Definition
 *    ________ _______ ____________________________
 *   |Op Code |   L   | B1 |   D1  | B2 |   D2     |
 *   |________|_______|____|_______|____|__________|
 *   0         8     16   20       32  36          47
 *
 */
class S390SS1Instruction : public TR::S390MemMemInstruction
   {
   uint16_t _len; ///< length field
   TR::LabelSymbol * _symbol;

   public:

   S390SS1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint16_t     len,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::CodeGenerator *cg)
           : S390MemMemInstruction(op, n, mf1, mf2, cg), _symbol(NULL), _len(len)
      {
      }

   S390SS1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                        uint16_t       len,
                        TR::MemoryReference *mf1,
                        TR::MemoryReference *mf2,
                        TR::Instruction *precedingInstruction,
                        TR::CodeGenerator *cg)
           : S390MemMemInstruction(op, n, mf1, mf2, precedingInstruction, cg),  _symbol(NULL), _len(len)
      {
      }

   S390SS1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint16_t     len,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::RegisterDependencyConditions *cond,
                         TR::CodeGenerator *cg)
           : S390MemMemInstruction(op, n, mf1, mf2, cond, cg),  _symbol(NULL), _len(len)
      {
      }

   S390SS1Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint16_t       len,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::RegisterDependencyConditions *cond,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390MemMemInstruction(op, n, mf1, mf2, cond, precedingInstruction, cg), _symbol(NULL), _len(len)
      {
      }

   virtual char *description() { return "S390SS1Instruction"; }
   virtual Kind getKind() { return IsSS1; }

   uint32_t getLen()             {return _len;}
   TR::LabelSymbol * getLabel()             {return _symbol;}
   void setLabel(TR::LabelSymbol * symbol)             {_symbol = symbol;}
   uint32_t setLen(uint16_t len) {return _len = len;}

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

class S390SS1WithImplicitGPRsInstruction : public TR::S390SS1Instruction
   {
   public:
   S390SS1WithImplicitGPRsInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                                         uint16_t     len,
                                         TR::MemoryReference *mf1,
                                         TR::MemoryReference *mf2,
                                         TR::RegisterDependencyConditions *cond,
                                         TR::Register * implicitRegSrc0,
                                         TR::Register * implicitRegSrc1,
                                         TR::Register * implicitRegTrg0,
                                         TR::Register * implicitRegTrg1,
                                         TR::CodeGenerator *cg) :
      S390SS1Instruction(op, n, len, mf1, mf2, cond, cg)
      {
      // Make sure memory references appear after registers in operands array
      int32_t i;
      int32_t nm=_sourceMemSize+_targetMemSize;
      void *vp[2]={_operands[0],_operands[1]};
      if (implicitRegTrg0 != NULL) useTargetRegister(implicitRegTrg0);
      if (implicitRegTrg1 != NULL) useTargetRegister(implicitRegTrg1);
      if (implicitRegSrc0 != NULL) useSourceRegister(implicitRegSrc0);
      if (implicitRegSrc1 != NULL) useSourceRegister(implicitRegSrc1);
      int32_t nr=_targetRegSize+_sourceRegSize;
      for (i=0; i<nr; i++)
         _operands[i]=_operands[i+nm];
      for (i=0; i<nm; i++)
         _operands[i+nr]=vp[i];
      }

   S390SS1WithImplicitGPRsInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                                         uint16_t       len,
                                         TR::MemoryReference *mf1,
                                         TR::MemoryReference *mf2,
                                         TR::RegisterDependencyConditions *cond,
                                         TR::Instruction *precedingInstruction,
                                         TR::Register * implicitRegSrc0,
                                         TR::Register * implicitRegSrc1,
                                         TR::Register * implicitRegTrg0,
                                         TR::Register * implicitRegTrg1,
                                         TR::CodeGenerator *cg) :
      S390SS1Instruction(op, n, len, mf1, mf2, cond, precedingInstruction, cg)
      {
      // Make sure memory references appear after registers in operands array
      int32_t i;
      int32_t nm=_sourceMemSize+_targetMemSize;
      void *vp[2]={_operands[0],_operands[1]};
      if (implicitRegTrg0 != NULL) useTargetRegister(implicitRegTrg0);
      if (implicitRegTrg1 != NULL) useTargetRegister(implicitRegTrg1);
      if (implicitRegSrc0 != NULL) useSourceRegister(implicitRegSrc0);
      if (implicitRegSrc1 != NULL) useSourceRegister(implicitRegSrc1);
      int32_t nr=_targetRegSize+_sourceRegSize;
      for (i=0; i<nr; i++)
         _operands[i]=_operands[i+nm];
      for (i=0; i<nm; i++)
         _operands[i+nr]=vp[i];
      }

   private:
   };

/**
 * S390SS2Instruction Class Definition
 *    ________ _____________ ______________________________
 *   |Op Code |  L1  |  L2  | B1 |   D1  |  B2  |   D2     |
 *   |________|______|______|____|_______|______|__________|
 *   0        8      12     16   20      32     36         47
 *
 * Also used for SS3 where L2 is called I3
 */
class S390SS2Instruction : public TR::S390SS1Instruction
   {
   uint16_t _len2;       ///< length field, also used as imm3 for SS3 encoded SRP
   int32_t _shiftAmount; ///< For SS3 encoded SRP

   public:

   S390SS2Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint16_t     len1,
                         TR::MemoryReference *mf1,
                         uint16_t     len2,
                         TR::MemoryReference *mf2,
                         TR::CodeGenerator *cg)
           : S390SS1Instruction(op, n, len1, mf1, mf2, cg), _len2(len2), _shiftAmount(0)
      {
      }

   S390SS2Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint16_t     len1,
                         TR::MemoryReference *mf1,
                         uint16_t     len2,
                         TR::MemoryReference *mf2,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390SS1Instruction(op, n, len1, mf1, mf2, precedingInstruction, cg), _len2(len2), _shiftAmount(0)
      {
      }

   S390SS2Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint32_t     len1,
                         TR::MemoryReference *mf1,
                         int32_t      shiftAmount,
                         uint32_t     roundAmount,
                         TR::CodeGenerator *cg)
           : S390SS1Instruction(op, n, len1, mf1, NULL, cg), _shiftAmount(shiftAmount), _len2(roundAmount)
      {
      }

   S390SS2Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         uint32_t     len1,
                         TR::MemoryReference *mf1,
                         int32_t      shiftAmount,
                         uint32_t     roundAmount,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390SS1Instruction(op, n, len1, mf1, NULL, precedingInstruction, cg), _shiftAmount(shiftAmount), _len2(roundAmount)
      {
      }

   virtual char *description() { return "S390SS2Instruction"; }
   virtual Kind getKind() { return IsSS2; }


   uint32_t getLen2()              { return _len2; }
   uint32_t setLen2(uint16_t len2) { return _len2 = len2; }

   /** For SS3 encoded SRP */
   uint32_t getImm3()              { return _len2; }
   uint32_t setImm3(uint16_t len2) { return _len2 = len2; }

   int32_t getShiftAmount()           { return _shiftAmount; }
   int32_t setShiftAmount(uint32_t s) { return _shiftAmount = s; }

   virtual uint8_t *generateBinaryEncoding();
   };

/**
 * S390SS4Instruction Class Definition
 *    ________ _____________ ______________________________
 *   |Op Code |  R1  |  R3  | B1 |   D1  |  B2  |   D2     |
 *   |________|______|______|____|_______|______|__________|
 *   0        8      12     16   20      32     36         47
 *
 *
 * Also used for an SS5 where B1(D1) is called B2(D2) and B2(D2) is called B4(D4)
 */
class S390SS4Instruction : public TR::S390SS1Instruction
   {
   int8_t      _ss4_lenidx;
   int8_t      _ss4_keyidx;
   public:

   S390SS4Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::Register *     lengthReg,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::Register * sourceKeyReg,
                         TR::CodeGenerator *cg)
           : S390SS1Instruction(op, n, 0, mf1, mf2, cg), _ss4_lenidx(-1), _ss4_keyidx(-1)
      {
      // Make sure memory references appear after registers in operands array
      int32_t i;
      int32_t nm=_sourceMemSize+_targetMemSize;
      void *vp[2]={_operands[0],_operands[1]};
      if (lengthReg)    { _ss4_lenidx=useSourceRegister(lengthReg); }
      if (sourceKeyReg) { _ss4_keyidx=useSourceRegister(sourceKeyReg); }
      int32_t nr=_targetRegSize+_sourceRegSize;
      for (i=0; i<nr; i++)
         _operands[i]=_operands[i+nm];
      for (i=0; i<nm; i++)
         _operands[i+nr]=vp[i];
      }

   S390SS4Instruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::Register *     lengthReg,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::Register * sourceKeyReg,
                         TR::Instruction * precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390SS1Instruction(op, n, 0, mf1, mf2, precedingInstruction, cg), _ss4_lenidx(-1), _ss4_keyidx(-1)
      {
      // Make sure memory references appear after registers in operands array
      int32_t i;
      int32_t nm=_sourceMemSize+_targetMemSize;
      void *vp[2]={_operands[0],_operands[1]};
      if (lengthReg)    { _ss4_lenidx=useSourceRegister(lengthReg); }
      if (sourceKeyReg) { _ss4_keyidx=useSourceRegister(sourceKeyReg); }
      int32_t nr=_targetRegSize+_sourceRegSize;
      for (i=0; i<nr; i++)
         _operands[i]=_operands[i+nm];
      for (i=0; i<nm; i++)
         _operands[i+nr]=vp[i];
      }

   virtual char *description() { return "S390SS4Instruction"; }
   virtual Kind getKind() { return IsSS4; }

   TR::Register * getLengthReg()    { return (_ss4_lenidx!=-1) ?  (sourceRegBase())[_ss4_lenidx] : NULL; }
   void setLengthReg(TR::Register *lengthReg)   { (sourceRegBase())[_ss4_lenidx] = lengthReg; }

   TR::Register * getSourceKeyReg() { return (_ss4_keyidx!=-1) ? (sourceRegBase())[_ss4_keyidx] : NULL; }
   void setSourceKeyReg(TR::Register * sourceKeyReg) { (sourceRegBase())[_ss4_keyidx] = sourceKeyReg; }

   virtual uint8_t *generateBinaryEncoding();
   };

/**
 * S390SS5WithImplicitGPRsInstruction Class Definition
 *    ________ _____________ ______________________________
 *   |Op Code |  R1  |  R3  | B2 |   D2  |  B4  |   D4     |
 *   |________|______|______|____|_______|______|__________|
 *   0        8      12     16   20      32     36         47
 *
 * Implicit GPR0 and/or GPR1
 */
class S390SS5WithImplicitGPRsInstruction : public TR::S390SS4Instruction
   {
   public:
   S390SS5WithImplicitGPRsInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                                         TR::Register * op1,
                                         TR::MemoryReference *mf2,
                                         TR::Register * op3,
                                         TR::MemoryReference *mf4,
                                         TR::Register * implicitRegSrc0,
                                         TR::Register * implicitRegSrc1,
                                         TR::Register * implicitRegTrg0,
                                         TR::Register * implicitRegTrg1,
                                         TR::CodeGenerator *cg) :
      S390SS4Instruction(op, n, op1, mf2, mf4, op3, cg)
      {
      // Make sure memory references appear after registers in operands array
      int32_t i;
      int32_t nm=_sourceMemSize+_targetMemSize;
      int32_t explicitSourceRegSize=_sourceRegSize;
      void *vp[2]={_operands[_sourceRegSize],_operands[_sourceRegSize+1]};
      if (implicitRegSrc0 != NULL) useSourceRegister(implicitRegSrc0);
      if (implicitRegSrc1 != NULL) useSourceRegister(implicitRegSrc1);
      // Because op1 and op3 are source registers let all source registers be first. Infrastructure can handle target after source but as long as they are contiguous
      if (implicitRegTrg0 != NULL) useTargetRegister(implicitRegTrg0);
      if (implicitRegTrg1 != NULL) useTargetRegister(implicitRegTrg1);
      // The first 2 (explictSourceRegSize) register operands are in place followed by the up to two memory operands
      // Move all the added implicit registers adjacents to the first two register operands
      int32_t nr=_targetRegSize+_sourceRegSize-explicitSourceRegSize;
      for (i=0; i<nr; i++)
         _operands[i+explicitSourceRegSize]=_operands[i+explicitSourceRegSize+nm];
      for (i=0; i<nm; i++)
         _operands[i+explicitSourceRegSize+nr]=vp[i];
      }

   S390SS5WithImplicitGPRsInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                                         TR::Register * op1,
                                         TR::MemoryReference *mf2,
                                         TR::Register * op3,
                                         TR::MemoryReference *mf4,
                                         TR::Instruction *precedingInstruction,
                                         TR::Register * implicitRegSrc0,
                                         TR::Register * implicitRegSrc1,
                                         TR::Register * implicitRegTrg0,
                                         TR::Register * implicitRegTrg1,
                                         TR::CodeGenerator *cg) :
      S390SS4Instruction(op, n, op1, mf2, mf4, op3, precedingInstruction, cg)
      {
      // Make sure memory references appear after registers in operands array
      int32_t i;
      int32_t nm=_sourceMemSize+_targetMemSize;
      int32_t explicitSourceRegSize=_sourceRegSize;
      void *vp[2]={_operands[_sourceRegSize],_operands[_sourceRegSize+1]};
      if (implicitRegSrc0 != NULL) useSourceRegister(implicitRegSrc0);
      if (implicitRegSrc1 != NULL) useSourceRegister(implicitRegSrc1);
      // Because op1 and op3 are source registers let all source registers be first. Infrastructure can handle target after source but as long as they are contiguous
      if (implicitRegTrg0 != NULL) useTargetRegister(implicitRegTrg0);
      if (implicitRegTrg1 != NULL) useTargetRegister(implicitRegTrg1);
      // The first 2 (explictSourceRegSize) register operands are in place followed by the up to two memory operands
      // Move all the added implicit registers adjacents to the first two register operands
      int32_t nr=_targetRegSize+_sourceRegSize-explicitSourceRegSize;
      for (i=0; i<nr; i++)
         _operands[i+explicitSourceRegSize]=_operands[i+explicitSourceRegSize+nm];
      for (i=0; i<nm; i++)
         _operands[i+explicitSourceRegSize+nr]=vp[i];
      }
   };

/**
 * S390SSEInstruction Class Definition
 *    ______________ ____________________________
 *   |Op Code       | B1 |   D1  | B2 |   D2     |
 *   |______________|____|_______|____|__________|
 *   0             16   20       32  36          47
 *
 */
class S390SSEInstruction : public TR::S390MemMemInstruction
   {
   public:

   S390SSEInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::CodeGenerator *cg)
           : S390MemMemInstruction(op, n, mf1, mf2, cg)
      {
      }

   S390SSEInstruction(TR::InstOpCode::Mnemonic op, TR::Node * n,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator *cg)
           : S390MemMemInstruction(op, n, mf1, mf2, precedingInstruction, cg)
      {
      }

   virtual char *description() { return "S390SSEInstruction"; }
   virtual Kind getKind() { return IsSSE; }
   };

////////////////////////////////////////////////////////////////////////////////
// S390RXInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RXInstruction : public TR::S390RegInstruction
   {
   uint32_t _constForMRField;
   Kind _kind;

   public:

   S390RXInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                        TR::Register            *treg,
                        TR::MemoryReference *mf,
                        TR::CodeGenerator       *cg)
      : S390RegInstruction(op, n, treg, cg), _kind(IsRX)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390RXInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                        TR::Register            *treg,
                        TR::MemoryReference *mf,
                        TR::Instruction         *precedingInstruction,
                        TR::CodeGenerator       *cg)
      : S390RegInstruction(op, n, treg, precedingInstruction, cg), _kind(IsRX)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390RXInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                        TR::RegisterPair        *regp,
                        TR::MemoryReference *mf,
                        TR::CodeGenerator       *cg)
      : S390RegInstruction(op, n, regp, cg), _kind(IsRX)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390RXInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                        TR::RegisterPair        *regp,
                        TR::MemoryReference *mf,
                        TR::Instruction         *precedingInstruction,
                        TR::CodeGenerator       *cg)
      : S390RegInstruction(op, n, regp, precedingInstruction, cg), _kind(IsRX)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      if (mf->getUnresolvedSnippet() != NULL)
         (mf->getUnresolvedSnippet())->setDataReferenceInstruction(this);
      }

   S390RXInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                        TR::Register            *treg,
                        uint32_t                constMR,
                        TR::CodeGenerator       *cg)
      : S390RegInstruction(op, n, treg, cg), _kind(IsRX),
        _constForMRField(constMR)
      {
      }

   S390RXInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                        TR::Register            *treg,
                        uint32_t                constMR,
                        TR::Instruction         *precedingInstruction,
                        TR::CodeGenerator       *cg)
      : S390RegInstruction(op, n, treg, precedingInstruction, cg), _kind(IsRX),
        _constForMRField(constMR)
      {
      }

   virtual char *description() { return "S390RXInstruction"; }
   virtual Kind getKind() { return _kind; }
   virtual void setKind(Kind kind) { _kind = kind; }

   uint32_t getConstForMRField() {return _constForMRField;}

   virtual TR::MemoryReference *getMemoryReference() { return (_sourceMemSize!=0) ? (sourceMemBase())[0] : NULL;}

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

/**
 * S390RXEInstruction Class Definition
 *
 * RXE
 *    ________________________________________________________
 *   |Op Code | R1 | X2 | B2 |      D2      | M3*|    |Op Code|
 *   |________|____|____|____|______________|____|____|_______|
 *   0        8    12   16   20             32   36   40      47
 *
 */
class S390RXEInstruction : public TR::S390RXInstruction
   {
   uint8_t mask3;
   public:
   S390RXEInstruction(TR::InstOpCode::Mnemonic         op, TR::Node * n,
                         TR::Register            *treg,
                         TR::MemoryReference *mf,
                         uint8_t                m3,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, treg, mf, cg)
      {
      mask3 = m3;
      }

   S390RXEInstruction(TR::InstOpCode::Mnemonic         op, TR::Node * n,
                         TR::Register            *treg,
                         TR::MemoryReference *mf,
                         uint8_t                m3,
                         TR::Instruction     *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, treg, mf, precedingInstruction, cg)
      {
      mask3 = m3;
      }
   uint8_t getM3() {return mask3;}
   virtual char *description() { return "S390RXEInstruction"; }
   virtual Kind getKind() { return IsRXE; }

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

////////////////////////////////////////////////////////////////////////////////
// S390RXYInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RXYInstruction : public TR::S390RXInstruction
   {
   public:

   S390RXYInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         TR::MemoryReference *mf,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, treg, mf, cg)
      {
      }

   S390RXYInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         TR::MemoryReference *mf,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, treg, mf, precedingInstruction, cg)
      {
      }

   S390RXYInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::RegisterPair        *regp,
                         TR::MemoryReference *mf,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, regp, mf, cg)
      {
      }

   S390RXYInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::RegisterPair        *regp,
                         TR::MemoryReference *mf,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, regp, mf, precedingInstruction, cg)
      {
      }

   S390RXYInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         uint32_t                constMR,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, treg, constMR, cg)
      {
      }

   S390RXYInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         uint32_t                constMR,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, treg, constMR, precedingInstruction, cg)
      {
      }

   virtual char *description() { return "S390RXYInstruction"; }
   virtual Kind getKind() { return IsRXY; }

   virtual uint8_t *generateBinaryEncoding();

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

////////////////////////////////////////////////////////////////////////////////
// S390RXYInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RXYbInstruction : public TR::S390MemInstruction
   {
   public:

   S390RXYbInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                          uint8_t                mask,
                          TR::MemoryReference *mf,
                          TR::CodeGenerator       *cg)
      : S390MemInstruction(op, n, mask, mf, cg)
      {
      }

   S390RXYbInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                          uint8_t                mask,
                          TR::MemoryReference *mf,
                          TR::Instruction        *precedingInstruction,
                          TR::CodeGenerator       *cg)
      : S390MemInstruction(op, n, mask, mf, precedingInstruction, cg)
      {
      }


   virtual char *description() { return "S390RXYbInstruction"; }

   virtual Kind getKind() { return IsRXYb; }
   virtual void setKind(Kind kind) { return; }
   };


/**
 * S390SSFInstruction Class Definition
 *    ________ _____________ ______________________________
 *   |Op Code |  R3  |  Op  | B1 |   D1  |  B2  |   D2     |
 *   |________|______|______|____|_______|______|__________|
 *   0        8      12     16   20      32     36         47
 *
 */
class S390SSFInstruction : public TR::S390RXInstruction
   {
   // TR::MemoryReference *_memoryReference2;     // second memory reference operand

   public:

   S390SSFInstruction(TR::InstOpCode::Mnemonic          op,
                         TR::Node                * n,
                         TR::Register            *reg,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, reg, mf1, cg)
      {
      useSourceMemoryReference(mf2);
      mf2->setIs2ndMemRef();
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   S390SSFInstruction(TR::InstOpCode::Mnemonic          op,
                         TR::Node                * n,
                         TR::Register            *reg,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, reg, mf1, precedingInstruction, cg)
      {
      useSourceMemoryReference(mf2);
      mf2->setIs2ndMemRef();
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   S390SSFInstruction(TR::InstOpCode::Mnemonic          op,
                         TR::Node                * n,
                         TR::RegisterPair        *regp,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, regp, mf1, cg)
      {
      useSourceMemoryReference(mf2);
      mf2->setIs2ndMemRef();
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   S390SSFInstruction(TR::InstOpCode::Mnemonic          op,
                         TR::Node                * n,
                         TR::RegisterPair        *regp,
                         TR::MemoryReference *mf1,
                         TR::MemoryReference *mf2,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RXInstruction(op, n, regp, mf1, precedingInstruction, cg)
      {
      useSourceMemoryReference(mf2);
      mf2->setIs2ndMemRef();
      setupThrowsImplicitNullPointerException(n,mf2);
      }

   virtual char *description() { return "S390SSFInstruction"; }
   virtual Kind getKind() { return IsSSF; }

   virtual TR::MemoryReference *getMemoryReference2() { return (_sourceMemSize==2) ? (sourceMemBase())[1] : NULL; }

   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual bool refsRegister(TR::Register *reg);
   virtual uint8_t *generateBinaryEncoding();
   };

////////////////////////////////////////////////////////////////////////////////
// S390RXFInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390RXFInstruction : public TR::S390RRInstruction
   {

   public:

   S390RXFInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         TR::Register           *sreg,
                         TR::MemoryReference *mf,
                         TR::CodeGenerator       *cg)
      : S390RRInstruction(op, n, treg, sreg, cg)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      }

   S390RXFInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         TR::Register           *sreg,
                         TR::MemoryReference *mf,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RRInstruction(op, n, treg, sreg, precedingInstruction, cg)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      }

   S390RXFInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         TR::Register           *sreg,
                         TR::MemoryReference *mf,
                         TR::RegisterDependencyConditions * cond,
                         TR::CodeGenerator       *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, cg)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      }

   S390RXFInstruction(TR::InstOpCode::Mnemonic          op, TR::Node * n,
                         TR::Register            *treg,
                         TR::Register           *sreg,
                         TR::MemoryReference *mf,
                         TR::RegisterDependencyConditions * cond,
                         TR::Instruction         *precedingInstruction,
                         TR::CodeGenerator       *cg)
      : S390RRInstruction(op, n, treg, sreg, cond, precedingInstruction, cg)
      {
      useSourceMemoryReference(mf);
      setupThrowsImplicitNullPointerException(n,mf);
      }

   virtual char *description() { return "S390RXFInstruction"; }
   virtual Kind getKind() { return IsRXF; }

   virtual TR::MemoryReference *getMemoryReference() {return (_sourceMemSize!=0) ? (sourceMemBase())[0] : NULL;}

   virtual uint8_t *generateBinaryEncoding();

   virtual bool refsRegister(TR::Register *reg);
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   };

/**
 * S390VInstruction Class Definition
 *
 * Vector operation Generic class
 * Six subtypes: VRI, VRR, VRS, VRV, VRX, VSI
 */
class S390VInstruction : public S390RegInstruction
   {
   char        *_opCodeBuffer;

   protected:
   S390VInstruction(
                       TR::CodeGenerator          * cg,
                       TR::InstOpCode::Mnemonic   op,
                       TR::Node                   * n,
                       TR::Register               * reg1)
   : S390RegInstruction(op, n, reg1, cg)
      {
      _opCodeBuffer = NULL;
      }

   S390VInstruction(
                       TR::CodeGenerator      * cg,
                       TR::InstOpCode::Mnemonic          op,
                       TR::Node               * n,
                       TR::Register           * reg1,
                       TR::Instruction    * precedingInstruction)
   : S390RegInstruction(op, n, reg1, precedingInstruction, cg)
      {
      _opCodeBuffer = NULL;
      }

   S390VInstruction(
                       TR::CodeGenerator         * cg,
                       TR::InstOpCode::Mnemonic  op,
                       TR::Node                  * n)
   : S390RegInstruction(op, n, cg)
      {
       _opCodeBuffer = NULL;

      }

   ~S390VInstruction();

   /**
    * Set mask field
    * <p>
    * field 0 to 3 corresponding to bit 20 to bit 35, 4 bits for each field
    * These fields cover all bytes that can potenailly hold instruction masks.
    * Applicable to all Vector instructions
    */
   virtual void setMaskField(uint32_t *instruction, uint8_t mask, int nField)
      {
      TR_ASSERT(nField >= 0 && nField <= 3, "Field index out of range.");

      int nibbleIndex = (nField < 3) ? (2 - nField) : (7);
      instruction = (nField < 3) ? instruction : (instruction + 1);
      TR::Instruction::setMaskField(instruction, mask, nibbleIndex);
      }
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual char *description() { return "S390VInstruction"; }
   virtual Kind getKind() = 0;
   virtual uint8_t * generateBinaryEncoding() = 0;
   virtual char *setOpCodeBuffer(char *c);
   virtual char *getOpCodeBuffer() { return _opCodeBuffer; }

   public:
   virtual const char *getExtendedMnemonicName() = 0;
   };

/**
 * S390VRIInstruction Class Definition
 *
 * Vector register-and-immediate operation with extended op-code field
 * Nine subtypes: VRI-a to VRI-i
 */
class S390VRIInstruction : public S390VInstruction
   {
   private:
   // masks starting from bit 28 to bit 35, 4 bits each field
   uint8_t     _mask3;
   uint8_t     _mask4;
   uint8_t     _mask5;

   // VRI formats' Immediate can have one/two of the following: I2, I3, I4
   // each can be 8, 12, or 16 bit long, not necessarily consecutive in each instruction
   // combined length can be up to 20 bits. Different sub-types use the following
   // two pieces differently
   uint16_t    _constantImm16;
   uint8_t     _constantImm8;

   bool        _printM3;
   bool        _printM4;
   bool        _printM5;

   /*
    *
    * We want these to be called only by helper constructors
    */
   protected:
   S390VRIInstruction(
                       TR::CodeGenerator      * cg               = NULL,
                       TR::InstOpCode::Mnemonic          op      = TR::InstOpCode::BAD,
                       TR::Node               * n                = NULL,
                       TR::Register           * targetReg        = NULL,
                       uint16_t                constantImm16      = 0,
                       uint8_t                 constantImm8      = 0,
                       uint8_t                 m3               = 0,    /*  4 bits (28 - 31 bit) */
                       uint8_t                 m4               = 0,    /*  4 bits (28 - 31 bit) */
                       uint8_t                 m5               = 0)    /*  4 bits (32 - 35 bit) */
   : S390VInstruction(cg, op, n, targetReg),
     _constantImm16(constantImm16),
     _constantImm8(constantImm8)
      {
      _mask3 = m3;
      _mask4 = m4;
      _mask5 = m5;

      _printM3 = getOpCode().usesM3();
      _printM4 = getOpCode().usesM4();
      _printM5 = getOpCode().usesM5();
      }

   public:
   uint8_t getM3() {return _mask3;}
   uint8_t getM4() {return _mask4;}
   uint8_t getM5() {return _mask5;}

   uint16_t getImmediateField16(){ return _constantImm16; }
   uint8_t  getImmediateField8(){ return _constantImm8; }

   virtual char *description() { return "S390VRIInstruction"; }
   virtual Kind getKind() = 0;

   const char *getExtendedMnemonicName();
   bool setPrintM3(bool b = false) { return _printM3 = b; }
   bool setPrintM4(bool b = false) { return _printM4 = b; }
   bool setPrintM5(bool b = false) { return _printM5 = b; }
   bool getPrintM3() { return _printM3; }
   bool getPrintM4() { return _printM4; }
   bool getPrintM5() { return _printM5; }


   protected:
   uint8_t* preGenerateBinaryEncoding();
   virtual uint8_t * generateBinaryEncoding() = 0;
   uint8_t* postGenerateBinaryEncoding(uint8_t*);
   };

#define CAT8TO16(val1, val2) ((uint16_t)((((uint16_t)val1) << 8) | ((uint16_t)val2)))

/**
 * VRI-a Class
 *    ________________________________________________________
 *   |Op Code | V1 |    |         I2        | M3*|RXB |Op Code|
 *   |________|____|____|___________________|____|____|_______|
 *   0        8    12   16                  32   36   40      47
 */
class S390VRIaInstruction : public S390VRIInstruction
   {
   public:
   S390VRIaInstruction(
                          TR::CodeGenerator      * cg            = NULL,
                          TR::InstOpCode::Mnemonic  op           = TR::InstOpCode::BAD,
                          TR::Node               * n             = NULL,
                          TR::Register           * targetReg     = NULL,
                          uint16_t                constantImm2   = 0,  /* 16 bits */
                          uint8_t                 mask3          = 0)     /*  4 bits */
      : S390VRIInstruction(cg, op, n, targetReg, constantImm2, 0, mask3, 0, 0)
      {
      }

   uint16_t getImmediateField2() { return getImmediateField16(); }

   virtual char *description() { return "S390VRIaInstruction"; }
   virtual Kind getKind() { return IsVRIa; }
   uint8_t * generateBinaryEncoding();
   };


/**
 * VRI-b Class
 *    ________________________________________________________
 *   |Op Code | V1 |    |   I2    |    I3   | M4*|RXB |Op Code|
 *   |________|____|____|_________|_________|____|____|_______|
 *   0        8    12   16        24   28   32   36   40      47
 */
class S390VRIbInstruction : public S390VRIInstruction
   {
   public:
   S390VRIbInstruction(
                          TR::CodeGenerator      * cg             = NULL,
                          TR::InstOpCode::Mnemonic          op    = TR::InstOpCode::BAD,
                          TR::Node               * n              = NULL,
                          TR::Register           * targetReg      = NULL,
                          uint8_t                 constantImm2    = 0,    /*  8 bits */
                          uint8_t                 constantImm3    = 0,    /*  8 bits */
                          uint8_t                 mask4           = 0)    /*  4 bits */
      : S390VRIInstruction(cg, op, n, targetReg, CAT8TO16(constantImm2, constantImm3), 0, 0, mask4, 0)
      {
      }


   uint8_t getImmediateField2() { return getImmediateField16() >> 8; }
   uint8_t getImmediateField3() { return getImmediateField16() & 0xff; }

   virtual char *description() { return "S390VRIbInstruction"; }
   virtual Kind getKind() { return IsVRIb; }
   uint8_t * generateBinaryEncoding();
   };

/** VRI-c Class
 *    ________________________________________________________
 *   |Op Code | V1 | V3 |         I2        | M4 |RXB |Op Code|
 *   |________|____|____|___________________|____|____|_______|
 *   0        8    12   16                  32   36   40      47
 */
class S390VRIcInstruction : public S390VRIInstruction
   {
   public:
   S390VRIcInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic          op               = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          TR::Register           * sourceReg3       = NULL,
                          uint16_t                constantImm2      = 0,    /* 16 bits */
                          uint8_t                 mask4             = 0)    /* 4 bits       */
   : S390VRIInstruction(cg, op, n, targetReg, constantImm2, 0, 0, mask4, 0)
      {
      if(getOpCode().setsOperand2())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);
      }

   uint16_t getImmediateField2() { return getImmediateField16(); }

   virtual char *description() { return "S390VRIcInstruction"; }
   virtual Kind getKind() { return IsVRIc; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRI-d Class
 *    ________________________________________________________
 *   |Op Code | V1 | V2 | V3 |    |    I4   | M5*|RXB |Op Code|
 *   |________|____|____|____|____|_________|____|____|_______|
 *   0        8    12   16   20   24        32   36   40      47
 */
class S390VRIdInstruction : public S390VRIInstruction
   {
   public:
   S390VRIdInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic          op      = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          TR::Register           * sourceReg2       = NULL,
                          TR::Register           * sourceReg3       = NULL,
                          uint8_t                 constantImm4      = 0,    /* 8 bit  */
                          uint8_t                 mask5             = 0)    /* 4 bits */
   : S390VRIInstruction(cg, op, n, targetReg, CAT8TO16(0, constantImm4), 0, 0, 0, mask5)
      {
      if (getOpCode().setsOperand2())
         useTargetRegister(sourceReg2);
      else
         useSourceRegister(sourceReg2);

      if (getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);
      }

   uint8_t getImmediateField4() { return getImmediateField16() & 0xff; }
   virtual char *description() { return "S390VRIdInstruction"; }
   virtual Kind getKind() { return IsVRId; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRI-e Class
 *    ________________________________________________________
 *   |Op Code | V1 | V2 |      I3      | M5 | M4 |RXB |Op Code|
 *   |________|____|____|______________|____|____|____|_______|
 *   0        8    12   16             28   32   36   40      47
 */
class S390VRIeInstruction : public S390VRIInstruction
   {
   public:
   S390VRIeInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic          op               = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          TR::Register           * sourceReg2       = NULL,
                          uint16_t                constantImm3      = 0,    /* 12 bits  */
                          uint8_t                 mask5             = 0,    /*  4 bits */
                          uint8_t                 mask4             = 0)    /*  4 bits */
   : S390VRIInstruction(cg, op, n, targetReg, (constantImm3 << 4), 0, 0, mask4, mask5)
      {
      // Error Checking
      TR_ASSERT((constantImm3 & 0xf000) == 0, "Incorrect length in immediate 3 value");

      if (getOpCode().setsOperand2())
         useTargetRegister(sourceReg2);
      else
         useSourceRegister(sourceReg2);
      }


   uint16_t getImmediateField3() { return getImmediateField16() >> 4; }

   virtual char *description() { return "S390VRIeInstruction"; }
   virtual Kind getKind() { return IsVRIe; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRI-f Class
 *    _________________________________________________________
 *   |Op Code | V1 | V2 | V3 |  /// | M5 |    I4  |RXB |Op Code|
 *   |________|____|____|____|______|____|________|____|_______|
 *   0        8    12   16   20     24   28        36   40      47
 */
class S390VRIfInstruction : public S390VRIInstruction
   {
   public:
   S390VRIfInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic          op               = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          TR::Register           * sourceReg2       = NULL,
                          TR::Register           * sourceReg3       = NULL,
                          uint8_t                constantImm4       = 0,    /*  8 bits */
                          uint8_t                 mask5             = 0)    /*  4 bits */
   : S390VRIInstruction(cg, op, n, targetReg, CAT8TO16(0, constantImm4), 0, 0, 0, mask5)
      {
      if (getOpCode().setsOperand2())
         useTargetRegister(sourceReg2);
      else
         useSourceRegister(sourceReg2);

      if (getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);
      }

   uint8_t getImmediateField4() { return getImmediateField16() & 0xff; }

   char *description() { return "S390VRIfInstruction"; }
   Kind getKind() { return IsVRIf; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRI-g Class
 *    _________________________________________________________
 *   |Op Code | V1 | V2 |   I4    | M5 |    I3    |RXB |Op Code|
 *   |________|____|____|_________|____|__________|____|_______|
 *   0        8    12   16        24    28        36   40      47
 */
class S390VRIgInstruction : public S390VRIInstruction
   {
   public:
   S390VRIgInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic          op      = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          TR::Register           * sourceReg2       = NULL,
                          uint8_t                constantImm3       = 0,    /*  8 bits */
                          uint8_t                constantImm4       = 0,    /*  8 bits */
                          uint8_t                 mask5             = 0)    /*  4 bits */
   : S390VRIInstruction(cg, op, n, targetReg, CAT8TO16(0, constantImm3), constantImm4, 0, 0, mask5)
      {
      if (getOpCode().setsOperand2())
         useTargetRegister(sourceReg2);
      else
         useSourceRegister(sourceReg2);
      }

   uint16_t getImmediateField3() { return getImmediateField16() & 0xff; }
   uint8_t getImmediateField4() { return getImmediateField8(); }

   char *description() { return "S390VRIgInstruction"; }
   Kind getKind() { return IsVRIg; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRI-h Class
 *    _________________________________________________________
 *   |Op Code | V1 |/// |        I2        |  I3  |RXB |Op Code|
 *   |________|____|____|__________________|______|____|_______|
 *   0        8    12   16                 32     36   40      47
 */
class S390VRIhInstruction : public S390VRIInstruction
   {
   public:
   S390VRIhInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic     op           = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          uint16_t                constantImm2      = 0,    /*  16 bits */
                          uint8_t                 constantImm3      = 0)    /*  4 bits */
   : S390VRIInstruction(cg, op, n, targetReg, constantImm2, constantImm3, 0, 0, 0)
      {
       // Error check constantImm3 is no longer than 4bit
       TR_ASSERT((constantImm3 & 0xf0) == 0, "Incorrect length in immediate 3 value");
      }

   uint16_t getImmediateField2() { return getImmediateField16(); }
   uint8_t getImmediateField3() { return getImmediateField8(); }

   char *description() { return "S390VRIhInstruction"; }
   Kind getKind() { return IsVRIh; }
   uint8_t * generateBinaryEncoding();
   };



/**
 * VRI-i Class
 *    _________________________________________________________
 *   |Op Code | V1 | R2 | /////  | M4 |    I3    |RXB |Op Code|
 *   |________|____|____|________|____|__________|____|_______|
 *   0        8    12   16       24    28         36   40      47
 */
class S390VRIiInstruction : public S390VRIInstruction
   {
   public:
   S390VRIiInstruction(
                          TR::CodeGenerator      * cg               = NULL,
                          TR::InstOpCode::Mnemonic          op      = TR::InstOpCode::BAD,
                          TR::Node               * n                = NULL,
                          TR::Register           * targetReg        = NULL,
                          TR::Register           * sourceReg2       = NULL,
                          uint8_t                constantImm3       = 0,    /*  8 bits */
                          uint8_t                 mask4             = 0)    /*  4 bits */
   : S390VRIInstruction(cg, op, n, targetReg, 0, constantImm3, 0, mask4, 0)
      {
       if (getOpCode().setsOperand2())
          useTargetRegister(sourceReg2);
       else
          useSourceRegister(sourceReg2);
      }

   uint8_t getImmediateField3() { return getImmediateField8(); }

   char *description() { return "S390VRIiInstruction"; }
   Kind getKind() { return IsVRIi; }
   uint8_t * generateBinaryEncoding();
   };


/**
 * S390VRRInstruction Class Definition
 *
 * Vector register-and-register operation with extended op-code field
 * Has 9 subtypes: VRR-a to VRR-i
 *
 */
class S390VRRInstruction : public S390VInstruction
   {
   // masks at bit 20 to bit 35 (not necessarily in order), 4 bits each field
   uint8_t       mask3;
   uint8_t       mask4;
   uint8_t       mask5;
   uint8_t       mask6;

   bool          _printM3;
   bool          _printM4;
   bool          _printM5;
   bool          _printM6;
   public:
   virtual char *description() { return "S390VRRInstruction"; }
   virtual Kind getKind() = 0;
   uint8_t getM3() {return mask3;}
   uint8_t getM4() {return mask4;}
   uint8_t getM5() {return mask5;}
   uint8_t getM6() {return mask6;}

   const char *getExtendedMnemonicName();
   bool setPrintM3(bool b = false) { return _printM3 = b; }
   bool setPrintM4(bool b = false) { return _printM4 = b; }
   bool setPrintM5(bool b = false) { return _printM5 = b; }
   bool setPrintM6(bool b = false) { return _printM6 = b; }

   bool getPrintM3() { return _printM3; }
   bool getPrintM4() { return _printM4; }
   bool getPrintM5() { return _printM5; }
   bool getPrintM6() { return _printM6; }

   /* We want these to be called only by helper constructors */
   protected:
   S390VRRInstruction(
                         TR::CodeGenerator       * cg  = NULL,
                         TR::InstOpCode::Mnemonic           op  = TR::InstOpCode::BAD,
                         TR::Node                * n   = NULL,
                         TR::Register            * targetReg  = NULL,
                         TR::Register            * sourceReg2 = NULL,
                         uint8_t                  m3   = 0,     /* Mask3 */
                         uint8_t                  m4   = 0,     /* Mask4 */
                         uint8_t                  m5   = 0,     /* Mask5 */
                         uint8_t                  m6   = 0)     /* Mask6 */
   : S390VInstruction(cg, op, n, targetReg)
      {
      if(sourceReg2 != NULL)
         {
         if (getOpCode().setsOperand2())
            useTargetRegister(sourceReg2);
         else
            useSourceRegister(sourceReg2);
         }
      mask3 = m3;
      mask4 = m4;
      mask5 = m5;
      mask6 = m6;

      _printM3 = getOpCode().usesM3();
      _printM4 = getOpCode().usesM4();
      _printM5 = getOpCode().usesM5();
      _printM6 = getOpCode().usesM6();
      }

      S390VRRInstruction(
                         TR::CodeGenerator       * cg,
                         TR::InstOpCode::Mnemonic           op,
                         TR::Node                * n,
                         TR::Register            * targetReg,
                         TR::Register            * sourceReg2,
                         uint8_t                  m3,          /* Mask3 */
                         uint8_t                  m4,          /* Mask4 */
                         uint8_t                  m5,          /* Mask5 */
                         uint8_t                  m6,          /* Mask6 */
                         TR::Instruction     * precedingInstruction)
   : S390VInstruction(cg, op, n, targetReg, precedingInstruction)
      {
      if (getOpCode().setsOperand2())
         useTargetRegister(sourceReg2);
      else
         useSourceRegister(sourceReg2);
      mask3 = m3;
      mask4 = m4;
      mask5 = m5;
      mask6 = m6;

      _printM3 = getOpCode().usesM3();
      _printM4 = getOpCode().usesM4();
      _printM5 = getOpCode().usesM5();
      _printM6 = getOpCode().usesM6();
      }

   virtual uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-a
 *    ________________________________________________________
 *   |Op Code | V1 | V2 |         | M5*| M4*| M3*|RXB |Op Code|
 *   |________|____|____|_________|____|____|____|____|_______|
 *   0        8    12   16        24   28   32   36   40      47
 */
class S390VRRaInstruction: public S390VRRInstruction
   {
   public:
   S390VRRaInstruction(
                          TR::CodeGenerator       * cg         = NULL,
                          TR::InstOpCode::Mnemonic           op         = TR::InstOpCode::BAD,
                          TR::Node                * n          = NULL,
                          TR::Register            * targetReg  = NULL,
                          TR::Register            * sourceReg2 = NULL,
                          uint8_t                  mask5      = 0,     /* 4 bits */
                          uint8_t                  mask4      = 0,     /* 4 bits */
                          uint8_t                  mask3      = 0)     /* 4 bits */
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, mask3, mask4, mask5, 0)
      {
      }

   S390VRRaInstruction(
                          TR::CodeGenerator       * cg,
                          TR::InstOpCode::Mnemonic           op,
                          TR::Node                * n,
                          TR::Register            * targetReg,
                          TR::Register            * sourceReg2,
                          uint8_t                  mask5,                  /* 4 bits */
                          uint8_t                  mask4,                  /* 4 bits */
                          uint8_t                  mask3,                  /* 4 bits */
                          TR::Instruction     * precedingInstruction)
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, mask3, mask4, mask5, 0, precedingInstruction)
      {
      }

   char *description() { return "S390VRRaInstruction"; }
   Kind getKind() { return IsVRRa; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-b
 *    ________________________________________________________
 *   |Op Code | V1 | V2 | V3 |    | M5*|    | M4*|RXB |Op Code|
 *   |________|____|____|____|____|____|____|____|____|_______|
 *   0        8    12   16        24   28   32   36   40      47
 */
class S390VRRbInstruction: public S390VRRInstruction
   {
   public:
   S390VRRbInstruction(
                          TR::CodeGenerator       * cg         = NULL,
                          TR::InstOpCode::Mnemonic           op         = TR::InstOpCode::BAS,
                          TR::Node                * n          = NULL,
                          TR::Register            * targetReg  = NULL,
                          TR::Register            * sourceReg2 = 0,
                          TR::Register            * sourceReg3 = 0,
                          uint8_t                  mask5       = 0,     /* 4 bits */
                          uint8_t                  mask4       = 0)     /* 4 bits */
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, 0, mask4, mask5, 0)
      {
      if(getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);
      }

   char *description() { return "S390VRRbInstruction"; }
   Kind getKind() { return IsVRRb; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-c
 *    ________________________________________________________
 *   |Op Code | V1 | V2 | V3 |    | M6*| M5*| M4*|RXB |Op Code|
 *   |________|____|____|____|____|____|____|____|____|_______|
 *   0        8    12   16        24   28   32   36   40      47
 */
class S390VRRcInstruction: public S390VRRInstruction
   {
   public:
   S390VRRcInstruction(
                          TR::CodeGenerator       * cg  = NULL,
                          TR::InstOpCode::Mnemonic           op  = TR::InstOpCode::BAD,
                          TR::Node                * n   = NULL,
                          TR::Register            * targetReg  = NULL,
                          TR::Register            * sourceReg2 = NULL,
                          TR::Register            * sourceReg3 = NULL,
                          uint8_t                  mask6 = 0,     /* 4 bits */
                          uint8_t                  mask5 = 0,     /* 4 bits */
                          uint8_t                  mask4 = 0)     /* 4 bits */
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, 0, mask4, mask5, mask6)
      {
      if (getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);
      }

   char *description() { return "S390VRRcInstruction"; }
   Kind getKind() { return IsVRRc; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-d
 *    ________________________________________________________
 *   |Op Code | V1 | V2 | V3 | M5*| M6*|    | V4 |RXB |Op Code|
 *   |________|____|____|____|____|____|____|____|____|_______|
 *   0        8    12   16   20   24   28   32   36   40      47
 */
class S390VRRdInstruction: public S390VRRInstruction
   {
   public:
   S390VRRdInstruction(
                          TR::CodeGenerator       * cg  = NULL,
                          TR::InstOpCode::Mnemonic           op  = TR::InstOpCode::BAD,
                          TR::Node                * n   = NULL,
                          TR::Register            * targetReg  = NULL,
                          TR::Register            * sourceReg2 = NULL,
                          TR::Register            * sourceReg3 = NULL,
                          TR::Register            * sourceReg4 = NULL,
                          uint8_t                  mask6 = 0,     /* 4 bits */
                          uint8_t                  mask5 = 0)     /* 4 bits */
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, 0, 0, mask5, mask6)
      {
      if (getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);

      if (getOpCode().setsOperand4())
         useTargetRegister(sourceReg4);
      else
         useSourceRegister(sourceReg4);
      }

   char *description() { return "S390VRRdInstruction"; }
   Kind getKind() { return IsVRRd; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-e
 *    ________________________________________________________
 *   |Op Code | V1 | V2 | V3 | M6*|    | M5*| V4 |RXB |Op Code|
 *   |________|____|____|____|____|____|____|____|____|_______|
 *   0        8    12   16   20        28   32   36   40      47
 */
class S390VRReInstruction: public S390VRRInstruction
   {
   public:
   S390VRReInstruction(
                          TR::CodeGenerator       * cg  = NULL,
                          TR::InstOpCode::Mnemonic           op  = TR::InstOpCode::BAD,
                          TR::Node                * n   = NULL,
                          TR::Register            * targetReg  = NULL,
                          TR::Register            * sourceReg2 = NULL,
                          TR::Register            * sourceReg3 = NULL,
                          TR::Register            * sourceReg4 = NULL,
                          uint8_t                  mask6 = 0,     /* 4 bits */
                          uint8_t                  mask5 = 0)     /* 4 bits */
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, 0, 0, mask5, mask6)
      {
      if (getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);

      if (getOpCode().setsOperand4())
         useTargetRegister(sourceReg4);
      else
         useSourceRegister(sourceReg4);
      }

   char *description() { return "S390VRReInstruction"; }
   Kind getKind() { return IsVRRe; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-f
 *    ________________________________________________________
 *   |Op Code | V1 | R2 | R3 | ///////////////// |RXB |Op Code|
 *   |________|____|____|____|___________________|____|_______|
 *   0        8    12   16   20                  36   40      47
 */
class S390VRRfInstruction: public S390VRRInstruction
   {
   public:
   S390VRRfInstruction(
                          TR::CodeGenerator       * cg  = NULL,
                          TR::InstOpCode::Mnemonic           op  = TR::InstOpCode::BAD,
                          TR::Node                * n   = NULL,
                          TR::Register            * targetReg  = NULL,
                          TR::Register            * sourceReg2 = NULL, /* GPR */
                          TR::Register            * sourceReg3 = NULL) /* GPR */
   : S390VRRInstruction(cg, op, n, targetReg, sourceReg2, 0, 0, 0, 0)
      {
      if (getOpCode().setsOperand3())
         useTargetRegister(sourceReg3);
      else
         useSourceRegister(sourceReg3);
      }

   char *description() { return "S390VRRfInstruction"; }
   Kind getKind() { return IsVRRf; }
   uint8_t * generateBinaryEncoding();
   };


/**
 * VRR-g
 *    ________________________________________________________
 *   |Op Code | // | V1 |   ///////////////////  |RXB |Op Code|
 *   |________|____|____|________________________|____|_______|
 *   0        8    12   16                       36   40     47
 */
class S390VRRgInstruction: public S390VRRInstruction
   {
   public:
   S390VRRgInstruction(
                          TR::CodeGenerator       * cg         = NULL,
                          TR::InstOpCode::Mnemonic  op         = TR::InstOpCode::BAD,
                          TR::Node                * n          = NULL,
                          TR::Register            * v1Reg  = NULL)
   : S390VRRInstruction(cg, op, n, v1Reg, NULL, 0, 0, 0, 0)
      {
      }

   char *description() { return "S390VRRgInstruction"; }
   Kind getKind() { return IsVRRg; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-h
 *    _________________________________________________________
 *   |Op Code | // | V1 | V2  | /// | M3*| ////// |RXB |Op Code|
 *   |________|____|____|_____|_____|____|________|____|_______|
 *   0        8    12   16   20      24   28       36   40    47
 */
class S390VRRhInstruction: public S390VRRInstruction
   {
   public:
   S390VRRhInstruction(
                          TR::CodeGenerator       * cg         = NULL,
                          TR::InstOpCode::Mnemonic         op  = TR::InstOpCode::BAD,
                          TR::Node                * n          = NULL,
                          TR::Register            * v1Reg      = NULL,
                          TR::Register            * v2Reg      = NULL,
                          uint8_t                   mask3      = 0)
   : S390VRRInstruction(cg, op, n, v1Reg, v2Reg, mask3, 0, 0, 0)
      {
      }

   char *description() { return "S390VRRhInstruction"; }
   Kind getKind() { return IsVRRh; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * VRR-i
 *    __________________________________________________________
 *   |Op Code | R1 | V2 | ///////// | M3  | ////// |RXB |Op Code|
 *   |________|____|____|___________|_____|________|____|_______|
 *   0        8    12   16           24    28       36   40    47
 */
class S390VRRiInstruction: public S390VRRInstruction
   {
   public:
   S390VRRiInstruction(
                          TR::CodeGenerator       * cg         = NULL,
                          TR::InstOpCode::Mnemonic   op        = TR::InstOpCode::BAD,
                          TR::Node                * n          = NULL,
                          TR::Register            * r1Reg      = NULL, /* GPR */
                          TR::Register            * v2Reg      = NULL,
                           uint8_t                   mask3     = 0)
   : S390VRRInstruction(cg, op, n, r1Reg, v2Reg, mask3, 0, 0, 0)
      {
      }

   char *description() { return "S390VRRiInstruction"; }
   Kind getKind() { return IsVRRi; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * S390VStorageInstruction Class Definition
 *
 * Vector Storage related operation with extended op-code field
 * Has the following subtypes: VRS(a-d) VRX, VRV, and VSI
 *
 */
class S390VStorageInstruction: public S390VInstruction
   {
   private:
   uint16_t _displacement2;     // D2 field, 12bit long

   protected:

   uint8_t  _maskField;         // VStorage instructions have at most 1 mask at bit 32-35
   bool     _printMaskField;

   S390VStorageInstruction(
                         TR::CodeGenerator        * cg,
                         TR::InstOpCode::Mnemonic op,
                         TR::Node                 * n,
                         TR::Register             * targetReg,   /* VRF or GPR */
                         TR::Register             * sourceReg,   /* VRF or GPR */
                         TR::MemoryReference      * mr,
                         uint8_t                  mask)          /* 4 bits  */
   : S390VInstruction(cg, op, n, targetReg), _displacement2(0), _maskField(mask)
      {
      if (sourceReg)
         {
         useSourceRegister(sourceReg);
         }
      useSourceMemoryReference(mr);
      setupThrowsImplicitNullPointerException(n, mr);

      if (mr->getUnresolvedSnippet() != NULL)
         {
         (mr->getUnresolvedSnippet())->setDataReferenceInstruction(this);
         }
      }

   S390VStorageInstruction(
                         TR::CodeGenerator          * cg,
                         TR::InstOpCode::Mnemonic   op,
                         TR::Node                   * n,
                         TR::Register               * targetReg,               /* VRF or GPR */
                         TR::Register               * sourceReg,               /* VRF or GPR */
                         TR::MemoryReference        * mr,
                         uint8_t                    mask,                    /* 4 bits  */
                         TR::Instruction     * precedingInstruction)
   : S390VInstruction(cg, op, n, targetReg, precedingInstruction), _displacement2(0), _maskField(mask)
      {
      if (sourceReg)
         useSourceRegister(sourceReg);
      useSourceMemoryReference(mr);
      setupThrowsImplicitNullPointerException(n,mr);

      if (mr->getUnresolvedSnippet() != NULL)
         {
         (mr->getUnresolvedSnippet())->setDataReferenceInstruction(this);
         }
      }


   /**
    * This S390VStorageInstruction contructor does not take target/source registers nor mask bits.
    * It is useful for vector storage formats that uses registers and immediates only: VSI.
   */
   S390VStorageInstruction(
                         TR::CodeGenerator        * cg,
                         TR::InstOpCode::Mnemonic op,
                         TR::Node                 * n)
   : S390VInstruction(cg, op, n),
     _displacement2(0),
     _maskField(0)
      {
      }

   virtual uint8_t * generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   public:
   virtual uint16_t getConstForMRField() {return _displacement2; }
   virtual TR::MemoryReference* getMemoryReference()  { return (_sourceMemSize!=0) ? (sourceMemBase())[0] : NULL; }
   virtual char *description() { return "S390VStorageInstruction"; }
   virtual Kind getKind() = 0;
   uint8_t getMaskField() { return _maskField; }

   const char *getExtendedMnemonicName();
   bool setPrintMaskField(bool b = false) { return _printMaskField = b; }
   bool getPrintMaskField() { return _printMaskField; }
   };

/**
 * S390VRSInstruction Class Definition
 *
 * Vector register-and-storage operation with extended op-code field
 * Has 4 subtypes: VRS-a to VRS-d
 *
 */
class S390VRSInstruction : public S390VInstruction
   {
   public:
   TR::Register* getFirstRegister() { return isTargetPair()? S390RegInstruction::getFirstRegister() : getRegisterOperand(1); }
   TR::Register* getLastRegister()  { return isTargetPair()? S390RegInstruction::getLastRegister()  : getRegisterOperand(2); }

   TR::Register* getSecondRegister() {return getRegisterOperand(2); }

//   virtual Kind getKind();
//   virtual bool refsRegister(TR::Register * reg);
//   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
//   virtual uint8_t * generateBinaryEncoding();
//   virtual bool getUsedRegisters(CS2::ListOf<TR::Register*, TR::Allocator>& usedRegs);
//   virtual bool getDefinedRegisters(CS2::ListOf<TR::Register *, TR::Allocator> & defedRegs);
//   virtual bool getKilledRegisters(CS2::ListOf<TR::Register *, TR::Allocator> & killedRegs);

   /* We want these to be called only by helper constructors */
   protected:
   S390VRSInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic           op           = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * targetReg    = NULL,   /* VRF        */
                         TR::Register            * sourceReg    = NULL,   /* VRF or GPR */
                         uint16_t                 displacement2= 0,      /* 12 bits */
                         uint8_t                  mask4        = 0,      /* 4 bits  */
                         uint8_t                  bitSet2      = 0)      /* 4 bits  */
      : S390VInstruction(cg, op, n)
         {
         }
   };

/**
 * VRS-a
 *    _________________________________________________________
 *   |Op Code | V1 | V3 | B2 |     D2        | M4*|RXB |Op Code|
 *   |________|____|____|____|_______________|____|____|_______|
 *   0        8    12   16   20              32   36   40      47
 */
class S390VRSaInstruction : public S390VStorageInstruction
   {
   public:
   S390VRSaInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic   op          = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * targetReg    = NULL,   /* VRF */
                         TR::Register            * sourceReg    = NULL,   /* VRF */
                         TR::MemoryReference     * mr           = NULL,
                         uint8_t                  mask4         = 0)      /*  4 bits */
   : S390VStorageInstruction(cg, op, n, targetReg, sourceReg, mr, mask4)
      {
      setPrintMaskField(getOpCode().usesM4());
      }
   Kind getKind() { return IsVRSa; }
   virtual char *description() { return "S390VRSaInstruction"; }
   };

/**
 * VRS-b
 *    _________________________________________________________
 *   |Op Code | V1 | R3 | B2 |     D2        | M4*|RXB |Op Code|
 *   |________|____|____|____|_______________|____|____|_______|
 *   0        8    12   16   20              32   36   40      47
 */
class S390VRSbInstruction : public S390VStorageInstruction
   {
   public:
   S390VRSbInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic           op  = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * targetReg    = NULL,   /* VRF */
                         TR::Register            * sourceReg    = NULL,   /* GPR */
                         TR::MemoryReference * mr               = NULL,
                         uint8_t                  mask4         = 0)      /*  4 bits */
   : S390VStorageInstruction(cg, op, n, targetReg, sourceReg, mr, mask4)
      {
      setPrintMaskField(getOpCode().usesM4());
      }

   Kind getKind() { return IsVRSb; }
   virtual char *description() { return "S390VRSbInstruction"; }
   };

/**
 * VRS-c
 *    _________________________________________________________
 *   |Op Code | R1 | V3 | B2 |     D2        | M4 |RXB |Op Code|
 *   |________|____|____|____|_______________|____|____|_______|
 *   0        8    12   16   20              32   36   40      47
 */
class S390VRScInstruction : public S390VStorageInstruction
   {
   public:
   S390VRScInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic           op           = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * targetReg    = NULL,   /* GPR */
                         TR::Register            * sourceReg    = NULL,   /* VRF */
                         TR::MemoryReference * mr               = NULL,
                         uint8_t                  mask4         = 0)      /*  4 bits */
   : S390VStorageInstruction(cg, op, n, targetReg, sourceReg, mr, mask4)
      {
      setPrintMaskField(getOpCode().usesM4());
      }
   Kind getKind() { return IsVRSc; }
   virtual char *description() { return "S390VRScInstruction"; }
   };



/**
 * VRS-d
 *    _________________________________________________________
 *   |Op Code |    | R3 | B2 |     D2        | V1 |RXB |Op Code|
 *   |________|____|____|____|_______________|____|____|_______|
 *   0        8    12   16   20              32   36   40      47
 *
 * Assume VRS-d instructions are either load or store
 *
 *  The operand order in the _operands array is:
 *  --[V1, R3, memRef] for load instructions, and
 *  --[R3, V1, memRef] for store instructions
 *
 */
class S390VRSdInstruction : public S390VStorageInstruction
   {
   public:
   S390VRSdInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic     op        = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * r3Reg        = NULL,   /* GPR */
                         TR::Register            * v1Reg        = NULL,   /* VRF */
                         TR::MemoryReference * mr               = NULL)
   : S390VStorageInstruction(cg, op, n)
      {
      if(getOpCode().isStore())
         {
         useTargetRegister(r3Reg);
         useSourceRegister(v1Reg);
         // In the current design, memRef is always the source for non-Mem-Mem instructions
         useSourceMemoryReference(mr);
         }
      else
         {
         useTargetRegister(v1Reg);
         useSourceRegister(r3Reg);
         useSourceMemoryReference(mr);
         }
      setPrintMaskField(false);
      }

   Kind getKind() { return IsVRSd; }
   uint8_t * generateBinaryEncoding();
   char *description() { return "S390VRSdInstruction"; }
   };


/**
 * S390VRVInstruction Class Definition
 *    _________________________________________________________
 *   |Op Code | V1 | V2 | B2 |     D2        | M3*|RXB |Op Code|
 *   |________|____|____|____|_______________|____|____|_______|
 *   0        8    12   16   20              32   36   40      47
 *
 * Vector register-and-vector-index-storage operation with ext. op-code field
 */
class S390VRVInstruction : public S390VStorageInstruction
   {
   public:
   S390VRVInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic           op           = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * sourceReg    = NULL,   /* VRF */
                         TR::MemoryReference * mr           = NULL,
                         uint8_t                  mask3        = 0)      /*  4 bits */
   : S390VStorageInstruction(cg, op, n, sourceReg, sourceReg, mr, mask3)
      {
      setPrintMaskField(getOpCode().usesM3());
      }
   Kind getKind() { return IsVRV; }
   uint8_t * generateBinaryEncoding();
   };

/**
 * S390VRXInstruction Class Definition
 *    ___________________________________________________________
 *   |Op Code |  V1  |  X2 | B2 |   D2   | M3* |  RXB | Op Code  |
 *   |________|______|_____|____|________|_____|______|__________|
 *   0        8      12   16    20      32   36      40        47
 *
 * Vector register-and-index-storage operation with extended op-code field
 */
class S390VRXInstruction : public S390VStorageInstruction
   {
   public:
   S390VRXInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic           op           = TR::InstOpCode::BAD,
                         TR::Node              * n            = NULL,
                         TR::Register            * reg          = NULL,   /* GPR */
                         TR::MemoryReference * mr           = NULL,
                         uint8_t                  mask3        = 0)      /*  4 bits */
   : S390VStorageInstruction(cg, op, n, reg, NULL, mr, mask3)
      {
      setPrintMaskField(getOpCode().usesM3());
      }

   S390VRXInstruction(
                         TR::CodeGenerator       * cg,
                         TR::InstOpCode::Mnemonic           op,
                         TR::Node              * n,
                         TR::Register            * reg,                  /* GPR */
                         TR::MemoryReference * mr,
                         uint8_t                  mask3,                /*  4 bits */
                         TR::Instruction     * precedingInstruction)
   : S390VStorageInstruction(cg, op, n, reg, NULL, mr, mask3, precedingInstruction)
      {
      setPrintMaskField(getOpCode().usesM3());
      }

   Kind getKind() { return IsVRX; }
   };


/**
 * S390VSIInstruction Class Definition
 *    __________________________________________________________
 *   |Op Code |    I3     | B2 |   D2   | V1  |  RXB | Op Code  |
 *   |________|___________|____|________|_____|______|__________|
 *   0        8           16    20      32    36      40       47
 *
 * Vector register-and-storage operation with extended op-code field
 *
 */
class S390VSIInstruction : public S390VStorageInstruction
   {
   uint8_t    _constantImm3;

   public:
   S390VSIInstruction(
                         TR::CodeGenerator       * cg           = NULL,
                         TR::InstOpCode::Mnemonic  op           = TR::InstOpCode::BAD,
                         TR::Node                * n            = NULL,
                         TR::Register            * v1Reg        = NULL,
                         TR::MemoryReference     * memRef       = NULL,
                         uint8_t                  constantImm3 = 0)
   : S390VStorageInstruction(cg, op, n),
     _constantImm3(constantImm3)
      {
      if(getOpCode().setsOperand1())
         {
         useTargetRegister(v1Reg);
         }
      else
         {
         useSourceRegister(v1Reg);
         }

      // memrefs are always named source memory reference regardless of what they actually are.
      useSourceMemoryReference(memRef);
      setupThrowsImplicitNullPointerException(n, memRef);

      if (memRef->getUnresolvedSnippet() != NULL)
         {
         (memRef->getUnresolvedSnippet())->setDataReferenceInstruction(this);
         }

      setPrintMaskField(false);
      }

   Kind getKind() { return IsVSI; }
   uint8_t getImmediateField3() { return _constantImm3; }
   char *description() { return "S390VSIInstruction"; }
   uint8_t * generateBinaryEncoding();
   };

////////////////////////////////////////////////////////////////////////////////
// S390NOPInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390NOPInstruction : public TR::Instruction
   {
   /** Type of this special purpose NOP */
   enum KindNOP
      {
      UnknownNOP   = 0,
      XPLinkCallNOP,
      FastLinkCallNOP
      };
   KindNOP _kindNOP;

   // Following fields are used for specialized XPLink NOP following a call site
   TR::Snippet *_targetSnippet;
   S390PseudoInstruction *_callDescInstr;  ///<  This is the branch-around fix for JNI call descriptors on zOS-31.
   intptrj_t _estimatedOffset;                ///<  Save estimated offset for conservative distance calc to Call Descriptor Snippet (XPLINK zOS31).
   int8_t  _callType;                         ///<  call type for NOP

   // Following fields are used for specialized FastLink NOP following a call site
   int32_t  _argumentsLengthOnCall;           ///< length of outgoing argument list

   public:
   S390NOPInstruction(TR::InstOpCode::Mnemonic op,
                         int32_t numbytes,
                         TR::Node *n,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg)
      {
      setTargetSnippet(NULL);
      setBinaryLength(numbytes);
      setEstimatedBinaryLength(numbytes);
      setCallType(0);
      setKindNOP(UnknownNOP);
      setArgumentsLengthOnCall(0);
      }

   S390NOPInstruction(TR::InstOpCode::Mnemonic op,
                         int32_t numbytes,
                         TR::Node * n,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg)
      {
      setTargetSnippet(NULL);
      setBinaryLength(numbytes);
      setEstimatedBinaryLength(numbytes);
      setCallType(0);
      setKindNOP(UnknownNOP);
      setArgumentsLengthOnCall(0);
      }


   S390NOPInstruction(TR::InstOpCode::Mnemonic op,
                         int32_t numbytes,
                         TR::Snippet *ts,
                         TR::Node *n,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg)
      {
      setTargetSnippet(ts);
      setBinaryLength(numbytes);
      setEstimatedBinaryLength(numbytes);
      setCallType(0);
      setKindNOP(UnknownNOP);
      setArgumentsLengthOnCall(0);
      }

   S390NOPInstruction(TR::InstOpCode::Mnemonic op,
                         int32_t numbytes,
                         TR::Snippet *ts,
                         TR::Node * n,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg)
      {
      setTargetSnippet(ts);
      setBinaryLength(numbytes);
      setEstimatedBinaryLength(numbytes);
      setCallType(0);
      setKindNOP(UnknownNOP);
      setArgumentsLengthOnCall(0);
      }

   /** Fastlink flavor */
   S390NOPInstruction(TR::InstOpCode::Mnemonic op,
                         int32_t numbytes,
                         int32_t argumentsLengthOnCall,
                         TR::Node *n,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg)
      {
      setTargetSnippet(NULL);
      setBinaryLength(numbytes);
      setEstimatedBinaryLength(numbytes);
      setCallType(0);
      setKindNOP(FastLinkCallNOP);
      setArgumentsLengthOnCall(argumentsLengthOnCall);
      }

   virtual char *description() { return "S390NOPInstruction"; }
   virtual Kind getKind() { return IsNOP; }

   TR::Snippet *getTargetSnippet()
      { return _targetSnippet; }
   TR::Snippet *setTargetSnippet(TR::Snippet *ts)
      { return _targetSnippet = ts; }

   S390PseudoInstruction *getCallDescInstr()
      { return _callDescInstr; }
   S390PseudoInstruction *setCallDescInstr(S390PseudoInstruction *cdi)
      { return _callDescInstr = cdi; }

   int8_t getCallType()
      { return _callType; }
   void setCallType(uint8_t callType)
      { _callType = callType; }

   void setArgumentsLengthOnCall(int32_t argumentsLengthOnCall)
      { _argumentsLengthOnCall = argumentsLengthOnCall; }
   int32_t getArgumentsLengthOnCall()
      { return _argumentsLengthOnCall; }
   enum KindNOP getKindNOP()
      { return _kindNOP; }
   void setKindNOP (KindNOP kindNOP)
      { _kindNOP = kindNOP; }


   virtual int32_t estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t *generateBinaryEncoding();

   };

////////////////////////////////////////////////////////////////////////////////
// S390IInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390IInstruction : public TR::Instruction
   {
   uint8_t                  _immediate;
   public:

   S390IInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node *n,
                         uint8_t im,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg), _immediate(im)
      {
      }

   S390IInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         uint8_t im,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg), _immediate(im)
      {
      }

   virtual char *description() { return "S390IInstruction"; }
   virtual Kind getKind() { return IsI; }

   uint32_t getImmediateField() { return _immediate; }
   };

////////////////////////////////////////////////////////////////////////////////
// S390EInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390EInstruction : public TR::Instruction
   {
   public:

   S390EInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node *n,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg)
      {
      setBinaryLength(2);
      setEstimatedBinaryLength(2);
      }

   S390EInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg)
      {
      setBinaryLength(2);
      setEstimatedBinaryLength(2);
      }

   /**
    * Following constructor is for PFPO instructino, where FPR0/FPR2 and GPR1 are target registers;
    * FPR4/FPR6 and GPR0 are source registers;
    * they are implied by the opcode, purpose of adding these is to notify RA the reg use/def dependencies
    */
   S390EInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node *n,
                         TR::CodeGenerator *cg,
                         TR::Register * tgt,
                         TR::Register * tgt2,
                         TR::Register * src,
                         TR::Register * src2,
                         TR::RegisterDependencyConditions * cond)
      : TR::Instruction(op, n, cond, cg)
      {
      useTargetRegister(tgt);
      useTargetRegister(tgt2);
      useSourceRegister(src);
      useSourceRegister(src2);
      setBinaryLength(2);
      setEstimatedBinaryLength(2);
      }

   S390EInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg,
                         TR::Register * tgt,
                         TR::Register * tgt2,
                         TR::Register * src,
                         TR::Register * src2,
                         TR::RegisterDependencyConditions *cond)
      : TR::Instruction(op, n, cond, precedingInstruction, cg)
      {
      useTargetRegister(tgt);
      useTargetRegister(tgt2);
      useSourceRegister(src);
      useSourceRegister(src2);
      setBinaryLength(2);
      setEstimatedBinaryLength(2);
      }

   virtual char *description() { return "S390EInstruction"; }
   virtual Kind getKind() { return IsE; }

   virtual uint8_t *generateBinaryEncoding();

   };

////////////////////////////////////////////////////////////////////////////////
// S390OpCodeOnlyInstruction Class Definition
////////////////////////////////////////////////////////////////////////////////
class S390OpCodeOnlyInstruction : public TR::Instruction
   {
   public:

   S390OpCodeOnlyInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node *n,
                         TR::CodeGenerator *cg)
      : TR::Instruction(op, n, cg)
      {
      }

   S390OpCodeOnlyInstruction(TR::InstOpCode::Mnemonic op,
                         TR::Node * n,
                         TR::Instruction *precedingInstruction,
                         TR::CodeGenerator                   *cg)
      : TR::Instruction(op, n, precedingInstruction, cg)
      {
      }

   virtual char *description() { return "S390OpCodeOnlyInstruction"; }
   virtual Kind getKind() { return IsOpCodeOnly; }

   };

}

////////////////////////////////////////////////////////////////////////////////
/*************************************************************************
 * Pseudo-safe downcast functions
 *
 *************************************************************************/

inline TR::S390MemInstruction * toS390MemInstruction(TR::Instruction *i)
   {
   return (TR::S390MemInstruction *)i;
   }

inline TR::S390RIEInstruction * toS390RIEInstruction(TR::Instruction *i)
   {
   return (TR::S390RIEInstruction *)i;
   }
inline TR::S390RRInstruction * toS390RRInstruction(TR::Instruction *i)
   {
   return (TR::S390RRInstruction *)i;
   }

inline TR::S390RXInstruction * toS390RXInstruction(TR::Instruction *i)
   {
   return (TR::S390RXInstruction *)i;
   }

inline TR::S390RXFInstruction * toS390RXFInstruction(TR::Instruction *i)
   {
   return (TR::S390RXFInstruction *)i;
   }

inline TR::S390LabelInstruction * toS390LabelInstruction(TR::Instruction *i)
   {
   return (TR::S390LabelInstruction *)i;
   }

inline TR::S390RIInstruction * toS390RIInstruction(TR::Instruction *i)
   {
   return (TR::S390RIInstruction *)i;
   }

inline TR::S390RILInstruction * toS390RILInstruction(TR::Instruction *i)
   {
   return (TR::S390RILInstruction *)i;
   }

inline TR::S390SS1Instruction * toS390SS1Instruction(TR::Instruction *i)
   {
   return (TR::S390SS1Instruction *)i;
   }

inline TR::S390SSEInstruction * toS390SSEInstruction(TR::Instruction *i)
   {
   return (TR::S390SSEInstruction *)i;
   }

inline TR::S390SS2Instruction * toS390SS2Instruction(TR::Instruction *i)
   {
   return (TR::S390SS2Instruction *)i;
   }

inline TR::S390SS4Instruction * toS390SS4Instruction(TR::Instruction *i)
   {
   return (TR::S390SS4Instruction *)i;
   }

inline TR::S390SSFInstruction * toS390SSFInstruction(TR::Instruction *i)
   {
   return (TR::S390SSFInstruction *)i;
   }

inline TR::S390RSInstruction * toS390RSInstruction(TR::Instruction *i)
   {
   return (TR::S390RSInstruction *)i;
   }

inline TR::S390VRSInstruction * toS390VRSInstruction(TR::Instruction *i)
   {
   return (TR::S390VRSInstruction *)i;
   }

inline TR::S390VRRaInstruction * toS390VRRaInstruction(TR::Instruction *i)
   {
   return (TR::S390VRRaInstruction *)i;
   }

inline TR::S390VRIaInstruction * toS390VRIaInstruction(TR::Instruction *i)
   {
   return (TR::S390VRIaInstruction *)i;
   }

inline TR::S390RSLInstruction * toS390RSLInstruction(TR::Instruction *i)
   {
   return (TR::S390RSLInstruction *)i;
   }

inline TR::S390RSLbInstruction * toS390RSLbInstruction(TR::Instruction *i)
   {
   return (TR::S390RSLbInstruction *)i;
   }

inline TR::S390RRFInstruction * toS390RRFInstruction(TR::Instruction *i)
   {
   return (TR::S390RRFInstruction *)i;
   }

inline TR::S390ImmInstruction * toS390ImmInstruction(TR::Instruction *i)
   {
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   TR_ASSERT(i->getS390ImmInstruction() != NULL, "trying to downcast to an S390ImmInstruction");
#endif
   return (TR::S390ImmInstruction *)i;
   }

inline TR::S390SMIInstruction * toS390SMIInstruction(TR::Instruction *i)
   {
   return (TR::S390SMIInstruction *)i;
   }

inline TR::S390VRSaInstruction * toS390VRSaInstruction(TR::Instruction *i)
   {
   return (TR::S390VRSaInstruction *)i;
   }

inline TR::S390VRSbInstruction * toS390VRSbInstruction(TR::Instruction *i)
   {
   return (TR::S390VRSbInstruction *)i;
   }

inline TR::S390VRScInstruction * toS390VRScInstruction(TR::Instruction *i)
   {
   return (TR::S390VRScInstruction *)i;
   }

inline TR::S390VRVInstruction * toS390VRVInstruction(TR::Instruction *i)
   {
   return (TR::S390VRVInstruction *)i;
   }

inline TR::S390VRXInstruction * toS390VRXInstruction(TR::Instruction *i)
   {
   return (TR::S390VRXInstruction *)i;
   }

TR::MemoryReference *getFirstReadWriteMemoryReference(TR::Instruction *i);

#endif
