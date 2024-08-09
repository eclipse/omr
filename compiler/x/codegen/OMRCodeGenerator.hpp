/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_X86_CODEGENERATOR_INCL
#define OMR_X86_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace X86 { class CodeGenerator; } }
namespace OMR { typedef OMR::X86::CodeGenerator CodeGeneratorConnector; }
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "compile/Compilation.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/LabelSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/BitVector.hpp"
#include "infra/TRlist.hpp"
#include "infra/Assert.hpp"
#include "codegen/InstOpCode.hpp"
#include "x/codegen/X86Register.hpp"
#include "env/CompilerEnv.hpp"

#if defined(LINUX) || defined(OSX)
#include <sys/time.h>
#endif

#include "codegen/Instruction.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GCStackAtlas.hpp"

class TR_GCStackMap;
namespace TR { class X86ConstantDataSnippet; }
namespace TR { class X86DataSnippet; }
class TR_OutlinedInstructions;
namespace OMR { namespace X86 { class CodeGenerator; } }
namespace TR { class CodeGenerator; }
namespace TR { class MemoryReference; }
namespace TR { class X86ImmInstruction;         }
namespace TR { class X86LabelInstruction;       }
namespace TR { class X86MemTableInstruction;    }
namespace TR { class X86ScratchRegisterManager; }
namespace TR { class X86VFPSaveInstruction;     }
namespace TR { struct X86LinkageProperties; }

namespace TR {

class ClobberingInstruction
   {
   TR::Instruction *_instruction;
   TR::list<TR::Register*> _clobberedRegisters;

public:
   TR_ALLOC(TR_Memory::ClobberingInstruction)

   ClobberingInstruction(TR_Memory * m) : _instruction(0), _clobberedRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())) {};
   ClobberingInstruction(TR::Instruction *instr, TR_Memory * m) : _instruction(instr), _clobberedRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())) {}

   TR::Instruction *getInstruction() {return _instruction;}
   TR::Instruction *setInstruction(TR::Instruction *i) {return (_instruction = i);}
   TR::list<TR::Register*> &getClobberedRegisters() {return _clobberedRegisters;}
   void addClobberedRegister(TR::Register *reg) {_clobberedRegisters.push_front(reg);}
   };

}

typedef TR::ClobberingInstruction TR_ClobberingInstruction;

struct TR_BetterSpillPlacement
   {
   TR_ALLOC(TR_Memory::BetterSpillPlacement)

   TR_BetterSpillPlacement *_next;
   TR_BetterSpillPlacement *_prev;
   TR::Register *_virtReg;
   TR::Instruction *_branchInstruction;
   uint32_t _freeRealRegs;
   };

#define CPUID_SIGNATURE_STEPPING_MASK       0x0000000f
#define CPUID_SIGNATURE_MODEL_MASK          0x000000f0
#define CPUID_SIGNATURE_FAMILY_MASK         0x00000f00
#define CPUID_SIGNATURE_PROCESSOR_MASK      0x00003000
#define CPUID_SIGNATURE_EXTENDEDMODEL_MASK  0x000f0000
#define CPUID_SIGNATURE_EXTENDEDFAMILY_MASK 0x0ff00000

struct TR_X86ProcessorInfo
   {
   public:

   TR_ALLOC(TR_Memory::IA32ProcessorInfo)

   TR_X86ProcessorInfo() { reset(); }

   enum TR_X86ProcessorVendors
      {
      TR_AuthenticAMD                  = 0x01,
      TR_GenuineIntel                  = 0x02,
      TR_UnknownVendor                 = 0x04
      };

   bool enabledXSAVE()                     {return testFeatureFlags2(TR_OSXSAVE);}
   bool hasBuiltInFPU()                    {return testFeatureFlags(TR_BuiltInFPU);}
   bool supportsVirtualModeExtension()     {return testFeatureFlags(TR_VirtualModeExtension);}
   bool supportsDebuggingExtension()       {return testFeatureFlags(TR_DebuggingExtension);}
   bool supportsPageSizeExtension()        {return testFeatureFlags(TR_PageSizeExtension);}
   bool supportsRDTSCInstruction()         {return testFeatureFlags(TR_RDTSCInstruction);}
   bool hasModelSpecificRegisters()        {return testFeatureFlags(TR_ModelSpecificRegisters);}
   bool supportsPhysicalAddressExtension() {return testFeatureFlags(TR_PhysicalAddressExtension);}
   bool supportsMachineCheckException()    {return testFeatureFlags(TR_MachineCheckException);}
   bool supportsCMPXCHG8BInstruction()     {return testFeatureFlags(TR_CMPXCHG8BInstruction);}
   bool supportsCMPXCHG16BInstruction()    {return testFeatureFlags2(TR_CMPXCHG16BInstruction);}
   bool hasAPICHardware()                  {return testFeatureFlags(TR_APICHardware);}
   bool hasMemoryTypeRangeRegisters()      {return testFeatureFlags(TR_MemoryTypeRangeRegisters);}
   bool supportsPageGlobalFlag()           {return testFeatureFlags(TR_PageGlobalFlag);}
   bool hasMachineCheckArchitecture()      {return testFeatureFlags(TR_MachineCheckArchitecture);}
   bool supportsCMOVInstructions()         {return testFeatureFlags(TR_CMOVInstructions);}
   bool hasPageAttributeTable()            {return testFeatureFlags(TR_PageAttributeTable);}
   bool has36BitPageSizeExtension()        {return testFeatureFlags(TR_36BitPageSizeExtension);}
   bool hasProcessorSerialNumber()         {return testFeatureFlags(TR_ProcessorSerialNumber);}
   bool supportsCLFLUSHInstruction()       {return testFeatureFlags(TR_CLFLUSHInstruction);}
   bool supportsCLWBInstruction()          {return testFeatureFlags8(TR_CLWB);}
   bool supportsDebugTraceStore()          {return testFeatureFlags(TR_DebugTraceStore);}
   bool hasACPIRegisters()                 {return testFeatureFlags(TR_ACPIRegisters);}
   bool supportsMMXInstructions()          {return testFeatureFlags(TR_MMXInstructions);}
   bool supportsFastFPSavesRestores()      {return testFeatureFlags(TR_FastFPSavesRestores);}
   bool supportsSSE()                      {return testFeatureFlags(TR_SSE);}
   bool supportsSSE2()                     {return testFeatureFlags(TR_SSE2);}
   bool supportsSSE3()                     {return testFeatureFlags2(TR_SSE3);}
   bool supportsSSSE3()                    {return testFeatureFlags2(TR_SSSE3);}
   bool supportsSSE4_1()                   {return testFeatureFlags2(TR_SSE4_1);}
   bool supportsSSE4_2()                   {return testFeatureFlags2(TR_SSE4_2);}
   bool supportsAVX()                      {return testFeatureFlags2(TR_AVX) && enabledXSAVE();}
   bool supportsAVX2()                     {return testFeatureFlags8(TR_AVX2) && enabledXSAVE();}
   bool supportsAVX512F()                  {return testFeatureFlags8(TR_AVX512F) && enabledXSAVE();}
   bool supportsAVX512BW()                 {return testFeatureFlags8(TR_AVX512BW) && enabledXSAVE();}
   bool supportsAVX512DQ()                 {return testFeatureFlags8(TR_AVX512DQ) && enabledXSAVE();}
   bool supportsAVX512CD()                 {return testFeatureFlags8(TR_AVX512CD) && enabledXSAVE();}
   bool supportsAVX512VL()                 {return testFeatureFlags8(TR_AVX512VL) && enabledXSAVE();}
   bool supportsAVX512VBMI2()              {return testFeatureFlags10(TR_AVX512_VBMI2) && enabledXSAVE();}
   bool supportsAVX512BITALG()             {return testFeatureFlags10(TR_AVX512_BITALG) && enabledXSAVE();}
   bool supportsAVX512VPOPCNTDQ()          {return testFeatureFlags10(TR_AVX512_VPOPCNTDQ) && enabledXSAVE();}
   bool supportsBMI1()                     {return testFeatureFlags8(TR_BMI1) && enabledXSAVE();}
   bool supportsBMI2()                     {return testFeatureFlags8(TR_BMI2) && enabledXSAVE();}
   bool supportsFMA()                      {return testFeatureFlags2(TR_FMA) && enabledXSAVE();}
   bool supportsCLMUL()                    {return testFeatureFlags2(TR_CLMUL);}
   bool supportsAESNI()                    {return testFeatureFlags2(TR_AESNI);}
   bool supportsPOPCNT()                   {return testFeatureFlags2(TR_POPCNT);}
   bool supportsSelfSnoop()                {return testFeatureFlags(TR_SelfSnoop);}
   bool supportsTM()                       {return testFeatureFlags8(TR_RTM);}
   bool supportsHyperThreading()           {return testFeatureFlags(TR_HyperThreading);}
   bool supportsHLE()                      {return testFeatureFlags8(TR_HLE);}
   bool hasThermalMonitor()                {return testFeatureFlags(TR_ThermalMonitor);}

   bool supportsMFence()                   {return testFeatureFlags(TR_SSE2);}
   bool supportsLFence()                   {return testFeatureFlags(TR_SSE2);}
   bool supportsSFence()                   {return testFeatureFlags(TR_SSE | TR_MMXInstructions);}
   bool prefersMultiByteNOP()              {return getX86Architecture() && isGenuineIntel() && !isIntelPentium();}

   uint32_t getCPUStepping(uint32_t signature)       {return (signature & CPUID_SIGNATURE_STEPPING_MASK);}
   uint32_t getCPUModel(uint32_t signature)          {return (signature & CPUID_SIGNATURE_MODEL_MASK) >> 4;}
   uint32_t getCPUFamily(uint32_t signature)         {return (signature & CPUID_SIGNATURE_FAMILY_MASK) >> 8;}
   uint32_t getCPUProcessor(uint32_t signature)      {return (signature & CPUID_SIGNATURE_PROCESSOR_MASK) >> 12;}
   uint32_t getCPUExtendedModel(uint32_t signature)  {return (signature & CPUID_SIGNATURE_EXTENDEDMODEL_MASK) >> 16;}
   uint32_t getCPUExtendedFamily(uint32_t signature) {return (signature & CPUID_SIGNATURE_EXTENDEDFAMILY_MASK) >> 20;}

   bool isIntelPentium()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelPentium; }
   bool isIntelP6()             { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelP6; }
   bool isIntelPentium4()       { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelPentium4; }
   bool isIntelCore2()          { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelCore2; }
   bool isIntelTulsa()          { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelTulsa; }
   bool isIntelNehalem()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelNehalem; }
   bool isIntelWestmere()       { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelWestmere; }
   bool isIntelSandyBridge()    { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelSandyBridge; }
   bool isIntelIvyBridge()      { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelIvyBridge; }
   bool isIntelHaswell()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelHaswell; }
   bool isIntelBroadwell()      { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelBroadwell; }
   bool isIntelSkylake()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelSkylake; }
   bool isIntelCascadeLake()    { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelCascadeLake; }
   bool isIntelCooperLake()     { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelCooperLake; }
   bool isIntelIceLake()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelIceLake; }
   bool isIntelSapphireRapids() { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelSapphireRapids; }
   bool isIntelEmeraldRapids()  { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelEmeraldRapids; }

   bool isIntelOldMachine()     { return (isIntelPentium() || isIntelP6() || isIntelPentium4() || isIntelCore2() || isIntelTulsa() || isIntelNehalem()); }

   bool isAMDK6()               { return (_processorDescription & 0x000000fe) == TR_ProcessorAMDK5; } // accept either K5 or K6
   bool isAMDAthlonDuron()      { return (_processorDescription & 0x000000ff) == TR_ProcessorAMDAthlonDuron; }
   bool isAMDOpteron()          { return (_processorDescription & 0x000000ff) == TR_ProcessorAMDOpteron; }
   bool isAMD15h()              { return (_processorDescription & 0x000000ff) == TR_ProcessorAMDFamily15h; }

   bool isGenuineIntel() {return _vendorFlags.testAny(TR_GenuineIntel);}
   bool isAuthenticAMD() {return _vendorFlags.testAny(TR_AuthenticAMD);}

   bool requiresLFENCE() { return false; /* Dummy for now, we may need TR::InstOpCode::LFENCE in future processors*/}

   int32_t getX86Architecture() { return (_processorDescription & 0x000000ff);}

private:

   flags8_t   _vendorFlags;
   flags32_t  _featureFlags;   // cache feature flags for re-use
   flags32_t  _featureFlags2;  // cache feature flags 2 for re-use
   flags32_t  _featureFlags8;  // cache feature flags 8 for re-use
   flags32_t  _featureFlags10; // cache feature flags 10 for re-use

   uint32_t _processorDescription;

   friend class OMR::X86::CodeGenerator;

   /**
    * @brief Zero initalize all member variables
    *
    */
   void reset();

   /**
    * @brief Initialize all member variables
    *
    * @param force Force initialization even if it has already been performed
    */
   void initialize(bool force = false);

   /**
    * @brief testFlag Ensures that the feature being tested for exists in the mask
    *                 and then checks whether the feature is set in the flag. The
    *                 reason for this is to facilitate correctness checks for
    *                 relocatable compilations. In order for the compiler to use a
    *                 processor feature, the feature flag should be added to the
    *                 mask so that the processor validation code also accounts for
    *                 the use of said feature.
    *
    * @param flag     Either _featureFlags, _featureFlags2, or _featureFlags8
    * @param feature  The feature being tested for
    * @param mask     The mask returned by either getFeatureFlagsMask(),
    *                 getFeatureFlags2Mask(), or getFeatureFlags8Mask()
    *
    * @return         The result of flag.testAny(feature)
    */
   bool testFlag(flags32_t &flag, uint32_t feature, uint32_t mask)
      {
      TR_ASSERT_FATAL(feature & mask, "The %x feature needs to be added to the "
                                      "getFeatureFlagsMask (or variant) function "
                                      "for correctness in relocatable compiles!\n",
                      feature);

      return flag.testAny(feature);
      }

   /**
    * @brief testFeatureFlags Wrapper around testFlag
    *
    * @param feature          The feature being tested for
    *
    * @return                 The result of testFlag
    */
   bool testFeatureFlags(uint32_t feature)
      {
      return testFlag(_featureFlags, feature, getFeatureFlagsMask());
      }

   /**
    * @brief testFeatureFlags2 Wrapper around testFlag
    *
    * @param feature           The feature being tested for
    *
    * @return                  The result of testFlag
    */
   bool testFeatureFlags2(uint32_t feature)
      {
      return testFlag(_featureFlags2, feature, getFeatureFlags2Mask());
      }

   /**
    * @brief testFeatureFlags8 Wrapper around testFlag
    *
    * @param feature           The feature being tested for
    *
    * @return                  The result of testFlag
    */
   bool testFeatureFlags8(uint32_t feature)
      {
      return testFlag(_featureFlags8, feature, getFeatureFlags8Mask());
      }

   bool testFeatureFlags10(uint32_t feature)
      {
      return testFlag(_featureFlags10, feature, getFeatureFlags10Mask());
      }
   };

enum TR_PaddingProperties
   {
   TR_NoOpPadding       = 0,
   TR_AtomicNoOpPadding = 1,
   // Note--other possible property flags:
   // - Don't care about condition codes (eg. add eax,0 would be ok)
   // - Not executed (eg. int3 or eyecatcher would be ok)
   };

#define PADDING_TABLE_MAX_ENCODING_LENGTH 9

class TR_X86PaddingTable
   {
   public:

   TR_X86PaddingTable(uint8_t biggestEncoding, uint8_t*** encodings, flags8_t flags, uint16_t sibMask, uint16_t prefixMask):
      _biggestEncoding(biggestEncoding), _encodings(encodings), _flags(flags), _sibMask(sibMask), _prefixMask(prefixMask) {}

   enum { registerMatters=1 }; // for _flags

   uint8_t     _biggestEncoding;
   flags8_t    _flags;
   uint8_t***  _encodings;
   uint16_t    _sibMask;    // 2^n is set if the NOP with size n uses a SIB byte
   uint16_t    _prefixMask; // 2^n is set if the NOP with size n has a prefix byte
   };

struct TR_VFPState
   {
   TR::RealRegister::RegNum _register;
   int32_t _displacement;

   TR_VFPState():_register(TR::RealRegister::NoReg),_displacement(0){} // Assumes noReg is zero
   void operator =  (const TR_VFPState &other){ _register = other._register; _displacement = other._displacement; }
   bool operator == (const TR_VFPState &other) const { return _register == other._register && _displacement == other._displacement; }
   bool operator != (const TR_VFPState &other) const { return !operator==(other); }
   };

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

   public:

   TR::Linkage *createLinkage(TR_LinkageConventions lc);
   void beginInstructionSelection();
   void endInstructionSelection();

   static TR_X86ProcessorInfo &getX86ProcessorInfo();
   static void initializeX86TargetProcessorInfo(bool force = false) { getX86ProcessorInfo().initialize(force); }

   typedef enum
      {
      Backward = 0,
      Forward  = 1
      } RegisterAssignmentDirection;

   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);
   void doBinaryEncoding();


   /*
    * \brief
    *        Adds items describing cold code cache to RSSReport
    *
    * \param coldCode
    *        Starting address of the cold code cache
    *
    */
   void addItemsToRSSReport(uint8_t *coldCode);

   void doBackwardsRegisterAssignment(TR_RegisterKinds kindsToAssign, TR::Instruction *startInstruction, TR::Instruction *appendInstruction = NULL);

   bool hasComplexAddressingMode() { return true; }
   bool getSupportsBitOpCodes() { return true; }

   static bool getSupportsOpCodeForAutoSIMD(TR::CPU *cpu, TR::ILOpCode opcode);
   bool getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode);

   bool getSupportsEncodeUtf16LittleWithSurrogateTest();
   bool getSupportsEncodeUtf16BigWithSurrogateTest();

   virtual bool getSupportsBitPermute();

   bool supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol);

   static bool isILOpCodeSupported(TR::ILOpCodes);

   bool hasTMEvaluator()                       {return true;}

   int64_t getLargestNegConstThatMustBeMaterialized() { TR_ASSERT(0, "Not Implemented on x86"); return 0; }
   int64_t getSmallestPosConstThatMustBeMaterialized() { TR_ASSERT(0, "Not Implemented on x86"); return 0; }

   void performNonLinearRegisterAssignmentAtBranch(TR::X86LabelInstruction *branchInstruction, TR_RegisterKinds kindsToBeAssigned);
   void prepareForNonLinearRegisterAssignmentAtMerge(TR::X86LabelInstruction *mergeInstruction);

   void processClobberingInstructions(TR::ClobberingInstruction * clobInstructionCursor, TR::Instruction *instructionCursor);

   TR::Instruction *generateInterpreterEntryInstruction(TR::Instruction *procEntryInstruction);

   // different from evaluateNode in that it returns a clobberable register
   TR::Register *shortClobberEvaluate(TR::Node *node);
   TR::Register *intClobberEvaluate(TR::Node *node);
   virtual TR::Register *longClobberEvaluate(TR::Node *node)=0;

   TR::Register *floatClobberEvaluate(TR::Node *node);
   TR::Register *doubleClobberEvaluate(TR::Node *node);

   const TR::X86LinkageProperties &getProperties() { return *_linkageProperties; }

   RegisterAssignmentDirection getAssignmentDirection() {return _assignmentDirection;}
   RegisterAssignmentDirection setAssignmentDirection(RegisterAssignmentDirection d)
      {
      return (_assignmentDirection = d);
      }

   TR::RealRegister *getFrameRegister()                       {return _frameRegister;}
   TR::RealRegister *getMethodMetaDataRegister();

   bool allowGlobalRegisterAcrossBranch(TR::RegisterCandidate *, TR::Node *);

   void buildRegisterMapForInstruction(TR_GCStackMap *map);

   bool isReturnInstruction(TR::Instruction *instr);
   bool isBranchInstruction(TR::Instruction *instr);

   /**
    * @brief Computes the 32-bit displacement between the given direct branch instruction
    *        and either the target helper or a trampoline to reach the target helper
    *
    * @param[in] branchInstructionAddress : buffer address of the branch instruction
    * @param[in] helper : \c TR::SymbolReference of the target helper
    *
    * @return The 32-bit displacement for the branch instruction
    */
   int32_t branchDisplacementToHelperOrTrampoline(uint8_t *branchInstructionAddress, TR::SymbolReference *helper);

   /**
    * \brief Reserve space in the code cache for a specified number of trampolines.
    *
    * \param[in] numTrampolines : number of trampolines to reserve
    *
    * \return : none
    */
   void reserveNTrampolines(int32_t numTrampolines) { return; }

   /**
    * \brief Provides the number of trampolines in the current CodeCache that have
    *        been reserved for unpopulated interface PIC (IPIC) slots.
    *
    * \return The number of reserved IPIC trampolines.
    */
   int32_t getNumReservedIPICTrampolines() const { return _numReservedIPICTrampolines; }

   /**
    * \brief Updates the number of reserved IPIC trampolines in the current CodeCache.
    *
    * \param[in] n : number of reserved IPIC trampolines
    */
   void setNumReservedIPICTrampolines(int32_t n) { _numReservedIPICTrampolines = n; }

   /**
    * \brief Changes the current CodeCache to the provided CodeCache.
    *
    * \param[in] newCodeCache : the CodeCache to switch to
    */
   void switchCodeCacheTo(TR::CodeCache *newCodeCache);

   // Note: This leaves the code aligned in the specified manner.
   TR::Instruction *generateSwitchToInterpreterPrePrologue(TR::Instruction *prev, uint8_t alignment, uint8_t alignmentMargin);

   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart);
   void emitDataSnippets();
   bool hasDataSnippets() { return _dataSnippetList.empty() ? false : true; }
   uint32_t getDataSnippetsSize();

   TR::list<TR::Register*> &getSpilledIntRegisters() {return _spilledIntRegisters;}

   TR::list<TR::Register*> &getLiveDiscardableRegisters() {return _liveDiscardableRegisters;}
   void addLiveDiscardableRegister(TR::Register *reg);
   void removeLiveDiscardableRegister(TR::Register *reg);
   void clobberLiveDiscardableRegisters(TR::Instruction  *instr, TR::MemoryReference  *mr);

   bool canTransformUnsafeCopyToArrayCopy();

   using OMR::CodeGenerator::canNullChkBeImplicit;
   bool canNullChkBeImplicit(TR::Node *node);

   TR::list<TR::Register*> &getDependentDiscardableRegisters() {return _dependentDiscardableRegisters;}
   void addDependentDiscardableRegister(TR::Register *reg) {_dependentDiscardableRegisters.push_front(reg);}
   void clobberLiveDependentDiscardableRegisters(TR::ClobberingInstruction *instr, TR::Register *baseReg);
   void reactivateDependentDiscardableRegisters(TR::Register *baseReg);
   void deactivateDependentDiscardableRegisters(TR::Register *baseReg);

   TR::list<TR::ClobberingInstruction*> &getClobberingInstructions() {return _clobberingInstructions;}
   void addClobberingInstruction(TR::ClobberingInstruction *i)  {_clobberingInstructions.push_front(i);}

   TR::list<TR_OutlinedInstructions*> &getOutlinedInstructionsList() {return _outlinedInstructionsList;}

   TR_X86ScratchRegisterManager *generateScratchRegisterManager(int32_t capacity=7);

   bool supportsConstantRematerialization();
   bool supportsLocalMemoryRematerialization();
   bool supportsStaticMemoryRematerialization();
   bool supportsIndirectMemoryRematerialization();
   bool supportsAddressRematerialization();
   bool supportsXMMRRematerialization();

   TR::Instruction *setLastCatchAppendInstruction(TR::Instruction *i) {return (_lastCatchAppendInstruction=i);}
   TR::Instruction *getLastCatchAppendInstruction()                  {return _lastCatchAppendInstruction;}

   void saveBetterSpillPlacements(TR::Instruction *branchInstruction);
   void removeBetterSpillPlacementCandidate(TR::RealRegister *realReg);
   TR::Instruction *findBetterSpillPlacement(TR::Register *virtReg, int32_t realRegNum);

   int32_t getInstructionPatchAlignmentBoundary()       {return _instructionPatchAlignmentBoundary;}
   void setInstructionPatchAlignmentBoundary(int32_t p) {_instructionPatchAlignmentBoundary = p;}

   int32_t getPicSlotCount()       {return _PicSlotCount;}
   void setPicSlotCount(int32_t c) {_PicSlotCount = c;}
   int32_t incPicSlotCountBy(int32_t i) {return (_PicSlotCount += i);}

   int32_t getLowestCommonCodePatchingAlignmentBoundary() {return _lowestCommonCodePatchingAlignmentBoundary;}
   void setLowestCommonCodePatchingAlignmentBoundary(int32_t b) {_lowestCommonCodePatchingAlignmentBoundary = b;}

   // NOT NEEDED, overridden in amd64/i386
   bool internalPointerSupportImplemented() {return false;}

   bool supportsSinglePrecisionSQRT() {return true;}

   bool supportsComplexAddressing();

   bool getSupportsNewObjectAlignment() { return true; }
   bool getSupportsTenuredObjectAlignment() { return true; }
   bool isObjectOfSizeWorthAligning(uint32_t size)
      {
      uint32_t lineSize = 64;//getX86ProcessorInfo().getL1DataCacheLineSize();
                             //the query doesn't seem to work on AMD processors;
      return  ((size < (lineSize<<1)) && (size > (lineSize >> 2)));

      }

   int32_t getMaximumNumbersOfAssignableGPRs();
   int32_t getMaximumNumbersOfAssignableFPRs();
   int32_t getMaximumNumbersOfAssignableVRs();
   bool willBeEvaluatedAsCallByCodeGen(TR::Node *node, TR::Compilation *comp);

   uint8_t getSizeOfCombinedBuffer();

   bool supportsInliningOfIsInstance();

   bool supportsPassThroughCopyToNewVirtualRegister() { return true; }

   bool doRematerialization() {return true;}

   bool materializesHeapBase() { return false; }
   bool canFoldLargeOffsetInAddressing() { return true; }

   int32_t arrayInitMinimumNumberOfBytes()
      {
      if (OMR::X86::CodeGenerator::comp()->target().is64Bit()) return 12;
      return 8;
      }

   // codegen methods referenced from ras/
   //
   uint32_t estimateBinaryLength(TR::MemoryReference *);

   void apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);

   bool isAddressScaleIndexSupported(int32_t scale) { if (scale <= 8) return true; return false; }

   void addMetaDataForBranchTableAddress(uint8_t *target, TR::Node *caseNode, TR::X86MemTableInstruction *jmpTableInstruction);


   ///////////////////////////////////
   //
   // No-op generation
   //

   // Emits length bytes of padding starting at cursor with the given properties.
   // Returns the first byte beyond the padding.
   // neighborhood, if provided, provides a context that allows for a good
   // no-op instruction to be chosen based on the surrounding instructions.
   //
   virtual uint8_t *generatePadding(uint8_t              *cursor,
                                    intptr_t             length,
                                    TR::Instruction    *neighborhood=NULL,
                                    TR_PaddingProperties  properties=TR_NoOpPadding,
                                    bool                  recursive=false);

   // Returns the number of bytes of padding required to cause cursor to be
   // aligned to the given boundary, minus the margin.
   // Specifically, cursor + margin + alignment(cursor, boundary, margin) will
   // be a multiple of boundary.
   //
   intptr_t alignment(intptr_t cursor, intptr_t boundary, intptr_t margin=0)
      {
      TR_ASSERT((boundary & (boundary-1)) == 0, "Can only align to powers of 2");
      return (-cursor - margin) & (boundary-1);
      }
   intptr_t alignment(void *cursor, intptr_t boundary, intptr_t margin=0);

   bool patchableRangeNeedsAlignment(void *cursor, intptr_t length, intptr_t boundary, intptr_t margin=0);

   bool nopsAlsoProcessedByRelocations() { return false; }

#if defined(DEBUG)
   void dumpPreFPRegisterAssignment(TR::Instruction *);
   void dumpPostFPRegisterAssignment(TR::Instruction *, TR::Instruction *);
   void dumpPreGPRegisterAssignment(TR::Instruction *);
   void dumpPostGPRegisterAssignment(TR::Instruction *, TR::Instruction *);
#endif

   void dumpDataSnippets(TR::FILE *pOutFile);

   TR::X86ConstantDataSnippet *findOrCreate2ByteConstant(TR::Node *, int16_t c);
   TR::X86ConstantDataSnippet *findOrCreate4ByteConstant(TR::Node *, int32_t c);
   TR::X86ConstantDataSnippet *findOrCreate8ByteConstant(TR::Node *, int64_t c);
   TR::X86ConstantDataSnippet *findOrCreate16ByteConstant(TR::Node *, void *c);
   TR::X86DataSnippet *create2ByteData(TR::Node *, int16_t c);
   TR::X86DataSnippet *create4ByteData(TR::Node *, int32_t c);
   TR::X86DataSnippet *create8ByteData(TR::Node *, int64_t c);
   TR::X86DataSnippet *create16ByteData(TR::Node *, void *c);

   bool considerTypeForGRA(TR::Node *node);
   bool considerTypeForGRA(TR::DataType dt);
   bool considerTypeForGRA(TR::SymbolReference *symRef);

   /*
    * \brief move out-of-line instructions from cold code to warm
    *
    */
   void moveOutOfLineInstructionsToWarmCode();

   uint32_t getOutOfLineCodeSize();

   /*
    * \brief create a data snippet.
    *
    * \param[in] node : the node which this data snippet belongs to
    * \param[in] data : a pointer to initial data or NULL for skipping initialization
    * \param[in] size : the size of this data snippet
    *
    * \return : a data snippet with specified size
    */
   TR::X86DataSnippet* createDataSnippet(TR::Node* node, void* data, size_t size);
   /*
    * \brief create a data snippet
    *
    * \param[in] node : the node which this data snippet belongs to
    * \param[in] data : the data which this data snippet holds
    *
    * \return : a data snippet containing one type T element
    */
   template<typename T> inline TR::X86DataSnippet* createDataSnippet(TR::Node* node, T data) { return createDataSnippet(node, &data, sizeof(data)); }
   /*
    * \brief find or create a constant data snippet.
    *
    * \param[in] node : the node which this constant data snippet belongs to
    * \param[in] data : a pointer to initial data or NULL for skipping initialization
    * \param[in] size : the size of this constant data snippet
    *
    * \return : a constant data snippet with specified size
    */
   TR::X86ConstantDataSnippet* findOrCreateConstantDataSnippet(TR::Node* node, void* data, size_t size);
   /*
    * \brief find or create a constant data snippet.
    *
    * \param[in] node : the node which this constant data snippet belongs to
    * \param[in] data : the data which this constant data snippet holds
    *
    * \return : a constant data snippet containing one type T element
    */
   template<typename T> inline TR::X86ConstantDataSnippet* findOrCreateConstantDataSnippet(TR::Node* node, T data) { return findOrCreateConstantDataSnippet(node, &data, sizeof(data)); }

   // The core "clobberEvaluate" logic for single registers (not register
   // pairs), parameterized by the opcode used to move the desired value into a
   // clobberable register if necessary
   TR::Register *gprClobberEvaluate(TR::Node *node, TR::InstOpCode::Mnemonic movRegRegOpCode);

   TR_OutlinedInstructions *findOutlinedInstructionsFromLabel(TR::LabelSymbol *label);
   TR_OutlinedInstructions *findOutlinedInstructionsFromMergeLabel(TR::LabelSymbol *label);

   const TR_VFPState         &vfpState()           { return _vfpState; }
   TR::X86VFPSaveInstruction  *vfpResetInstruction(){ return _vfpResetInstruction; }
   void setVFPState(const TR_VFPState &newState){ _vfpState = newState; }
   void initializeVFPState(TR::RealRegister::RegNum reg, int32_t displacement){ _vfpState._register = reg; _vfpState._displacement = displacement; }


   void removeUnavailableRegisters(TR::RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters);

   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm);

   virtual uint8_t nodeResultGPRCount  (TR::Node *node, TR_RegisterPressureState *state);

   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return &_globalRegisterBitVectors[kind]; }

   virtual void simulateNodeEvaluation (TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);

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

   void initialize();

protected:

   CodeGenerator(TR::Compilation *comp);

   // Note: the following should be called by subclasses near the end of their
   // constructors.  This breaks a rather nasty cyclic initialization dependency,
   // but it is pretty ugly.
   // TODO: Figure out a cleaner solution for this.
   void initializeX86(TR::Compilation *comp);

   TR::RealRegister::RegNum pickNOPRegister(TR::Instruction  *successor);

   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];

   static uint8_t k8PaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH];
   static uint8_t intelMultiBytePaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH];
   static uint8_t old32BitPaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH];

   static TR_X86PaddingTable _intelMultiBytePaddingTable;
   static TR_X86PaddingTable _K8PaddingTable;
   static TR_X86PaddingTable _old32BitPaddingTable;

   TR::X86ImmInstruction *_returnTypeInfoInstruction;
   const TR::X86LinkageProperties  *_linkageProperties;

   TR_VFPState                     _vfpState;
   TR::X86VFPSaveInstruction       *_vfpResetInstruction;

   TR::vector<TR::X86DataSnippet*>       _dataSnippetList;

   private:

   bool nodeIsFoldableMemOperand(TR::Node *node, TR::Node *parent, TR_RegisterPressureState *state);

   TR::RealRegister             *_frameRegister;

   TR::Instruction                 *_lastCatchAppendInstruction;
   TR_BetterSpillPlacement        *_betterSpillPlacements;

   TR::list<TR::Register*>               _spilledIntRegisters;
   TR::list<TR::Register*>               _liveDiscardableRegisters;
   TR::list<TR::Register*>               _dependentDiscardableRegisters;
   TR::list<TR::ClobberingInstruction*>  _clobberingInstructions;
   std::list<TR::ClobberingInstruction*, TR::typed_allocator<TR::ClobberingInstruction*, TR::Allocator> >::iterator _clobIterator;
   TR::list<TR_OutlinedInstructions*>    _outlinedInstructionsList;

   RegisterAssignmentDirection     _assignmentDirection;

   int32_t                         _instructionPatchAlignmentBoundary;
   int32_t                         _PicSlotCount;
   int32_t                         _lowestCommonCodePatchingAlignmentBoundary;

   TR_X86PaddingTable             *_paddingTable;

   TR::LabelSymbol                  *_switchToInterpreterLabel;

   TR::InstOpCode::Mnemonic                   _xmmDoubleLoadOpCode;

   int32_t _numReservedIPICTrampolines; ///< number of reserved IPIC trampolines

   enum TR_X86CodeGeneratorFlags
      {
      EnableBetterSpillPlacements              = 0x00000001, ///< use better spill placements
      EnableRematerialisation                  = 0x00000002, ///< use register rematerialisation
      EnableRegisterAssociations               = 0x00000004, ///< use register associations for register assignment
      EnableSinglePrecisionMethods             = 0x00000008, ///< support changing FPCW to single precision for individual methods
      EnableRegisterInterferences              = 0x00000010, ///< consider register interferences during register assignment
      EnableRegisterWeights                    = 0x00000020, ///< use register weights in choosing a best register candidate
      // Available                             = 0x00000040,
      // Available                             = 0x00000080,
      EnableImplicitDivideCheck                = 0x00000100, ///< platform can catch hardware exceptions for divide overflow and divide by zero
      GenerateMasmListingSyntax                = 0x00000200, ///< generate Masm-style syntax in the debug listings
      MapAutosTo8ByteSlots                     = 0x00000400, ///< don't round up sizes of autos to an 8-byte slot size when the stack is mapped
      EnableTLHPrefetching                     = 0x00000800, ///< enable software prefetches on TLH allocates
      // Available                             = 0x00001000,
      // Available                             = 0x00002000,
      // Available                             = 0x00004000,
      MethodEnterExitTracingEnabled            = 0x00008000, ///< trace method enter/exits
      // Available                             = 0x00010000,
      PushPreservedRegisters                   = 0x00020000  ///< we've chosen to save/restore preserved regs using push/pop instructions instead of movs
      };

   flags32_t _flags;

   public:

   bool enableBetterSpillPlacements()
      {
      return _flags.testAny(EnableBetterSpillPlacements);
      }
   void setEnableBetterSpillPlacements() {_flags.set(EnableBetterSpillPlacements);}
   void resetEnableBetterSpillPlacements() {_flags.reset(EnableBetterSpillPlacements);}

   bool enableRematerialisation()
      {
      return _flags.testAny(EnableRematerialisation);
      }
   void setEnableRematerialisation() {_flags.set(EnableRematerialisation);}
   void resetEnableRematerialisation() {_flags.reset(EnableRematerialisation);}

   bool enableSinglePrecisionMethods()
      {
      return _flags.testAny(EnableSinglePrecisionMethods);
      }
   void setEnableSinglePrecisionMethods() {_flags.set(EnableSinglePrecisionMethods);}

   bool enableRegisterWeights()
      {
      return _flags.testAny(EnableRegisterWeights);
      }
   void setEnableRegisterWeights() {_flags.set(EnableRegisterWeights);}

   bool enableRegisterInterferences()
      {
      return _flags.testAny(EnableRegisterInterferences);
      }
   void setEnableRegisterInterferences() {_flags.set(EnableRegisterInterferences);}

   bool enableRegisterAssociations()
      {
      return _flags.testAny(EnableRegisterAssociations);
      }
   void setEnableRegisterAssociations() {_flags.set(EnableRegisterAssociations);}

   bool enableTLHPrefetching()
      {
      return _flags.testAny(EnableTLHPrefetching);
      }
   void setEnableTLHPrefetching() {_flags.set(EnableTLHPrefetching);}

   bool generateMasmListingSyntax()    {return _flags.testAny(GenerateMasmListingSyntax);}
   void setGenerateMasmListingSyntax() {_flags.set(GenerateMasmListingSyntax);}

   bool codegenSupportsUnsignedIntegerDivide() {return true;}

   virtual bool supportsDirectJNICallsForAOT() { return true; }

   bool enableImplicitDivideCheck()    { return _flags.testAny(EnableImplicitDivideCheck); }
   void setEnableImplicitDivideCheck() { _flags.set(EnableImplicitDivideCheck); }

   bool mapAutosTo8ByteSlots()     { return _flags.testAny(MapAutosTo8ByteSlots); }
   void setMapAutosTo8ByteSlots()  { _flags.set(MapAutosTo8ByteSlots); }

   TR::X86ImmInstruction* getReturnTypeInfoInstruction() { return _returnTypeInfoInstruction; }

   TR::InstOpCode::Mnemonic getXMMDoubleLoadOpCode() {return _xmmDoubleLoadOpCode;}
   void setXMMDoubleLoadOpCode(TR::InstOpCode::Mnemonic o) {_xmmDoubleLoadOpCode = o;}

   TR::LabelSymbol *getSwitchToInterpreterLabel() {return _switchToInterpreterLabel;}
   void setSwitchToInterpreterLabel(TR::LabelSymbol *s) {_switchToInterpreterLabel = s;}

   bool methodEnterExitTracingEnabled()
      {
      return _flags.testAny(MethodEnterExitTracingEnabled);
      }
   void setMethodEnterExitTracingEnabled() {_flags.set(MethodEnterExitTracingEnabled);}

   bool pushPreservedRegisters()
      { return _flags.testAny(PushPreservedRegisters); }
   void setPushPreservedRegisters() {_flags.set(PushPreservedRegisters);}

   // arrayTranslateTableRequiresAlignment is N/A, since we don't have
   // arraytranslate tables.
   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget);
   int32_t arrayTranslateAndTestMinimumNumberOfIterations();
   };

}

}

#define IS_32BIT_RIP(x,rip)  ((intptr_t)(x) == (intptr_t)(rip) + (int32_t)((intptr_t)(x) - (intptr_t)(rip)))

class TR_X86ScratchRegisterManager: public TR_ScratchRegisterManager
   {
   public:

   TR_X86ScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg) : TR_ScratchRegisterManager(capacity, cg){}
   bool reclaimAddressRegister(TR::MemoryReference *mr);
   };

#endif
