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

#ifndef OMR_ILOPS_INCL
#define OMR_ILOPS_INCL

#include <stdint.h>
#include "il/DataTypes.hpp"
#include "il/ILHelpers.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILProps.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

class TR_DebugExt;
namespace TR { class SymbolReference; }

namespace OMR
{

/**
 * Opcode properties.
 */
struct OpCodeProperties
   {
   TR::ILOpCodes       opcode; ///< Enum value. In the _opCodeProperties table, every entry
                               ///< must be in the same position as its enum entry value.
   char const *        name;   ///< Opcode Name

   /**
    * Opcode properties
    */
   uint32_t            properties1; // These should be flags32_t, but gcc becomes too slow
   uint32_t            properties2; // at compiling the big table in IlOps.cpp trying to inline
   uint32_t            properties3; // all those trivial flags32_t constructor calls
   uint32_t            properties4;

   TR::DataType        dataType;
   uint32_t            typeProperties;
   TR::ILChildPropType childTypes;
   TR::ILOpCodes       swapChildrenOpCode;
   TR::ILOpCodes       reverseBranchOpCode;
   TR::ILOpCodes       booleanCompareOpCode;
   TR::ILOpCodes       ifCompareOpCode;
   };

/**
 * ILOpCode largely consists of a query interface to
 * ask questions about TR IL Opcodes.
 */
class ILOpCode
   {

public:

   // Deprecate?
   ILOpCode()
      : _opCode(TR::BadILOp)
      {}

   ILOpCode(TR::ILOpCodes opCode)
      : _opCode(opCode)
      {
      TR_ASSERT(opCode <= TR::LastTROp, "assertion failure");
      }

   TR::ILOpCodes getOpCodeValue() const           { return (TR::ILOpCodes)_opCode; }
   TR::ILOpCodes setOpCodeValue(TR::ILOpCodes op) { TR_ASSERT(op <= TR::LastTROp, "assertion failure"); return (TR::ILOpCodes) (_opCode = op); }

   /// Get the opcode to be used if the children of this opcode are swapped.
   /// e.g. ificmplt --> ificmpgt
   TR::ILOpCodes getOpCodeForSwapChildren() const
      { return _opCodeProperties[_opCode].swapChildrenOpCode; }

   /// Get the opcode to be used if the sense of this (compare) opcode is reversed.
   /// e.g. ificmplt --> ificmpge
   TR::ILOpCodes getOpCodeForReverseBranch() const
      { return _opCodeProperties[_opCode].reverseBranchOpCode; }

   /// Get the boolean compare opcode that corresponds to this compare-and-branch
   /// opcode.
   /// e.g. ificmplt --> icmplt
   TR::ILOpCodes convertIfCmpToCmp()
      { return _opCodeProperties[_opCode].booleanCompareOpCode; }

   /// Get the compare-and-branch opcode that corresponds to this boolean compare
   ///e.g. icmplt --> ificmplt
   TR::ILOpCodes convertCmpToIfCmp()
      { return _opCodeProperties[_opCode].ifCompareOpCode; }

   TR::DataType getDataType() const                  { return _opCodeProperties[_opCode].dataType; }
   static TR::DataType getDataType(TR::ILOpCodes op) { return _opCodeProperties[op].dataType; }

   TR::DataType getType() const                  { return _opCodeProperties[_opCode].dataType; }
   static TR::DataType getType(TR::ILOpCodes op) { return _opCodeProperties[op].dataType; }

   const char *getName() const { return _opCodeProperties[_opCode].name; }

   // Query Functions

   uint32_t getSize() const                  { return typeProperties().getValue(ILTypeProp::Size_Mask); }
   static uint32_t getSize(TR::ILOpCodes op) { return ILOpCode(op).getSize(); }

   /**
    * ILTypeProp
    */
   bool isVoid()                     const { return typeProperties().getValue() == 0; }
   bool isByte()                     const { return (typeProperties().testAny(ILTypeProp::Integer | ILTypeProp::Unsigned) && typeProperties().testAll(ILTypeProp::Size_1)); }
   bool isShort()                    const { return (typeProperties().testAny(ILTypeProp::Integer | ILTypeProp::Unsigned) && typeProperties().testAll(ILTypeProp::Size_2)); }
   bool isInt()                      const { return (typeProperties().testAny(ILTypeProp::Integer | ILTypeProp::Unsigned) && typeProperties().testAll(ILTypeProp::Size_4)); }
   bool isLong()                     const { return (typeProperties().testAny(ILTypeProp::Integer | ILTypeProp::Unsigned) && typeProperties().testAll(ILTypeProp::Size_8)); }
   bool isRef()                      const { return typeProperties().testAny(ILTypeProp::Address); }
   bool isFloat()                    const { return typeProperties().testAll(ILTypeProp::Floating_Point | ILTypeProp::Size_4); }
   bool isDouble()                   const { return typeProperties().testAll(ILTypeProp::Floating_Point | ILTypeProp::Size_8); }
   bool isInteger()                  const { return typeProperties().testAny(ILTypeProp::Integer); }
   bool isUnsigned()                 const { return typeProperties().testAny(ILTypeProp::Unsigned); }
   bool isUnsignedConversion()       const { return isUnsigned() && isConversion(); }
   bool isFloatingPoint()            const { return typeProperties().testAny(ILTypeProp::Floating_Point); }
   bool isVector()                   const { return typeProperties().testAny(ILTypeProp::Vector); }
   bool isIntegerOrAddress()         const { return typeProperties().testAny(ILTypeProp::Integer | ILTypeProp::Address); }
   bool is1Byte()                    const { return typeProperties().testAny(ILTypeProp::Size_1); }
   bool is2Byte()                    const { return typeProperties().testAny(ILTypeProp::Size_2); }
   bool is4Byte()                    const { return typeProperties().testAny(ILTypeProp::Size_4); }
   bool is8Byte()                    const { return typeProperties().testAny(ILTypeProp::Size_8); }
   bool is16Byte()                   const { return typeProperties().testAny(ILTypeProp::Size_16); }
   bool is32Byte()                   const { return typeProperties().testAny(ILTypeProp::Size_32); }
   /**
    * Opcode does not explicitly encode datatype in the property. Nodes with
    * this flag must determine datatype from other means such as children,
    * symrefs, etc.
    */
   bool hasNoDataType()              const { return typeProperties().testAny(ILTypeProp::HasNoDataType); }

   /**
    * ILChildProp
    */
   uint32_t expectedChildCount()     const { return GET_CHILD_COUNT(childProperties()); }
   TR::DataTypes expectedChildType(uint32_t childIndex) const { return GET_CHILD_TYPE(childIndex, childProperties()); }
   bool canHaveGlRegDeps()           const { return getOpCodeValue() == TR::BBStart || getOpCodeValue() == TR::BBEnd || isCall() || isBranch(); }

   /**
    * ILProp1
    */
   bool isCommutative()              const { return properties1().testAny(ILProp1::Commutative); }
   bool isAssociative()              const { return properties1().testAny(ILProp1::Associative); }
   bool isConversion()               const { return properties1().testAny(ILProp1::Conversion); }
   bool isAdd()                      const { return properties1().testAny(ILProp1::Add); }
   bool isSub()                      const { return properties1().testAny(ILProp1::Sub); }
   bool isMul()                      const { return properties1().testAny(ILProp1::Mul); }
   bool isDiv()                      const { return properties1().testAny(ILProp1::Div); }
   bool isRem()                      const { return properties1().testAny(ILProp1::Rem); }
   bool isLeftShift()                const { return properties1().testAny(ILProp1::LeftShift); }
   bool isRightShift()               const { return properties1().testAny(ILProp1::RightShift); }
   bool isShift()                    const { return properties1().testAny(ILProp1::LeftShift | ILProp1::RightShift); }
   bool isShiftLogical()             const { return properties1().testAny(ILProp1::ShiftLogical); }
   bool isBooleanCompare()           const { return properties1().testAny(ILProp1::BooleanCompare); }
   bool isBranch()                   const { return properties1().testAny(ILProp1::Branch); }
   bool isCompBranchOnly()           const { return properties1().testAny(ILProp1::CompBranchOnly); }
   bool isIf()                       const { return properties1().testAll(ILProp1::BooleanCompare | ILProp1::Branch) && !isCompBranchOnly(); }
   bool isGoto()                     const { return properties1().testAll(ILProp1::Branch | ILProp1::TreeTop) && !isCompBranchOnly() && !isIf(); }
   bool isIndirect()                 const { return properties1().testAny(ILProp1::Indirect); }
   bool isLoadVar()                  const { return properties1().testAny(ILProp1::LoadVar); }
   bool isLoadConst()                const { return properties1().testAny(ILProp1::LoadConst); }
   bool isLoad()                     const { return properties1().testAny(ILProp1::Load); }
   bool isLoadVarDirect()            const { return properties1().testValue(ILProp1::Indirect | ILProp1::LoadVar, ILProp1::LoadVar); }
   bool isLoadDirect()               const { return isLoad() && !isIndirect(); }
   bool isLoadIndirect()             const { return properties1().testAll(ILProp1::Indirect | ILProp1::LoadVar); }
   bool isStore()                    const { return properties1().testAny(ILProp1::Store); }
   bool isStoreDirect()              const { return properties1().testValue(ILProp1::Indirect | ILProp1::Store, ILProp1::Store); }
   bool isStoreIndirect()            const { return properties1().testAll(ILProp1::Indirect | ILProp1::Store); }
   bool isLoadVarOrStore()           const { return properties1().testAny((ILProp1::Store | ILProp1::LoadVar)); }
   bool isLoadReg()                  const { return properties1().testAny(ILProp1::LoadReg); }
   bool isStoreReg()                 const { return properties1().testAny(ILProp1::StoreReg); }
   bool isLoadDirectOrReg()          const { return isLoadDirect() || isLoadReg(); }
   bool isStoreDirectOrReg()         const { return isStoreDirect() || isStoreReg(); }
   bool isIntegralLoadVar()          const { return isLoadVar() && getType().isIntegral(); }
   bool isIntegralConst()            const { return isLoadConst() && getType().isIntegral(); }
   bool isAnd()                      const { return properties1().testAny(ILProp1::And); }
   bool isOr()                       const { return properties1().testAny(ILProp1::Or); }
   bool isXor()                      const { return properties1().testAny(ILProp1::Xor); }
   bool isNeg()                      const { return properties1().testAny(ILProp1::Neg); }
   bool isBitwiseLogical()           const { return isAnd() || isXor() || isOr(); }
   bool isReturn()                   const { return properties1().testAny(ILProp1::Return); }
   bool isCall()                     const { return properties1().testAny(ILProp1::Call); }
   bool isCallDirect()               const { return properties1().testValue(ILProp1::Indirect | ILProp1::Call, ILProp1::Call); }
   bool isCallIndirect()             const { return properties1().testAll(ILProp1::Call | ILProp1::Indirect); }
   bool isTreeTop()                  const { return properties1().testAny(ILProp1::TreeTop); }
   bool hasSymbolReference()         const { return properties1().testAny(ILProp1::HasSymbolRef); }
   bool isMemoryReference()          const { return properties1().testAll(ILProp1::HasSymbolRef | ILProp1::LoadVar); }
   bool isSwitch()                   const { return properties1().testAny(ILProp1::Switch); }

   /**
    * ILProp2
    */
   bool mustBeLowered()              const { return properties2().testAny(ILProp2::MustBeLowered); }
   bool canShareValueNumber()        const { return properties2().testAny(ILProp2::ValueNumberShare); }
   bool isWrtBar()                   const { return properties2().testAny(ILProp2::WriteBarrierStore); }
   bool isReadBar()                  const { return properties2().testAny(ILProp2::ReadBarrierLoad); }
   bool canRaiseException()          const { return properties2().testAny(ILProp2::CanRaiseException); }
   bool isCheck()                    const { return properties2().testAny(ILProp2::Check); }
   bool isNullCheck()                const { return properties2().testAny(ILProp2::NullCheck); }
   bool isResolveCheck()             const { return properties2().testAny(ILProp2::ResolveCheck); }
   bool isResolveOrNullCheck()       const { return properties2().testAny(ILProp2::ResolveCheck | ILProp2::NullCheck); }
   bool isBndCheck()                 const { return properties2().testAny(ILProp2::BndCheck); }
   bool isCheckCast()                const { return properties2().testAny(ILProp2::CheckCast); }
   bool isCheckCastOrNullCheck()     const { return properties2().testAny(ILProp2::CheckCast | ILProp2::NullCheck); }
   bool mayUseSystemStack()          const { return properties2().testAny(ILProp2::MayUseSystemStack); }
   bool isSupportedForPRE()          const { return properties2().testAny(ILProp2::SupportedForPRE); }
   bool isLeftRotate()               const { return properties2().testAny(ILProp2::LeftRotate); }
   bool isRotate()                   const { return properties2().testAny(ILProp2::LeftRotate); }
   bool isUnsignedCompare()          const { return properties2().testAny(ILProp2::UnsignedCompare); }
   bool isOverflowCompare()          const { return properties2().testAny(ILProp2::OverflowCompare); }
   bool isSelect()                   const { return properties2().testAny(ILProp2::Select); }
   bool isSelectAdd()                const { return properties2().testAny(ILProp2::SelectAdd); }
   bool isSelectSub()                const { return properties2().testAny(ILProp2::SelectSub); }
   bool isArithmetic()               const { return isAdd() || isSub() || isMul() || isDiv() || isRem() || isLeftShift() || isRightShift() || isShiftLogical() || isAnd() || isXor() || isOr() || isNeg() || isSelectAdd() || isSelectSub(); }
   bool isCondCodeComputation()      const { return properties2().testAny(ILProp2::CondCodeComputation); }
   bool isJumpWithMultipleTargets()  const { return properties2().testAny(ILProp2::JumpWithMultipleTargets); }  // Transactional Memory uses this
   bool isLoadAddr()                 const { return properties2().testAny(ILProp2::LoadAddress); }
   bool isMax()                      const { return properties2().testAny(ILProp2::Max); }
   bool isMin()                      const { return properties2().testAny(ILProp2::Min); }
   bool isNew()                      const { return properties2().testAny(ILProp2::New); }
   bool isZeroExtension()            const { return properties2().testAny(ILProp2::ZeroExtension); }
   bool isSignExtension()            const { return properties2().testAny(ILProp2::SignExtension); }

   /**
    * ILProp3
    */
   bool isFence()                    const { return properties3().testAny(ILProp3::Fence); }
   bool isExceptionRangeFence()      const { return properties3().testAny(ILProp3::ExceptionRangeFence); }
   bool isLikeUse()                  const { return properties3().testAny(ILProp3::LikeUse); }
   bool isLikeDef()                  const { return properties3().testAny(ILProp3::LikeDef); }
   bool isSpineCheck()               const { return properties3().testAny(ILProp3::SpineCheck); }
   bool isArrayLength()              const { return properties3().testAny(ILProp3::ArrayLength); }
   bool isSignedExponentiation()     const { return properties3().testAny(ILProp3::SignedExponentiation); }    // denotes the power (not the base) is signed
   bool isUnsignedExponentiation()   const { return properties3().testAny(ILProp3::UnsignedExponentiation); }  // denotes the power (not the base) is unsigned
   bool isExponentiation()           const { return isSignedExponentiation() || isUnsignedExponentiation(); }
   bool isCompareTrueIfLess()        const { return properties3().testAny(ILProp3::CompareTrueIfLess); }
   bool isCompareTrueIfGreater()     const { return properties3().testAny(ILProp3::CompareTrueIfGreater); }
   bool isCompareTrueIfEqual()       const { return properties3().testAny(ILProp3::CompareTrueIfEqual); }
   bool isCompareTrueIfUnordered()   const { return properties3().testAny(ILProp3::CompareTrueIfUnordered); }
   bool isCompareForEquality()       const { return isBooleanCompare() && (isCompareTrueIfLess() == isCompareTrueIfGreater()); }
   bool isCompareForOrder()          const { return isBooleanCompare() && (isCompareTrueIfLess() != isCompareTrueIfGreater()); }
   bool hasBranchChildren()          const { return properties3().testAny(ILProp3::HasBranchChild); }
   bool skipDynamicLitPoolOnInts()   const { return properties3().testAny(ILProp3::SkipDynamicLitPoolOnInts); }
   bool isAbs()                      const { return properties3().testAny(ILProp3::Abs); }
   bool isVectorReduction()          const { return properties3().testAny(ILProp3::VectorReduction); }
   bool isSignum()                   const { return properties3().testAny(ILProp3::Signum); }
   bool isReverseLoadOrStore()       const { return properties3().testAny(ILProp3::ReverseLoadOrStore); }



   bool isArrayRef()            const { return (isAdd() && isCommutative() && isAssociative() && isRef()); } // returns true if TR::aiadd or TR::aladd
   bool isTwoChildrenAddress()  const { return isArrayRef(); }
   bool isCase()                const { return getOpCodeValue() == TR::Case; }
   bool isAnchor()              const { return getOpCodeValue() == TR::compressedRefs; }
   bool isOverflowCheck()         const { return (getOpCodeValue() == TR::OverflowCHK)         ||
                                               (getOpCodeValue() == TR::UnsignedOverflowCHK);
                                      }

   bool hasPinningArrayPointer()
      {
      return (getOpCodeValue() == TR::aiadd)  ||
             (getOpCodeValue() == TR::aladd);
      }

   bool isLongCompare() const
      {
      return (getOpCodeValue() == TR::lcmp)    ||
             (getOpCodeValue() == TR::lcmpeq)  ||
             (getOpCodeValue() == TR::lcmpne)  ||
             (getOpCodeValue() == TR::lcmplt)  ||
             (getOpCodeValue() == TR::lcmpge)  ||
             (getOpCodeValue() == TR::lcmpgt)  ||
             (getOpCodeValue() == TR::lcmple)  ||
             (getOpCodeValue() == TR::lucmp)   ||
             (getOpCodeValue() == TR::lucmplt) ||
             (getOpCodeValue() == TR::lucmpge) ||
             (getOpCodeValue() == TR::lucmpgt) ||
             (getOpCodeValue() == TR::lucmple);
      }

   bool isFunctionCall() const
      {
      return isCall()                           &&
             getOpCodeValue() != TR::arraycopy  &&
             getOpCodeValue() != TR::arrayset   &&
             getOpCodeValue() != TR::bitOpMem   &&
             getOpCodeValue() != TR::bitOpMemND &&
             getOpCodeValue() != TR::arraycmp   &&
             getOpCodeValue() != TR::arraycmpWithPad;
      }

   bool isCompareDouble()
      {
      switch (_opCode)
         {
         case TR::ifdcmpeq:
         case TR::ifdcmpne:
         case TR::ifdcmpge:
         case TR::ifdcmpgt:
         case TR::ifdcmple:
         case TR::ifdcmplt:
         case TR::ifdcmpequ:
         case TR::ifdcmpneu:
         case TR::ifdcmpgeu:
         case TR::ifdcmpleu:
         case TR::ifdcmpgtu:
         case TR::ifdcmpltu:
         case TR::dcmpeq:
         case TR::dcmpne:
         case TR::dcmpge:
         case TR::dcmpgt:
         case TR::dcmple:
         case TR::dcmplt:
         case TR::dcmpequ:
         case TR::dcmpneu:
         case TR::dcmpgeu:
         case TR::dcmpleu:
         case TR::dcmpgtu:
         case TR::dcmpltu:
            return true;
         default:
            return false;
         }

      return false;
      }

   bool canUseBranchOnCount()
      {
      return getOpCodeValue() == TR::ificmpgt  ||
             getOpCodeValue() == TR::iflcmpgt  ||
             getOpCodeValue() == TR::ifiucmpgt ||
             getOpCodeValue() == TR::ifacmpgt  ||
             getOpCodeValue() == TR::iflucmpgt ||
             getOpCodeValue() == TR::iadd      ||
             getOpCodeValue() == TR::ladd      ||
             getOpCodeValue() == TR::iuaddc    ||
             getOpCodeValue() == TR::aiadd     ||
             getOpCodeValue() == TR::luaddc    ||
             getOpCodeValue() == TR::aladd     ||
             getOpCodeValue() == TR::isub      ||
             getOpCodeValue() == TR::lsub      ||
             getOpCodeValue() == TR::iusubb    ||
             getOpCodeValue() == TR::asub      ||
             getOpCodeValue() == TR::lusubb;
      }

   bool isStrictWidenConversion()
      {
      return getOpCodeValue() == TR::f2d  ||
             getOpCodeValue() == TR::b2i  ||
             getOpCodeValue() == TR::s2i  ||
             getOpCodeValue() == TR::b2l  ||
             getOpCodeValue() == TR::bu2i ||
             getOpCodeValue() == TR::bu2l ||
             getOpCodeValue() == TR::s2l  ||
             getOpCodeValue() == TR::su2i ||
             getOpCodeValue() == TR::su2l ||
             getOpCodeValue() == TR::i2l;
      }

   bool isMemToMemOp()
      {
      return getOpCodeValue() == TR::bitOpMem ||
             getOpCodeValue() == TR::arrayset ||
             getOpCodeValue() == TR::arraycmp ||
             getOpCodeValue() == TR::arraycopy;
      }

   static TR::ILOpCodes getProperConversion(TR::DataType sourceDataType, TR::DataType targetDataType, bool needUnsignedConversion)
      {
      TR::ILOpCodes op = TR::DataType::getDataTypeConversion(sourceDataType, targetDataType);
      if (!needUnsignedConversion) return op;

      //FIXME: maybe its better to come up with two conversionMap tables rather
      // than having a big switch here
      switch (op)
         {
         case TR::b2s: return TR::bu2s;
         case TR::b2i: return TR::bu2i;
         case TR::b2l: return TR::bu2l;
         case TR::b2a: return TR::bu2a;// do we care about this?
         case TR::b2f: return TR::bu2f;
         case TR::b2d: return TR::bu2d;

         case TR::s2i: return TR::su2i;
         case TR::s2l: return TR::su2l;
         case TR::s2a: return TR::su2a;
         case TR::s2f: return TR::su2f;
         case TR::s2d: return TR::su2d;

         case TR::i2l: return TR::iu2l;
         case TR::i2a: return TR::iu2a;
         case TR::i2f: return TR::iu2f;
         case TR::i2d: return TR::iu2d;

         case TR::l2a: return TR::lu2a;
         case TR::l2f: return TR::lu2f;
         case TR::l2d: return TR::lu2d;

         case TR::f2i: return TR::f2iu;
         case TR::f2l: return TR::f2lu;
         case TR::f2b: return TR::f2bu;
         case TR::f2s: return TR::f2s;

         case TR::d2i: return TR::d2iu;
         case TR::d2l: return TR::d2lu;
         case TR::d2b: return TR::d2bu;
         case TR::d2s: return TR::d2s;

         default: return op;
         }
      }

   static TR::ILOpCodes convertSignedCmpToUnsignedCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::icmpeq:
            return TR::icmpeq;
         case TR::icmpne:
            return TR::icmpne;
         case TR::icmplt:
            return TR::iucmplt;
         case TR::icmpge:
            return TR::iucmpge;
         case TR::icmpgt:
            return TR::iucmpgt;
         case TR::icmple:
            return TR::iucmple;

         case TR::ificmpeq:
            return TR::ificmpeq;
         case TR::ificmpne:
            return TR::ificmpne;
         case TR::ificmplt:
            return TR::ifiucmplt;
         case TR::ificmpge:
            return TR::ifiucmpge;
         case TR::ificmpgt:
            return TR::ifiucmpgt;
         case TR::ificmple:
            return TR::ifiucmple;

         case TR::lcmpeq:
            return TR::lcmpeq;
         case TR::lcmpne:
            return TR::lcmpne;
         case TR::lcmplt:
            return TR::lucmplt;
         case TR::lcmpge:
            return TR::lucmpge;
         case TR::lcmpgt:
            return TR::lucmpgt;
         case TR::lcmple:
            return TR::lucmple;

         case TR::iflcmpeq:
            return TR::iflcmpeq;
         case TR::iflcmpne:
            return TR::iflcmpne;
         case TR::iflcmplt:
            return TR::iflucmplt;
         case TR::iflcmpge:
            return TR::iflucmpge;
         case TR::iflcmpgt:
            return TR::iflucmpgt;
         case TR::iflcmple:
            return TR::iflucmple;

         default:
           return TR::BadILOp;
         }
      }

   static TR::ILOpCodes compareOpCode(TR::DataType dt, enum TR_ComparisonTypes ct, bool unsignedCompare = false);

   static TR_ComparisonTypes getCompareType(TR::ILOpCodes op);

   static bool isStrictlyLessThanCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ificmplt:
         case TR::ifiucmplt:
         case TR::iflcmplt:
         case TR::iflucmplt:
         case TR::ifbcmplt:
         case TR::ifscmplt:
         case TR::ifsucmplt:
         case TR::iffcmplt:
         case TR::iffcmpltu:
         case TR::ifdcmplt:
         case TR::ifdcmpltu:
            return true;
         default:
            return false;
         }
      }

   static bool isStrictlyGreaterThanCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ificmpgt:
         case TR::ifiucmpgt:
         case TR::iflcmpgt:
         case TR::iflucmpgt:
         case TR::ifbcmpgt:
         case TR::ifscmpgt:
         case TR::ifsucmpgt:
         case TR::iffcmpgt:
         case TR::iffcmpgtu:
         case TR::ifdcmpgt:
         case TR::ifdcmpgtu:
            return true;
         default:
            return false;
         }
      }

   static bool isLessCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ificmplt:
         case TR::ifiucmplt:
         case TR::iflcmplt:
         case TR::iflucmplt:
         case TR::ifbcmplt:
         case TR::ifscmplt:
         case TR::ifsucmplt:
         case TR::iffcmplt:
         case TR::iffcmpltu:
         case TR::ifdcmplt:
         case TR::ifdcmpltu:

         case TR::ificmple:
         case TR::ifiucmple:
         case TR::iflcmple:
         case TR::iflucmple:
         case TR::ifbcmple:
         case TR::ifscmple:
         case TR::ifsucmple:
         case TR::iffcmple:
         case TR::iffcmpleu:
         case TR::ifdcmple:
         case TR::ifdcmpleu:
            return true;
         default:
            return false;
         }
      }

   static bool isGreaterCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ificmpgt:
         case TR::ifiucmpgt:
         case TR::iflcmpgt:
         case TR::iflucmpgt:
         case TR::ifbcmpgt:
         case TR::ifscmpgt:
         case TR::ifsucmpgt:
         case TR::iffcmpgt:
         case TR::iffcmpgtu:
         case TR::ifdcmpgt:
         case TR::ifdcmpgtu:

         case TR::ificmpge:
         case TR::ifiucmpge:
         case TR::iflcmpge:
         case TR::iflucmpge:
         case TR::ifbcmpge:
         case TR::ifscmpge:
         case TR::ifsucmpge:
         case TR::iffcmpge:
         case TR::iffcmpgeu:
         case TR::ifdcmpge:
         case TR::ifdcmpgeu:
            return true;
         default:
            return false;
         }
      }

   static bool isEqualCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ificmpeq:
         case TR::iflcmpeq:
         case TR::ifacmpeq:
         case TR::ifbcmpeq:
         case TR::ifscmpeq:
         case TR::iffcmpeq:
         case TR::ifdcmpeq:
            return true;
         default:
            return false;
         }
      }

   static bool isNotEqualCmp(TR::ILOpCodes op)
      {
      switch (op)
         {
         case TR::ificmpne:
         case TR::iflcmpne:
         case TR::ifacmpne:
         case TR::ifbcmpne:
         case TR::ifscmpne:
         case TR::iffcmpne:
         case TR::ifdcmpne:
            return true;
         default:
            return false;
         }
      }

   static TR::ILOpCodes indirectLoadOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::bloadi;
         case TR::Int16:    return TR::sloadi;
         case TR::Int32:    return TR::iloadi;
         case TR::Int64:    return TR::lloadi;
         case TR::Address:  return TR::aloadi;
         case TR::Float:    return TR::floadi;
         case TR::Double:   return TR::dloadi;
         default: TR_ASSERT(0, "no load opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes absOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int32:   return TR::iabs;
         case TR::Int64:   return TR::labs;
         case TR::Float:   return TR::fabs;
         case TR::Double:  return TR::dabs;
         default: TR_ASSERT(false, "no abs opcode for this datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes addOpCode(TR::DataType type, bool is64Bit)
      {
      switch(type)
         {
         case TR::Int8:     return TR::badd;
         case TR::Int16:    return TR::sadd;
         case TR::Int32:    return TR::iadd;
         case TR::Int64:    return TR::ladd;
         case TR::Address:  return (is64Bit) ? TR::aladd : TR::aiadd;
         case TR::Float:    return TR::fadd;
         case TR::Double:   return TR::dadd;
         case TR::VectorInt8:   return TR::vadd;
         case TR::VectorInt16:  return TR::vadd;
         case TR::VectorInt32:  return TR::vadd;
         case TR::VectorInt64:  return TR::vadd;
         case TR::VectorFloat:  return TR::vadd;
         case TR::VectorDouble: return TR::vadd;
         default: TR_ASSERT(0, "no add opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes unsignedAddOpCode(TR::DataType type, bool is64Bit)
      {
      switch(type)
         {
         case TR::Int8:     return TR::badd;
         case TR::Int16:    return TR::sadd;
         case TR::Int32:    return TR::iadd;
         case TR::Int64:    return TR::ladd;
	      case TR::Address:  return (is64Bit) ? TR::aladd : TR::aiadd;
         default: TR_ASSERT(0, "no add opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes subtractOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:    return TR::bsub;
         case TR::Int16:   return TR::ssub;
         case TR::Int32:   return TR::isub;
         case TR::Int64:   return TR::lsub;
         case TR::Float:   return TR::fsub;
         case TR::Double:  return TR::dsub;
         case TR::VectorInt8:   return TR::vsub;
         case TR::VectorInt16:  return TR::vsub;
         case TR::VectorInt32:  return TR::vsub;
         case TR::VectorInt64:  return TR::vsub;
         case TR::VectorFloat:  return TR::vsub;
         case TR::VectorDouble: return TR::vsub;
         default: TR_ASSERT(0, "no sub opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes unsignedSubtractOpCode(TR::DataType type)
      {
      switch(type)
         {
         default: TR_ASSERT(0, "no unsigned sub opcode for this datatype");
         }
      TR_ASSERT_FATAL(0, "all unsigned subtract opcode has been deprecated. (eclipse/omr#2657)");
      return TR::BadILOp;
      }

   static TR::ILOpCodes multiplyOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:    return TR::bmul;
         case TR::Int16:   return TR::smul;
         case TR::Int32:   return TR::imul;
         case TR::Int64:   return TR::lmul;
         case TR::Float:   return TR::fmul;
         case TR::Double:  return TR::dmul;
         case TR::VectorInt8:   return TR::vmul;
         case TR::VectorInt16:  return TR::vmul;
         case TR::VectorInt32:  return TR::vmul;
         case TR::VectorInt64:  return TR::vmul;
         case TR::VectorFloat:  return TR::vmul;
         case TR::VectorDouble: return TR::vmul;
         default: TR_ASSERT(0, "no mul opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes divideOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:    return TR::bdiv;
         case TR::Int16:   return TR::sdiv;
         case TR::Int32:   return TR::idiv;
         case TR::Int64:   return TR::ldiv;
         case TR::Float:   return TR::fdiv;
         case TR::Double:  return TR::ddiv;
         case TR::VectorInt8:   return TR::vdiv;
         case TR::VectorInt16:  return TR::vdiv;
         case TR::VectorInt32:  return TR::vdiv;
         case TR::VectorInt64:  return TR::vdiv;
         case TR::VectorFloat:  return TR::vdiv;
         case TR::VectorDouble: return TR::vdiv;
         default: TR_ASSERT(0, "no div opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes unsignedDivideOpCode(TR::DataType type)
      {
      switch (type)
         {
         case TR::Int32: return TR::iudiv;
         case TR::Int64: return TR::ludiv;
         default: TR_ASSERT(0, "no unsigned div opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes remainderOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:    return TR::brem;
         case TR::Int16:   return TR::srem;
         case TR::Int32:   return TR::irem;
         case TR::Int64:   return TR::lrem;
         case TR::Float:   return TR::frem;
         case TR::Double:  return TR::drem;
         default: TR_ASSERT(0, "no div opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes andOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:   return TR::band;
         case TR::Int16:  return TR::sand;
         case TR::Int32:  return TR::iand;
         case TR::Int64:  return TR::land;
         default: TR_ASSERT(false, "no and opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes orOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:   return TR::bor;
         case TR::Int16:  return TR::sor;
         case TR::Int32:  return TR::ior;
         case TR::Int64:  return TR::lor;
         default: TR_ASSERT(false, "no or opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes xorOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:   return TR::bxor;
         case TR::Int16:  return TR::sxor;
         case TR::Int32:  return TR::ixor;
         case TR::Int64:  return TR::lxor;
         default: TR_ASSERT(false, "no xor opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes shiftLeftOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:   return TR::bshl;
         case TR::Int16:  return TR::sshl;
         case TR::Int32:  return TR::ishl;
         case TR::Int64:  return TR::lshl;
         default: TR_ASSERT(false, "no shl opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes shiftRightOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:   return TR::bshr;
         case TR::Int16:  return TR::sshr;
         case TR::Int32:  return TR::ishr;
         case TR::Int64:  return TR::lshr;
         default: TR_ASSERT(false, "no shr opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes unsignedShiftRightOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int32:  return TR::iushr;
         case TR::Int64:  return TR::lushr;
         default: TR_ASSERT(false, "no ushr opcode for datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes negateOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:    return TR::bneg;
         case TR::Int16:   return TR::sneg;
         case TR::Int32:   return TR::ineg;
         case TR::Int64:   return TR::lneg;
         case TR::Float:   return TR::fneg;
         case TR::Double:  return TR::dneg;
         default: TR_ASSERT(0, "no negate opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes integerOpForLongOp(TR::ILOpCodes longOp)
      {
      switch (longOp)
         {
         case TR::ladd:   return TR::iadd;
         case TR::lsub:   return TR::isub;
         case TR::lmul:   return TR::imul;
         case TR::ldiv:   return TR::idiv;
         case TR::lrem:   return TR::irem;
         case TR::labs:   return TR::iabs;
         case TR::lneg:   return TR::ineg;
         case TR::lshr:   return TR::ishr;
         case TR::lushr:  return TR::iushr;
         case TR::lshl:   return TR::ishl;
         default: TR_ASSERT(0,"unexpected long arithmetic op\n");
         }

      return TR::BadILOp;
      }

   static bool isOpCodeAnImplicitNoOp(ILOpCode &opCode)
      {
      if (opCode.isConversion())
         {
         TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
         switch (opCodeValue)
            {
            case TR::i2l:
            case TR::i2f:
            case TR::i2d:
            case TR::l2f:
            case TR::l2d:
            case TR::f2i:
            case TR::f2l:
            case TR::f2b:
            case TR::f2s:
            case TR::f2c:
            case TR::d2i:
            case TR::d2l:
            case TR::d2b:
            case TR::d2s:
            case TR::d2c:
            case TR::b2l:
            case TR::b2f:
            case TR::b2d:
            case TR::s2l:
            case TR::s2f:
            case TR::s2d:
            case TR::su2l:
            case TR::su2f:
            case TR::su2d:
               return false;
            default:
              {
              return true;
              }
            }
         }

      return false;
      }

   static TR::ILOpCodes ifcmpgeOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::Int8:     return isUnsigned ? TR::ifbucmpge : TR::ifbcmpge;
         case TR::Int16:    return isUnsigned ? TR::ifsucmpge : TR::ifscmpge;
         case TR::Int32:    return isUnsigned ? TR::ifiucmpge : TR::ificmpge;
         case TR::Int64:    return isUnsigned ? TR::iflucmpge : TR::iflcmpge;
         case TR::Address:  return TR::ifacmpge;
         case TR::Float:    return TR::iffcmpge;
         case TR::Double:   return TR::ifdcmpge;
         default: TR_ASSERT(0, "no ifcmpge opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpleOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::Int8:     return isUnsigned ? TR::ifbucmple : TR::ifbcmple;
         case TR::Int16:    return isUnsigned ? TR::ifsucmple : TR::ifscmple;
         case TR::Int32:    return isUnsigned ? TR::ifiucmple : TR::ificmple;
         case TR::Int64:    return isUnsigned ? TR::iflucmple : TR::iflcmple;
         case TR::Address:  return TR::ifacmple;
         case TR::Float:    return TR::iffcmple;
         case TR::Double:   return TR::ifdcmple;
         default: TR_ASSERT(0, "no ifcmple opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpgtOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::Int8:     return isUnsigned ? TR::ifbucmpgt : TR::ifbcmpgt;
         case TR::Int16:    return isUnsigned ? TR::ifsucmpgt : TR::ifscmpgt;
         case TR::Int32:    return isUnsigned ? TR::ifiucmpgt : TR::ificmpgt;
         case TR::Int64:    return isUnsigned ? TR::iflucmpgt : TR::iflcmpgt;
         case TR::Address:  return TR::ifacmpgt;
         case TR::Float:    return TR::iffcmpgt;
         case TR::Double:   return TR::ifdcmpgt;
         default: TR_ASSERT(0, "no ifcmpgt opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpltOpCode(TR::DataType type, bool isUnsigned)
      {
      switch(type)
         {
         case TR::Int8:     return isUnsigned ? TR::ifbucmplt : TR::ifbcmplt;
         case TR::Int16:    return isUnsigned ? TR::ifsucmplt : TR::ifscmplt;
         case TR::Int32:    return isUnsigned ? TR::ifiucmplt : TR::ificmplt;
         case TR::Int64:    return isUnsigned ? TR::iflucmplt : TR::iflcmplt;
         case TR::Address:  return TR::ifacmplt;
         case TR::Float:    return TR::iffcmplt;
         case TR::Double:   return TR::ifdcmplt;
         default: TR_ASSERT(0, "no ifcmplt opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpeqOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::ifbcmpeq;
         case TR::Int16:    return TR::ifscmpeq;
         case TR::Int32:    return TR::ificmpeq;
         case TR::Int64:    return TR::iflcmpeq;
         case TR::Address:  return TR::ifacmpeq;
         case TR::Float:    return TR::iffcmpeq;
         case TR::Double:   return TR::ifdcmpeq;
         default: TR_ASSERT(0, "no ifcmpeq opcode for this datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes ifcmpneOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::ifbcmpne;
         case TR::Int16:    return TR::ifscmpne;
         case TR::Int32:    return TR::ificmpne;
         case TR::Int64:    return TR::iflcmpne;
         case TR::Address:  return TR::ifacmpne;
         case TR::Float:    return TR::iffcmpne;
         case TR::Double:   return TR::ifdcmpne;
         default: TR_ASSERT(0, "no ifcmpne opcode for this datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes cmpeqOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::bcmpeq;
         case TR::Int16:    return TR::scmpeq;
         case TR::Int32:    return TR::icmpeq;
         case TR::Int64:    return TR::lcmpeq;
         case TR::Address:  return TR::acmpeq;
         case TR::Float:    return TR::fcmpeq;
         case TR::Double:   return TR::dcmpeq;
         default: TR_ASSERT(0, "no cmpeq opcode for this datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes getComparisonWithoutUnordered(TR::ILOpCodes cmp)
      {
      switch(cmp)
         {
         case TR::fcmpequ:    return TR::fcmpeq;
         case TR::fcmpneu:    return TR::fcmpne;
         case TR::fcmpltu:    return TR::fcmplt;
         case TR::fcmpgeu:    return TR::fcmpge;
         case TR::fcmpgtu:    return TR::fcmpgt;
         case TR::fcmpleu:    return TR::fcmple;
         case TR::dcmpequ:    return TR::dcmpeq;
         case TR::dcmpneu:    return TR::dcmpne;
         case TR::dcmpltu:    return TR::dcmplt;
         case TR::dcmpgeu:    return TR::dcmpge;
         case TR::dcmpgtu:    return TR::dcmpgt;
         case TR::dcmpleu:    return TR::dcmple;
         case TR::iffcmpequ:  return TR::iffcmpeq;
         case TR::iffcmpneu:  return TR::iffcmpne;
         case TR::iffcmpltu:  return TR::iffcmplt;
         case TR::iffcmpgeu:  return TR::iffcmpge;
         case TR::iffcmpgtu:  return TR::iffcmpgt;
         case TR::iffcmpleu:  return TR::iffcmple;
         case TR::ifdcmpequ:  return TR::ifdcmpeq;
         case TR::ifdcmpneu:  return TR::ifdcmpne;
         case TR::ifdcmpltu:  return TR::ifdcmplt;
         case TR::ifdcmpgeu:  return TR::ifdcmpge;
         case TR::ifdcmpgtu:  return TR::ifdcmpgt;
         case TR::ifdcmpleu:  return TR::ifdcmple;
         default:             return TR::BadILOp;
         }
      }

   static TR::ILOpCodes constOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::bconst;
         case TR::Int16:    return TR::sconst;
         case TR::Int32:    return TR::iconst;
         case TR::Int64:    return TR::lconst;
         case TR::Address:  return TR::aconst;
         case TR::Float:    return TR::fconst;
         case TR::Double:   return TR::dconst;
         default: TR_ASSERT(0, "no const opcode for this datatype %s",type.toString());
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes selectOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::bselect;
         case TR::Int16:    return TR::sselect;
         case TR::Int32:    return TR::iselect;
         case TR::Int64:    return TR::lselect;
         case TR::Address:  return TR::aselect;
         default: return TR::BadILOp;
         }
      }

   static TR::ILOpCodes getCorrespondingLogicalComparison(TR::ILOpCodes op)
         {
         switch (op)
            {
            case TR::ifbcmplt:  return TR::ifbucmplt;
            case TR::ifscmplt:  return TR::ifsucmplt;
            case TR::ificmplt:  return TR::ifiucmplt;
            case TR::iflcmplt:  return TR::iflucmplt;
            case TR::bcmplt:    return TR::bucmplt;
            case TR::scmplt:    return TR::sucmplt;
            case TR::icmplt:    return TR::iucmplt;
            case TR::lcmplt:    return TR::lucmplt;

            case TR::ifbcmple:  return TR::ifbucmple;
            case TR::ifscmple:  return TR::ifsucmple;
            case TR::ificmple:  return TR::ifiucmple;
            case TR::iflcmple:  return TR::iflucmple;
            case TR::bcmple:    return TR::bucmple;
            case TR::scmple:    return TR::sucmple;
            case TR::icmple:    return TR::iucmple;
            case TR::lcmple:    return TR::lucmple;


            case TR::ifbcmpgt:  return TR::ifbucmpgt;
            case TR::ifscmpgt:  return TR::ifsucmpgt;
            case TR::ificmpgt:  return TR::ifiucmpgt;
            case TR::iflcmpgt:  return TR::iflucmpgt;
            case TR::bcmpgt:    return TR::bucmpgt;
            case TR::scmpgt:    return TR::sucmpgt;
            case TR::icmpgt:    return TR::iucmpgt;
            case TR::lcmpgt:    return TR::lucmpgt;

            case TR::ifbcmpge:  return TR::ifbucmpge;
            case TR::ifscmpge:  return TR::ifsucmpge;
            case TR::ificmpge:  return TR::ifiucmpge;
            case TR::iflcmpge:  return TR::iflucmpge;
            case TR::bcmpge:    return TR::bucmpge;
            case TR::scmpge:    return TR::sucmpge;
            case TR::icmpge:    return TR::iucmpge;
            case TR::lcmpge:    return TR::lucmpge;

            case TR::vcmplt:    return TR::vucmplt;
            case TR::vcmpgt:    return TR::vucmpgt;
            case TR::vcmple:    return TR::vucmple;
            case TR::vcmpge:    return TR::vucmpge;

            default: return op;
            }
         }

   static TR::ILOpCodes returnOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::ireturn;
         case TR::Int16:    return TR::ireturn;
         case TR::Int32:    return TR::ireturn;
         case TR::Int64:    return TR::lreturn;
         case TR::Address:  return TR::areturn;
         case TR::Float:    return TR::freturn;
         case TR::Double:   return TR::dreturn;
         case TR::NoType:   return TR::Return;   // void return
         default: TR_ASSERT(0, "no return opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes getDirectCall(TR::ILOpCodes ind)
      {
      switch (ind)
         {
         case TR::icalli:    return TR::icall;
         case TR::lcalli:    return TR::lcall;
         case TR::fcalli:    return TR::fcall;
         case TR::dcalli:    return TR::dcall;
         case TR::acalli:    return TR::acall;
         case TR::calli:     return TR::call;
         default: TR_ASSERT(0,"no direct call for this op-code");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes getIndirectCall(TR::DataType type)
      {
      switch (type)
         {
         case TR::Int8:     return TR::icalli;
         case TR::Int16:    return TR::icalli;
         case TR::Int32:    return TR::icalli;
         case TR::Int64:    return TR::lcalli;
         case TR::Float:    return TR::fcalli;
         case TR::Double:   return TR::dcalli;
         case TR::Address:  return TR::acalli;
         case TR::NoType:   return TR::calli;
         default: TR_ASSERT(0,"no indirect call for this type");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes getDirectCall(TR::DataType type)
      {
      switch (type)
         {
         case TR::Int8:     return TR::icall;
         case TR::Int16:    return TR::icall;
         case TR::Int32:    return TR::icall;
         case TR::Int64:    return TR::lcall;
         case TR::Float:    return TR::fcall;
         case TR::Double:   return TR::dcall;
         case TR::Address:  return TR::acall;
         case TR::NoType:   return TR::call;
         default: TR_ASSERT(0,"no direct call for this type");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes convertScalarToVector(TR::ILOpCodes op)
      {
      ILOpCode opcode;
      opcode.setOpCodeValue(op);

      switch (op)
         {
         case TR::bload:
         case TR::cload:
         case TR::sload:
         case TR::iload:
         case TR::lload:
         case TR::fload:
         case TR::dload:
            return TR::vload;
         case TR::bloadi:
         case TR::cloadi:
         case TR::sloadi:
         case TR::iloadi:
         case TR::lloadi:
         case TR::floadi:
         case TR::dloadi:
            return TR::vloadi;
         case TR::bstore:
         case TR::cstore:
         case TR::sstore:
         case TR::istore:
         case TR::lstore:
         case TR::fstore:
         case TR::dstore:
            return TR::vstore;
         case TR::bstorei:
         case TR::cstorei:
         case TR::sstorei:
         case TR::istorei:
         case TR::lstorei:
         case TR::fstorei:
         case TR::dstorei:
            return TR::vstorei;
         case TR::badd:
         case TR::sadd:
         case TR::iadd:
         case TR::ladd:
         case TR::fadd:
         case TR::dadd:
            return TR::vadd;
         case TR::bsub:
         case TR::ssub:
         case TR::isub:
         case TR::lsub:
         case TR::fsub:
         case TR::dsub:
            return TR::vsub;
         case TR::bmul:
         case TR::smul:
         case TR::imul:
         case TR::lmul:
         case TR::fmul:
         case TR::dmul:
            return TR::vmul;
         case TR::bdiv:
         case TR::sdiv:
         case TR::idiv:
         case TR::ldiv:
         case TR::fdiv:
         case TR::ddiv:
            return TR::vdiv;
         case TR::bconst:
         case TR::sconst:
         case TR::iconst:
         case TR::lconst:
         case TR::fconst:
         case TR::dconst:
            return TR::vsplats;
         case TR::brem:
         case TR::srem:
         case TR::irem:
         case TR::lrem:
         case TR::frem:
         case TR::drem:
            return TR::vrem;
         case TR::bneg:
         case TR::sneg:
         case TR::ineg:
         case TR::lneg:
         case TR::fneg:
         case TR::dneg:
            return TR::vneg;
         case TR::bor:
         case TR::sor:
         case TR::ior:
         case TR::lor:
            return TR::vor;
         case TR::band:
         case TR::sand:
         case TR::iand:
         case TR::land:
            return TR::vand;
         case TR::bxor:
         case TR::sxor:
         case TR::ixor:
         case TR::lxor:
            return TR::vxor;
         case TR::bushr:
         case TR::sushr:
         case TR::iushr:
         case TR::lushr:
            return TR::vushr;
         case TR::l2d:
            return TR::vl2vd;
         default:
            return TR::BadILOp;

         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes getRotateOpCodeFromDt(TR::DataType type)
      {
      switch (type)
         {
         case TR::Int32:
            return TR::irol;
         case TR::Int64:
            return TR::lrol;
         default:
            return TR::BadILOp;
         }
      return TR::BadILOp;
      }

   bool isSqrt()
      {
      auto op = getOpCodeValue();
      if (op == TR::fsqrt || op == TR::dsqrt || op == TR::vdsqrt)
         return true;
      return false;
      }


   template <typename T> static TR::ILOpCodes getConstOpCode();
   template <typename T> static TR::ILOpCodes getAddOpCode();
   template <typename T> static TR::ILOpCodes getSubOpCode();
   template <typename T> static TR::ILOpCodes getMulOpCode();
   template <typename T> static TR::ILOpCodes getNegOpCode();

   static void setTarget();

   /**
    * Run a sanity check on our array lengths and contents to ensure
    * everything is in sync.
    */
   static void checkILOpArrayLengths();

protected:

   TR::ILOpCodes _opCode;

   // Access properties words as flags.
   flags32_t properties1()    const { return flags32_t(_opCodeProperties[_opCode].properties1); }
   flags32_t properties2()    const { return flags32_t(_opCodeProperties[_opCode].properties2); }
   flags32_t properties3()    const { return flags32_t(_opCodeProperties[_opCode].properties3); }
   flags32_t properties4()    const { return flags32_t(_opCodeProperties[_opCode].properties4); }
   flags32_t typeProperties() const { return flags32_t(_opCodeProperties[_opCode].typeProperties); }
   TR::ILChildPropType childProperties() const { return _opCodeProperties[_opCode].childTypes; }

   static OpCodeProperties _opCodeProperties[TR::NumIlOps];

private:

   friend class ::TR_DebugExt;

   };

/**
 * FIXME: I suspect that these templates may be an antipattern, involving a fair
 *        amount of mixing between host and target types...
 */
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode< uint8_t>() { return TR::bconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<  int8_t>() { return TR::bconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<uint16_t>() { return TR::sconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode< int16_t>() { return TR::sconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<uint32_t>() { return TR::iconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode< int32_t>() { return TR::iconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<uint64_t>() { return TR::lconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode< int64_t>() { return TR::lconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<   float>() { return TR::fconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<  double>() { return TR::dconst; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getConstOpCode<   void*>() { return TR::aconst; }

template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode< uint8_t>() { return TR::badd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode<  int8_t>() { return TR::badd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode<uint16_t>() { return TR::sadd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode< int16_t>() { return TR::sadd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode<uint32_t>() { return TR::iadd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode< int32_t>() { return TR::iadd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode<uint64_t>() { return TR::ladd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode< int64_t>() { return TR::ladd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode<   float>() { return TR::fadd; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getAddOpCode<  double>() { return TR::dadd; }

template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode< uint8_t>() { return TR::bsub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode<  int8_t>() { return TR::bsub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode<uint16_t>() { return TR::ssub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode< int16_t>() { return TR::ssub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode<uint32_t>() { return TR::isub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode< int32_t>() { return TR::isub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode<uint64_t>() { return TR::lsub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode< int64_t>() { return TR::lsub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode<   float>() { return TR::fsub; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getSubOpCode<  double>() { return TR::dsub; }

template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode<  int8_t>() { return TR::bmul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode< int16_t>() { return TR::smul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode<uint32_t>() { return TR::imul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode< int32_t>() { return TR::imul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode<uint64_t>() { return TR::lmul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode< int64_t>() { return TR::lmul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode<   float>() { return TR::fmul; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getMulOpCode<  double>() { return TR::dmul; }

template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode<  int8_t>() { return TR::bneg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode< int16_t>() { return TR::sneg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode<uint32_t>() { return TR::ineg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode< int32_t>() { return TR::ineg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode<uint64_t>() { return TR::lneg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode< int64_t>() { return TR::lneg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode<   float>() { return TR::fneg; }
template <> inline TR::ILOpCodes OMR::ILOpCode::getNegOpCode<  double>() { return TR::dneg; }

} // namespace OMR

#endif
