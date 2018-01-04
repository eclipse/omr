/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef VALUEPROPAGATIONTABLE_INCL
#define VALUEPROPAGATIONTABLE_INCL

TR::Node *constrainAcall(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAdd(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAddressRef(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAload(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainANewArray(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainVariableNew(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainVariableNewArray(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAnyIntLoad(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainArrayChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainArraycopy(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainArrayCopyBndChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainArraylength(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainArrayStoreChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAstore(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainB2i(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainB2s(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainB2l(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBndChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBndChkWithSpineChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBu2i(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBu2s(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBu2l(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainByteConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCall(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCase(OMR::ValuePropagation *vp, TR::Node *node);
//TR::Node *constrainCharConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCheckcast(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCheckcastNullChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmp(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmpeq(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmpge(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmpgt(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmple(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmplt(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCmpne(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainCondBranch(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainDivChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainOverflowChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainUnsignedOverflowChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainDload(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainFload(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainFloatCmp(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainGoto(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainI2l(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIaload(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIand(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIdiv(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIfcmpeq(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIfcmpge(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIfcmpgt(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIfcmple(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIfcmplt(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIfcmpne(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIiload(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainImul(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIumul(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIneg(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIabs(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainInstanceOf(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIntAndFloatConstHelper(OMR::ValuePropagation *vp, TR::Node *node, int32_t value);
TR::Node *constrainIntConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainFloatConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIntLoad(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIntStore(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIor(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIrem(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIshl(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIshr(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIu2l(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIushr(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIxor(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLand(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLdiv(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLload(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLmul(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLneg(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLabs(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLoadaddr(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLongConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLongStore(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLor(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLrem(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLshl(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLshr(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLushr(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainLxor(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainMonent(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainMonexit(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainMonexitfence(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainTstart(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainTfinish(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainTabort(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainMultiANewArray(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNarrowToByte(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNarrowToChar(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNarrowToInt(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNarrowToShort(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNew(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNewArray(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainNullChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainZeroChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainAsm(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainIgoto(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainResolveChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainResolveNullChk(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainReturn(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainS2i(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainS2l(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainShortConst(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainShortLoad(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainStore(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainSu2i(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainSu2l(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainSubtract(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainSwitch(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainThrow(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainTRT(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainWrtBar(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBCDCHK(OMR::ValuePropagation *vp, TR::Node *node);

TR::Node *constrainBCDAggrLoad(OMR::ValuePropagation *vp, TR::Node *node);

TR::Node *constrainIntegralToBCD(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node *constrainBCDToIntegral(OMR::ValuePropagation *vp, TR::Node *node);


TR::Node * constrainIntegerHighestOneBit(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainIntegerLowestOneBit(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainIntegerNumberOfLeadingZeros(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainIntegerNumberOfTrailingZeros(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainIntegerBitCount(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainLongHighestOneBit(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainLongLowestOneBit(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainLongNumberOfLeadingZeros(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainLongNumberOfTrailingZeros(OMR::ValuePropagation *vp, TR::Node *node);
TR::Node * constrainLongBitCount(OMR::ValuePropagation *vp, TR::Node *node);

const ValuePropagationPtr constraintHandlers[] =
   {
   constrainChildren,        // TR_BadILOpp
   constrainAConst,          // TR::aconst
   constrainIntConst,        // TR::iconst
   constrainLongConst,       // TR::lconst
   constrainFloatConst,        // TR::fconst
   constrainLongConst,       // TR::dconst
   constrainByteConst,       // TR::bconst
   constrainShortConst,      // TR::sconst
   constrainIntLoad,         // TR::iload
   constrainFload,           // TR::fload
   constrainDload,           // TR::dload
   constrainAload,           // TR::aload
   constrainIntLoad,         // TR::bload
   constrainShortLoad,       // TR::sload
   constrainLload,           // TR::lload
   constrainIiload,          // TR::iloadi
   constrainFload,           // TR::floadi
   constrainDload,           // TR::dloadi
   constrainIaload,          // TR::aloadi
   constrainIntLoad,         // TR::bloadi
   constrainShortLoad,         // TR::sloadi
   constrainLload,           // TR::lloadi
   constrainIntStore,        // TR::istore
   constrainLongStore,       // TR::lstore
   constrainStore,           // TR::fstore
   constrainStore,           // TR::dstore
   constrainAstore,          // TR::astore
   constrainWrtBar,          // TR::wrtbar
   constrainIntStore,        // TR::bstore
   constrainIntStore,        // TR::sstore
   constrainStore,           // TR::lstorei
   constrainStore,           // TR::fstorei
   constrainStore,           // TR::dstorei
   constrainAstore,          // TR::astorei
   constrainWrtBar,          // TR::wrtbari
   constrainStore,           // TR::bstorei
   constrainStore,           // TR::sstorei
   constrainStore,           // TR::istorei
   constrainGoto,            // TR::Goto
   constrainReturn,          // TR::ireturn
   constrainReturn,          // TR::lreturn
   constrainReturn,          // TR::freturn
   constrainReturn,          // TR::dreturn
   constrainReturn,          // TR::areturn
   constrainReturn,          // TR::Return
   constrainChildren,        // TR::asynccheck
   constrainThrow,           // TR::athrow
   constrainCall,            // TR::icall
   constrainCall,            // TR::lcall
   constrainCall,            // TR::fcall
   constrainCall,            // TR::dcall
   constrainAcall,           // TR::acall
   constrainVcall,           // TR::call
   constrainAdd,             // TR::iadd
   constrainAdd,             // TR::ladd
   constrainChildren,        // TR::fadd
   constrainChildren,        // TR::dadd
   constrainAdd,             // TR::badd
   constrainAdd,             // TR::sadd
   constrainSubtract,        // TR::isub
   constrainSubtract,        // TR::lsub
   constrainChildren,        // TR::fsub
   constrainChildren,        // TR::dsub
   constrainSubtract,        // TR::bsub
   constrainSubtract,        // TR::ssub
   constrainSubtract,        // TR::asub    todo
   constrainImul,            // TR::imul
   constrainLmul,            // TR::lmul
   constrainChildren,        // TR::fmul
   constrainChildren,        // TR::dmul
   constrainChildren,        // TR::bmul
   constrainChildren,        // TR::smul
   constrainIumul,           // TR::iumul
   constrainIdiv,            // TR::idiv
   constrainLdiv,            // TR::ldiv
   constrainChildren,        // TR::fdiv
   constrainChildren,        // TR::ddiv
   constrainChildren,        // TR::bdiv
   constrainChildren,        // TR::sdiv
   constrainIdiv,            // TR::iudiv
   constrainChildren,        // TR::ludiv todo
   constrainIrem,            // TR::irem
   constrainLrem,            // TR::lrem
   constrainChildren,        // TR::frem
   constrainChildren,        // TR::drem
   constrainChildren,        // TR::brem
   constrainChildren,        // TR::srem
   constrainIrem,            // TR::iurem
   constrainIneg,            // TR::ineg
   constrainLneg,            // TR::lneg
   constrainChildren,        // TR::fneg
   constrainChildren,        // TR::dneg
   constrainChildren,        // TR::bneg
   constrainChildren,        // TR::sneg

   constrainIabs,            // TR::iabs
   constrainLabs,            // TR::labs
   constrainChildren,        // TR::fabs    todo
   constrainChildren,        // TR::dabs    todo

   constrainIshl,            // TR::ishl
   constrainLshl,            // TR::lshl
   constrainChildren,        // TR::bshl
   constrainChildren,        // TR::sshl
   constrainIshr,            // TR::ishr
   constrainLshr,            // TR::lshr
   constrainChildren,        // TR::bshr
   constrainChildren,        // TR::sshr
   constrainIushr,           // TR::iushr
   constrainLushr,           // TR::lushr
   constrainChildren,        // TR::bushr
   constrainChildren,        // TR::sushr
   constrainChildren,        // TR::irol
   constrainChildren,        // TR::lrol
   constrainIand,            // TR::iand
   constrainLand,            // TR::land
   constrainChildren,        // TR::band
   constrainChildren,        // TR::sand
   constrainIor,             // TR::ior
   constrainLor,             // TR::lor
   constrainChildren,        // TR::bor
   constrainChildren,        // TR::sor
   constrainIxor,            // TR::ixor
   constrainLxor,            // TR::lxor
   constrainChildren,        // TR::bxor
   constrainChildren,        // TR::sxor

   constrainI2l,             // TR::i2l
   constrainChildren,        // TR::i2f
   constrainChildren,        // TR::i2d
   constrainNarrowToByte,    // TR::i2b
   constrainNarrowToShort,   // TR::i2s
   constrainChildren,        // TR::i2a   todo

   constrainIu2l,            // TR::iu2l
   constrainChildren,        // TR::iu2f
   constrainChildren,        // TR::iu2d
   constrainChildren,        // TR::iu2a   todo

   constrainNarrowToInt,     // TR::l2i
   constrainChildren,        // TR::l2f
   constrainChildren,        // TR::l2d
   constrainNarrowToByte,    // TR::l2b
   constrainNarrowToShort,   // TR::l2s
   constrainChildren,        // TR::l2a   todo

   constrainChildren,        // TR::lu2f
   constrainChildren,        // TR::lu2d
   constrainChildren,        // TR::lu2a   todo

   constrainChildren,        // TR::f2i
   constrainChildren,        // TR::f2l
   constrainChildren,        // TR::f2d
   constrainNarrowToByte,    // TR::f2b
   constrainNarrowToShort,   // TR::f2s

   constrainChildren,        // TR::d2i
   constrainChildren,        // TR::d2l
   constrainChildren,        // TR::d2f
   constrainNarrowToByte,    // TR::d2b
   constrainNarrowToShort,   // TR::d2s

   constrainB2i,             // TR::b2i
   constrainB2l,             // TR::b2l
   constrainChildren,        // TR::b2f
   constrainChildren,        // TR::b2d
   constrainB2s,             // TR::b2s
   constrainChildren,        // TR::b2a

   constrainBu2i,            // TR::bu2i
   constrainBu2l,            // TR::bu2l
   constrainChildren,        // TR::bu2f
   constrainChildren,        // TR::bu2d
   constrainBu2s,            // TR::bu2s
   constrainChildren,        // TR::bu2a

   constrainS2i,             // TR::s2i
   constrainS2l,             // TR::s2l
   constrainChildren,        // TR::s2f
   constrainChildren,        // TR::s2d
   constrainNarrowToByte,    // TR::s2b
   constrainChildren,        // TR::s2a     todo

   constrainSu2i,            // TR::su2i
   constrainSu2l,            // TR::su2l
   constrainChildren,        // TR::su2f
   constrainChildren,        // TR::su2d
   constrainChildren,        // TR::su2a     todo

   constrainChildren,        // TR::a2i
   constrainChildren,        // TR::a2l
   constrainChildren,        // TR::a2b
   constrainChildren,        // TR::a2s
   constrainCmpeq,           // TR::icmpeq
   constrainCmpne,           // TR::icmpne
   constrainCmplt,           // TR::icmplt
   constrainCmpge,           // TR::icmpge
   constrainCmpgt,           // TR::icmpgt
   constrainCmple,           // TR::icmple
   constrainCmpeq,           // TR::iucmpeq
   constrainCmpne,           // TR::iucmpne
   constrainCmp,             // TR::iucmplt
   constrainCmp,             // TR::iucmpge
   constrainCmp,             // TR::iucmpgt
   constrainCmp,             // TR::iucmple
   constrainCmpeq,           // TR::lcmpeq
   constrainCmpne,           // TR::lcmpne
   constrainCmplt,           // TR::lcmplt
   constrainCmpge,           // TR::lcmpge
   constrainCmpgt,           // TR::lcmpgt
   constrainCmple,           // TR::lcmple
   constrainCmpeq,           // TR::lucmpeq
   constrainCmpne,           // TR::lucmpne
   constrainCmp,             // TR::lucmplt
   constrainCmp,             // TR::lucmpge
   constrainCmp,             // TR::lucmpgt
   constrainCmp,             // TR::lucmple
   constrainCmp,             // TR::fcmpeq
   constrainCmp,             // TR::fcmpne
   constrainCmp,             // TR::fcmplt
   constrainCmp,             // TR::fcmpge
   constrainCmp,             // TR::fcmpgt
   constrainCmp,             // TR::fcmple
   constrainCmp,             // TR::fcmpequ
   constrainCmp,             // TR::fcmpneu
   constrainCmp,             // TR::fcmpltu
   constrainCmp,             // TR::fcmpgeu
   constrainCmp,             // TR::fcmpgtu
   constrainCmp,             // TR::fcmpleu
   constrainCmp,             // TR::dcmpeq
   constrainCmp,             // TR::dcmpne
   constrainCmp,             // TR::dcmplt
   constrainCmp,             // TR::dcmpge
   constrainCmp,             // TR::dcmpgt
   constrainCmp,             // TR::dcmple
   constrainCmp,             // TR::dcmpequ
   constrainCmp,             // TR::dcmpneu
   constrainCmp,             // TR::dcmpltu
   constrainCmp,             // TR::dcmpgeu
   constrainCmp,             // TR::dcmpgtu
   constrainCmp,             // TR::dcmpleu

   constrainCmpeq,           // TR::acmpeq
   constrainCmpne,           // TR::acmpne
   constrainCmp,             // TR::acmplt
   constrainCmp,             // TR::acmpge
   constrainCmp,             // TR::acmpgt
   constrainCmp,             // TR::acmple

   constrainCmpeq,           // TR::bcmpeq
   constrainCmpne,           // TR::bcmpne
   constrainCmplt,           // TR::bcmplt
   constrainCmpge,           // TR::bcmpge
   constrainCmpgt,           // TR::bcmpgt
   constrainCmple,           // TR::bcmple
   constrainCmpeq,           // TR::bucmpeq
   constrainCmpne,           // TR::bucmpne
   constrainCmp,             // TR::bucmplt
   constrainCmp,             // TR::bucmpge
   constrainCmp,             // TR::bucmpgt
   constrainCmp,             // TR::bucmple
   constrainCmpeq,           // TR::scmpeq
   constrainCmpne,           // TR::scmpne
   constrainCmplt,           // TR::scmplt
   constrainCmpge,           // TR::scmpge
   constrainCmpgt,           // TR::scmpgt
   constrainCmple,           // TR::scmple
   constrainCmpeq,           // TR::sucmpeq
   constrainCmpne,           // TR::sucmpne
   constrainCmplt,           // TR::sucmplt
   constrainCmpge,           // TR::sucmpge
   constrainCmpgt,           // TR::sucmpgt
   constrainCmple,           // TR::sucmple
   constrainFloatCmp,        // TR::lcmp
   constrainFloatCmp,        // TR::fcmpl
   constrainFloatCmp,        // TR::fcmpg
   constrainFloatCmp,        // TR::dcmpl
   constrainFloatCmp,        // TR::dcmpg
   constrainIfcmpeq,         // TR::ificmpeq
   constrainIfcmpne,         // TR::ificmpne
   constrainIfcmplt,         // TR::ificmplt
   constrainIfcmpge,         // TR::ificmpge
   constrainIfcmpgt,         // TR::ificmpgt
   constrainIfcmple,         // TR::ificmple
   constrainIfcmpeq,         // TR::ifiucmpeq
   constrainIfcmpne,         // TR::ifiucmpne
   constrainCondBranch,      // TR::ifiucmplt
   constrainCondBranch,      // TR::ifiucmpge
   constrainCondBranch,      // TR::ifiucmpgt
   constrainCondBranch,      // TR::ifiucmple
   constrainIfcmpeq,         // TR::iflcmpeq
   constrainIfcmpne,         // TR::iflcmpne
   constrainIfcmplt,         // TR::iflcmplt
   constrainIfcmpge,         // TR::iflcmpge
   constrainIfcmpgt,         // TR::iflcmpgt
   constrainIfcmple,         // TR::iflcmple
   constrainIfcmpeq,         // TR::iflucmpeq
   constrainIfcmpne,         // TR::iflucmpne
   constrainIfcmplt,         // TR::iflucmplt
   constrainIfcmpge,         // TR::iflucmpge
   constrainIfcmpgt,         // TR::iflucmpgt
   constrainIfcmple,         // TR::iflucmple
   constrainCondBranch,      // TR::iffcmpeq
   constrainCondBranch,      // TR::iffcmpne
   constrainCondBranch,      // TR::iffcmplt
   constrainCondBranch,      // TR::iffcmpge
   constrainCondBranch,      // TR::iffcmpgt
   constrainCondBranch,      // TR::iffcmple
   constrainCondBranch,      // TR::iffcmpequ
   constrainCondBranch,      // TR::iffcmpneu
   constrainCondBranch,      // TR::iffcmpltu
   constrainCondBranch,      // TR::iffcmpgeu
   constrainCondBranch,      // TR::iffcmpgtu
   constrainCondBranch,      // TR::iffcmpleu
   constrainCondBranch,      // TR::ifdcmpeq
   constrainCondBranch,      // TR::ifdcmpne
   constrainCondBranch,      // TR::ifdcmplt
   constrainCondBranch,      // TR::ifdcmpge
   constrainCondBranch,      // TR::ifdcmpgt
   constrainCondBranch,      // TR::ifdcmple
   constrainCondBranch,      // TR::ifdcmpequ
   constrainCondBranch,      // TR::ifdcmpneu
   constrainCondBranch,      // TR::ifdcmpltu
   constrainCondBranch,      // TR::ifdcmpgeu
   constrainCondBranch,      // TR::ifdcmpgtu
   constrainCondBranch,      // TR::ifdcmpleu

   constrainIfcmpeq,         // TR::ifacmpeq
   constrainIfcmpne,         // TR::ifacmpne
   constrainIfcmplt,         // TR::ifacmplt
   constrainIfcmpge,         // TR::ifacmpge
   constrainIfcmpgt,         // TR::ifacmpgt
   constrainIfcmple,         // TR::ifacmple

   constrainCondBranch,      // TR::ifbcmpeq
   constrainCondBranch,      // TR::ifbcmpne
   constrainCondBranch,      // TR::ifbcmplt
   constrainCondBranch,      // TR::ifbcmpge
   constrainCondBranch,      // TR::ifbcmpgt
   constrainCondBranch,      // TR::ifbcmple
   constrainCondBranch,      // TR::ifbucmpeq
   constrainCondBranch,      // TR::ifbucmpne
   constrainCondBranch,      // TR::ifbucmplt
   constrainCondBranch,      // TR::ifbucmpge
   constrainCondBranch,      // TR::ifbucmpgt
   constrainCondBranch,      // TR::ifbucmple
   constrainCondBranch,      // TR::ifscmpeq
   constrainCondBranch,      // TR::ifscmpne
   constrainCondBranch,      // TR::ifscmplt
   constrainCondBranch,      // TR::ifscmpge
   constrainCondBranch,      // TR::ifscmpgt
   constrainCondBranch,      // TR::ifscmple
   constrainCondBranch,      // TR::ifsucmpeq
   constrainCondBranch,      // TR::ifsucmpne
   constrainCondBranch,      // TR::ifsucmplt
   constrainCondBranch,      // TR::ifsucmpge
   constrainCondBranch,      // TR::ifsucmpgt
   constrainCondBranch,      // TR::ifsucmple
   constrainLoadaddr,        // TR::loadaddr
   constrainZeroChk,         // TR::ZEROCHK
   constrainChildren,        // TR::callIf
   constrainChildren,        // TR::iRegLoad
   constrainChildren,        // TR::aRegLoad
   constrainChildren,        // TR::lRegLoad
   constrainChildren,        // TR::fRegLoad
   constrainChildren,        // TR::dRegLoad
   constrainChildren,        // TR::sRegLoad
   constrainChildren,        // TR::bRegLoad
   constrainChildren,        // TR::iRegStore
   constrainChildren,        // TR::aRegStore
   constrainChildren,        // TR::lRegStore
   constrainChildren,        // TR::fRegStore
   constrainChildren,        // TR::dRegStore
   constrainChildren,        // TR::sRegStore
   constrainChildren,        // TR::bRegStore
   constrainChildren,        // TR::GlRegDeps

   constrainChildrenFirstToLast,        // TR::iternary
   constrainChildrenFirstToLast,        // TR::lternary
   constrainChildrenFirstToLast,        // TR::bternary
   constrainChildrenFirstToLast,        // TR::sternary
   constrainChildrenFirstToLast,        // TR::aternary
   constrainChildrenFirstToLast,        // TR::fternary
   constrainChildrenFirstToLast,        // TR::dternary
   constrainChildren,        // TR::treetop
   constrainChildren,        // TR::MethodEnterHook
   constrainChildren,        // TR::MethodExitHook
   constrainChildren,        // TR::PassThrough
   constrainChildren,        // TR::compressedRefs

   constrainChildren,        // TR::BBStart
   constrainChildren,         // TR::BBEnd


   constrainChildren,        // TR::virem
   constrainChildren,        // TR::vimin
   constrainChildren,        // TR::vimax
   constrainChildren,        // TR::vigetelem
   constrainChildren,        // TR::visetelem
   constrainChildren,        // TR::vimergel
   constrainChildren,        // TR::vimergeh
   constrainChildren,        // TR::vicmpeq
   constrainChildren,        // TR::vicmpgt
   constrainChildren,        // TR::vicmpge
   constrainChildren,        // TR::vicmplt
   constrainChildren,        // TR::vicmple
   constrainChildren,        // TR::vicmpalleq
   constrainChildren,        // TR::vicmpallne
   constrainChildren,        // TR::vicmpallgt
   constrainChildren,        // TR::vicmpallge
   constrainChildren,        // TR::vicmpalllt
   constrainChildren,        // TR::vicmpallle
   constrainChildren,        // TR::vicmpanyeq
   constrainChildren,        // TR::vicmpanyne
   constrainChildren,        // TR::vicmpanygt
   constrainChildren,        // TR::vicmpanyge
   constrainChildren,        // TR::vicmpanylt
   constrainChildren,        // TR::vicmpanyle

   constrainChildren,        // TR::vnot
   constrainChildren,        // TR::vselect
   constrainChildren,        // TR::vperm

   constrainChildren,        // TR::vsplats
   constrainChildren,        // TR::vdmergel
   constrainChildren,        // TR::vdmergeh
   constrainChildren,        // TR::vdsetelem
   constrainChildren,        // TR::vdgetelem
   constrainChildren,        // TR::vdsel
   constrainChildren,        // TR::vdrem
   constrainChildren,        // TR::vdmadd
   constrainChildren,        // TR::vdnmsub
   constrainChildren,        // TR::vdmsub
   constrainChildren,        // TR::vdmax
   constrainChildren,        // TR::vdmin
   constrainCmp,             // TR::vdcmpeq
   constrainCmp,             // TR::vdcmpne
   constrainCmp,             // TR::vdcmpgt
   constrainCmp,             // TR::vdcmpge
   constrainCmp,             // TR::vdcmplt
   constrainCmp,             // TR::vdcmple
   constrainCmp,             // TR::vdcmpalleq
   constrainCmp,             // TR::vdcmpallne
   constrainCmp,             // TR::vdcmpallgt
   constrainCmp,             // TR::vdcmpallge
   constrainCmp,             // TR::vdcmpalllt
   constrainCmp,             // TR::vdcmpallle
   constrainCmp,             // TR::vdcmpanyeq
   constrainCmp,             // TR::vdcmpanyne
   constrainCmp,             // TR::vdcmpanygt
   constrainCmp,             // TR::vdcmpanyge
   constrainCmp,             // TR::vdcmpanylt
   constrainCmp,             // TR::vdcmpanyle
   constrainChildren,        // TR::vdsqrt
   constrainChildren,        // TR::vdlog

   constrainChildren,        // TR::vinc
   constrainChildren,        // TR::vdec
   constrainChildren,        // TR::vneg
   constrainChildren,        // TR::vcom
   constrainAdd,             // TR::vadd
   constrainSubtract,        // TR::vsub
   constrainChildren,        // TR::vmul
   constrainChildren,        // TR::vdiv
   constrainChildren,        // TR::vrem
   constrainChildren,        // TR::vand
   constrainChildren,        // TR::vor
   constrainChildren,        // TR::vxor
   constrainChildren,        // TR::vshl
   constrainChildren,        // TR::vushr
   constrainChildren,        // TR::vshr
   constrainCmp,             // TR::vcmpeq
   constrainCmp,             // TR::vcmpne
   constrainCmp,             // TR::vcmplt
   constrainCmp,             // TR::vucmplt
   constrainCmp,             // TR::vcmpgt
   constrainCmp,             // TR::vucmpgt
   constrainCmp,             // TR::vcmple
   constrainCmp,             // TR::vucmple
   constrainCmp,             // TR::vcmpge
   constrainCmp,             // TR::vucmpge
   constrainChildren,        // TR::vload
   constrainChildren,        // TR::vloadi
   constrainStore,           // TR::vstore
   constrainStore,           // TR::vstorei
   constrainChildren,        // TR::vrand
   constrainReturn,          // TR::vreturn
   constrainCall,            // TR::vcall
   constrainCall,            // TR::vcalli
   constrainChildrenFirstToLast,        // TR::vternary
   constrainChildren,        // TR::v2v
   constrainChildren,        // TR::vl2vd
   constrainChildren,        // TR::vconst
   constrainChildren,        // TR::getvelem
   constrainChildren,        // TR::vsetelem

   constrainChildren,        // TR::vbRegLoad
   constrainChildren,        // TR::vsRegLoad
   constrainChildren,        // TR::viRegLoad
   constrainChildren,        // TR::vlRegLoad
   constrainChildren,        // TR::vfRegLoad
   constrainChildren,        // TR::vdRegLoad
   constrainChildren,        // TR::vbRegStore
   constrainChildren,        // TR::vsRegStore
   constrainChildren,        // TR::viRegStore
   constrainChildren,        // TR::vlRegStore
   constrainChildren,        // TR::vfRegStore
   constrainChildren,        // TR::vdRegStore


/*
 *END OF OPCODES REQUIRED BY OMR
 */
   constrainIntConst,        // TR::iuconst
   constrainLongConst,       // TR::luconst
   constrainByteConst,       // TR::buconst
   constrainIntLoad,         // TR::iuload
   constrainLload,           // TR::luload
   constrainIntLoad,         // TR::buload
   constrainIiload,          // TR::iuloadi
   constrainLload,           // TR::luloadi
   constrainIntLoad,         // TR::buloadi
   constrainIntStore,        // TR::iustore
   constrainLongStore,       // TR::lustore
   constrainIntStore,        // TR::bustore
   constrainStore,           // TR::iustorei
   constrainStore,           // TR::lustorei
   constrainStore,           // TR::bustorei
   constrainReturn,          // TR::iureturn
   constrainReturn,          // TR::lureturn
   constrainCall,            // TR::iucall
   constrainCall,            // TR::lucall
   constrainAdd,             // TR::iuadd
   constrainAdd,             // TR::luadd
   constrainAdd,             // TR::buadd
   constrainSubtract,        // TR::iusub
   constrainSubtract,        // TR::lusub
   constrainSubtract,        // TR::busub
   constrainIneg,            // TR::iuneg
   constrainLneg,            // TR::luneg
   constrainIshl,            // TR::iushl
   constrainLshl,            // TR::lushl
   constrainChildren,        // TR::f2iu
   constrainChildren,        // TR::f2lu
   constrainChildren,        // TR::f2bu   todo
   constrainNarrowToChar,    // TR::f2c
   constrainChildren,        // TR::d2iu
   constrainChildren,        // TR::d2lu
   constrainChildren,        // TR::d2bu
   constrainNarrowToChar,    // TR::d2c
   constrainChildren,        // TR::iuRegLoad
   constrainChildren,        // TR::luRegLoad
   constrainChildren,        // TR::iuRegStore
   constrainChildren,        // TR::luRegStore
   constrainChildrenFirstToLast,        // TR::iuternary
   constrainChildrenFirstToLast,        // TR::luternary
   constrainChildrenFirstToLast,        // TR::buternary
   constrainChildrenFirstToLast,        // TR::suternary
   constrainShortConst,      // TR::cconst
   constrainIntLoad,         // TR::cload
   constrainIntLoad,         // TR::cloadi
   constrainIntStore,        // TR::cstore
   constrainStore,           // TR::cstorei
   constrainMonent,          // TR::monent
   constrainMonexit,         // TR::monexit
   constrainMonexitfence,    // TR::monexitfence
   constrainTstart,          // TR::tstart
   constrainTfinish,         // TR::tfinish
   constrainTabort,          // TR::tfinish
   constrainInstanceOf,      // TR::instanceof
   constrainCheckcast,       // TR::checkcast
   constrainCheckcastNullChk,// TR::checkcastAndNULLCHK
   constrainNew,             // TR::New
   constrainNewArray,        // TR::newarray
   constrainANewArray,       // TR::anewarray
   constrainVariableNew,     // TR::variableNew
   constrainVariableNewArray,     // TR::variableNewArray
   constrainMultiANewArray,  // TR::multianewarray
   constrainArraylength,     // TR::arraylength
   constrainArraylength,     // TR::contigarraylength
   constrainArraylength,     // TR::discontigarraylength
   constrainCall,            // TR::icalli
   constrainCall,            // TR::iucalli
   constrainCall,            // TR::lcalli
   constrainCall,            // TR::lucalli
   constrainCall,            // TR::fcalli
   constrainCall,            // TR::dcalli
   constrainAcall,           // TR::acalli
   constrainCall,            // TR::calli
   constrainChildren,        // TR::fence
   constrainChildren,        // TR::luaddh
   constrainAdd,             // TR::cadd
   constrainAddressRef,      // TR::aiadd
   constrainAddressRef,      // TR::aiuadd
   constrainAddressRef,      // TR::aladd
   constrainAddressRef,      // TR::aluadd
   constrainChildren,        // TR::lusubh
   constrainSubtract,        // TR::csub
   constrainChildren,        // TR::imulh
   constrainChildren,        // TR::iumulh
   constrainChildren,        // TR::lmulh
   constrainChildren,        // TR::lumulh
//   constrainChildren,        // TR::cmul
//   constrainChildren,        // TR::cdiv
//   constrainChildren,        // TR::crem

   //   constrainChildren,        // TR::cshl
//   constrainChildren,        // TR::cushr

   constrainChildren,        // TR::ibits2f
   constrainChildren,        // TR::fbits2i
   constrainChildren,        // TR::lbits2d
   constrainChildren,        // TR::dbits2l
   constrainSwitch,          // TR::lookup
   constrainChildren,        // TR::trtLookup
   constrainCase,            // TR::Case
   constrainSwitch,          // TR::table
   constrainChildren,        // TR::exceptionRangeFence
   constrainChildren,        // TR::dbgFence
   constrainNullChk,         // TR::NULLCHK
   constrainResolveChk,      // TR::ResolveCHK
   constrainResolveNullChk,  // TR::ResolveAndNULLCHK
   constrainDivChk,          // TR::DIVCHK
   constrainOverflowChk,     // TR::OverflowCHK
   constrainUnsignedOverflowChk,     // TR::UnsignedOverflowCHK
   constrainBndChk,          // TR::BNDCHK
   constrainArrayCopyBndChk, // TR::ArrayCopyBNDCHK
   constrainBndChkWithSpineChk, // TR::BNDCHKwithSpineCHK
   constrainChildren,        // TR::SpineCHK
   constrainArrayStoreChk,   // TR::ArrayStoreCHK
   constrainArrayChk,        // TR::ArrayCHK
   constrainChildren,        // TR::Ret
   constrainArraycopy,       // TR::arraycopy
   constrainChildren,        // TR::arrayset
   constrainChildren,        // TR::arraytranslate
   constrainTRT,             // TR::arraytranslateAndTest
   constrainChildren,        // TR::long2String
   constrainChildren,        // TR::bitOpMem
   constrainChildren,        // TR::bitOpMemND
   constrainChildren,        // TR::arraycmp
   constrainChildren,        // TR::arraycmpWithPad
   constrainChildren,        // TR::allocationFence
   constrainChildren,        // TR::loadFence
   constrainChildren,        // TR::storeFence
   constrainChildren,        // TR::fullFence
   constrainChildren,        // TR::MergeNew

   constrainChildren,        // TR::computeCC

   constrainChildren,        // TR::butest
   constrainChildren,        // TR::sutest

   constrainChildren,        // TR::bucmp
   constrainChildren,        // TR::bcmp
   constrainChildren,        // TR::sucmp
   constrainChildren,        // TR::scmp
   constrainChildren,        // TR::iucmp
   constrainChildren,        // TR::icmp
   constrainChildren,        // TR::lucmp

   constrainCondBranch,      // TR::ificmpo
   constrainCondBranch,      // TR::ificmpno
   constrainCondBranch,      // TR::iflcmpo
   constrainCondBranch,      // TR::iflcmpno
   constrainCondBranch,      // TR::ificmno
   constrainCondBranch,      // TR::ificmnno
   constrainCondBranch,      // TR::iflcmno
   constrainCondBranch,      // TR::iflcmnno

   constrainChildren,        // TR::iuaddc
   constrainChildren,        // TR::luaddc
   constrainChildren,        // TR::iusubb
   constrainChildren,        // TR::lusubb

   constrainChildren,        // TR::icmpset
   constrainChildren,        // TR::lcmpset
   constrainChildren,        // TR::bztestnset
   constrainChildren,        // TR::ibatomicor
   constrainChildren,        // TR::isatomicor
   constrainChildren,        // TR::iiatomicor
   constrainChildren,        // TR::ilatomicor
   constrainChildren,        // TR::dexp
   constrainCondBranch,      // TR::branch
   constrainIgoto,           // TR::igoto
   constrainChildren,        // TR::bexp
   constrainChildren,        // TR::buexp
   constrainChildren,        // TR::sexp
   constrainChildren,        // TR::cexp
   constrainChildren,        // TR::iexp
   constrainChildren,        // TR::iuexp
   constrainChildren,        // TR::lexp
   constrainChildren,        // TR::luexp
   constrainChildren,        // TR::fexp
   constrainChildren,        // TR::fuexp
   constrainChildren,        // TR::duexp

   constrainChildren,        // TR::ixfrs
   constrainChildren,        // TR::lxfrs
   constrainChildren,        // TR::fxfrs
   constrainChildren,        // TR::dxfrs

   constrainChildren,        // TR::fint
   constrainChildren,        // TR::dint
   constrainChildren,        // TR::fnint
   constrainChildren,        // TR::dnint

   constrainChildren,        // TR::fsqrt
   constrainChildren,        // TR::dsqrt

   constrainChildren,        // TR::getstack
   constrainChildren,        // TR::dealloca


   constrainChildren,        // TR::ishfl
   constrainChildren,        // TR::lshfl
   constrainChildren,        // TR::iushfl
   constrainChildren,        // TR::lushfl
   constrainChildren,        // TR::bshfl
   constrainChildren,        // TR::sshfl
   constrainChildren,        // TR::bushfl
   constrainChildren,        // TR::sushfl

   constrainChildren,        // TR::idoz

   constrainChildren,        // TR::dcos
   constrainChildren,        // TR::dsin
   constrainChildren,        // TR::dtan

   constrainChildren,        // TR::dcosh
   constrainChildren,        // TR::dsinh
   constrainChildren,        // TR::dtanh

   constrainChildren,        // TR::dacos
   constrainChildren,        // TR::dasin
   constrainChildren,        // TR::datan

   constrainChildren,        // TR::datan2

   constrainChildren,        // TR::dlog

   constrainChildren,        // TR::imulover
   constrainChildren,        // TR::dfloor
   constrainChildren,        // TR::ffloor
   constrainChildren,        // TR::dceil
   constrainChildren,        // TR::fceil
   constrainIgoto,           // TR::ibranch
   constrainIgoto,           // TR::mbranch
   constrainChildren,        // TR::getpm
   constrainChildren,        // TR::setpm
   constrainChildren,        // TR::loadAutoOffset
//#endif

   constrainChildren,        // TR::imax
   constrainChildren,        // TR::iumax
   constrainChildren,        // TR::lmax
   constrainChildren,        // TR::lumax
   constrainChildren,        // TR::fmax
   constrainChildren,        // TR::dmax

   constrainChildren,        // TR::imin
   constrainChildren,        // TR::iumin
   constrainChildren,        // TR::lmin
   constrainChildren,        // TR::lumin
   constrainChildren,        // TR::fmin
   constrainChildren,        // TR::dmin

   constrainChildren,        // TR::trt
   constrainChildren,        // TR::trtSimple

	constrainIntegerHighestOneBit,
	constrainIntegerLowestOneBit,
	constrainIntegerNumberOfLeadingZeros,
	constrainIntegerNumberOfTrailingZeros,
	constrainIntegerBitCount,
	constrainLongHighestOneBit,
	constrainLongLowestOneBit,
	constrainLongNumberOfLeadingZeros,
	constrainLongNumberOfTrailingZeros,
	constrainLongBitCount,

   constrainChildren,           // TR::ibyteswap

   constrainChildren,           // TR::bbitpermute
   constrainChildren,           // TR::sbitpermute
   constrainChildren,           // TR::ibitpermute
   constrainChildren,           // TR::lbitpermute

   constrainChildren,        // TR::Prefetch

#ifdef J9_PROJECT_SPECIFIC
   constrainChildren,           // TR::dfconst
   constrainChildren,           // TR::ddconst
   constrainChildren,           // TR::deconst
   constrainChildren,           // TR::dfload
   constrainChildren,           // TR::ddload
   constrainChildren,           // TR::deload
   constrainChildren,           // TR::dfloadi
   constrainChildren,           // TR::ddloadi
   constrainChildren,           // TR::deloadi
   constrainChildren,           // TR::dfstore
   constrainChildren,           // TR::ddstore
   constrainChildren,           // TR::destore
   constrainChildren,           // TR::dfstorei
   constrainChildren,           // TR::ddstorei
   constrainChildren,           // TR::destorei
   constrainReturn,             // TR::dfreturn
   constrainReturn,             // TR::ddreturn
   constrainReturn,             // TR::dereturn
   constrainCall,               // TR::dfcall
   constrainCall,               // TR::ddcall
   constrainCall,               // TR::decall
   constrainCall,               // TR::idfcall
   constrainCall,               // TR::iddcall
   constrainCall,               // TR::idecall
   constrainChildren,           // TR::dfadd
   constrainChildren,           // TR::ddadd
   constrainChildren,           // TR::deadd
   constrainChildren,           // TR::dfsub
   constrainChildren,           // TR::ddsub
   constrainChildren,           // TR::desub
   constrainChildren,           // TR::dfmul
   constrainChildren,           // TR::ddmul
   constrainChildren,           // TR::demul
   constrainChildren,           // TR::dfdiv
   constrainChildren,           // TR::dddiv
   constrainChildren,           // TR::dediv
   constrainChildren,           // TR::dfrem
   constrainChildren,           // TR::ddrem
   constrainChildren,           // TR::derem
   constrainChildren,           // TR::dfneg
   constrainChildren,           // TR::ddneg
   constrainChildren,           // TR::deneg
   constrainChildren,           // TR::dfabs
   constrainChildren,           // TR::ddabs
   constrainChildren,           // TR::deabs
   constrainChildren,           // TR::dfshl
   constrainChildren,           // TR::dfshr
   constrainChildren,           // TR::ddshl
   constrainChildren,           // TR::ddshr
   constrainChildren,           // TR::deshl
   constrainChildren,           // TR::deshr
   constrainChildren,           // TR::dfshrRounded
   constrainChildren,           // TR::ddshrRounded
   constrainChildren,           // TR::deshrRounded
   constrainChildren,           // TR::dfSetNegative
   constrainChildren,           // TR::ddSetNegative
   constrainChildren,           // TR::deSetNegative
   constrainChildren,           // TR::dfModifyPrecision
   constrainChildren,           // TR::ddModifyPrecision
   constrainChildren,           // TR::deModifyPrecision

   constrainChildren,           // TR::i2df
   constrainChildren,           // TR::iu2df
   constrainChildren,           // TR::l2df
   constrainChildren,           // TR::lu2df
   constrainChildren,           // TR::f2df
   constrainChildren,           // TR::d2df
   constrainChildren,           // TR::dd2df
   constrainChildren,           // TR::de2df
   constrainChildren,           // TR::b2df
   constrainChildren,           // TR::bu2df
   constrainChildren,           // TR::s2df
   constrainChildren,           // TR::su2df

   constrainChildren,           // TR::df2i
   constrainChildren,           // TR::df2iu
   constrainChildren,           // TR::df2l
   constrainChildren,           // TR::df2lu
   constrainChildren,           // TR::df2f
   constrainChildren,           // TR::df2d
   constrainChildren,           // TR::df2dd
   constrainChildren,           // TR::df2de
   constrainChildren,           // TR::df2b
   constrainChildren,           // TR::df2bu
   constrainChildren,           // TR::df2s
   constrainChildren,           // TR::df2c

   constrainChildren,           // TR::i2dd
   constrainChildren,           // TR::iu2dd
   constrainChildren,           // TR::l2dd
   constrainChildren,           // TR::lu2dd
   constrainChildren,           // TR::f2dd
   constrainChildren,           // TR::d2dd
   constrainChildren,           // TR::de2dd
   constrainChildren,           // TR::b2dd
   constrainChildren,           // TR::bu2dd
   constrainChildren,           // TR::s2dd
   constrainChildren,           // TR::su2dd

   constrainChildren,           // TR::dd2i
   constrainChildren,           // TR::dd2iu
   constrainChildren,           // TR::dd2l
   constrainChildren,           // TR::dd2lu
   constrainChildren,           // TR::dd2f
   constrainChildren,           // TR::dd2d
   constrainChildren,           // TR::dd2de
   constrainChildren,           // TR::dd2b
   constrainChildren,           // TR::dd2bu
   constrainChildren,           // TR::dd2s
   constrainChildren,           // TR::dd2c

   constrainChildren,           // TR::i2de
   constrainChildren,           // TR::iu2de
   constrainChildren,           // TR::l2de
   constrainChildren,           // TR::lu2de
   constrainChildren,           // TR::f2de
   constrainChildren,           // TR::d2de
   constrainChildren,           // TR::b2de
   constrainChildren,           // TR::bu2de
   constrainChildren,           // TR::s2de
   constrainChildren,           // TR::su2de

   constrainChildren,           // TR::de2i
   constrainChildren,           // TR::de2iu
   constrainChildren,           // TR::de2l
   constrainChildren,           // TR::de2lu
   constrainChildren,           // TR::de2f
   constrainChildren,           // TR::de2d
   constrainChildren,           // TR::de2b
   constrainChildren,           // TR::de2bu
   constrainChildren,           // TR::de2s
   constrainChildren,           // TR::de2c

   constrainCondBranch,         // TR::ifdfcmpeq
   constrainCondBranch,         // TR::ifdfcmpne
   constrainCondBranch,         // TR::ifdfcmplt
   constrainCondBranch,         // TR::ifdfcmpge
   constrainCondBranch,         // TR::ifdfcmpgt
   constrainCondBranch,         // TR::ifdfcmple
   constrainCondBranch,         // TR::ifdfcmpequ
   constrainCondBranch,         // TR::ifdfcmpneu
   constrainCondBranch,         // TR::ifdfcmpltu
   constrainCondBranch,         // TR::ifdfcmpgeu
   constrainCondBranch,         // TR::ifdfcmpgtu
   constrainCondBranch,         // TR::ifdfcmpleu

   constrainChildren,           // TR::dfcmpeq
   constrainChildren,           // TR::dfcmpne
   constrainChildren,           // TR::dfcmplt
   constrainChildren,           // TR::dfcmpge
   constrainChildren,           // TR::dfcmpgt
   constrainChildren,           // TR::dfcmple
   constrainChildren,           // TR::dfcmpequ
   constrainChildren,           // TR::dfcmpneu
   constrainChildren,           // TR::dfcmpltu
   constrainChildren,           // TR::dfcmpgeu
   constrainChildren,           // TR::dfcmpgtu
   constrainChildren,           // TR::dfcmpleu

   constrainCondBranch,         // TR::ifddcmpeq
   constrainCondBranch,         // TR::ifddcmpne
   constrainCondBranch,         // TR::ifddcmplt
   constrainCondBranch,         // TR::ifddcmpge
   constrainCondBranch,         // TR::ifddcmpgt
   constrainCondBranch,         // TR::ifddcmple
   constrainCondBranch,         // TR::ifddcmpequ
   constrainCondBranch,         // TR::ifddcmpneu
   constrainCondBranch,         // TR::ifddcmpltu
   constrainCondBranch,         // TR::ifddcmpgeu
   constrainCondBranch,         // TR::ifddcmpgtu
   constrainCondBranch,         // TR::ifddcmpleu

   constrainChildren,           // TR::ddcmpeq
   constrainChildren,           // TR::ddcmpne
   constrainChildren,           // TR::ddcmplt
   constrainChildren,           // TR::ddcmpge
   constrainChildren,           // TR::ddcmpgt
   constrainChildren,           // TR::ddcmple
   constrainChildren,           // TR::ddcmpequ
   constrainChildren,           // TR::ddcmpneu
   constrainChildren,           // TR::ddcmpltu
   constrainChildren,           // TR::ddcmpgeu
   constrainChildren,           // TR::ddcmpgtu
   constrainChildren,           // TR::ddcmpleu

   constrainCondBranch,         // TR::ifdecmpeq
   constrainCondBranch,         // TR::ifdecmpne
   constrainCondBranch,         // TR::ifdecmplt
   constrainCondBranch,         // TR::ifdecmpge
   constrainCondBranch,         // TR::ifdecmpgt
   constrainCondBranch,         // TR::ifdecmple
   constrainCondBranch,         // TR::ifdecmpequ
   constrainCondBranch,         // TR::ifdecmpneu
   constrainCondBranch,         // TR::ifdecmpltu
   constrainCondBranch,         // TR::ifdecmpgeu
   constrainCondBranch,         // TR::ifdecmpgtu
   constrainCondBranch,         // TR::ifdecmpleu

   constrainChildren,           // TR::decmpeq
   constrainChildren,           // TR::decmpne
   constrainChildren,           // TR::decmplt
   constrainChildren,           // TR::decmpge
   constrainChildren,           // TR::decmpgt
   constrainChildren,           // TR::decmple
   constrainChildren,           // TR::decmpequ
   constrainChildren,           // TR::decmpneu
   constrainChildren,           // TR::decmpltu
   constrainChildren,           // TR::decmpgeu
   constrainChildren,           // TR::decmpgtu
   constrainChildren,           // TR::decmpleu

   constrainChildren,           // TR::dfRegLoad
   constrainChildren,           // TR::ddRegLoad
   constrainChildren,           // TR::deRegLoad
   constrainChildren,           // TR::dfRegStore
   constrainChildren,           // TR::ddRegStore
   constrainChildren,           // TR::deRegStore

   constrainChildrenFirstToLast,        // TR::dfternary
   constrainChildrenFirstToLast,        // TR::ddternary
   constrainChildrenFirstToLast,        // TR::deternary

   constrainChildren,           // TR::dfexp
   constrainChildren,           // TR::ddexp
   constrainChildren,           // TR::deexp
   constrainChildren,           // TR::dfnint
   constrainChildren,           // TR::ddnint
   constrainChildren,           // TR::denint
   constrainChildren,           // TR::dfsqrt
   constrainChildren,           // TR::ddsqrt
   constrainChildren,           // TR::desqrt

   constrainChildren,           // TR::dfcos
   constrainChildren,           // TR::ddcos
   constrainChildren,           // TR::decos
   constrainChildren,           // TR::dfsin
   constrainChildren,           // TR::ddsin
   constrainChildren,           // TR::desin
   constrainChildren,           // TR::dftan
   constrainChildren,           // TR::ddtan
   constrainChildren,           // TR::detan

   constrainChildren,           // TR::dfcosh
   constrainChildren,           // TR::ddcosh
   constrainChildren,           // TR::decosh
   constrainChildren,           // TR::dfsinh
   constrainChildren,           // TR::ddsinh
   constrainChildren,           // TR::desinh
   constrainChildren,           // TR::dftanh
   constrainChildren,           // TR::ddtanh
   constrainChildren,           // TR::detanh

   constrainChildren,           // TR::dfacos
   constrainChildren,           // TR::ddacos
   constrainChildren,           // TR::deacos
   constrainChildren,           // TR::dfasin
   constrainChildren,           // TR::ddasin
   constrainChildren,           // TR::deasin
   constrainChildren,           // TR::dfatan
   constrainChildren,           // TR::ddatan
   constrainChildren,           // TR::deatan

   constrainChildren,           // TR::dfatan2
   constrainChildren,           // TR::ddatan2
   constrainChildren,           // TR::deatan2
   constrainChildren,           // TR::dflog
   constrainChildren,           // TR::ddlog
   constrainChildren,           // TR::delog
   constrainChildren,           // TR::dffloor
   constrainChildren,           // TR::ddfloor
   constrainChildren,           // TR::defloor
   constrainChildren,           // TR::dfceil
   constrainChildren,           // TR::ddceil
   constrainChildren,           // TR::deceil
   constrainChildren,           // TR::dfmax
   constrainChildren,           // TR::ddmax
   constrainChildren,           // TR::demax
   constrainChildren,           // TR::dfmin
   constrainChildren,           // TR::ddmin
   constrainChildren,           // TR::demin

   constrainChildren,           // TR::dfInsExp
   constrainChildren,           // TR::ddInsExp
   constrainChildren,           // TR::deInsExp

   constrainChildren,           // TR::ddclean
   constrainChildren,           // TR::declean

   constrainBCDAggrLoad,        // TR::zdload
   constrainBCDAggrLoad,        // TR::zdloadi
   constrainStore,              // TR::zdstore
   constrainStore,              // TR::zdstorei

   constrainChildren,           // TR::pd2zd
   constrainChildren,           // TR::zd2pd

   constrainBCDAggrLoad,        // TR::zdsleLoad
   constrainBCDAggrLoad,        // TR::zdslsLoad
   constrainBCDAggrLoad,        // TR::zdstsLoad

   constrainBCDAggrLoad,        // TR::zdsleLoadi
   constrainBCDAggrLoad,        // TR::zdslsLoadi
   constrainBCDAggrLoad,        // TR::zdstsLoadi

   constrainStore,              // TR::zdsleStore
   constrainStore,              // TR::zdslsStore
   constrainStore,              // TR::zdstsStore

   constrainStore,              // TR::zdsleStorei
   constrainStore,              // TR::zdslsStorei
   constrainStore,              // TR::zdstsStorei

   constrainChildren,           // TR::zd2zdsle
   constrainChildren,           // TR::zd2zdsls
   constrainChildren,           // TR::zd2zdsts

   constrainChildren,           // TR::zdsle2pd
   constrainChildren,           // TR::zdsls2pd
   constrainChildren,           // TR::zdsts2pd

   constrainChildren,           // TR::zdsle2zd
   constrainChildren,           // TR::zdsls2zd
   constrainChildren,           // TR::zdsts2zd

   constrainChildren,           // TR::pd2zdsls
   constrainChildren,           // TR::pd2zdslsSetSign
   constrainChildren,           // TR::pd2zdsts
   constrainChildren,           // TR::pd2zdstsSetSign

   constrainChildren,           // TR::zd2df
   constrainChildren,           // TR::df2zd
   constrainChildren,           // TR::zd2dd
   constrainChildren,           // TR::dd2zd
   constrainChildren,           // TR::zd2de
   constrainChildren,           // TR::de2zd

   constrainChildren,           // TR::zd2dfAbs
   constrainChildren,           // TR::zd2ddAbs
   constrainChildren,           // TR::zd2deAbs

   constrainChildren,           // TR::df2zdSetSign
   constrainChildren,           // TR::dd2zdSetSign
   constrainChildren,           // TR::de2zdSetSign

   constrainChildren,           // TR::df2zdClean
   constrainChildren,           // TR::dd2zdClean
   constrainChildren,           // TR::de2zdClean

   constrainBCDAggrLoad,        // TR::udLoad
   constrainBCDAggrLoad,        // TR::udslLoad
   constrainBCDAggrLoad,        // TR::udstLoad

   constrainBCDAggrLoad,        // TR::udLoadi
   constrainBCDAggrLoad,        // TR::udslLoadi
   constrainBCDAggrLoad,        // TR::udstLoadi

   constrainStore,              // TR::udStore
   constrainStore,              // TR::udslStore
   constrainStore,              // TR::udstStore

   constrainStore,              // TR::udStorei
   constrainStore,              // TR::udslStorei
   constrainStore,              // TR::udstStorei

   constrainChildren,           // TR::pd2ud
   constrainChildren,           // TR::pd2udsl
   constrainChildren,           // TR::pd2udst

   constrainChildren,           // TR::udsl2ud
   constrainChildren,           // TR::udst2ud

   constrainChildren,           // TR::ud2pd
   constrainChildren,           // TR::udsl2pd
   constrainChildren,           // TR::udst2pd

   constrainBCDAggrLoad,        // TR::pdload
   constrainBCDAggrLoad,        // TR::pdloadi
   constrainStore,              // TR::pdstore
   constrainStore,              // TR::pdstorei
   constrainChildren,           // TR::pdadd
   constrainChildren,           // TR::pdsub
   constrainChildren,           // TR::pdmul
   constrainChildren,           // TR::pddiv
   constrainChildren,           // TR::pdrem
   constrainChildren,           // TR::pdneg
   constrainChildren,           // TR::pdabs
   constrainChildren,           // TR::pdshr
   constrainChildren,           // TR::pdshl

   constrainChildren,           // TR::pdshrSetSign
   constrainChildren,           // TR::pdshlSetSign
   constrainChildren,           // TR::pdshrPreserveSign
   constrainChildren,           // TR::pdshlPreserveSign
   constrainChildren,           // TR::pdshlOverflow
//#if defined(TR_TARGET_S390)
   constrainChildren,           // TR::pdchk
//#endif
   constrainBCDToIntegral,      // TR::pd2i
   constrainChildren,           // TR::pd2iOverflow
   constrainBCDToIntegral,      // TR::pd2iu
   constrainIntegralToBCD,      // TR::i2pd
   constrainIntegralToBCD,      // TR::iu2pd
   constrainBCDToIntegral,      // TR::pd2l
   constrainChildren,           // TR::pd2lOverflow
   constrainBCDToIntegral,      // TR::pd2lu
   constrainIntegralToBCD,      // TR::l2pd
   constrainIntegralToBCD,      // TR::lu2pd
   constrainChildren,           // TR::pd2f
   constrainChildren,           // TR::pd2d
   constrainChildren,           // TR::f2pd
   constrainChildren,           // TR::d2pd

   constrainChildren,           // TR::pdcmpeq
   constrainChildren,           // TR::pdcmpne
   constrainChildren,           // TR::pdcmplt
   constrainChildren,           // TR::pdcmpge
   constrainChildren,           // TR::pdcmpgt
   constrainChildren,           // TR::pdcmple

   constrainChildren,           // TR::pdcheck
   constrainChildren,           // TR::pdfix

   constrainChildren,           // TR::pdclean

   constrainChildren,           // TR::pdexp
   constrainChildren,           // TR::pduexp

   constrainChildren,           // TR::pdclear
   constrainChildren,           // TR::pdclearSetSign

   constrainChildren,           // TR::pdSetSign

   constrainChildren,           // TR::pddivrem
   constrainChildren,           // TR::pddivSelect
   constrainChildren,           // TR::pdremSelect

   constrainChildren,           // TR::pdModifyPrecision

   constrainChildren,           // TR::countDigits

   constrainChildren,           // TR::pd2df
   constrainChildren,           // TR::pd2dfAbs
   constrainChildren,           // TR::df2pd
   constrainChildren,           // TR::df2pdSetSign
   constrainChildren,           // TR::df2pdClean
   constrainChildren,           // TR::pd2dd
   constrainChildren,           // TR::pd2ddAbs
   constrainChildren,           // TR::dd2pd
   constrainChildren,           // TR::dd2pdSetSign
   constrainChildren,           // TR::dd2pdClean
   constrainChildren,           // TR::pd2de
   constrainChildren,           // TR::pd2deAbs
   constrainChildren,           // TR::de2pd
   constrainChildren,           // TR::de2pdSetSign
   constrainChildren,           // TR::de2pdClean
   constrainBCDCHK,          // TR::BCDCHK
   constrainIiload,             // TR::ircload
   constrainIiload,             // TR::irsload
   constrainIiload,             // TR::iruiload
   constrainIiload,             // TR::iriload
   constrainLload,              // TR::irulload
   constrainLload,              // TR::irlload
   constrainStore,              // TR::irsstore
   constrainStore,              // TR::iristore
   constrainStore,              // TR::irlstore
#endif

   };

#endif
