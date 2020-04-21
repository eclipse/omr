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

#ifndef OMR_AMD64_TREE_EVALUATOR_INCL
#define OMR_AMD64_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { namespace X86 { namespace AMD64 { class TreeEvaluator; } } }
namespace OMR { typedef OMR::X86::AMD64::TreeEvaluator TreeEvaluatorConnector; }
#else
#error OMR::X86::AMD64::TreeEvaluator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/x/codegen/OMRTreeEvaluator.hpp"

namespace TR { class Register; }


namespace OMR
{

namespace X86
{

namespace AMD64
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::X86::TreeEvaluator
   {

   public:
   static TR::Register *landEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // TODO:AMD64: Delete these?
   static TR::Register *aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *laddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   };

} // namespace AMD64

} // namespace X86

} // namespace OMR

#endif
