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

#ifndef OMR_SIMPLIFIERHANDLERS_INCL
#define OMR_SIMPLIFIERHANDLERS_INCL

TR::Node * dftSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier *s);
TR::Node * lstoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * directLoadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * indirectLoadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * indirectStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * directStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * astoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * gotoSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifdCallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * acallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * vcallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * treetopSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * anchorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iaddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * laddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * faddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * daddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * baddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * saddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * isubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ssubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * imulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * smulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * idivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ldivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fdivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ddivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bdivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sdivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * inegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * snegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * constSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lconstSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ilfdabsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ishlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ishrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * irolSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lrolSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iandSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * landSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bandSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sandSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * borSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ixorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lxorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bxorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sxorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fsqrtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dsqrtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2aSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iu2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iu2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iu2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2aSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lu2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lu2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * b2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * b2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * b2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * b2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * b2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bu2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bu2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bu2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bu2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bu2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * s2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * s2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * s2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * s2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * s2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * su2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * su2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * su2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * su2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ificmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ificmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ificmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ificmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ificmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ificmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iflcmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iflcmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iflcmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iflcmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iflcmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iflcmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * normalizeCmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifacmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifacmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifCmpWithEqualitySimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifCmpWithoutEqualitySimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lucmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lucmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lucmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lucmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * acmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * acmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lcmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * passThroughSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * endBlockSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * selectSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * a2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * a2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * v2vSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * vsetelemSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * checkcastSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * checkcastAndNULLCHKSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * variableNewSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * caddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * csubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * imulhSimplifier(TR::Node * node, TR::Block *block, TR::Simplifier * s);
TR::Node * lmulhSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2cSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2cSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * expSimplifier(TR::Node *node,TR::Block *block,TR::Simplifier *s);
TR::Node * ibits2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lbits2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fbits2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dbits2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifxcmpoSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * ifxcmnoSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lookupSwitchSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * tableSwitchSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * nullchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * divchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bndchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * arraycopybndchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bndchkwithspinechkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bcmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bucmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * scmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * sucmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * icmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s);
TR::Node * iucmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s);
TR::Node * lucmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * imaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lmaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * fmaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * dmaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * emaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * byteswapSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * computeCCSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * arraysetSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bitOpMemSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * bitOpMemNDSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * arrayCmpWithPadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * eaddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * esubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * enegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * emulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * eremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * edivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * i2eSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * iu2eSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * l2eSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * f2eSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * d2eSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * c2eSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * e2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * e2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * e2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * e2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * NewSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * lowerTreeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);
TR::Node * arrayLengthSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s);


TR::Node * removeArithmeticsUnderIntegralCompare(TR::Node* node,TR::Simplifier * s);
#endif
