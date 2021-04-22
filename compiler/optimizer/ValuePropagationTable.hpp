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


/*
 * One-to-one mapping between opcodes and their value propagation handlers
 */

#define BadILOpVPHandler constrainChildren
#define aconstVPHandler constrainAConst
#define iconstVPHandler constrainIntConst
#define lconstVPHandler constrainLongConst
#define fconstVPHandler constrainFloatConst
#define dconstVPHandler constrainLongConst
#define bconstVPHandler constrainByteConst
#define sconstVPHandler constrainShortConst
#define iloadVPHandler constrainIntLoad
#define floadVPHandler constrainFload
#define dloadVPHandler constrainDload
#define aloadVPHandler constrainAload
#define bloadVPHandler constrainIntLoad
#define sloadVPHandler constrainShortLoad
#define lloadVPHandler constrainLload
#define irdbarVPHandler constrainIntLoad
#define frdbarVPHandler constrainFload
#define drdbarVPHandler constrainDload
#define ardbarVPHandler constrainAload
#define brdbarVPHandler constrainIntLoad
#define srdbarVPHandler constrainShortLoad
#define lrdbarVPHandler constrainLload
#define iloadiVPHandler constrainIiload
#define floadiVPHandler constrainFload
#define dloadiVPHandler constrainDload
#define aloadiVPHandler constrainIaload
#define bloadiVPHandler constrainIntLoad
#define sloadiVPHandler constrainShortLoad
#define lloadiVPHandler constrainLload
#define irdbariVPHandler constrainIiload
#define frdbariVPHandler constrainFload
#define drdbariVPHandler constrainDload
#define ardbariVPHandler constrainIaload
#define brdbariVPHandler constrainIntLoad
#define srdbariVPHandler constrainShortLoad
#define lrdbariVPHandler constrainLload
#define istoreVPHandler constrainIntStore
#define lstoreVPHandler constrainLongStore
#define fstoreVPHandler constrainStore
#define dstoreVPHandler constrainStore
#define astoreVPHandler constrainAstore
#define bstoreVPHandler constrainIntStore
#define sstoreVPHandler constrainIntStore
#define iwrtbarVPHandler constrainIntStore
#define lwrtbarVPHandler constrainLongStore
#define fwrtbarVPHandler constrainStore
#define dwrtbarVPHandler constrainStore
#define awrtbarVPHandler constrainWrtBar
#define bwrtbarVPHandler constrainIntStore
#define swrtbarVPHandler constrainIntStore
#define lstoreiVPHandler constrainStore
#define fstoreiVPHandler constrainStore
#define dstoreiVPHandler constrainStore
#define astoreiVPHandler constrainAstore
#define bstoreiVPHandler constrainStore
#define sstoreiVPHandler constrainStore
#define istoreiVPHandler constrainStore
#define lwrtbariVPHandler constrainStore
#define fwrtbariVPHandler constrainStore
#define dwrtbariVPHandler constrainStore
#define awrtbariVPHandler constrainWrtBar
#define bwrtbariVPHandler constrainStore
#define swrtbariVPHandler constrainStore
#define iwrtbariVPHandler constrainStore
#define GotoVPHandler constrainGoto
#define ireturnVPHandler constrainReturn
#define lreturnVPHandler constrainReturn
#define freturnVPHandler constrainReturn
#define dreturnVPHandler constrainReturn
#define areturnVPHandler constrainReturn
#define ReturnVPHandler constrainReturn
#define asynccheckVPHandler constrainChildren
#define athrowVPHandler constrainThrow
#define icallVPHandler constrainCall
#define lcallVPHandler constrainCall
#define fcallVPHandler constrainCall
#define dcallVPHandler constrainCall
#define acallVPHandler constrainAcall
#define callVPHandler constrainVcall
#define iaddVPHandler constrainAdd
#define laddVPHandler constrainAdd
#define faddVPHandler constrainChildren
#define daddVPHandler constrainChildren
#define baddVPHandler constrainAdd
#define saddVPHandler constrainAdd
#define isubVPHandler constrainSubtract
#define lsubVPHandler constrainSubtract
#define fsubVPHandler constrainChildren
#define dsubVPHandler constrainChildren
#define bsubVPHandler constrainSubtract
#define ssubVPHandler constrainSubtract
#define asubVPHandler constrainSubtract
#define imulVPHandler constrainImul
#define lmulVPHandler constrainLmul
#define fmulVPHandler constrainChildren
#define dmulVPHandler constrainChildren
#define bmulVPHandler constrainChildren
#define smulVPHandler constrainChildren
#define idivVPHandler constrainIdiv
#define ldivVPHandler constrainLdiv
#define fdivVPHandler constrainChildren
#define ddivVPHandler constrainChildren
#define bdivVPHandler constrainChildren
#define sdivVPHandler constrainChildren
#define iudivVPHandler constrainIdiv
#define ludivVPHandler constrainChildren
#define iremVPHandler constrainIrem
#define lremVPHandler constrainLrem
#define fremVPHandler constrainChildren
#define dremVPHandler constrainChildren
#define bremVPHandler constrainChildren
#define sremVPHandler constrainChildren
#define iuremVPHandler constrainIrem
#define inegVPHandler constrainIneg
#define lnegVPHandler constrainLneg
#define fnegVPHandler constrainChildren
#define dnegVPHandler constrainChildren
#define bnegVPHandler constrainChildren
#define snegVPHandler constrainChildren
#define iabsVPHandler constrainIabs
#define labsVPHandler constrainLabs
#define fabsVPHandler constrainChildren
#define dabsVPHandler constrainChildren
#define ishlVPHandler constrainIshl
#define lshlVPHandler constrainLshl
#define bshlVPHandler constrainChildren
#define sshlVPHandler constrainChildren
#define ishrVPHandler constrainIshr
#define lshrVPHandler constrainLshr
#define bshrVPHandler constrainChildren
#define sshrVPHandler constrainChildren
#define iushrVPHandler constrainIushr
#define lushrVPHandler constrainLushr
#define bushrVPHandler constrainChildren
#define sushrVPHandler constrainChildren
#define irolVPHandler constrainChildren
#define lrolVPHandler constrainChildren
#define iandVPHandler constrainIand
#define landVPHandler constrainLand
#define bandVPHandler constrainChildren
#define sandVPHandler constrainChildren
#define iorVPHandler constrainIor
#define lorVPHandler constrainLor
#define borVPHandler constrainChildren
#define sorVPHandler constrainChildren
#define ixorVPHandler constrainIxor
#define lxorVPHandler constrainLxor
#define bxorVPHandler constrainChildren
#define sxorVPHandler constrainChildren
#define i2lVPHandler constrainI2l
#define i2fVPHandler constrainChildren
#define i2dVPHandler constrainChildren
#define i2bVPHandler constrainNarrowToByte
#define i2sVPHandler constrainNarrowToShort
#define i2aVPHandler constrainChildren
#define iu2lVPHandler constrainIu2l
#define iu2fVPHandler constrainChildren
#define iu2dVPHandler constrainChildren
#define iu2aVPHandler constrainChildren
#define l2iVPHandler constrainNarrowToInt
#define l2fVPHandler constrainChildren
#define l2dVPHandler constrainChildren
#define l2bVPHandler constrainNarrowToByte
#define l2sVPHandler constrainNarrowToShort
#define l2aVPHandler constrainChildren
#define lu2fVPHandler constrainChildren
#define lu2dVPHandler constrainChildren
#define lu2aVPHandler constrainChildren
#define f2iVPHandler constrainChildren
#define f2lVPHandler constrainChildren
#define f2dVPHandler constrainChildren
#define f2bVPHandler constrainNarrowToByte
#define f2sVPHandler constrainNarrowToShort
#define d2iVPHandler constrainChildren
#define d2lVPHandler constrainChildren
#define d2fVPHandler constrainChildren
#define d2bVPHandler constrainNarrowToByte
#define d2sVPHandler constrainNarrowToShort
#define b2iVPHandler constrainB2i
#define b2lVPHandler constrainB2l
#define b2fVPHandler constrainChildren
#define b2dVPHandler constrainChildren
#define b2sVPHandler constrainB2s
#define b2aVPHandler constrainChildren
#define bu2iVPHandler constrainBu2i
#define bu2lVPHandler constrainBu2l
#define bu2fVPHandler constrainChildren
#define bu2dVPHandler constrainChildren
#define bu2sVPHandler constrainBu2s
#define bu2aVPHandler constrainChildren
#define s2iVPHandler constrainS2i
#define s2lVPHandler constrainS2l
#define s2fVPHandler constrainChildren
#define s2dVPHandler constrainChildren
#define s2bVPHandler constrainNarrowToByte
#define s2aVPHandler constrainChildren
#define su2iVPHandler constrainSu2i
#define su2lVPHandler constrainSu2l
#define su2fVPHandler constrainChildren
#define su2dVPHandler constrainChildren
#define su2aVPHandler constrainChildren
#define a2iVPHandler constrainChildren
#define a2lVPHandler constrainChildren
#define a2bVPHandler constrainChildren
#define a2sVPHandler constrainChildren
#define icmpeqVPHandler constrainCmpeq
#define icmpneVPHandler constrainCmpne
#define icmpltVPHandler constrainCmplt
#define icmpgeVPHandler constrainCmpge
#define icmpgtVPHandler constrainCmpgt
#define icmpleVPHandler constrainCmple
#define iucmpltVPHandler constrainCmp
#define iucmpgeVPHandler constrainCmp
#define iucmpgtVPHandler constrainCmp
#define iucmpleVPHandler constrainCmp
#define lcmpeqVPHandler constrainCmpeq
#define lcmpneVPHandler constrainCmpne
#define lcmpltVPHandler constrainCmplt
#define lcmpgeVPHandler constrainCmpge
#define lcmpgtVPHandler constrainCmpgt
#define lcmpleVPHandler constrainCmple
#define lucmpltVPHandler constrainCmp
#define lucmpgeVPHandler constrainCmp
#define lucmpgtVPHandler constrainCmp
#define lucmpleVPHandler constrainCmp
#define fcmpeqVPHandler constrainCmp
#define fcmpneVPHandler constrainCmp
#define fcmpltVPHandler constrainCmp
#define fcmpgeVPHandler constrainCmp
#define fcmpgtVPHandler constrainCmp
#define fcmpleVPHandler constrainCmp
#define fcmpequVPHandler constrainCmp
#define fcmpneuVPHandler constrainCmp
#define fcmpltuVPHandler constrainCmp
#define fcmpgeuVPHandler constrainCmp
#define fcmpgtuVPHandler constrainCmp
#define fcmpleuVPHandler constrainCmp
#define dcmpeqVPHandler constrainCmp
#define dcmpneVPHandler constrainCmp
#define dcmpltVPHandler constrainCmp
#define dcmpgeVPHandler constrainCmp
#define dcmpgtVPHandler constrainCmp
#define dcmpleVPHandler constrainCmp
#define dcmpequVPHandler constrainCmp
#define dcmpneuVPHandler constrainCmp
#define dcmpltuVPHandler constrainCmp
#define dcmpgeuVPHandler constrainCmp
#define dcmpgtuVPHandler constrainCmp
#define dcmpleuVPHandler constrainCmp
#define acmpeqVPHandler constrainCmpeq
#define acmpneVPHandler constrainCmpne
#define acmpltVPHandler constrainCmp
#define acmpgeVPHandler constrainCmp
#define acmpgtVPHandler constrainCmp
#define acmpleVPHandler constrainCmp
#define bcmpeqVPHandler constrainCmpeq
#define bcmpneVPHandler constrainCmpne
#define bcmpltVPHandler constrainCmplt
#define bcmpgeVPHandler constrainCmpge
#define bcmpgtVPHandler constrainCmpgt
#define bcmpleVPHandler constrainCmple
#define bucmpltVPHandler constrainCmp
#define bucmpgeVPHandler constrainCmp
#define bucmpgtVPHandler constrainCmp
#define bucmpleVPHandler constrainCmp
#define scmpeqVPHandler constrainCmpeq
#define scmpneVPHandler constrainCmpne
#define scmpltVPHandler constrainCmplt
#define scmpgeVPHandler constrainCmpge
#define scmpgtVPHandler constrainCmpgt
#define scmpleVPHandler constrainCmple
#define sucmpltVPHandler constrainCmplt
#define sucmpgeVPHandler constrainCmpge
#define sucmpgtVPHandler constrainCmpgt
#define sucmpleVPHandler constrainCmple
#define lcmpVPHandler constrainFloatCmp
#define fcmplVPHandler constrainFloatCmp
#define fcmpgVPHandler constrainFloatCmp
#define dcmplVPHandler constrainFloatCmp
#define dcmpgVPHandler constrainFloatCmp
#define ificmpeqVPHandler constrainIfcmpeq
#define ificmpneVPHandler constrainIfcmpne
#define ificmpltVPHandler constrainIfcmplt
#define ificmpgeVPHandler constrainIfcmpge
#define ificmpgtVPHandler constrainIfcmpgt
#define ificmpleVPHandler constrainIfcmple
#define ifiucmpltVPHandler constrainCondBranch
#define ifiucmpgeVPHandler constrainCondBranch
#define ifiucmpgtVPHandler constrainCondBranch
#define ifiucmpleVPHandler constrainCondBranch
#define iflcmpeqVPHandler constrainIfcmpeq
#define iflcmpneVPHandler constrainIfcmpne
#define iflcmpltVPHandler constrainIfcmplt
#define iflcmpgeVPHandler constrainIfcmpge
#define iflcmpgtVPHandler constrainIfcmpgt
#define iflcmpleVPHandler constrainIfcmple
#define iflucmpltVPHandler constrainIfcmplt
#define iflucmpgeVPHandler constrainIfcmpge
#define iflucmpgtVPHandler constrainIfcmpgt
#define iflucmpleVPHandler constrainIfcmple
#define iffcmpeqVPHandler constrainCondBranch
#define iffcmpneVPHandler constrainCondBranch
#define iffcmpltVPHandler constrainCondBranch
#define iffcmpgeVPHandler constrainCondBranch
#define iffcmpgtVPHandler constrainCondBranch
#define iffcmpleVPHandler constrainCondBranch
#define iffcmpequVPHandler constrainCondBranch
#define iffcmpneuVPHandler constrainCondBranch
#define iffcmpltuVPHandler constrainCondBranch
#define iffcmpgeuVPHandler constrainCondBranch
#define iffcmpgtuVPHandler constrainCondBranch
#define iffcmpleuVPHandler constrainCondBranch
#define ifdcmpeqVPHandler constrainCondBranch
#define ifdcmpneVPHandler constrainCondBranch
#define ifdcmpltVPHandler constrainCondBranch
#define ifdcmpgeVPHandler constrainCondBranch
#define ifdcmpgtVPHandler constrainCondBranch
#define ifdcmpleVPHandler constrainCondBranch
#define ifdcmpequVPHandler constrainCondBranch
#define ifdcmpneuVPHandler constrainCondBranch
#define ifdcmpltuVPHandler constrainCondBranch
#define ifdcmpgeuVPHandler constrainCondBranch
#define ifdcmpgtuVPHandler constrainCondBranch
#define ifdcmpleuVPHandler constrainCondBranch
#define ifacmpeqVPHandler constrainIfcmpeq
#define ifacmpneVPHandler constrainIfcmpne
#define ifacmpltVPHandler constrainIfcmplt
#define ifacmpgeVPHandler constrainIfcmpge
#define ifacmpgtVPHandler constrainIfcmpgt
#define ifacmpleVPHandler constrainIfcmple
#define ifbcmpeqVPHandler constrainCondBranch
#define ifbcmpneVPHandler constrainCondBranch
#define ifbcmpltVPHandler constrainCondBranch
#define ifbcmpgeVPHandler constrainCondBranch
#define ifbcmpgtVPHandler constrainCondBranch
#define ifbcmpleVPHandler constrainCondBranch
#define ifbucmpltVPHandler constrainCondBranch
#define ifbucmpgeVPHandler constrainCondBranch
#define ifbucmpgtVPHandler constrainCondBranch
#define ifbucmpleVPHandler constrainCondBranch
#define ifscmpeqVPHandler constrainCondBranch
#define ifscmpneVPHandler constrainCondBranch
#define ifscmpltVPHandler constrainCondBranch
#define ifscmpgeVPHandler constrainCondBranch
#define ifscmpgtVPHandler constrainCondBranch
#define ifscmpleVPHandler constrainCondBranch
#define ifsucmpltVPHandler constrainCondBranch
#define ifsucmpgeVPHandler constrainCondBranch
#define ifsucmpgtVPHandler constrainCondBranch
#define ifsucmpleVPHandler constrainCondBranch
#define loadaddrVPHandler constrainLoadaddr
#define ZEROCHKVPHandler constrainZeroChk
#define callIfVPHandler constrainChildren
#define iRegLoadVPHandler constrainChildren
#define aRegLoadVPHandler constrainChildren
#define lRegLoadVPHandler constrainChildren
#define fRegLoadVPHandler constrainChildren
#define dRegLoadVPHandler constrainChildren
#define sRegLoadVPHandler constrainChildren
#define bRegLoadVPHandler constrainChildren
#define iRegStoreVPHandler constrainChildren
#define aRegStoreVPHandler constrainChildren
#define lRegStoreVPHandler constrainChildren
#define fRegStoreVPHandler constrainChildren
#define dRegStoreVPHandler constrainChildren
#define sRegStoreVPHandler constrainChildren
#define bRegStoreVPHandler constrainChildren
#define GlRegDepsVPHandler constrainChildren
#define iselectVPHandler constrainChildrenFirstToLast
#define lselectVPHandler constrainChildrenFirstToLast
#define bselectVPHandler constrainChildrenFirstToLast
#define sselectVPHandler constrainChildrenFirstToLast
#define aselectVPHandler constrainChildrenFirstToLast
#define fselectVPHandler constrainChildrenFirstToLast
#define dselectVPHandler constrainChildrenFirstToLast
#define treetopVPHandler constrainChildren
#define MethodEnterHookVPHandler constrainChildren
#define MethodExitHookVPHandler constrainChildren
#define PassThroughVPHandler constrainChildren
#define compressedRefsVPHandler constrainChildren
#define BBStartVPHandler constrainChildren
#define BBEndVPHandler constrainChildren
#define viremVPHandler constrainChildren
#define viminVPHandler constrainChildren
#define vimaxVPHandler constrainChildren
#define vigetelemVPHandler constrainChildren
#define visetelemVPHandler constrainChildren
#define vimergelVPHandler constrainChildren
#define vimergehVPHandler constrainChildren
#define vicmpeqVPHandler constrainChildren
#define vicmpgtVPHandler constrainChildren
#define vicmpgeVPHandler constrainChildren
#define vicmpltVPHandler constrainChildren
#define vicmpleVPHandler constrainChildren
#define vicmpalleqVPHandler constrainChildren
#define vicmpallneVPHandler constrainChildren
#define vicmpallgtVPHandler constrainChildren
#define vicmpallgeVPHandler constrainChildren
#define vicmpallltVPHandler constrainChildren
#define vicmpallleVPHandler constrainChildren
#define vicmpanyeqVPHandler constrainChildren
#define vicmpanyneVPHandler constrainChildren
#define vicmpanygtVPHandler constrainChildren
#define vicmpanygeVPHandler constrainChildren
#define vicmpanyltVPHandler constrainChildren
#define vicmpanyleVPHandler constrainChildren
#define vnotVPHandler constrainChildren
#define vbitselectVPHandler constrainChildren
#define vpermVPHandler constrainChildren
#define vsplatsVPHandler constrainChildren
#define vdmergelVPHandler constrainChildren
#define vdmergehVPHandler constrainChildren
#define vdsetelemVPHandler constrainChildren
#define vdgetelemVPHandler constrainChildren
#define vdselVPHandler constrainChildren
#define vdremVPHandler constrainChildren
#define vdmaddVPHandler constrainChildren
#define vdnmsubVPHandler constrainChildren
#define vdmsubVPHandler constrainChildren
#define vdmaxVPHandler constrainChildren
#define vdminVPHandler constrainChildren
#define vdcmpeqVPHandler constrainCmp
#define vdcmpneVPHandler constrainCmp
#define vdcmpgtVPHandler constrainCmp
#define vdcmpgeVPHandler constrainCmp
#define vdcmpltVPHandler constrainCmp
#define vdcmpleVPHandler constrainCmp
#define vdcmpalleqVPHandler constrainCmp
#define vdcmpallneVPHandler constrainCmp
#define vdcmpallgtVPHandler constrainCmp
#define vdcmpallgeVPHandler constrainCmp
#define vdcmpallltVPHandler constrainCmp
#define vdcmpallleVPHandler constrainCmp
#define vdcmpanyeqVPHandler constrainCmp
#define vdcmpanyneVPHandler constrainCmp
#define vdcmpanygtVPHandler constrainCmp
#define vdcmpanygeVPHandler constrainCmp
#define vdcmpanyltVPHandler constrainCmp
#define vdcmpanyleVPHandler constrainCmp
#define vdsqrtVPHandler constrainChildren
#define vdlogVPHandler constrainChildren
#define vincVPHandler constrainChildren
#define vdecVPHandler constrainChildren
#define vnegVPHandler constrainChildren
#define vcomVPHandler constrainChildren
#define vaddVPHandler constrainAdd
#define vsubVPHandler constrainSubtract
#define vmulVPHandler constrainChildren
#define vdivVPHandler constrainChildren
#define vremVPHandler constrainChildren
#define vandVPHandler constrainChildren
#define vorVPHandler constrainChildren
#define vxorVPHandler constrainChildren
#define vshlVPHandler constrainChildren
#define vushrVPHandler constrainChildren
#define vshrVPHandler constrainChildren
#define vcmpeqVPHandler constrainCmp
#define vcmpneVPHandler constrainCmp
#define vcmpltVPHandler constrainCmp
#define vucmpltVPHandler constrainCmp
#define vcmpgtVPHandler constrainCmp
#define vucmpgtVPHandler constrainCmp
#define vcmpleVPHandler constrainCmp
#define vucmpleVPHandler constrainCmp
#define vcmpgeVPHandler constrainCmp
#define vucmpgeVPHandler constrainCmp
#define vloadVPHandler constrainChildren
#define vloadiVPHandler constrainChildren
#define vstoreVPHandler constrainStore
#define vstoreiVPHandler constrainStore
#define vrandVPHandler constrainChildren
#define vreturnVPHandler constrainReturn
#define vcallVPHandler constrainCall
#define vcalliVPHandler constrainCall
#define vselectVPHandler constrainChildrenFirstToLast
#define v2vVPHandler constrainChildren
#define vl2vdVPHandler constrainChildren
#define vconstVPHandler constrainChildren
#define getvelemVPHandler constrainChildren
#define vsetelemVPHandler constrainChildren
#define vbRegLoadVPHandler constrainChildren
#define vsRegLoadVPHandler constrainChildren
#define viRegLoadVPHandler constrainChildren
#define vlRegLoadVPHandler constrainChildren
#define vfRegLoadVPHandler constrainChildren
#define vdRegLoadVPHandler constrainChildren
#define vbRegStoreVPHandler constrainChildren
#define vsRegStoreVPHandler constrainChildren
#define viRegStoreVPHandler constrainChildren
#define vlRegStoreVPHandler constrainChildren
#define vfRegStoreVPHandler constrainChildren
#define vdRegStoreVPHandler constrainChildren
#define iuloadVPHandler constrainIntLoad
#define luloadVPHandler constrainLload
#define buloadVPHandler constrainIntLoad
#define iuloadiVPHandler constrainIiload
#define luloadiVPHandler constrainLload
#define buloadiVPHandler constrainIntLoad
#define iustoreVPHandler constrainIntStore
#define lustoreVPHandler constrainLongStore
#define bustoreVPHandler constrainIntStore
#define iustoreiVPHandler constrainStore
#define lustoreiVPHandler constrainStore
#define bustoreiVPHandler constrainStore
#define iuaddVPHandler constrainAdd
#define luaddVPHandler constrainAdd
#define buaddVPHandler constrainAdd
#define iusubVPHandler constrainSubtract
#define lusubVPHandler constrainSubtract
#define busubVPHandler constrainSubtract
#define iunegVPHandler constrainIneg
#define lunegVPHandler constrainLneg
#define f2iuVPHandler constrainChildren
#define f2luVPHandler constrainChildren
#define f2buVPHandler constrainChildren
#define f2cVPHandler constrainNarrowToChar
#define d2iuVPHandler constrainChildren
#define d2luVPHandler constrainChildren
#define d2buVPHandler constrainChildren
#define d2cVPHandler constrainNarrowToChar
#define iuRegLoadVPHandler constrainChildren
#define luRegLoadVPHandler constrainChildren
#define iuRegStoreVPHandler constrainChildren
#define luRegStoreVPHandler constrainChildren
#define cloadVPHandler constrainIntLoad
#define cloadiVPHandler constrainIntLoad
#define cstoreVPHandler constrainIntStore
#define cstoreiVPHandler constrainStore
#define monentVPHandler constrainMonent
#define monexitVPHandler constrainMonexit
#define monexitfenceVPHandler constrainMonexitfence
#define tstartVPHandler constrainTstart
#define tfinishVPHandler constrainTfinish
#define tabortVPHandler constrainTabort
#define instanceofVPHandler constrainInstanceOf
#define checkcastVPHandler constrainCheckcast
#define checkcastAndNULLCHKVPHandler constrainCheckcastNullChk
#define NewVPHandler constrainNew
#define newvalueVPHandler constrainChildren
#define newarrayVPHandler constrainNewArray
#define anewarrayVPHandler constrainANewArray
#define variableNewVPHandler constrainVariableNew
#define variableNewArrayVPHandler constrainVariableNewArray
#define multianewarrayVPHandler constrainMultiANewArray
#define arraylengthVPHandler constrainArraylength
#define contigarraylengthVPHandler constrainArraylength
#define discontigarraylengthVPHandler constrainArraylength
#define icalliVPHandler constrainCall
#define lcalliVPHandler constrainCall
#define fcalliVPHandler constrainCall
#define dcalliVPHandler constrainCall
#define acalliVPHandler constrainAcall
#define calliVPHandler constrainCall
#define fenceVPHandler constrainChildren
#define luaddhVPHandler constrainChildren
#define caddVPHandler constrainAdd
#define aiaddVPHandler constrainAddressRef
#define aiuaddVPHandler constrainAddressRef
#define aladdVPHandler constrainAddressRef
#define aluaddVPHandler constrainAddressRef
#define lusubhVPHandler constrainChildren
#define csubVPHandler constrainSubtract
#define imulhVPHandler constrainChildren
#define iumulhVPHandler constrainChildren
#define lmulhVPHandler constrainChildren
#define lumulhVPHandler constrainChildren
#define ibits2fVPHandler constrainChildren
#define fbits2iVPHandler constrainChildren
#define lbits2dVPHandler constrainChildren
#define dbits2lVPHandler constrainChildren
#define lookupVPHandler constrainSwitch
#define trtLookupVPHandler constrainChildren
#define CaseVPHandler constrainCase
#define tableVPHandler constrainSwitch
#define exceptionRangeFenceVPHandler constrainChildren
#define dbgFenceVPHandler constrainChildren
#define NULLCHKVPHandler constrainNullChk
#define ResolveCHKVPHandler constrainResolveChk
#define ResolveAndNULLCHKVPHandler constrainResolveNullChk
#define DIVCHKVPHandler constrainDivChk
#define OverflowCHKVPHandler constrainOverflowChk
#define UnsignedOverflowCHKVPHandler constrainUnsignedOverflowChk
#define BNDCHKVPHandler constrainBndChk
#define ArrayCopyBNDCHKVPHandler constrainArrayCopyBndChk
#define BNDCHKwithSpineCHKVPHandler constrainBndChkWithSpineChk
#define SpineCHKVPHandler constrainChildren
#define ArrayStoreCHKVPHandler constrainArrayStoreChk
#define ArrayCHKVPHandler constrainArrayChk
#define RetVPHandler constrainChildren
#define arraycopyVPHandler constrainArraycopy
#define arraysetVPHandler constrainChildren
#define arraytranslateVPHandler constrainChildren
#define arraytranslateAndTestVPHandler constrainTRT
#define long2StringVPHandler constrainChildren
#define bitOpMemVPHandler constrainChildren
#define bitOpMemNDVPHandler constrainChildren
#define arraycmpVPHandler constrainChildren
#define arraycmpWithPadVPHandler constrainChildren
#define allocationFenceVPHandler constrainChildren
#define loadFenceVPHandler constrainChildren
#define storeFenceVPHandler constrainChildren
#define fullFenceVPHandler constrainChildren
#define MergeNewVPHandler constrainChildren
#define computeCCVPHandler constrainChildren
#define butestVPHandler constrainChildren
#define sutestVPHandler constrainChildren
#define bucmpVPHandler constrainChildren
#define bcmpVPHandler constrainChildren
#define sucmpVPHandler constrainChildren
#define scmpVPHandler constrainChildren
#define iucmpVPHandler constrainChildren
#define icmpVPHandler constrainChildren
#define lucmpVPHandler constrainChildren
#define ificmpoVPHandler constrainCondBranch
#define ificmpnoVPHandler constrainCondBranch
#define iflcmpoVPHandler constrainCondBranch
#define iflcmpnoVPHandler constrainCondBranch
#define ificmnoVPHandler constrainCondBranch
#define ificmnnoVPHandler constrainCondBranch
#define iflcmnoVPHandler constrainCondBranch
#define iflcmnnoVPHandler constrainCondBranch
#define iuaddcVPHandler constrainChildren
#define luaddcVPHandler constrainChildren
#define iusubbVPHandler constrainChildren
#define lusubbVPHandler constrainChildren
#define icmpsetVPHandler constrainChildren
#define lcmpsetVPHandler constrainChildren
#define bztestnsetVPHandler constrainChildren
#define ibatomicorVPHandler constrainChildren
#define isatomicorVPHandler constrainChildren
#define iiatomicorVPHandler constrainChildren
#define ilatomicorVPHandler constrainChildren
#define dexpVPHandler constrainChildren
#define branchVPHandler constrainCondBranch
#define igotoVPHandler constrainIgoto
#define bexpVPHandler constrainChildren
#define buexpVPHandler constrainChildren
#define sexpVPHandler constrainChildren
#define cexpVPHandler constrainChildren
#define iexpVPHandler constrainChildren
#define iuexpVPHandler constrainChildren
#define lexpVPHandler constrainChildren
#define luexpVPHandler constrainChildren
#define fexpVPHandler constrainChildren
#define fuexpVPHandler constrainChildren
#define duexpVPHandler constrainChildren
#define ixfrsVPHandler constrainChildren
#define lxfrsVPHandler constrainChildren
#define fxfrsVPHandler constrainChildren
#define dxfrsVPHandler constrainChildren
#define fintVPHandler constrainChildren
#define dintVPHandler constrainChildren
#define fnintVPHandler constrainChildren
#define dnintVPHandler constrainChildren
#define fsqrtVPHandler constrainChildren
#define dsqrtVPHandler constrainChildren
#define getstackVPHandler constrainChildren
#define deallocaVPHandler constrainChildren
#define idozVPHandler constrainChildren
#define dcosVPHandler constrainChildren
#define dsinVPHandler constrainChildren
#define dtanVPHandler constrainChildren
#define dcoshVPHandler constrainChildren
#define dsinhVPHandler constrainChildren
#define dtanhVPHandler constrainChildren
#define dacosVPHandler constrainChildren
#define dasinVPHandler constrainChildren
#define datanVPHandler constrainChildren
#define datan2VPHandler constrainChildren
#define dlogVPHandler constrainChildren
#define dfloorVPHandler constrainChildren
#define ffloorVPHandler constrainChildren
#define dceilVPHandler constrainChildren
#define fceilVPHandler constrainChildren
#define ibranchVPHandler constrainIgoto
#define mbranchVPHandler constrainIgoto
#define getpmVPHandler constrainChildren
#define setpmVPHandler constrainChildren
#define loadAutoOffsetVPHandler constrainChildren
#define imaxVPHandler constrainChildren
#define iumaxVPHandler constrainChildren
#define lmaxVPHandler constrainChildren
#define lumaxVPHandler constrainChildren
#define fmaxVPHandler constrainChildren
#define dmaxVPHandler constrainChildren
#define iminVPHandler constrainChildren
#define iuminVPHandler constrainChildren
#define lminVPHandler constrainChildren
#define luminVPHandler constrainChildren
#define fminVPHandler constrainChildren
#define dminVPHandler constrainChildren
#define trtVPHandler constrainChildren
#define trtSimpleVPHandler constrainChildren
#define ihbitVPHandler constrainIntegerHighestOneBit
#define ilbitVPHandler constrainIntegerLowestOneBit
#define inolzVPHandler constrainIntegerNumberOfLeadingZeros
#define inotzVPHandler constrainIntegerNumberOfTrailingZeros
#define ipopcntVPHandler constrainIntegerBitCount
#define lhbitVPHandler constrainLongHighestOneBit
#define llbitVPHandler constrainLongLowestOneBit
#define lnolzVPHandler constrainLongNumberOfLeadingZeros
#define lnotzVPHandler constrainLongNumberOfTrailingZeros
#define lpopcntVPHandler constrainLongBitCount
#define sbyteswapVPHandler constrainChildren
#define ibyteswapVPHandler constrainChildren
#define lbyteswapVPHandler constrainChildren
#define bbitpermuteVPHandler constrainChildren
#define sbitpermuteVPHandler constrainChildren
#define ibitpermuteVPHandler constrainChildren
#define lbitpermuteVPHandler constrainChildren
#define PrefetchVPHandler constrainChildren

#ifdef J9_PROJECT_SPECIFIC
#define zdloadVPHandler constrainBCDAggrLoad
#define zdloadiVPHandler constrainBCDAggrLoad
#define zdstoreVPHandler constrainStore
#define zdstoreiVPHandler constrainStore
#define pd2zdVPHandler constrainChildren
#define zd2pdVPHandler constrainChildren
#define zdsleLoadVPHandler constrainBCDAggrLoad
#define zdslsLoadVPHandler constrainBCDAggrLoad
#define zdstsLoadVPHandler constrainBCDAggrLoad
#define zdsleLoadiVPHandler constrainBCDAggrLoad
#define zdslsLoadiVPHandler constrainBCDAggrLoad
#define zdstsLoadiVPHandler constrainBCDAggrLoad
#define zdsleStoreVPHandler constrainStore
#define zdslsStoreVPHandler constrainStore
#define zdstsStoreVPHandler constrainStore
#define zdsleStoreiVPHandler constrainStore
#define zdslsStoreiVPHandler constrainStore
#define zdstsStoreiVPHandler constrainStore
#define zd2zdsleVPHandler constrainChildren
#define zd2zdslsVPHandler constrainChildren
#define zd2zdstsVPHandler constrainChildren
#define zdsle2pdVPHandler constrainChildren
#define zdsls2pdVPHandler constrainChildren
#define zdsts2pdVPHandler constrainChildren
#define zdsle2zdVPHandler constrainChildren
#define zdsls2zdVPHandler constrainChildren
#define zdsts2zdVPHandler constrainChildren
#define pd2zdslsVPHandler constrainChildren
#define pd2zdslsSetSignVPHandler constrainChildren
#define pd2zdstsVPHandler constrainChildren
#define pd2zdstsSetSignVPHandler constrainChildren
#define udLoadVPHandler constrainBCDAggrLoad
#define udslLoadVPHandler constrainBCDAggrLoad
#define udstLoadVPHandler constrainBCDAggrLoad
#define udLoadiVPHandler constrainBCDAggrLoad
#define udslLoadiVPHandler constrainBCDAggrLoad
#define udstLoadiVPHandler constrainBCDAggrLoad
#define udStoreVPHandler constrainStore
#define udslStoreVPHandler constrainStore
#define udstStoreVPHandler constrainStore
#define udStoreiVPHandler constrainStore
#define udslStoreiVPHandler constrainStore
#define udstStoreiVPHandler constrainStore
#define pd2udVPHandler constrainChildren
#define pd2udslVPHandler constrainChildren
#define pd2udstVPHandler constrainChildren
#define udsl2udVPHandler constrainChildren
#define udst2udVPHandler constrainChildren
#define ud2pdVPHandler constrainChildren
#define udsl2pdVPHandler constrainChildren
#define udst2pdVPHandler constrainChildren
#define pdloadVPHandler constrainBCDAggrLoad
#define pdloadiVPHandler constrainBCDAggrLoad
#define pdstoreVPHandler constrainStore
#define pdstoreiVPHandler constrainStore
#define pdaddVPHandler constrainChildren
#define pdsubVPHandler constrainChildren
#define pdmulVPHandler constrainChildren
#define pddivVPHandler constrainChildren
#define pdremVPHandler constrainChildren
#define pdnegVPHandler constrainChildren
#define pdabsVPHandler constrainChildren
#define pdshrVPHandler constrainChildren
#define pdshlVPHandler constrainChildren
#define pdshrSetSignVPHandler constrainChildren
#define pdshlSetSignVPHandler constrainChildren
#define pdshlOverflowVPHandler constrainChildren
#define pdchkVPHandler constrainChildren
#define pd2iVPHandler constrainBCDToIntegral
#define pd2iOverflowVPHandler constrainChildren
#define pd2iuVPHandler constrainBCDToIntegral
#define i2pdVPHandler constrainIntegralToBCD
#define iu2pdVPHandler constrainIntegralToBCD
#define pd2lVPHandler constrainBCDToIntegral
#define pd2lOverflowVPHandler constrainChildren
#define pd2luVPHandler constrainBCDToIntegral
#define l2pdVPHandler constrainIntegralToBCD
#define lu2pdVPHandler constrainIntegralToBCD
#define pd2fVPHandler constrainChildren
#define pd2dVPHandler constrainChildren
#define f2pdVPHandler constrainChildren
#define d2pdVPHandler constrainChildren
#define pdcmpeqVPHandler constrainChildren
#define pdcmpneVPHandler constrainChildren
#define pdcmpltVPHandler constrainChildren
#define pdcmpgeVPHandler constrainChildren
#define pdcmpgtVPHandler constrainChildren
#define pdcmpleVPHandler constrainChildren
#define pdcleanVPHandler constrainChildren
#define pdclearVPHandler constrainChildren
#define pdclearSetSignVPHandler constrainChildren
#define pdSetSignVPHandler constrainChildren
#define pdModifyPrecisionVPHandler constrainChildren
#define countDigitsVPHandler constrainChildren
#define BCDCHKVPHandler constrainBCDCHK
#endif

const ValuePropagationPtr constraintHandlers[] =
   {
#define OPCODE_MACRO(\
   opcode, \
   name, \
   prop1, \
   prop2, \
   prop3, \
   prop4, \
   dataType, \
   typeProps, \
   childProps, \
   swapChildrenOpcode, \
   reverseBranchOpcode, \
   boolCompareOpcode, \
   ifCompareOpcode, \
   ...) opcode ## VPHandler,

   BadILOpVPHandler,

#include "il/Opcodes.enum"
#undef OPCODE_MACRO

   };

#endif
