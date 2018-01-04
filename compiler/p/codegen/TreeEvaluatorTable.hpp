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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/*
 * This table is #included in a static table.
 * Only Function Pointers are allowed.
 */
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::BadILOp
   TR::TreeEvaluator::aconstEvaluator,                  // TR::aconst
   TR::TreeEvaluator::iconstEvaluator,                  // TR::iconst
   TR::TreeEvaluator::lconstEvaluator,                  // TR::lconst
   TR::TreeEvaluator::fconstEvaluator,                  // TR::fconst
   TR::TreeEvaluator::dconstEvaluator,                  // TR::dconst
   TR::TreeEvaluator::iconstEvaluator,                  // TR::bconst
   TR::TreeEvaluator::iconstEvaluator,                  // TR::sconst
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iload
   TR::TreeEvaluator::floadEvaluator,                   // TR::fload
   TR::TreeEvaluator::dloadEvaluator,                   // TR::dload
   TR::TreeEvaluator::aloadEvaluator,                   // TR::aload ibm@59591
   TR::TreeEvaluator::bloadEvaluator,                   // TR::bload
   TR::TreeEvaluator::sloadEvaluator,                   // TR::sload
   TR::TreeEvaluator::lloadEvaluator,                   // TR::lload
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iloadi
   TR::TreeEvaluator::floadEvaluator,                   // TR::floadi
   TR::TreeEvaluator::dloadEvaluator,                   // TR::dloadi
   TR::TreeEvaluator::aloadEvaluator,                   // TR::aloadi ibm@59591
   TR::TreeEvaluator::bloadEvaluator,                   // TR::bloadi
   TR::TreeEvaluator::sloadEvaluator,                   // TR::sloadi
   TR::TreeEvaluator::lloadEvaluator,                   // TR::lloadi
   TR::TreeEvaluator::istoreEvaluator,                  // TR::istore
   TR::TreeEvaluator::lstoreEvaluator,                  // TR::lstore
   TR::TreeEvaluator::fstoreEvaluator,                  // TR::fstore
   TR::TreeEvaluator::dstoreEvaluator,                  // TR::dstore
   TR::TreeEvaluator::astoreEvaluator,                  // TR::astore ibm@59591
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::wrtbar (J9)
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bstore
   TR::TreeEvaluator::sstoreEvaluator,                  // TR::sstore
   TR::TreeEvaluator::lstoreEvaluator,                  // TR::lstorei
   TR::TreeEvaluator::fstoreEvaluator,                  // TR::fstorei
   TR::TreeEvaluator::dstoreEvaluator,                  // TR::dstorei
   TR::TreeEvaluator::astoreEvaluator,                  // TR::astorei ibm@59591
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::wrtbari (J9)
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bstorei
   TR::TreeEvaluator::sstoreEvaluator,                  // TR::sstorei
   TR::TreeEvaluator::istoreEvaluator,                  // TR::istorei
   TR::TreeEvaluator::gotoEvaluator,                    // TR::Goto
   TR::TreeEvaluator::ireturnEvaluator,                 // TR::ireturn
   TR::TreeEvaluator::lreturnEvaluator,                 // TR::lreturn
   TR::TreeEvaluator::freturnEvaluator,                 // TR::freturn
   TR::TreeEvaluator::freturnEvaluator,                 // TR::dreturn
   TR::TreeEvaluator::ireturnEvaluator,                 // TR::areturn
   TR::TreeEvaluator::returnEvaluator,                  // TR::Return
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::asynccheck (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::athrow
   TR::TreeEvaluator::directCallEvaluator,              // TR::icall
   TR::TreeEvaluator::directCallEvaluator,              // TR::lcall
   TR::TreeEvaluator::directCallEvaluator,              // TR::fcall
   TR::TreeEvaluator::directCallEvaluator,              // TR::dcall
   TR::TreeEvaluator::directCallEvaluator,              // TR::acall
   TR::TreeEvaluator::directCallEvaluator,              // TR::call
   TR::TreeEvaluator::iaddEvaluator,                    // TR::iadd
   TR::TreeEvaluator::laddEvaluator,                    // TR::ladd
   TR::TreeEvaluator::faddEvaluator,                    // TR::fadd
   TR::TreeEvaluator::daddEvaluator,                    // TR::dadd
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::badd
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::sadd
   TR::TreeEvaluator::isubEvaluator,                    // TR::isub
   TR::TreeEvaluator::lsubEvaluator,                    // TR::lsub
   TR::TreeEvaluator::fsubEvaluator,                    // TR::fsub
   TR::TreeEvaluator::dsubEvaluator,                    // TR::dsub
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bsub
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ssub
   TR::TreeEvaluator::asubEvaluator,                    // TR::asub
   TR::TreeEvaluator::imulEvaluator,                    // TR::imul
   TR::TreeEvaluator::lmulEvaluator,                    // TR::lmul
   TR::TreeEvaluator::fmulEvaluator,                    // TR::fmul
   TR::TreeEvaluator::dmulEvaluator,                    // TR::dmul
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bmul
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::smul
   TR::TreeEvaluator::imulEvaluator,                    // TR::iumul
   TR::TreeEvaluator::idivEvaluator,                    // TR::idiv
   TR::TreeEvaluator::ldivEvaluator,                    // TR::ldiv
   TR::TreeEvaluator::fdivEvaluator,                    // TR::fdiv
   TR::TreeEvaluator::ddivEvaluator,                    // TR::ddiv
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bdiv
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::sdiv
   TR::TreeEvaluator::idivEvaluator,                    // TR::iudiv
   TR::TreeEvaluator::ldivEvaluator,                    // TR::ludiv
   TR::TreeEvaluator::iremEvaluator,                    // TR::irem
   TR::TreeEvaluator::lremEvaluator,                    // TR::lrem
   TR::TreeEvaluator::fremEvaluator,                    // TR::frem
   TR::TreeEvaluator::dremEvaluator,                    // TR::drem
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::brem
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::srem
   TR::TreeEvaluator::iremEvaluator,                    // TR::iurem
   TR::TreeEvaluator::inegEvaluator,                    // TR::ineg
   TR::TreeEvaluator::lnegEvaluator,                    // TR::lneg
   TR::TreeEvaluator::fnegEvaluator,                    // TR::fneg
   TR::TreeEvaluator::fnegEvaluator,                    // TR::dneg
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bneg
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::sneg
   TR::TreeEvaluator::iabsEvaluator,                    // TR::iabs
   TR::TreeEvaluator::labsEvaluator,                    // TR::labs
   TR::TreeEvaluator::fabsEvaluator,                    // TR::fabs
   TR::TreeEvaluator::dabsEvaluator,                    // TR::dabs
   TR::TreeEvaluator::ishlEvaluator,                    // TR::ishl
   TR::TreeEvaluator::lshlEvaluator,                    // TR::lshl
   TR::TreeEvaluator::ishlEvaluator,                    // TR::bshl
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::sshl
   TR::TreeEvaluator::ishrEvaluator,                    // TR::ishr
   TR::TreeEvaluator::lshrEvaluator,                    // TR::lshr
   TR::TreeEvaluator::ishrEvaluator,                    // TR::bshr
   TR::TreeEvaluator::ishrEvaluator,                    // TR::sshr
   TR::TreeEvaluator::iushrEvaluator,                   // TR::iushr
   TR::TreeEvaluator::lushrEvaluator,                   // TR::lushr
   TR::TreeEvaluator::iushrEvaluator,                   // TR::bushr
   TR::TreeEvaluator::iushrEvaluator,                   // TR::sushr
   TR::TreeEvaluator::irolEvaluator,                    // TR::irol
   TR::TreeEvaluator::lrolEvaluator,                    // TR::lrol
   TR::TreeEvaluator::iandEvaluator,                    // TR::iand
   TR::TreeEvaluator::landEvaluator,                    // TR::land
   TR::TreeEvaluator::iandEvaluator,                    // TR::band
   TR::TreeEvaluator::iandEvaluator,                    // TR::sand
   TR::TreeEvaluator::iorEvaluator,                     // TR::ior
   TR::TreeEvaluator::lorEvaluator,                     // TR::lor
   TR::TreeEvaluator::iorEvaluator,                     // TR::bor
   TR::TreeEvaluator::iorEvaluator,                     // TR::sor
   TR::TreeEvaluator::ixorEvaluator,                    // TR::ixor
   TR::TreeEvaluator::lxorEvaluator,                    // TR::lxor
   TR::TreeEvaluator::ixorEvaluator,                    // TR::bxor
   TR::TreeEvaluator::ixorEvaluator,                    // TR::sxor
   TR::TreeEvaluator::s2lEvaluator,                     // TR::i2l
   TR::TreeEvaluator::i2fEvaluator,                     // TR::i2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::i2d
   TR::TreeEvaluator::i2bEvaluator,                     // TR::i2b
   TR::TreeEvaluator::i2sEvaluator,                     // TR::i2s
   TR::TreeEvaluator::i2aEvaluator,                     // TR::i2a
   TR::TreeEvaluator::iu2lEvaluator,                    // TR::iu2l
   TR::TreeEvaluator::i2fEvaluator,                     // TR::iu2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::iu2d
   TR::TreeEvaluator::iu2aEvaluator,                    // TR::iu2a
   TR::TreeEvaluator::l2iEvaluator,                     // TR::l2i
   TR::TreeEvaluator::l2fEvaluator,                     // TR::l2f
   TR::TreeEvaluator::l2dEvaluator,                     // TR::l2d
   TR::TreeEvaluator::l2bEvaluator,                     // TR::l2b
   TR::TreeEvaluator::l2sEvaluator,                     // TR::l2s
   TR::TreeEvaluator::l2aEvaluator,                     // TR::l2a
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::lu2f
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::lu2d
   TR::TreeEvaluator::l2aEvaluator,                     // TR::lu2a
   TR::TreeEvaluator::d2iEvaluator,                     // TR::f2i
   TR::TreeEvaluator::d2lEvaluator,                     // TR::f2l
   TR::TreeEvaluator::f2dEvaluator,                     // TR::f2d
   TR::TreeEvaluator::d2iEvaluator,                     // TR::f2b
   TR::TreeEvaluator::d2iEvaluator,                     // TR::f2s
   TR::TreeEvaluator::d2iEvaluator,                     // TR::d2i
   TR::TreeEvaluator::d2lEvaluator,                     // TR::d2l
   TR::TreeEvaluator::d2fEvaluator,                     // TR::d2f
   TR::TreeEvaluator::d2iEvaluator,                     // TR::d2b
   TR::TreeEvaluator::d2iEvaluator,                     // TR::d2s
   TR::TreeEvaluator::b2iEvaluator,                     // TR::b2i
   TR::TreeEvaluator::b2lEvaluator,                     // TR::b2l
   TR::TreeEvaluator::i2dEvaluator,                     // TR::b2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::b2d
   TR::TreeEvaluator::b2iEvaluator,                     // TR::b2s
   TR::TreeEvaluator::b2aEvaluator,                     // TR::b2a
   TR::TreeEvaluator::bu2iEvaluator,                    // TR::bu2i
   TR::TreeEvaluator::bu2lEvaluator,                    // TR::bu2l
   TR::TreeEvaluator::i2dEvaluator,                     // TR::bu2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::bu2d
   TR::TreeEvaluator::bu2iEvaluator,                    // TR::bu2s
   TR::TreeEvaluator::bu2aEvaluator,                    // TR::bu2a
   TR::TreeEvaluator::s2iEvaluator,                     // TR::s2i
   TR::TreeEvaluator::s2lEvaluator,                     // TR::s2l
   TR::TreeEvaluator::i2dEvaluator,                     // TR::s2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::s2d
   TR::TreeEvaluator::i2bEvaluator,                     // TR::s2b
   TR::TreeEvaluator::s2aEvaluator,                     // TR::s2a
   TR::TreeEvaluator::su2iEvaluator,                    // TR::su2i
   TR::TreeEvaluator::su2lEvaluator,                    // TR::su2l
   TR::TreeEvaluator::i2dEvaluator,                     // TR::su2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::su2d
   TR::TreeEvaluator::su2aEvaluator,                    // TR::su2a
   TR::TreeEvaluator::a2iEvaluator,                     // TR::a2i
   TR::TreeEvaluator::a2lEvaluator,                     // TR::a2l
   TR::TreeEvaluator::a2bEvaluator,                     // TR::a2b
   TR::TreeEvaluator::a2sEvaluator,                     // TR::a2s
   TR::TreeEvaluator::icmpeqEvaluator,                  // TR::icmpeq
   TR::TreeEvaluator::icmpneEvaluator,                  // TR::icmpne
   TR::TreeEvaluator::icmpltEvaluator,                  // TR::icmplt
   TR::TreeEvaluator::icmpgeEvaluator,                  // TR::icmpge
   TR::TreeEvaluator::icmpgtEvaluator,                  // TR::icmpgt
   TR::TreeEvaluator::icmpleEvaluator,                  // TR::icmple
   TR::TreeEvaluator::icmpeqEvaluator,                  // TR::iucmpeq
   TR::TreeEvaluator::icmpneEvaluator,                  // TR::iucmpne
   TR::TreeEvaluator::iucmpltEvaluator,                 // TR::iucmplt
   TR::TreeEvaluator::iucmpgeEvaluator,                 // TR::iucmpge
   TR::TreeEvaluator::iucmpgtEvaluator,                 // TR::iucmpgt
   TR::TreeEvaluator::iucmpleEvaluator,                 // TR::iucmple
   TR::TreeEvaluator::lcmpeqEvaluator,                  // TR::lcmpeq
   TR::TreeEvaluator::lcmpneEvaluator,                  // TR::lcmpne
   TR::TreeEvaluator::lcmpltEvaluator,                  // TR::lcmplt
   TR::TreeEvaluator::lcmpgeEvaluator,                  // TR::lcmpge
   TR::TreeEvaluator::lcmpgtEvaluator,                  // TR::lcmpgt
   TR::TreeEvaluator::lcmpleEvaluator,                  // TR::lcmple
   TR::TreeEvaluator::lcmpeqEvaluator,                  // TR::lucmpeq
   TR::TreeEvaluator::lcmpneEvaluator,                  // TR::lucmpne
   TR::TreeEvaluator::lucmpltEvaluator,                 // TR::lucmplt
   TR::TreeEvaluator::lucmpgeEvaluator,                 // TR::lucmpge
   TR::TreeEvaluator::lucmpgtEvaluator,                 // TR::lucmpgt
   TR::TreeEvaluator::lucmpleEvaluator,                 // TR::lucmple
   TR::TreeEvaluator::dcmpeqEvaluator,                  // TR::fcmpeq
   TR::TreeEvaluator::dcmpneEvaluator,                  // TR::fcmpne
   TR::TreeEvaluator::dcmpltEvaluator,                  // TR::fcmplt
   TR::TreeEvaluator::dcmpgeEvaluator,                  // TR::fcmpge
   TR::TreeEvaluator::dcmpgtEvaluator,                  // TR::fcmpgt
   TR::TreeEvaluator::dcmpleEvaluator,                  // TR::fcmple
   TR::TreeEvaluator::dcmpequEvaluator,                 // TR::fcmpequ
   TR::TreeEvaluator::dcmpneEvaluator,                  // TR::fcmpneu
   TR::TreeEvaluator::dcmpltuEvaluator,                 // TR::fcmpltu
   TR::TreeEvaluator::dcmpgeuEvaluator,                 // TR::fcmpgeu
   TR::TreeEvaluator::dcmpgtuEvaluator,                 // TR::fcmpgtu
   TR::TreeEvaluator::dcmpleuEvaluator,                 // TR::fcmpleu
   TR::TreeEvaluator::dcmpeqEvaluator,                  // TR::dcmpeq
   TR::TreeEvaluator::dcmpneEvaluator,                  // TR::dcmpne
   TR::TreeEvaluator::dcmpltEvaluator,                  // TR::dcmplt
   TR::TreeEvaluator::dcmpgeEvaluator,                  // TR::dcmpge
   TR::TreeEvaluator::dcmpgtEvaluator,                  // TR::dcmpgt
   TR::TreeEvaluator::dcmpleEvaluator,                  // TR::dcmple
   TR::TreeEvaluator::dcmpequEvaluator,                 // TR::dcmpequ
   TR::TreeEvaluator::dcmpneEvaluator,                  // TR::dcmpneu
   TR::TreeEvaluator::dcmpltuEvaluator,                 // TR::dcmpltu
   TR::TreeEvaluator::dcmpgeuEvaluator,                 // TR::dcmpgeu
   TR::TreeEvaluator::dcmpgtuEvaluator,                 // TR::dcmpgtu
   TR::TreeEvaluator::dcmpleuEvaluator,                 // TR::dcmpleu
   TR::TreeEvaluator::acmpeqEvaluator,                  // TR::acmpeq
   TR::TreeEvaluator::acmpneEvaluator,                  // TR::acmpne
   TR::TreeEvaluator::acmpltEvaluator,                  // TR::acmplt
   TR::TreeEvaluator::acmpgeEvaluator,                  // TR::acmpge
   TR::TreeEvaluator::acmpgtEvaluator,                  // TR::acmpgt
   TR::TreeEvaluator::acmpleEvaluator,                  // TR::acmple
   TR::TreeEvaluator::icmpeqEvaluator,                  // TR::bcmpeq
   TR::TreeEvaluator::icmpneEvaluator,                  // TR::bcmpne
   TR::TreeEvaluator::icmpltEvaluator,                  // TR::bcmplt
   TR::TreeEvaluator::icmpgeEvaluator,                  // TR::bcmpge
   TR::TreeEvaluator::icmpgtEvaluator,                  // TR::bcmpgt
   TR::TreeEvaluator::icmpleEvaluator,                  // TR::bcmple
   TR::TreeEvaluator::icmpeqEvaluator,                  // TR::bucmpeq
   TR::TreeEvaluator::icmpneEvaluator,                  // TR::bucmpne
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmplt
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmpge
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmpgt
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmple
   TR::TreeEvaluator::icmpeqEvaluator,                  // TR::scmpeq
   TR::TreeEvaluator::icmpneEvaluator,                  // TR::scmpne
   TR::TreeEvaluator::icmpltEvaluator,                  // TR::scmplt
   TR::TreeEvaluator::icmpgeEvaluator,                  // TR::scmpge
   TR::TreeEvaluator::icmpgtEvaluator,                  // TR::scmpgt
   TR::TreeEvaluator::icmpleEvaluator,                  // TR::scmple
   TR::TreeEvaluator::icmpeqEvaluator,                  // TR::sucmpeq
   TR::TreeEvaluator::icmpneEvaluator,                  // TR::sucmpne
   TR::TreeEvaluator::iucmpltEvaluator,                 // TR::sucmplt
   TR::TreeEvaluator::iucmpgeEvaluator,                 // TR::sucmpge
   TR::TreeEvaluator::iucmpgtEvaluator,                 // TR::sucmpgt
   TR::TreeEvaluator::iucmpleEvaluator,                 // TR::sucmple
   TR::TreeEvaluator::lcmpEvaluator,                    // TR::lcmp
   TR::TreeEvaluator::dcmplEvaluator,                   // TR::fcmpl
   TR::TreeEvaluator::dcmpgEvaluator,                   // TR::fcmpg
   TR::TreeEvaluator::dcmplEvaluator,                   // TR::dcmpl
   TR::TreeEvaluator::dcmpgEvaluator,                   // TR::dcmpg
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ificmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ificmpne
   TR::TreeEvaluator::ificmpltEvaluator,                // TR::ificmplt
   TR::TreeEvaluator::ificmpgeEvaluator,                // TR::ificmpge
   TR::TreeEvaluator::ificmpgtEvaluator,                // TR::ificmpgt
   TR::TreeEvaluator::ificmpleEvaluator,                // TR::ificmple
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifiucmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifiucmpne
   TR::TreeEvaluator::ifiucmpltEvaluator,               // TR::ifiucmplt
   TR::TreeEvaluator::ifiucmpgeEvaluator,               // TR::ifiucmpge
   TR::TreeEvaluator::ifiucmpgtEvaluator,               // TR::ifiucmpgt
   TR::TreeEvaluator::ifiucmpleEvaluator,               // TR::ifiucmple
   TR::TreeEvaluator::iflcmpeqEvaluator,                // TR::iflcmpeq
   TR::TreeEvaluator::iflcmpeqEvaluator,                // TR::iflcmpne
   TR::TreeEvaluator::iflcmpltEvaluator,                // TR::iflcmplt
   TR::TreeEvaluator::iflcmpgeEvaluator,                // TR::iflcmpge
   TR::TreeEvaluator::iflcmpgtEvaluator,                // TR::iflcmpgt
   TR::TreeEvaluator::iflcmpleEvaluator,                // TR::iflcmple
   TR::TreeEvaluator::iflcmpeqEvaluator,                // TR::iflucmpeq
   TR::TreeEvaluator::iflcmpeqEvaluator,                // TR::iflucmpne
   TR::TreeEvaluator::iflucmpltEvaluator,               // TR::iflucmplt
   TR::TreeEvaluator::iflucmpgeEvaluator,               // TR::iflucmpge
   TR::TreeEvaluator::iflucmpgtEvaluator,               // TR::iflucmpgt
   TR::TreeEvaluator::iflucmpleEvaluator,               // TR::iflucmple
   TR::TreeEvaluator::ifdcmpeqEvaluator,                // TR::iffcmpeq
   TR::TreeEvaluator::ifdcmpneEvaluator,                // TR::iffcmpne
   TR::TreeEvaluator::ifdcmpltEvaluator,                // TR::iffcmplt
   TR::TreeEvaluator::ifdcmpgeEvaluator,                // TR::iffcmpge
   TR::TreeEvaluator::ifdcmpgtEvaluator,                // TR::iffcmpgt
   TR::TreeEvaluator::ifdcmpleEvaluator,                // TR::iffcmple
   TR::TreeEvaluator::ifdcmpequEvaluator,               // TR::iffcmpequ
   TR::TreeEvaluator::ifdcmpneEvaluator,                // TR::iffcmpneu
   TR::TreeEvaluator::ifdcmpltuEvaluator,               // TR::iffcmpltu
   TR::TreeEvaluator::ifdcmpgeuEvaluator,               // TR::iffcmpgeu
   TR::TreeEvaluator::ifdcmpgtuEvaluator,               // TR::iffcmpgtu
   TR::TreeEvaluator::ifdcmpleuEvaluator,               // TR::iffcmpleu
   TR::TreeEvaluator::ifdcmpeqEvaluator,                // TR::ifdcmpeq
   TR::TreeEvaluator::ifdcmpneEvaluator,                // TR::ifdcmpne
   TR::TreeEvaluator::ifdcmpltEvaluator,                // TR::ifdcmplt
   TR::TreeEvaluator::ifdcmpgeEvaluator,                // TR::ifdcmpge
   TR::TreeEvaluator::ifdcmpgtEvaluator,                // TR::ifdcmpgt
   TR::TreeEvaluator::ifdcmpleEvaluator,                // TR::ifdcmple
   TR::TreeEvaluator::ifdcmpequEvaluator,               // TR::ifdcmpequ
   TR::TreeEvaluator::ifdcmpneEvaluator,                // TR::ifdcmpneu
   TR::TreeEvaluator::ifdcmpltuEvaluator,               // TR::ifdcmpltu
   TR::TreeEvaluator::ifdcmpgeuEvaluator,               // TR::ifdcmpgeu
   TR::TreeEvaluator::ifdcmpgtuEvaluator,               // TR::ifdcmpgtu
   TR::TreeEvaluator::ifdcmpleuEvaluator,               // TR::ifdcmpleu
   TR::TreeEvaluator::ifacmpeqEvaluator,                // TR::ifacmpeq
   TR::TreeEvaluator::ifacmpneEvaluator,                // TR::ifacmpne
   TR::TreeEvaluator::ifacmpltEvaluator,                // TR::ifacmplt
   TR::TreeEvaluator::ifacmpgeEvaluator,                // TR::ifacmpge
   TR::TreeEvaluator::ifacmpgtEvaluator,                // TR::ifacmpgt
   TR::TreeEvaluator::ifacmpleEvaluator,                // TR::ifacmple
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifbcmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifbcmpne
   TR::TreeEvaluator::ificmpltEvaluator,                // TR::ifbcmplt
   TR::TreeEvaluator::ificmpgeEvaluator,                // TR::ifbcmpge
   TR::TreeEvaluator::ificmpgtEvaluator,                // TR::ifbcmpgt
   TR::TreeEvaluator::ificmpleEvaluator,                // TR::ifbcmple
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifbucmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifbucmpne
   TR::TreeEvaluator::ifiucmpltEvaluator,               // TR::ifbucmplt
   TR::TreeEvaluator::ifiucmpgeEvaluator,               // TR::ifbucmpge
   TR::TreeEvaluator::ifiucmpgtEvaluator,               // TR::ifbucmpgt
   TR::TreeEvaluator::ifiucmpleEvaluator,               // TR::ifbucmple
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifscmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifscmpne
   TR::TreeEvaluator::ificmpltEvaluator,                // TR::ifscmplt
   TR::TreeEvaluator::ificmpgeEvaluator,                // TR::ifscmpge
   TR::TreeEvaluator::ificmpgtEvaluator,                // TR::ifscmpgt
   TR::TreeEvaluator::ificmpleEvaluator,                // TR::ifscmple
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifsucmpeq
   TR::TreeEvaluator::ificmpeqEvaluator,                // TR::ifsucmpne
   TR::TreeEvaluator::ifiucmpltEvaluator,               // TR::ifsucmplt
   TR::TreeEvaluator::ifiucmpgeEvaluator,               // TR::ifsucmpge
   TR::TreeEvaluator::ifiucmpgtEvaluator,               // TR::ifsucmpgt
   TR::TreeEvaluator::ifiucmpleEvaluator,               // TR::ifsucmple
   TR::TreeEvaluator::loadaddrEvaluator,                // TR::loadaddr
   TR::TreeEvaluator::ZEROCHKEvaluator,                 // TR::ZEROCHK (J9, but general implementation)
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::callIf
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::iRegLoad
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::aRegLoad
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::lRegLoad
   TR::TreeEvaluator::fRegLoadEvaluator,                // TR::fRegLoad
   TR::TreeEvaluator::dRegLoadEvaluator,                // TR::dRegLoad
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::sRegLoad
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::bRegLoad
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::iRegStore
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::aRegStore
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::lRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::fRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::dRegStore
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::sRegStore
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::bRegStore
   TR::TreeEvaluator::GlRegDepsEvaluator,               // TR::GlRegDeps
   TR::TreeEvaluator::iternaryEvaluator,                // TR::iternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::lternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::bternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::sternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::aternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::fternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::dternary
   TR::TreeEvaluator::treetopEvaluator,                 // TR::treetop
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::MethodEnterHook (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::MethodExitHook (J9)
   TR::TreeEvaluator::passThroughEvaluator,             // TR::PassThrough
   TR::TreeEvaluator::compressedRefsEvaluator,          // TR::compressedRefs
   TR::TreeEvaluator::BBStartEvaluator,                 // TR::BBStart
   TR::TreeEvaluator::BBEndEvaluator,                   // TR::BBEnd

   TR::TreeEvaluator::viremEvaluator,                   // TR::virem
   TR::TreeEvaluator::viminEvaluator,                   // TR::vimin
   TR::TreeEvaluator::vimaxEvaluator,                   // TR::vimax
   TR::TreeEvaluator::vigetelemEvaluator,               // TR::vigetelem
   TR::TreeEvaluator::visetelemEvaluator,               // TR::visetelem
   TR::TreeEvaluator::vimergeEvaluator,                 // TR::vimergel
   TR::TreeEvaluator::vimergeEvaluator,                 // TR::vimergeh
   TR::TreeEvaluator::vicmpeqEvaluator,                 // TR::vicmpeq
   TR::TreeEvaluator::vicmpgtEvaluator,                 // TR::vicmpgt
   TR::TreeEvaluator::vicmpgeEvaluator,                 // TR::vicmpge
   TR::TreeEvaluator::vicmpltEvaluator,                 // TR::vicmplt
   TR::TreeEvaluator::vicmpleEvaluator,                 // TR::vicmple
   TR::TreeEvaluator::vicmpalleqEvaluator,              // TR::vicmpalleq
   TR::TreeEvaluator::vicmpallneEvaluator,              // TR::vicmpallne
   TR::TreeEvaluator::vicmpallgtEvaluator,              // TR::vicmpallgt
   TR::TreeEvaluator::vicmpallgeEvaluator,              // TR::vicmpallge
   TR::TreeEvaluator::vicmpallltEvaluator,              // TR::vicmpalllt
   TR::TreeEvaluator::vicmpallleEvaluator,              // TR::vicmpallle
   TR::TreeEvaluator::vicmpanyeqEvaluator,              // TR::vicmpanyeq
   TR::TreeEvaluator::vicmpanyneEvaluator,              // TR::vicmpanyne
   TR::TreeEvaluator::vicmpanygtEvaluator,              // TR::vicmpanygt
   TR::TreeEvaluator::vicmpanygeEvaluator,              // TR::vicmpanyge
   TR::TreeEvaluator::vicmpanyltEvaluator,              // TR::vicmpanylt
   TR::TreeEvaluator::vicmpanyleEvaluator,              // TR::vicmpanyle
   TR::TreeEvaluator::vnotEvaluator,                    // TR::vnot
   TR::TreeEvaluator::vselectEvaluator,                 // TR::vselect
   TR::TreeEvaluator::vpermEvaluator,                   // TR::vperm

   TR::TreeEvaluator::vsplatsEvaluator,                 // TR::vsplats
   TR::TreeEvaluator::vdmergeEvaluator,                 // TR::vdmergel
   TR::TreeEvaluator::vdmergeEvaluator,                 // TR::vdmergeh
   TR::TreeEvaluator::vdsetelemEvaluator,               // TR::vdsetelem
   TR::TreeEvaluator::vdgetelemEvaluator,               // TR::vdgetelem
   TR::TreeEvaluator::vdselEvaluator,                   // TR::vdsel
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::vdrem
   TR::TreeEvaluator::vdmaddEvaluator,                  // TR::vdmadd
   TR::TreeEvaluator::vdnmsubEvaluator,                 // TR::vdnmsub
   TR::TreeEvaluator::vdmsubEvaluator,                  // TR::vdmsub
   TR::TreeEvaluator::vdmaxEvaluator,                   // TR::vdmax
   TR::TreeEvaluator::vdminEvaluator,                   // TR::vdmin
   TR::TreeEvaluator::vdcmpeqEvaluator,                 // TR::vdcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::vdcmpne
   TR::TreeEvaluator::vdcmpgtEvaluator,                 // TR::vdcmpgt
   TR::TreeEvaluator::vdcmpgeEvaluator,                 // TR::vdcmpge
   TR::TreeEvaluator::vdcmpltEvaluator,                 // TR::vdcmplt
   TR::TreeEvaluator::vdcmpleEvaluator,                 // TR::vdcmple
   TR::TreeEvaluator::vdcmpalleqEvaluator,              // TR::vdcmpalleq
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::vdcmpallne
   TR::TreeEvaluator::vdcmpallgtEvaluator,              // TR::vdcmpallgt
   TR::TreeEvaluator::vdcmpallgeEvaluator,              // TR::vdcmpallge
   TR::TreeEvaluator::vdcmpallltEvaluator,              // TR::vdcmpalllt
   TR::TreeEvaluator::vdcmpallleEvaluator,              // TR::vdcmpallle
   TR::TreeEvaluator::vdcmpanyeqEvaluator,              // TR::vdcmpanyeq
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::vdcmpanyne
   TR::TreeEvaluator::vdcmpanygtEvaluator,              // TR::vdcmpanygt
   TR::TreeEvaluator::vdcmpanygeEvaluator,              // TR::vdcmpanyge
   TR::TreeEvaluator::vdcmpanyltEvaluator,              // TR::vdcmpanylt
   TR::TreeEvaluator::vdcmpanyleEvaluator,              // TR::vdcmpanyle
   TR::TreeEvaluator::vdsqrtEvaluator,                  // TR::vdsqrt
   TR::TreeEvaluator::vdlogEvaluator,                   // TR::vdlog

   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vinc
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdec
   TR::TreeEvaluator::vnegEvaluator,                    // TR::vneg
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcom
   TR::TreeEvaluator::vaddEvaluator,                    // TR::vadd
   TR::TreeEvaluator::vsubEvaluator,                    // TR::vsub
   TR::TreeEvaluator::vmulEvaluator,                    // TR::vmul
   TR::TreeEvaluator::vdivEvaluator,                    // TR::vdiv
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vrem
   TR::TreeEvaluator::vandEvaluator,                    // TR::vand
   TR::TreeEvaluator::vorEvaluator,                    // TR::vor
   TR::TreeEvaluator::vxorEvaluator,                    // TR::vxor
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
   TR::TreeEvaluator::vloadEvaluator,                   // TR::vload
   TR::TreeEvaluator::vloadEvaluator,                   // TR::vloadi
   TR::TreeEvaluator::vstoreEvaluator,                  // TR::vstore
   TR::TreeEvaluator::vstoreEvaluator,                  // TR::vstorei
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vrand
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vreturn
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcall
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcalli
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vternary
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::v2v
   TR::TreeEvaluator::vl2vdEvaluator,                    // TR::vl2vd
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vconst
   TR::TreeEvaluator::getvelemEvaluator,                // TR::getvelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vsetelem

   TR::TreeEvaluator::vRegLoadEvaluator,                // TR::vbRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,                // TR::vsRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,                // TR::viRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,                // TR::vlRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,                // TR::vfRegLoad
   TR::TreeEvaluator::vRegLoadEvaluator,                // TR::vdRegLoad
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::vbRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::vsRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::viRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::vlRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::vfRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::vdRegStore

/*
 *END OF OPCODES REQUIRED BY OMR
 */
   TR::TreeEvaluator::iconstEvaluator,                  // TR::iuconst
   TR::TreeEvaluator::lconstEvaluator,                  // TR::luconst
   TR::TreeEvaluator::iconstEvaluator,                  // TR::buconst
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iuload
   TR::TreeEvaluator::lloadEvaluator,                   // TR::luload
   TR::TreeEvaluator::buloadEvaluator,                  // TR::buload
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iuloadi
   TR::TreeEvaluator::lloadEvaluator,                   // TR::luloadi
   TR::TreeEvaluator::buloadEvaluator,                  // TR::buloadi
   TR::TreeEvaluator::istoreEvaluator,                  // TR::iustore
   TR::TreeEvaluator::lstoreEvaluator,                  // TR::lustore
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bustore
   TR::TreeEvaluator::istoreEvaluator,                  // TR::iustorei
   TR::TreeEvaluator::lstoreEvaluator,                  // TR::lustorei
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bustorei
   TR::TreeEvaluator::ireturnEvaluator,                 // TR::iureturn
   TR::TreeEvaluator::lreturnEvaluator,                 // TR::lureturn
   TR::TreeEvaluator::directCallEvaluator,              // TR::iucall
   TR::TreeEvaluator::directCallEvaluator,              // TR::lucall
   TR::TreeEvaluator::iaddEvaluator,                    // TR::iuadd
   TR::TreeEvaluator::laddEvaluator,                    // TR::luadd
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::buadd
   TR::TreeEvaluator::isubEvaluator,                    // TR::iusub
   TR::TreeEvaluator::lsubEvaluator,                    // TR::lusub
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::busub
   TR::TreeEvaluator::inegEvaluator,                    // TR::iuneg
   TR::TreeEvaluator::lnegEvaluator,                    // TR::luneg
   TR::TreeEvaluator::ishlEvaluator,                    // TR::iushl
   TR::TreeEvaluator::lshlEvaluator,                    // TR::lushl
   TR::TreeEvaluator::d2iuEvaluator,                    // TR::f2iu
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::f2lu
   TR::TreeEvaluator::d2iuEvaluator,                    // TR::f2bu
   TR::TreeEvaluator::d2iEvaluator,                     // TR::f2c
   TR::TreeEvaluator::d2iuEvaluator,                    // TR::d2iu
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::d2lu
   TR::TreeEvaluator::d2iuEvaluator,                    // TR::d2bu
   TR::TreeEvaluator::d2iEvaluator,                     // TR::d2c
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::iuRegLoad
   TR::TreeEvaluator::gprRegLoadEvaluator,              // TR::luRegLoad
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::iuRegStore
   TR::TreeEvaluator::gprRegStoreEvaluator,             // TR::luRegStore
   TR::TreeEvaluator::iternaryEvaluator,                // TR::iuternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::luternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::buternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::suternary
   TR::TreeEvaluator::iconstEvaluator,                  // TR::cconst
   TR::TreeEvaluator::cloadEvaluator,                   // TR::cload
   TR::TreeEvaluator::cloadEvaluator,                   // TR::cloadi
   TR::TreeEvaluator::cstoreEvaluator,                  // TR::cstore
   TR::TreeEvaluator::cstoreEvaluator,                  // TR::cstorei
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::monent (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::monexit (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::monexitfence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                  // TR::tstart (J9)
   TR::TreeEvaluator::badILOpEvaluator,                 // TR::tfinish (J9)
   TR::TreeEvaluator::badILOpEvaluator,                  // TR::tabort (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::instanceof (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::checkcast (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::checkcastAndNULLCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::New (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::newarray (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::anewarray (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::variableNew (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::variableNewArray (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::multianewarray (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::arraylength (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::contigarraylength (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::discontigarraylength (J9)
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::icalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::iucalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::lcalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::lucalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::fcalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::dcalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::acalli
   TR::TreeEvaluator::indirectCallEvaluator,            // TR::calli
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::fence
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::luaddh
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::cadd
   TR::TreeEvaluator::iaddEvaluator,                    // TR::aiadd
   TR::TreeEvaluator::iaddEvaluator,                    // TR::aiuadd
   TR::TreeEvaluator::laddEvaluator,                    // TR::aladd
   TR::TreeEvaluator::laddEvaluator,                    // TR::aluadd
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lusubh
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::csub
   TR::TreeEvaluator::imulhEvaluator,                   // TR::imulh
   TR::TreeEvaluator::imulhEvaluator,                   // TR::iumulh
   TR::TreeEvaluator::lmulhEvaluator,                   // TR::lmulh
   TR::TreeEvaluator::lmulhEvaluator,                   // TR::lumulh
   TR::TreeEvaluator::ibits2fEvaluator,                 // TR::ibits2f
   TR::TreeEvaluator::fbits2iEvaluator,                 // TR::fbits2i
   TR::TreeEvaluator::lbits2dEvaluator,                 // TR::lbits2d
   TR::TreeEvaluator::dbits2lEvaluator,                 // TR::dbits2l
   TR::TreeEvaluator::lookupEvaluator,                  // TR::lookup
   TR::TreeEvaluator::NOPEvaluator,                     // TR::trtLookup
   TR::TreeEvaluator::NOPEvaluator,                     // TR::Case
   TR::TreeEvaluator::tableEvaluator,                   // TR::table
   TR::TreeEvaluator::exceptionRangeFenceEvaluator,     // TR::exceptionRangeFence
   TR::TreeEvaluator::exceptionRangeFenceEvaluator,     // TR::dbgFence (J9)
   TR::TreeEvaluator::NULLCHKEvaluator,                 // TR::NULLCHK
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ResolveCHK (J9)
   TR::TreeEvaluator::resolveAndNULLCHKEvaluator,       // TR::ResolveAndNULLCHK
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::DIVCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::OverflowCHK
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::UnsignedOverflowCHK
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::BNDCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ArrayCopyBNDCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::BNDCHKwithSpineCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::SpineCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ArrayStoreCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ArrayCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::Ret
   TR::TreeEvaluator::arraycopyEvaluator,               // TR::arraycopy
   TR::TreeEvaluator::arraysetEvaluator,                // TR::arrayset
   TR::TreeEvaluator::arraytranslateEvaluator,          // TR::arraytranslate
   TR::TreeEvaluator::arraytranslateAndTestEvaluator,   // TR::arraytranslateAndTest
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::long2String
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bitOpMem
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bitOpMemND
   TR::TreeEvaluator::arraycmpEvaluator,                // TR::arraycmp
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::arraycmpWithPad
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::allocationFence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::loadFence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::storeFence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::fullFence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::MergeNew (J9)
   TR::TreeEvaluator::computeCCEvaluator,               // TR::computeCC
   TR::TreeEvaluator::butestEvaluator,                  // TR::butest
   TR::TreeEvaluator::sutestEvaluator,                  // TR::sutest
   TR::TreeEvaluator::iucmpEvaluator,                   // TR::bucmp
   TR::TreeEvaluator::icmpEvaluator,                    // TR::bcmp
   TR::TreeEvaluator::iucmpEvaluator,                   // TR::sucmp
   TR::TreeEvaluator::icmpEvaluator,                    // TR::scmp
   TR::TreeEvaluator::iucmpEvaluator,                   // TR::iucmp
   TR::TreeEvaluator::icmpEvaluator,                    // TR::icmp
   TR::TreeEvaluator::iucmpEvaluator,                   // TR::lucmp
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmpo
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmpno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmpo
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmpno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmnno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmnno
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iuaddc
   TR::TreeEvaluator::laddEvaluator,                    // TR::luaddc
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iusubb
   TR::TreeEvaluator::lsubEvaluator,                    // TR::lusubb
   TR::TreeEvaluator::cmpsetEvaluator,                  // TR::icmpset
   TR::TreeEvaluator::cmpsetEvaluator,                  // TR::lcmpset
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bztestnset
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ibatomicor
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::isatomicor
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iiatomicor
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ilatomicor

   TR::TreeEvaluator::badILOpEvaluator,

   TR::TreeEvaluator::badILOpEvaluator,                    // TR::branch
   TR::TreeEvaluator::igotoEvaluator,                   // TR::igoto
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bexp
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::buexp
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::sexp
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::cexp
   TR::TreeEvaluator::iexpEvaluator,                    // TR::iexp
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::iuexp
   TR::TreeEvaluator::lexpEvaluator,                    // TR::lexp
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::luexp
   TR::TreeEvaluator::dexpEvaluator,                    // TR::fexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fuexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::duexp
   TR::TreeEvaluator::ixfrsEvaluator,                   // TR::ixfrs
   TR::TreeEvaluator::lxfrsEvaluator,                   // TR::lxfrs
   TR::TreeEvaluator::dxfrsEvaluator,                   // TR::fxfrs
   TR::TreeEvaluator::dxfrsEvaluator,                   // TR::dxfrs
   TR::TreeEvaluator::dintEvaluator,                    // TR::fint
   TR::TreeEvaluator::dintEvaluator,                    // TR::dint
   TR::TreeEvaluator::dnintEvaluator,                   // TR::fnint
   TR::TreeEvaluator::dnintEvaluator,                   // TR::dnint
   TR::TreeEvaluator::fsqrtEvaluator,                   // TR::fsqrt
   TR::TreeEvaluator::dsqrtEvaluator,                   // TR::dsqrt
   TR::TreeEvaluator::getstackEvaluator,                // TR::getstack
   TR::TreeEvaluator::deallocaEvaluator,                // TR::dealloca
   TR::TreeEvaluator::ishflEvaluator,                   // TR::ishfl
   TR::TreeEvaluator::lshflEvaluator,                   // TR::lshfl
   TR::TreeEvaluator::ishflEvaluator,                   // TR::iushfl
   TR::TreeEvaluator::lshflEvaluator,                   // TR::lushfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bshfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::sshfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bushfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::sushfl
   TR::TreeEvaluator::idozEvaluator,                    // TR::idoz
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dcos
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dsin
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dtan
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dcosh
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dsinh
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dtanh
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dacos
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dasin
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::datan
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::datan2
   TR::TreeEvaluator::libmFuncEvaluator,                // TR::dlog
   TR::TreeEvaluator::muloverEvaluator,                 // TR::imulover
   TR::TreeEvaluator::dfloorEvaluator,                  // TR::dfloor
   TR::TreeEvaluator::dfloorEvaluator,                  // TR::ffloor
   TR::TreeEvaluator::dfloorEvaluator,                  // TR::dceil
   TR::TreeEvaluator::dfloorEvaluator,                  // TR::fceil
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ibranch
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::mbranch
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::getpm
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::setpm
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::loadAutoOffset
   TR::TreeEvaluator::maxEvaluator,                     // TR::imax
   TR::TreeEvaluator::maxEvaluator,                     // TR::iumax
   TR::TreeEvaluator::maxEvaluator,                     // TR::lmax
   TR::TreeEvaluator::maxEvaluator,                     // TR::lumax
   TR::TreeEvaluator::maxEvaluator,                     // TR::fmax
   TR::TreeEvaluator::maxEvaluator,                     // TR::dmax
   TR::TreeEvaluator::minEvaluator,                     // TR::imin
   TR::TreeEvaluator::minEvaluator,                     // TR::iumin
   TR::TreeEvaluator::minEvaluator,                     // TR::lmin
   TR::TreeEvaluator::minEvaluator,                     // TR::lumin
   TR::TreeEvaluator::minEvaluator,                     // TR::fmin
   TR::TreeEvaluator::minEvaluator,                     // TR::dmin
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::trt
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::trtSimple
   TR::TreeEvaluator::integerHighestOneBit,             // TR::ihbit (J9)
   TR::TreeEvaluator::integerLowestOneBit,              // TR::ilbit (J9)
   TR::TreeEvaluator::integerNumberOfLeadingZeros,      // TR::inolz (J9)
   TR::TreeEvaluator::integerNumberOfTrailingZeros,     // TR::inotz (J9)
   TR::TreeEvaluator::integerBitCount,                  // TR::ipopcnt (J9)
   TR::TreeEvaluator::longHighestOneBit,                // TR::lhbit (J9)
   TR::TreeEvaluator::longLowestOneBit,                 // TR::llbit (J9)
   TR::TreeEvaluator::longNumberOfLeadingZeros,         // TR::lnolz (J9)
   TR::TreeEvaluator::longNumberOfTrailingZeros,        // TR::lnotz (J9)
   TR::TreeEvaluator::longBitCount,                     // TR::lpopcnt (J9)
   TR::TreeEvaluator::ibyteswapEvaluator,               // TR::ibyteswap
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::bbitpermute
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::sbitpermute
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::ibitpermute
   TR::TreeEvaluator::unImpOpEvaluator,                 // TR::lbitpermute
   TR::TreeEvaluator::PrefetchEvaluator,                // TR::Prefetch
