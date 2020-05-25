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

#ifndef OMR_DATATYPE_INCL
#define OMR_DATATYPE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_DATATYPE_CONNECTOR
#define OMR_DATATYPE_CONNECTOR
namespace OMR { class DataType; }
namespace OMR { typedef OMR::DataType DataTypeConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "il/ILOpCodes.hpp"
#include "infra/Annotations.hpp"

#if defined(TR_HOST_ARM) && !defined(__VFP_FP__)
#include "env/CompilerEnv.hpp"
#endif

namespace TR { class DataType; }

typedef int32_t CASECONST_TYPE;

/* FIXME: Deprecate this and remove */
#define CONSTANT64(x) x##LL


#define LONG_SHIFT_MASK 63
#define INT_SHIFT_MASK 31

/**
 * @name OMRDataTypeFPLimits
 *
 * Floating point limits
 *
 * @{
 */

#define FLOAT_POS_INFINITY    0x7F800000
#define FLOAT_NEG_INFINITY    0xFF800000
#define FLOAT_NAN             0x7FC00000
#define FLOAT_NAN_1_LOW       0x7F800001
#define FLOAT_NAN_1_HIGH      0x7FFFFFFF
#define FLOAT_NAN_2_LOW       0xFF800001
#define FLOAT_NAN_2_HIGH      0xFFFFFFFF

/* FIXME: Deprecate this and remove. Hopefully we don't have half-endian ARM implementatations any more.
 * The two halves of a double are in opposite order on ARM-based Linux.
 * VFP uses the standard format.
 */
#if defined(TR_HOST_ARM) && !defined(__VFP_FP__)
#define DOUBLE_ORDER(d) ( TR::Compiler->host.cpu.isLittleEndian() ? ( ( ((uint64_t)(d)) >> 32 ) | ( ((uint64_t)(d)) << 32 ) ) : (d) )
#else
#define DOUBLE_ORDER(d) (d)
#endif

#define DOUBLE_POS_INFINITY   DOUBLE_ORDER(((uint64_t)0x7FF00000)<<32)
#define DOUBLE_NEG_INFINITY   DOUBLE_ORDER(((uint64_t)0xFFF00000)<<32)
#define DOUBLE_NAN            DOUBLE_ORDER(((uint64_t)0x7FF80000)<<32)

#define DOUBLE_NAN_1_LOW      ((uint64_t)((((uint64_t)0x7FF00000)<<32)+1))
#define DOUBLE_NAN_1_HIGH     ((uint64_t)TR::getMaxSigned<TR::Int64>())
#define DOUBLE_NAN_2_LOW      ((uint64_t)((((uint64_t)0xFFF00000)<<32)+1))
#define DOUBLE_NAN_2_HIGH     ((uint64_t)((int64_t)-1))
#define IN_DOUBLE_NAN_1_RANGE(d) (DOUBLE_ORDER(d) >= DOUBLE_NAN_1_LOW && DOUBLE_ORDER(d) <= DOUBLE_NAN_1_HIGH)
#define IN_DOUBLE_NAN_2_RANGE(d) (DOUBLE_ORDER(d) >= DOUBLE_NAN_2_LOW && DOUBLE_ORDER(d) <= DOUBLE_NAN_2_HIGH)

#define DOUBLE_ONE            DOUBLE_ORDER(((uint64_t)0x3FF00000)<<32)
#define DOUBLE_NEG_ZERO       DOUBLE_ORDER(((uint64_t)(FLOAT_NEG_ZERO))<<32)
#define DOUBLE_POS_ZERO       ((uint64_t)0)

#define LONG_DOUBLE_ONE_LOW       0
#define LONG_DOUBLE_ONE_HIGH      DOUBLE_ORDER(((uint64_t)0x3FFF0000)<<32)

#define FLOAT_ONE             0x3F800000
#define FLOAT_NEG_ZERO        0x80000000
#define FLOAT_POS_ZERO        0

#define LONG_DOUBLE_POS_ZERO_LOW     ((uint64_t)0)
#define LONG_DOUBLE_POS_ZERO_HIGH    ((uint64_t)0)

#define FLOAT_ONE_HALF              0x3FE00000
#define DOUBLE_ONE_HALF             DOUBLE_ORDER(((uint64_t)0x3FE00000)<<32)
#define LONG_DOUBLE_ONE_HALF_HIGH   DOUBLE_ORDER(((uint64_t)0x3FFE0000)<<32)
#define LONG_DOUBLE_ONE_HALF_LOW    ((uint64_t)0)

/** @} */

// These are the max sizes representable on a TR::Node a codegen may have smaller limits when deciding what operations to inline
// If these sizes are changed then the sizes in the Node::DecimalInfo bit container below may have to be changed
#define TR_MAX_DECIMAL_PRECISION   63  // NOTE: update J9::DataType::getMaxPackedDecimalPrecision() when changing this constant

#define TR_VECTOR_REGISTER_SIZE 16
#define TR_MAX_INPUT_PACKED_DECIMAL_PRECISION 31

#define TR_FIRST_VALID_SIGN_CODE 0x0a
#define TR_ALTERNATE_PLUS_CODE   0x0a       // plus
#define TR_ALTERNATE_MINUS_CODE  0x0b       // minus
#define TR_PREFERRED_PLUS_CODE   0x0c       // plus (preferred)
#define TR_PREFERRED_MINUS_CODE  0x0d       // minus (preferred)
#define TR_ALTERNATE_PLUS_CODE_2 0x0e       // plus
#define TR_ZONED_PLUS         0x0f       // plus (zone)
#define TR_LAST_VALID_SIGN_CODE  0x0f

#define TR_INVALID_SIGN_CODE     0x00 // reserve zero to be the invalid sign marker
#define TR_IGNORED_SIGN_CODE     (-1)   // a special marker value for when a setSign operation can set any or no sign code value (as it will be ignored by consumer)

#define TR_BCD_SIGN_CHAR_SIZE 1                 // one byte for leading leading +/-/u char
#define TR_BCD_STRLEN (TR_MAX_DECIMAL_PRECISION+TR_BCD_SIGN_CHAR_SIZE+1) // +1 for sign, +1 for terminating null -- for pretty printed string e.g. "+1234"

#define TR_MAX_OTYPE_SIZE TR::getMaxSigned<TR::Int32>()

enum TR_YesNoMaybe
   {
   TR_no,
   TR_yes,
   TR_maybe
   };

// NOTE : update these tables when adding/modifying enum TR_RawBCDSignCode
// OMRDataTypes.cpp
//    bcdToRawSignCodeMap
//    rawToBCDSignCodeMap
// OMRDataTypes.cpp
//    _TR_RawBCDSignCodeNames
//    _TR_RawBCDSignCodeValues
// ras/Tree.cpp
//    pRawSignCodeNames
// And update the value table in il/OMRDataTypes.cpp (_TR_RawBCDSignCodeValues)
// And ensure the bit fields in Node::DecimalInfo are still big enough -- keeping in mind that the implementation defined choice for the underlying type may be signed
// (see note below next to 'last_raw_bcd_sign    = raw_bcd_sign_0xf')
#define TR_NUM_RAW_SIGN_CODES 4
enum TR_RawBCDSignCode
   {
   raw_bcd_sign_unknown = 0,
   raw_bcd_sign_0xc     = 1,
   first_valid_sign     = raw_bcd_sign_0xc,
   raw_bcd_sign_0xd     = 2,
   raw_bcd_sign_0xf     = 3,
   last_raw_bcd_sign    = raw_bcd_sign_0xf, // NOTE: the TR_RawBCDSignCode fields in Node::DecimalInfo are 3 bits so the max value for a raw_bcd_sign_* is 7 (enum range is -8->7)
   num_raw_bcd_sign_codes
   };

enum TR_SignCodeSize
   {
   UnknownSignCodeSize,
   EmbeddedHalfByte,
   SeparateOneByte,
   SeparateTwoByte
   };

enum TR_DigitSize
   {
   UnknownDigitSize,
   HalfByteDigit,
   OneByteDigit,
   TwoByteDigit
   };

// Per the C++ language specification, using reinterpret_cast<>() is considered inherently
// platform-specific and non-portable.  The recommended way to coerce pointers is by
// chaining static casts and converting through void*.  For better readability and for
// ease of grep, I feel we should add a custom cast for this scenario:
template <typename T> T pointer_cast(void *p) { return static_cast<T>(p); }

enum TR_SharedCacheHint
   {
   TR_NoHint                = 0x0000,   // when to register:
   TR_HintUpgrade           = 0x0001,   // on decide
   TR_HintInline            = 0x0002,   // on decide
   TR_HintHot               = 0x0004,   // on complete
   TR_HintScorching         = 0x0008,   // on complete
   TR_HintEDO               = 0x0010,   // on decide
   TR_HintDLT               = 0x0020,   // on decide
   TR_HintFailedCHTable     = 0x0040,   // on fail
   TR_HintMethodCompiledDuringStartup = 0x0080,   // on complete
   TR_HintFailedWarm        = 0x0100,   // on fail
   TR_HintFailedHot         = 0x0200,   // on fail
   TR_HintFailedScorching   = 0x0400,   // on fail
   TR_HintFailedValidation  = 0x0800,   // on fail
   TR_HintLargeMemoryMethodW= 0x1000,   // Compilation at warm requires large amount of JIT scratch memory
                                        // Used to contain footprint during startup by inhibiting AOT forced
                                        // upgrades at warm instead of cold
   TR_HintLargeMemoryMethodC= 0x2000,   // Compilation at cold requires large amount of JIT scratch memory
                                        // Do not perform AOT upgrades during startup
   TR_HintLargeCompCPUW     = 0x4000,   // Compilation takes a long time to perform
   TR_HintLargeCompCPUC     = 0x8000,   // Compilation takes a long time to perform
   TR_HintAny               = 0xFFFF
   };

#define NULLVALUE          0

namespace TR
{

/**
 * Data type supported by OMR and whatever the configured language is.
 */
enum DataTypes
   {
   NoType=0,
   Int8,
   Int16,
   Int32,
   Int64,
   Float,
   Double,
   Address,
   VectorInt8,
   VectorInt16,
   VectorInt32,
   VectorInt64,
   VectorFloat,
   VectorDouble,
   Aggregate,
   NumOMRTypes,
#include "il/DataTypesEnum.hpp"
   NumTypes
   };
}

/**
 * @name OMRDataTypeIntegerLimits
 *
 * These define the limits of OMR DataTypes. Note that these MAX_INT (defined by us)
 * is supposed to be distinct from INT_MAX (defined by the system). The former
 * corresponds to the TR::Int32 DataType whereas the latter is the limit of the
 * integer on the system. Be careful to use the correct version.
 *
 * @{
 */

namespace TR
{

   template <TR::DataTypes T> inline int64_t getMinSigned() { static_assert(T == -1, "getMinSigned is not valid for this type"); return 0; }
   template <TR::DataTypes T> inline uint64_t getMinUnsigned() { static_assert(T == -1, "getMinUnsigned is not valid for this type"); return 0; }
   template <TR::DataTypes T> inline int64_t getMaxSigned() { static_assert(T == -1, "getMaxSigned is not valid for this type"); return 0; }
   template <TR::DataTypes T> inline uint64_t getMaxUnsigned() { static_assert(T == -1, "getMaxUnsigned is not valid for this type"); return 0; }
   template <TR::DataTypes T> inline int32_t getMaxSignedPrecision() { static_assert(T == -1, "getMaxSignedPrecision is not valid for this type"); return 0; }
   template <TR::DataTypes T> inline int32_t getMaxUnsignedPrecision() { static_assert(T == -1, "getMaxUnsignedPrecision is not valid for this type"); return 0; }

   template <> inline int64_t getMinSigned<TR::Int8 >() { return -128; }
   template <> inline int64_t getMinSigned<TR::Int16>() { return -32768; }
   template <> inline int64_t getMinSigned<TR::Int32>() { return ((int32_t)0X80000000); }
   template <> inline int64_t getMinSigned<TR::Int64>() { return (((int64_t)1)<<63) /**< -9223372036854775808 */; }

   template <> inline uint64_t getMinUnsigned<TR::Int8 >() { return 0; }
   template <> inline uint64_t getMinUnsigned<TR::Int16>() { return 0; }
   template <> inline uint64_t getMinUnsigned<TR::Int32>() { return ((uint32_t)0); }
   template <> inline uint64_t getMinUnsigned<TR::Int64>() { return ((uint64_t)0); }

   template <> inline int64_t getMaxSigned<TR::Int8 >() { return 127; }
   template <> inline int64_t getMaxSigned<TR::Int16>() { return 32767; }
   template <> inline int64_t getMaxSigned<TR::Int32>() { return ((int32_t)0X7FFFFFFF); }
   template <> inline int64_t getMaxSigned<TR::Int64>() { return ((int64_t)(((uint64_t)((int64_t)-1))>>1)) /**<  9223372036854775807 */; }

   template <> inline uint64_t getMaxUnsigned<TR::Int8 >() { return 255; }
   template <> inline uint64_t getMaxUnsigned<TR::Int16>() { return 65535; }
   template <> inline uint64_t getMaxUnsigned<TR::Int32>() { return ((uint32_t)0xFFFFFFFF); }
   template <> inline uint64_t getMaxUnsigned<TR::Int64>() { return ((uint64_t)-1); }

   template <> inline int32_t getMaxSignedPrecision<TR::Int8 >() { return 3; }
   template <> inline int32_t getMaxSignedPrecision<TR::Int16>() { return 5; }
   template <> inline int32_t getMaxSignedPrecision<TR::Int32>() { return 10; }
   template <> inline int32_t getMaxSignedPrecision<TR::Int64>() { return 19; }

   template <> inline int32_t getMaxUnsignedPrecision<TR::Int8 >() { return 3; }
   template <> inline int32_t getMaxUnsignedPrecision<TR::Int16>() { return 5; }
   template <> inline int32_t getMaxUnsignedPrecision<TR::Int32>() { return 10; }
   template <> inline int32_t getMaxUnsignedPrecision<TR::Int64>() { return 20; }

} // namespace TR

/** @} */

namespace OMR
{

class OMR_EXTENSIBLE DataType
   {
public:
   DataType() : _type(TR::NoType) { }
   DataType(TR::DataTypes t) : _type(t) { }

   inline TR::DataType* self();
   inline const TR::DataType* self() const;

   TR::DataTypes getDataType() const { return _type; }

   inline TR::DataType& operator=(const TR::DataType& rhs);
   inline TR::DataType& operator=(TR::DataTypes rhs);

   inline bool operator==(const TR::DataType& rhs);
   inline bool operator==(TR::DataTypes rhs);

   inline bool operator!=(const TR::DataType& rhs);
   inline bool operator!=(TR::DataTypes rhs);

   inline bool operator<=(const TR::DataType& rhs);
   inline bool operator<=(TR::DataTypes rhs);

   inline bool operator<(const TR::DataType& rhs);
   inline bool operator<(TR::DataTypes rhs);

   inline bool operator>=(const TR::DataType& rhs);
   inline bool operator>=(TR::DataTypes rhs);

   inline bool operator>(const TR::DataType& rhs);
   inline bool operator>(TR::DataTypes rhs);

   inline operator int();

   inline bool isInt8();
   inline bool isInt16();
   inline bool isInt32();
   inline bool isInt64();

   inline bool isIntegral();

   inline bool isFloatingPoint();
   inline bool isVector();
   inline bool isBFPorHFP();
   inline bool isDouble();
   inline bool isFloat();

   inline bool isAddress();
   inline bool isAggregate();

   bool canGetMaxPrecisionFromType();
   int32_t getMaxPrecisionFromType();

   TR::DataType getVectorIntegralType();
   TR::DataType getVectorElementType();

   TR::DataType vectorToScalar();
   TR::DataType scalarToVector();

   const char * toString() const;

   static TR::DataType getIntegralTypeFromPrecision(int32_t precision);

   static TR::DataType getFloatTypeFromSize(int32_t size);

   static TR::ILOpCodes getDataTypeConversion(TR::DataType t1, TR::DataType t2);

   static TR::ILOpCodes getDataTypeBitConversion(TR::DataType t1, TR::DataType t2);

   static const char    * getName(TR::DataType dt);
   static int32_t         getSize(TR::DataType dt);
   static void            setSize(TR::DataType dt, int32_t newValue);
   static const char    * getPrefix(TR::DataType dt);

   template <typename T> static bool isSignedInt8()  { return false; }
   template <typename T> static bool isSignedInt16() { return false; }
   template <typename T> static bool isSignedInt32() { return false; }
   template <typename T> static bool isSignedInt64() { return false; }
   template <typename T> static bool isSignedInt()   { return isSignedInt8<T>()  || isSignedInt16<T>() || isSignedInt32<T>() || isSignedInt64<T>(); }
   template <typename T> static bool isInt32()       { return isSignedInt32<T>() || isUnsignedInt32<T>(); }

   template <typename T> static bool isUnsignedInt8()  { return false; }
   template <typename T> static bool isUnsignedInt16() { return false; }
   template <typename T> static bool isUnsignedInt32() { return false; }
   template <typename T> static bool isUnsignedInt64() { return false; }
   template <typename T> static bool isUnsignedInt()   { return isUnsignedInt8<T>()  || isUnsignedInt16<T>() || isUnsignedInt32<T>() || isUnsignedInt64<T>(); }

protected:
   TR::DataTypes _type;

   };

template <> inline bool OMR::DataType::isSignedInt8 < int8_t>() { return true; }
template <> inline bool OMR::DataType::isSignedInt16<int16_t>() { return true; }
template <> inline bool OMR::DataType::isSignedInt32<int32_t>() { return true; }
template <> inline bool OMR::DataType::isSignedInt64<int64_t>() { return true; }

template <> inline bool OMR::DataType::isUnsignedInt8 < uint8_t>() { return true; }
template <> inline bool OMR::DataType::isUnsignedInt16<uint16_t>() { return true; }
template <> inline bool OMR::DataType::isUnsignedInt32<uint32_t>() { return true; }
template <> inline bool OMR::DataType::isUnsignedInt64<uint64_t>() { return true; }

} // namespace OMR

#endif
