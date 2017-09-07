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

#ifndef OMR_Z_MACHINEBASE_INCL
#define OMR_Z_MACHINEBASE_INCL
/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINEBASE_CONNECTOR
#define OMR_MACHINEBASE_CONNECTOR
namespace OMR {namespace Z { class Machine; } }
namespace OMR { typedef OMR::Z::Machine MachineConnector; }
#else
#error OMR::Z::Machine expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRMachine.hpp"

#include <string.h>                  // for memset
#include "codegen/RealRegister.hpp"  // for RealRegister, etc
#include "il/DataTypes.hpp"          // for CONSTANT64, etc
#include "infra/Flags.hpp"           // for flags32_t
#include "infra/TRlist.hpp"

class TR_Debug;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { namespace Z { class MachineBase; } }
template <class T> class TR_Stack;
template <typename ListKind> class List;

////////////////////////////////////////////////////////////////////////////////
// Misc constant value definitions
////////////////////////////////////////////////////////////////////////////////

// Number of machine registers
#define NUM_S390_GPR 16
#define NUM_S390_HPR 16
#define NUM_S390_FPR 16
#define NUM_S390_VRF 16 ///< 32 after full RA complete
#define NUM_S390_AR  16
#define NUM_S390_FPR_PAIRS 8

/** Max. displacement */
#define MAXDISP      4096

// Min/Max. long displacement
#define MAXLONGDISP      +524287 // 0x7FFFF
#define MINLONGDISP      -524288 // 0x80000

// single byte Immediate field limit
#define MAX_UNSIGNED_IMMEDIATE_BYTE_VAL    0xFF
#define MAX_IMMEDIATE_BYTE_VAL   	       127           // 0x7f
#define MIN_IMMEDIATE_BYTE_VAL  	         -128           // 0x80

#define MAX_RELOCATION_VAL              65535
#define MIN_RELOCATION_VAL             -65536
#define MAX_12_RELOCATION_VAL              4095
#define MIN_12_RELOCATION_VAL             -4096
#define MAX_24_RELOCATION_VAL              16777215
#define MIN_24_RELOCATION_VAL             -16777216

// Immediate field limit
#define MAX_UNSIGNED_IMMEDIATE_VAL    0xFFFF
#define MAX_IMMEDIATE_VAL   	       32767         // 0x7fff
#define MIN_IMMEDIATE_VAL  	      -32768         // 0x8000

// LL: Maximum Immediate field limit for Golden Eagle
#define GE_MAX_IMMEDIATE_VAL          (int64_t)CONSTANT64(0x000000007FFFFFFF)
#define GE_MIN_IMMEDIATE_VAL          (int64_t)CONSTANT64(0xFFFFFFFF80000000)

// Maximum unsigned immediate field limits for Golden Eagle
#define GE_MAX_UNSIGNED_IMMEDIATE_VAL           (int64_t)CONSTANT64(0x00000000FFFFFFFF)
#define GE_MIN_UNSIGNED_IMMEDIATE_VAL           (int64_t)CONSTANT64(0x0000000000000000)


// Constants for assignBestRegPair()
#define NOBOOKKEEPING false
#define BOOKKEEPING   true

// Constants defining whether we allow blocked regs as legal
#define ALLOWBLOCKED     true
#define DISALLOWBLOCKED  false
#define ALLOWLOCKED      true
#define DISALLOWBLOCKED  false

#define  REAL_REGISTER(ri)  machine->getS390RealRegister(ri)

#define GLOBAL_REG_FOR_LITPOOL     3    // GPR6

#define TR_LONG_TO_PACKED_SIZE            16    ///< The result byte size in memory for a long to packed conversion (i.e. with CVDG)
#define TR_INTEGER_TO_PACKED_SIZE         8     ///< The result byte size in memory for a integer to packed conversion (i.e. with CVD)
#define TR_PACKED_TO_LONG_SIZE            16    ///< The source byte size in memory for a packed to long conversion (i.e. with CVBG)
#define TR_PACKED_TO_INTEGER_SIZE         8     ///< The source byte size in memory for a packed to integer conversion (i.e. with CVB)
#define TR_ASCII_TO_PACKED_RESULT_SIZE    16    ///< The result byte size in memory for an ascii to packed conversion (i.e. with PKA)
#define TR_UNICODE_TO_PACKED_RESULT_SIZE  16    ///< The result byte size in memory for a unicode to packed conversion (i.e. with PKU)
#define TR_PACKED_TO_UNICODE_SOURCE_SIZE  16    ///< The source byte size in memory for a packed to unicode conversion (i.e. with UNPKU)

#define TR_PACKED_TO_DECIMAL_FLOAT_SIZE         8  ///< The source byte size in memory for a pd2df conversion (i.e. with CDSTR -- pd2df and pd2dd use same instruction)
#define TR_PACKED_TO_DECIMAL_FLOAT_PRECISION    15

#define TR_PACKED_TO_DECIMAL_DOUBLE_SIZE        8  ///< The source byte size in memory for a pd2dd conversion (i.e. with CDSTR)
#define TR_PACKED_TO_DECIMAL_DOUBLE_PRECISION   15

#define TR_PACKED_TO_DECIMAL_LONG_DOUBLE_SIZE       16 ///< The source byte size in memory for a pd2de conversion (i.e. with CXSTR)
#define TR_PACKED_TO_DECIMAL_LONG_DOUBLE_PRECISION  31

#define TR_PACKED_TO_DECIMAL_FLOAT_SIZE_ARCH11         9  ///< The source byte size in memory for a pd2df conversion (i.e. with CDPT -- pd2df and pd2dd use same instruction)
#define TR_PACKED_TO_DECIMAL_FLOAT_PRECISION_ARCH11    16 ///< NOTE: Although max size is 9 bytes only 16 and not 17 digits can be converted by CDPT to DFP -- df2pd and dd2pd use same instruction

#define TR_PACKED_TO_DECIMAL_DOUBLE_SIZE_ARCH11        9  ///< The source byte size in memory for a pd2dd conversion (i.e. with CDPT)
#define TR_PACKED_TO_DECIMAL_DOUBLE_PRECISION_ARCH11   16 ///< NOTE: Although max size is 9 bytes only 16 and not 17 digits can be converted by CDPT to DFP

#define TR_PACKED_TO_DECIMAL_LONG_DOUBLE_SIZE_ARCH11        18 ///< The source byte size in memory for a pd2de conversion (i.e. with CXPT)
#define TR_PACKED_TO_DECIMAL_LONG_DOUBLE_PRECISION_ARCH11   34 ///< NOTE: Although max size is 18 bytes only 34 and not 35 digits can be converted by CDPT to DFP

#define TR_DECIMAL_FLOAT_TO_PACKED_SIZE         8  ///< The destination byte size in memory for a df2pd conversion (i.e. with CSDTR -- df2pd and dd2pd use same instruction)
#define TR_DECIMAL_FLOAT_TO_PACKED_PRECISION    15

#define TR_DECIMAL_DOUBLE_TO_PACKED_SIZE        8  ///< The destination byte size in memory for a dd2pd conversion (i.e. with CSDTR)
#define TR_DECIMAL_DOUBLE_TO_PACKED_PRECISION   15

#define TR_DECIMAL_LONG_DOUBLE_TO_PACKED_SIZE      16 ///< The destination byte size in memory for a de2pd conversion (i.e. with CSXTR)
#define TR_DECIMAL_LONG_DOUBLE_TO_PACKED_PRECISION 31

#define TR_DECIMAL_FLOAT_TO_PACKED_SIZE_ARCH11        9  ///< The destination byte size in memory for a df2pd conversion (i.e. with CPDT -- df2pd and dd2pd use same instruction)
#define TR_DECIMAL_FLOAT_TO_PACKED_PRECISION_ARCH11   16  ///< NOTE: Although max size is 9 bytes only 16 and not 17 digits can be converted by CPDT -- df2pd and dd2pd use same instruction

#define TR_DECIMAL_DOUBLE_TO_PACKED_SIZE_ARCH11       9  ///< The destination byte size in memory for a dd2pd conversion (i.e. with CPDT)
#define TR_DECIMAL_DOUBLE_TO_PACKED_PRECISION_ARCH11  16  ///< NOTE: Although max size is 9 bytes only 16 and not 17 digits can be converted by CPDT

#define TR_DECIMAL_LONG_DOUBLE_TO_PACKED_SIZE_ARCH11        18 ///< The destination byte size in memory for a de2pd conversion (i.e. with CPXT)
#define TR_DECIMAL_LONG_DOUBLE_TO_PACKED_PRECISION_ARCH11   34 ///< NOTE: Although max size is 18 bytes only 34 and not 35 digits can be converted by CPXT

#define TR_ZONED_TO_DECIMAL_FLOAT_MAX_PRECISION       16    ///< CDZT - same as DecimalDouble
#define TR_ZONED_TO_DECIMAL_DOUBLE_MAX_PRECISION      16    ///< CDZT
#define TR_ZONED_TO_DECIMAL_LONG_DOUBLE_MAX_PRECISION 34    ///< CXZT

#define TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION        16   ///< CZDT - same as DecimalDouble
#define TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION       16   ///< CZDT
#define TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION  34   ///< CZXT

#define TR_ZONED_TO_DECIMAL_FLOAT_MAX_SIZE       16    ///< CDZT - same as DecimalDouble
#define TR_ZONED_TO_DECIMAL_DOUBLE_MAX_SIZE      16    ///< CDZT
#define TR_ZONED_TO_DECIMAL_LONG_DOUBLE_MAX_SIZE 34    ///< CXZT

#define TR_DECIMAL_FLOAT_TO_ZONED_MAX_SIZE        16   ///< CZDT - same as DecimalDouble
#define TR_DECIMAL_DOUBLE_TO_ZONED_MAX_SIZE       16   ///< CZDT
#define TR_DECIMAL_LONG_DOUBLE_TO_ZONED_SIZE      34   ///< CZXT

#define TR_DECIMAL_FLOAT_BIAS       101
#define TR_DECIMAL_DOUBLE_BIAS      398
#define TR_DECIMAL_LONG_DOUBLE_BIAS 6176

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine Definition
////////////////////////////////////////////////////////////////////////////////
namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE Machine : public OMR::Machine
   {
   TR::RealRegister            *_registerFile[TR::RealRegister::NumRegisters];
   TR::Register                *_registerAssociations[TR::RealRegister::NumRegisters];
   uint32_t                     _globalRegisterNumberToRealRegisterMap[NUM_S390_GPR+NUM_S390_FPR+NUM_S390_VRF+NUM_S390_HPR];
   TR::RealRegister::RegNum     _S390FirstOfFPRegisterPairs[NUM_S390_FPR_PAIRS];
   TR::RealRegister::RegNum     _S390SecondOfFPRegisterPairs[NUM_S390_FPR_PAIRS];

   uint16_t                     _registerFlagsSnapShot[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegState   _registerStatesSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_registerAssociationsSnapShot[TR::RealRegister::NumRegisters];
   bool                         _registerAssignedSnapShot[TR::RealRegister::NumRegisters];
   bool                         _registerAssignedHighSnapShot[TR::RealRegister::NumRegisters];
   uint16_t                     _registerWeightSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_assignedRegisterSnapShot[TR::RealRegister::NumRegisters];
   uint32_t                     _globalRegisterNumberToRealRegisterMapSnapShot[TR::RealRegister::NumRegisters];
   bool                         _containsHPRSpillSnapShot[TR::RealRegister::NumRegisters];

   /** Used to keep track of blocked registers (HPR/GPR) that upgrades/spill's etc should not use. Typical stores ~0-3 registers. */
   TR_Stack<TR::RealRegister *>               *_blockedUpgradedRegList;

   TR::CodeGenerator *_cg;

   TR_GlobalRegisterNumber  _firstGlobalAccessRegisterNumber;
   TR_GlobalRegisterNumber  _lastGlobalAccessRegisterNumber;
   TR_GlobalRegisterNumber  _firstGlobalGPRRegisterNumber;
   TR_GlobalRegisterNumber  _lastGlobalGPRRegisterNumber;
   TR_GlobalRegisterNumber  _firstGlobalHPRRegisterNumber;
   TR_GlobalRegisterNumber  _lastGlobalHPRRegisterNumber;
   TR_GlobalRegisterNumber  _last8BitGlobalGPRRegisterNumber;
   TR_GlobalRegisterNumber  _firstGlobalFPRRegisterNumber;
   TR_GlobalRegisterNumber  _firstOverlappedGlobalFPRRegisterNumber;
   TR_GlobalRegisterNumber  _lastGlobalFPRRegisterNumber;
   TR_GlobalRegisterNumber  _lastOverlappedGlobalFPRRegisterNumber;
   TR_GlobalRegisterNumber  _firstGlobalVRFRegisterNumber;
   TR_GlobalRegisterNumber  _firstOverlappedGlobalVRFRegisterNumber;
   TR_GlobalRegisterNumber  _lastGlobalVRFRegisterNumber;
   TR_GlobalRegisterNumber  _lastOverlappedGlobalVRFRegisterNumber;
   TR_GlobalRegisterNumber  _lastGlobalCCRRegisterNumber;
   TR_GlobalRegisterNumber  _lastVolatileNonLinkGPR;
   TR_GlobalRegisterNumber  _lastLinkageGPR;
   TR_GlobalRegisterNumber  _lastVolatileNonLinkFPR;
   TR_GlobalRegisterNumber  _lastLinkageFPR;
   TR_GlobalRegisterNumber  _globalEnvironmentRegisterNumber;
   TR_GlobalRegisterNumber  _globalCAARegisterNumber;
   TR_GlobalRegisterNumber  _globalParentDSARegisterNumber;
   TR_GlobalRegisterNumber  _globalEntryPointRegisterNumber;
   TR_GlobalRegisterNumber  _globalReturnAddressRegisterNumber;

   void initialiseRegisterFile();
   void initializeFPRegPairTable();

   public:

   Machine(TR::CodeGenerator *cg);

   TR::RealRegister *getS390RealRegister(TR::RealRegister::RegNum regNum)
      {
      return _registerFile[regNum];
      }

   TR::RealRegister *getS390RealRegister(int32_t regNum)
      {
      return _registerFile[regNum];
      }

   TR::RealRegister *getRealRegister(TR_GlobalRegisterNumber grn)
       {
       return _registerFile[_globalRegisterNumberToRealRegisterMap[grn]];
       }

   uint8_t getGPRSize();
   uint8_t getFPRSize() const { return 8;}
   uint8_t getARSize() const { return 4;}
   uint8_t getHPRSize() const { return 4;}
   uint8_t getVRFSize() const { return 16;}

   TR::RegisterDependencyConditions * createDepCondForLiveGPRs(TR::list<TR::Register*> *spilledRegisterList);

   /**
    * @return number of single cycle instructions
    * that must be processed to avoid AGI
    */
   int32_t getAGIDelay() const { return 4;}


   /** GENERAL INTERFACE for OBTAINING ANY TYPE of REG */
   TR::Register *assignBestRegister(TR::Register    *targetRegister,
				   TR::Instruction *currInst,
                                   bool            doBookKeeping,
                                   uint64_t        availRegMask = 0xffffffff);


   // EVENODD PAIR REGISTERS methods
   TR::Register *assignBestRegisterPair(TR::Register    *regPair,
                                       TR::Instruction *currInst,
                                       bool            doBookKeeping,
                                       uint64_t        availRegMask = 0x0000ffff);

   uint32_t genBitVectOfAssignableGPRs();
   uint8_t genBitVectOfLiveGPRPairs();

   TR::RealRegister* findBestSwapRegister(TR::Register* reg1, TR::Register* reg2);
   TR::Instruction* registerCopy(TR::Instruction *precedingInstruction,
                                    TR_RegisterKinds rk,
                                    TR::Register *targetReg, TR::Register *sourceReg,
                                    TR::CodeGenerator *cg, flags32_t instFlags);
   TR::Instruction* registerExchange(TR::Instruction      *precedingInstruction,
                                        TR_RegisterKinds     rk,
                                        TR::RealRegister *targetReg,
                                        TR::RealRegister *sourceReg,
                                        TR::RealRegister *middleReg,
                                        TR::CodeGenerator    *cg,
                                        flags32_t            instFlags);

   bool  isLegalEvenOddPair(TR::RealRegister* evenReg, TR::RealRegister* oddReg, uint64_t availRegMask=0x0000ffff);
   bool  isLegalEvenOddRestrictedPair(TR::RealRegister* evenReg, TR::RealRegister* oddReg, uint64_t availRegMask=0x0000ffff);
   bool  isLegalEvenRegister(TR::RealRegister* reg, bool allowBlocked, uint64_t availRegMask=0x0000ffff, bool allowLocked=false);
   bool  isLegalOddRegister(TR::RealRegister* reg, bool allowBlocked, uint64_t availRegMask=0x0000ffff, bool allowLocked=false);

   TR::RealRegister* findBestLegalEvenRegister(uint64_t availRegMask=0x0000ffff);
   TR::RealRegister* findBestLegalOddRegister(uint64_t availRegMask=0x0000ffff);
   bool  isLegalFPPair(TR::RealRegister* highReg, TR::RealRegister* lowReg, uint64_t availRegMask=0x0000ffff);
   bool  isLegalFirstOfFPRegister(TR::RealRegister* reg, bool allowBlocked, uint64_t availRegMask=0x0000ffff, bool allowLocked=false);
   bool  isLegalSecondOfFPRegister(TR::RealRegister* reg, bool allowBlocked, uint64_t availRegMask=0x0000ffff, bool allowLocked=false);
   TR::RealRegister* findBestLegalSiblingFPRegister(bool isFirst, uint64_t availRegMask=0x0000ffff);

   bool findBestFreeRegisterPair(TR::RealRegister** firstReg, TR::RealRegister** lastReg, TR_RegisterKinds rk,
				    TR::Instruction* currInst, uint64_t availRegMask=0x0000ffff);
   void    freeBestRegisterPair(TR::RealRegister** firstReg, TR::RealRegister** lastReg, TR_RegisterKinds rk,
			            TR::Instruction* currInst, uint64_t availRegMask=0x0000ffff);
   void    freeBestFPRegisterPair(TR::RealRegister** firstReg, TR::RealRegister** lastReg,
			            TR::Instruction* currInst, uint64_t availRegMask=0x0000ffff);
   uint64_t filterColouredRegisterConflicts(TR::Register *targetRegister, TR::Register *siblingRegister, TR::Instruction *currInstr);

   // Access Register managed
   TR::RealRegister* findVirtRegInHighWordRegister(TR::Register *virtReg);

   // Used to keep track of blocked GPR's during HPR upgrades/spills/copies to prevent clobbering
   bool addToUpgradedBlockedList(TR::RealRegister * reg);
   void allocateUpgradedBlockedList(TR_Stack<TR::RealRegister*> * mem);
   TR::RealRegister * getNextRegFromUpgradedBlockedList();

   void blockVolatileAccessRegisters();
   void unblockVolatileAccessRegisters();

   // High Register managed
   void spillAllVolatileHighRegisters(TR::Instruction  *currentInstruction);
   void blockVolatileHighRegisters();
   void unblockVolatileHighRegisters();

   // SINGLE REGISTERs methods
   uint64_t constructFreeRegBitVector(TR::Instruction  *currentInstruction);
   TR::RealRegister* findRegNotUsedInInstruction(TR::Instruction  *currentInstruction);

   TR::Register *assignBestRegisterSingle(TR::Register    *targetRegister,
                                         TR::Instruction *currInst,
                                         bool            doBookKeeping,
                                         uint64_t        availRegMask = 0xffffffff);

   TR::RealRegister *findBestFreeRegister(TR::Instruction   *currentInstruction,
                                             TR_RegisterKinds  rk,
                                             TR::Register      *virtualReg = NULL,
                                             uint64_t          availRegMask = 0xffffffff,
                                             bool              needsHighWord = false);

   TR::RealRegister *freeBestRegister(TR::Instruction   *currentInstruction,
                                         TR::Register     *virtReg,
                                         TR_RegisterKinds rk,
                                         uint64_t availRegMask = 0xffffffff,
                                         bool allowNullReturn = false,
                                         bool doNotSpillToSiblingHPR = false);

   TR::RealRegister *findBestRegisterForShuffle(TR::Instruction  *currentInstruction,
                                                   TR::Register         *currentAssignedRegister,
                                                   uint64_t             availRegMask = 0xffffffff);

   TR::RealRegister *shuffleOrSpillRegister(TR::Instruction * currInst,
                                            TR::Register * toFreeReg,
                                            uint64_t availRegMask);


   void freeRealRegister(TR::Instruction *currentInstruction,
                         TR::RealRegister *targetReal,
                         bool is64BitReg);

   TR::Instruction * freeHighWordRegister(TR::Instruction *currentInstruction, TR::RealRegister *targetRegisterHW, flags32_t instFlags);
   void spillRegister(TR::Instruction *currentInstruction, TR::Register *virtReg, uint32_t availHighWordRegMap = -1);

   TR::RealRegister *reverseSpillState(TR::Instruction      *currentInstruction,
                                          TR::Register         *spilledRegister,
                                          TR::RealRegister *targetRegister = NULL);

   bool isAssignable(TR::Register *virtReg, TR::RealRegister *realReg);

   TR::Instruction *coerceRegisterAssignment(TR::Instruction *currentInstruction,
                                            TR::Register    *virtualRegister,
                                            TR::RealRegister::RegNum registerNumber,
                                            flags32_t       instFlags);


   TR::CodeGenerator *cg() {return _cg;}
   TR_Debug         *getDebug();

   uint32_t *initializeGlobalRegisterTable();
   uint32_t initGlobalVectorRegisterMap(uint32_t vectorOffset);
   void lockGlobalRegister(int32_t globalRegisterTableIndex);
   void releaseGlobalRegister(int32_t globalRegisterTableIndex, TR::RealRegister::RegNum gReg);
   int32_t findGlobalRegisterIndex(TR::RealRegister::RegNum gReg);

   void releaseLiteralPoolRegister();    // free up GPR6

   //  Tactical GRA stuff ///////////////////////////////////
   uint32_t *getGlobalRegisterTable()
      {
      return _globalRegisterNumberToRealRegisterMap;
      }


   /*************************** Access regs ***************************/


   TR_GlobalRegisterNumber getFirstGlobalAccessRegisterNumber()
      {
      return _firstGlobalAccessRegisterNumber;
      }

   TR_GlobalRegisterNumber setFirstGlobalAccessRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLastGlobalAccessRegisterNumber()
       {
       return _lastGlobalAccessRegisterNumber;
       }

   TR_GlobalRegisterNumber setLastGlobalAccessRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLastGlobalGPRRegisterNumber()
      {
      return _lastGlobalGPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLastGlobalHPRRegisterNumber()
      {
      return _lastGlobalHPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastGlobalHPRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getFirstGlobalGPRRegisterNumber()
      {
      return _firstGlobalGPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setFirstGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getFirstGlobalHPRRegisterNumber()
      {
      return _firstGlobalHPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setFirstGlobalHPRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLast8BitGlobalGPRRegisterNumber()
      {
      return _last8BitGlobalGPRRegisterNumber;
      }
   TR_GlobalRegisterNumber setLast8BitGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _last8BitGlobalGPRRegisterNumber = reg;
      }


   /*************************** floating point regs ***************************/

   TR_GlobalRegisterNumber getFirstGlobalFPRRegisterNumber()
      {
      return _firstGlobalFPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setFirstGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLastGlobalFPRRegisterNumber()
      {
      return _lastGlobalFPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getFirstOverlappedGlobalFPRRegisterNumber()
       {
       return _firstOverlappedGlobalFPRRegisterNumber;
       }

   TR_GlobalRegisterNumber setFirstOverlappedGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg)
       {
       return _firstOverlappedGlobalFPRRegisterNumber = reg;
       }

   TR_GlobalRegisterNumber getLastOverlappedGlobalFPRRegisterNumber()
      {
      return _lastOverlappedGlobalFPRRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastOverlappedGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _lastOverlappedGlobalFPRRegisterNumber = reg;
      }

   /*************************** vector regs **************************/


   TR_GlobalRegisterNumber getFirstGlobalVRFRegisterNumber()
      {
      return _firstGlobalVRFRegisterNumber;
      }

   TR_GlobalRegisterNumber setFirstGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLastGlobalVRFRegisterNumber()
      {
      return _lastGlobalVRFRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getFirstOverlappedGlobalVRFRegisterNumber()
      {
      return _firstOverlappedGlobalVRFRegisterNumber;
      }

   TR_GlobalRegisterNumber setFirstOverlappedGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _firstOverlappedGlobalVRFRegisterNumber = reg;
      }

   TR_GlobalRegisterNumber getLastOverlappedGlobalVRFRegisterNumber()
      {
      return _lastOverlappedGlobalVRFRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastOverlappedGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _lastOverlappedGlobalVRFRegisterNumber = reg;
      }


   /*************************** Misc regs **************************/

   TR_GlobalRegisterNumber getGlobalEnvironmentRegisterNumber()
      {
      return _globalEnvironmentRegisterNumber;
      }

   TR_GlobalRegisterNumber setGlobalEnvironmentRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _globalEnvironmentRegisterNumber = reg;
      }

   TR_GlobalRegisterNumber getGlobalCAARegisterNumber()
      {
      return _globalCAARegisterNumber;
      }

   TR_GlobalRegisterNumber setGlobalCAARegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _globalCAARegisterNumber=reg;
      }

   TR_GlobalRegisterNumber getGlobalParentDSARegisterNumber()
      {
      return _globalParentDSARegisterNumber;
      }

   TR_GlobalRegisterNumber setGlobalParentDSARegisterNumber(TR_GlobalRegisterNumber reg)
      {
      return _globalParentDSARegisterNumber = reg;
      }

   TR_GlobalRegisterNumber setGlobalReturnAddressRegisterNumber(TR_GlobalRegisterNumber reg)
      {
        return _globalReturnAddressRegisterNumber = reg;
      }

   TR_GlobalRegisterNumber getGlobalReturnAddressRegisterNumber()
      {
        return _globalReturnAddressRegisterNumber;
      }

   TR_GlobalRegisterNumber setGlobalEntryPointRegisterNumber(TR_GlobalRegisterNumber reg)
      {
        return _globalEntryPointRegisterNumber = reg;
      }

   TR_GlobalRegisterNumber getGlobalEntryPointRegisterNumber()
      {
        return _globalEntryPointRegisterNumber;
      }

   TR_GlobalRegisterNumber getLastGlobalCCRRegisterNumber()
      {
      return _lastGlobalCCRRegisterNumber;
      }

   TR_GlobalRegisterNumber setLastGlobalCCRRegisterNumber(TR_GlobalRegisterNumber reg);

   TR_GlobalRegisterNumber getLastVolatileNonLinkGPR()
     {
     return _lastVolatileNonLinkGPR;
     }

   TR_GlobalRegisterNumber setLastVolatileNonLinkGPR(TR_GlobalRegisterNumber reg)
     {
     return _lastVolatileNonLinkGPR=reg;
     }

   TR_GlobalRegisterNumber getLastLinkageGPR()
     {
     return _lastLinkageGPR;
     }

   TR_GlobalRegisterNumber setLastLinkageGPR(TR_GlobalRegisterNumber reg)
     {
     return _lastLinkageGPR=reg;
     }

   TR_GlobalRegisterNumber getLastVolatileNonLinkFPR()
     {
     return _lastVolatileNonLinkFPR;
     }
   TR_GlobalRegisterNumber setLastVolatileNonLinkFPR(TR_GlobalRegisterNumber reg)
     {
     return _lastVolatileNonLinkFPR=reg;
     }

   TR_GlobalRegisterNumber getLastLinkageFPR()
     {
     return _lastLinkageFPR;
     }

   TR_GlobalRegisterNumber setLastLinkageFPR(TR_GlobalRegisterNumber reg)
     {
     return _lastLinkageFPR=reg;
     }

   TR::Register * getAccessRegisterFromGlobalRegisterNumber(TR_GlobalRegisterNumber reg);

   TR::Register * getGPRFromGlobalRegisterNumber(TR_GlobalRegisterNumber reg);

   TR::Register * getHPRFromGlobalRegisterNumber(TR_GlobalRegisterNumber reg);

   ////////////////////////////////

   // Register Association Stuff ///////////////
   void setRegisterWeightsFromAssociations();
   void createRegisterAssociationDirective(TR::Instruction *cursor);

   TR::Register *getVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum)
       {
       return _registerAssociations[regNum];
       }

   TR::Register *setVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum, TR::Register *virtReg);

   void clearRegisterAssociations()
      {
      memset(_registerAssociations, 0, sizeof(TR::Register *) * (TR::RealRegister::NumRegisters));
      }
   bool supportLockedRegisterAssignment();

   bool isRestrictedReg(TR::RealRegister::RegNum reg);

   TR::RealRegister *getRegisterFile(int32_t i);

   void takeRegisterStateSnapShot();
   void restoreRegisterStateFromSnapShot();

   // These return the next available tableIndex
   int32_t addGlobalReg(TR::RealRegister::RegNum reg, int32_t tableIndex);
   int32_t addGlobalRegLater(TR::RealRegister::RegNum reg, int32_t tableIndex);
   int32_t getGlobalReg(TR::RealRegister::RegNum reg);
   };
}
}

#endif
