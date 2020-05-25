/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef OMR_ARM64_CODEGENERATOR_INCL
#define OMR_ARM64_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace ARM64 { class CodeGenerator; } }
namespace OMR { typedef OMR::ARM64::CodeGenerator CodeGeneratorConnector; }
#else
#error OMR::ARM64::CodeGenerator expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include "codegen/RegisterConstants.hpp"
#include "infra/Annotations.hpp"
#include "runtime/Runtime.hpp"

class TR_ARM64OutOfLineCodeSection;
namespace TR { class ARM64LinkageProperties; }
namespace TR { class ARM64ConstantDataSnippet; }

/**
 * @brief Generates instructions for loading 32-bit integer value to a register
 * @param[in] cg : CodeGenerator
 * @param[in] node : node
 * @param[in] value : integer value
 * @param[in] trgReg : target register
 * @param[in] cursor : instruction cursor
 */
extern TR::Instruction *loadConstant32(TR::CodeGenerator *cg, TR::Node *node, int32_t value, TR::Register *trgReg, TR::Instruction *cursor = NULL);
/**
 * @brief Generates instructions for loading 64-bit integer value to a register
 * @param[in] cg : CodeGenerator
 * @param[in] node : node
 * @param[in] value : integer value
 * @param[in] trgReg : target register
 * @param[in] cursor : instruction cursor
 */
extern TR::Instruction *loadConstant64(TR::CodeGenerator *cg, TR::Node *node, int64_t value, TR::Register *trgReg, TR::Instruction *cursor = NULL);

extern TR::Instruction *loadAddressConstant(TR::CodeGenerator *cg, TR::Node *node, intptr_t value, TR::Register *trgReg, TR::Instruction *cursor=NULL, bool isPicSite=false, int16_t typeAddress=-1);

 /* @brief Generates instruction for loading address constant to register using constant data snippet
 * @param[in] cg : CodeGenerator
 * @param[in] node: node
 * @param[in] address : address
 * @param[in] trgReg : target register
 * @param[in] reloKind : relocation kind
 * @param[in] cursor : instruction cursor
 * @return generated instruction
 */
extern TR::Instruction *loadAddressConstantInSnippet(TR::CodeGenerator *cg, TR::Node *node, intptr_t address, TR::Register *trgReg, TR_ExternalRelocationTargetKind reloKind, TR::Instruction *cursor=NULL);

/**
 * @brief Helper function for encoding immediate value of logic instructions.
 * @param[in] value : immediate value to encode
 * @param[in] is64Bit : true if 64bit instruction
 * @param[out] n : N bit
 * @param[out] immEncoded : immr and imms encoded in 12bit field
 * @return true if value can be encoded as immediate operand
 */
extern bool logicImmediateHelper(uint64_t value, bool is64Bit, bool &n, uint32_t &immEncoded);

/**
 * Decodes bitmask for 32bit variants of logic op codes
 * @param[in] Nbit: N bit
 * @param[in] immr : immr part of immediate
 * @param[in] imms : imms part of immediate
 * @param[out] wmask : returned immediate
 * @return true if successfully decoded
 */
extern bool decodeBitMasks(bool Nbit, uint32_t immr, uint32_t imms, uint32_t &wmask);

/**
 * Decodes bitmask for 64bit variants of logic op codes
 * @param[in] Nbit: N bit
 * @param[in] immr : immr part of immediate
 * @param[in] imms : imms part of immediate
 * @param[out] wmask : returned immediate
 * @return true if successfully decoded
 */
extern bool decodeBitMasks(bool Nbit, uint32_t immr, uint32_t imms, uint64_t &wmask);

struct TR_ARM64BinaryEncodingData : public TR_BinaryEncodingData
   {
   int32_t estimate;
   TR::Instruction *cursorInstruction;
   TR::Instruction *i2jEntryInstruction;
   TR::Recompilation *recomp;
   };

namespace OMR
{

namespace ARM64
{

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

   public:

   CodeGenerator();

   /**
    * @brief AArch64 hook to begin instruction selection
    */
   void beginInstructionSelection();

   /**
    * @brief AArch64 hook to end instruction selection
    */
   void endInstructionSelection();

   /**
    * @brief AArch64 local register assignment pass
    * @param[in] kindsToAssign : mask of register kinds to assign in this pass
    */
   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);

   /**
    * @brief AArch64 binary encoding pass
    */
   void doBinaryEncoding();

   /**
    * @brief Creates linkage
    * @param[in] lc : linkage convention
    * @return created linkage
    */
   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   /**
    * @brief Emits data snippets
    */
   void emitDataSnippets();

   /**
    * @brief Has data snippets or not
    * @return true if it has data snippets, false otherwise
    */
   bool hasDataSnippets() { return !_dataSnippetList.empty();}

   /**
    * @brief Sets estimated locations for data snippet labels
    * @param[in] estimatedSnippetStart : estimated snippet start
    * @return estimated location
    */
   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart);

   /**
    * @brief Dumps data snippets
    * @param[in] outFile : FILE for output
    */
   void dumpDataSnippets(TR::FILE *outFile);

   /**
    * @brief Generates switch-to-interpreter pre-prologue
    * @param[in] cursor : instruction cursor
    * @param[in] node : node
    * @return instruction cursor
    */
   TR::Instruction *generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node);

   /**
    * @brief Clobber evaluate
    * @param[in] node : node to be evaluated
    * @return Register for evaluated result
    */
   TR::Register *gprClobberEvaluate(TR::Node *node);
   // different from evaluateNode in that it returns a clobberable register

   /**
    * @brief Returns the linkage properties
    * @return Linkage properties
    */
   const TR::ARM64LinkageProperties &getProperties() { return *_linkageProperties; }

   /**
    * @brief Returns the stack pointer register
    * @return stack pointer register
    */
   TR::RealRegister *getStackPointerRegister() { return _stackPtrRegister; }
   /**
    * @brief Sets the stack pointer register
    * @param[in] r : stack pointer register
    * @return stack pointer register
    */
   TR::RealRegister *setStackPointerRegister(TR::RealRegister *r) { return (_stackPtrRegister = r); }

   /**
    * @brief Returns the method meta-data register
    * @return meta-data register
    */
   TR::RealRegister *getMethodMetaDataRegister() { return _methodMetaDataRegister; }
   /**
    * @brief Sets the method meta-data register
    * @param[in] r : method meta-data register
    * @return meta-data register
    */
   TR::RealRegister *setMethodMetaDataRegister(TR::RealRegister *r) { return (_methodMetaDataRegister = r); }

   /**
    * @brief Applies 24-bit Label relative relocation (for conditional branch)
    * @param[in] cursor : instruction cursor
    * @param[in] label : label
    */
   void apply24BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label);

   /**
    * @brief Applies 32-bit Label relative relocation (for unconditional branch)
    * @param[in] cursor : instruction cursor
    * @param[in] label : label
    */
   void apply32BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label);

   /**
    * @brief find or create a constant data snippet for 8 byte constant.
    *
    * @param[in] node : the node which this constant data snippet belongs to
    * @param[in] c    : 8 byte constant
    *
    * @return : a constant data snippet
    */
   TR::ARM64ConstantDataSnippet *findOrCreate8ByteConstant(TR::Node *node, int64_t c);

   /**
    * @brief find or create a constant data snippet.
    *
    * @param[in] node : the node which this constant data snippet belongs to
    * @param[in] data : a pointer to initial data or NULL for skipping initialization
    * @param[in] size : the size of this constant data snippet
    *
    * @return : a constant data snippet with specified size
    */
   TR::ARM64ConstantDataSnippet* findOrCreateConstantDataSnippet(TR::Node* node, void* data, size_t size);

   /**
    * @brief Status of IsOutOfLineHotPath flag
    * @return IsOutOfLineHotPath flag
    */
   bool isOutOfLineHotPath() { return _flags.testAny(IsOutOfLineHotPath); }
   /**
    * @brief Sets IsOutOfLineHotPath flag
    * @param[in] v : IsOutOfLineHotPath flag
    */
   void setIsOutOfLineHotPath(bool v) { _flags.set(IsOutOfLineHotPath, v);}

   /**
    * @brief Picks register
    * @param[in] regCan : register candidate
    * @param[in] barr : array of blocks
    * @param[in] availableRegisters : available registers
    * @param[out] highRegisterNumber : high register number
    * @param[in] candidates : candidates already assigned
    * @return register number
    */
   TR_GlobalRegisterNumber pickRegister(TR_RegisterCandidate *regCan, TR::Block **barr, TR_BitVector &availableRegisters, TR_GlobalRegisterNumber &highRegisterNumber, TR_LinkHead<TR_RegisterCandidate> *candidates);
   /**
    * @brief Allows global register across branch or not
    * @param[in] regCan : register candidate
    * @param[in] branchNode : branch node
    * @return true when allowed, false otherwise
    */
   bool allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *regCan, TR::Node * branchNode);
   /**
    * @brief Gets the maximum number of GPRs allowed across edge
    * @param[in] node : node
    * @return maximum number of GPRs allowed across edge
    */
   using OMR::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge;
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *node);
   /**
    * @brief Gets the maximum number of FPRs allowed across edge
    * @param[in] node : node
    * @return maximum number of FPRs allowed across edge
    */
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *node);
   /**
    * @brief Specified global register is available or not
    * @param[in] i : global register number
    * @param[in] dt : data type
    * @return true if the specified global register is available, false otherwise
    */
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt);

   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];
   /**
    * @brief Gets global registers
    * @param[in] kind : kind of spill
    * @param[in] lc : linkage convention
    * @return BitVector of global registers
    */
   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc) { return &_globalRegisterBitVectors[kind]; }

   /**
    * @brief Gets linkage global register number
    * @param[in] linkageRegisterIndex : register index
    * @param[in] type : data type
    * @return register number for specified index, -1 if index is too large for datat type
    */
   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type);

   /**
    * @brief Gets the maximum number of assignable GPRs
    * @return the maximum number of assignable GPRs
    */
   int32_t getMaximumNumbersOfAssignableGPRs();
   /**
    * @brief Gets the maximum number of assignable FPRs
    * @return the maximum number of assignable FPRs
    */
   int32_t getMaximumNumbersOfAssignableFPRs();

   /**
     * @brief give the largest negative constant that must be materialized
     * @return the largest negative constant that must be materialized
     */
    int64_t getLargestNegConstThatMustBeMaterialized();

    /**
     * @brief give the largest positive constant that must be materialized
     * @return the largest positive constant that must be materialized
     */
    int64_t getSmallestPosConstThatMustBeMaterialized();

   /**
    * @brief Builds register map
    * @param[in] map : GC stack map
    */
   void buildRegisterMapForInstruction(TR_GCStackMap *map);

   TR_GlobalRegisterNumber _gprLinkageGlobalRegisterNumbers[TR::RealRegister::NumRegisters]; // could be smaller
   TR_GlobalRegisterNumber _fprLinkageGlobalRegisterNumbers[TR::RealRegister::NumRegisters]; // could be smaller

   /**
    * @brief Answers whether a trampoline is required for a direct call instruction to
    *           reach a target address.
    *
    * @param[in] targetAddress : the absolute address of the call target
    * @param[in] sourceAddress : the absolute address of the call instruction
    *
    * @return : true if a trampoline is required; false otherwise.
    */
   bool directCallRequiresTrampoline(intptr_t targetAddress, intptr_t sourceAddress);

   // OutOfLineCodeSection List functions
   TR::list<TR_ARM64OutOfLineCodeSection*> &getARM64OutOfLineCodeSectionList() {return _outOfLineCodeSectionList;}

   // We need to provide an implementation to avoid an unimplemented assert. See issue #4446.
   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget) { return 8; }
   // We need to provide an implementation to avoid an unimplemented assert. See issue #4446.
   int32_t arrayTranslateAndTestMinimumNumberOfIterations() { return 8; }

   /**
    * @return Retrieves the cached returnTypeInfo instruction
    */
   TR::ARM64ImmInstruction *getReturnTypeInfoInstruction() { return _returnTypeInfoInstruction; }

   /**
    * @brief Caches the returnTypeInfo instruction
    * @param[in] rtii : the returnTypeInfo instruction
    */
   void setReturnTypeInfoInstruction(TR::ARM64ImmInstruction *rtii) { _returnTypeInfoInstruction = rtii; }

   /**
    * @brief Generates pre-prologue
    * @param[in] data : binary encoding data
    */
   void generateBinaryEncodingPrePrologue(TR_ARM64BinaryEncodingData &data);

   /**
    * @brief Generates nop
    * @param[in] node: node
    * @param[in] preced : preceding instruction
    */
   TR::Instruction *generateNop(TR::Node *node, TR::Instruction *preced = 0);

   /**
    * @brief Returns bit mask for real register
    * @param[in] reg: real register number
    * 
    * @return bit mask for real register
    */
   static uint32_t registerBitMask(int32_t reg);

   private:

   enum // flags
      {
      IsOutOfLineHotPath    = 0x00000200,
      DummyLastFlag
      };

   flags32_t _flags;

   uint32_t _numGPR;
   uint32_t _numFPR;

   TR::RealRegister *_stackPtrRegister;
   TR::RealRegister *_methodMetaDataRegister;
   TR::ARM64ImmInstruction *_returnTypeInfoInstruction;
   const TR::ARM64LinkageProperties *_linkageProperties;
   TR::list<TR_ARM64OutOfLineCodeSection*> _outOfLineCodeSectionList;
   TR::vector<TR::ARM64ConstantDataSnippet*> _dataSnippetList;

   };

} // ARM64

} // TR

#endif
