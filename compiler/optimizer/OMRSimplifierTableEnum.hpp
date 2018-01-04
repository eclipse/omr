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

#ifndef OMR_SIMPLIFIERTABLE_ENUM_INCL
#define OMR_SIMPLIFIERTABLE_ENUM_INCL

   dftSimplifier,           // TR::BadILOp
   dftSimplifier,           // TR::aconst
   constSimplifier,         // TR::iconst
   lconstSimplifier,        // TR::lconst
   dftSimplifier,           // TR::fconst
   dftSimplifier,           // TR::dconst
   constSimplifier,         // TR::bconst
   constSimplifier,         // TR::sconst
   directLoadSimplifier,    // TR::iload
   directLoadSimplifier,    // TR::fload
   directLoadSimplifier,    // TR::dload
   directLoadSimplifier,    // TR::aload
   directLoadSimplifier,    // TR::bload
   directLoadSimplifier,    // TR::sload
   directLoadSimplifier,    // TR::lload
   indirectLoadSimplifier,  // TR::iloadi
   indirectLoadSimplifier,  // TR::floadi
   indirectLoadSimplifier,  // TR::dloadi
   indirectLoadSimplifier,  // TR::aloadi
   indirectLoadSimplifier,  // TR::bloadi
   indirectLoadSimplifier,  // TR::sloadi
   indirectLoadSimplifier,  // TR::lloadi
   directStoreSimplifier,   // TR::istore
   lstoreSimplifier,        // TR::lstore
   dftSimplifier,           // TR::fstore
   dftSimplifier,           // TR::dstore
   astoreSimplifier,        // TR::astore
   dftSimplifier,           // TR::wrtbar
   dftSimplifier,           // TR::bstore
   dftSimplifier,           // TR::sstore
   indirectStoreSimplifier, // TR::lstorei
   indirectStoreSimplifier, // TR::fstorei
   indirectStoreSimplifier, // TR::dstorei
   indirectStoreSimplifier, // TR::astorei
   indirectStoreSimplifier, // TR::wrtbari
   indirectStoreSimplifier, // TR::bstorei
   indirectStoreSimplifier, // TR::sstorei
   indirectStoreSimplifier, // TR::istorei
   gotoSimplifier,          // TR::Goto
   dftSimplifier,           // TR::ireturn
   dftSimplifier,           // TR::lreturn
   dftSimplifier,           // TR::freturn
   dftSimplifier,           // TR::dreturn
   dftSimplifier,           // TR::areturn
   dftSimplifier,           // TR::Return
   dftSimplifier,           // TR::asynccheck
   dftSimplifier,           // TR::athrow
   ifdCallSimplifier,       // TR::icall
   lcallSimplifier,         // TR::lcall
   ifdCallSimplifier,       // TR::fcall
   ifdCallSimplifier,       // TR::dcall
   acallSimplifier,         // TR::acall
   vcallSimplifier,         // TR::call
   iaddSimplifier,          // TR::iadd
   laddSimplifier,          // TR::ladd
   faddSimplifier,          // TR::fadd
   daddSimplifier,          // TR::dadd
   baddSimplifier,          // TR::badd
   saddSimplifier,          // TR::sadd
   isubSimplifier,          // TR::isub
   lsubSimplifier,          // TR::lsub
   fsubSimplifier,          // TR::fsub
   dsubSimplifier,          // TR::dsub
   bsubSimplifier,          // TR::bsub
   ssubSimplifier,          // TR::ssub
   dftSimplifier,           // TR::asub   todo
   imulSimplifier,          // TR::imul
   lmulSimplifier,          // TR::lmul
   fmulSimplifier,          // TR::fmul
   dmulSimplifier,          // TR::dmul
   bmulSimplifier,          // TR::bmul
   smulSimplifier,          // TR::smul
   imulSimplifier,          // TR::iumul
   idivSimplifier,          // TR::idiv
   ldivSimplifier,          // TR::ldiv
   fdivSimplifier,          // TR::fdiv
   ddivSimplifier,          // TR::ddiv
   bdivSimplifier,          // TR::bdiv
   sdivSimplifier,          // TR::sdiv
   idivSimplifier,          // TR::iudiv
   ldivSimplifier,          // TR::ludiv
   iremSimplifier,          // TR::irem
   lremSimplifier,          // TR::lrem
   fremSimplifier,          // TR::frem
   dremSimplifier,          // TR::drem
   bremSimplifier,          // TR::brem
   sremSimplifier,          // TR::srem
   iremSimplifier,          // TR::iurem
   inegSimplifier,          // TR::ineg
   lnegSimplifier,          // TR::lneg
   fnegSimplifier,          // TR::fneg
   dnegSimplifier,          // TR::dneg
   bnegSimplifier,          // TR::bneg
   snegSimplifier,          // TR::sneg


   iabsSimplifier,           // TR::iabs
   labsSimplifier,           // TR::labs
   dftSimplifier,            // TR::fabs   todo
   dftSimplifier,            // TR::dabs   todo

   ishlSimplifier,          // TR::ishl
   lshlSimplifier,          // TR::lshl
   bshlSimplifier,          // TR::bshl
   sshlSimplifier,          // TR::sshl
   ishrSimplifier,          // TR::ishr
   lshrSimplifier,          // TR::lshr
   bshrSimplifier,          // TR::bshr
   sshrSimplifier,          // TR::sshr
   iushrSimplifier,         // TR::iushr
   lushrSimplifier,         // TR::lushr
   bushrSimplifier,         // TR::bushr
   sushrSimplifier,         // TR::sushr
   irolSimplifier,          // TR::irol
   lrolSimplifier,          // TR::lrol
   iandSimplifier,          // TR::iand
   landSimplifier,          // TR::land
   bandSimplifier,          // TR::band
   sandSimplifier,          // TR::sand
   iorSimplifier,           // TR::ior
   lorSimplifier,           // TR::lor
   borSimplifier,           // TR::bor
   sorSimplifier,           // TR::sor
   ixorSimplifier,          // TR::ixor
   lxorSimplifier,          // TR::lxor
   bxorSimplifier,          // TR::bxor
   sxorSimplifier,          // TR::sxor
   i2lSimplifier,           // TR::i2l
   i2fSimplifier,           // TR::i2f
   i2dSimplifier,           // TR::i2d
   i2bSimplifier,           // TR::i2b
   i2sSimplifier,           // TR::i2s
   i2aSimplifier,           // TR::i2a
   iu2lSimplifier,          // TR::iu2l
   iu2fSimplifier,           // TR::iu2f
   iu2dSimplifier,           // TR::iu2d
   i2aSimplifier,           // TR::iu2a

   l2iSimplifier,           // TR::l2i
   l2fSimplifier,           // TR::l2f
   l2dSimplifier,           // TR::l2d
   l2bSimplifier,           // TR::l2b
   l2sSimplifier,           // TR::l2s
   l2aSimplifier,           // TR::l2a
   lu2fSimplifier,          // TR::lu2f
   lu2dSimplifier,          // TR::lu2d
   l2aSimplifier,           // TR::lu2a

#ifdef TR_HOST_X86
   dftSimplifier,           // TR::f2i
   dftSimplifier,           // TR::f2l
#else
   f2iSimplifier,
   f2lSimplifier,
#endif
   f2dSimplifier,           // TR::f2d
   f2bSimplifier,           // TR::f2b
   f2sSimplifier,           // TR::f2s

   d2iSimplifier,           // TR::d2i
   d2lSimplifier,           // TR::d2l
   d2fSimplifier,           // TR::d2f
   d2bSimplifier,           // TR::d2b
   d2sSimplifier,           // TR::d2s

   b2iSimplifier,           // TR::b2i
   b2lSimplifier,           // TR::b2l
   b2fSimplifier,           // TR::b2f
   b2dSimplifier,           // TR::b2d
   b2sSimplifier,           // TR::b2s
   dftSimplifier,           // TR::b2a   todo

   bu2iSimplifier,          // TR::bu2i
   bu2lSimplifier,          // TR::bu2l
   bu2fSimplifier,          // TR::bu2f
   bu2dSimplifier,          // TR::bu2d
   bu2sSimplifier,          // TR::bu2s
   dftSimplifier,           // TR::bu2a   todo

   s2iSimplifier,           // TR::s2i
   s2lSimplifier,           // TR::s2l
   s2fSimplifier,           // TR::s2f
   s2dSimplifier,           // TR::s2d
   s2bSimplifier,           // TR::s2b
   dftSimplifier,           // TR::s2a   todo

   su2iSimplifier,          // TR::su2i
   su2lSimplifier,          // TR::su2l
   su2fSimplifier,          // TR::su2f
   su2dSimplifier,          // TR::su2d
   dftSimplifier,           // TR::su2a   todo

   a2iSimplifier,           // TR::a2i
   a2lSimplifier,           // TR::a2l
   dftSimplifier,           // TR::a2b   todo
   dftSimplifier,           // TR::a2s   todo
   icmpeqSimplifier,        // TR::icmpeq
   icmpneSimplifier,        // TR::icmpne
   icmpltSimplifier,        // TR::icmplt
   icmpgeSimplifier,        // TR::icmpge
   icmpgtSimplifier,        // TR::icmpgt
   icmpleSimplifier,        // TR::icmple
   dftSimplifier,           // TR::iucmpeq
   dftSimplifier,           // TR::iucmpne
   dftSimplifier,           // TR::iucmplt
   dftSimplifier,           // TR::iucmpge
   dftSimplifier,           // TR::iucmpgt
   dftSimplifier,           // TR::iucmple
   lcmpeqSimplifier,        // TR::lcmpeq
   lcmpneSimplifier,        // TR::lcmpne
   lcmpltSimplifier,        // TR::lcmplt
   lcmpgeSimplifier,        // TR::lcmpge
   lcmpgtSimplifier,        // TR::lcmpgt
   lcmpleSimplifier,        // TR::lcmple
   lcmpeqSimplifier,        // TR::lucmpeq
   lcmpneSimplifier,        // TR::lucmpne
   lucmpltSimplifier,       // TR::lucmplt
   lucmpgeSimplifier,       // TR::lucmpge
   lucmpgtSimplifier,       // TR::lucmpgt
   lucmpleSimplifier,       // TR::lucmple
   normalizeCmpSimplifier,  // TR::fcmpeq
   normalizeCmpSimplifier,  // TR::fcmpne
   normalizeCmpSimplifier,  // TR::fcmplt
   normalizeCmpSimplifier,  // TR::fcmpge
   normalizeCmpSimplifier,  // TR::fcmpgt
   normalizeCmpSimplifier,  // TR::fcmple
   normalizeCmpSimplifier,  // TR::fcmpequ
   normalizeCmpSimplifier,  // TR::fcmpneu
   normalizeCmpSimplifier,  // TR::fcmpltu
   normalizeCmpSimplifier,  // TR::fcmpgeu
   normalizeCmpSimplifier,  // TR::fcmpgtu
   normalizeCmpSimplifier,  // TR::fcmpleu
   normalizeCmpSimplifier,  // TR::dcmpeq
   normalizeCmpSimplifier,  // TR::dcmpne
   normalizeCmpSimplifier,  // TR::dcmplt
   normalizeCmpSimplifier,  // TR::dcmpge
   normalizeCmpSimplifier,  // TR::dcmpgt
   normalizeCmpSimplifier,  // TR::dcmple
   normalizeCmpSimplifier,  // TR::dcmpequ
   normalizeCmpSimplifier,  // TR::dcmpneu
   normalizeCmpSimplifier,  // TR::dcmpltu
   normalizeCmpSimplifier,  // TR::dcmpgeu
   normalizeCmpSimplifier,  // TR::dcmpgtu
   normalizeCmpSimplifier,  // TR::dcmpleu
   acmpeqSimplifier,        // TR::acmpeq
   acmpneSimplifier,        // TR::acmpne
   dftSimplifier,           // TR::acmplt
   dftSimplifier,           // TR::acmpge
   dftSimplifier,           // TR::acmpgt
   dftSimplifier,           // TR::acmple
   bcmpeqSimplifier,        // TR::bcmpeq
   bcmpneSimplifier,        // TR::bcmpne
   bcmpltSimplifier,        // TR::bcmplt
   bcmpgeSimplifier,        // TR::bcmpge
   bcmpgtSimplifier,        // TR::bcmpgt
   bcmpleSimplifier,        // TR::bcmple
   bcmpeqSimplifier,        // TR::bucmpeq
   bcmpneSimplifier,        // TR::bucmpne
   bcmpltSimplifier,        // TR::bucmplt
   bcmpgeSimplifier,        // TR::bucmpge
   bcmpgtSimplifier,        // TR::bucmpgt
   bcmpleSimplifier,        // TR::bucmple
   scmpeqSimplifier,        // TR::scmpeq
   scmpneSimplifier,        // TR::scmpne
   scmpltSimplifier,        // TR::scmplt
   scmpgeSimplifier,        // TR::scmpge
   scmpgtSimplifier,        // TR::scmpgt
   scmpleSimplifier,        // TR::scmple
   sucmpeqSimplifier,        // TR::sucmpeq
   sucmpneSimplifier,        // TR::sucmpne
   sucmpltSimplifier,        // TR::sucmplt
   sucmpgeSimplifier,        // TR::sucmpge
   sucmpgtSimplifier,        // TR::sucmpgt
   sucmpleSimplifier,        // TR::sucmple
   lcmpSimplifier,          // TR::lcmp
   dftSimplifier,           // TR::fcmpl
   dftSimplifier,           // TR::fcmpg
   dftSimplifier,           // TR::dcmpl
   dftSimplifier,           // TR::dcmpg
   ificmpeqSimplifier,      // TR::ificmpeq
   ificmpneSimplifier,      // TR::ificmpne
   ificmpltSimplifier,      // TR::ificmplt
   ificmpgeSimplifier,      // TR::ificmpge
   ificmpgtSimplifier,      // TR::ificmpgt
   ificmpleSimplifier,      // TR::ificmple
   ificmpeqSimplifier,      // TR::ifiucmpeq
   ificmpneSimplifier,      // TR::ifiucmpne
   ificmpltSimplifier,      // TR::ifiucmplt
   ificmpgeSimplifier,      // TR::ifiucmpge
   ificmpgtSimplifier,      // TR::ifiucmpgt
   ificmpleSimplifier,      // TR::ifiucmple
   iflcmpeqSimplifier,      // TR::iflcmpeq
   iflcmpneSimplifier,      // TR::iflcmpne
   iflcmpltSimplifier,      // TR::iflcmplt
   iflcmpgeSimplifier,      // TR::iflcmpge
   iflcmpgtSimplifier,      // TR::iflcmpgt
   iflcmpleSimplifier,      // TR::iflcmple
   iflcmpeqSimplifier,      // TR::iflucmpeq
   iflcmpneSimplifier,      // TR::iflucmpne
   iflcmpltSimplifier,      // TR::iflucmplt
   iflcmpgeSimplifier,      // TR::iflucmpge
   iflcmpgtSimplifier,      // TR::iflucmpgt
   iflcmpleSimplifier,      // TR::iflucmple
   normalizeCmpSimplifier,  // TR::iffcmpeq
   normalizeCmpSimplifier,  // TR::iffcmpne
   normalizeCmpSimplifier,  // TR::iffcmplt
   normalizeCmpSimplifier,  // TR::iffcmpge
   normalizeCmpSimplifier,  // TR::iffcmpgt
   normalizeCmpSimplifier,  // TR::iffcmple
   normalizeCmpSimplifier,  // TR::iffcmpequ
   normalizeCmpSimplifier,  // TR::iffcmpneu
   normalizeCmpSimplifier,  // TR::iffcmpltu
   normalizeCmpSimplifier,  // TR::iffcmpgeu
   normalizeCmpSimplifier,  // TR::iffcmpgtu
   normalizeCmpSimplifier,  // TR::iffcmpleu
   normalizeCmpSimplifier,  // TR::ifdcmpeq
   normalizeCmpSimplifier,  // TR::ifdcmpne
   normalizeCmpSimplifier,  // TR::ifdcmplt
   normalizeCmpSimplifier,  // TR::ifdcmpge
   normalizeCmpSimplifier,  // TR::ifdcmpgt
   normalizeCmpSimplifier,  // TR::ifdcmple
   normalizeCmpSimplifier,  // TR::ifdcmpequ
   normalizeCmpSimplifier,  // TR::ifdcmpneu
   normalizeCmpSimplifier,  // TR::ifdcmpltu
   normalizeCmpSimplifier,  // TR::ifdcmpgeu
   normalizeCmpSimplifier,  // TR::ifdcmpgtu
   normalizeCmpSimplifier,  // TR::ifdcmpleu
   ifacmpeqSimplifier,      // TR::ifacmpeq
   ifacmpneSimplifier,      // TR::ifacmpne
   dftSimplifier,           // TR::ifacmplt
   dftSimplifier,           // TR::ifacmpge
   dftSimplifier,           // TR::ifacmpgt
   dftSimplifier,           // TR::ifacmple
   ifCmpWithEqualitySimplifier,     // TR::ifbcmpeq
   ifCmpWithoutEqualitySimplifier,  // TR::ifbcmpne
   ifCmpWithoutEqualitySimplifier,  // TR::ifbcmplt
   ifCmpWithEqualitySimplifier,     // TR::ifbcmpge
   ifCmpWithoutEqualitySimplifier,  // TR::ifbcmpgt
   ifCmpWithEqualitySimplifier,     // TR::ifbcmple
   ifCmpWithEqualitySimplifier,     // TR::ifbucmpeq
   ifCmpWithoutEqualitySimplifier,  // TR::ifbucmpne
   ifCmpWithoutEqualitySimplifier,  // TR::ifbucmplt
   ifCmpWithEqualitySimplifier,     // TR::ifbucmpge
   ifCmpWithoutEqualitySimplifier,  // TR::ifbucmpgt
   ifCmpWithEqualitySimplifier,     // TR::ifbucmple
   ifCmpWithEqualitySimplifier,     // TR::ifscmpeq
   ifCmpWithoutEqualitySimplifier,  // TR::ifscmpne
   ifCmpWithoutEqualitySimplifier,  // TR::ifscmplt
   ifCmpWithEqualitySimplifier,     // TR::ifscmpge
   ifCmpWithoutEqualitySimplifier,  // TR::ifscmpgt
   ifCmpWithEqualitySimplifier,     // TR::ifscmple
   ifCmpWithEqualitySimplifier,     // TR::ifsucmpeq
   ifCmpWithoutEqualitySimplifier,  // TR::ifsucmpne
   ifCmpWithoutEqualitySimplifier,  // TR::ifsucmplt
   ifCmpWithEqualitySimplifier,     // TR::ifsucmpge
   ifCmpWithoutEqualitySimplifier,  // TR::ifsucmpgt
   ifCmpWithEqualitySimplifier,     // TR::ifsucmple
   dftSimplifier,           // TR::loadaddr
   lowerTreeSimplifier,     // TR::ZEROCHK
   lowerTreeSimplifier,     // TR::callIf
   dftSimplifier,           // TR::iRegLoad
   dftSimplifier,           // TR::aRegLoad
   dftSimplifier,           // TR::lRegLoad
   dftSimplifier,           // TR::fRegLoad
   dftSimplifier,           // TR::dRegLoad
   dftSimplifier,           // TR::sRegLoad
   dftSimplifier,           // TR::bRegLoad
   dftSimplifier,           // TR::iRegStore
   dftSimplifier,           // TR::aRegStore
   dftSimplifier,           // TR::lRegStore
   dftSimplifier,           // TR::fRegStore
   dftSimplifier,           // TR::dRegStore
   dftSimplifier,           // TR::sRegStore
   dftSimplifier,           // TR::bRegStore
   dftSimplifier,           // TR::GlRegDeps

   ternarySimplifier,       // TR::iternary
   ternarySimplifier,       // TR::lternary
   ternarySimplifier,       // TR::bternary
   ternarySimplifier,       // TR::sternary
   ternarySimplifier,       // TR::aternary
   ternarySimplifier,       // TR::fternary
   ternarySimplifier,       // TR::dternary
   treetopSimplifier,       // TR::treetop
   lowerTreeSimplifier,     // TR::MethodEnterHook
   lowerTreeSimplifier,     // TR::MethodExitHook
   passThroughSimplifier,   // TR::PassThrough
   anchorSimplifier,        // TR::compressedRefs

   dftSimplifier,           // TR::BBStart
   endBlockSimplifier,       // TR::BBEnd


   dftSimplifier,           // TR::virem
   dftSimplifier,           // TR::vimin
   dftSimplifier,           // TR::vimax
   dftSimplifier,           // TR::vigetelem
   dftSimplifier,           // TR::visetelem
   dftSimplifier,           // TR::vimergel
   dftSimplifier,           // TR::vimergeh
   dftSimplifier,           // TR::vicmpeq
   dftSimplifier,           // TR::vicmpgt
   dftSimplifier,           // TR::vicmpge
   dftSimplifier,           // TR::vicmplt
   dftSimplifier,           // TR::vicmple
   dftSimplifier,           // TR::vicmpalleq
   dftSimplifier,           // TR::vicmpallne
   dftSimplifier,           // TR::vicmpallgt
   dftSimplifier,           // TR::vicmpallge
   dftSimplifier,           // TR::vicmpalllt
   dftSimplifier,           // TR::vicmpallle
   dftSimplifier,           // TR::vicmpanyeq
   dftSimplifier,           // TR::vicmpanyne
   dftSimplifier,           // TR::vicmpanygt
   dftSimplifier,           // TR::vicmpanyge
   dftSimplifier,           // TR::vicmpanylt
   dftSimplifier,           // TR::vicmpanyle

   dftSimplifier,           // TR::vnot
   dftSimplifier,           // TR::vselect
   dftSimplifier,           // TR::vperm

   dftSimplifier,           // TR::vsplats
   dftSimplifier,           // TR::vdmergel
   dftSimplifier,           // TR::vdmergeh
   dftSimplifier,           // TR::vdsetelem
   dftSimplifier,           // TR::vdgetelem
   dftSimplifier,           // TR::vdsel
   dftSimplifier,           // TR::vdrem
   dftSimplifier,           // TR::vdmadd
   dftSimplifier,           // TR::vdnmsub
   dftSimplifier,           // TR::vdmsub
   dftSimplifier,           // TR::vdmax
   dftSimplifier,           // TR::vdmin

   normalizeCmpSimplifier,  // TR::vdcmpeq
   normalizeCmpSimplifier,  // TR::vdcmpne
   normalizeCmpSimplifier,  // TR::vdcmpgt
   normalizeCmpSimplifier,  // TR::vdcmpge
   normalizeCmpSimplifier,  // TR::vdcmplt
   normalizeCmpSimplifier,  // TR::vdcmple

   normalizeCmpSimplifier,  // TR::vdcmpalleq
   normalizeCmpSimplifier,  // TR::vdcmpallne
   normalizeCmpSimplifier,  // TR::vdcmpallgt
   normalizeCmpSimplifier,  // TR::vdcmpallge
   normalizeCmpSimplifier,  // TR::vdcmpalllt
   normalizeCmpSimplifier,  // TR::vdcmpallle
   normalizeCmpSimplifier,  // TR::vdcmpanyeq
   normalizeCmpSimplifier,  // TR::vdcmpanyne
   normalizeCmpSimplifier,  // TR::vdcmpanygt
   normalizeCmpSimplifier,  // TR::vdcmpanyge
   normalizeCmpSimplifier,  // TR::vdcmpanylt
   normalizeCmpSimplifier,  // TR::vdcmpanyle
   dftSimplifier,           // TR::vdsqrt
   dftSimplifier,           // TR::vdlog

   dftSimplifier,           // TR::vinc
   dftSimplifier,           // TR::vdec
   dftSimplifier,           // TR::vneg
   dftSimplifier,           // TR::vcom
   dftSimplifier,           // TR::vadd
   dftSimplifier,           // TR::vsub
   dftSimplifier,           // TR::vmul
   dftSimplifier,           // TR::vdiv
   dftSimplifier,           // TR::vrem
   dftSimplifier,           // TR::vand
   dftSimplifier,           // TR::vor
   dftSimplifier,           // TR::vxor
   dftSimplifier,           // TR::vshl
   dftSimplifier,           // TR::vushr
   dftSimplifier,           // TR::vshr
   dftSimplifier,           // TR::vcmpeq
   dftSimplifier,           // TR::vcmpne
   dftSimplifier,           // TR::vcmplt
   dftSimplifier,           // TR::vucmplt
   dftSimplifier,           // TR::vcmpgt
   dftSimplifier,           // TR::vucmpgt
   dftSimplifier,           // TR::vcmple
   dftSimplifier,           // TR::vucmple
   dftSimplifier,           // TR::vcmpge
   dftSimplifier,           // TR::vucmpge
   directLoadSimplifier,    // TR::vload
   indirectLoadSimplifier,  // TR::vloadi
   directStoreSimplifier,   // TR::vstore
   indirectStoreSimplifier, // TR::vstorei
   dftSimplifier,           // TR::vrand
   dftSimplifier,           // TR::vreturn
   dftSimplifier,           // TR::vcall
   dftSimplifier,           // TR::vcalli
   dftSimplifier,           // TR::vternary
   v2vSimplifier,           // TR::v2v
   dftSimplifier,           // TR::vl2vd
   dftSimplifier,           // TR::vconst
   dftSimplifier,           // TR::getvelem
   vsetelemSimplifier,      // TR::vsetelem

   dftSimplifier,           // TR::vbRegLoad
   dftSimplifier,           // TR::vsRegLoad
   dftSimplifier,           // TR::viRegLoad
   dftSimplifier,           // TR::vlRegLoad
   dftSimplifier,           // TR::vfRegLoad
   dftSimplifier,           // TR::vdRegLoad
   dftSimplifier,           // TR::vbRegStore
   dftSimplifier,           // TR::vsRegStore
   dftSimplifier,           // TR::viRegStore
   dftSimplifier,           // TR::vlRegStore
   dftSimplifier,           // TR::vfRegStore
   dftSimplifier,           // TR::vdRegStore

   constSimplifier,         // TR::iuconst
   lconstSimplifier,        // TR::luconst
   constSimplifier,         // TR::buconst
   directLoadSimplifier,    // TR::iuload
   directLoadSimplifier,    // TR::luload
   directLoadSimplifier,    // TR::buload
   indirectLoadSimplifier,  // TR::iuloadi
   indirectLoadSimplifier,  // TR::luloadi
   indirectLoadSimplifier,  // TR::buloadi
   directStoreSimplifier,   // TR::iustore
   lstoreSimplifier,        // TR::lustore
   dftSimplifier,           // TR::bustore
   indirectStoreSimplifier, // TR::iustorei
   indirectStoreSimplifier, // TR::lustorei
   indirectStoreSimplifier, // TR::bustorei
   dftSimplifier,           // TR::iureturn
   dftSimplifier,           // TR::lureturn
   ifdCallSimplifier,       // TR::iucall
   lcallSimplifier,         // TR::lucall
   iaddSimplifier,          // TR::iuadd
   laddSimplifier,          // TR::luadd
   baddSimplifier,          // TR::buadd
   isubSimplifier,          // TR::iusub
   lsubSimplifier,          // TR::lusub
   bsubSimplifier,          // TR::busub
   inegSimplifier,          // TR::iuneg
   lnegSimplifier,          // TR::luneg
   ishlSimplifier,          // TR::iushl
   lshlSimplifier,          // TR::lushl
   f2iSimplifier,           // TR::f2iu
   f2lSimplifier,           // TR::f2iu
   f2bSimplifier,           // TR::f2bu
   f2cSimplifier,           // TR::f2c
   d2iSimplifier,           // TR::d2iu
   d2lSimplifier,           // TR::d2lu
   d2bSimplifier,           // TR::d2bu
   d2cSimplifier,           // TR::d2c
   dftSimplifier,           // TR::iuRegLoad
   dftSimplifier,           // TR::luRegLoad
   dftSimplifier,           // TR::iuRegStore
   dftSimplifier,           // TR::luRegStore
   ternarySimplifier,       // TR::iuternary
   ternarySimplifier,       // TR::luternary
   ternarySimplifier,       // TR::buternary
   ternarySimplifier,       // TR::suternary
   constSimplifier,         // TR::cconst
   directLoadSimplifier,    // TR::cload
   indirectLoadSimplifier,  // TR::cloadi
   dftSimplifier,           // TR::cstore
   indirectStoreSimplifier, // TR::cstorei
   dftSimplifier,           // TR::monent
   dftSimplifier,           // TR::monexit
   dftSimplifier,           // TR::monexitfence
   dftSimplifier,           // TR::tstart
   dftSimplifier,           // TR::tfinish
   dftSimplifier,           // TR::tabort
   dftSimplifier,           // TR::instanceof
   checkcastSimplifier,     // TR::checkcast
   checkcastAndNULLCHKSimplifier,     // TR::checkcastAndNULLCHK
   NewSimplifier,           // TR::New
   dftSimplifier,           // TR::newarray
   dftSimplifier,           // TR::anewarray
   variableNewSimplifier,   // TR::variableNew
   variableNewSimplifier,   // TR::variableNewArray
   dftSimplifier,           // TR::multianewarray
   arrayLengthSimplifier,   // TR::arraylength
   arrayLengthSimplifier,   // TR::contigarraylength
   arrayLengthSimplifier,   // TR::discontigarraylength
   dftSimplifier,           // TR::icalli
   dftSimplifier,           // TR::iucalli
   dftSimplifier,           // TR::lcalli
   dftSimplifier,           // TR::lucalli
   dftSimplifier,           // TR::fcalli
   dftSimplifier,           // TR::dcalli
   dftSimplifier,           // TR::acalli
   dftSimplifier,           // TR::calli
   dftSimplifier,           // TR::fence
   dftSimplifier,           // TR::luaddh
   caddSimplifier,          // TR::cadd
   iaddSimplifier,          // TR::aiadd
   iaddSimplifier,          // TR::aiuadd
   laddSimplifier,          // TR::aladd
   laddSimplifier,          // TR::aluadd
   dftSimplifier,           // TR::lusubh
   csubSimplifier,          // TR::csub
   imulhSimplifier,           // TR::imulh
   dftSimplifier,           // TR::iumulh
   lmulhSimplifier,         // TR::lmulh
   lmulhSimplifier,         // TR::lumulh
//   cmulSimplifier,          // TR::cmul
//   cdivSimplifier,          // TR::cdiv
//   cremSimplifier,          // TR::crem

//   cshlSimplifier,          // TR::cshl
//   cushrSimplifier,         // TR::cushr

   ibits2fSimplifier,       // TR::ibits2f
   fbits2iSimplifier,       // TR::fbits2i
   lbits2dSimplifier,       // TR::lbits2d
   dbits2lSimplifier,       // TR::dbits2l

   lookupSwitchSimplifier,  // TR::lookup
   dftSimplifier,           // TR::trtLookup
   dftSimplifier,           // TR::Case
   tableSwitchSimplifier,   // TR::table
   dftSimplifier,           // TR::exceptionRangeFence
   dftSimplifier,           // TR::dbgFence
   nullchkSimplifier,       // TR::NULLCHK
   dftSimplifier,           // TR::ResolveCHK
   dftSimplifier,           // TR::ResolveAndNULLCHK
   divchkSimplifier,        // TR::DIVCHK
   dftSimplifier,           // TR::OverflowCHK
   dftSimplifier,           // TR::UnsignedOverflowCHK
   bndchkSimplifier,        // TR::BNDCHK
   arraycopybndchkSimplifier, // TR::ArrayCopyBNDCHK
   bndchkwithspinechkSimplifier, // TR::BNDCHKwithSpineCHK
   dftSimplifier,           // TR::SpineCHK
   dftSimplifier,           // TR::ArrayStoreCHK
   dftSimplifier,           // TR::ArrayCHK
   dftSimplifier,           // TR::Ret
   dftSimplifier,           // TR::arraycopy
   arraysetSimplifier,      // TR::arrayset
   dftSimplifier,           // TR::arraytranslate
   dftSimplifier,           // TR::arraytranslateAndTest
   dftSimplifier,           // TR::long2String
   bitOpMemSimplifier,      // TR::bitOpMem
   bitOpMemNDSimplifier,    // TR::bitOpMemND
   dftSimplifier,           // TR::arraycmp
   arrayCmpWithPadSimplifier,  // TR::arraycmpWithPad
   dftSimplifier,           // TR::allocationFence
   dftSimplifier,           // TR::loadFence
   dftSimplifier,           // TR::storeFence
   dftSimplifier,           // TR::fullFence
   dftSimplifier,           // TR::MergeNew

   computeCCSimplifier,     // TR::computeCC

   dftSimplifier,           // TR::butest
   dftSimplifier,           // TR::sutest

   bucmpSimplifier,         // TR::bucmp
   bcmpSimplifier,          // TR::bcmp
   sucmpSimplifier,         // TR::sucmp
   scmpSimplifier,          // TR::scmp
   iucmpSimplifier,         // TR::iucmp
   icmpSimplifier,          // TR::icmp
   lucmpSimplifier,         // TR::lucmp

   ifxcmpoSimplifier,       // TR::ificmpo
   ifxcmpoSimplifier,       // TR::ificmpno
   ifxcmpoSimplifier,       // TR::iflcmpo
   ifxcmpoSimplifier,       // TR::iflcmpno
   ifxcmnoSimplifier,       // TR::ificmno
   ifxcmnoSimplifier,       // TR::ificmnno
   ifxcmnoSimplifier,       // TR::iflcmno
   ifxcmnoSimplifier,       // TR::iflcmnno

   iaddSimplifier,          // TR::iuaddc
   laddSimplifier,          // TR::luaddc
   isubSimplifier,          // TR::iusubb
   lsubSimplifier,          // TR::lusubb
   dftSimplifier,           // TR::icmpset
   dftSimplifier,           // TR::lcmpset
   dftSimplifier,           // TR::btestnset
   dftSimplifier,           // TR::ibatomicor
   dftSimplifier,           // TR::isatomicor
   dftSimplifier,           // TR::iiatomicor
   dftSimplifier,           // TR::ilatomicor

   expSimplifier,           // TR::dexp
   dftSimplifier,           // TR::branch
   dftSimplifier,           // TR::igoto

   dftSimplifier,           // TR::bexp
   dftSimplifier,           // TR::buexp
   dftSimplifier,           // TR::sexp
   dftSimplifier,           // TR::cexp
   dftSimplifier,           // TR::iexp
   dftSimplifier,           // TR::iuexp
   dftSimplifier,           // TR::lexp
   dftSimplifier,           // TR::luexp
   expSimplifier,           // TR::fexp
   expSimplifier,           // TR::fuexp
   expSimplifier,           // TR::duexp

   dftSimplifier,           // TR::ixfrs
   dftSimplifier,           // TR::lxfrs
   dftSimplifier,           // TR::fxfrs
   dftSimplifier,           // TR::dxfrs

   dftSimplifier,           // TR::fint
   dftSimplifier,           // TR::dint
   dftSimplifier,           // TR::fnint
   dftSimplifier,           // TR::dnint

   dftSimplifier,           // TR::fsqrt
   dftSimplifier,           // TR::dsqrt

   dftSimplifier,           // TR::getstack
   dftSimplifier,           // TR::dealloca

   dftSimplifier,           // TR::ishfl
   dftSimplifier,           // TR::lshfl
   dftSimplifier,           // TR::iushfl
   dftSimplifier,           // TR::lushfl
   dftSimplifier,           // TR::bshfl
   dftSimplifier,           // TR::sshfl
   dftSimplifier,           // TR::bushfl
   dftSimplifier,           // TR::sushfl

   dftSimplifier,           // TR::idoz

   dftSimplifier,           // TR::dcos
   dftSimplifier,           // TR::dsin
   dftSimplifier,           // TR::dtan

   dftSimplifier,           // TR::dcosh
   dftSimplifier,           // TR::dsinh
   dftSimplifier,           // TR::dtanh

   dftSimplifier,           // TR::dacos
   dftSimplifier,           // TR::dasin
   dftSimplifier,           // TR::datan

   dftSimplifier,           // TR::datan2

   dftSimplifier,           // TR::dlog

   dftSimplifier,           // TR::imulover
   dftSimplifier,           // TR::dfloor
   dftSimplifier,           // TR::ffloor
   dftSimplifier,           // TR::dceil
   dftSimplifier,           // TR::fceil
   dftSimplifier,           // TR::ibranch
   dftSimplifier,           // TR::mbranch
   dftSimplifier,           // TR::getpm
   dftSimplifier,           // TR::setpm
   dftSimplifier,           // TR::loadAutoOffset
//#endif


   imaxminSimplifier,       // TR::imax
   imaxminSimplifier,       // TR::iumax
   lmaxminSimplifier,       // TR::lmax
   lmaxminSimplifier,       // TR::lumax
   fmaxminSimplifier,       // TR::fmax
   dmaxminSimplifier,       // TR::dmax

   imaxminSimplifier,       // TR::imin
   imaxminSimplifier,       // TR::iumin
   lmaxminSimplifier,       // TR::lmin
   lmaxminSimplifier,       // TR::lumin
   fmaxminSimplifier,       // TR::fmin
   dmaxminSimplifier,       // TR::dmin

   dftSimplifier,           // TR::trt
   dftSimplifier,           // TR::trtSimple

   dftSimplifier, //TR::ihbit
   dftSimplifier, //TR::ilbit
   dftSimplifier, //TR::inolz
   dftSimplifier, //TR::inotz
   dftSimplifier, //TR::ipopcnt

   dftSimplifier, //TR::lhbit
   dftSimplifier, //TR::llbit
   dftSimplifier, //TR::lnolz
   dftSimplifier, //TR::lnotz
   dftSimplifier, //TR::lpopcnt

   dftSimplifier,            // TR::ibyteswap

   dftSimplifier,            // TR::bbitpermute
   dftSimplifier,            // TR::sbitpermute
   dftSimplifier,            // TR::ibitpermute
   dftSimplifier,            // TR::lbitpermute

   dftSimplifier,           // TR::Prefetch

#endif
