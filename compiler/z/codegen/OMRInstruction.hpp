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

#ifndef OMR_Z_INSTRUCTIONBASE_INCL
#define OMR_Z_INSTRUCTIONBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTRUCTION_CONNECTOR
#define OMR_INSTRUCTION_CONNECTOR
namespace OMR { namespace Z { class Instruction; } }
namespace OMR { typedef OMR::Z::Instruction InstructionConnector; }
#else
#error OMR::Z::Instruction expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRInstruction.hpp"

#include <stddef.h>                                    // for NULL
#include <stdint.h>                                    // for uint32_t, etc
#include "codegen/InstOpCode.hpp"                      // for InstOpCode, InstOpCode::Mnemonic
#include "codegen/Register.hpp"                        // for Register
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"                     // for Compilation, etc
#include "cs2/arrayof.h"                               // for ArrayOf
#include "cs2/hashtab.h"                               // for HashTable, etc
#include "cs2/sparsrbit.h"
#include "env/TRMemory.hpp"                            // for Allocator, etc
#include "infra/Assert.hpp"                            // for TR_ASSERT
#include "infra/Flags.hpp"                             // for flags16_t

class TR_Debug;
namespace TR { class S390ImmInstruction; }
class TR_S390RegisterDependencyGroup;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class RealRegister; }
namespace TR { class RegisterDependencyConditions; }

#define printInstr(comp, instr)  (comp->getDebug() ? comp->getDebug()->printInstruction((instr)) : (void)0)

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE Instruction : public OMR::Instruction
   {
   protected:

   Instruction(TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic    op,
                   TR::Node          *n);

   Instruction(TR::CodeGenerator *cg,
                   TR::Instruction *precedingInstruction,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n = 0);

   void initialize(TR::RegisterDependencyConditions * cond) { _conditions = cond;}
   void initialize(TR::Instruction * precedingInstruction = NULL, bool instFlag = false, TR::RegisterDependencyConditions * cond = NULL, bool condFlag = false);

   public:

   //  Basic Block Index Routines
   int32_t getBlockIndex() { return _blockIndex; }
   void setBlockIndex(int32_t i) { _blockIndex = i; }

   //  Register Dependency Routines
   TR::RegisterDependencyConditions* getDependencyConditions() {return _conditions;}
   TR::RegisterDependencyConditions* setDependencyConditions(TR::RegisterDependencyConditions *cond);
   TR::RegisterDependencyConditions* setDependencyConditionsNoBookKeeping(TR::RegisterDependencyConditions *cond);
   TR::RegisterDependencyConditions* updateDependencyConditions(TR::RegisterDependencyConditions *cond);
   void resetDependencyConditions(TR::RegisterDependencyConditions * cond = NULL) { _conditions = cond;}

   TR::Register* tgtRegArrElem(int32_t i);
   TR::Register* srcRegArrElem(int32_t i);

   bool isOutOfLineEX()  { return _flags.testAny(OutOfLineEX); }
   void setOutOfLineEX() { _flags.set(OutOfLineEX); }
   TR::Instruction *getOutOfLineEXInstr();

   void setupThrowsImplicitNullPointerException(TR::Node *n, TR::MemoryReference *mr);

   bool throwsImplicitNullPointerException()  { return _flags.testAny(ThrowsImplicitNullPointerException); }
   void setThrowsImplicitNullPointerException();

   // TODO - set this flag whenever an instruction can cause an implicit exception
   // currently this is only implemented for null pointer exceptions, but it should
   // include others - such as divide by 0, etc.
   bool throwsImplicitException()  { return _flags.testAny(ThrowsImplicitException); }
   void setThrowsImplicitException() { _flags.set(ThrowsImplicitException); }

   bool isExtDisp()  {return _flags.testAny(ExtDisp);}
   void setExtDisp() {_flags.set(ExtDisp);}

   bool isExtDisp2()  {return _flags.testAny(ExtDisp2);}
   void setExtDisp2() {_flags.set(ExtDisp2);}

   bool isExceptBranchOp() { return _flags.testAny(ExceptBranchOp); }
   void setExceptBranchOp() {_flags.set(ExceptBranchOp);}

   bool isCCuseKnown() { return _flags.testAny(CCuseKnown); }
   void setCCuseKnown() {_flags.set(CCuseKnown);}

   bool isStartInternalControlFlow(){ return _flags.testAny(StartInternalControlFlow); }
   void setStartInternalControlFlow() {_flags.set(StartInternalControlFlow);}

   bool isEndInternalControlFlow(){ return _flags.testAny(EndInternalControlFlow); }
   void setEndInternalControlFlow() {_flags.set(EndInternalControlFlow);}

   // Region numbers start life as just the inline indexes
   // but are later extended by any optimization that duplicates
   // IL (e.g. loopCanonicalizer)
   int16_t getRegionNumber() { return 0; }
   void setRegionNumber(int16_t r) {  }

   bool isCCused() { return _flags.testAny(CCused); }
   void setCCused() {_flags.set(CCused);}

   bool isDebugHookOp() { return _flags.testAny(DebugHookOp); }
   void setDebugHookOp() {_flags.set(DebugHookOp);}

   bool hasLongDisplacementSupport();
   TR::InstOpCode::Mnemonic opCodeCanBeAdjustedTo(TR::InstOpCode::Mnemonic);
   void attemptOpAdjustmentForLongDisplacement();
   TR::Register* getRegForBinaryEncoding(TR::Register* reg);
   void useRegister(TR::Register *reg, bool isDummy = false);
   bool matchesAnyRegister(TR::Register* reg, TR::Register* instReg);
   bool matchesAnyRegister(TR::Register* reg, TR::Register* instReg1, TR::Register* instReg2);
   bool isDefRegister(TR::Register * reg);

   virtual bool getRegisters(TR::list<TR::Register *> &regs);
   virtual bool getUsedRegisters(TR::list<TR::Register *> &usedRegs);
   virtual bool getDefinedRegisters(TR::list<TR::Register *> &defedRegs);
   virtual bool getKilledRegisters(TR::list<TR::Register *> &killedRegs);

   int32_t renameRegister(TR::Register *from, TR::Register *to);

   virtual Kind getKind() { return IsNotExtended; }
   virtual bool isRegInstruction() { return false; }

   virtual void setKind(Kind kind) { TR_ASSERT(0, "Should not be called unless working with an RX, RS or RI instructions"); }

   virtual TR::MemoryReference* getMemoryReference()  {  return NULL; }
   virtual TR::MemoryReference* getMemoryReference2() {  return NULL; }

   /** Local-local allocatable regs used in genBin phase to patch up code */
   virtual bool getOneLocalLocalAllocFreeReg(TR::RealRegister** reg);
   virtual bool getTwoLocalLocalAllocFreeReg(TR::RealRegister** reg1, TR::RealRegister** reg2);

   virtual TR::RealRegister* getLocalLocalSpillReg1()                         { return _longDispSpillReg1;       }
   virtual TR::RealRegister* setLocalLocalSpillReg1(TR::RealRegister* reg) { return _longDispSpillReg1 = reg; }

   virtual TR::RealRegister* getLocalLocalSpillReg2()                         { return _longDispSpillReg2;       }
   virtual TR::RealRegister* setLocalLocalSpillReg2(TR::RealRegister* reg) { return _longDispSpillReg2 = reg; }

   virtual uint32_t getBinLocalFreeRegs()             { return _binFreeRegs;       }
   virtual uint32_t setBinLocalFreeRegs(uint32_t bin) { return _binFreeRegs = bin; }

   virtual TR::RealRegister * assignBestSpillRegister();
   virtual TR::RealRegister * assignBestSpillRegister2();

   virtual uint32_t assignFreeRegBitVector();


   uint32_t getEstimatedBinaryOffset();
   void setEstimatedBinaryOffset(uint32_t l);


   virtual int32_t getMachineOpCode();

   virtual bool isLoad()              { return _opcode.isLoad() > 0; }
   virtual bool isStore()             { return _opcode.isStore() > 0; }
   virtual bool isBranchOp()          { return _opcode.isBranchOp() > 0; }
   virtual bool isTrap()              { return _opcode.isTrap() > 0; }
   virtual bool isLabel()             { return _opcode.isLabel() > 0; }
   virtual bool isFloat()             { return _opcode.singleFPOp() > 0 || _opcode.doubleFPOp() > 0; }
   virtual bool isAdmin()             { return _opcode.isAdmin() > 0; }
   virtual bool isBeginBlock()        { return _opcode.isBeginBlock() > 0; }
   virtual bool isDebugFence()        { return false; }

   virtual bool is4ByteLoad();
   virtual bool isAsmGen();
   virtual bool isRet();
   virtual bool isTailCall();

   virtual bool implicitlyUsesGPR0() { return _opcode.implicitlyUsesGPR0() > 0; }
   virtual bool implicitlyUsesGPR1() { return _opcode.implicitlyUsesGPR1() > 0; }
   virtual bool implicitlyUsesGPR2() { return _opcode.implicitlyUsesGPR2() > 0; }

   virtual bool implicitlySetsGPR0() { return _opcode.implicitlySetsGPR0() > 0; }
   virtual bool implicitlySetsGPR1() { return _opcode.implicitlySetsGPR1() > 0; }
   virtual bool implicitlySetsGPR2() { return _opcode.implicitlySetsGPR2() > 0; }

   bool isCompare() { return _opcode.isCompare() > 0; }
   bool fprOp()     { return _opcode.fprOp() > 0; }

   virtual void setMaskField(uint32_t *instruction, int8_t mask, int8_t nibbleIndex);
   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);


   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   /** @return true if reg matches any uses operand registers or implicitly used real registers */
   virtual bool refsRegister(TR::Register *reg);
   /** @return ture if reg matches and result register */
   virtual bool defsRegister(TR::Register *reg);
   virtual bool defsAnyRegister(TR::Register *reg); // Includes looking into register pairs
   virtual bool usesOnlyRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);
   virtual bool containsRegister(TR::Register *reg);
   virtual bool startOfLiveRange(TR::Register *reg); // True if virtual register is dead before current instruction

   virtual bool dependencyRefsRegister(TR::Register *reg);

   void setUseDefRegisters(bool updateDependencies);
   void resetUseDefRegisters();

   virtual TR::Register *getTargetRegister(uint32_t i);
   virtual TR::Register *getSourceRegister(uint32_t i);
   uint32_t getNumRegisterOperands() { return _targetRegSize+_sourceRegSize; }
   virtual TR::Register *getRegisterOperand(uint32_t i) { return (i>0 && i<=_targetRegSize+_sourceRegSize) ? (TR::Register*)_operands[i-1] : NULL;}
   virtual void setRegisterOperand(uint32_t i, TR::Register *newReg) { TR_ASSERT((i>0 && i<=_targetRegSize+_sourceRegSize),"Register operand number %d does not exist.",i); _operands[i-1]=newReg;}

   virtual bool matchesTargetRegister(TR::Register* reg);

   bool     sourceRegUsedInMemoryReference(uint32_t i);

   void addComment(char *) { }
   void      setBreakPoint(bool v) {v ? _index |= BreakPoint : _index &= ~BreakPoint;}

   virtual bool isCall();

   #if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   /** The following safe virtual downcast method is used under debug only for assertion checking */
   virtual TR::S390ImmInstruction *getS390ImmInstruction();
   #endif


   void setCCInfo();
   void readCCInfo();
   void clearCCInfo();
   void addARDependencyCondition(TR::Register * virtAR, TR::Register * assignedGPR);

   const char *getName(TR_Debug * debug);
   bool isBreakPoint() {return (_index & BreakPoint) != 0;}
   bool isWillBePatched() { return (_index & WillBePatched) != 0; }
   void setWillBePatched() { _index |= WillBePatched; }
   virtual char *description() { return "S390"; }


   private:

   int32_t       _blockIndex;
   TR::RegisterDependencyConditions *_conditions;

   enum // _flags
      {
      // Available                        = 0x0001,
      ExtDisp                             = 0x0002, ///< inst has had a late long displacement fixup on the first memRef
      // Available                        = 0x0004,
      // Available                        = 0x0008,
      ExceptBranchOp                      = 0x0010,
      ExtDisp2                            = 0x0020, ///< inst has had a late long displacement fixup on the second memRef
      Reserved5                           = 0x0040,
      Reserved6                           = 0x0080,
      CCuseKnown                          = 0x0100, ///< Usage of CC set by current instruction is known.
      CCused                              = 0x0200, ///< CC set by current instruction is used by subsequent instructions.
      OutOfLineEX                         = 0x0400, ///< TR::InstOpCode::EX instruction references a ConstantInstructionSnippet object
                                                    ///< or an SS_FORMAT instruction is the target of an TR::InstOpCode::EX instruction
      ThrowsImplicitException             = 0x0800,
      ThrowsImplicitNullPointerException  = 0x1000,
      StartInternalControlFlow            = 0x2000,
      EndInternalControlFlow              = 0x4000,
      WillBePatched                       = 0x08000000,
      BreakPoint                          = 0x20000000,  ///< Z codegen
      DebugHookOp                         = 0x8000 ///< (WCODE) instruction for debug hooks
      };


   flags16_t _flags;


   protected:

   /**
    * Objective for organizing operands is as follows:
    *   Operands are TR::Register * or any derrived type, TR_MemoryReference * in the order target registers, source registers, source mem, target mem
    *   Register operands fall into target or source depending how they are used. If a register operand is set it will be in target category, if register
    *     is set and used it will also be in target category but not in source. The opcode property UsesTarget indicates that all target registers are
    *     used for input. 390 Instructions have 0,1,2, or 3 register operands often refered in manual as R1,R2,R3. These operands can be retreived using
    *     getRegisterOperand(i). Note that a compare or store instruction that does not set operand 1 will put R1 into the sourceReg section of operands.
    *     This is not what it was like before. getTargetRegister used to fetch R1 and getSourceRegister used to fetch R2. These were not always meaning
    *     source and target so now these methods are disallowed. Register operands can be register pairs for instructions that use an even-odd pair.
    */
   void **        _operands;

   int8_t     _sourceRegSize;
   int8_t     _targetRegSize;
   int8_t     _sourceMemSize;
   int8_t     _targetMemSize;
   int8_t     _targetStart;
   int8_t     _sourceStart;

   int8_t   numOperands(void) { return _sourceRegSize+_targetRegSize+_sourceMemSize+_targetMemSize; }
   int8_t   lastOperandIndex(void);
   void     swap_operands(int i,int j) { void *op1 = _operands[i]; _operands[i]=_operands[j]; _operands[j]=op1; }

   TR::Register **targetRegBase()
      { return (_targetRegSize!=0) ? (TR::Register**)_operands+_targetStart : NULL;  }
   TR::Register **sourceRegBase()
      { return (_sourceRegSize!=0) ? ((TR::Register**)_operands)+_sourceStart : NULL;  }
   TR::MemoryReference** sourceMemBase()
      { return (_sourceMemSize!=0) ? ((TR::MemoryReference**)_operands)+_targetRegSize+_sourceRegSize : NULL;  }
   TR::MemoryReference** targetMemBase()
      { return (_targetMemSize!=0) ? ((TR::MemoryReference**)_operands)+_targetRegSize+_sourceRegSize+_sourceMemSize : NULL;  }

   template <typename T>
   class RegisterArray
      {
      public:
         TR::Allocator allocator() { return TR::comp()->allocator(); }

         static void *operator new(size_t size, TR::Allocator a)
            { return a.allocate(size); }
         static void  operator delete(void *ptr, size_t size)
            { ((RegisterArray*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() must return the same allocator as used for new */

         /* Virtual destructor is necessary for the above delete operator to work
          * See "Modern C++ Design" section 4.7
          */
         virtual ~RegisterArray() {}

      private:
         // template <class AElementType, class Allocator = CS2::allocator, size_t segmentBits = 8>
         typename CS2::ArrayOf< T, TR::Allocator, 3 > _impl;

      public:
         explicit RegisterArray(TR::Compilation * c) :
            _impl(c->allocator())
            {
            }

         T & operator[](size_t i)
            {
            return _impl[i];
            }

         size_t size()
            {
            size_t n = 0;
            for (size_t i = 0; i < _impl.NumberOfElements(); i++)
               {
               if (_impl[i] != NULL)
                  {
                  n += 1;
                  }
               }
            return n;
            }

         void MakeEmpty()
            {
            _impl.MakeEmpty();
            }
      };

   class RegisterBitVector
      {
      public:

         static void *operator new(size_t size, TR::Allocator a)
            { return a.allocate(size); }
         static void  operator delete(void *ptr, size_t size)
            { ((RegisterBitVector*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() must return the same allocator as used for new */

         /* Virtual destructor is necessary for the above delete operator to work
          * See "Modern C++ Design" section 4.7
          */
         virtual ~RegisterBitVector() {}

         TR::Allocator allocator() { return TR::comp()->allocator(); }

      private:
         CS2::ASparseBitVector< TR::Allocator > _impl;

      public:
         explicit RegisterBitVector(TR::Compilation * c) :
            _impl(c->allocator())
            {
            }

         void set(size_t i)
            {
            _impl[i] = true;
            }

         bool isSet(size_t i)
            {
            return _impl.ValueAt(i);
            }
      };

   RegisterArray<TR::Register*> * _useRegs;
   RegisterArray<TR::Register*> * _defRegs;
   RegisterBitVector * _sourceUsedInMemoryReference;

   // Long Disp Information
   TR::RealRegister * _longDispSpillReg1;
   TR::RealRegister * _longDispSpillReg2;
   uint32_t              _binFreeRegs;

   //  Look for spilled regs
   //
   bool anySpilledRegisters(TR::Register ** listReg, int32_t listRegSize);
   bool anySpilledRegisters(TR::MemoryReference ** listMem, int32_t listMemSize);

   //  registerSets for generic register assignment
   int32_t gatherRegPairsAtFrontOfArray(TR::Register** regArr, int32_t sourceRegSize);
   TR::Register* assignRegisterNoDependencies(TR::Register* reg);
   void assignOrderedRegisters(TR_RegisterKinds kindToBeAssigned);
   virtual void assignRegistersAndDependencies(TR_RegisterKinds kindToBeAssigned);
   void blockHPR(TR::Register * reg);
   void unblockHPR(TR::Register * reg);
   void block(TR::Register** sourceReg, int32_t _sourceRegSize, TR::Register** targetReg, int targetRegSize,
              TR::MemoryReference** sourceMem, TR::MemoryReference** targetMem);
   void unblock(TR::Register** sourceReg, int32_t sourceRegSize, TR::Register** targetReg, int targetRegSize,
                TR::MemoryReference** sourceMem, TR::MemoryReference** targetMem);

   void  recordOperand(void *operand, int8_t &operandCount);

   uint32_t useSourceRegister(TR::Register* reg);
   uint32_t useTargetRegister(TR::Register* reg);
   uint32_t useSourceMemoryReference(TR::MemoryReference* memRef);
   uint32_t useTargetMemoryReference(TR::MemoryReference* memRef, TR::MemoryReference* sourceMemRef);


   bool isHPRUpgradable(uint16_t regType);

   bool checkRegForGPR0Disable(TR::InstOpCode::Mnemonic op, TR::Register* reg);



   };

}

}

#endif /* OMR_Z_INSTRUCTIONBASE_INCL */
