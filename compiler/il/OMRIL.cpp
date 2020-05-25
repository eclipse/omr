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

#include "il/IL.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Assert.hpp"


TR::ILOpCodes OMR::IL::opCodesForConst[] =
   {
   TR::BadILOp,  // NoType
   TR::bconst,   // Int8
   TR::sconst,   // Int16
   TR::iconst,   // Int32
   TR::lconst,   // Int64
   TR::fconst,   // Float
   TR::dconst,   // Double
   TR::aconst,   // Address
   TR::vconst,   // VectorInt8
   TR::vconst,   // VectorInt16
   TR::vconst,   // VectorInt32
   TR::vconst,   // VectorInt64
   TR::vconst,   // VectorFloat
   TR::vconst,   // VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForDirectLoad[] =
   {
   TR::BadILOp,  // NoType
   TR::bload,    // Int8
   TR::sload,    // Int16
   TR::iload,    // Int32
   TR::lload,    // Int64
   TR::fload,    // Float
   TR::dload,    // Double
   TR::aload,    // Address
   TR::vload,    // VectorInt8
   TR::vload,    // VectorInt16
   TR::vload,    // VectorInt32
   TR::vload,    // VectorInt64
   TR::vload,    // VectorFloat
   TR::vload,    // VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForDirectReadBarrier[] =
   {
   TR::BadILOp,  // NoType
   TR::brdbar,   // Int8
   TR::srdbar,   // Int16
   TR::irdbar,   // Int32
   TR::lrdbar,   // Int64
   TR::frdbar,   // Float
   TR::drdbar,   // Double
   TR::ardbar,   // Address
   TR::BadILOp,  // VectorInt8
   TR::BadILOp,  // VectorInt16
   TR::BadILOp,  // VectorInt32
   TR::BadILOp,  // VectorInt64
   TR::BadILOp,  // VectorFloat
   TR::BadILOp,  // VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForDirectStore[] =
   {
   TR::BadILOp,  // NoType
   TR::bstore,   // Int8
   TR::sstore,   // Int16
   TR::istore,   // Int32
   TR::lstore,   // Int64
   TR::fstore,   // Float
   TR::dstore,   // Double
   TR::astore,   // Address
   TR::vstore,   // VectorInt8
   TR::vstore,   // VectorInt16
   TR::vstore,   // VectorInt32
   TR::vstore,   // VectorInt64
   TR::vstore,   // VectorFloat
   TR::vstore,   // VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForDirectWriteBarrier[] =
   {
   TR::BadILOp,  // NoType
   TR::bwrtbar,   // Int8
   TR::swrtbar,   // Int16
   TR::iwrtbar,   // Int32
   TR::lwrtbar,   // Int64
   TR::fwrtbar,   // Float
   TR::dwrtbar,   // Double
   TR::awrtbar,   // Address
   TR::BadILOp,   // VectorInt8
   TR::BadILOp,   // VectorInt16
   TR::BadILOp,   // VectorInt32
   TR::BadILOp,   // VectorInt64
   TR::BadILOp,   // VectorFloat
   TR::BadILOp,   // VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIndirectLoad[] =
   {
   TR::BadILOp,  // NoType
   TR::bloadi,   // Int8
   TR::sloadi,   // Int16
   TR::iloadi,   // Int32
   TR::lloadi,   // Int64
   TR::floadi,   // Float
   TR::dloadi,   // Double
   TR::aloadi,   // Address
   TR::vloadi,   // TR::VectorInt8
   TR::vloadi,   // TR::VectorInt16
   TR::vloadi,   // TR::VectorInt32
   TR::vloadi,   // TR::VectorInt64
   TR::vloadi,   // TR::VectorFloat
   TR::vloadi,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIndirectReadBarrier[] =
   {
   TR::BadILOp,  // NoType
   TR::brdbari,  // Int8
   TR::srdbari,  // Int16
   TR::irdbari,  // Int32
   TR::lrdbari,  // Int64
   TR::frdbari,  // Float
   TR::drdbari,  // Double
   TR::ardbari,  // Address
   TR::BadILOp,  // TR::VectorInt8
   TR::BadILOp,  // TR::VectorInt16
   TR::BadILOp,  // TR::VectorInt32
   TR::BadILOp,  // TR::VectorInt64
   TR::BadILOp,  // TR::VectorFloat
   TR::BadILOp,  // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIndirectStore[] =
   {
   TR::BadILOp,  // NoType
   TR::bstorei,  // Int8
   TR::sstorei,  // Int16
   TR::istorei,  // Int32
   TR::lstorei,  // Int64
   TR::fstorei,  // Float
   TR::dstorei,  // Double
   TR::astorei,  // Address
   TR::vstorei,  // TR::VectorInt8
   TR::vstorei,  // TR::VectorInt16
   TR::vstorei,  // TR::VectorInt32
   TR::vstorei,  // TR::VectorInt64
   TR::vstorei,  // TR::VectorFloat
   TR::vstorei,  // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIndirectWriteBarrier[] =
   {
   TR::BadILOp,  // NoType
   TR::bwrtbari,   // Int8
   TR::swrtbari,   // Int16
   TR::iwrtbari,   // Int32
   TR::lwrtbari,   // Int64
   TR::fwrtbari,   // Float
   TR::dwrtbari,   // Double
   TR::awrtbari,   // Address
   TR::BadILOp,   // VectorInt8
   TR::BadILOp,   // VectorInt16
   TR::BadILOp,   // VectorInt32
   TR::BadILOp,   // VectorInt64
   TR::BadILOp,   // VectorFloat
   TR::BadILOp,   // VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIndirectArrayLoad[] =
   {
   TR::BadILOp,  // NoType
   TR::bloadi,   // Int8
   TR::sloadi,   // Int16
   TR::iloadi,   // Int32
   TR::lloadi,   // Int64
   TR::floadi,   // Float
   TR::dloadi,   // Double
   TR::aloadi,   // Address
   TR::vloadi,   // TR::VectorInt8
   TR::vloadi,   // TR::VectorInt16
   TR::vloadi,   // TR::VectorInt32
   TR::vloadi,   // TR::VectorInt64
   TR::vloadi,   // TR::VectorFloat
   TR::vloadi,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIndirectArrayStore[] =
   {
   TR::BadILOp,  // NoType
   TR::bstorei,  // Int8
   TR::sstorei,  // Int16
   TR::istorei,  // Int32
   TR::lstorei,  // Int64
   TR::fstorei,  // Float
   TR::dstorei,  // Double
   TR::astorei,  // Address
   TR::vstorei,  // TR::VectorInt8
   TR::vstorei,  // TR::VectorInt16
   TR::vstorei,  // TR::VectorInt32
   TR::vstorei,  // TR::VectorInt64
   TR::vstorei,  // TR::VectorFloat
   TR::vstorei,  // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForRegisterLoad[] =
   {
   TR::BadILOp,   // NoType
   TR::bRegLoad,  // Int8
   TR::sRegLoad,  // Int16
   TR::iRegLoad,  // Int32
   TR::lRegLoad,  // Int64
   TR::fRegLoad,  // Float
   TR::dRegLoad,  // Double
   TR::aRegLoad,  // Address
   TR::vsRegLoad, // TR::VectorInt8
   TR::vbRegLoad, // TR::VectorInt16
   TR::viRegLoad, // TR::VectorInt32
   TR::vlRegLoad, // TR::VectorInt64
   TR::vfRegLoad, // TR::VectorFloat
   TR::vdRegLoad, // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForRegisterStore[] =
   {
   TR::BadILOp,    // NoType
   TR::bRegStore,  // Int8
   TR::sRegStore,  // Int16
   TR::iRegStore,  // Int32
   TR::lRegStore,  // Int64
   TR::fRegStore,  // Float
   TR::dRegStore,  // Double
   TR::aRegStore,  // Address
   TR::vbRegStore, // TR::VectorInt8
   TR::vsRegStore, // TR::VectorInt16
   TR::viRegStore, // TR::VectorInt32
   TR::vlRegStore, // TR::VectorInt64
   TR::vfRegStore, // TR::VectorFloat
   TR::vdRegStore, // TR::VectorDouble
   TR::BadILOp,    // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForCompareEquals[] =
   {
   TR::BadILOp,  // NoType
   TR::bcmpeq,   // Int8
   TR::scmpeq,   // Int16
   TR::icmpeq,   // Int32
   TR::lcmpeq,   // Int64
   TR::fcmpeq,   // Float
   TR::dcmpeq,   // Double
   TR::acmpeq,   // Address
   TR::vcmpeq,   // TR::VectorInt8
   TR::vcmpeq,   // TR::VectorInt16
   TR::vcmpeq,   // TR::VectorInt32
   TR::vcmpeq,   // TR::VectorInt64
   TR::vcmpeq,   // TR::VectorFloat
   TR::vcmpeq,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIfCompareEquals[] =
   {
   TR::BadILOp,   // NoType
   TR::ifbcmpeq,  // Int8
   TR::ifscmpeq,  // Int16
   TR::ificmpeq,  // Int32
   TR::iflcmpeq,  // Int64
   TR::iffcmpeq,  // Float
   TR::ifdcmpeq,  // Double
   TR::ifacmpeq,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForCompareNotEquals[] =
   {
   TR::BadILOp,  // NoType
   TR::bcmpne,   // Int8
   TR::scmpne,   // Int16
   TR::icmpne,   // Int32
   TR::lcmpne,   // Int64
   TR::fcmpne,   // Float
   TR::dcmpne,   // Double
   TR::acmpne,   // Address
   TR::vcmpne,   // TR::VectorInt8
   TR::vcmpne,   // TR::VectorInt16
   TR::vcmpne,   // TR::VectorInt32
   TR::vcmpne,   // TR::VectorInt64
   TR::vcmpne,   // TR::VectorFloat
   TR::vcmpne,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIfCompareNotEquals[] =
   {
   TR::BadILOp,   // NoType
   TR::ifbcmpne,  // Int8
   TR::ifscmpne,  // Int16
   TR::ificmpne,  // Int32
   TR::iflcmpne,  // Int64
   TR::iffcmpne,  // Float
   TR::ifdcmpne,  // Double
   TR::ifacmpne,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForCompareLessThan[] =
   {
   TR::BadILOp,  // NoType
   TR::bcmplt,   // Int8
   TR::scmplt,   // Int16
   TR::icmplt,   // Int32
   TR::lcmplt,   // Int64
   TR::fcmplt,   // Float
   TR::dcmplt,   // Double
   TR::acmplt,   // Address
   TR::vcmplt,   // TR::VectorInt8
   TR::vcmplt,   // TR::VectorInt16
   TR::vcmplt,   // TR::VectorInt32
   TR::vcmplt,   // TR::VectorInt64
   TR::vcmplt,   // TR::VectorFloat
   TR::vcmplt,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForCompareLessOrEquals[] =
   {
   TR::BadILOp,  // NoType
   TR::bcmple,   // Int8
   TR::scmple,   // Int16
   TR::icmple,   // Int32
   TR::lcmple,   // Int64
   TR::fcmple,   // Float
   TR::dcmple,   // Double
   TR::acmple,   // Address
   TR::vcmple,   // TR::VectorInt8
   TR::vcmple,   // TR::VectorInt16
   TR::vcmple,   // TR::VectorInt32
   TR::vcmple,   // TR::VectorInt64
   TR::vcmple,   // TR::VectorFloat
   TR::vcmple,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIfCompareLessThan[] =
   {
   TR::BadILOp,   // NoType
   TR::ifbcmplt,  // Int8
   TR::ifscmplt,  // Int16
   TR::ificmplt,  // Int32
   TR::iflcmplt,  // Int64
   TR::iffcmplt,  // Float
   TR::ifdcmplt,  // Double
   TR::ifacmplt,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIfCompareLessOrEquals[] =
   {
   TR::BadILOp,   // NoType
   TR::ifbcmple,  // Int8
   TR::ifscmple,  // Int16
   TR::ificmple,  // Int32
   TR::iflcmple,  // Int64
   TR::iffcmple,  // Float
   TR::ifdcmple,  // Double
   TR::ifacmple,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForCompareGreaterThan[] =
   {
   TR::BadILOp,  // NoType
   TR::bcmpgt,   // Int8
   TR::scmpgt,   // Int16
   TR::icmpgt,   // Int32
   TR::lcmpgt,   // Int64
   TR::fcmpgt,   // Float
   TR::dcmpgt,   // Double
   TR::acmpgt,   // Address
   TR::vcmpgt,   // TR::VectorInt8
   TR::vcmpgt,   // TR::VectorInt16
   TR::vcmpgt,   // TR::VectorInt32
   TR::vcmpgt,   // TR::VectorInt64
   TR::vcmpgt,   // TR::VectorFloat
   TR::vcmpgt,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForCompareGreaterOrEquals[] =
   {
   TR::BadILOp,  // NoType
   TR::bcmpge,   // Int8
   TR::scmpge,   // Int16
   TR::icmpge,   // Int32
   TR::lcmpge,   // Int64
   TR::fcmpge,   // Float
   TR::dcmpge,   // Double
   TR::acmpge,   // Address
   TR::vcmpge,   // TR::VectorInt8
   TR::vcmpge,   // TR::VectorInt16
   TR::vcmpge,   // TR::VectorInt32
   TR::vcmpge,   // TR::VectorInt64
   TR::vcmpge,   // TR::VectorFloat
   TR::vcmpge,   // TR::VectorDouble
   TR::BadILOp,  // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIfCompareGreaterThan[] =
   {
   TR::BadILOp,   // NoType
   TR::ifbcmpgt,  // Int8
   TR::ifscmpgt,  // Int16
   TR::ificmpgt,  // Int32
   TR::iflcmpgt,  // Int64
   TR::iffcmpgt,  // Float
   TR::ifdcmpgt,  // Double
   TR::ifacmpgt,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForIfCompareGreaterOrEquals[] =
   {
   TR::BadILOp,   // NoType
   TR::ifbcmpge,  // Int8
   TR::ifscmpge,  // Int16
   TR::ificmpge,  // Int32
   TR::iflcmpge,  // Int64
   TR::iffcmpge,  // Float
   TR::ifdcmpge,  // Double
   TR::ifacmpge,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

TR::ILOpCodes OMR::IL::opCodesForSelect [] =
   {
   TR::BadILOp,  // NoType
   TR::bselect,  // Int8
   TR::sselect,  // Int16
   TR::iselect,  // Int32
   TR::lselect,  // Int64
   TR::fselect,  // Float
   TR::dselect,  // Double
   TR::aselect,  // Address
   TR::BadILOp,   // TR::VectorInt8
   TR::BadILOp,   // TR::VectorInt16
   TR::BadILOp,   // TR::VectorInt32
   TR::BadILOp,   // TR::VectorInt64
   TR::BadILOp,   // TR::VectorFloat
   TR::BadILOp,   // TR::VectorDouble
   TR::BadILOp,   // TR::Aggregate
   };

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForConst) / sizeof(OMR::IL::opCodesForConst[0])),
              "OMR::IL::opCodesForConst is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForDirectLoad) / sizeof(OMR::IL::opCodesForDirectLoad[0])),
              "OMR::IL::opCodesForDirectLoad is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForDirectStore) / sizeof(OMR::IL::opCodesForDirectStore[0])),
              "OMR::IL::opCodesForDirectStore is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIndirectLoad) / sizeof(OMR::IL::opCodesForIndirectLoad[0])),
              "OMR::IL::opCodesForIndirectLoad is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIndirectStore) / sizeof(OMR::IL::opCodesForIndirectStore[0])),
              "OMR::IL::opCodesForIndirectStore is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIndirectArrayLoad) / sizeof(OMR::IL::opCodesForIndirectArrayLoad[0])),
              "OMR::IL::opCodesForIndirectArrayLoad is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIndirectArrayStore) / sizeof(OMR::IL::opCodesForIndirectArrayStore[0])),
              "OMR::IL::opCodesForIndirectArrayStore is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForRegisterLoad) / sizeof(OMR::IL::opCodesForRegisterLoad[0])),
              "OMR::IL::opCodesForRegisterLoad is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForRegisterStore) / sizeof(OMR::IL::opCodesForRegisterStore[0])),
              "OMR::IL::opCodesForRegisterStore is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForCompareEquals) / sizeof(OMR::IL::opCodesForCompareEquals[0])),
              "OMR::IL::opCodesForCompareEquals is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIfCompareEquals) / sizeof(OMR::IL::opCodesForIfCompareEquals[0])),
              "OMR::IL::opCodesForIfCompareEquals is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForCompareNotEquals) / sizeof(OMR::IL::opCodesForCompareNotEquals[0])),
              "OMR::IL::opCodesForCompareNotEquals is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIfCompareNotEquals) / sizeof(OMR::IL::opCodesForIfCompareNotEquals[0])),
              "OMR::IL::opCodesForIfCompareNotEquals is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForCompareLessThan) / sizeof(OMR::IL::opCodesForCompareLessThan[0])),
              "OMR::IL::opCodesForCompareLessThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForCompareLessOrEquals) / sizeof(OMR::IL::opCodesForCompareLessOrEquals[0])),
              "OMR::IL::opCodesForCompareLessThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIfCompareLessThan) / sizeof(OMR::IL::opCodesForIfCompareLessThan[0])),
              "OMR::IL::opCodesForIfCompareLessThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIfCompareLessOrEquals) / sizeof(OMR::IL::opCodesForIfCompareLessOrEquals[0])),
              "OMR::IL::opCodesForIfCompareLessThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForCompareGreaterThan) / sizeof(OMR::IL::opCodesForCompareGreaterThan[0])),
              "OMR::IL::opCodesForCompareGreaterThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForCompareGreaterOrEquals) / sizeof(OMR::IL::opCodesForCompareGreaterOrEquals[0])),
              "OMR::IL::opCodesForCompareGreaterThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIfCompareGreaterThan) / sizeof(OMR::IL::opCodesForIfCompareGreaterThan[0])),
              "OMR::IL::opCodesForIfCompareGreaterThan is not the correct size");

static_assert(TR::NumOMRTypes == (sizeof(OMR::IL::opCodesForIfCompareGreaterOrEquals) / sizeof(OMR::IL::opCodesForIfCompareGreaterOrEquals[0])),
              "OMR::IL::opCodesForIfCompareGreaterThan is not the correct size");


TR::ILOpCodes
OMR::IL::opCodeForCorrespondingIndirectLoad(TR::ILOpCodes loadOpCode)
   {
   switch (loadOpCode)
      {
      case TR::bloadi: return TR::bstorei;
      case TR::sloadi: return TR::sstorei;
      case TR::iloadi: return TR::istorei;
      case TR::lloadi: return TR::lstorei;
      case TR::floadi: return TR::fstorei;
      case TR::dloadi: return TR::dstorei;
      case TR::aloadi: return TR::astorei;
      case TR::vloadi: return TR::vstorei;
      case TR::cloadi: return TR::cstorei;
      case TR::buloadi: return TR::bstorei;
      case TR::iuload: return TR::istore;
      case TR::iuloadi: return TR::istorei;
      case TR::luloadi: return TR::lstorei;
      case TR::brdbari:
      case TR::srdbari:
      case TR::irdbari:
      case TR::lrdbari:
      case TR::frdbari:
      case TR::drdbari:
      case TR::ardbari:
         //There is not necessarily a guaranteed symmetry about whether an indirect rdbar should be mapped to
         //an indirect wrtbar or a normal indirect store. The mapping of rdbar/ wrtbar totally depends on the
         //actual use in subprojects and should be undefined in OMR level.
         TR_ASSERT_FATAL(0, "xrdbari can't be used with global opcode mapping API at OMR level\n");
      default: break;
      }

   TR_ASSERT(0, "no corresponding store opcode for specified load opcode");
   return TR::BadILOp;
   }


TR::ILOpCodes
OMR::IL::opCodeForCorrespondingIndirectStore(TR::ILOpCodes storeOpCode)
   {

   switch (storeOpCode)
      {
      case TR::bstorei:  return TR::bloadi;
      case TR::sstorei:  return TR::sloadi;
      case TR::istorei:  return TR::iloadi;
      case TR::lstorei:  return TR::lloadi;
      case TR::fstorei:  return TR::floadi;
      case TR::dstorei:  return TR::dloadi;
      case TR::astorei:  return TR::aloadi;
      case TR::awrtbari: return TR::aloadi;
      case TR::vstorei:  return TR::vloadi;
      case TR::cstorei:  return TR::sloadi;
      case TR::bustorei: return TR::bloadi;
      case TR::iustore:  return TR::iload;
      case TR::iustorei: return TR::iloadi;
      case TR::lustorei: return TR::lloadi;
      case TR::bwrtbari:
      case TR::swrtbari:
      case TR::iwrtbari:
      case TR::lwrtbari:
      case TR::fwrtbari:
      case TR::dwrtbari:
         //There is not necessarily a guaranteed symmetry about whether an indirect wrtbar should be mapped to
         //an indirect rdbar or a normal indirect load. The mapping of rdbar/wrtbar totally depends on the
         //actual use in subprojects and should be undefined in OMR level.
         TR_ASSERT_FATAL(0, "xwrtbari can't be used with global opcode mapping API at OMR level\n");

      default: break;
      }

   TR_ASSERT(0, "no corresponding load opcode for specified store opcode");
   return TR::BadILOp;
   }

TR::ILOpCodes
OMR::IL::opCodeForSelect(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "Unexpected data type");

   return OMR::IL::opCodesForSelect[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForConst(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForConst[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForDirectLoad(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForDirectLoad[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForDirectReadBarrier(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForDirectReadBarrier[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForDirectStore(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForDirectStore[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForDirectWriteBarrier(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForDirectWriteBarrier[dt];
   }


TR::ILOpCodes
OMR::IL::opCodeForIndirectLoad(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIndirectLoad[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIndirectReadBarrier(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIndirectReadBarrier[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIndirectStore(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIndirectStore[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIndirectWriteBarrier(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIndirectWriteBarrier[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIndirectArrayLoad(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIndirectArrayLoad[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIndirectArrayStore(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIndirectArrayStore[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForRegisterLoad(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForRegisterLoad[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForRegisterStore(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForRegisterStore[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForCompareEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForCompareEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIfCompareEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIfCompareEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForCompareNotEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForCompareNotEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIfCompareNotEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIfCompareNotEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForCompareLessThan(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForCompareLessThan[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForCompareLessOrEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForCompareLessOrEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIfCompareLessThan(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIfCompareLessThan[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIfCompareLessOrEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIfCompareLessOrEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForCompareGreaterThan(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForCompareGreaterThan[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForCompareGreaterOrEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForCompareGreaterOrEquals[dt];
   }

TR::ILOpCodes
OMR::IL::opCodeForIfCompareGreaterThan(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIfCompareGreaterThan[dt];
   }
TR::ILOpCodes
OMR::IL::opCodeForIfCompareGreaterOrEquals(TR::DataType dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "unexpected opcode");

   return OMR::IL::opCodesForIfCompareGreaterOrEquals[dt];
   }
