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

/*
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
   TR::TreeEvaluator::istoreEvaluator,      // TR::astore
#if J9_PROJECT_SPECIFIC
   TR::TreeEvaluator::wrtbarEvaluator,      // TR::wrtbar
#else
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::wrtbar
#endif
   TR::TreeEvaluator::bstoreEvaluator,      // TR::bstore
   TR::TreeEvaluator::sstoreEvaluator,      // TR::sstore
   TR::TreeEvaluator::lstoreEvaluator,      // TR::lstorei
   TR::TreeEvaluator::ifstoreEvaluator,     // TR::fstorei
   TR::TreeEvaluator::idstoreEvaluator,     // TR::dstorei
   TR::TreeEvaluator::istoreEvaluator,      // TR::astorei
#if J9_PROJECT_SPECIFIC
   TR::TreeEvaluator::iwrtbarEvaluator,     // TR::wrtbari
#else
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::wrtbari
#endif
   TR::TreeEvaluator::bstoreEvaluator,      // TR::bstorei
   TR::TreeEvaluator::sstoreEvaluator,      // TR::sstorei
   TR::TreeEvaluator::istoreEvaluator,      // TR::istorei
   TR::TreeEvaluator::gotoEvaluator,        // TR::Goto
   TR::TreeEvaluator::ireturnEvaluator,     // TR::ireturn
   TR::TreeEvaluator::lreturnEvaluator,     // TR::lreturn
   TR::TreeEvaluator::freturnEvaluator,     // TR::freturn
   TR::TreeEvaluator::dreturnEvaluator,     // TR::dreturn
   TR::TreeEvaluator::ireturnEvaluator,     // TR::areturn
   TR::TreeEvaluator::returnEvaluator,      // TR::Return
   TR::TreeEvaluator::asynccheckEvaluator,  // TR::asynccheck
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
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::badd
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::sadd
   TR::TreeEvaluator::isubEvaluator,        // TR::isub
   TR::TreeEvaluator::lsubEvaluator,        // TR::lsub
   TR::TreeEvaluator::fsubEvaluator,        // TR::fsub
   TR::TreeEvaluator::dsubEvaluator,        // TR::dsub
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bsub
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ssub
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::asub
   TR::TreeEvaluator::imulEvaluator,        // TR::imul
   TR::TreeEvaluator::lmulEvaluator,        // TR::lmul
   TR::TreeEvaluator::fmulEvaluator,        // TR::fmul
   TR::TreeEvaluator::dmulEvaluator,        // TR::dmul
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bmul
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::smul
   TR::TreeEvaluator::imulEvaluator,        // TR::iumul
   TR::TreeEvaluator::idivEvaluator,        // TR::idiv
   TR::TreeEvaluator::ldivEvaluator,        // TR::ldiv
   TR::TreeEvaluator::fdivEvaluator,        // TR::fdiv
   TR::TreeEvaluator::ddivEvaluator,        // TR::ddiv
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bdiv
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::sdiv
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::iudiv
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::ludiv
   TR::TreeEvaluator::iremEvaluator,        // TR::irem
   TR::TreeEvaluator::lremEvaluator,        // TR::lrem
   TR::TreeEvaluator::fremEvaluator,        // TR::frem
   TR::TreeEvaluator::dremEvaluator,        // TR::drem
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::brem
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::srem
   TR::TreeEvaluator::iremEvaluator,        // TR::iurem
   TR::TreeEvaluator::inegEvaluator,        // TR::ineg
   TR::TreeEvaluator::lnegEvaluator,        // TR::lneg
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::fnegEvaluator,        // TR::fneg
#else
   TR::TreeEvaluator::dnegEvaluator,        // TR::fneg
#endif
   TR::TreeEvaluator::dnegEvaluator,        // TR::dneg
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bneg
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::sneg

   TR::TreeEvaluator::iabsEvaluator,        // TR::iabs
   TR::TreeEvaluator::labsEvaluator,        // TR::labs
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::fabsEvaluator,        // TR::fabs
   TR::TreeEvaluator::dabsEvaluator,        // TR::dabs
#else
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::fabs
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::dabs
#endif
   TR::TreeEvaluator::ishlEvaluator,        // TR::ishl
   TR::TreeEvaluator::lshlEvaluator,        // TR::lshl
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bshl
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::sshl
   TR::TreeEvaluator::ishrEvaluator,        // TR::ishr
   TR::TreeEvaluator::lshrEvaluator,        // TR::lshr
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bshr
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::sshr
   TR::TreeEvaluator::iushrEvaluator,       // TR::iushr
   TR::TreeEvaluator::lushrEvaluator,       // TR::lushr
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bushr
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::sushr
   TR::TreeEvaluator::irolEvaluator,        // TR::irol
   TR::TreeEvaluator::lrolEvaluator,        // TR::lrol
   TR::TreeEvaluator::iandEvaluator,        // TR::iand
   TR::TreeEvaluator::landEvaluator,        // TR::land
   TR::TreeEvaluator::iandEvaluator,        // TR::band
   TR::TreeEvaluator::iandEvaluator,        // TR::sand
   TR::TreeEvaluator::iorEvaluator,         // TR::ior
   TR::TreeEvaluator::lorEvaluator,         // TR::lor
   TR::TreeEvaluator::iorEvaluator,         // TR::bor
   TR::TreeEvaluator::iorEvaluator,         // TR::sor
   TR::TreeEvaluator::ixorEvaluator,        // TR::ixor
   TR::TreeEvaluator::lxorEvaluator,        // TR::lxor
   TR::TreeEvaluator::ixorEvaluator,        // TR::bxor
   TR::TreeEvaluator::ixorEvaluator,        // TR::sxor

   TR::TreeEvaluator::b2lEvaluator,         // TR::i2l
   TR::TreeEvaluator::i2fEvaluator,         // TR::i2f
   TR::TreeEvaluator::i2dEvaluator,         // TR::i2d
   TR::TreeEvaluator::i2bEvaluator,         // TR::i2b
   TR::TreeEvaluator::i2sEvaluator,         // TR::i2s
   TR::TreeEvaluator::passThroughEvaluator, // TR::i2a

   TR::TreeEvaluator::iu2lEvaluator,        // TR::iu2l
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::i2fEvaluator,         // TR::iu2f
   TR::TreeEvaluator::i2dEvaluator,         // TR::iu2d
#else
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iu2f
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iu2d
#endif
   TR::TreeEvaluator::passThroughEvaluator, // TR::iu2a

   TR::TreeEvaluator::l2iEvaluator,         // TR::l2i
   TR::TreeEvaluator::l2fEvaluator,         // TR::l2f
   TR::TreeEvaluator::l2dEvaluator,         // TR::l2d
   TR::TreeEvaluator::i2bEvaluator,         // TR::l2b
   TR::TreeEvaluator::i2sEvaluator,         // TR::l2s
   TR::TreeEvaluator::l2iEvaluator,         // TR::l2a

   TR::TreeEvaluator::badILOpEvaluator,     // TR::lu2f
   TR::TreeEvaluator::badILOpEvaluator,     // TR::lu2d
   TR::TreeEvaluator::l2iEvaluator,         // TR::lu2a

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2i
   TR::TreeEvaluator::f2lEvaluator,         // TR::f2l
#else
   TR::TreeEvaluator::d2iEvaluator,         // TR::f2i
   TR::TreeEvaluator::d2lEvaluator,         // TR::f2l
#endif
   TR::TreeEvaluator::f2dEvaluator,         // TR::f2d
   TR::TreeEvaluator::d2bEvaluator,         // TR::f2b
   TR::TreeEvaluator::d2sEvaluator,         // TR::f2s

   TR::TreeEvaluator::d2iEvaluator,         // TR::d2i
   TR::TreeEvaluator::d2lEvaluator,         // TR::d2l
   TR::TreeEvaluator::d2fEvaluator,         // TR::d2f
   TR::TreeEvaluator::d2bEvaluator,         // TR::d2b
   TR::TreeEvaluator::d2sEvaluator,         // TR::d2s

   TR::TreeEvaluator::b2iEvaluator,         // TR::b2i
   TR::TreeEvaluator::b2lEvaluator,         // TR::b2l
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::i2fEvaluator,         // TR::b2f
   TR::TreeEvaluator::i2dEvaluator,         // TR::b2d
#else
   TR::TreeEvaluator::b2dEvaluator,         // TR::b2f
   TR::TreeEvaluator::b2dEvaluator,         // TR::b2d
#endif
   TR::TreeEvaluator::b2iEvaluator,         // TR::b2s
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::b2a

   TR::TreeEvaluator::bu2iEvaluator,        // TR::bu2i
   TR::TreeEvaluator::bu2lEvaluator,        // TR::bu2l
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bu2f
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bu2d
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bu2s
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::bu2a

   TR::TreeEvaluator::b2iEvaluator,         // TR::s2i
   TR::TreeEvaluator::b2lEvaluator,         // TR::s2l
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::i2fEvaluator,         // TR::s2f
   TR::TreeEvaluator::i2dEvaluator,         // TR::s2d
#else
   TR::TreeEvaluator::s2dEvaluator,         // TR::s2f
   TR::TreeEvaluator::s2dEvaluator,         // TR::s2d
#endif
   TR::TreeEvaluator::i2bEvaluator,         // TR::s2b
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::s2a

   TR::TreeEvaluator::i2cEvaluator,         // TR::su2i
   TR::TreeEvaluator::su2lEvaluator,         // TR::su2l
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::i2fEvaluator,         // TR::su2f
   TR::TreeEvaluator::i2dEvaluator,         // TR::su2d
#else
   TR::TreeEvaluator::su2dEvaluator,         // TR::su2f
   TR::TreeEvaluator::su2dEvaluator,         // TR::su2d
#endif
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::su2a

   TR::TreeEvaluator::passThroughEvaluator, // TR::a2i
   TR::TreeEvaluator::passThroughEvaluator, // TR::a2l
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::a2b
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::a2s

   TR::TreeEvaluator::icmpeqEvaluator,      // TR::icmpeq
   TR::TreeEvaluator::icmpneEvaluator,      // TR::icmpne
   TR::TreeEvaluator::icmpltEvaluator,      // TR::icmplt
   TR::TreeEvaluator::icmpgeEvaluator,      // TR::icmpge
   TR::TreeEvaluator::icmpgtEvaluator,      // TR::icmpgt
   TR::TreeEvaluator::icmpleEvaluator,      // TR::icmple
   TR::TreeEvaluator::icmpeqEvaluator,      // TR::iucmpeq
   TR::TreeEvaluator::icmpneEvaluator,      // TR::iucmpne
   TR::TreeEvaluator::iucmpltEvaluator,     // TR::iucmplt
   TR::TreeEvaluator::iucmpgeEvaluator,     // TR::iucmpge
   TR::TreeEvaluator::iucmpgtEvaluator,     // TR::iucmpgt
   TR::TreeEvaluator::iucmpleEvaluator,     // TR::iucmple

   TR::TreeEvaluator::lcmpeqEvaluator,      // TR::lcmpeq
   TR::TreeEvaluator::lcmpneEvaluator,      // TR::lcmpne
   TR::TreeEvaluator::lcmpltEvaluator,      // TR::lcmplt
   TR::TreeEvaluator::lcmpgeEvaluator,      // TR::lcmpge
   TR::TreeEvaluator::lcmpgtEvaluator,      // TR::lcmpgt
   TR::TreeEvaluator::lcmpleEvaluator,      // TR::lcmple
   TR::TreeEvaluator::lcmpeqEvaluator,      // TR::lucmpeq
   TR::TreeEvaluator::lcmpneEvaluator,      // TR::lucmpne
   TR::TreeEvaluator::lucmpltEvaluator,     // TR::lucmplt
   TR::TreeEvaluator::lucmpgeEvaluator,     // TR::lucmpge
   TR::TreeEvaluator::lucmpgtEvaluator,     // TR::lucmpgt
   TR::TreeEvaluator::lucmpleEvaluator,     // TR::lucmple

   TR::TreeEvaluator::dcmpeqEvaluator,      // TR::fcmpeq
   TR::TreeEvaluator::dcmpneEvaluator,      // TR::fcmpne
   TR::TreeEvaluator::dcmpltEvaluator,      // TR::fcmplt
   TR::TreeEvaluator::dcmpgeEvaluator,      // TR::fcmpge
   TR::TreeEvaluator::dcmpgtEvaluator,      // TR::fcmpgt
   TR::TreeEvaluator::dcmpleEvaluator,      // TR::fcmple
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::dcmpeqEvaluator,      // TR::fcmpequ
#else
   TR::TreeEvaluator::dcmpequEvaluator,     // TR::fcmpequ
#endif
   TR::TreeEvaluator::dcmpneEvaluator,      // TR::fcmpneu
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::dcmpltEvaluator,      // TR::fcmpltu
   TR::TreeEvaluator::dcmpgeEvaluator,      // TR::fcmpgeu
   TR::TreeEvaluator::dcmpgtEvaluator,      // TR::fcmpgtu
   TR::TreeEvaluator::dcmpleEvaluator,      // TR::fcmpleu
#else
   TR::TreeEvaluator::dcmpltuEvaluator,     // TR::fcmpltu
   TR::TreeEvaluator::dcmpgeuEvaluator,     // TR::fcmpgeu
   TR::TreeEvaluator::dcmpgtuEvaluator,     // TR::fcmpgtu
   TR::TreeEvaluator::dcmpleuEvaluator,     // TR::fcmpleu
#endif
   TR::TreeEvaluator::dcmpeqEvaluator,      // TR::dcmpeq
   TR::TreeEvaluator::dcmpneEvaluator,      // TR::dcmpne
   TR::TreeEvaluator::dcmpltEvaluator,      // TR::dcmplt
   TR::TreeEvaluator::dcmpgeEvaluator,      // TR::dcmpge
   TR::TreeEvaluator::dcmpgtEvaluator,      // TR::dcmpgt
   TR::TreeEvaluator::dcmpleEvaluator,      // TR::dcmple
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::dcmpeqEvaluator,      // TR::dcmpequ
#else
   TR::TreeEvaluator::dcmpequEvaluator,     // TR::dcmpequ
#endif
   TR::TreeEvaluator::dcmpneEvaluator,      // TR::dcmpneu
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::dcmpltEvaluator,      // TR::dcmpltu
   TR::TreeEvaluator::dcmpgeEvaluator,      // TR::dcmpgeu
   TR::TreeEvaluator::dcmpgtEvaluator,      // TR::dcmpgtu
   TR::TreeEvaluator::dcmpleEvaluator,      // TR::dcmpleu
#else
   TR::TreeEvaluator::dcmpltuEvaluator,     // TR::dcmpltu
   TR::TreeEvaluator::dcmpgeuEvaluator,     // TR::dcmpgeu
   TR::TreeEvaluator::dcmpgtuEvaluator,     // TR::dcmpgtu
   TR::TreeEvaluator::dcmpleuEvaluator,     // TR::dcmpleu
#endif
   TR::TreeEvaluator::icmpeqEvaluator,       // TR::acmpeq
   TR::TreeEvaluator::icmpneEvaluator,       // TR::acmpne
   TR::TreeEvaluator::iucmpltEvaluator,      // TR::acmplt
   TR::TreeEvaluator::iucmpgeEvaluator,      // TR::acmpge
   TR::TreeEvaluator::iucmpgtEvaluator,      // TR::acmpgt
   TR::TreeEvaluator::iucmpleEvaluator,      // TR::acmple

   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bcmpne
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bcmplt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bcmpge
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bcmple
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bucmpeq
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bucmpne
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bucmplt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bucmpge
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bucmpgt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::bucmple
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::scmpeq
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::scmpne
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::scmplt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::scmpge
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::scmpgt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::scmple
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::sucmpeq
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::sucmpne
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::sucmplt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::sucmpge
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::sucmpgt
   TR::TreeEvaluator::unImpOpEvaluator,      // TR::sucmple
   TR::TreeEvaluator::lcmpEvaluator,        // TR::lcmp
   TR::TreeEvaluator::dcmplEvaluator,       // TR::fcmpl
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::dcmplEvaluator,       // TR::fcmpg
   TR::TreeEvaluator::dcmplEvaluator,       // TR::dcmpl
   TR::TreeEvaluator::dcmplEvaluator,       // TR::dcmpg
#else
   TR::TreeEvaluator::dcmpgEvaluator,       // TR::fcmpg
   TR::TreeEvaluator::dcmplEvaluator,       // TR::dcmpl
   TR::TreeEvaluator::dcmpgEvaluator,       // TR::dcmpg
#endif
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ificmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ificmpne
   TR::TreeEvaluator::ificmpltEvaluator,    // TR::ificmplt
   TR::TreeEvaluator::ificmpgeEvaluator,    // TR::ificmpge
   TR::TreeEvaluator::ificmpgtEvaluator,    // TR::ificmpgt
   TR::TreeEvaluator::ificmpleEvaluator,    // TR::ificmple
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ifiucmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,    // TR::ifiucmpne
   TR::TreeEvaluator::ifiucmpltEvaluator,   // TR::ifiucmplt
   TR::TreeEvaluator::ifiucmpgeEvaluator,   // TR::ifiucmpge
   TR::TreeEvaluator::ifiucmpgtEvaluator,   // TR::ifiucmpgt
   TR::TreeEvaluator::ifiucmpleEvaluator,   // TR::ifiucmple

   TR::TreeEvaluator::iflcmpeqEvaluator,    // TR::iflcmpeq
   TR::TreeEvaluator::iflcmpeqEvaluator,    // TR::iflcmpne
   TR::TreeEvaluator::iflcmpltEvaluator,    // TR::iflcmplt
   TR::TreeEvaluator::iflcmpgeEvaluator,    // TR::iflcmpge
   TR::TreeEvaluator::iflcmpgtEvaluator,    // TR::iflcmpgt
   TR::TreeEvaluator::iflcmpleEvaluator,    // TR::iflcmple
   TR::TreeEvaluator::iflcmpeqEvaluator,    // TR::iflucmpeq
   TR::TreeEvaluator::iflcmpeqEvaluator,    // TR::iflucmpne
   TR::TreeEvaluator::iflucmpltEvaluator,   // TR::iflucmplt
   TR::TreeEvaluator::iflucmpgeEvaluator,   // TR::iflucmpge
   TR::TreeEvaluator::iflucmpgtEvaluator,   // TR::iflucmpgt
   TR::TreeEvaluator::iflucmpleEvaluator,   // TR::iflucmple

   TR::TreeEvaluator::ifdcmpeqEvaluator,    // TR::iffcmpeq
   TR::TreeEvaluator::ifdcmpneEvaluator,    // TR::iffcmpne
   TR::TreeEvaluator::ifdcmpltEvaluator,    // TR::iffcmplt
   TR::TreeEvaluator::ifdcmpgeEvaluator,    // TR::iffcmpge
   TR::TreeEvaluator::ifdcmpgtEvaluator,    // TR::iffcmpgt
   TR::TreeEvaluator::ifdcmpleEvaluator,    // TR::iffcmple
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::ifdcmpeqEvaluator,    // TR::iffcmpequ
#else
   TR::TreeEvaluator::ifdcmpequEvaluator,   // TR::iffcmpequ
#endif
   TR::TreeEvaluator::ifdcmpneEvaluator,    // TR::iffcmpneu
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::ifdcmpltEvaluator,    // TR::iffcmpltu
   TR::TreeEvaluator::ifdcmpgeEvaluator,    // TR::iffcmpgeu
   TR::TreeEvaluator::ifdcmpgtEvaluator,    // TR::iffcmpgtu
   TR::TreeEvaluator::ifdcmpleEvaluator,    // TR::iffcmpleu
#else
   TR::TreeEvaluator::ifdcmpltuEvaluator,   // TR::iffcmpltu
   TR::TreeEvaluator::ifdcmpgeuEvaluator,   // TR::iffcmpgeu
   TR::TreeEvaluator::ifdcmpgtuEvaluator,   // TR::iffcmpgtu
   TR::TreeEvaluator::ifdcmpleuEvaluator,   // TR::iffcmpleu
#endif
   TR::TreeEvaluator::ifdcmpeqEvaluator,    // TR::ifdcmpeq
   TR::TreeEvaluator::ifdcmpneEvaluator,    // TR::ifdcmpne
   TR::TreeEvaluator::ifdcmpltEvaluator,    // TR::ifdcmplt
   TR::TreeEvaluator::ifdcmpgeEvaluator,    // TR::ifdcmpge
   TR::TreeEvaluator::ifdcmpgtEvaluator,    // TR::ifdcmpgt
   TR::TreeEvaluator::ifdcmpleEvaluator,    // TR::ifdcmple
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::ifdcmpeqEvaluator,    // TR::ifdcmpequ
#else
   TR::TreeEvaluator::ifdcmpequEvaluator,   // TR::ifdcmpequ
#endif
   TR::TreeEvaluator::ifdcmpneEvaluator,    // TR::ifdcmpneu
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::ifdcmpltEvaluator,    // TR::ifdcmpltu
   TR::TreeEvaluator::ifdcmpgeEvaluator,    // TR::ifdcmpgeu
   TR::TreeEvaluator::ifdcmpgtEvaluator,    // TR::ifdcmpgtu
   TR::TreeEvaluator::ifdcmpleEvaluator,    // TR::ifdcmpleu
#else
   TR::TreeEvaluator::ifdcmpltuEvaluator,   // TR::ifdcmpltu
   TR::TreeEvaluator::ifdcmpgeuEvaluator,   // TR::ifdcmpgeu
   TR::TreeEvaluator::ifdcmpgtuEvaluator,   // TR::ifdcmpgtu
   TR::TreeEvaluator::ifdcmpleuEvaluator,   // TR::ifdcmpleu
#endif
   TR::TreeEvaluator::ifacmpeqEvaluator,    // TR::ifacmpeq

   TR::TreeEvaluator::ificmpeqEvaluator,   // TR::ifacmpne
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifacmplt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifacmpge
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifacmpgt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifacmple

   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbcmpne
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbcmplt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbcmpge
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbcmple
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbucmpeq
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbucmpne
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbucmplt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbucmpge
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbucmpgt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifbucmple
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifscmpeq
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifscmpne
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifscmplt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifscmpge
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifscmpgt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifscmple
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifsucmpeq
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifsucmpne
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifsucmplt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifsucmpge
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifsucmpgt
   TR::TreeEvaluator::unImpOpEvaluator,    // TR::ifsucmple
   TR::TreeEvaluator::loadaddrEvaluator,    // TR::loadaddr
   TR::TreeEvaluator::ZEROCHKEvaluator,     // TR::ZEROCHK
   TR::TreeEvaluator::unImpOpEvaluator,        // TR::callIf
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::iRegLoad
   TR::TreeEvaluator::aRegLoadEvaluator,    // TR::aRegLoad
   TR::TreeEvaluator::lRegLoadEvaluator,    // TR::lRegLoad
   TR::TreeEvaluator::fRegLoadEvaluator,    // TR::fRegLoad
   TR::TreeEvaluator::fRegLoadEvaluator,    // TR::dRegLoad
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::sRegLoad
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::bRegLoad
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::iRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::aRegStore
   TR::TreeEvaluator::lRegStoreEvaluator,   // TR::lRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,   // TR::fRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,   // TR::dRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::sRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::bRegStore
   TR::TreeEvaluator::GlRegDepsEvaluator,   // TR::GlRegDeps
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::lternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::sternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::aternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::fternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::dternary
   TR::TreeEvaluator::treetopEvaluator,     // TR::treetop
   TR::TreeEvaluator::badILOpEvaluator,     // TR::MethodEnterHook
   TR::TreeEvaluator::badILOpEvaluator,     // TR::MethodExitHook
   TR::TreeEvaluator::passThroughEvaluator, // TR::PassThrough
   TR::TreeEvaluator::badILOpEvaluator,     // TR::compressedRefs

   TR::TreeEvaluator::BBStartEvaluator,     // TR::BBStart
   TR::TreeEvaluator::BBEndEvaluator,        // TR::BBEnd

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
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vselect
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vperm
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vsplats
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmergel
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmergeh
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdsetelem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdgetelem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdsel
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdrem
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdmadd
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

   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vinc
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdec
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vneg
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcom
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vadd
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vsub
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vmul
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdiv
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vrem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vand
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vor
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vxor
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vshl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vushr
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vshr
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcmpne
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcmplt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vucmplt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vucmpgt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcmple
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vucmple
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcmpge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vucmpge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vload
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vloadi
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vstore
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vstorei
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vrand
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vreturn
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcall
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcalli
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vternary
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::v2v
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vl2vd
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vconst
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::getvelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vsetelem

   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vbRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vsRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::viRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vlRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vfRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdRegLoad
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vbRegStore
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vsRegStore
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vlRegStore
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vlRegStore
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vfRegStore
   TR::TreeEvaluator::unImpOpEvaluator,         //  TR::vdRegStore

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
   TR::TreeEvaluator::ireturnEvaluator,     // TR::iureturn
   TR::TreeEvaluator::lreturnEvaluator,     // TR::lureturn
   TR::TreeEvaluator::directCallEvaluator,  // TR::iucall
   TR::TreeEvaluator::directCallEvaluator,  // TR::lucall
   TR::TreeEvaluator::iaddEvaluator,        // TR::iuadd
   TR::TreeEvaluator::laddEvaluator,        // TR::luadd
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::buadd
   TR::TreeEvaluator::isubEvaluator,        // TR::iusub
   TR::TreeEvaluator::lsubEvaluator,        // TR::lusub
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::busub
   TR::TreeEvaluator::inegEvaluator,        // TR::iuneg
   TR::TreeEvaluator::lnegEvaluator,        // TR::luneg
   TR::TreeEvaluator::ishlEvaluator,        // TR::iushl
   TR::TreeEvaluator::lshlEvaluator,        // TR::lushl
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::f2iEvaluator,         // TR::f2iu
   TR::TreeEvaluator::f2lEvaluator,         // TR::f2lu
#else
   TR::TreeEvaluator::d2iEvaluator,         // TR::f2iu
   TR::TreeEvaluator::d2lEvaluator,         // TR::f2lu
#endif
   TR::TreeEvaluator::d2bEvaluator,         // TR::f2bu
   TR::TreeEvaluator::d2cEvaluator,         // TR::f2c
   TR::TreeEvaluator::d2iEvaluator,         // TR::d2iu
   TR::TreeEvaluator::d2lEvaluator,         // TR::d2lu
   TR::TreeEvaluator::d2bEvaluator,         // TR::d2bu
   TR::TreeEvaluator::d2cEvaluator,         // TR::d2c
   TR::TreeEvaluator::iRegLoadEvaluator,    // TR::iuRegLoad
   TR::TreeEvaluator::lRegLoadEvaluator,    // TR::luRegLoad
   TR::TreeEvaluator::iRegStoreEvaluator,   // TR::iuRegStore
   TR::TreeEvaluator::lRegStoreEvaluator,   // TR::luRegStore
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iuternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::luternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::buternary
   TR::TreeEvaluator::badILOpEvaluator,     // TR::suternary
   TR::TreeEvaluator::cconstEvaluator,      // TR::cconst
   TR::TreeEvaluator::cloadEvaluator,       // TR::cload
   TR::TreeEvaluator::cloadEvaluator,       // TR::cloadi
   TR::TreeEvaluator::sstoreEvaluator,      // TR::cstore
   TR::TreeEvaluator::sstoreEvaluator,      // TR::cstorei
#if J9_PROJECT_SPECIFIC
   TR::TreeEvaluator::monentEvaluator,      // TR::monent
   TR::TreeEvaluator::monexitEvaluator,     // TR::monexit
#else
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::monent
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::monexit
#endif
   TR::TreeEvaluator::monexitfenceEvaluator, // TR::monexitfence
   TR::TreeEvaluator::badILOpEvaluator,     // TR::tstart
   TR::TreeEvaluator::badILOpEvaluator,     // TR::tfinish
   TR::TreeEvaluator::badILOpEvaluator,     // TR::tabort
#if J9_PROJECT_SPECIFIC
   TR::TreeEvaluator::instanceofEvaluator,  // TR::instanceof
   TR::TreeEvaluator::checkcastEvaluator,     // TR::checkcast
   TR::TreeEvaluator::checkcastAndNULLCHKEvaluator,     // TR::checkcastAndNULLCHK
   TR::TreeEvaluator::newObjectEvaluator,   // TR::New
   TR::TreeEvaluator::newArrayEvaluator,    // TR::newarray
   TR::TreeEvaluator::anewArrayEvaluator,   // TR::anewarray
   TR::TreeEvaluator::newObjectEvaluator,   // TR::variableNew
   TR::TreeEvaluator::anewArrayEvaluator,    // TR::variableNewArray
#else
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::instanceof
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::checkcast
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::checkcastAndNULLCHK
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::New
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::newarray
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::anewarray
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::variableNew
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::variableNewArray
#endif
   TR::TreeEvaluator::multianewArrayEvaluator,// TR::multianewarray
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
   TR::TreeEvaluator::badILOpEvaluator,     // TR::fence
   TR::TreeEvaluator::badILOpEvaluator,     // TR::luaddh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::cadd
   TR::TreeEvaluator::iaddEvaluator,        // TR::aiadd
   TR::TreeEvaluator::iaddEvaluator,        // TR::aiuadd
   TR::TreeEvaluator::iaddEvaluator,        // TR::aladd
   TR::TreeEvaluator::iaddEvaluator,        // TR::aluadd
   TR::TreeEvaluator::badILOpEvaluator,     // TR::lusubh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::csub
   TR::TreeEvaluator::imulhEvaluator,       // TR::imulh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::iumulh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::lmulh
   TR::TreeEvaluator::unImpOpEvaluator,     // TR::lumulh
//   TR::TreeEvaluator::cmulEvaluator,      // TR::cmul
//   TR::TreeEvaluator::cdivEvaluator,      // TR::cdiv
//   TR::TreeEvaluator::cremEvaluator,      // TR::crem
//   TR::TreeEvaluator::cshlEvaluator,      // TR::cshl
//   TR::TreeEvaluator::cushrEvaluator,     // TR::cushr

   TR::TreeEvaluator::ibits2fEvaluator,     // TR::ibits2f
   TR::TreeEvaluator::fbits2iEvaluator,     // TR::fbits2i
   TR::TreeEvaluator::lbits2dEvaluator,     // TR::lbits2d
   TR::TreeEvaluator::dbits2lEvaluator,     // TR::dbits2l
   TR::TreeEvaluator::lookupEvaluator,      // TR::lookup
   TR::TreeEvaluator::NOPEvaluator,         // TR::trtLookup
   TR::TreeEvaluator::NOPEvaluator,         // TR::Case
   TR::TreeEvaluator::tableEvaluator,       // TR::table
   TR::TreeEvaluator::exceptionRangeFenceEvaluator,       // TR::exceptionRangeFence
   TR::TreeEvaluator::exceptionRangeFenceEvaluator,       // TR::dbgFence
   TR::TreeEvaluator::NULLCHKEvaluator,     // TR::NULLCHK
   TR::TreeEvaluator::resolveCHKEvaluator,  // TR::ResolveCHK
   TR::TreeEvaluator::resolveAndNULLCHKEvaluator, // TR::ResolveAndNULLCHK
   TR::TreeEvaluator::DIVCHKEvaluator,      // TR::DIVCHK
   TR::TreeEvaluator::badILOpEvaluator,      // TR::OverflowCHK
   TR::TreeEvaluator::badILOpEvaluator,      // TR::UnsignedOverflowCHK
   TR::TreeEvaluator::BNDCHKEvaluator,      // TR::BNDCHK
   TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator,// TR::ArrayCopyBNDCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::BNDCHKwithSpineCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::SpineCHK
   TR::TreeEvaluator::ArrayStoreCHKEvaluator,// TR::ArrayStoreCHK
   TR::TreeEvaluator::ArrayCHKEvaluator,    // TR::ArrayCHK
   TR::TreeEvaluator::badILOpEvaluator,     // TR::Ret
   TR::TreeEvaluator::arraycopyEvaluator,   // TR::arraycopy
   TR::TreeEvaluator::arraysetEvaluator,    // TR::arrayset
   TR::TreeEvaluator::arraytranslateEvaluator,    // TR::arraytranslate
   TR::TreeEvaluator::arraytranslateAndTestEvaluator,    // TR::arraytranslateAndTest
   TR::TreeEvaluator::badILOpEvaluator,     // TR::long2String
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bitOpMem
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bitOpMemND
   TR::TreeEvaluator::arraycmpEvaluator,    // TR::arraycmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::arraycmpWithPad
   TR::TreeEvaluator::NOPEvaluator,         // TR::allocationFence
   TR::TreeEvaluator::NOPEvaluator,         // TR::loadFence
   TR::TreeEvaluator::NOPEvaluator,         // TR::storeFence
   TR::TreeEvaluator::NOPEvaluator,         // TR::fullFence
   TR::TreeEvaluator::badILOpEvaluator,     // TR::MergeNew
   TR::TreeEvaluator::badILOpEvaluator,     // TR::computeCC
   TR::TreeEvaluator::badILOpEvaluator,     // TR::butest
   TR::TreeEvaluator::badILOpEvaluator,     // TR::sutest
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bucmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::bcmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::sucmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::scmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iucmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::icmp
   TR::TreeEvaluator::badILOpEvaluator,     // TR::lucmp

   TR::TreeEvaluator::badILOpEvaluator,     // TR::ificmpo
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ificmpno
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iflcmpo
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iflcmpno
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ificmno
   TR::TreeEvaluator::badILOpEvaluator,     // TR::ificmnno
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iflcmno
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iflcmnno

   TR::TreeEvaluator::badILOpEvaluator,     // TR::iuaddc
   TR::TreeEvaluator::badILOpEvaluator,     // TR::luaddc
   TR::TreeEvaluator::badILOpEvaluator,     // TR::iusubb
   TR::TreeEvaluator::badILOpEvaluator,     // TR::lusubb

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::icmpset
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lcmpset
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::btestnset
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ibatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::isatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::iiatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ilatomicor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dexp
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::branch
   TR::TreeEvaluator::igotoEvaluator,        // TR::igoto


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

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ixfrs
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lxfrs
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fxfrs
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dxfrs

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fint
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dint
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fnint
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dnint

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fsqrt
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dsqrt

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::getstack
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dealloca

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ishfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lshfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::iushfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::lushfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::bshfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::sshfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::bushfl
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::sushfl

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::idoz

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dcos
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dsin
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dtan

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dcosh
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dsinh
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dtanh

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dacos
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dasin
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::datan

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::datan2

   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dlog


   TR::TreeEvaluator::unImpOpEvaluator,         // TR::imulover
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::dfloor
   TR::TreeEvaluator::unImpOpEvaluator,          // TR::ffloor
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dceil
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fceil
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::ibranch
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::mbranch
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::getpm
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::setpm
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::loadAutoOffset
//#endif


   TR::TreeEvaluator::maxEvaluator,          // TR::imax
   TR::TreeEvaluator::maxEvaluator,          // TR::iumax
   TR::TreeEvaluator::maxEvaluator,          // TR::lmax
   TR::TreeEvaluator::maxEvaluator,          // TR::lumax
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::fmaxEvaluator,         // TR::fmax
   TR::TreeEvaluator::fmaxEvaluator,         // TR::dmax
#else
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fmax
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dmax
#endif
   TR::TreeEvaluator::minEvaluator,          // TR::imin
   TR::TreeEvaluator::minEvaluator,          // TR::iumin
   TR::TreeEvaluator::minEvaluator,          // TR::lmin
   TR::TreeEvaluator::minEvaluator,          // TR::lumin
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR::TreeEvaluator::fminEvaluator,         // TR::fmin
   TR::TreeEvaluator::fminEvaluator,         // TR::dmin
#else
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::fmin
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::dmin
#endif
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::trt
   TR::TreeEvaluator::unImpOpEvaluator,         // TR::trtSimple

	TR::TreeEvaluator::integerHighestOneBit,
	TR::TreeEvaluator::integerLowestOneBit,
	TR::TreeEvaluator::integerNumberOfLeadingZeros,
	TR::TreeEvaluator::integerNumberOfTrailingZeros,
	TR::TreeEvaluator::integerBitCount,
	TR::TreeEvaluator::longHighestOneBit,
	TR::TreeEvaluator::longLowestOneBit,
	TR::TreeEvaluator::longNumberOfLeadingZeros,
	TR::TreeEvaluator::longNumberOfTrailingZeros,
	TR::TreeEvaluator::longBitCount,

   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::ibyteswap

   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bbitpermute
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::sbitpermute
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::ibitpermute
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::lbitpermute

   TR::TreeEvaluator::NOPEvaluator,         // Temporarily using for TR::Prefetch
