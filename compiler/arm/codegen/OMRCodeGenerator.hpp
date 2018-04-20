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

#ifndef OMR_ARM_CODEGENERATOR_INCL
#define OMR_ARM_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace ARM { class CodeGenerator; } }
namespace OMR { typedef OMR::ARM::CodeGenerator CodeGeneratorConnector; }
#else
#error OMR::ARM::CodeGenerator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include <stdint.h>
#include "arm/codegen/ARMOps.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "env/jittypes.h"
#include "optimizer/DataFlowAnalysis.hpp"

namespace TR { class Register; }

extern TR::Instruction *armLoadConstant(TR::Node     *node,
                                    int32_t         value,
                                    TR::Register    *targetRegister,
                                    TR::CodeGenerator *codeGen,
                                    TR::Instruction *cursor=NULL);

extern TR::Instruction *loadAddressConstantInSnippet(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    intptrj_t      address,
                                    TR::Register    *targetRegister,
                                    bool           isUnloadablePicSite=false,
                                    TR::Instruction *cursor=NULL);

extern TR::Instruction *loadAddressConstantFixed(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    intptrj_t         value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    uint8_t         *targetAddress = NULL,
                                    uint8_t         *targetAddress2 = NULL,
                                    int16_t         typeAddress = -1,
                                    bool            doAOTRelocation = true);

extern TR::Instruction *loadAddressConstant(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    intptrj_t         value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    bool            isPicSite=false,
                                    int16_t         typeAddress = -1,
                                    uint8_t         *targetAddress = NULL,
                                    uint8_t         *targetAddress2 = NULL);

extern int32_t findOrCreateARMHelperTrampoline(int32_t helperIndex);

uint32_t encodeBranchDistance(uint32_t from, uint32_t to);
uint32_t encodeHelperBranchAndLink(TR::SymbolReference *symRef, uint8_t *cursor, TR::Node *node, TR::CodeGenerator *cg);
uint32_t encodeHelperBranch(bool isBranchAndLink, TR::SymbolReference *symRef, uint8_t *cursor, TR_ARMConditionCode cc, TR::Node *node, TR::CodeGenerator *cg);

class TR_BackingStore;
namespace TR { class Machine; }
namespace TR { class CodeGenerator; }
namespace TR { struct ARMLinkageProperties; }
namespace TR { class ARMImmInstruction; }
class TR_ARMOpCode;
namespace TR { class ARMConstantDataSnippet; }
class TR_ARMLoadLabelItem;
class TR_BitVector;
class TR_ARMScratchRegisterManager;

extern uint8_t *storeArgumentItem(TR_ARMOpCodes op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg);
extern uint8_t *loadArgumentItem(TR_ARMOpCodes op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg);

#include "il/Node.hpp"
#include "arm/codegen/ARMOutOfLineCodeSection.hpp"


namespace OMR
{

namespace ARM
{

class CodeGenerator;

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

   public:

   CodeGenerator(); /* @@ */
   CodeGenerator(TR::Compilation * comp);
   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   void beginInstructionSelection();
   void endInstructionSelection();
   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);
   void doBinaryEncoding();

   void emitDataSnippets();
   bool hasDataSnippets();
   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart);

#ifdef DEBUG
   void dumpDataSnippets(TR::FILE *outFile);
#endif

   TR::Instruction *generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node);

   int32_t findOrCreateAddressConstant(void *v, TR::DataType t,
                  TR::Instruction *n0, TR::Instruction *n1,
                  TR::Instruction *n2, TR::Instruction *n3,
                  TR::Instruction *n4,
                  TR::Node *node, bool isUnloadablePicSite);

   TR::Register *gprClobberEvaluate(TR::Node *node);

   const TR::ARMLinkageProperties &getProperties() { return *_linkageProperties; }

   TR::RealRegister *getFrameRegister()                       {return _frameRegister;}
   TR::RealRegister *setFrameRegister(TR::RealRegister *r) {return (_frameRegister = r);}

   TR::RealRegister *getMethodMetaDataRegister()                       {return _methodMetaDataRegister;}
   TR::RealRegister *setMethodMetaDataRegister(TR::RealRegister *r) {return (_methodMetaDataRegister = r);}

   void buildRegisterMapForInstruction(TR_GCStackMap *map);
   void apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   void apply8BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);

   // ARM specific thresholds for constant re-materialization
   int64_t getLargestNegConstThatMustBeMaterialized() {return -32769;}  // minimum 16-bit signed int minus 1
   int64_t getSmallestPosConstThatMustBeMaterialized() {return 32768;}  // maximum 16-bit signed int plus 1
   bool shouldValueBeInACommonedNode(int64_t); // no virt, cast

   // @@ bool canNullChkBeImplicit(TR::Node *node);

   bool hasCall()                 {return _flags.testAny(HasCall);}
   bool noStackFrame()            {return _flags.testAny(NoStackFrame);}
   bool canExceptByTrap()         {return _flags.testAny(CanExceptByTrap);}
   bool hasHardFloatReturn()      {return _flags.testAny(HasHardFloatReturn);}
   bool isOutOfLineHotPath()      {return _flags.testAny(IsOutOfLineHotPath);}

   void setHasHardFloatReturn()   { _flags.set(HasHardFloatReturn);}
   void setHasCall()              { _flags.set(HasCall);}
   void setNoStackFrame()         { _flags.set(NoStackFrame);}
   void setCanExceptByTrap()      { _flags.set(CanExceptByTrap);}
   void setIsOutOfLineHotPath(bool v)   { _flags.set(IsOutOfLineHotPath, v);}


   // This is needed by register simulation pickRegister in CodeGenRA common code.
   TR_GlobalRegisterNumber pickRegister(TR_RegisterCandidate *, TR::Block * *, TR_BitVector & availableRegisters, TR_GlobalRegisterNumber & globalRegisterNumber, TR_LinkHead<TR_RegisterCandidate> *candidates);
   bool allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *, TR::Node * branchNode);
   using OMR::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge;
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *);
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt);
   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];
   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return &_globalRegisterBitVectors[kind]; }

   TR_GlobalRegisterNumber _gprLinkageGlobalRegisterNumbers[TR::RealRegister::NumRegisters], _fprLinkageGlobalRegisterNumbers[TR::RealRegister::NumRegisters]; // these could be smaller
   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type);

   int32_t getMaximumNumbersOfAssignableGPRs();
   int32_t getMaximumNumbersOfAssignableFPRs();

   static uint32_t registerBitMask(int32_t reg)
      {
      return 1 << (reg - TR::RealRegister::FirstGPR);
      }

   // OutOfLineCodeSection List functions
   List<TR_ARMOutOfLineCodeSection> &getARMOutOfLineCodeSectionList() {return _outOfLineCodeSectionList;}
   TR_ARMOutOfLineCodeSection *findOutLinedInstructionsFromLabel(TR::LabelSymbol *label);


   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget) { return 8; } //FIXME
   int32_t arrayTranslateAndTestMinimumNumberOfIterations() { return 8; } //FIXME


   private:

   enum // flags
      {
      HasCall               = 0x00000200,
      NoStackFrame          = 0x00000400,
      CanExceptByTrap       = 0x00000800,
      HasHardFloatReturn    = 0x00001000,
      IsOutOfLineColdPath   = 0x00002000, // AVAILABLE
      IsOutOfLineHotPath    = 0x00004000,
      DummyLastFlag
      };

   static TR_Processor            _processor;
   flags32_t                      _flags;
   uint32_t                         _numGPR;
   uint32_t                         _numFPR;
   TR::RealRegister            *_frameRegister;
   TR::RealRegister            *_methodMetaDataRegister;
   TR::ARMConstantDataSnippet       *_constantData;
   TR::ARMLinkageProperties   *_linkageProperties;
   List<TR_ARMOutOfLineCodeSection> _outOfLineCodeSectionList;
   // Internal Control Flow Depth Counters
   int32_t                          _internalControlFlowNestingDepth;
   int32_t                          _internalControlFlowSafeNestingDepth;

   protected:

   TR::ARMImmInstruction          *_returnTypeInfoInstruction;

   };

} // ARM

} // TR

#endif
