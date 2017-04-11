/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#ifndef OMR_ILPROPS_INCL
#define OMR_ILPROPS_INCL

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
      MayUseVMThread               = 0x00000200,
      MayUseSystemStack            = 0x00000400,
      SupportedForPRE              = 0x00000800,
      LeftRotate                   = 0x00001000,
      UnsignedCompare              = 0x00002000,
      OverflowCompare              = 0x00004000,
      Ternary                      = 0x00008000,
      TernaryAdd                   = 0x00010000,
      TernarySub                   = 0x00020000,
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
