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
   TR::TreeEvaluator::aconstEvaluator,                // TR::aconst
   TR::TreeEvaluator::iconstEvaluator,                  // TR::iconst
   TR::TreeEvaluator::lconstEvaluator,                // TR::lconst
   TR::TreeEvaluator::fconstEvaluator,                  // TR::fconst
   TR::TreeEvaluator::dconstEvaluator,                  // TR::dconst
   TR::TreeEvaluator::bconstEvaluator,                  // TR::bconst
   TR::TreeEvaluator::sconstEvaluator,                  // TR::sconst
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iload
   TR::TreeEvaluator::floadEvaluator,                   // TR::fload
   TR::TreeEvaluator::dloadEvaluator,                   // TR::dload
   TR::TreeEvaluator::aloadEvaluator,                 // TR::aload
   TR::TreeEvaluator::bloadEvaluator,                   // TR::bload
   TR::TreeEvaluator::sloadEvaluator,                   // TR::sload
   TR::TreeEvaluator::lloadEvaluator,                 // TR::lload
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iloadi
   TR::TreeEvaluator::floadEvaluator,                   // TR::floadi
   TR::TreeEvaluator::dloadEvaluator,                   // TR::dloadi
   TR::TreeEvaluator::aloadEvaluator,                 // TR::aloadi
   TR::TreeEvaluator::bloadEvaluator,                   // TR::bloadi
   TR::TreeEvaluator::sloadEvaluator,                   // TR::sloadi
   TR::TreeEvaluator::lloadEvaluator,                 // TR::lloadi
   TR::TreeEvaluator::istoreEvaluator,                  // TR::istore
   TR::TreeEvaluator::lstoreEvaluator,                // TR::lstore
   TR::TreeEvaluator::floatingPointStoreEvaluator,      // TR::fstore
   TR::TreeEvaluator::floatingPointStoreEvaluator,      // TR::dstore
   TR::TreeEvaluator::lstoreEvaluator,                // TR::astore
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::wrtbar (J9)
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bstore
   TR::TreeEvaluator::sstoreEvaluator,                  // TR::sstore
   TR::TreeEvaluator::lstoreEvaluator,                // TR::lstorei
   TR::TreeEvaluator::floatingPointStoreEvaluator,      // TR::fstorei
   TR::TreeEvaluator::floatingPointStoreEvaluator,      // TR::dstorei
   TR::TreeEvaluator::lstoreEvaluator,                // TR::astorei
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::wrtbari (J9)
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bstorei
   TR::TreeEvaluator::sstoreEvaluator,                  // TR::sstorei
   TR::TreeEvaluator::istoreEvaluator,                  // TR::istorei
   TR::TreeEvaluator::gotoEvaluator,                    // TR::Goto
   TR::TreeEvaluator::integerReturnEvaluator,           // TR::ireturn
   TR::TreeEvaluator::integerReturnEvaluator,           // TR::lreturn
   TR::TreeEvaluator::fpReturnEvaluator,                // TR::freturn
   TR::TreeEvaluator::fpReturnEvaluator,                // TR::dreturn
   TR::TreeEvaluator::integerReturnEvaluator,           // TR::areturn
   TR::TreeEvaluator::returnEvaluator,                  // TR::Return
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::asynccheck (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::athrow
   TR::TreeEvaluator::directCallEvaluator,              // TR::icall
   TR::TreeEvaluator::directCallEvaluator,              // TR::lcall
   TR::TreeEvaluator::directCallEvaluator,              // TR::fcall
   TR::TreeEvaluator::directCallEvaluator,              // TR::dcall
   TR::TreeEvaluator::directCallEvaluator,              // TR::acall
   TR::TreeEvaluator::directCallEvaluator,              // TR::call
   TR::TreeEvaluator::integerAddEvaluator,              // TR::iadd
   TR::TreeEvaluator::integerAddEvaluator,              // TR::ladd
   TR::TreeEvaluator::faddEvaluator,                    // TR::fadd
   TR::TreeEvaluator::daddEvaluator,                    // TR::dadd
   TR::TreeEvaluator::baddEvaluator,                    // TR::badd
   TR::TreeEvaluator::saddEvaluator,                    // TR::sadd
   TR::TreeEvaluator::integerSubEvaluator,              // TR::isub
   TR::TreeEvaluator::integerSubEvaluator,              // TR::lsub
   TR::TreeEvaluator::fsubEvaluator,                    // TR::fsub
   TR::TreeEvaluator::dsubEvaluator,                    // TR::dsub
   TR::TreeEvaluator::bsubEvaluator,                    // TR::bsub
   TR::TreeEvaluator::ssubEvaluator,                    // TR::ssub
   TR::TreeEvaluator::integerSubEvaluator,              // TR::asub
   TR::TreeEvaluator::integerMulEvaluator,              // TR::imul
   TR::TreeEvaluator::integerMulEvaluator,              // TR::lmul
   TR::TreeEvaluator::fmulEvaluator,                    // TR::fmul
   TR::TreeEvaluator::dmulEvaluator,                    // TR::dmul
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bmul
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::smul
   TR::TreeEvaluator::integerMulEvaluator,              // TR::iumul
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::idiv
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::ldiv
   TR::TreeEvaluator::fdivEvaluator,                    // TR::fdiv
   TR::TreeEvaluator::ddivEvaluator,                    // TR::ddiv
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bdiv
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::sdiv
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::iudiv
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::ludiv
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::irem
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::lrem
   TR::TreeEvaluator::fpRemEvaluator,                   // TR::frem
   TR::TreeEvaluator::fpRemEvaluator,                   // TR::drem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::brem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::srem
   TR::TreeEvaluator::integerDivOrRemEvaluator,         // TR::iurem
   TR::TreeEvaluator::integerNegEvaluator,              // TR::ineg
   TR::TreeEvaluator::integerNegEvaluator,              // TR::lneg
   TR::TreeEvaluator::fnegEvaluator,                    // TR::fneg
   TR::TreeEvaluator::dnegEvaluator,                    // TR::dneg
   TR::TreeEvaluator::bnegEvaluator,                    // TR::bneg
   TR::TreeEvaluator::snegEvaluator,                    // TR::sneg
   TR::TreeEvaluator::integerAbsEvaluator,              // TR::iabs
   TR::TreeEvaluator::integerAbsEvaluator,              // TR::labs
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::fabs
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::dabs
   TR::TreeEvaluator::integerShlEvaluator,              // TR::ishl
   TR::TreeEvaluator::integerShlEvaluator,              // TR::lshl
   TR::TreeEvaluator::bshlEvaluator,                    // TR::bshl
   TR::TreeEvaluator::sshlEvaluator,                    // TR::sshl
   TR::TreeEvaluator::integerShrEvaluator,              // TR::ishr
   TR::TreeEvaluator::integerShrEvaluator,              // TR::lshr
   TR::TreeEvaluator::bshrEvaluator,                    // TR::bshr
   TR::TreeEvaluator::sshrEvaluator,                    // TR::sshr
   TR::TreeEvaluator::integerUshrEvaluator,             // TR::iushr
   TR::TreeEvaluator::integerUshrEvaluator,             // TR::lushr
   TR::TreeEvaluator::bushrEvaluator,                   // TR::bushr
   TR::TreeEvaluator::sushrEvaluator,                   // TR::sushr
   TR::TreeEvaluator::integerRolEvaluator,              // TR::irol
   TR::TreeEvaluator::integerRolEvaluator,              // TR::lrol
   TR::TreeEvaluator::iandEvaluator,                    // TR::iand
   TR::TreeEvaluator::landEvaluator,                  // TR::land
   TR::TreeEvaluator::bandEvaluator,                    // TR::band
   TR::TreeEvaluator::sandEvaluator,                    // TR::sand
   TR::TreeEvaluator::iorEvaluator,                     // TR::ior
   TR::TreeEvaluator::lorEvaluator,                   // TR::lor
   TR::TreeEvaluator::borEvaluator,                     // TR::bor
   TR::TreeEvaluator::sorEvaluator,                     // TR::sor
   TR::TreeEvaluator::ixorEvaluator,                    // TR::ixor
   TR::TreeEvaluator::lxorEvaluator,                  // TR::lxor
   TR::TreeEvaluator::bxorEvaluator,                    // TR::bxor
   TR::TreeEvaluator::sxorEvaluator,                    // TR::sxor
   TR::TreeEvaluator::i2lEvaluator,                   // TR::i2l
   TR::TreeEvaluator::i2fEvaluator,                     // TR::i2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::i2d
   TR::TreeEvaluator::i2bEvaluator,                     // TR::i2b
   TR::TreeEvaluator::i2bEvaluator,                     // TR::i2s
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::i2a
   TR::TreeEvaluator::iu2lEvaluator,                  // TR::iu2l
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iu2f
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iu2d
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iu2a
   TR::TreeEvaluator::l2iEvaluator,                   // TR::l2i
   TR::TreeEvaluator::l2fEvaluator,                   // TR::l2f
   TR::TreeEvaluator::l2dEvaluator,                   // TR::l2d
   TR::TreeEvaluator::l2iEvaluator,                   // TR::l2b
   TR::TreeEvaluator::l2iEvaluator,                   // TR::l2s
   TR::TreeEvaluator::l2aEvaluator,                     // TR::l2a
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lu2f
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lu2d
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lu2a
   TR::TreeEvaluator::f2iEvaluator,                     // TR::f2i
   TR::TreeEvaluator::f2iEvaluator,                     // TR::f2l <- (Uses f2i intentionally)
   TR::TreeEvaluator::f2dEvaluator,                     // TR::f2d
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::f2b
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::f2s
   TR::TreeEvaluator::f2iEvaluator,                     // TR::d2i <- (Uses f2i intentionally)
   TR::TreeEvaluator::f2iEvaluator,                     // TR::d2l <- (Uses f2i intentionally)
   TR::TreeEvaluator::d2fEvaluator,                     // TR::d2f
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::d2b
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::d2s
   TR::TreeEvaluator::b2iEvaluator,                     // TR::b2i
   TR::TreeEvaluator::b2lEvaluator,                   // TR::b2l
   TR::TreeEvaluator::i2fEvaluator,                     // TR::b2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::b2d
   TR::TreeEvaluator::b2sEvaluator,                     // TR::b2s
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::b2a
   TR::TreeEvaluator::bu2iEvaluator,                    // TR::bu2i
   TR::TreeEvaluator::bu2lEvaluator,                  // TR::bu2l
   TR::TreeEvaluator::i2fEvaluator,                    // TR::bu2f
   TR::TreeEvaluator::i2dEvaluator,                    // TR::bu2d
   TR::TreeEvaluator::bu2sEvaluator,                    // TR::bu2s
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bu2a
   TR::TreeEvaluator::s2iEvaluator,                     // TR::s2i
   TR::TreeEvaluator::s2lEvaluator,                   // TR::s2l
   TR::TreeEvaluator::i2fEvaluator,                     // TR::s2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::s2d
   TR::TreeEvaluator::i2bEvaluator,                     // TR::s2b
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::s2a
   TR::TreeEvaluator::su2iEvaluator,                     // TR::su2i
   TR::TreeEvaluator::su2lEvaluator,                   // TR::su2l
   TR::TreeEvaluator::i2fEvaluator,                     // TR::su2f
   TR::TreeEvaluator::i2dEvaluator,                     // TR::su2d
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::su2a
   TR::TreeEvaluator::passThroughEvaluator,             // TR::a2i
   TR::TreeEvaluator::a2lEvaluator,                     // TR::a2l
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::a2b
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::a2s
   TR::TreeEvaluator::integerCmpeqEvaluator,            // TR::icmpeq
   TR::TreeEvaluator::integerCmpneEvaluator,            // TR::icmpne
   TR::TreeEvaluator::integerCmpltEvaluator,            // TR::icmplt
   TR::TreeEvaluator::integerCmpgeEvaluator,            // TR::icmpge
   TR::TreeEvaluator::integerCmpgtEvaluator,            // TR::icmpgt
   TR::TreeEvaluator::integerCmpleEvaluator,            // TR::icmple
   TR::TreeEvaluator::integerCmpeqEvaluator,            // TR::iucmpeq
   TR::TreeEvaluator::integerCmpneEvaluator,            // TR::iucmpne
   TR::TreeEvaluator::unsignedIntegerCmpltEvaluator,    // TR::iucmplt
   TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator,    // TR::iucmpge
   TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator,    // TR::iucmpgt
   TR::TreeEvaluator::unsignedIntegerCmpleEvaluator,    // TR::iucmple
   TR::TreeEvaluator::integerCmpeqEvaluator,            // TR::lcmpeq
   TR::TreeEvaluator::integerCmpneEvaluator,            // TR::lcmpne
   TR::TreeEvaluator::integerCmpltEvaluator,            // TR::lcmplt
   TR::TreeEvaluator::integerCmpgeEvaluator,            // TR::lcmpge
   TR::TreeEvaluator::integerCmpgtEvaluator,            // TR::lcmpgt
   TR::TreeEvaluator::integerCmpleEvaluator,            // TR::lcmple
   TR::TreeEvaluator::integerCmpeqEvaluator,            // TR::lucmpeq
   TR::TreeEvaluator::integerCmpneEvaluator,            // TR::lucmpne
   TR::TreeEvaluator::unsignedIntegerCmpltEvaluator,    // TR::lucmplt
   TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator,    // TR::lucmpge
   TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator,    // TR::lucmpgt
   TR::TreeEvaluator::unsignedIntegerCmpleEvaluator,    // TR::lucmple
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpeq
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpne
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmplt
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpge
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpgt
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmple
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpequ
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpneu
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpltu
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpgeu
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpgtu
   TR::TreeEvaluator::compareFloatAndSetEvaluator,      // TR::fcmpleu
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpeq
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpne
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmplt
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpge
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpgt
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmple
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpequ
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpneu
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpltu
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpgeu
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpgtu
   TR::TreeEvaluator::compareDoubleAndSetEvaluator,     // TR::dcmpleu
   TR::TreeEvaluator::integerCmpeqEvaluator,            // TR::acmpeq
   TR::TreeEvaluator::integerCmpneEvaluator,            // TR::acmpne
   TR::TreeEvaluator::unsignedIntegerCmpltEvaluator,    // TR::acmplt
   TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator,    // TR::acmpge
   TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator,    // TR::acmpgt
   TR::TreeEvaluator::unsignedIntegerCmpleEvaluator,    // TR::acmple
   TR::TreeEvaluator::bcmpeqEvaluator,                  // TR::bcmpeq
   TR::TreeEvaluator::bcmpeqEvaluator,                  // TR::bcmpne
   TR::TreeEvaluator::bcmpltEvaluator,                  // TR::bcmplt
   TR::TreeEvaluator::bcmpgeEvaluator,                  // TR::bcmpge
   TR::TreeEvaluator::bcmpgtEvaluator,                  // TR::bcmpgt
   TR::TreeEvaluator::bcmpleEvaluator,                  // TR::bcmple
   TR::TreeEvaluator::bcmpeqEvaluator,                  // TR::bucmpeq (zPDT)
   TR::TreeEvaluator::bcmpeqEvaluator,                  // TR::bucmpne (zPDT)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmplt
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmpge
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmpgt
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bucmple
   TR::TreeEvaluator::scmpeqEvaluator,                  // TR::scmpeq
   TR::TreeEvaluator::scmpeqEvaluator,                  // TR::scmpne
   TR::TreeEvaluator::scmpltEvaluator,                  // TR::scmplt
   TR::TreeEvaluator::scmpgeEvaluator,                  // TR::scmpge
   TR::TreeEvaluator::scmpgtEvaluator,                  // TR::scmpgt
   TR::TreeEvaluator::scmpleEvaluator,                  // TR::scmple
   TR::TreeEvaluator::sucmpeqEvaluator,                  // TR::sucmpeq
   TR::TreeEvaluator::sucmpeqEvaluator,                  // TR::sucmpne
   TR::TreeEvaluator::sucmpltEvaluator,                  // TR::sucmplt
   TR::TreeEvaluator::sucmpgeEvaluator,                  // TR::sucmpge
   TR::TreeEvaluator::sucmpgtEvaluator,                  // TR::sucmpgt
   TR::TreeEvaluator::sucmpleEvaluator,                  // TR::sucmple
   TR::TreeEvaluator::lcmpEvaluator,                  // TR::lcmp
   TR::TreeEvaluator::compareFloatEvaluator,            // TR::fcmpl
   TR::TreeEvaluator::compareFloatEvaluator,            // TR::fcmpg
   TR::TreeEvaluator::compareDoubleEvaluator,           // TR::dcmpl
   TR::TreeEvaluator::compareDoubleEvaluator,           // TR::dcmpg
   TR::TreeEvaluator::integerIfCmpeqEvaluator,          // TR::ificmpeq
   TR::TreeEvaluator::integerIfCmpneEvaluator,          // TR::ificmpne
   TR::TreeEvaluator::integerIfCmpltEvaluator,          // TR::ificmplt
   TR::TreeEvaluator::integerIfCmpgeEvaluator,          // TR::ificmpge
   TR::TreeEvaluator::integerIfCmpgtEvaluator,          // TR::ificmpgt
   TR::TreeEvaluator::integerIfCmpleEvaluator,          // TR::ificmple
   TR::TreeEvaluator::integerIfCmpeqEvaluator,          // TR::ifiucmpeq
   TR::TreeEvaluator::integerIfCmpneEvaluator,          // TR::ifiucmpne
   TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator,  // TR::ifiucmplt
   TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator,  // TR::ifiucmpge
   TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator,  // TR::ifiucmpgt
   TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator,  // TR::ifiucmple
   TR::TreeEvaluator::integerIfCmpeqEvaluator,          // TR::iflcmpeq
   TR::TreeEvaluator::integerIfCmpneEvaluator,          // TR::iflcmpne
   TR::TreeEvaluator::integerIfCmpltEvaluator,          // TR::iflcmplt
   TR::TreeEvaluator::integerIfCmpgeEvaluator,          // TR::iflcmpge
   TR::TreeEvaluator::integerIfCmpgtEvaluator,          // TR::iflcmpgt
   TR::TreeEvaluator::integerIfCmpleEvaluator,          // TR::iflcmple
   TR::TreeEvaluator::integerIfCmpeqEvaluator,          // TR::iflucmpeq
   TR::TreeEvaluator::integerIfCmpneEvaluator,          // TR::iflucmpne
   TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator,  // TR::iflucmplt
   TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator,  // TR::iflucmpge
   TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator,  // TR::iflucmpgt
   TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator,  // TR::iflucmple
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpeq
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpne
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmplt
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpge
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpgt
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmple
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpequ
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpneu
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpltu
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpgeu
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpgtu
   TR::TreeEvaluator::compareFloatAndBranchEvaluator,   // TR::iffcmpleu
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpeq
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpne
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmplt
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpge
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpgt
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmple
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpequ
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpneu
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpltu
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpgeu
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpgtu
   TR::TreeEvaluator::compareDoubleAndBranchEvaluator,  // TR::ifdcmpleu
   TR::TreeEvaluator::integerIfCmpeqEvaluator,          // TR::ifacmpeq
   TR::TreeEvaluator::integerIfCmpneEvaluator,          // TR::ifacmpne
   TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator,  // TR::ifacmplt
   TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator,  // TR::ifacmpge
   TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator,  // TR::ifacmpgt
   TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator,  // TR::ifacmple
   TR::TreeEvaluator::ifbcmpeqEvaluator,                // TR::ifbcmpeq
   TR::TreeEvaluator::ifbcmpeqEvaluator,                // TR::ifbcmpne
   TR::TreeEvaluator::ifbcmpltEvaluator,                // TR::ifbcmplt
   TR::TreeEvaluator::ifbcmpgeEvaluator,                // TR::ifbcmpge
   TR::TreeEvaluator::ifbcmpgtEvaluator,                // TR::ifbcmpgt
   TR::TreeEvaluator::ifbcmpleEvaluator,                // TR::ifbcmple
   TR::TreeEvaluator::ifbcmpeqEvaluator,                // TR::ifbucmpeq
   TR::TreeEvaluator::ifbcmpeqEvaluator,                // TR::ifbucmpne
   TR::TreeEvaluator::ifbucmpltEvaluator,               // TR::ifbucmplt
   TR::TreeEvaluator::ifbucmpgeEvaluator,               // TR::ifbucmpge
   TR::TreeEvaluator::ifbucmpgtEvaluator,               // TR::ifbucmpgt
   TR::TreeEvaluator::ifbucmpleEvaluator,               // TR::ifbucmple
   TR::TreeEvaluator::ifscmpeqEvaluator,                // TR::ifscmpeq
   TR::TreeEvaluator::ifscmpeqEvaluator,                // TR::ifscmpne
   TR::TreeEvaluator::ifscmpltEvaluator,                // TR::ifscmplt
   TR::TreeEvaluator::ifscmpgeEvaluator,                // TR::ifscmpge
   TR::TreeEvaluator::ifscmpgtEvaluator,                // TR::ifscmpgt
   TR::TreeEvaluator::ifscmpleEvaluator,                // TR::ifscmple
   TR::TreeEvaluator::ifsucmpeqEvaluator,               // TR::ifsucmpeq
   TR::TreeEvaluator::ifsucmpeqEvaluator,               // TR::ifsucmpne
   TR::TreeEvaluator::ifsucmpltEvaluator,               // TR::ifsucmplt
   TR::TreeEvaluator::ifsucmpgeEvaluator,               // TR::ifsucmpge
   TR::TreeEvaluator::ifsucmpgtEvaluator,               // TR::ifsucmpgt
   TR::TreeEvaluator::ifsucmpleEvaluator,               // TR::ifsucmple
   TR::TreeEvaluator::loadaddrEvaluator,                // TR::loadaddr
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ZEROCHK (J9)
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::callIf
   TR::TreeEvaluator::integerRegLoadEvaluator,          // TR::iRegLoad
   TR::TreeEvaluator::aRegLoadEvaluator,                // TR::aRegLoad
   TR::TreeEvaluator::integerRegLoadEvaluator,          // TR::lRegLoad <- (Uses iRegLoad intentionally)
   TR::TreeEvaluator::fRegLoadEvaluator,                // TR::fRegLoad
   TR::TreeEvaluator::dRegLoadEvaluator,                // TR::dRegLoad
   TR::TreeEvaluator::integerRegLoadEvaluator,          // TR::sRegLoad
   TR::TreeEvaluator::integerRegLoadEvaluator,          // TR::bRegLoad
   TR::TreeEvaluator::iRegStoreEvaluator,               // TR::iRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,               // TR::aRegStore
   TR::TreeEvaluator::lRegStoreEvaluator,               // TR::lRegStore
   TR::TreeEvaluator::fRegStoreEvaluator,               // TR::fRegStore
   TR::TreeEvaluator::dRegStoreEvaluator,               // TR::dRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,               // TR::sRegStore
   TR::TreeEvaluator::iRegStoreEvaluator,               // TR::bRegStore
   TR::TreeEvaluator::GlRegDepsEvaluator,               // TR::GlRegDeps
   TR::TreeEvaluator::iternaryEvaluator,                // TR::iternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::lternary
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::bternary
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::sternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::aternary
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::fternary
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::dternary
   TR::TreeEvaluator::treetopEvaluator,                 // TR::treetop
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::MethodEnterHook (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::MethodExitHook (J9)
   TR::TreeEvaluator::passThroughEvaluator,             // TR::PassThrough
   TR::TreeEvaluator::compressedRefsEvaluator,          // TR::compressedRefs
   TR::TreeEvaluator::BBStartEvaluator,                 // TR::BBStart
   TR::TreeEvaluator::BBEndEvaluator,                    // TR::BBEnd

   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::virem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vimin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vimax
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vigetelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::visetelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vimergel
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vimergeh
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpeq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpgt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmplt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmple
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpalleq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpallne
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpallgt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpallge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpalllt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpallle
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpanyeq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpanyne
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpanygt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpanyge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpanylt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vicmpanyle
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vnot
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vselect
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vperm
   TR::TreeEvaluator::SIMDsplatsEvaluator,                 // TR::vsplats
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdmergel
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdmergeh
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdsetelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdgetelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdsel
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdrem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdmadd
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdnmsub
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdmsub
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdmax
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdmin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpeq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpne
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpgt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmplt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmple
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpalleq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpallne
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpallgt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpallge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpalllt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpallle
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpanyeq
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpanyne
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpanygt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpanyge
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpanylt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdcmpanyle
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdsqrt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdlog

   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vinc
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vdec
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vneg
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcom
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vadd
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vsub
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vmul
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vdiv
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vrem
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vand
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vor
   TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator, // TR::vxor
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
   TR::TreeEvaluator::SIMDloadEvaluator,                   // TR::vload
   TR::TreeEvaluator::SIMDloadEvaluator,                   // TR::vloadi
   TR::TreeEvaluator::SIMDstoreEvaluator,                  // TR::vstore
   TR::TreeEvaluator::SIMDstoreEvaluator,                  // TR::vstorei
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vrand
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vreturn
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcall
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vcalli
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vternary
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::v2v
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vl2vd
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vconst
   TR::TreeEvaluator::SIMDgetvelemEvaluator,               // TR::getvelem
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::vsetelem

   TR::TreeEvaluator::SIMDRegLoadEvaluator,                // TR::vbRegLoad
   TR::TreeEvaluator::SIMDRegLoadEvaluator,                // TR::vsRegLoad
   TR::TreeEvaluator::SIMDRegLoadEvaluator,                // TR::viRegLoad
   TR::TreeEvaluator::SIMDRegLoadEvaluator,                // TR::vlRegLoad
   TR::TreeEvaluator::SIMDRegLoadEvaluator,                // TR::vfRegLoad
   TR::TreeEvaluator::SIMDRegLoadEvaluator,                // TR::vdRegLoad
   TR::TreeEvaluator::SIMDRegStoreEvaluator,               // TR::vbRegStore
   TR::TreeEvaluator::SIMDRegStoreEvaluator,               // TR::vsRegStore
   TR::TreeEvaluator::SIMDRegStoreEvaluator,               // TR::viRegStore
   TR::TreeEvaluator::SIMDRegStoreEvaluator,               // TR::vlRegStore
   TR::TreeEvaluator::SIMDRegStoreEvaluator,               // TR::vfRegStore
   TR::TreeEvaluator::SIMDRegStoreEvaluator,               // TR::vdRegStore

/*
 *END OF OPCODES REQUIRED BY OMR
 */
   TR::TreeEvaluator::iconstEvaluator,                  // TR::iuconst
   TR::TreeEvaluator::lconstEvaluator,                // TR::luconst
   TR::TreeEvaluator::bconstEvaluator,                  // TR::buconst
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iuload
   TR::TreeEvaluator::lloadEvaluator,                 // TR::luload
   TR::TreeEvaluator::bloadEvaluator,                   // TR::buload
   TR::TreeEvaluator::iloadEvaluator,                   // TR::iuloadi
   TR::TreeEvaluator::lloadEvaluator,                 // TR::luloadi
   TR::TreeEvaluator::bloadEvaluator,                   // TR::buloadi
   TR::TreeEvaluator::istoreEvaluator,                  // TR::iustore
   TR::TreeEvaluator::lstoreEvaluator,                // TR::lustore
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bustore
   TR::TreeEvaluator::istoreEvaluator,                  // TR::iustorei
   TR::TreeEvaluator::lstoreEvaluator,                // TR::lustorei
   TR::TreeEvaluator::bstoreEvaluator,                  // TR::bustorei
   TR::TreeEvaluator::integerReturnEvaluator,           // TR::iureturn
   TR::TreeEvaluator::integerReturnEvaluator,           // TR::lureturn
   TR::TreeEvaluator::directCallEvaluator,              // TR::iucall
   TR::TreeEvaluator::directCallEvaluator,              // TR::lucall
   TR::TreeEvaluator::integerAddEvaluator,              // TR::iuadd
   TR::TreeEvaluator::integerAddEvaluator,              // TR::luadd
   TR::TreeEvaluator::baddEvaluator,                    // TR::buadd
   TR::TreeEvaluator::integerSubEvaluator,              // TR::iusub
   TR::TreeEvaluator::integerSubEvaluator,              // TR::lusub
   TR::TreeEvaluator::bsubEvaluator,                    // TR::busub
   TR::TreeEvaluator::integerNegEvaluator,              // TR::iuneg
   TR::TreeEvaluator::integerNegEvaluator,              // TR::luneg
   TR::TreeEvaluator::integerShlEvaluator,              // TR::iushl
   TR::TreeEvaluator::integerShlEvaluator,              // TR::lushl
   TR::TreeEvaluator::f2iEvaluator,                     // TR::f2iu
   TR::TreeEvaluator::f2iEvaluator,                     // TR::f2lu <- (Uses f2i intentionally)
   TR::TreeEvaluator::f2bEvaluator,                     // TR::f2bu
   TR::TreeEvaluator::f2cEvaluator,                     // TR::f2c
   TR::TreeEvaluator::f2iEvaluator,                     // TR::d2iu <- (Uses f2i intentionally)
   TR::TreeEvaluator::f2iEvaluator,                     // TR::d2lu <- (Uses f2i intentionally)
   TR::TreeEvaluator::d2bEvaluator,                     // TR::d2bu
   TR::TreeEvaluator::d2cEvaluator,                     // TR::d2c
   TR::TreeEvaluator::integerRegLoadEvaluator,          // TR::iuRegLoad
   TR::TreeEvaluator::integerRegLoadEvaluator,          // TR::luRegLoad <- (Uses iRegLoad intentionally)
   TR::TreeEvaluator::iRegStoreEvaluator,               // TR::iuRegStore
   TR::TreeEvaluator::lRegStoreEvaluator,               // TR::luRegStore
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::iuternary
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::luternary
   TR::TreeEvaluator::iternaryEvaluator,                // TR::buternary
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::suternary
   TR::TreeEvaluator::cconstEvaluator,                  // TR::cconst
   TR::TreeEvaluator::sloadEvaluator,                   // TR::cload
   TR::TreeEvaluator::sloadEvaluator,                   // TR::cloadi
   TR::TreeEvaluator::cstoreEvaluator,                  // TR::cstore
   TR::TreeEvaluator::cstoreEvaluator,                  // TR::cstorei
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::monent (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::monexit (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::monexitfence (J9)
   TR::TreeEvaluator::tstartEvaluator,                  // TR::tstart
   TR::TreeEvaluator::tfinishEvaluator,                 // TR::tfinish
   TR::TreeEvaluator::tabortEvaluator,                  // TR::tabort
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
   TR::TreeEvaluator::fenceEvaluator,                   // TR::fence
   TR::TreeEvaluator::integerAddEvaluator,              // TR::luaddh
   TR::TreeEvaluator::caddEvaluator,                    // TR::cadd
   TR::TreeEvaluator::integerAddEvaluator,              // TR::aiadd
   TR::TreeEvaluator::integerAddEvaluator,              // TR::aiuadd
   TR::TreeEvaluator::integerAddEvaluator,              // TR::aladd
   TR::TreeEvaluator::integerAddEvaluator,              // TR::aluadd
   TR::TreeEvaluator::integerSubEvaluator,              // TR::lusubh
   TR::TreeEvaluator::csubEvaluator,                    // TR::csub
   TR::TreeEvaluator::integerMulhEvaluator,             // TR::imulh
   TR::TreeEvaluator::integerMulhEvaluator,             // TR::iumulh
   TR::TreeEvaluator::integerMulhEvaluator,             // TR::lmulh
   TR::TreeEvaluator::integerMulhEvaluator,             // TR::lumulh
   TR::TreeEvaluator::ibits2fEvaluator,                 // TR::ibits2f
   TR::TreeEvaluator::fbits2iEvaluator,                 // TR::fbits2i
   TR::TreeEvaluator::lbits2dEvaluator,               // TR::lbits2d
   TR::TreeEvaluator::dbits2lEvaluator,               // TR::dbits2l
   TR::TreeEvaluator::lookupEvaluator,                  // TR::lookup (J9)
   TR::TreeEvaluator::NOPEvaluator,                     // TR::trtLookup
   TR::TreeEvaluator::NOPEvaluator,                     // TR::Case
   TR::TreeEvaluator::tableEvaluator,                   // TR::table (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::exceptionRangeFence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::dbgFence (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::NULLCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ResolveCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ResolveAndNULLCHK (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::DIVCHK (J9)
   TR::TreeEvaluator::overflowCHKEvaluator,                // TR::OverflowCHK
   TR::TreeEvaluator::overflowCHKEvaluator,                // TR::UnsignedOverflowCHK 
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
   TR::TreeEvaluator::butestEvaluator,                  // TR::butest (zPDT)
   TR::TreeEvaluator::sutestEvaluator,                  // TR::sutest (zPDT)
   TR::TreeEvaluator::bucmpEvaluator,                   // TR::bucmp (zPDT)
   TR::TreeEvaluator::bcmpEvaluator,                    // TR::bcmp (zPDT)
   TR::TreeEvaluator::sucmpEvaluator,                   // TR::sucmp (zPDT)
   TR::TreeEvaluator::scmpEvaluator,                    // TR::scmp (zPDT)
   TR::TreeEvaluator::iucmpEvaluator,                   // TR::iucmp (zPDT)
   TR::TreeEvaluator::icmpEvaluator,                    // TR::icmp (zPDT)
   TR::TreeEvaluator::iucmpEvaluator,                   // TR::lucmp
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmpo
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmpno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmpo
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmpno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::ificmnno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmno
   TR::TreeEvaluator::ifxcmpoEvaluator,                 // TR::iflcmnno
   TR::TreeEvaluator::integerAddEvaluator,              // TR::iuaddc
   TR::TreeEvaluator::integerAddEvaluator,              // TR::luaddc
   TR::TreeEvaluator::integerSubEvaluator,              // TR::iusubb
   TR::TreeEvaluator::integerSubEvaluator,              // TR::lusubb
   TR::TreeEvaluator::icmpsetEvaluator,                 // TR::icmpset
   TR::TreeEvaluator::icmpsetEvaluator,                 // TR::lcmpset
   TR::TreeEvaluator::bztestnsetEvaluator,              // TR::bztestnset
   TR::TreeEvaluator::atomicorEvaluator,                // TR::ibatomicor
   TR::TreeEvaluator::atomicorEvaluator,                // TR::isatomicor
   TR::TreeEvaluator::atomicorEvaluator,                // TR::iiatomicor
   TR::TreeEvaluator::atomicorEvaluator,                // TR::ilatomicor
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::branch
   TR::TreeEvaluator::igotoEvaluator,                   // TR::igoto

   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::buexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::sexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::cexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::iexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::iuexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::lexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::luexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fuexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::duexp
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::ixfrs
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::lxfrs
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fxfrs
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dxfrs
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fint
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dint
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fnint
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dnint
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fsqrt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dsqrt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::getstack
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dealloca
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::ishfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::lshfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::iushfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::lushfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bshfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::sshfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::bushfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::sushfl
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::idoz
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dcos
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dsin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dtan
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dcosh
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dsinh
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dtanh
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dacos
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dasin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::datan
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::datan2
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dlog
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::imulover
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dfloor
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::ffloor
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dceil
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fceil
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::ibranch
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::mbranch
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::getpm
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::setpm
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::loadAutoOffset
   TR::TreeEvaluator::minmaxEvaluator,                  // TR::imax
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::iumax
   TR::TreeEvaluator::minmaxEvaluator,                  // TR::lmax
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::lumax
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fmax
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dmax
   TR::TreeEvaluator::minmaxEvaluator,                  // TR::imin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::iumin
   TR::TreeEvaluator::minmaxEvaluator,                  // TR::lmin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::lumin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::fmin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::dmin
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::trt
   TR::TreeEvaluator::unImpOpEvaluator,                    // TR::trtSimple
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ihbit (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ilbit (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::inolz (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::inotz (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::ipopcnt (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lhbit (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::llbit (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lnolz (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lnotz (J9)
   TR::TreeEvaluator::badILOpEvaluator,                    // TR::lpopcnt (J9)
   TR::TreeEvaluator::ibyteswapEvaluator,                  // TR::ibyteswap
   TR::TreeEvaluator::bitpermuteEvaluator,                 // TR::bbitpermute
   TR::TreeEvaluator::bitpermuteEvaluator,                 // TR::sbitpermute
   TR::TreeEvaluator::bitpermuteEvaluator,                 // TR::ibitpermute
   TR::TreeEvaluator::bitpermuteEvaluator,                 // TR::lbitpermute
   TR::TreeEvaluator::PrefetchEvaluator,                // TR::Prefetch
