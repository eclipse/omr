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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * IlOpCode to TR_TreeEvaluatorFunctionPointer map.
 * This table is #included in a static table.
 * Only Function Pointers are allowed.
 */
   TR::TreeEvaluator::badILOpEvaluator,     // TR::BadILOp
   TR::TreeEvaluator::aconstEvaluator,      // TR::aconst
   TR::TreeEvaluator::iconstEvaluator,      // TR::iconst
   TR::TreeEvaluator::lconstEvaluator,      // TR::lconst
   TR::TreeEvaluator::fconstEvaluator,      // TR::fconst
   TR::TreeEvaluator::dconstEvaluator,      // TR::dconst
   TR::TreeEvaluator::bconstEvaluator,      // TR::bconst
   TR::TreeEvaluator::sconstEvaluator,      // TR::sconst
   TR::TreeEvaluator::iloadEvaluator,       // TR::iload
   TR::TreeEvaluator::floadEvaluator,       // TR::fload
   TR::TreeEvaluator::dloadEvaluator,       // TR::dload
   TR::TreeEvaluator::aloadEvaluator,       // TR::aload
   TR::TreeEvaluator::bloadEvaluator,       // TR::bload
   TR::TreeEvaluator::sloadEvaluator,       // TR::sload
   TR::TreeEvaluator::lloadEvaluator,       // TR::lload
   TR::TreeEvaluator::iloadEvaluator,       // TR::iloadi
   TR::TreeEvaluator::floadEvaluator,       // TR::floadi
   TR::TreeEvaluator::dloadEvaluator,       // TR::dloadi
   TR::TreeEvaluator::aloadEvaluator,       // TR::aloadi
   TR::TreeEvaluator::bloadEvaluator,       // TR::bloadi
   TR::TreeEvaluator::sloadEvaluator,       // TR::sloadi
   TR::TreeEvaluator::lloadEvaluator,       // TR::lloadi
   TR::TreeEvaluator::istoreEvaluator,      // TR::istore
   TR::TreeEvaluator::lstoreEvaluator,      // TR::lstore
   TR::TreeEvaluator::fstoreEvaluator,      // TR::fstore
   TR::TreeEvaluator::dstoreEvaluator,      // TR::dstore
   TR::TreeEvaluator::astoreEvaluator,      // TR::astore
   TR::TreeEvaluator::badILOpEvaluator,     // TR::wrtbar
   TR::TreeEvaluator::bstoreEvaluator,      // TR::bstore
   TR::TreeEvaluator::sstoreEvaluator,      // TR::sstore
   TR::TreeEvaluator::lstoreEvaluator,      // TR::lstorei
   TR::TreeEvaluator::fstoreEvaluator,      // TR::fstorei
   TR::TreeEvaluator::dstoreEvaluator,      // TR::dstorei
   TR::TreeEvaluator::astoreEvaluator,      // TR::astorei
   TR::TreeEvaluator::badILOpEvaluator,     // TR::wrtbari
   TR::TreeEvaluator::bstoreEvaluator,      // TR::bstorei
   TR::TreeEvaluator::sstoreEvaluator,      // TR::sstorei
   TR::TreeEvaluator::istoreEvaluator,      // TR::istorei
   TR::TreeEvaluator::gotoEvaluator,        // TR::Goto
   TR::TreeEvaluator::returnEvaluator,      // TR::ireturn
   TR::TreeEvaluator::returnEvaluator,      // TR::lreturn
   TR::TreeEvaluator::returnEvaluator,      // TR::freturn
   TR::TreeEvaluator::returnEvaluator,      // TR::dreturn
   TR::TreeEvaluator::returnEvaluator,      // TR::areturn
   TR::TreeEvaluator::returnEvaluator,      // TR::Return
   TR::TreeEvaluator::badILOpEvaluator,     // TR::asynccheck
   TR::TreeEvaluator::badILOpEvaluator,     // TR::athrow
   TR::TreeEvaluator::directCallEvaluator,  // TR::icall
   TR::TreeEvaluator::directCallEvaluator,  // TR::lcall
   TR::TreeEvaluator::directCallEvaluator,  // TR::fcall
   TR::TreeEvaluator::directCallEvaluator,  // TR::dcall
   TR::TreeEvaluator::directCallEvaluator,  // TR::acall
   TR::TreeEvaluator::directCallEvaluator,  // TR::call
   TR::TreeEvaluator::iaddEvaluator,        // TR::iadd
   TR::TreeEvaluator::laddEvaluator,        // TR::ladd
   TR::TreeEvaluator::faddEvaluator,        // TR::fadd
   TR::TreeEvaluator::daddEvaluator,        // TR::dadd
   TR::TreeEvaluator::baddEvaluator,        // TR::badd
   TR::TreeEvaluator::saddEvaluator,        // TR::sadd
   TR::TreeEvaluator::isubEvaluator,        // TR::isub
   TR::TreeEvaluator::lsubEvaluator,        // TR::lsub
   TR::TreeEvaluator::fsubEvaluator,        // TR::fsub
   TR::TreeEvaluator::dsubEvaluator,        // TR::dsub
   TR::TreeEvaluator::bsubEvaluator,        // TR::bsub
   TR::TreeEvaluator::ssubEvaluator,        // TR::ssub
   TR::TreeEvaluator::isubEvaluator,        // TR::asub
   TR::TreeEvaluator::imulEvaluator,        // TR::imul
   TR::TreeEvaluator::lmulEvaluator,        // TR::lmul
   TR::TreeEvaluator::fmulEvaluator,        // TR::fmul
   TR::TreeEvaluator::dmulEvaluator,        // TR::dmul
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::bmul
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::smul
   TR::TreeEvaluator::imulEvaluator,        // TR::iumul
   TR::TreeEvaluator::idivEvaluator,        // TR::idiv
   TR::TreeEvaluator::ldivEvaluator,        // TR::ldiv
   TR::TreeEvaluator::fdivEvaluator,        // TR::fdiv
   TR::TreeEvaluator::ddivEvaluator,        // TR::ddiv
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::bdiv
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::sdiv
   TR::TreeEvaluator::idivEvaluator,        // TR::iudiv
   TR::TreeEvaluator::ldivEvaluator,        // TR::ludiv
   TR::TreeEvaluator::iremEvaluator,        // TR::irem
   TR::TreeEvaluator::lremEvaluator,        // TR::lrem
   TR::TreeEvaluator::fremEvaluator,        // TR::frem
   TR::TreeEvaluator::dremEvaluator,        // TR::drem
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::brem
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::srem
   TR::TreeEvaluator::iremEvaluator,        // TR::iurem
   TR::TreeEvaluator::inegEvaluator,        // TR::ineg
   TR::TreeEvaluator::lnegEvaluator,        // TR::lneg
   TR::TreeEvaluator::fnegEvaluator,        // TR::fneg
   TR::TreeEvaluator::dnegEvaluator,        // TR::dneg
   TR::TreeEvaluator::bnegEvaluator,        // TR::bneg
   TR::TreeEvaluator::snegEvaluator,        // TR::sneg
   TR::TreeEvaluator::iabsEvaluator,        // TR::iabs
   TR::TreeEvaluator::labsEvaluator,        // TR::labs
   TR::TreeEvaluator::iabsEvaluator,        // TR::fabs
   TR::TreeEvaluator::iabsEvaluator,        // TR::dabs

   TR::TreeEvaluator::ishlEvaluator,        // TR::ishl
   TR::TreeEvaluator::lshlEvaluator,        // TR::lshl
   TR::TreeEvaluator::bshlEvaluator,        // TR::bshl
   TR::TreeEvaluator::sshlEvaluator,        // TR::sshl
   TR::TreeEvaluator::ishrEvaluator,        // TR::ishr
   TR::TreeEvaluator::lshrEvaluator,        // TR::lshr
   TR::TreeEvaluator::bshrEvaluator,        // TR::bshr
   TR::TreeEvaluator::sshrEvaluator,        // TR::sshr
   TR::TreeEvaluator::iushrEvaluator,       // TR::iushr
   TR::TreeEvaluator::lushrEvaluator,       // TR::lushr
   TR::TreeEvaluator::bushrEvaluator,       // TR::bushr
   TR::TreeEvaluator::sushrEvaluator,       // TR::sushr
   TR::TreeEvaluator::integerRolEvaluator,  // TR::irol
   TR::TreeEvaluator::integerRolEvaluator,  // TR::lrol
   TR::TreeEvaluator::iandEvaluator,        // TR::iand
   TR::TreeEvaluator::landEvaluator,        // TR::land
   TR::TreeEvaluator::bandEvaluator,        // TR::band
   TR::TreeEvaluator::sandEvaluator,        // TR::sand

   TR::TreeEvaluator::iorEvaluator,         // TR::ior
   TR::TreeEvaluator::lorEvaluator,         // TR::lor
   TR::TreeEvaluator::borEvaluator,         // TR::bor
   TR::TreeEvaluator::sorEvaluator,         // TR::sor

   TR::TreeEvaluator::ixorEvaluator,        // TR::ixor
   TR::TreeEvaluator::lxorEvaluator,        // TR::lxor
   TR::TreeEvaluator::bxorEvaluator,        // TR::bxor
   TR::TreeEvaluator::sxorEvaluator,        // TR::sxor

#define SIGNED true
#define UNSIGN false
   TR::TreeEvaluator::extendCastEvaluator<SIGNED, 32, 64>,   // TR::i2l
   TR::TreeEvaluator::i2fEvaluator,                          // TR::i2f
   TR::TreeEvaluator::i2dEvaluator,                          // TR::i2d
   TR::TreeEvaluator::narrowCastEvaluator<SIGNED,  8>,       // TR::i2b
   TR::TreeEvaluator::narrowCastEvaluator<SIGNED, 16>,       // TR::i2s
   TR::TreeEvaluator::addressCastEvaluator<32, true>,        // TR::i2a

   TR::TreeEvaluator::extendCastEvaluator<UNSIGN, 32, 64>,   // TR::iu2l
   TR::TreeEvaluator::i2fEvaluator,                          // TR::iu2f
   TR::TreeEvaluator::i2dEvaluator,                          // TR::iu2d
   TR::TreeEvaluator::addressCastEvaluator<32, true>,        // TR::iu2a

   TR::TreeEvaluator::narrowCastEvaluator<SIGNED, 32>,       // TR::l2i
   TR::TreeEvaluator::l2fEvaluator,                          // TR::l2f
   TR::TreeEvaluator::l2dEvaluator,                          // TR::l2d
   TR::TreeEvaluator::narrowCastEvaluator<SIGNED,  8>,       // TR::l2b
   TR::TreeEvaluator::narrowCastEvaluator<SIGNED, 16>,       // TR::l2s
   TR::TreeEvaluator::l2aEvaluator,                          // TR::l2a

   TR::TreeEvaluator::l2fEvaluator,                          // TR::lu2f
   TR::TreeEvaluator::l2dEvaluator,                          // TR::lu2d
   TR::TreeEvaluator::addressCastEvaluator<64, true>,        // TR::lu2a

   TR::TreeEvaluator::f2iEvaluator,         // TR::f2i
   TR::TreeEvaluator::f2lEvaluator,         // TR::f2l
   TR::TreeEvaluator::f2dEvaluator,         // TR::f2d
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2b
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2s

   TR::TreeEvaluator::d2iEvaluator,         // TR::d2i
   TR::TreeEvaluator::d2lEvaluator,         // TR::d2l
   TR::TreeEvaluator::d2fEvaluator,         // TR::d2f
   TR::TreeEvaluator::d2iEvaluator,         // TR::d2b
   TR::TreeEvaluator::d2iEvaluator,         // TR::d2s

   TR::TreeEvaluator::extendCastEvaluator<SIGNED,  8, 32>,   // TR::b2i
   TR::TreeEvaluator::extendCastEvaluator<SIGNED,  8, 64>,   // TR::b2l
   TR::TreeEvaluator::i2fEvaluator,                          // TR::b2f
   TR::TreeEvaluator::i2dEvaluator,                          // TR::b2d
   TR::TreeEvaluator::extendCastEvaluator<SIGNED,  8, 32>,   // TR::b2s
   TR::TreeEvaluator::passThroughEvaluator,                  // TR::b2a


   TR::TreeEvaluator::extendCastEvaluator<UNSIGN,  8, 32>,   // TR::bu2i
   TR::TreeEvaluator::extendCastEvaluator<UNSIGN,  8, 64>,   // TR::bu2l
   TR::TreeEvaluator::i2fEvaluator,                          // TR::bu2f
   TR::TreeEvaluator::i2dEvaluator,                          // TR::bu2d
   TR::TreeEvaluator::extendCastEvaluator<UNSIGN,  8, 32>,   // TR::bu2s
   TR::TreeEvaluator::addressCastEvaluator<8, true>,         // TR::bu2a

   TR::TreeEvaluator::extendCastEvaluator<SIGNED, 16, 32>,   // TR::s2i
   TR::TreeEvaluator::extendCastEvaluator<SIGNED, 16, 64>,   // TR::s2l
   TR::TreeEvaluator::i2fEvaluator,                          // TR::s2f
   TR::TreeEvaluator::i2dEvaluator,                          // TR::s2d
   TR::TreeEvaluator::narrowCastEvaluator<SIGNED,  8>,       // TR::s2b
   TR::TreeEvaluator::addressCastEvaluator<16, true>,        // TR::s2a

   TR::TreeEvaluator::extendCastEvaluator<UNSIGN, 16, 32>,   // TR::su2i
   TR::TreeEvaluator::extendCastEvaluator<UNSIGN, 16, 64>,   // TR::su2l
   TR::TreeEvaluator::i2fEvaluator,                          // TR::su2f
   TR::TreeEvaluator::i2dEvaluator,                          // TR::su2d
   TR::TreeEvaluator::addressCastEvaluator<16, true>,        // TR::su2a

   TR::TreeEvaluator::addressCastEvaluator<32, false>,       // TR::a2i
   TR::TreeEvaluator::addressCastEvaluator<64, false>,       // TR::a2l
   TR::TreeEvaluator::addressCastEvaluator< 8, false>,       // TR::a2b
   TR::TreeEvaluator::addressCastEvaluator<16, false>,       // TR::a2s

#undef SIGNED
#undef UNSIGN

   TR::TreeEvaluator::icmpeqEvaluator,      // TR::icmpeq
   TR::TreeEvaluator::icmpeqEvaluator,      // TR::icmpne
   TR::TreeEvaluator::icmpltEvaluator,      // TR::icmplt
   TR::TreeEvaluator::icmpgeEvaluator,      // TR::icmpge
   TR::TreeEvaluator::icmpgtEvaluator,      // TR::icmpgt
   TR::TreeEvaluator::icmpleEvaluator,      // TR::icmple
   TR::TreeEvaluator::icmpeqEvaluator,      // TR::iucmpeq
   TR::TreeEvaluator::icmpeqEvaluator,      // TR::iucmpne
   TR::TreeEvaluator::icmpltEvaluator,      // TR::iucmplt
   TR::TreeEvaluator::icmpgeEvaluator,      // TR::iucmpge
   TR::TreeEvaluator::icmpgtEvaluator,      // TR::iucmpgt
   TR::TreeEvaluator::icmpleEvaluator,      // TR::iucmple

   TR::TreeEvaluator::lcmpeqEvaluator,      // TR::lcmpeq
   TR::TreeEvaluator::lcmpneEvaluator,      // TR::lcmpne
   TR::TreeEvaluator::lcmpltEvaluator,      // TR::lcmplt
   TR::TreeEvaluator::lcmpgeEvaluator,      // TR::lcmpge
   TR::TreeEvaluator::lcmpgtEvaluator,      // TR::lcmpgt
   TR::TreeEvaluator::lcmpleEvaluator,      // TR::lcmple
   TR::TreeEvaluator::lcmpeqEvaluator,      // TR::lucmpeq
   TR::TreeEvaluator::lcmpneEvaluator,      // TR::lucmpne
   TR::TreeEvaluator::lcmpltEvaluator,      // TR::lucmplt
   TR::TreeEvaluator::lcmpgeEvaluator,      // TR::lucmpge
   TR::TreeEvaluator::lcmpgtEvaluator,      // TR::lucmpgt
   TR::TreeEvaluator::lcmpleEvaluator,      // TR::lucmple

   TR::TreeEvaluator::fcmpeqEvaluator,      // TR::fcmpeq
   TR::TreeEvaluator::fcmpneEvaluator,      // TR::fcmpne
   TR::TreeEvaluator::fcmpltEvaluator,      // TR::fcmplt
   TR::TreeEvaluator::fcmpgeEvaluator,      // TR::fcmpge
   TR::TreeEvaluator::fcmpgtEvaluator,      // TR::fcmpgt
   TR::TreeEvaluator::fcmpleEvaluator,      // TR::fcmple
   TR::TreeEvaluator::fcmpequEvaluator,     // TR::fcmpequ
   TR::TreeEvaluator::fcmpneuEvaluator,     // TR::fcmpneu
   TR::TreeEvaluator::fcmpltuEvaluator,     // TR::fcmpltu
   TR::TreeEvaluator::fcmpgeuEvaluator,     // TR::fcmpgeu
   TR::TreeEvaluator::fcmpgtuEvaluator,     // TR::fcmpgtu
   TR::TreeEvaluator::fcmpleuEvaluator,     // TR::fcmpleu
   TR::TreeEvaluator::fcmpeqEvaluator,      // TR::dcmpeq
   TR::TreeEvaluator::fcmpneEvaluator,      // TR::dcmpne
   TR::TreeEvaluator::fcmpltEvaluator,      // TR::dcmplt
   TR::TreeEvaluator::fcmpgeEvaluator,      // TR::dcmpge
   TR::TreeEvaluator::fcmpgtEvaluator,      // TR::dcmpgt
   TR::TreeEvaluator::fcmpleEvaluator,      // TR::dcmple
   TR::TreeEvaluator::fcmpequEvaluator,     // TR::dcmpequ
   TR::TreeEvaluator::fcmpneuEvaluator,     // TR::dcmpneu
   TR::TreeEvaluator::fcmpltuEvaluator,     // TR::dcmpltu
   TR::TreeEvaluator::fcmpgeuEvaluator,     // TR::dcmpgeu
   TR::TreeEvaluator::fcmpgtuEvaluator,     // TR::dcmpgtu
   TR::TreeEvaluator::fcmpleuEvaluator,     // TR::dcmpleu

   TR::TreeEvaluator::acmpeqEvaluator,      // TR::acmpeq
   TR::TreeEvaluator::icmpeqEvaluator,      // TR::acmpne
   TR::TreeEvaluator::icmpltEvaluator,      // TR::acmplt
   TR::TreeEvaluator::icmpgeEvaluator,      // TR::acmpge
   TR::TreeEvaluator::icmpgtEvaluator,      // TR::acmpgt
   TR::TreeEvaluator::icmpleEvaluator,      // TR::acmple

   TR::TreeEvaluator::bcmpeqEvaluator,      // TR::bcmpeq
   TR::TreeEvaluator::bcmpeqEvaluator,      // TR::bcmpne
   TR::TreeEvaluator::bcmpltEvaluator,      // TR::bcmplt
   TR::TreeEvaluator::bcmpgeEvaluator,      // TR::bcmpge
   TR::TreeEvaluator::bcmpgtEvaluator,      // TR::bcmpgt
   TR::TreeEvaluator::bcmpleEvaluator,      // TR::bcmple
   TR::TreeEvaluator::bcmpeqEvaluator,      // TR::bucmpeq
   TR::TreeEvaluator::bcmpeqEvaluator,      // TR::bucmpne
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bucmplt
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bucmpge
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bucmpgt
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bucmple
   TR::TreeEvaluator::scmpeqEvaluator,      // TR::scmpeq
   TR::TreeEvaluator::scmpeqEvaluator,      // TR::scmpne
   TR::TreeEvaluator::scmpltEvaluator,      // TR::scmplt
   TR::TreeEvaluator::scmpgeEvaluator,      // TR::scmpge
   TR::TreeEvaluator::scmpgtEvaluator,      // TR::scmpgt
   TR::TreeEvaluator::scmpleEvaluator,      // TR::scmple
   TR::TreeEvaluator::sucmpeqEvaluator,      // TR::sucmpeq
   TR::TreeEvaluator::sucmpeqEvaluator,      // TR::sucmpne
   TR::TreeEvaluator::sucmpltEvaluator,      // TR::sucmplt
   TR::TreeEvaluator::sucmpgeEvaluator,      // TR::sucmpge
   TR::TreeEvaluator::sucmpgtEvaluator,      // TR::sucmpgt
   TR::TreeEvaluator::sucmpleEvaluator,      // TR::sucmple
   TR::TreeEvaluator::lcmpEvaluator,        // TR::lcmp
   TR::TreeEvaluator::fcmplEvaluator,       // TR::fcmpl
   TR::TreeEvaluator::fcmplEvaluator,       // TR::fcmpg
   TR::TreeEvaluator::fcmplEvaluator,       // TR::dcmpl
   TR::TreeEvaluator::fcmplEvaluator,       // TR::dcmpg
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ificmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ificmpne
   TR::TreeEvaluator::ificmpltEvaluator,    // TR::ificmplt
   TR::TreeEvaluator::ificmpgeEvaluator,    // TR::ificmpge
   TR::TreeEvaluator::ificmpgtEvaluator,    // TR::ificmpgt
   TR::TreeEvaluator::ificmpleEvaluator,    // TR::ificmple
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ifiucmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ifiucmpne : todo
   TR::TreeEvaluator::ificmpltEvaluator,    // TR::ifiucmplt : Temporary place holders till unsigned opcodes are implemented
   TR::TreeEvaluator::ificmpgeEvaluator,    // TR::ifiucmpge
   TR::TreeEvaluator::ificmpgtEvaluator,    // TR::ifiucmpgt
   TR::TreeEvaluator::ificmpleEvaluator,    // TR::ifiucmple

   TR::TreeEvaluator::iflcmpeqEvaluator,    // TR::iflcmpeq
   TR::TreeEvaluator::iflcmpneEvaluator,    // TR::iflcmpne
   TR::TreeEvaluator::iflcmpltEvaluator,    // TR::iflcmplt
   TR::TreeEvaluator::iflcmpgeEvaluator,    // TR::iflcmpge
   TR::TreeEvaluator::iflcmpgtEvaluator,    // TR::iflcmpgt
   TR::TreeEvaluator::iflcmpleEvaluator,    // TR::iflcmple
   TR::TreeEvaluator::iflcmpeqEvaluator,    // TR::iflucmpeq
   TR::TreeEvaluator::iflcmpneEvaluator,    // TR::iflucmpne
   TR::TreeEvaluator::iflcmpltEvaluator,    // TR::iflucmplt
   TR::TreeEvaluator::iflcmpgeEvaluator,    // TR::iflucmpge
   TR::TreeEvaluator::iflcmpgtEvaluator,    // TR::iflucmpgt
   TR::TreeEvaluator::iflcmpleEvaluator,    // TR::iflucmple

   TR::TreeEvaluator::iffcmpeqEvaluator,    // TR::iffcmpeq
   TR::TreeEvaluator::iffcmpneEvaluator,    // TR::iffcmpne
   TR::TreeEvaluator::iffcmpltEvaluator,    // TR::iffcmplt
   TR::TreeEvaluator::iffcmpgeEvaluator,    // TR::iffcmpge
   TR::TreeEvaluator::iffcmpgtEvaluator,    // TR::iffcmpgt
   TR::TreeEvaluator::iffcmpleEvaluator,    // TR::iffcmple
   TR::TreeEvaluator::iffcmpequEvaluator,   // TR::iffcmpequ
   TR::TreeEvaluator::iffcmpneuEvaluator,   // TR::iffcmpneu
   TR::TreeEvaluator::iffcmpltuEvaluator,   // TR::iffcmpltu
   TR::TreeEvaluator::iffcmpgeuEvaluator,   // TR::iffcmpgeu
   TR::TreeEvaluator::iffcmpgtuEvaluator,   // TR::iffcmpgtu
   TR::TreeEvaluator::iffcmpleuEvaluator,   // TR::iffcmpleu
   TR::TreeEvaluator::iffcmpeqEvaluator,    // TR::ifdcmpeq
   TR::TreeEvaluator::iffcmpneEvaluator,    // TR::ifdcmpne
   TR::TreeEvaluator::iffcmpltEvaluator,    // TR::ifdcmplt
   TR::TreeEvaluator::iffcmpgeEvaluator,    // TR::ifdcmpge
   TR::TreeEvaluator::iffcmpgtEvaluator,    // TR::ifdcmpgt
   TR::TreeEvaluator::iffcmpleEvaluator,    // TR::ifdcmple
   TR::TreeEvaluator::iffcmpequEvaluator,   // TR::ifdcmpequ
   TR::TreeEvaluator::iffcmpneuEvaluator,   // TR::ifdcmpneu
   TR::TreeEvaluator::iffcmpltuEvaluator,   // TR::ifdcmpltu
   TR::TreeEvaluator::iffcmpgeuEvaluator,   // TR::ifdcmpgeu
   TR::TreeEvaluator::iffcmpgtuEvaluator,   // TR::ifdcmpgtu
   TR::TreeEvaluator::iffcmpleuEvaluator,   // TR::ifdcmpleu

   TR::TreeEvaluator::ifacmpeqEvaluator,    // TR::ifacmpeq
   TR::TreeEvaluator::ifacmpneEvaluator,    // TR::ifacmpne
   TR::TreeEvaluator::ificmpltEvaluator,    // TR::ifacmplt
   TR::TreeEvaluator::ificmpgeEvaluator,    // TR::ifacmpge
   TR::TreeEvaluator::ificmpgtEvaluator,    // TR::ifacmpgt
   TR::TreeEvaluator::ificmpleEvaluator,    // TR::ifacmple

   TR::TreeEvaluator::ifbcmpeqEvaluator,    // TR::ifbcmpeq
   TR::TreeEvaluator::ifbcmpeqEvaluator,    // TR::ifbcmpne
   TR::TreeEvaluator::ifbcmpltEvaluator,    // TR::ifbcmplt
   TR::TreeEvaluator::ifbcmpgeEvaluator,    // TR::ifbcmpge
   TR::TreeEvaluator::ifbcmpgtEvaluator,    // TR::ifbcmpgt
   TR::TreeEvaluator::ifbcmpleEvaluator,    // TR::ifbcmple
   TR::TreeEvaluator::ifbcmpeqEvaluator,    // TR::ifbucmpeq
   TR::TreeEvaluator::ifbcmpeqEvaluator,    // TR::ifbucmpne
   TR::TreeEvaluator::ifbcmpltEvaluator,    // TR::ifbucmplt
   TR::TreeEvaluator::ifbcmpgeEvaluator,    // TR::ifbucmpge
   TR::TreeEvaluator::ifbcmpgtEvaluator,    // TR::ifbucmpgt
   TR::TreeEvaluator::ifbcmpleEvaluator,    // TR::ifbucmple
   TR::TreeEvaluator::ifscmpeqEvaluator,    // TR::ifscmpeq
   TR::TreeEvaluator::ifscmpeqEvaluator,    // TR::ifscmpne
   TR::TreeEvaluator::ifscmpltEvaluator,    // TR::ifscmplt
   TR::TreeEvaluator::ifscmpgeEvaluator,    // TR::ifscmpge
   TR::TreeEvaluator::ifscmpgtEvaluator,    // TR::ifscmpgt
   TR::TreeEvaluator::ifscmpleEvaluator,    // TR::ifscmple
   TR::TreeEvaluator::ifsucmpeqEvaluator,    // TR::ifsucmpeq
   TR::TreeEvaluator::ifsucmpeqEvaluator,    // TR::ifsucmpne
   TR::TreeEvaluator::ifsucmpltEvaluator,    // TR::ifsucmplt
   TR::TreeEvaluator::ifsucmpgeEvaluator,    // TR::ifsucmpge
   TR::TreeEvaluator::ifsucmpgtEvaluator,    // TR::ifsucmpgt
   TR::TreeEvaluator::ifsucmpleEvaluator,    // TR::ifsucmple
   TR::TreeEvaluator::loadaddrEvaluator,    // TR::loadaddr
   TR::TreeEvaluator::ZEROCHKEvaluator,     // TR::ZEROCHK
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::callIf
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::iRegLoad
   TR::TreeEvaluator::aRegLoadEvaluator,    // TR::aRegLoad
   TR::TreeEvaluator::lRegLoadEvaluator,    // TR::lRegLoad
   TR::TreeEvaluator::fRegLoadEvaluator,    // TR::fRegLoad
   TR::TreeEvaluator::dRegLoadEvaluator,    // TR::dRegLoad
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::sRegLoad
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::bRegLoad
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::iRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::aRegStore
   TR::TreeEvaluator::lRegStoreEvaluator,   // TR::lRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,   // TR::fRegStore
   TR::TreeEvaluator::dRegStoreEvaluator,   // TR::dRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::sRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::bRegStore
   TR::TreeEvaluator::GlRegDepsEvaluator,   // TR::GlRegDeps
   TR::TreeEvaluator::ternaryEvaluator,     // TR::iternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::lternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::bternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::sternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::aternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::fternary
   TR::TreeEvaluator::dternaryEvaluator,     // TR::dternary
   TR::TreeEvaluator::treetopEvaluator,     // TR::treetop
   TR::TreeEvaluator::badILOpEvaluator,     // TR::MethodEnterHook
   TR::TreeEvaluator::badILOpEvaluator,     // TR::MethodExitHook
   TR::TreeEvaluator::passThroughEvaluator, // TR::PassThrough
   TR::TreeEvaluator::compressedRefsEvaluator,  // TR::compressedRefs
   TR::TreeEvaluator::BBStartEvaluator,     // TR::BBStart
   TR::TreeEvaluator::BBEndEvaluator,       // TR::BBEnd


   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::virem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vimin
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vimax
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vigetelem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::visetelem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vimergel
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vimergeh
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpeq
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpgt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpge
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmplt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmple
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpalleq
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpallne
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpallgt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpallge
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpalllt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpallle
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpanyeq
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpanyne
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpanygt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpanyge
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpanylt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vicmpanyle
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vnot
   TR::TreeEvaluator::vselEvaluator,        //  TR::vselect
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vperm
   TR::TreeEvaluator::vsplatsEvaluator,     //  TR::vsplats
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmergel
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmergeh
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdsetelem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdgetelem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdsel
   TR::TreeEvaluator::vdremEvaluator,       //  TR::vdrem
   TR::TreeEvaluator::vdmaddEvaluator,      //  TR::vdmadd
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdnmsub
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmsub
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmax
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmin
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpne
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpge
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmplt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmple
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpalleq
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpallne
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpallgt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpallge
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpalllt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpallle
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpanyeq
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpanyne
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpanygt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpanyge
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpanylt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdcmpanyle
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdsqrt
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdlog

   TR::TreeEvaluator::vincEvaluator,        // TR::vinc
   TR::TreeEvaluator::vdecEvaluator,        // TR::vdec
   TR::TreeEvaluator::vnegEvaluator,        // TR::vneg
   TR::TreeEvaluator::vcomEvaluator,        // TR::vcom
   TR::TreeEvaluator::vaddEvaluator,        // TR::vadd
   TR::TreeEvaluator::vsubEvaluator,        // TR::vsub
   TR::TreeEvaluator::vmulEvaluator,        // TR::vmul
   TR::TreeEvaluator::vdivEvaluator,        // TR::vdiv
   TR::TreeEvaluator::vremEvaluator,        // TR::vrem
   TR::TreeEvaluator::vandEvaluator,        // TR::vand
   TR::TreeEvaluator::vorEvaluator,         // TR::vor
   TR::TreeEvaluator::vxorEvaluator,        // TR::vxor
   TR::TreeEvaluator::vectorElementShiftHelper,        // TR::vshl
   TR::TreeEvaluator::vectorElementShiftHelper,        // TR::vushr
   TR::TreeEvaluator::vectorElementShiftHelper,        // TR::vshr
   TR::TreeEvaluator::vcmpeqEvaluator,      // TR::vcmpeq
   TR::TreeEvaluator::vcmpneEvaluator,      // TR::vcmpne
   TR::TreeEvaluator::vcmpltEvaluator,      // TR::vcmplt
   TR::TreeEvaluator::vcmpltEvaluator,      // TR::vucmplt
   TR::TreeEvaluator::vcmpgtEvaluator,      // TR::vcmpgt
   TR::TreeEvaluator::vcmpgtEvaluator,      // TR::vucmpgt
   TR::TreeEvaluator::vcmpleEvaluator,      // TR::vcmple
   TR::TreeEvaluator::vcmpleEvaluator,      // TR::vucmple
   TR::TreeEvaluator::vcmpgeEvaluator,      // TR::vcmpge
   TR::TreeEvaluator::vcmpgeEvaluator,      // TR::vucmpge
   TR::TreeEvaluator::vloadEvaluator,       // TR::vload
   TR::TreeEvaluator::vloadEvaluator,       // TR::vloadi
   TR::TreeEvaluator::vstoreEvaluator,      // TR::vstore
   TR::TreeEvaluator::vstoreEvaluator,      // TR::vstorei
   TR::TreeEvaluator::vrandEvaluator,       // TR::vrand
   TR::TreeEvaluator::vreturnEvaluator,     // TR::vreturn
   TR::TreeEvaluator::directCallEvaluator,  // TR::vcall
   TR::TreeEvaluator::indirectCallEvaluator,// TR::vcalli
   TR::TreeEvaluator::ternaryEvaluator,     // TR::vternary
   TR::TreeEvaluator::passThroughEvaluator, // TR::v2v
   TR::TreeEvaluator::vl2vdEvaluator,       // TR::vl2vd 
   TR::TreeEvaluator::vconstEvaluator,      // TR::vconst
   TR::TreeEvaluator::getvelemEvaluator,    // TR::getvelem
   TR::TreeEvaluator::vsetelemEvaluator,    // TR::vsetelem

   TR::TreeEvaluator::vRegLoadEvaluator,        //  TR::vbRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,        //  TR::vsRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,        //  TR::viRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,        //  TR::vlRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,        //  TR::vfRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,        //  TR::vdRegLoad
   TR::TreeEvaluator::vRegStoreEvaluator,       //  TR::vbRegStore
   TR::TreeEvaluator::vRegStoreEvaluator,       //  TR::vsRegStore
   TR::TreeEvaluator::vRegStoreEvaluator,       //  TR::viRegStore
   TR::TreeEvaluator::vRegStoreEvaluator,       //  TR::vlRegStore
   TR::TreeEvaluator::vRegStoreEvaluator,       //  TR::vfRegStore
   TR::TreeEvaluator::vRegStoreEvaluator,       //  TR::vdRegStore


/*
 *END OF OPCODES REQUIRED BY OMR
 */
   TR::TreeEvaluator::iconstEvaluator,      // TR::iuconst
   TR::TreeEvaluator::lconstEvaluator,      // TR::luconst
   TR::TreeEvaluator::bconstEvaluator,      // TR::buconst
   TR::TreeEvaluator::iloadEvaluator,       // TR::iuload
   TR::TreeEvaluator::lloadEvaluator,       // TR::luload
   TR::TreeEvaluator::bloadEvaluator,       // TR::buload
   TR::TreeEvaluator::iloadEvaluator,       // TR::iuloadi
   TR::TreeEvaluator::lloadEvaluator,       // TR::luloadi
   TR::TreeEvaluator::bloadEvaluator,       // TR::buloadi
   TR::TreeEvaluator::istoreEvaluator,      // TR::iustore
   TR::TreeEvaluator::lstoreEvaluator,      // TR::lustore
   TR::TreeEvaluator::bstoreEvaluator,      // TR::bustore
   TR::TreeEvaluator::istoreEvaluator,      // TR::iustorei
   TR::TreeEvaluator::lstoreEvaluator,      // TR::lustorei
   TR::TreeEvaluator::bstoreEvaluator,      // TR::bustorei
   TR::TreeEvaluator::returnEvaluator,      // TR::iureturn
   TR::TreeEvaluator::returnEvaluator,      // TR::lureturn
   TR::TreeEvaluator::directCallEvaluator,  // TR::iucall
   TR::TreeEvaluator::directCallEvaluator,  // TR::lucall
   TR::TreeEvaluator::iaddEvaluator,        // TR::iuadd
   TR::TreeEvaluator::laddEvaluator,        // TR::luadd
   TR::TreeEvaluator::baddEvaluator,        // TR::buadd
   TR::TreeEvaluator::isubEvaluator,        // TR::iusub
   TR::TreeEvaluator::lsubEvaluator,        // TR::lusub
   TR::TreeEvaluator::bsubEvaluator,        // TR::busub
   TR::TreeEvaluator::inegEvaluator,        // TR::iuneg
   TR::TreeEvaluator::lnegEvaluator,        // TR::luneg
   TR::TreeEvaluator::ishlEvaluator,        // TR::iushl
   TR::TreeEvaluator::lshlEvaluator,        // TR::lushl
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2iu
   TR::TreeEvaluator::f2lEvaluator,         // TR::f2lu
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2bu
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2c
   TR::TreeEvaluator::d2iEvaluator,         // TR::d2iu
   TR::TreeEvaluator::d2lEvaluator,         // TR::d2lu
   TR::TreeEvaluator::d2iEvaluator,         // TR::d2bu
   TR::TreeEvaluator::d2iEvaluator,         // TR::d2c
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::iuRegLoad
   TR::TreeEvaluator::lRegLoadEvaluator,    // TR::luRegLoad
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::iuRegStore
   TR::TreeEvaluator::lRegStoreEvaluator,   // TR::luRegStore
   TR::TreeEvaluator::ternaryEvaluator,     // TR::iuternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::luternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::buternary
   TR::TreeEvaluator::ternaryEvaluator,     // TR::suternary
   TR::TreeEvaluator::cconstEvaluator,      // TR::cconst
   TR::TreeEvaluator::sloadEvaluator,       // TR::cload
   TR::TreeEvaluator::sloadEvaluator,       // TR::cloadi
   TR::TreeEvaluator::cstoreEvaluator,      // TR::cstore
   TR::TreeEvaluator::cstoreEvaluator,      // TR::cstorei
   TR::TreeEvaluator::badILOpEvaluator,     // TR::monent
   TR::TreeEvaluator::badILOpEvaluator,     // TR::monexit
   TR::TreeEvaluator::badILOpEvaluator,     //TR::monexitfence
   TR::TreeEvaluator::badILOpEvaluator,      // TR::tstart
   TR::TreeEvaluator::badILOpEvaluator,     // TR::tfinish
   TR::TreeEvaluator::badILOpEvaluator,      // TR::tabort
   TR::TreeEvaluator::badILOpEvaluator,     // TR::instanceof
   TR::TreeEvaluator::badILOpEvaluator,     // TR::checkcast
   TR::TreeEvaluator::badILOpEvaluator,     // TR::checkcastAndNULLCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::New
   TR::TreeEvaluator::badILOpEvaluator,     // TR::newarray
   TR::TreeEvaluator::badILOpEvaluator,     // TR::anewarray
   TR::TreeEvaluator::badILOpEvaluator,     // TR::variableNew
   TR::TreeEvaluator::badILOpEvaluator,     // TR::variableNewArray
   TR::TreeEvaluator::badILOpEvaluator,     // TR::multianewarray
   TR::TreeEvaluator::badILOpEvaluator,     // TR::arraylength
   TR::TreeEvaluator::badILOpEvaluator,     // TR::contigarraylength
   TR::TreeEvaluator::badILOpEvaluator,     // TR::discontigarraylength
   TR::TreeEvaluator::indirectCallEvaluator,// TR::icalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::iucalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::lcalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::lucalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::fcalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::dcalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::acalli
   TR::TreeEvaluator::indirectCallEvaluator,// TR::calli
   TR::TreeEvaluator::fenceEvaluator,       // TR::fence
   TR::TreeEvaluator::badILOpEvaluator,     // TR::luaddh
   TR::TreeEvaluator::caddEvaluator,        // TR::cadd
   TR::TreeEvaluator::aiaddEvaluator,       // TR::aiadd
   TR::TreeEvaluator::aiaddEvaluator,       // TR::aiuadd
   TR::TreeEvaluator::aladdEvaluator,       // TR::aladd
   TR::TreeEvaluator::aladdEvaluator,       // TR::aluadd
   TR::TreeEvaluator::badILOpEvaluator,     // TR::lusubh
   TR::TreeEvaluator::csubEvaluator,        // TR::csub
   TR::TreeEvaluator::mulhEvaluator,        // TR::imulh
   TR::TreeEvaluator::mulhEvaluator,        // TR::iumulh
   TR::TreeEvaluator::lmulhEvaluator,       // TR::lmulh
   TR::TreeEvaluator::lmulhEvaluator,       // TR::lumulh

   TR::TreeEvaluator::ibits2fEvaluator,     // TR::ibits2f
   TR::TreeEvaluator::fbits2iEvaluator,     // TR::fbits2i
   TR::TreeEvaluator::lbits2dEvaluator,     // TR::lbits2d
   TR::TreeEvaluator::dbits2lEvaluator,     // TR::dbits2l

   TR::TreeEvaluator::lookupEvaluator,      // TR::lookup
   TR::TreeEvaluator::lookupEvaluator,      // TR::trtLookup
   TR::TreeEvaluator::NOPEvaluator,         // TR::Case
   TR::TreeEvaluator::tableEvaluator,       // TR::table
   TR::TreeEvaluator::exceptionRangeFenceEvaluator,       // TR::exceptionRangeFence
   TR::TreeEvaluator::exceptionRangeFenceEvaluator,       // TR::dbgFence
   TR::TreeEvaluator::NULLCHKEvaluator,     // TR::NULLCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ResolveCHK
   TR::TreeEvaluator::resolveAndNULLCHKEvaluator, // TR::ResolveAndNULLCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::DIVCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::OverflowCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::UnsignedOverflowCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::BNDCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ArrayCopyBNDCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::BNDCHKwithSpineCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::SpineCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ArrayStoreCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ArrayCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,     // TR::Ret
   TR::TreeEvaluator::arraycopyEvaluator,            // TR::arraycopy
   TR::TreeEvaluator::arraysetEvaluator,             // TR::arrayset
   TR::TreeEvaluator::arraytranslateEvaluator,       // TR::arraytranslate
   TR::TreeEvaluator::arraytranslateAndTestEvaluator,// TR::arraytranslateAndTest
   TR::TreeEvaluator::long2StringEvaluator, // TR::long2String
   TR::TreeEvaluator::bitOpMemEvaluator,    // TR::bitOpMem
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bitOpMemND
   TR::TreeEvaluator::arraycmpEvaluator,    // TR::arraycmp
   TR::TreeEvaluator::arraycmpEvaluatorWithPad, // TR::arraycmpWithPad
   TR::TreeEvaluator::NOPEvaluator,         // TR::allocationFence
   TR::TreeEvaluator::barrierFenceEvaluator,// TR::loadFence
   TR::TreeEvaluator::barrierFenceEvaluator,// TR::storeFence
   TR::TreeEvaluator::barrierFenceEvaluator,// TR::fullFence
   TR::TreeEvaluator::badILOpEvaluator,     // TR::MergeNew

   TR::TreeEvaluator::computeCCEvaluator,   // TR::computeCC
   TR::TreeEvaluator::butestEvaluator,      // TR::butest
   TR::TreeEvaluator::badILOpEvaluator,     // TR::sutest

   TR::TreeEvaluator::badILOpEvaluator,     // TR::bucmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bcmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::sucmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::scmp
   TR::TreeEvaluator::iucmpEvaluator,       // TR::iucmp
   TR::TreeEvaluator::icmpEvaluator,        // TR::icmp
   TR::TreeEvaluator::lucmpEvaluator,       // TR::lucmp

   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::ificmpo
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::ificmpno
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::iflcmpo
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::iflcmpno
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::ificmno
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::ificmnno
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::iflcmno
   TR::TreeEvaluator::ifxcmpoEvaluator,     // TR::iflcmnno
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iuaddc
   TR::TreeEvaluator::laddEvaluator,        // TR::luaddc
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iusubb
   TR::TreeEvaluator::lsubEvaluator,        // TR::lusubb
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::icmpset
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lcmpset
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::btestnset
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ibatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::isatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::iiatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ilatomicor


   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dexp
   TR::TreeEvaluator::branchEvaluator,      // TR::branch
   TR::TreeEvaluator::igotoEvaluator,       // TR::igoto



   TR::TreeEvaluator::unImpOpEvaluator,         // TR::bexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::buexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::sexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::cexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::iexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::iuexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::luexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fuexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::duexp

   TR::TreeEvaluator::ixfrsEvaluator,       // TR::ixfrs
   TR::TreeEvaluator::lxfrsEvaluator,       // TR::lxfrs
   TR::TreeEvaluator::fxfrsEvaluator,       // TR::fxfrs
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dxfrs

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fint
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dint
   TR::TreeEvaluator::fnintEvaluator,       // TR::fnint
   TR::TreeEvaluator::dnintEvaluator,       // TR::dnint

   TR::TreeEvaluator::fsqrtEvaluator,       // TR::fsqrt
   TR::TreeEvaluator::dsqrtEvaluator,       // TR::dsqrt

   TR::TreeEvaluator::getstackEvaluator,    // TR::getstack
   TR::TreeEvaluator::deallocaEvaluator,    // TR::dealloca

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ishfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lshfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::iushfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lushfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::bshfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::sshfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::bushfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::sushfl

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::idoz

   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dcos
   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dsin
   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dtan

   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dcosh
   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dsinh
   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dtanh

   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dacos
   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dasin
   TR::TreeEvaluator::libmFuncEvaluator,    // TR::datan

   TR::TreeEvaluator::libmFuncEvaluator,    // TR::datan2

   TR::TreeEvaluator::libmFuncEvaluator,    // TR::dlog


   TR::TreeEvaluator::unImpOpEvaluator,         // TR::imulover
   TR::TreeEvaluator::dfloorEvaluator,      // TR::dfloor
   TR::TreeEvaluator::ffloorEvaluator,      // TR::ffloor
   TR::TreeEvaluator::dceilEvaluator,       // TR::dceil
   TR::TreeEvaluator::fceilEvaluator,       // TR::fceil
   TR::TreeEvaluator::ibranchEvaluator,     // TR::ibranch
   TR::TreeEvaluator::mbranchEvaluator,     // TR::mbranch
   TR::TreeEvaluator::getpmEvaluator,       // TR::getpm
   TR::TreeEvaluator::setpmEvaluator,       // TR::setpm
   TR::TreeEvaluator::loadAutoOffsetEvaluator,// TR::loadAutoOffset

   TR::TreeEvaluator::maxEvaluator,         // TR::imax
   TR::TreeEvaluator::maxEvaluator,         // TR::iumax
   TR::TreeEvaluator::maxEvaluator,         // TR::lmax
   TR::TreeEvaluator::maxEvaluator,         // TR::lumax
   TR::TreeEvaluator::maxEvaluator,         // TR::fmax
   TR::TreeEvaluator::maxEvaluator,         // TR::dmax

   TR::TreeEvaluator::minEvaluator,         // TR::imin
   TR::TreeEvaluator::minEvaluator,         // TR::iumin
   TR::TreeEvaluator::minEvaluator,         // TR::lmin
   TR::TreeEvaluator::minEvaluator,         // TR::lumin
   TR::TreeEvaluator::minEvaluator,         // TR::fmin
   TR::TreeEvaluator::minEvaluator,         // TR::dmin
   TR::TreeEvaluator::trtEvaluator,         // TR::trt
   TR::TreeEvaluator::trtEvaluator,         // TR::trtSimple

   TR::TreeEvaluator::integerHighestOneBit,              // TR::ihbit (J9)
   TR::TreeEvaluator::integerLowestOneBit,               // TR::ilbit (J9)
   TR::TreeEvaluator::integerNumberOfLeadingZeros,       // TR::inolz (J9)
   TR::TreeEvaluator::integerNumberOfTrailingZeros,      // TR::inotz (J9)
   TR::TreeEvaluator::integerBitCount,                   // TR::ipopcnt
   TR::TreeEvaluator::longHighestOneBit,                 // TR::lhbit (J9)
   TR::TreeEvaluator::longLowestOneBit,                  // TR::llbit (J9)
   TR::TreeEvaluator::longNumberOfLeadingZeros,          // TR::lnolz (J9)
   TR::TreeEvaluator::longNumberOfTrailingZeros,         // TR::lnotz (J9)
   TR::TreeEvaluator::longBitCount,                      // TR::lpopcnt

   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ibyteswap

   TR::TreeEvaluator::bitpermuteEvaluator,     // TR::bbitpermute
   TR::TreeEvaluator::bitpermuteEvaluator,     // TR::sbitpermute
   TR::TreeEvaluator::bitpermuteEvaluator,     // TR::ibitpermute
   TR::TreeEvaluator::bitpermuteEvaluator,     // TR::lbitpermute

   TR::TreeEvaluator::PrefetchEvaluator,    // TR::Prefetch
