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

#ifndef OMR_ILPROPS_INCL
#define OMR_ILPROPS_INCL

#include <il/DataTypes.hpp>

namespace ILTypeProp
   {

   enum
      {
      Size_1                            = 0x00000001,
      Size_2                            = 0x00000002,
      Size_4                            = 0x00000004,
      Size_8                            = 0x00000008,
      Size_16                           = 0x00000010,
      Size_32                           = 0x00000020,
      Size_Mask                         = 0x000000ff,

      HasNoDataType                     = 0x00000100,
      Integer                           = 0x00000200,
      Address                           = 0x00000400,
#ifdef TR_TARGET_64BIT
      Reference                         = (Address        | Size_8),
#else
      Reference                         = (Address        | Size_4),
#endif
      Floating_Point                    = 0x00000800,
      Unsigned                          = 0x00001000,
      Vector                            = 0x00002000,
      LastOMRILTypeProp                 = Vector
      };
   }

/**
 * @brief A type representing the expected number and types of child nodes
 *
 * The expected number of children and their types is stored a 64-bit integer.
 * The 64-bit integer is divided into 8 segments, 8 bits each. The high 8 bits
 * specify the expected number of children. The remaining bits, from high to low,
 * specify the expected type of each child, from last to first. Types are
 * represented by their TR::DataTypes value. A value of 0xff for any field means
 * the field is unspecified.
 *
 *    +-------------+-------------+-------------+-------------+
 *    | child count | child7 type | child6 type | child5 type |
 *    +-------------+-------------+-------------+-------------+
 *    63           56            48            40            32
 *
 *    +-------------+-------------+-------------+-------------+
 *    | child4 type | child3 type | child2 type | child1 type |
 *    +-------------+-------------+-------------+-------------+
 *    31           24            16             8             0
 *
 * Examples:
 *
 * 0x00_00_00_00_00_00_00_00
 *    means a node expects no children
 *
 * 0x01_00_00_00_00_00_00_00 | TR::Int32
 *    means a nodes expects one 32-bit integer child
 *
 * 0x02_00_00_00_00_00_00_00 | (TR::Int16 << 8) | TR::Int8
 *    means a node expects two children: the first an 8-bit integer,
 *    the second a 16-bit integer
 *
 * 0x01_00_00_00_00_00_00_ff
 *    means a node expects one child of an unspecified type
 *
 * 0xff_ff_ff_ff_ff_ff_ff_ff
 *    means a node expects an unspecified number of children of unspecified type
 *
 * 0xff_ff_ff_ff_ff_ff_00_00 | (TR::Int64 << 8) | TR::Int32
 *    means a node expects an unspecified number of children with the first
 *    child being a 32-bit integer, the second child being a 64-bit integer, and
 *    the type of any other chidlren is unspecified
 *
 * To simplify handling of these values, some macros are defined for correctly
 * composing and decomposing them using bit-masks. Some default values are also
 * provided as static const variables. Although this could be implemented as an
 * enum, some compilers used for testing had problems handling 64-bit enums.
 */
namespace TR { typedef uint64_t ILChildPropType; }
namespace ILChildProp
   {

   static const TR::ILChildPropType NoChildren = 0ULL;
   static const TR::ILChildPropType UnspecifiedChildType = 0xffULL;
   static const TR::ILChildPropType UnspecifiedChildCount = 0xffULL;
   static const TR::ILChildPropType Unspecified = 0xffffffffffffffffULL;

   static const TR::ILChildPropType IndirectCallType = 0xffffffffffffff00ULL | TR::Address;

   static const TR::ILChildPropType FirstChildTypeMask   =               0xffULL;
   static const TR::ILChildPropType SecondChildTypeMask  =             0xff00ULL;
   static const TR::ILChildPropType ThirdChildTypeMask   =           0xff0000ULL;
   static const TR::ILChildPropType FourthChildTypeMask  =         0xff000000ULL;
   static const TR::ILChildPropType FifthChildTypeMask   =       0xff00000000ULL;
   static const TR::ILChildPropType SixthChildTypeMask   =     0xff0000000000ULL;
   static const TR::ILChildPropType SeventhChildTypeMask =   0xff000000000000ULL;
   static const TR::ILChildPropType ChildCountMask       = 0xff00000000000000ULL;
   }

// make sure that all TR::DataTypes can fit in one byte
// (remembering that 255 is a reserved value)
static_assert(TR::NumTypes < 254, "There are too many data types to fit in one byte.");

#define FIRST_CHILD(type)     (type) <<  0
#define SECOND_CHILD(type)    (type) <<  8
#define THIRD_CHILD(type)     (type) << 16
#define FOURTH_CHILD(type)    (type) << 24
#define FIFTH_CHILD(type)     (type) << 32
#define SIXTH_CHILD(type)     (type) << 40
#define SEVENTH_CHILD(type)   (type) << 48
#define CHILD_COUNT(c)        (c)    << 56

#define ONE_CHILD(type)                                              CHILD_COUNT(1ULL) | FIRST_CHILD(type)
#define TWO_CHILD(type1, type2)                                      CHILD_COUNT(2ULL) | FIRST_CHILD(type1) | SECOND_CHILD(type2)
#define THREE_CHILD(type1, type2, type3)                             CHILD_COUNT(3ULL) | FIRST_CHILD(type1) | SECOND_CHILD(type2) | THIRD_CHILD(type3)
#define FOUR_CHILD(type1, type2, type3, type4)                       CHILD_COUNT(4ULL) | FIRST_CHILD(type1) | SECOND_CHILD(type2) | THIRD_CHILD(type3) | FOURTH_CHILD(type4)
#define FIVE_CHILD(type1, type2, type3, type4, type5)                CHILD_COUNT(5ULL) | FIRST_CHILD(type1) | SECOND_CHILD(type2) | THIRD_CHILD(type3) | FOURTH_CHILD(type4) | FIFTH_CHILD(type5)
#define SIX_CHILD(type1, type2, type3, type4, type5, type6)          CHILD_COUNT(6ULL) | FIRST_CHILD(type1) | SECOND_CHILD(type2) | THIRD_CHILD(type3) | FOURTH_CHILD(type4) | FIFTH_CHILD(type5) | SIXTH_CHILD(type6)
#define SEVEN_CHILD(type1, type2, type3, type4, type5, type6, type7) CHILD_COUNT(7ULL) | FIRST_CHILD(type1) | SECOND_CHILD(type2) | THIRD_CHILD(type3) | FOURTH_CHILD(type4) | FIFTH_CHILD(type5) | SIXTH_CHILD(type6) | SEVENTH_CHILD(type7)

#define TWO_SAME_CHILD(type)     TWO_CHILD((type), (type))
#define THREE_SAME_CHILD(type)   THREE_CHILD((type), (type), (type))

#define GET_FIRST_CHILD_TYPE(childProp)   TR::DataTypes(((childProp) >>  0) & 0xff)
#define GET_SECOND_CHILD_TYPE(childProp)  TR::DataTypes(((childProp) >>  8) & 0xff)
#define GET_THIRD_CHILD_TYPE(childProp)   TR::DataTypes(((childProp) >> 16) & 0xff)
#define GET_FOURTH_CHILD_TYPE(childProp)  TR::DataTypes(((childProp) >> 24) & 0xff)
#define GET_FIFTH_CHILD_TYPE(childProp)   TR::DataTypes(((childProp) >> 32) & 0xff)
#define GET_SIXTH_CHILD_TYPE(childProp)   TR::DataTypes(((childProp) >> 40) & 0xff)
#define GET_SEVENTH_CHILD_TYPE(childProp) TR::DataTypes(((childProp) >> 48) & 0xff)
#define GET_CHILD_TYPE(index, childProp)  TR::DataTypes(((childProp) >> ((index)*8)) & 0xff)
#define GET_CHILD_COUNT(childProp)        ((childProp) >> 56) & 0xff

// Property flags for property word 1
namespace ILProp1
   {
   enum
      {
      Commutative          = 0x00000001,
      Associative          = 0x00000002,
      Conversion           = 0x00000004,
      Add                  = 0x00000008,
      Sub                  = 0x00000010,
      Mul                  = 0x00000020,
      Div                  = 0x00000040,
      Rem                  = 0x00000080,
      LeftShift            = 0x00000100,
      RightShift           = 0x00000200,
      ShiftLogical         = 0x00000400,
      BooleanCompare       = 0x00000800,
      Branch               = 0x00001000,
      StrBranch            = 0x00002000,
      CompBranchOnly       = 0x00004000,
      Indirect             = 0x00008000,
      LoadVar              = 0x00010000,
      LoadConst            = 0x00020000,
      Load                 = (LoadVar | LoadConst),
      Store                = 0x00040000,
      LoadReg              = 0x00080000,
      StoreReg             = 0x00100000,
      And                  = 0x00200000,
      Or                   = 0x00400000,
      Xor                  = 0x00800000,
      Neg                  = 0x01000000,
      Return               = 0x02000000,
      Call                 = 0x04000000,
      TreeTop              = 0x08000000,
      HasSymbolRef         = 0x10000000,
      Switch               = 0x20000000,
      // Available         = 0x40000000,
      // Available         = 0x80000000,
      };
   }

// Property flags for property word 2
namespace ILProp2
   {
   enum
      {
      MustBeLowered                = 0x00000001,
      ValueNumberShare             = 0x00000002,
      WriteBarrierStore            = 0x00000004,
      CanRaiseException            = 0x00000008,
      Check                        = 0x00000010,
      NullCheck                    = 0x00000020,
      ResolveCheck                 = 0x00000040,
      BndCheck                     = 0x00000080,
      CheckCast                    = 0x00000100,
      ReadBarrierLoad              = 0x00000200,
      MayUseSystemStack            = 0x00000400,
      SupportedForPRE              = 0x00000800,
      LeftRotate                   = 0x00001000,
      UnsignedCompare              = 0x00002000,
      OverflowCompare              = 0x00004000,
      Select                       = 0x00008000,
      SelectAdd                    = 0x00010000,
      SelectSub                    = 0x00020000,
      CondCodeComputation          = 0x00040000,
      JumpWithMultipleTargets      = 0x00080000,
      LoadAddress                  = 0x00100000,
      Max                          = 0x00200000,
      Min                          = 0x00400000,
      New                          = 0x00800000, // object allocation opcodes
      ZeroExtension                = 0x01000000,
      SignExtension                = 0x02000000,
      ByteSwap                     = 0x04000000,
      // Available                 = 0x08000000,
      // Available                 = 0x10000000,
      // Available                 = 0x20000000,
      // Available                 = 0x40000000,
      // Available                 = 0x80000000,
      };
   }

// Property flags for property word 3
namespace ILProp3
   {
   enum
      {
      Fence                       = 0x00000001,
      ExceptionRangeFence         = 0x00000002,
      LikeUse                     = 0x00000004,
      LikeDef                     = 0x00000008,
      SpineCheck                  = 0x00000010,
      ArrayLength                 = 0x00000020,
      SignedExponentiation        = 0x00000040, ///< signed/unsigned base with signed exponent
      UnsignedExponentiation      = 0x00000080, ///< signed/unsigned base with unsigned exponent
      CompareTrueIfLess           = 0x00000100, ///< Result is true if left <  right.  Set for < <= !=
      CompareTrueIfGreater        = 0x00000200, ///< Result is true if left >  right.  Set for > >= !=
      CompareTrueIfEqual          = 0x00000400, ///< Result is true if left == right.  Set for <= >= ==
      CompareTrueIfUnordered      = 0x00000800, ///< Result is true if left and right are incomparable.  (NaN)
      CanUseStoreAsAnAccumulator  = 0x00001000,
      HasBranchChild              = 0x00002000, ///< used in conjunction with jumpwithmultipletargets
      SkipDynamicLitPoolOnInts    = 0x00004000, ///< do not perform dynamicLiteralPool on any integral constant children of this node (used for lengths of array ops)
      Abs                         = 0x00008000,
      VectorReduction             = 0x00010000, ///< Indicates if opcode performs vector reduction that produces scalar result
      Signum                      = 0x00020000, ///< For Xcmp opcodes
      ReverseLoadOrStore          = 0x00040000,
      // Available                = 0x00080000,
      // Available                = 0x00100000,
      // Available                = 0x00200000,
      // Available                = 0x00400000,
      // Available                = 0x00800000,
      // Available                = 0x01000000,
      // Available                = 0x02000000,
      // Available                = 0x04000000,
      // Available                = 0x08000000,
      // Available                = 0x10000000,
      // Available                = 0x20000000,
      // Available                = 0x40000000,
      // Available                = 0x80000000,
      };
   }

#endif
