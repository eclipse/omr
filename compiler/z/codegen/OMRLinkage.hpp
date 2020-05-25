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

#ifndef OMR_Z_LINKAGE_INCL
#define OMR_Z_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { namespace Z { class Linkage; } }
namespace OMR { typedef OMR::Z::Linkage LinkageConnector; }
#endif

#include "compiler/codegen/OMRLinkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"

namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Snippet; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class SystemLinkage; }
template <class T> class List;

/**
 * 390 Explicit linkage conventions
 */
enum TR_S390LinkageConventions
   {
   TR_S390LinkageDefault   = 0,     ///< reserved for default construction

   // Java specific linkages
   TR_JavaPrivate          = 0x1,   ///< Java private linkage
   TR_JavaHelper           = 0x2,   ///< Java helper linkage

   TR_SystemXPLink         = 0x20,  ///< (Java) zOS XPLink convention

   // zOS Type 1 linkages (non-Java) - all have bit 0x40 set
   #define TR_SystemOS_MASK  0x40

   // zOS Type 1 linkage - C++ Fastlink
   TR_SystemFastLink       = 0x4D,  ///< (non-Java) zOS (C++) FastLink convention

   // Linux Platform Linkages
   TR_SystemLinux          = 60     ///< (Java) Linux convention
   };

////////////////////////////////////////////////////////////////////////////////
// linkage properties
////////////////////////////////////////////////////////////////////////////////

#define FirstParmAtFixedOffset        0x001
#define SplitLongParm                 0x002
#define RightToLeft                   0x004
#define NeedsWidening                 0x008
#define AllParmsOnStack               0x010
#define ParmsInReverseOrder           0x020
#define SkipGPRsForFloatParms         0x040
#define PadFloatParms                 0x080
#define TwoStackSlotsForLongAndDouble 0x100
// Available                          0x200
#define AggregatesPassedOnParmStack   0x400
#define AggregatesPassedInParmRegs    0x800
#define AggregatesReturnedInRegs      0x1000
// Available                          0x2000
#define SmallIntParmsAlignedRight     0x4000  ///< < gprSize int parms aligned into the parmword
// Available                          0x8000
#define ForceSaveIncomingParameters   0x10000 ///< Force parameters to be saved: example: non-Java
#define LongDoubleReturnedOnStorage   0x20000 ///< zLinux C/C++
#define ComplexReturnedOnStorage      0x40000 ///< zLinux C/C++
#define LongDoublePassedOnStorage     0x80000 ///< zLinux C/C++
#define ComplexPassedOnStorage        0x100000 ///< zLinux C/C++
#define ParmsMappedBeforeAutos        0x200000 ///< e.g. for C++ FASTLINK


////////////////////////////////////////////////////////////////////////////////
//RightToLeft
//     This name is confusing but actually this means the order of
//     parameters on stack relative to the stack pointer.
//
//     eg.  foo(1, 2, 3)
//
//
//       |  1  |
//       |  2  |
//       |  3  |   RightToLeft means the rightmost argument 3 is closer to the
//       |-----|   stack poiter
//       |     |
//        -----    <-- stack pointer
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// register flags
////////////////////////////////////////////////////////////////////////////////

#define Preserved                   0x01
#define IntegerArgument             0x02
#define FloatArgument               0x04
#define IntegerReturn               0x08
#define FloatReturn                 0x10
#define IntegerArgumentAddToPost    0x20
#define VectorReturn                0x40
#define VectorArgument              0x80
// Non-Java.  We need to add a
// particular integer arg register to both the pre- and post-conditions
// of a BASR.

#define  REGNUM(ri)      ((TR::RealRegister::RegNum)(ri))
#define  REGINDEX(i)     (i-TR::RealRegister::FirstGPR)
#define  FPREGINDEX(i)   (i-TR::RealRegister::FirstFPR)
#define  VRFREGINDEX(i)  (i-TR::RealRegister::FirstAssignableVRF)

/**
 * ANYREGINDEX is a confusing macro so...
 *
 * 1. Defensively using a do { .. } while(0) for macro-safety won't work because of the way we use ANYREGINDEX
 * 2. In-line commenting doesn't seem to work reliably for multi-line macros (hence this comment..)
 * 3. The indentation of the answer to (i >= ...) ? is the same as the indentation of the condition
 *      i.e (i >= TR::RealRegister::FirstFPR)/    <<-----
 *          ...                                             | Notice the equal indentation
 *          FPREGINDEX(i)                            <<-----
 * 4. REGINDEX(i) is the default i.e GPR index, and so lacks a matching select condition.
 * 5. Each unrelated condition or answer to a condition is separated by TR indentation rule i.e 3 spaces
 *
 * Feel free to refactor this into if/else conditions :)
 */
#define  ANYREGINDEX(i)  ((i >= TR::RealRegister::FirstFPR) ?                   \
                            ((i >= TR::RealRegister::FirstAssignableVRF) ?      \
                              VRFREGINDEX(i) :                                   \
                              FPREGINDEX(i)) :                                    \
                            REGINDEX(i))

/**
 * Starting special linkage index for linkages that have "special arguments"
 */
#define TR_FirstSpecialLinkageIndex  0x10


////////////////////////////////////////////////////////////////////////////////
//  TR::S390Linkage Definition
////////////////////////////////////////////////////////////////////////////////

// Normal Stack Pointer Register refers to the register defined by the
// ABI to hold the stack pointer
// Stack Pointer Register refers to the register through which most stack
// accesses occur.  Usually, but not always, they are the same.
namespace OMR
{
namespace Z
{
/**
 * This is the base class of all 390 linkages
 */
class OMR_EXTENSIBLE Linkage : public OMR::Linkage
   {
private:
   TR_LinkageConventions _linkageType;
   TR_S390LinkageConventions _explicitLinkageType;
   uint32_t _properties;
   uint32_t _registerFlags[TR::RealRegister::NumRegisters];
   uint8_t _numIntegerArgumentRegisters;
   TR::RealRegister::RegNum _intArgRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numFloatArgumentRegisters;
   TR::RealRegister::RegNum _floatArgRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numVectorArgumentRegisters;
   TR::RealRegister::RegNum _vectorArgRegisters[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegNum _integerReturnRegister;
   TR::RealRegister::RegNum _floatReturnRegister;
   TR::RealRegister::RegNum _doubleReturnRegister;
   TR::RealRegister::RegNum _longDoubleReturnRegister0;
   TR::RealRegister::RegNum _longDoubleReturnRegister2;
   TR::RealRegister::RegNum _longDoubleReturnRegister4;
   TR::RealRegister::RegNum _longDoubleReturnRegister6;
   TR::RealRegister::RegNum _vectorReturnRegister;
   TR::RealRegister::RegNum _longLowReturnRegister;
   TR::RealRegister::RegNum _longHighReturnRegister;
   TR::RealRegister::RegNum _longReturnRegister;
   TR::RealRegister::RegNum _entryPointRegister;
   TR::RealRegister::RegNum _litPoolRegister;
   TR::RealRegister::RegNum _staticBaseRegister;
   TR::RealRegister::RegNum _privateStaticBaseRegister;   ///< locked register for private WSA, if its allocated
   TR::RealRegister::RegNum _returnAddrRegister;
   TR::RealRegister::RegNum _vtableIndexArgumentRegister; ///< for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _j9methodArgumentRegister;    ///< for icallVMprJavaSendStatic

   int32_t _offsetToRegSaveArea;
   int32_t _offsetToLongDispSlot;
   int32_t _offsetToFirstParm;
   uint8_t _numberOfDependencyGPRegisters;
   int32_t _offsetToFirstLocal;
   bool    _stackSizeCheckNeeded;
   bool    _raContextSaveNeeded;
   bool    _raContextRestoreNeeded;
   int32_t _largestOutgoingArgumentAreaSize; ///< Arguments for registers could be saved in discontiguous area
                                             ///< this size does not include the discontiguous register parm area size
   int32_t _largestOutgoingArgumentAreaSize64;
protected:
   TR::RealRegister::RegNum _firstSaved;
   TR::RealRegister::RegNum _lastSaved;
   TR::RealRegister::RegNum _stackPointerRegister;

   static bool needsAlignment(TR::DataType dt, TR::CodeGenerator * cg);
   static int32_t getFirstMaskedBit(int16_t mask, int32_t from , int32_t to);
   static int32_t getLastMaskedBit(int16_t mask, int32_t from , int32_t to);
   static int32_t getFirstMaskedBit(int16_t mask);
   static int32_t getLastMaskedBit(int16_t mask);

public:

enum TR_DispatchType
   {
   TR_JNIDispatch            = 0,
   TR_DirectDispatch         = 1,
   TR_SystemDispatch         = 2,
   TR_NumDispatchTypes       = 3
   };

   Linkage(TR::CodeGenerator *);

   Linkage(TR::CodeGenerator *, TR_S390LinkageConventions, TR_LinkageConventions);

   TR_S390LinkageConventions getExplicitLinkageType() { return _explicitLinkageType; }
   void setExplicitLinkageType(TR_S390LinkageConventions lc ) { _explicitLinkageType = lc; }

   bool isOSLinkageType();
   bool isXPLinkLinkageType();
   bool isFastLinkLinkageType();

   bool isZLinuxLinkageType();

   bool    setStackSizeCheckNeeded(bool v) { return _stackSizeCheckNeeded = v; }
   bool    getStackSizeCheckNeeded() { return _stackSizeCheckNeeded; }

   bool    setRaContextSaveNeeded(bool v) { return _raContextSaveNeeded = v; }
   bool    getRaContextSaveNeeded() { return _raContextSaveNeeded; }

   bool    setRaContextRestoreNeeded(bool v) { return _raContextSaveNeeded = v; }
   bool    getRaContextRestoreNeeded() { return _raContextSaveNeeded; }

// Definitions from TR::Linkage
   virtual void createPrologue(TR::Instruction * cursor) = 0;
   virtual void createEpilogue(TR::Instruction * cursor) = 0;
   virtual void mapStack(TR::ResolvedMethodSymbol * symbol) = 0;
   virtual void mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t & stackIndex) = 0;
   virtual bool hasToBeOnStack(TR::ParameterSymbol * parm) = 0;
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method) = 0;
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol>&parmList) = 0;

   virtual TR::Instruction * loadUpArguments(TR::Instruction * cursor);
   virtual void removeOSCOnSavedArgument(TR::Instruction* instr, TR::Register* sReg, int32_t stackOffset);

   virtual void * saveArguments(void * cursor, bool genBinary, bool InPreProlog = false, int32_t frameOffset = 0, List<TR::ParameterSymbol> *parameterList=NULL);

   virtual void initS390RealRegisterLinkage() = 0;

   virtual void lockRegister(TR::RealRegister * lpReal);
   virtual void unlockRegister(TR::RealRegister * lpReal);

   virtual TR::Register * buildDirectDispatch(TR::Node * callNode) = 0;
   virtual TR::Register * buildIndirectDispatch(TR::Node * callNode) = 0;

   virtual void buildVirtualDispatch(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, TR::Register * vftReg, uint32_t sizeOfArguments)
      {
      TR_ASSERT(0, "ERROR: Empty declaration called");
      }

   public:

   virtual void replaceCallWithJumpInstruction(TR::Instruction *callInstruction);

   TR::InstOpCode::Mnemonic getOpCodeForLinkage(TR::Node * child, bool isStore, bool isRegReg);
   TR::InstOpCode::Mnemonic getRegCopyOpCodeForLinkage(TR::Node * child);
   TR::InstOpCode::Mnemonic getStoreOpCodeForLinkage(TR::Node * child);
   TR::InstOpCode::Mnemonic getLoadOpCodeForLinkage(TR::Node * child);
   TR::Register *getStackRegisterForOutgoingArguments(TR::Node *n, TR::RegisterDependencyConditions *dependencies);

   public:

   TR::Register *  copyArgRegister(TR::Node * callNode, TR::Node * child, TR::Register * argRegister);
   TR::Register *  pushLongArg32(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs,
       int32_t numFloatArgs, int32_t * stackOffsetPtr,
       TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister=NULL);
   TR::Register *  pushArg(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs,
       int32_t numFloatArgs, int32_t * stackOffsetPtr,
       TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister=NULL);
   TR::Register *  pushAggregateArg(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs,
       int32_t numFloatArgs, int32_t * stackOffsetPtr,
       TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister=NULL);
   TR::Register *  pushJNIReferenceArg(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs,
       int32_t numFloatArgs, int32_t * stackOffsetPtr,
       TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister=NULL);
   TR::Register *  pushVectorArg(TR::Node * callNode, TR::Node * child,  int32_t numVectorArgs, int32_t pindex,
       int32_t * stackOffsetPtr, TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister=NULL);
   virtual void doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask);
   virtual void addSpecialRegDepsForBuildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, int32_t& from, int32_t step){ return; }
   virtual int32_t storeExtraEnvRegForBuildArgs(TR::Node * callNode, TR::Linkage* linkage,
            TR::RegisterDependencyConditions * dependencies, bool isFastJNI, int32_t stackOffset, int8_t gprSize, uint32_t &numIntegerArgs) {return stackOffset;}
   virtual int64_t addFECustomizedReturnRegDependency(int64_t killMask, TR::Linkage* linkage, TR::DataType resType, TR::RegisterDependencyConditions * dependencies) {return killMask; }
   virtual int32_t buildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies, bool isFastJNI, int64_t killMask, TR::Register* &vftReg, bool PassReceiver = true);
   TR::Instruction * storeArgumentOnStack(TR::Node * callNode, TR::InstOpCode::Mnemonic opCode, TR::Register * argReg, int32_t *stackOffsetPtr, TR::Register* stackRegister);
   TR::Instruction * storeLongDoubleArgumentOnStack(TR::Node * callNode, TR::DataType argType, TR::InstOpCode::Mnemonic opCode, TR::Register * argReg, int32_t *stackOffsetPtr, TR::Register* stackRegister);
   void loadIntArgumentsFromStack(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies, TR::DataType argType, int32_t stackOffset, int32_t argsSize, int32_t numIntegerArgs, TR::Register* stackRegister);

   int32_t  isNeedsWidening()  { return _properties & NeedsWidening; }
   int32_t  isAllParmsOnStack()  { return _properties & AllParmsOnStack; }
   int32_t  isParmsInReverseOrder()  { return _properties & ParmsInReverseOrder; }
   int32_t  isSkipGPRsForFloatParms()  { return _properties & SkipGPRsForFloatParms; }
   int32_t  isPadFloatParms()  { return _properties & PadFloatParms; }
   int32_t  isTwoStackSlotsForLongAndDouble()  { return _properties & TwoStackSlotsForLongAndDouble; }
   int32_t  isAggregatesPassedInParmRegs() { return _properties & AggregatesPassedInParmRegs; }
   int32_t  isAggregatesPassedOnParmStack() { return _properties & AggregatesPassedOnParmStack; }
   int32_t  isAggregatesReturnedInRegs() { return _properties & AggregatesReturnedInRegs; }
   int32_t  isSmallIntParmsAlignedRight() { return _properties & SmallIntParmsAlignedRight; }
   virtual bool isAggregateReturnedInIntRegisters(int32_t aggregateLenth)   { return false; }
   virtual bool isAggregateReturnedInRegistersCall(TR::Node *callNode) { return false; }
   virtual bool isAggregateReturnedInIntRegistersAndMemory(int32_t aggregateLenth)   { return false; }
   virtual bool isAggregateReturnedInRegistersAndMemoryCall(TR::Node *callNode) { return false; }

   int32_t  isForceSaveIncomingParameters() { return _properties & ForceSaveIncomingParameters; }
   int32_t  isLongDoubleReturnedOnStorage() { return _properties & LongDoubleReturnedOnStorage; }
   int32_t  isLongDoublePassedOnStorage() { return _properties & LongDoublePassedOnStorage; }
   int32_t  isParmsMappedBeforeAutos() { return _properties & ParmsMappedBeforeAutos; }

   int64_t killAndAssignRegister(int64_t killMask, TR::RegisterDependencyConditions * deps,
      TR::Register ** virtualReg, TR::RealRegister::RegNum regNum,
      TR::CodeGenerator * codeGen, bool isAllocate=false, bool isDummy=false);

   int64_t killAndAssignRegister(int64_t killMask, TR::RegisterDependencyConditions * deps,
      TR::Register ** virtualReg, TR::RealRegister* realReg,
      TR::CodeGenerator * codeGen, bool isAllocate=false, bool isDummy=false);

   virtual void generateDispatchReturnLable(TR::Node * callNode, TR::CodeGenerator * codeGen,
			TR::RegisterDependencyConditions * &deps, TR::Register * javaReturnRegister,bool hasGlRegDeps, TR::Node *GlobalRegDeps);
   virtual TR::Register * buildNativeDispatch(TR::Node * callNode, TR_DispatchType dispatchType);
   virtual TR::Register * buildNativeDispatch(TR::Node * callNode, TR_DispatchType dispatchType,
		   TR::RegisterDependencyConditions * &deps, bool isFastJNI, bool isPassReceiver, bool isJNIGCPoint);

   // these are called by buildNativeDispatch
   virtual void setupRegisterDepForLinkage(TR::Node *, TR_DispatchType, TR::RegisterDependencyConditions * &, int64_t &, TR::SystemLinkage *, TR::Node * &, bool &, TR::Register **, TR::Register *&);
   virtual void setupBuildArgForLinkage(TR::Node *, TR_DispatchType, TR::RegisterDependencyConditions *, bool, bool, int64_t &, TR::Node *, bool, TR::SystemLinkage *);
   virtual void performCallNativeFunctionForLinkage(TR::Node *, TR_DispatchType, TR::Register *&, TR::SystemLinkage *, TR::RegisterDependencyConditions *&, TR::Register *, TR::Register *, bool);

   TR::Register * buildSystemLinkageDispatch(TR::Node * callNode);

   virtual int32_t setOffsetToRegSaveArea(int32_t savesize) { return _offsetToRegSaveArea = savesize; }
   virtual int32_t getOffsetToRegSaveArea() { return _offsetToRegSaveArea; }

   virtual int32_t getLargestOutgoingArgumentAreaSize() { return _largestOutgoingArgumentAreaSize; }
   virtual void setLargestOutgoingArgumentAreaSize(int32_t size) { _largestOutgoingArgumentAreaSize = size; }

   virtual int32_t getLargestOutgoingArgumentAreaSize64() { return _largestOutgoingArgumentAreaSize64; }
   virtual void setLargestOutgoingArgumentAreaSize64(int32_t size) { _largestOutgoingArgumentAreaSize64 = size; }

   virtual int32_t getOutgoingParameterBlockSize();

   int32_t  getOffsetToLongDispSlot() { return _offsetToLongDispSlot; }
   int32_t  setOffsetToLongDispSlot(int32_t offset) { return _offsetToLongDispSlot=offset; }

   virtual uint32_t setProperty (uint32_t p) { return _properties |= p; }
   virtual uint32_t setProperties            (uint32_t p){ return _properties = p; }
   virtual uint32_t getProperties()          { return _properties; }
   virtual uint32_t clearProperty (uint32_t p) { return _properties &= ~p; }
   uint32_t getRightToLeft() { return _properties & RightToLeft; }
   int32_t  isFirstParmAtFixedOffset()   { return _properties & FirstParmAtFixedOffset; }
   int32_t  isSplitLongParm()  { return _properties & SplitLongParm; }

   int32_t setRegisterFlag(TR::RealRegister::RegNum regNum, int32_t regFlag) {return _registerFlags[regNum]|= regFlag; }
   int32_t setRegisterFlags(TR::RealRegister::RegNum regNum, int32_t regFlags) {return _registerFlags[regNum]= regFlags; }
   int32_t resetRegisterFlags(TR::RealRegister::RegNum regNum, int32_t regFlags) {return _registerFlags[regNum]&= ~regFlags; }
   uint32_t* getRegisterFlags() { return _registerFlags; }

   uint32_t getPreserved(TR::RealRegister::RegNum regNum) { return _registerFlags[regNum] & Preserved;}
   uint32_t getIntegerArgument(TR::RealRegister::RegNum regNum) { return _registerFlags[regNum] & IntegerArgument; }
   uint32_t getFloatArgument(TR::RealRegister::RegNum regNum) { return _registerFlags[regNum] & FloatArgument; }
   uint32_t getIntegerReturn(TR::RealRegister::RegNum regNum) { return _registerFlags[regNum] & IntegerReturn; }
   uint32_t getFloatReturn(TR::RealRegister::RegNum regNum) { return _registerFlags[regNum] & FloatReturn; }
   uint32_t getIntegerArgumentAddToPost(TR::RealRegister::RegNum regNum) { return _registerFlags[regNum] & IntegerArgumentAddToPost; }
   void clearIntegerArgumentAddToPost(TR::RealRegister::RegNum regNum) { _registerFlags[regNum] &= ~IntegerArgumentAddToPost; }

   virtual uint8_t setNumIntegerArgumentRegisters(uint8_t n)  { return _numIntegerArgumentRegisters = n; }
   virtual uint8_t getNumIntegerArgumentRegisters()  { return _numIntegerArgumentRegisters; }

   virtual uint8_t setNumFloatArgumentRegisters  (uint8_t n)    { return _numFloatArgumentRegisters = n; }
   virtual uint8_t getNumFloatArgumentRegisters()    { return _numFloatArgumentRegisters; }

   virtual uint8_t setNumVectorArgumentRegisters  (uint8_t n)    { return _numVectorArgumentRegisters = n; }
   virtual uint8_t getNumVectorArgumentRegisters()               { return _numVectorArgumentRegisters; }

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind);

   void markPreservedRegsInBlock(int32_t);
   void markPreservedRegsInDep(TR::RegisterDependencyConditions *);


  // set the indexth integer argument register
   virtual void setIntegerArgumentRegister(uint32_t index, TR::RealRegister::RegNum r);

   // get the indexth integer argument register
   virtual TR::RealRegister::RegNum
   getIntegerArgumentRegister(uint32_t index)
      {
      if (index < _numIntegerArgumentRegisters)
         return _intArgRegisters[index];
      else
         return TR::RealRegister::NoReg;
      }

   /** Get the indexth Long High argument register */
   virtual TR::RealRegister::RegNum getLongHighArgumentRegister(uint32_t index);

   /** Set the indexth float argument register */
   virtual void setFloatArgumentRegister(uint32_t index, TR::RealRegister::RegNum r);

   /** Get the indexth float argument register */
   virtual TR::RealRegister::RegNum
   getFloatArgumentRegister(uint32_t index)
      {
      if (index < _numFloatArgumentRegisters)
         return _floatArgRegisters[index];
      else
         return TR::RealRegister::NoReg;
      }

   /** Set the indexth vector argument register */
    virtual void setVectorArgumentRegister(uint32_t index, TR::RealRegister::RegNum r);

   /** Get the indexth vector argument register */
   virtual TR::RealRegister::RegNum
   getVectorArgumentRegister(uint32_t index)
      {
      if (index < _numVectorArgumentRegisters)
         return _vectorArgRegisters[index];
      else
         return TR::RealRegister::NoReg;
      }

   virtual TR::RealRegister::RegNum setIntegerReturnRegister (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getIntegerReturnRegister()  { return _integerReturnRegister; }

   virtual TR::RealRegister::RegNum setLongLowReturnRegister (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongLowReturnRegister()  { return _longLowReturnRegister; }

   virtual TR::RealRegister::RegNum setLongHighReturnRegister(TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongHighReturnRegister() { return _longHighReturnRegister; }

   virtual TR::RealRegister::RegNum setLongReturnRegister    (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongReturnRegister()     { return _longReturnRegister; }

   virtual TR::RealRegister::RegNum setFloatReturnRegister   (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getFloatReturnRegister()    { return _floatReturnRegister; }

   virtual TR::RealRegister::RegNum setDoubleReturnRegister  (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getDoubleReturnRegister()   { return _doubleReturnRegister; }

   virtual TR::RealRegister::RegNum setLongDoubleReturnRegister0  (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongDoubleReturnRegister0()   { return _longDoubleReturnRegister0; }
   virtual TR::RealRegister::RegNum setLongDoubleReturnRegister2  (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongDoubleReturnRegister2()   { return _longDoubleReturnRegister2; }
   virtual TR::RealRegister::RegNum setLongDoubleReturnRegister4  (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongDoubleReturnRegister4()   { return _longDoubleReturnRegister4; }
   virtual TR::RealRegister::RegNum setLongDoubleReturnRegister6  (TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getLongDoubleReturnRegister6()   { return _longDoubleReturnRegister6; }
   virtual TR::RealRegister::RegNum setVectorReturnRegister(TR::RealRegister::RegNum r);
   virtual TR::RealRegister::RegNum getVectorReturnRegister()    { return _vectorReturnRegister; }

   //Special Registers
   virtual TR::RealRegister::RegNum setStackPointerRegister  (TR::RealRegister::RegNum r) { return _stackPointerRegister = r; }
   virtual TR::RealRegister::RegNum getStackPointerRegister()   { return _stackPointerRegister; }
   virtual TR::RealRegister *getStackPointerRealRegister();
   virtual TR::RealRegister::RegNum setStackPointerRegister  (uint32_t num, TR::RealRegister::RegNum r) { return _stackPointerRegister = r; }
   virtual TR::RealRegister::RegNum getStackPointerRegister(uint32_t num)   { return _stackPointerRegister; }
   virtual TR::RealRegister *getStackPointerRealRegister(uint32_t num);

   virtual TR::RealRegister::RegNum getNormalStackPointerRegister();
   virtual TR::RealRegister *getNormalStackPointerRealRegister();

   virtual TR::RealRegister::RegNum setEntryPointRegister    (TR::RealRegister::RegNum r) { return _entryPointRegister = r; }
   virtual TR::RealRegister::RegNum getEntryPointRegister()     { return _entryPointRegister; }
   virtual TR::RealRegister *getEntryPointRealRegister();

   virtual TR::RealRegister::RegNum setLitPoolRegister (TR::RealRegister::RegNum r)       { return _litPoolRegister = r; }
   virtual TR::RealRegister::RegNum getLitPoolRegister()        { return _litPoolRegister; }
   virtual TR::RealRegister *getLitPoolRealRegister();

   virtual TR::RealRegister::RegNum setStaticBaseRegister (TR::RealRegister::RegNum r)       { return _staticBaseRegister = r; }
   virtual TR::RealRegister::RegNum getStaticBaseRegister()        { return _staticBaseRegister; }
   virtual TR::RealRegister *getStaticBaseRealRegister();

   virtual TR::RealRegister::RegNum setPrivateStaticBaseRegister (TR::RealRegister::RegNum r)       { return _privateStaticBaseRegister = r; }
   virtual TR::RealRegister::RegNum getPrivateStaticBaseRegister()        { return _privateStaticBaseRegister; }
   virtual TR::RealRegister *getPrivateStaticBaseRealRegister();

   virtual TR::RealRegister::RegNum setReturnAddressRegister (TR::RealRegister::RegNum r) { return _returnAddrRegister = r; }
   virtual TR::RealRegister::RegNum getReturnAddressRegister()  { return _returnAddrRegister; }
   virtual TR::RealRegister *getReturnAddressRealRegister();

   virtual TR::RealRegister::RegNum setVTableIndexArgumentRegister(TR::RealRegister::RegNum r)  { return _vtableIndexArgumentRegister = r; }
   virtual TR::RealRegister::RegNum getVTableIndexArgumentRegister()  { return _vtableIndexArgumentRegister; }
   virtual TR::RealRegister *getVTableIndexArgumentRegisterRealRegister();

   virtual TR::RealRegister::RegNum setJ9MethodArgumentRegister(TR::RealRegister::RegNum r)  { return _j9methodArgumentRegister = r; }
   virtual TR::RealRegister::RegNum getJ9MethodArgumentRegister()  { return _j9methodArgumentRegister; }
   virtual TR::RealRegister *getJ9MethodArgumentRegisterRealRegister();

   virtual TR::RealRegister::RegNum getMethodMetaDataRegister() { return TR::RealRegister::NoReg; }
   virtual TR::RealRegister *getMethodMetaDataRealRegister()
      {
      TR_ASSERT(0, "MethodMetaDataRealRegister shouldn't be called from TR::Linkage");
      return NULL;
      }

   virtual TR::RealRegister::RegNum getENVPointerRegister() { return TR::RealRegister::NoReg; }
   virtual TR::RealRegister::RegNum getCAAPointerRegister() { return TR::RealRegister::NoReg; }
   virtual TR::RealRegister::RegNum getParentDSAPointerRegister() { return TR::RealRegister::NoReg; }

   virtual int32_t setOffsetToFirstParm   (int32_t offset)  { return _offsetToFirstParm = offset; }
   virtual int32_t getOffsetToFirstParm()    { return _offsetToFirstParm; }

   virtual uint32_t setOffsetToFirstLocal(uint32_t offset)  { return _offsetToFirstLocal = offset; }
   virtual uint32_t getOffsetToFirstLocal()        { return _offsetToFirstLocal; }

   virtual int32_t setNumberOfDependencyGPRegisters(int32_t n)    { return _numberOfDependencyGPRegisters = n; }
   virtual int32_t getNumberOfDependencyGPRegisters()    { return _numberOfDependencyGPRegisters; }

   virtual int32_t setupLiteralPoolRegister(TR::Snippet *firstSnippet) { return -1; }

   TR::RealRegister *  getRealRegister(TR::RealRegister::RegNum rNum);

   TR::RealRegister::RegNum getFirstSavedRegister(int32_t fromreg, int32_t toreg);
   TR::RealRegister::RegNum getLastSavedRegister(int32_t fromreg, int32_t toreg);
   TR::RealRegister::RegNum getFirstRestoredRegister(int32_t fromreg, int32_t toreg);
   TR::RealRegister::RegNum getLastRestoredRegister(int32_t fromreg, int32_t toreg);

   protected:

   TR::Instruction * getLastPrologueInstruction(){ return _lastPrologueInstr; }
   TR::Instruction * getFirstPrologueInstruction(){ return _firstPrologueInstr; }
   void setLastPrologueInstruction(TR::Instruction * cursor){ _lastPrologueInstr = cursor; }
   void setFirstPrologueInstruction(TR::Instruction * cursor){ _firstPrologueInstr = cursor; }
   TR::Instruction * restorePreservedRegs(TR::RealRegister::RegNum,
                               TR::RealRegister::RegNum, int32_t,
                               TR::Instruction *, TR::Node *, TR::RealRegister *, TR::MemoryReference *,
                               TR::RealRegister::RegNum);

private:

   TR::Instruction * _lastPrologueInstr;
   TR::Instruction * _firstPrologueInstr;
   };
}
}

#endif
