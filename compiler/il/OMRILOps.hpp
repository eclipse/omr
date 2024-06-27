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

#ifndef OMR_ILOPS_INCL
#define OMR_ILOPS_INCL

#include <stdint.h>
#include "il/DataTypes.hpp"
#include "il/ILHelpers.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILProps.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace TR { class SymbolReference; }

namespace OMR
{

/**
 * Opcode properties.
 */
struct OpCodeProperties
   {
   TR::ILOpCodes       opcode;     ///< Enum value. In the _opCodeProperties table, every entry
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
      TR_ASSERT(opCode < TR::NumAllIlOps, "assertion failure");
      }

  /** \brief
   *     Creates vector opcode given vector operation and the source/result vector or mask type
   *
   *  \param operation
   *     Vector operation
   *
   *  \param vectorType
   *     Type of the vector or mask which is either source or result (or both)
   *
   *  \return
   *     Vector opcode
   */
   static TR::ILOpCodes createVectorOpCode(TR::VectorOperation operation, TR::DataType vectorType)
      {
      TR_ASSERT_FATAL(vectorType.isVector() || vectorType.isMask(), "createVectorOpCode should take vector or mask type\n");

      TR_ASSERT_FATAL(operation < TR::firstTwoTypeVectorOperation, "Vector operation should be one vector type operation\n");

      if (vectorType.isMask())
         vectorType = TR::DataType::vectorFromMaskType(vectorType);

      return (TR::ILOpCodes)(TR::NumScalarIlOps + operation*TR::NumVectorTypes + (vectorType - TR::FirstVectorType));
      }

  /** \brief
   *     Creates vector opcode given vector operation and both source and result vector or mask type
   *
   *  \param operation
   *     Vector operation
   *
   *  \param srcVectorType
   *     Type of the vector or mask which is the source
   *
   *  \param resVectorType
   *     Type of the vector or mask which is the result
   *
   *  \return
   *     Vector opcode
   */
   static TR::ILOpCodes createVectorOpCode(TR::VectorOperation operation, TR::DataType srcVectorType, TR::DataType resVectorType)
      {
      TR_ASSERT_FATAL(srcVectorType.isVector() || srcVectorType.isMask(), "createVectorOpCode should take vector or mask source type\n");
      TR_ASSERT_FATAL(resVectorType.isVector() || resVectorType.isMask(), "createVectorOpCode should take vector or mask result type\n");

      TR_ASSERT_FATAL(operation >= TR::firstTwoTypeVectorOperation, "Vector operation should be two vector type operation\n");

      if (srcVectorType.isMask())
         srcVectorType = TR::DataType::vectorFromMaskType(srcVectorType);

      if (resVectorType.isMask())
         resVectorType = TR::DataType::vectorFromMaskType(resVectorType);

      return (TR::ILOpCodes)(TR::NumScalarIlOps + TR::NumOneVectorTypeOps +
                             (operation - TR::firstTwoTypeVectorOperation) * TR::NumVectorTypes * TR::NumVectorTypes +
                             (srcVectorType - TR::FirstVectorType) * TR::NumVectorTypes +
                             (resVectorType - TR::FirstVectorType));
      }

   bool isTwoTypeVectorOpCode()
         {
         if (!isVectorOpCode()) return false;

         return (getVectorOperation() >= TR::firstTwoTypeVectorOperation);
         }

  /** \brief
   *     Checks if the opcode represents vector operation
   *
   *  \return
   *     True if the opcode represents vector operation, false otherwise
   */
   bool isVectorOpCode() const { return _opCode >= TR::NumScalarIlOps; }

  /** \brief
   *     Checks if the opcode represents vector operation
   *
   *  \param op
   *     Opcode
   *
   *  \return
   *     True if the opcode represents vector operation, false otherwise
   */
   static bool isVectorOpCode(ILOpCode op) { return op._opCode >= TR::NumScalarIlOps; }

  /** \brief
   *     Checks if the opcode is an OMR opcode
   *
   *  \param op
   *     Opcode
   *
   *  \return
   *     True if the opcode is an OMR, false otherwise
   */
   static bool isOMROpCode(ILOpCode op) { return op._opCode <= TR::LastScalarOMROp || isVectorOpCode(op); }

  /** \brief
   *     Returns vector operation
   *
   *  \return
   *     Vector operation
   */
   TR::VectorOperation getVectorOperation() const
      {
      return getVectorOperation(_opCode);
      }

  /** \brief
   *     Returns vector operation
   *
   *  \param op
   *     Opcode
   *
   *  \return
   *     Vector operation
   */
   static TR::VectorOperation getVectorOperation(ILOpCode op)
      {
      TR_ASSERT_FATAL(isVectorOpCode(op), "getVectorOperation() can only be called for vector opcode\n");

      TR::ILOpCodes opcode = op._opCode;

      return (TR::VectorOperation) ((opcode < (TR::NumScalarIlOps + TR::NumOneVectorTypeOps)) ?
                                    (opcode - TR::NumScalarIlOps) / TR::NumVectorTypes :
                                    (opcode - TR::NumScalarIlOps - TR::NumOneVectorTypeOps) / (TR::NumVectorTypes * TR::NumVectorTypes) + TR::firstTwoTypeVectorOperation);
      }

  /** \brief
   *     Returns vector type
   *
   *  \return
   *     Vector type
   */
   TR::DataType getVectorDataType() const
      {
      return getVectorDataType(_opCode);
      }

  /** \brief
   *     Returns vector type
   *
   *  \param op
   *     Opcode
   *
   *  \return
   *     Vector type
   */
   static TR::DataType getVectorDataType(ILOpCode op)
      {
      return getVectorResultDataType(op);
      }

  /** \brief
   *     Returns vector result type
   *
   *  \return
   *     Vector type
   */
   TR::DataType getVectorResultDataType() const
      {
      return getVectorResultDataType(_opCode);
      }

  /** \brief
   *     Returns result vector type
   *
   *  \param op
   *     Opcode
   *
   *  \return
   *     Vector type
   */
   static TR::DataType getVectorResultDataType(ILOpCode op)
      {
      TR_ASSERT_FATAL(isVectorOpCode(op), "getVectorResultDataType() can only be called for vector opcode\n");

      TR::ILOpCodes opcode = op._opCode;

      return (opcode < (TR::NumScalarIlOps + TR::NumOneVectorTypeOps)) ?
             (TR::DataTypes)((opcode - TR::NumScalarIlOps) % TR::NumVectorTypes + TR::FirstVectorType) :
             (TR::DataTypes)(((opcode - TR::NumScalarIlOps - TR::NumOneVectorTypeOps) % (TR::NumVectorTypes * TR::NumVectorTypes)) % TR::NumVectorTypes + TR::FirstVectorType);
      }

  /** \brief
   *     Returns vector source type
   *
   *  \return
   *     Vector type
   */
   TR::DataType getVectorSourceDataType() const
      {
      return getVectorSourceDataType(_opCode);
      }

  /** \brief
   *     Returns source vector type
   *
   *  \param op
   *     Opcode
   *
   *  \return
   *     Vector type
   */
   static TR::DataType getVectorSourceDataType(ILOpCode op)
      {
      TR_ASSERT_FATAL(isVectorOpCode(op), "getVectorSourceDataType() can only be called on vector opcode\n");

      TR::ILOpCodes opcode = op._opCode;

      TR_ASSERT_FATAL(opcode >= (TR::NumScalarIlOps + TR::NumOneVectorTypeOps), "getVectorSourceDataType() can only be called for two vector type opcodes (e.g. vconv)\n");

      return (TR::DataTypes)(((opcode - TR::NumScalarIlOps - TR::NumOneVectorTypeOps) %
                             (TR::NumVectorTypes * TR::NumVectorTypes)) / TR::NumVectorTypes + TR::FirstVectorType);
      }


  /** \brief
   *     Returns index into OMR opcode properties and handler tables. All OMR tables contain VectorOperation
   *     entries following regular opcodes
   *
   *  \return
   *     Index to OMR tables
   */
   int32_t getTableIndex() const { return isVectorOpCode() ? (TR::NumScalarIlOps + getVectorOperation()) : _opCode; }
   static int32_t getTableIndex(TR::VectorOperation vectorOperation) { return TR::NumScalarIlOps + vectorOperation; }

   TR::ILOpCodes getOpCodeValue() const           { return (TR::ILOpCodes)_opCode; }
   TR::ILOpCodes setOpCodeValue(TR::ILOpCodes op) { return (TR::ILOpCodes) (_opCode = op); }

   /// Get the opcode to be used if the children of this opcode are swapped.
   /// e.g. ificmplt --> ificmpgt
   TR::ILOpCodes getOpCodeForSwapChildren() const
      {
      if (!isVectorOpCode()) return _opCodeProperties[getTableIndex()].swapChildrenOpCode;

      TR::VectorOperation operation = (TR::VectorOperation)_opCodeProperties[getTableIndex()].swapChildrenOpCode;

      if (operation < TR::firstTwoTypeVectorOperation)
         return createVectorOpCode(operation, getVectorResultDataType());
      else
         return createVectorOpCode(operation, getVectorSourceDataType(), getVectorResultDataType());
      }

   /// Get the opcode to be used if the sense of this (compare) opcode is reversed.
   /// e.g. ificmplt --> ificmpge
   TR::ILOpCodes getOpCodeForReverseBranch() const
      {
      if (!isVectorOpCode()) return _opCodeProperties[getTableIndex()].reverseBranchOpCode;

      TR::VectorOperation operation = (TR::VectorOperation)_opCodeProperties[getTableIndex()].reverseBranchOpCode;

      if (operation < TR::firstTwoTypeVectorOperation)
         return createVectorOpCode(operation, getVectorResultDataType());
      else
         return createVectorOpCode(operation, getVectorSourceDataType(), getVectorResultDataType());
      }


   /// Get the boolean compare opcode that corresponds to this compare-and-branch
   /// opcode.
   /// e.g. ificmplt --> icmplt
   TR::ILOpCodes convertIfCmpToCmp()
      { return _opCodeProperties[getTableIndex()].booleanCompareOpCode; }

   /// Get the compare-and-branch opcode that corresponds to this boolean compare
   ///e.g. icmplt --> ificmplt
   TR::ILOpCodes convertCmpToIfCmp()
      { return _opCodeProperties[getTableIndex()].ifCompareOpCode; }

   TR::DataType getDataType() const
      { return getDataType(_opCode); }

   static TR::DataType getDataType(TR::ILOpCodes op)
         {
         if (!isVectorOpCode(op)) return _opCodeProperties[op].dataType;

         ILOpCode opcode(op);

         if (opcode.isVectorResult())
            {
            return getVectorResultDataType(op);
            }
         else if (opcode.isMaskResult())
            {
            TR::DataType dt = getVectorResultDataType(op);
            return TR::DataType::createMaskType(dt.getVectorElementType(), dt.getVectorLength());
            }
         else if (opcode.isVectorElementResult())
            {
            return getVectorResultDataType(op).getVectorElementType();
            }
         else
            {
            return _opCodeProperties[opcode.getTableIndex()].dataType;
            }
         }

   TR::DataType getType() const                  { return getDataType(); }
   static TR::DataType getType(TR::ILOpCodes op) { return getDataType(op); }

   const char *getName() const { return _opCodeProperties[getTableIndex()].name; }

   // Query Functions
   uint32_t getSize() const
         { return isVectorOpCode() ? OMR::DataType::getSize(getDataType())
                                   : typeProperties().getValue(ILTypeProp::Size_Mask); }

   static uint32_t getSize(TR::ILOpCodes op)
         { return isVectorOpCode(op) ? OMR::DataType::getSize(getDataType(op))
                                   : ILOpCode(op).getSize(); }

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
   bool isVectorResult()             const { return typeProperties().testAny(ILTypeProp::VectorResult); }
   bool isMaskResult()               const { return typeProperties().testAny(ILTypeProp::MaskResult); }
   bool isVectorElementResult()      const { return typeProperties().testAny(ILTypeProp::VectorElementResult); }
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
   bool isVectorMasked()             const { return properties1().testAny(ILProp1::VectorMasked); }

   /**
    * @brief This query must return true for any opcode that may appear at the
    *        top of a tree (i.e., any node that a `TR::TreeTop` points to).  For
    *        example, a store opcode would return true because it may appear under
    *        a TR::TreeTop, even though it might also appear as a child of a NULLCHK
    *        node in another circumstance.
    */
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
   bool isMaskReduction()            const { return properties3().testAny(ILProp3::MaskReduction); }
   bool isSignum()                   const { return properties3().testAny(ILProp3::Signum); }



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
      return isCall()                              &&
             getOpCodeValue() != TR::arraycopy     &&
             getOpCodeValue() != TR::arrayset      &&
             getOpCodeValue() != TR::bitOpMem      &&
             getOpCodeValue() != TR::arraycmp      &&
             getOpCodeValue() != TR::arraycmplen;
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
      return getOpCodeValue() == TR::bitOpMem      ||
             getOpCodeValue() == TR::arrayset      ||
             getOpCodeValue() == TR::arraycmp      ||
             getOpCodeValue() == TR::arraycopy     ||
             getOpCodeValue() == TR::arraycmplen;
      }

   static TR::ILOpCodes getDataTypeConversion(TR::DataType t1, TR::DataType t2);
   static TR::ILOpCodes getDataTypeBitConversion(TR::DataType t1, TR::DataType t2);

   static TR::ILOpCodes getProperConversion(TR::DataType sourceDataType, TR::DataType targetDataType, bool needUnsignedConversion)
      {
      TR::ILOpCodes op = getDataTypeConversion(sourceDataType, targetDataType);
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

   static TR::ILOpCodes indirectStoreOpCode(TR::DataType type)
      {
      switch(type)
         {
         case TR::Int8:     return TR::bstorei;
         case TR::Int16:    return TR::sstorei;
         case TR::Int32:    return TR::istorei;
         case TR::Int64:    return TR::lstorei;
         case TR::Address:  return TR::astorei;
         case TR::Float:    return TR::fstorei;
         case TR::Double:   return TR::dstorei;
         default: TR_ASSERT(0, "no load opcode for this datatype");
        }
      return TR::BadILOp;
      }

   static TR::ILOpCodes absOpCode(TR::DataType type)
      {
      if (type.isVector()) return createVectorOpCode(TR::vabs, type);

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
      if (type.isVector()) return createVectorOpCode(TR::vadd, type);

      switch(type)
         {
         case TR::Int8:     return TR::badd;
         case TR::Int16:    return TR::sadd;
         case TR::Int32:    return TR::iadd;
         case TR::Int64:    return TR::ladd;
         case TR::Address:  return (is64Bit) ? TR::aladd : TR::aiadd;
         case TR::Float:    return TR::fadd;
         case TR::Double:   return TR::dadd;
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
      if (type.isVector()) return createVectorOpCode(TR::vsub, type);

      switch(type)
         {
         case TR::Int8:    return TR::bsub;
         case TR::Int16:   return TR::ssub;
         case TR::Int32:   return TR::isub;
         case TR::Int64:   return TR::lsub;
         case TR::Float:   return TR::fsub;
         case TR::Double:  return TR::dsub;
         default: TR_ASSERT(0, "no sub opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes multiplyOpCode(TR::DataType type)
      {
      if (type.isVector()) return createVectorOpCode(TR::vmul, type);

      switch(type)
         {
         case TR::Int8:    return TR::bmul;
         case TR::Int16:   return TR::smul;
         case TR::Int32:   return TR::imul;
         case TR::Int64:   return TR::lmul;
         case TR::Float:   return TR::fmul;
         case TR::Double:  return TR::dmul;
         default: TR_ASSERT(0, "no mul opcode for this datatype");
         }
      return TR::BadILOp;
      }

   static TR::ILOpCodes divideOpCode(TR::DataType type)
      {
      if (type.isVector()) return createVectorOpCode(TR::vdiv, type);

      switch(type)
         {
         case TR::Int8:    return TR::bdiv;
         case TR::Int16:   return TR::sdiv;
         case TR::Int32:   return TR::idiv;
         case TR::Int64:   return TR::ldiv;
         case TR::Float:   return TR::fdiv;
         case TR::Double:  return TR::ddiv;
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

   static TR::ILOpCodes reductionToVerticalOpcode(TR::ILOpCodes op, TR::VectorLength vectorLength)
      {
      ILOpCode opcode;
      opcode.setOpCodeValue(op);
      TR::VectorOperation operation;

      switch (opcode.getVectorOperation())
         {
         case TR::vmreductionAdd:
         case TR::vreductionAdd:
            operation = TR::vadd;
            break;
         case TR::vmreductionMul:
         case TR::vreductionMul:
            operation = TR::vmul;
            break;
         case TR::vmreductionAnd:
         case TR::vreductionAnd:
            operation = TR::vand;
            break;
         case TR::vmreductionOr:
         case TR::vreductionOr:
            operation = TR::vor;
            break;
         case TR::vmreductionXor:
         case TR::vreductionXor:
            operation = TR::vxor;
            break;
         case TR::vmreductionMin:
         case TR::vreductionMin:
            operation = TR::vmin;
            break;
         case TR::vmreductionMax:
         case TR::vreductionMax:
            operation = TR::vmax;
            break;
         default:
            return TR::BadILOp;
         }

      return ILOpCode::createVectorOpCode(operation, TR::DataType::createVectorType(opcode.getDataType().getDataType(), vectorLength));
      }

   static TR::ILOpCodes convertScalarToVector(TR::ILOpCodes op, TR::VectorLength vectorLength)
      {
      ILOpCode opcode;
      opcode.setOpCodeValue(op);

      TR::DataType elementType = opcode.getDataType();
      if (!elementType.isVectorElement()) return TR::BadILOp;

      TR::DataTypes vectorType = TR::DataType::createVectorType(elementType.getDataType(), vectorLength);
      TR::VectorOperation vectorOperation;

      switch (op)
         {
         case TR::fsqrt:
         case TR::dsqrt:
            vectorOperation = TR::vsqrt;
            break;
         case TR::imin:
         case TR::lmin:
         case TR::fmin:
         case TR::dmin:
            vectorOperation = TR::vmin;
            break;
         case TR::imax:
         case TR::lmax:
         case TR::fmax:
         case TR::dmax:
            vectorOperation = TR::vmax;
            break;
         case TR::bload:
         case TR::sload:
         case TR::iload:
         case TR::lload:
         case TR::fload:
         case TR::dload:
            vectorOperation = TR::vload;
            break;
         case TR::bloadi:
         case TR::sloadi:
         case TR::iloadi:
         case TR::lloadi:
         case TR::floadi:
         case TR::dloadi:
            vectorOperation = TR::vloadi;
            break;
         case TR::bstore:
         case TR::sstore:
         case TR::istore:
         case TR::lstore:
         case TR::fstore:
         case TR::dstore:
            vectorOperation = TR::vstore;
            break;
         case TR::bstorei:
         case TR::sstorei:
         case TR::istorei:
         case TR::lstorei:
         case TR::fstorei:
         case TR::dstorei:
            vectorOperation = TR::vstorei;
            break;
         case TR::badd:
         case TR::sadd:
         case TR::iadd:
         case TR::ladd:
         case TR::fadd:
         case TR::dadd:
            vectorOperation = TR::vadd;
            break;
         case TR::bsub:
         case TR::ssub:
         case TR::isub:
         case TR::lsub:
         case TR::fsub:
         case TR::dsub:
            vectorOperation = TR::vsub;
            break;
         case TR::bmul:
         case TR::smul:
         case TR::imul:
         case TR::lmul:
         case TR::fmul:
         case TR::dmul:
            vectorOperation = TR::vmul;
            break;
         case TR::bdiv:
         case TR::sdiv:
         case TR::idiv:
         case TR::ldiv:
         case TR::fdiv:
         case TR::ddiv:
            vectorOperation = TR::vdiv;
            break;
         case TR::bconst:
         case TR::sconst:
         case TR::iconst:
         case TR::lconst:
         case TR::fconst:
         case TR::dconst:
            vectorOperation = TR::vsplats;
            break;
         case TR::bneg:
         case TR::sneg:
         case TR::ineg:
         case TR::lneg:
         case TR::fneg:
         case TR::dneg:
            vectorOperation = TR::vneg;
            break;
         case TR::iabs:
         case TR::labs:
         case TR::fabs:
         case TR::dabs:
            vectorOperation = TR::vabs;
            break;
         case TR::bor:
         case TR::sor:
         case TR::ior:
         case TR::lor:
            vectorOperation = TR::vor;
            break;
         case TR::band:
         case TR::sand:
         case TR::iand:
         case TR::land:
            vectorOperation = TR::vand;
            break;
         case TR::bxor:
         case TR::sxor:
         case TR::ixor:
         case TR::lxor:
            vectorOperation = TR::vxor;
            break;
         case TR::l2d:
            return ILOpCode::createVectorOpCode(TR::vconv, TR::DataType::createVectorType(TR::Int64, vectorLength),
                                                            TR::DataType::createVectorType(TR::Double, vectorLength));
         case TR::bshl:
         case TR::sshl:
         case TR::ishl:
         case TR::lshl:
            vectorOperation = TR::vshl;
            break;
         case TR::bshr:
         case TR::sshr:
         case TR::ishr:
         case TR::lshr:
            vectorOperation = TR::vshr;
            break;
         case TR::bushr:
         case TR::sushr:
         case TR::iushr:
         case TR::lushr:
            vectorOperation = TR::vushr;
            break;
         case TR::irol:
         case TR::lrol:
            vectorOperation = TR::vrol;
            break;
         case TR::ipopcnt:
         case TR::lpopcnt:
            vectorOperation = TR::vpopcnt;
            break;
         case TR::inotz:
         case TR::lnotz:
            vectorOperation = TR::vnotz;
            break;
         case TR::inolz:
         case TR::lnolz:
            vectorOperation = TR::vnolz;
            break;
         case TR::bcompressbits:
         case TR::scompressbits:
         case TR::icompressbits:
         case TR::lcompressbits:
            vectorOperation = TR::vcompressbits;
            break;
         case TR::bexpandbits:
         case TR::sexpandbits:
         case TR::iexpandbits:
         case TR::lexpandbits:
            vectorOperation = TR::vexpandbits;
            break;
         case TR::sbyteswap:
         case TR::ibyteswap:
         case TR::lbyteswap:
            vectorOperation = TR::vbyteswap;
            break;
         default:
            return TR::BadILOp;
         }

      return ILOpCode::createVectorOpCode(vectorOperation, vectorType);
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
      if (op == TR::fsqrt || op == TR::dsqrt)
         return true;

      if (isVectorOpCode(op) && getVectorOperation(op) == TR::vsqrt)
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
   flags32_t properties1()    const { return flags32_t(_opCodeProperties[getTableIndex()].properties1); }
   flags32_t properties2()    const { return flags32_t(_opCodeProperties[getTableIndex()].properties2); }
   flags32_t properties3()    const { return flags32_t(_opCodeProperties[getTableIndex()].properties3); }
   flags32_t properties4()    const { return flags32_t(_opCodeProperties[getTableIndex()].properties4); }
   flags32_t typeProperties() const { return flags32_t(_opCodeProperties[getTableIndex()].typeProperties); }
   TR::ILChildPropType childProperties() const { return _opCodeProperties[getTableIndex()].childTypes; }

   static const int32_t opCodePropertiesSize = TR::NumScalarIlOps + TR::NumVectorOperations;
   static OpCodeProperties _opCodeProperties[opCodePropertiesSize];
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
