/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdint.h>
#include "arm/codegen/ARMInstruction.hpp"
#include "arm/codegen/ARMOperand2.hpp"
#include "codegen/AheadOfTimeCompile.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/ARMAOTRelocation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Annotations.hpp"
#include "infra/Bit.hpp"

TR::Register*
OMR::ARM::TreeEvaluator::BadILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::frdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::drdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::brdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::srdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lrdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::floadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::aloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::frdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::drdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::brdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::srdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lrdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::astoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::swrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifstoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::idstoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::astoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::istoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::swrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::GotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gotoEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::areturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ireturnEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::returnEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::athrowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::icallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::callEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::baddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::saddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ssubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::asubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::smulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iudivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ludivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iuremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iremEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::snegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::borEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::b2lEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::i2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::l2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2sEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2bEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2sEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::b2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::b2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::b2iEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::b2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bu2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::b2iEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::b2lEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2cEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::su2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::su2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::a2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::a2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::a2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::a2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::icmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::icmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmplEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmplEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmplEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iflcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifacmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifacmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifacmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifacmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifacmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifbucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifscmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifscmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifscmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifscmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifscmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifscmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifsucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifsucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifsucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ifsucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::aRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::aselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::MethodEnterHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::MethodExitHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::PassThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::compressedRefsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

// mask evaluators
TR::Register*
OMR::ARM::TreeEvaluator::mAnyTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mAllTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mmAnyTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mmAllTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mTrueCountEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mFirstTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mLastTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mToLongBitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mLongBitsToMaskEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::b2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::s2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::i2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::l2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::v2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::m2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::m2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::m2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::m2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::m2vEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

// vector evaluators
TR::Register*
OMR::ARM::TreeEvaluator::vnotEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vfmaEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionAddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionAndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionFirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionMinEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionOrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionOrUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreductionXorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vbitselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vconvEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmfmaEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmnotEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionAddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionAndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionFirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionMinEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionOrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionOrUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmreductionXorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcompressEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vexpandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::mcompressEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vbitswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmbitswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::vmexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::f2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::f2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2lEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::f2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2bEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2cEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::d2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::d2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2lEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::d2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2bEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::NewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::newObjectEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::newvalueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::newarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::newArrayEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::anewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::anewArrayEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::variableNewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::newObjectEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::variableNewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::anewArrayEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::multianewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::multianewArrayEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::contigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::discontigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::icalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::acalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::calliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::luaddhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iaddEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iaddEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lusubhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::CaseEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ResolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::OverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::UnsignedOverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::SpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::long2StringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bitOpMemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::allocationFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::loadFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::storeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fullFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::computeCCEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::butestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sutestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::scmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::icmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ificmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ificmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iflcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iflcmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ificmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ificmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iflcmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iflcmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iuaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::luaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::icmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lcmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bztestnsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ibatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::isatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iiatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ilatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::branchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ffloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::fceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fmaxEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::iuminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::luminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::dminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fminEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ihbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerHighestOneBit(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ilbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerLowestOneBit(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::inolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNumberOfLeadingZeros(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::inotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNumberOfTrailingZeros(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ipopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerBitCount(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lhbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longHighestOneBit(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::llbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longLowestOneBit(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longNumberOfLeadingZeros(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longNumberOfTrailingZeros(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longBitCount(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::bbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::sbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::ibitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::lbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::ARM::TreeEvaluator::PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

// Used in buildJNIDispatch in ARMLinkage.cpp for calculating the magic number
uint32_t findARMLoadConstantLength(int32_t value)
   {
     intParts localVal(value);
     uint32_t bitValue    = value, notBits = ~bitValue;
     uint32_t bitTrailing = trailingZeroes(bitValue) & ~1;
     uint32_t notTrailing = trailingZeroes(notBits) & ~1;
     uint32_t base        = bitValue>>bitTrailing;
     uint32_t notBase     = notBits>>notTrailing;
     uint32_t result      = 1;

     if(((base & 0xFFFFFF00) == 0) || ((notBase & 0xFFFFFF00) == 0)) // is Immed8r || reversed immed8r
        {
        //result = 1;
        }
     else if((base & 0xFFFF0000) == 0)                               // 16bit Value
        {
        result = 2;
        }
     else if((notBase & 0xFFFF0000) == 0)                            // reversed 16bits
        {
        result = 3;
        }
     else if((base & 0xFF000000) == 0)                               // 24bit Value
        {
        result = 2;
        if(((base >> 8) & 0x000000FF) != 0)
           result = 3;
        }
     else
        {
        result = 4;
        }
     return result;
   }

// PLEASE READ:
// The code to determine the number of instructions to use is reflected in findARMLoadConstantLength
// Please change the code there as well when making a change here
TR::Instruction *armLoadConstant(TR::Node *node, int32_t value, TR::Register *trgReg, TR::CodeGenerator *cg, TR::Instruction *cursor)
   {
     intParts localVal(value);
     uint32_t bitValue    = value, notBits = ~bitValue;
     uint32_t bitTrailing = trailingZeroes(bitValue) & ~1;
     uint32_t notTrailing = trailingZeroes(notBits) & ~1;
     uint32_t base        = bitValue>>bitTrailing;
     uint32_t notBase     = notBits>>notTrailing;
     TR::Compilation *comp = cg->comp();

     if (comp->getOption(TR_TraceCG))
        {
        traceMsg(comp, "In armLoadConstant with\n");
        traceMsg(comp, "\tvalue = %d\n", value);
        traceMsg(comp, "\tnotBits = %d\n", notBits);
        traceMsg(comp, "\tbitTrailing = %d\n", bitTrailing);
        traceMsg(comp, "\tnotTrailing = %d\n", notTrailing);
        traceMsg(comp, "\tbase = %d\n", base);
        traceMsg(comp, "\tnotBase = %d\n", notBase);
        }

     TR::Instruction *insertingInstructions = cursor;
     if (cursor == NULL)
        cursor = cg->getAppendInstruction();

     // The straddled cases are not caught yet ...

     if ((base & 0xFFFFFF00) == 0)                                  // is Immed8r
        {
        if (bitTrailing == 32)
           {
           bitTrailing = 0;
           }
        cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::mov, node, trgReg, base & 0x000000FF, bitTrailing, cursor);
        }
     else if ((notBase & 0xFFFFFF00) == 0)                         // reversed immed8r
        {
        if (notTrailing == 32)
           {
           notTrailing = 0;
           }
        cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::mvn, node, trgReg, notBase & 0x000000FF, notTrailing, cursor);
        }
     else if ((base & 0xFFFF0000) == 0)                            // 16bit Value
        {
        cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::mov, node, trgReg, (base>>8) & 0x000000FF, 8 + bitTrailing, cursor);
        cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, base & 0x000000FF, bitTrailing, cursor);
        }
     else if ((notBase & 0xFFFF0000) == 0)                         // reversed 16bits
        {
        cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::mov, node, trgReg, (notBase>>8) & 0x000000FF, 8 + notTrailing, cursor);
        cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, notBase & 0x000000FF, notTrailing, cursor);
        cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mvn, node, trgReg, trgReg, cursor);
        }
     else if ((base & 0xFF000000) == 0)                            // 24bit Value
        {
        cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::mov, node, trgReg, (base>>16) & 0x000000FF, 16 + bitTrailing, cursor);
        if (((base >> 8) & 0x000000FF) != 0)
           cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, (base>>8) & 0x000000FF, 8 + bitTrailing, cursor);
        cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, base & 0x000000FF, bitTrailing, cursor);
        }
     else
        {
        TR_ARMOperand2 *op2_3 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
        TR_ARMOperand2 *op2_2 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
        TR_ARMOperand2 *op2_1 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
        TR_ARMOperand2 *op2_0 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);
        cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, trgReg, op2_3, cursor);
        cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_2, cursor);
        cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_1, cursor);
        cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_0, cursor);
        }

     if (!insertingInstructions)
        cg->setAppendInstruction(cursor);

     return(cursor);
   }

TR::Instruction *fixedSeqMemAccess(TR::CodeGenerator *cg, TR::Node *node, int32_t addr, TR::Instruction **nibbles, TR::Register *trgReg, TR::Instruction *cursor)
   {
   // Note: tempReg can be the same register as srcOrTrg. Caller needs to make sure it is right.
   TR::Instruction *cursorCopy = cursor;
   intParts localVal(addr);
   TR::Compilation *comp = cg->comp();

   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   TR_ARMOperand2 *op2_3 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
   TR_ARMOperand2 *op2_2 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
   TR_ARMOperand2 *op2_1 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
   TR_ARMOperand2 *op2_0 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);

   nibbles[0] = cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, trgReg, op2_3, cursor);
   nibbles[1] = cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_2, cursor);
   nibbles[2] = cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_1, cursor);
   nibbles[3] = cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_0, cursor);

   TR::MemoryReference *memRef = new (cg->trHeapMemory()) TR::MemoryReference(trgReg, 0, sizeof(addr), cg);
   nibbles[4] = cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, trgReg, memRef, cursor);

   if (cursorCopy == NULL)
      cg->setAppendInstruction(cursor);

   return cursor;
   }

TR::Instruction *loadAddressConstantInSnippet(TR::CodeGenerator *cg, TR::Node * node, intptr_t address, TR::Register *trgReg, bool isUnloadablePicSite, TR::Instruction *cursor)
   {
   TR::Instruction *q[5];
   TR::Instruction *c = fixedSeqMemAccess(cg, node, 0, q, trgReg, cursor);
   cg->findOrCreateAddressConstant(&address, TR::Address, q[0], q[1], q[2], q[3], q[4], node, isUnloadablePicSite);
   return c;
   }

TR::Instruction *loadAddressConstantFixed(TR::CodeGenerator *cg, TR::Node * node, intptr_t value, TR::Register *trgReg, TR::Instruction *cursor, uint8_t *targetAddress, uint8_t *targetAddress2, int16_t typeAddress, bool doAOTRelocation)
   {
   TR::Compilation *comp = cg->comp();
   bool isAOT = comp->compileRelocatableCode();
   // load a 32-bit constant into a register with a fixed 4 instruction sequence
   TR::Instruction *temp = cursor;
   intParts localVal(value);

   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   TR_ARMOperand2 *op2_3 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
   TR_ARMOperand2 *op2_2 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
   TR_ARMOperand2 *op2_1 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
   TR_ARMOperand2 *op2_0 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);

   cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, trgReg, op2_3, cursor);

   if (value != 0x0)
      {
#ifdef J9_PROJECT_SPECIFIC
      TR_FixedSequenceKind seqKind = fixedSequence1;//does not matter
      if (typeAddress == -1)
         {
         if (doAOTRelocation)
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                              cursor,
                              (uint8_t *)value,
                              (uint8_t *)seqKind,
                              TR_FixedSequenceAddress2, cg),
                              __FILE__,
                              __LINE__,
                              node);
         }
      else
         {
         if (typeAddress == TR_DataAddress)
            {
            if (doAOTRelocation)
               {
               TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
               recordInfo->data1 = (uintptr_t)node->getSymbolReference();
               recordInfo->data2 = (uintptr_t)node->getInlinedSiteIndex();
               recordInfo->data3 = (uintptr_t)seqKind;

               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)recordInfo,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else if ((typeAddress == TR_ClassAddress) || (typeAddress == TR_MethodObject))
            {
            if (doAOTRelocation)
               {
                TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
                recordInfo->data1 = (uintptr_t)targetAddress;
                recordInfo->data2 = (uintptr_t)targetAddress2;
                recordInfo->data3 = (uintptr_t)seqKind;

                cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                  cursor,
                                  (uint8_t *)recordInfo,
                                  (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                  __FILE__,
                                  __LINE__,
                                  node);
               }
            }
         else if (typeAddress == TR_ConstantPool)
            {
            if (doAOTRelocation)
               {
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 targetAddress,
                                 targetAddress2,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else if (typeAddress == TR_MethodEnterExitHookAddress)
            {
            if (doAOTRelocation)
               {
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)node->getSymbolReference(),
                                 (uint8_t *)seqKind,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else if (typeAddress == TR_CallsiteTableEntryAddress)
            {
            if (doAOTRelocation)
               {
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)node->getSymbolReference(),
                                 (uint8_t *)seqKind,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else if (typeAddress == TR_MethodTypeTableEntryAddress)
            {
            if (doAOTRelocation)
               {
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)node->getSymbolReference(),
                                 (uint8_t *)seqKind,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else
            {
            if (doAOTRelocation)
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)value,
                                 (uint8_t *)seqKind,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
            }
         }
#endif
      }
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_2, cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_1, cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, trgReg, op2_0, cursor);

   if (temp == NULL)
      cg->setAppendInstruction(cursor);

   return(cursor);
   }

TR::Instruction *loadAddressConstant(TR::CodeGenerator *cg, TR::Node * node, intptr_t value, TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite, int16_t typeAddress, uint8_t *targetAddress, uint8_t *targetAddress2)
   {
   if (cg->comp()->compileRelocatableCode())
      return loadAddressConstantFixed(cg, node, value, trgReg, cursor, targetAddress, targetAddress2, typeAddress);

   return armLoadConstant(node, value, trgReg, cg, cursor);
   }

// also handles iiload
TR::Register *OMR::ARM::TreeEvaluator::iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   tempReg = cg->allocateRegister();
   if (node->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      tempReg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      tempReg->setContainsInternalPointer();
      }

   node->setRegister(tempReg);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, tempReg, tempMR);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb, node);
      }
   tempMR->decNodeReferenceCounts();

   return tempReg;
   }

static bool nodeIsNeeded(TR::Node *checkNode, TR::Node *node)
   {
   bool result = (checkNode->getOpCode().isCall() ||
                 (checkNode != node && (checkNode->getOpCodeValue() == TR::aloadi || checkNode->getOpCodeValue() == TR::aload)));
   TR::Node *child = NULL;
   for (uint16_t i = 0; i < checkNode->getNumChildren() && !result; i++)
      {
      child = checkNode->getChild(i);
      if (child->getOpCode().isCall())
         result = true;
      // If the class ptr load is feeding anything other than a nullchk it's probably needed
      else if (child == node && !checkNode->getOpCode().isNullCheck())
         result = true;
      else if (child != node && (child->getOpCodeValue() == TR::aloadi || child->getOpCodeValue() == TR::aload))
         result = true;
      else
         result = nodeIsNeeded(child, node);
      if (result || child == node)
         break;
      }
   return result;
   }

// handles aload and iaload
TR::Register *OMR::ARM::TreeEvaluator::aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   // NEW to check for it feeding to a single vgdnop
   static bool disableLoadForVGDNOP = (feGetEnv("TR_DisableLoadForVGDNOP") != NULL);
   bool checkFeedingVGDNOP = !disableLoadForVGDNOP &&
      node->getOpCodeValue() == TR::aloadi &&
      node->getSymbolReference() &&
      node->getReferenceCount() == 2 &&
      !node->getSymbolReference()->isUnresolved() &&
      node->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow;

   if (OMR_UNLIKELY(checkFeedingVGDNOP))
      {
      TR::Node *topNode = cg->getCurrentEvaluationTreeTop()->getNode();
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t index = symRef->getReferenceNumber();
      int32_t numHelperSymbols = cg->symRefTab()->getNumHelperSymbols();
      if (!nodeIsNeeded(topNode, node) &&
         (comp->getSymRefTab()->isNonHelper(index, TR::SymbolReferenceTable::vftSymbol)) &&
          topNode->getOpCodeValue() != TR::treetop &&  // side effects free
         (!topNode->getOpCode().isNullCheck() || !topNode->getOpCode().isResolveCheck()))
         {
         // Search the current block for the feeding cmp to see
         TR::TreeTop *tt = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
         TR::Node *checkNode = NULL;
         while (tt && (tt->getNode()->getOpCodeValue() != TR::BBEnd))
            {
            checkNode = tt->getNode();
            if (cg->getSupportsVirtualGuardNOPing() &&
               (checkNode->isNopableInlineGuard() || checkNode->isHCRGuard()) &&
               (checkNode->getOpCodeValue() == TR::ificmpne || checkNode->getOpCodeValue() == TR::iflcmpne ||
                checkNode->getOpCodeValue() == TR::ifacmpne) &&
                checkNode->getFirstChild() == node)
               {
               // Try get the virtual guard
               TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(checkNode);
               if (!((comp->performVirtualGuardNOPing() || node->isHCRGuard()) &&
                   comp->isVirtualGuardNOPingRequired(virtualGuard)) &&
                   virtualGuard->canBeRemoved())
                  {
                  break;
                  }
               else
                  {
                  if (!topNode->getOpCode().isNullCheck() && !topNode->getOpCode().isResolveCheck())
                     node->decReferenceCount();
                  cg->recursivelyDecReferenceCount(node->getFirstChild());
                  TR::Register *tempReg = cg->allocateRegister(); // Empty register
                  node->setRegister(tempReg);
                  return tempReg;
                  }
               }
            tt = tt->getNextTreeTop();
            }
         }
      }

   TR::Register *tempReg;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (!node->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      if (node->getSymbolReference()->getSymbol()->isNotCollected())
         tempReg = cg->allocateRegister();
      else
         tempReg = cg->allocateCollectedReferenceRegister();
      }
   else
      {
      tempReg = cg->allocateRegister();
      tempReg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      tempReg->setContainsInternalPointer();
      }

   node->setRegister(tempReg);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, tempReg, tempMR);

#ifdef J9_PROJECT_SPECIFIC
   if (node->getSymbolReference() == cg->comp()->getSymRefTab()->findVftSymbolRef())
      {
      generateVFTMaskInstruction(cg, node, tempReg);
      }
#endif

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb, node);
      }
   tempMR->decNodeReferenceCounts();

   return tempReg;
   }

// also handles ilload
TR::Register *OMR::ARM::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register           *lowReg  = cg->allocateRegister();
   TR::Register           *highReg = cg->allocateRegister();
   TR::RegisterPair       *trgReg = cg->allocateRegisterPair(lowReg, highReg);
   TR::Compilation *comp = cg->comp();
   bool bigEndian = cg->comp()->target().cpu.isBigEndian();
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      TR::SymbolReference *vrlRef = comp->getSymRefTab()->findOrCreateVolatileReadLongSymbolRef(comp->getMethodSymbol());

      generateTrg1MemInstruction(cg, TR::InstOpCode::add, node, bigEndian ? highReg: lowReg, tempMR);
      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
      TR::addDependency(deps, lowReg, bigEndian ? TR::RealRegister::gr1 : TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(deps, highReg, bigEndian ? TR::RealRegister::gr0 : TR::RealRegister::gr1, TR_GPR, cg);

      generateImmSymInstruction(cg, TR::InstOpCode::bl,
                                node, (uintptr_t)vrlRef->getMethodAddress(),
                                deps,
                                vrlRef);
      tempMR->decNodeReferenceCounts();
      //deps->stopUsingDepRegs(cg, lowReg, highReg);
      cg->machine()->setLinkRegisterKilled(true);
      }
   else
      {
      TR::MemoryReference *lowMR        = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
      TR::MemoryReference *highMR       = new (cg->trHeapMemory()) TR::MemoryReference(*lowMR, 4, 4, cg);

      generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, bigEndian ? highReg : lowReg, lowMR);
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, bigEndian ? lowReg : highReg, highMR);
      lowMR->decNodeReferenceCounts();
      }
   node->setRegister(trgReg);
   return trgReg;
   }

// aloadEvaluator handled by iloadEvaluator


// also handles ibload
TR::Register *OMR::ARM::TreeEvaluator::bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrsb, 1, cg);
   }

// also handles isload
TR::Register *OMR::ARM::TreeEvaluator::sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrsh, 2, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::commonLoadEvaluator(TR::Node *node,  TR::InstOpCode::Mnemonic memToRegOp, int32_t memSize, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = node->setRegister(cg->allocateRegister());
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, memSize, cg);
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   generateTrg1MemInstruction(cg, memToRegOp, node, tempReg, tempMR);
   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb, node);
      }
   tempMR->decNodeReferenceCounts();
   return tempReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   TR::MemoryReference *tempMR              = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   TR::Register            *destinationRegister = cg->evaluate(node->getSecondChild());
   TR::Node                *firstChild = node->getFirstChild();
   TR::Register            *sourceRegister;
   bool killSource = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (firstChild->getReferenceCount() > 1 && firstChild->getRegister() != NULL)
      {
      if (!firstChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, sourceRegister, firstChild->getRegister());
      killSource = true;
      }
   else
      sourceRegister = cg->evaluate(firstChild);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb, node);
      }

   generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, tempMR, sourceRegister);

   VMwrtbarEvaluator(node, sourceRegister, destinationRegister, NULL, true, cg);

   cg->machine()->setLinkRegisterKilled(true);

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   tempMR->decNodeReferenceCounts();

   return NULL;
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   TR::MemoryReference *tempMR              = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   TR::Register            *destinationRegister = cg->evaluate(node->getChild(2));
   TR::Node                *secondChild = node->getSecondChild();
   TR::Register            *sourceRegister;
   bool killSource = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   /* comp->useCompressedPointers() is false for 32bit environment, leaving the compressed pointer support unimplemented. */
   if (secondChild->getReferenceCount() > 1 && secondChild->getRegister() != NULL)
      {
      if (!secondChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(secondChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, sourceRegister, secondChild->getRegister());
      killSource = true;
      }
   else
      sourceRegister = cg->evaluate(secondChild);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb, node);
      }

   generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, tempMR, sourceRegister);

   VMwrtbarEvaluator(node, sourceRegister, destinationRegister, NULL, true, cg);

   cg->machine()->setLinkRegisterKilled(true);

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(2));
   tempMR->decNodeReferenceCounts();

   return NULL;
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

// also handles ilstore
TR::Register *OMR::ARM::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *valueChild;
   TR::Compilation *comp = cg->comp();
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }
   bool bigEndian = cg->comp()->target().cpu.isBigEndian();
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   TR::Register *valueReg = cg->evaluate(valueChild);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      TR::Register           *addrReg  = cg->allocateRegister();
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      TR::SymbolReference *vwlRef = comp->getSymRefTab()->findOrCreateVolatileWriteLongSymbolRef(comp->getMethodSymbol());

      generateTrg1MemInstruction(cg, TR::InstOpCode::add, node, addrReg, tempMR);
      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
      TR::addDependency(deps, addrReg, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(deps, valueReg->getLowOrder(), TR::RealRegister::gr1, TR_GPR, cg);
      TR::addDependency(deps, valueReg->getHighOrder(), TR::RealRegister::gr2, TR_GPR, cg);

      generateImmSymInstruction(cg, TR::InstOpCode::bl,
                                node, (uintptr_t)vwlRef->getMethodAddress(),
                                deps,
                                vwlRef);
      tempMR->decNodeReferenceCounts();
      //deps->stopUsingDepRegs(cg, valueReg->getLowOrder(), valueReg->getHighOrder());
      cg->machine()->setLinkRegisterKilled(true);
      }
   else
      {
      TR::MemoryReference *lowMR  = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
      TR::MemoryReference *highMR = new (cg->trHeapMemory()) TR::MemoryReference(*lowMR, 4, 4, cg);

      if (bigEndian)
         {
         generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, lowMR, valueReg->getHighOrder());
         generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, highMR, valueReg->getLowOrder());
         }
      else
         {
         generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, lowMR, valueReg->getLowOrder());
         generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, highMR, valueReg->getHighOrder());
         }
      lowMR->decNodeReferenceCounts();
      }
   valueChild->decReferenceCount();
   return NULL;
   }

// also handles ibstore
TR::Register *OMR::ARM::TreeEvaluator::bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::strb, 1, cg);
   }

// also handles isstore,isstore
TR::Register *OMR::ARM::TreeEvaluator::sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::strh, 2, cg);
   }

// also handles astore, iastore, iistore
TR::Register *OMR::ARM::TreeEvaluator::istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::str, 4, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::commonStoreEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic memToRegOp, int32_t memSize, TR::CodeGenerator *cg)
   {
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, memSize, cg);
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   TR::Node *valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb_st, node);
      }
   generateMemSrc1Instruction(cg, memToRegOp, node, tempMR, cg->evaluate(valueChild));
   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? TR::InstOpCode::dmb_v6 : TR::InstOpCode::dmb, node);
      }
   valueChild->decReferenceCount();
   tempMR->decNodeReferenceCounts();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   return VMmonentEvaluator(node, cg);
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   return VMmonexitEvaluator(node, cg);
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerBitCount(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longLowestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longBitCount(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }

TR::Register *OMR::ARM::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraycmplenEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

bool OMR::ARM::TreeEvaluator::stopUsingCopyReg(
      TR::Node* node,
      TR::Register*& reg,
      TR::CodeGenerator* cg)
   {
   if (node != NULL)
      {
      reg = cg->evaluate(node);
      if (!cg->canClobberNodesRegister(node))
         {
         TR::Register *copyReg;
         if (reg->containsInternalPointer() ||
             !reg->containsCollectedReference())
            {
            copyReg = cg->allocateRegister();
            if (reg->containsInternalPointer())
               {
               copyReg->setPinningArrayPointer(reg->getPinningArrayPointer());
               copyReg->setContainsInternalPointer();
               }
            }
         else
            copyReg = cg->allocateCollectedReferenceRegister();

         generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, copyReg, reg);
         reg = copyReg;
         return true;
         }
      }

   return false;
   }

static void constLengthArrayCopyEvaluator(TR::Node *node, int32_t byteLen, TR::Register *src, TR::Register *dst, TR::CodeGenerator *cg)
   {
   int32_t groups = byteLen >> 4;
   int32_t residual=byteLen & 0x0000000F;
   int32_t ri_x = 0, ri;
   TR::Register                        *regs[4] = {NULL, NULL, NULL, NULL};

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(7,7, cg->trMemory());

   TR::addDependency(deps, src, TR::RealRegister::NoReg, TR_GPR, cg);
   deps->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   deps->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   TR::addDependency(deps, dst, TR::RealRegister::NoReg, TR_GPR, cg);
   deps->getPreConditions()->getRegisterDependency(1)->setExcludeGPR0();
   deps->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();


   if (byteLen == 0)
      {
      return;
      }

   regs[0] = cg->allocateRegister(TR_GPR); //as byteLen > 0 here, will need at least one register
   TR::addDependency(deps, regs[0], TR::RealRegister::NoReg, TR_GPR, cg);
   if (groups != 0)
      {
      regs[1] = cg->allocateRegister(TR_GPR);
      regs[2] = cg->allocateRegister(TR_GPR);
      regs[3] = cg->allocateRegister(TR_GPR);
      TR::addDependency(deps, regs[1], TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, regs[2], TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, regs[3], TR::RealRegister::NoReg, TR_GPR, cg);

      TR::Register *countReg = cg->allocateRegister();
      if (groups <= UPPER_IMMED12)
         generateTrg1ImmInstruction(cg, TR::InstOpCode::mov, node, countReg, groups, 0);


      else
         armLoadConstant(node, groups,countReg,cg);

      TR::LabelSymbol *loopStart = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStart);

      for( ri = 0; ri < 4; ri++)
         {
         TR::MemoryReference *mr = new (cg->trHeapMemory()) TR::MemoryReference(src, 4*ri, cg);
       //  mr->setImmediatePreIndexed();  //store the base + offset back to the base reg
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, regs[ri], mr);
         }
      for( ri = 0; ri < 4; ri++)
         {
         TR::MemoryReference *mr = new (cg->trHeapMemory()) TR::MemoryReference(dst, 4*ri, cg);
       //  mr->setImmediatePreIndexed();
         generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, mr, regs[ri]);
         }

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, src, src, 16, 0);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, dst, dst, 16, 0);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sub_r, node, countReg, countReg, 1, 0);
      generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, loopStart);

    }

   for ( ri = 0; ri < residual>>2; ri++)
      {
      //TR::Register *oneReg = regs[ri];
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldr, node, regs[0], new (cg->trHeapMemory()) TR::MemoryReference(src, 4*ri, cg));
      generateMemSrc1Instruction(cg, TR::InstOpCode::str, node, new (cg->trHeapMemory()) TR::MemoryReference(dst, 4*ri, cg), regs[0]);
      }
   if(residual & 2)
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrh, node, regs[0], new (cg->trHeapMemory()) TR::MemoryReference(src, 4*ri, cg));
      generateMemSrc1Instruction(cg, TR::InstOpCode::strh, node, new (cg->trHeapMemory()) TR::MemoryReference(dst, 4*ri, cg), regs[0]);
      ri++;
      ri_x = 2;
      }
   if(residual & 1)
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrb, node, regs[0], new (cg->trHeapMemory()) TR::MemoryReference(src, (4*ri)+ri_x, cg));
      generateMemSrc1Instruction(cg, TR::InstOpCode::strb, node, new (cg->trHeapMemory()) TR::MemoryReference(dst, (4*ri)+ri_x, cg), regs[0]);
      }

   return;

   }

TR::Register *OMR::ARM::TreeEvaluator::arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   TR::Node             *srcObjNode, *dstObjNode, *srcAddrNode, *dstAddrNode, *lengthNode;
   TR::Register         *srcObjReg=NULL, *dstObjReg=NULL, *srcAddrReg, *dstAddrReg, *lengthReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4, stopUsingCopyReg5 = false;
   TR::SymbolReference *arrayCopyHelper;
   FILE *outFile;

   bool isSimpleCopy = (node->getNumChildren() == 3);

   if (isSimpleCopy)
      {
      srcObjNode = NULL;
      dstObjNode = NULL;
      srcAddrNode = node->getChild(0);
      dstAddrNode = node->getChild(1);
      lengthNode = node->getChild(2);
      }
   else
      {
      srcObjNode = node->getChild(0);
      dstObjNode = node->getChild(1);
      srcAddrNode = node->getChild(2);
      dstAddrNode = node->getChild(3);
      lengthNode = node->getChild(4);
      }

   stopUsingCopyReg1 = stopUsingCopyReg(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = stopUsingCopyReg(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = stopUsingCopyReg(srcAddrNode, srcAddrReg, cg);
   stopUsingCopyReg4 = stopUsingCopyReg(dstAddrNode, dstAddrReg, cg);

   lengthReg = cg->evaluate(lengthNode);
   if (!cg->canClobberNodesRegister(lengthNode))
      {
      TR::Register *lenCopyReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, lenCopyReg, lengthReg);
      lengthReg = lenCopyReg;
      stopUsingCopyReg5 = true;
      }

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3,3, cg->trMemory());
   // Build up the dependency conditions
   // r0 is length in bytes, r1 is srcAddr, r2 is dstAddr
   TR::addDependency(deps, lengthReg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(deps, srcAddrReg, TR::RealRegister::gr1, TR_GPR, cg);
   TR::addDependency(deps, dstAddrReg, TR::RealRegister::gr2, TR_GPR, cg);

   arrayCopyHelper = cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMarrayCopy);

   generateImmSymInstruction(cg, TR::InstOpCode::bl,
                                   node, (uintptr_t)arrayCopyHelper->getMethodAddress(),
                                   deps,
                                   arrayCopyHelper);

#ifdef J9_PROJECT_SPECIFIC
   if (!isSimpleCopy)
      {
      TR::TreeEvaluator::genWrtbarForArrayCopy(node, srcObjReg, dstObjReg, cg);
      }
#endif

   if (srcObjNode != NULL)
      cg->decReferenceCount(srcObjNode);
   if (dstObjNode != NULL)
      cg->decReferenceCount(dstObjNode);
   cg->decReferenceCount(srcAddrNode);
   cg->decReferenceCount(dstAddrNode);
   cg->decReferenceCount(lengthNode);
   if (stopUsingCopyReg1)
      cg->stopUsingRegister(srcObjReg);
   if (stopUsingCopyReg2)
      cg->stopUsingRegister(dstObjReg);
   if (stopUsingCopyReg3)
      cg->stopUsingRegister(srcAddrReg);
   if (stopUsingCopyReg4)
      cg->stopUsingRegister(dstAddrReg);
   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The child contains an inline test. If it succeeds, the helper is called.
   // The address of the helper is contained as an int in this node.
   //
   TR::Node     *testNode = node->getFirstChild();
   TR::Node     *firstChild       = testNode->getFirstChild();
   TR::Register *src1Reg   = cg->evaluate(firstChild);
   TR::Node *secondChild = testNode->getSecondChild();
   TR::SymbolReference *asynccheckHelper;

   bool biggerCheck = true;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      bool negated;
      uint32_t base, rotate;
      int32_t tmpChild = secondChild->getInt();
      negated = tmpChild < 0 && tmpChild != 0x80000000; //the negative of the maximum negative int is itself so can not use cmn here

      if(constantIsImmed8r(negated ? -tmpChild : tmpChild, &base, &rotate))
         {
         generateSrc1ImmInstruction(cg, negated ? TR::InstOpCode::cmn : TR::InstOpCode::cmp, node, src1Reg, base, rotate);
         biggerCheck = false;
         }
      }

   if(biggerCheck)
      {
      TR::Register *src2Reg   = cg->evaluate(secondChild);
      generateSrc2Instruction(cg, TR::InstOpCode::cmp, node, src2Reg, src1Reg);
      }

   TR::Instruction *gcPoint;
   TR_ASSERT(testNode->getOpCodeValue() == TR::icmpeq, "opcode not icmpeq - find new condition");

   asynccheckHelper = node->getSymbolReference();
   gcPoint = generateImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptr_t)asynccheckHelper->getMethodAddress(), NULL, asynccheckHelper, NULL, NULL, ARMConditionCodeEQ);
   gcPoint->ARMNeedsGCMap(0xFFFFFFFF);
   cg->machine()->setLinkRegisterKilled(true);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   testNode->decReferenceCount();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   return VMinstanceOfEvaluator(node, cg);
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   return OMR::ARM::TreeEvaluator::VMcheckcastEvaluator(node, cg);
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   return OMR::ARM::TreeEvaluator::VMcheckcastEvaluator(node, cg);
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   return VMnewEvaluator(node, cg);
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      {
      return VMnewEvaluator(node, cg);
      }
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      {
      return VMnewEvaluator(node, cg);
      }
#else
   return OMR::ARM::TreeEvaluator::unImpOpEvaluator(node, cg);
#endif
   }

TR::Register *OMR::ARM::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

// handles: TR::call, TR::acall, TR::icall, TR::lcall, TR::fcall, TR::dcall
TR::Register *OMR::ARM::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());

   return linkage->buildDirectDispatch(node);
   }

TR::Register *OMR::ARM::TreeEvaluator::performCall(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef     = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());
   TR::Register *returnRegister;

   if (isIndirect)
      returnRegister = linkage->buildIndirectDispatch(node);
   else
      returnRegister = linkage->buildDirectDispatch(node);

   return returnRegister;

   }

// handles: TR::icalli, TR::acalli, TR::fcalli, TR::dcalli, TR::lcalli, TR::calli
TR::Register *OMR::ARM::TreeEvaluator::indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());

   return linkage->buildIndirectDispatch(node);
   }

TR::Register *OMR::ARM::TreeEvaluator::treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->evaluate(node->getFirstChild());
   node->getFirstChild()->decReferenceCount();
   return tempReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, TR::InstOpCode::fence, node, node);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: DANGER: the following needs rewriting...
   TR::Register            *resultReg;
   TR::Symbol *sym = node->getSymbol();
   TR::MemoryReference  *mref = new (cg->trHeapMemory()) TR::MemoryReference(node, node->getSymbolReference(), 4, cg);

   if (mref->getUnresolvedSnippet() != NULL)
      {
      resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
      if (mref->useIndexedForm())
         {
         cg->comp()->failCompilation<TR::CompilationException>("implement unresolved loadAddr indexed");
         generateTrg1MemInstruction(cg, TR::InstOpCode::add, node, resultReg, mref);
         }
      else
         {
         // the following is a HACK that assumes that the resolve will
         // have the addr in a register and fix up the next instruction
         // to do the right thing.
         generateTrg1MemInstruction(cg, TR::InstOpCode::mov, node, resultReg, mref);
         }
      }
   else
      {
      if (mref->useIndexedForm())
         {
         resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, resultReg, mref->getBaseRegister(), mref->getIndexRegister());
         }
      else
         {
         int32_t offset = mref->getOffset();
         if (mref->hasDelayedOffset() || offset != 0)
            {
            resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
            if (mref->hasDelayedOffset())
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::add, node, resultReg, mref);
               }
            else
               {
               uint32_t base, rotate;

               if (constantIsImmed8r(offset, &base, &rotate))
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::add, node, resultReg, mref->getBaseRegister(), base, rotate);
                  }
               else
                  {
                  armLoadConstant(node, offset, resultReg, cg, NULL);
                  generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, resultReg, mref->getBaseRegister(), resultReg);
                  }
               }
            }
         else
            {
            resultReg = mref->getBaseRegister();
            if (resultReg == cg->getMethodMetaDataRegister())
               {
               resultReg = cg->allocateRegister();
               generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, resultReg, mref->getBaseRegister());
               }
            }
         }
      }
   node->setRegister(resultReg);
   return resultReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      if (node->getRegLoadStoreSymbolReference()->getSymbol()->isNotCollected() ||
          node->getRegLoadStoreSymbolReference()->getSymbol()->isInternalPointer())
         {
         globalReg = cg->allocateRegister();
         if (node->getRegLoadStoreSymbolReference()->getSymbol()->isInternalPointer())
            {
            globalReg->setContainsInternalPointer();
            globalReg->setPinningArrayPointer(node->getRegLoadStoreSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         }
      else
         globalReg = cg->allocateCollectedReferenceRegister();

      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister();
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegisterPair(cg->allocateRegister(),
                                                cg->allocateRegister());
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

// Also handles TR::aRegStore
TR::Register *OMR::ARM::TreeEvaluator::iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   child->decReferenceCount();
   return globalReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   child->decReferenceCount();
   return globalReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int i;

   for (i=0; i<node->getNumChildren(); i++)
      {
      cg->evaluate(node->getChild(i));
      node->getChild(i)->decReferenceCount();
      }
   return(NULL);
   }


//also handles i2a, iu2a, a2i, a2l
TR::Register *OMR::ARM::TreeEvaluator::passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->evaluate(node->getFirstChild());
   node->setRegister(targetRegister);
   node->getFirstChild()->decReferenceCount();
   return targetRegister;
   }

TR::Register *OMR::ARM::TreeEvaluator::BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Block *block = node->getBlock();
   TR::RegisterDependencyConditions *deps = NULL;

   if (!block->isExtensionOfPreviousBlock() && node->getNumChildren()>0)
      {
      int32_t     i;
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      deps = generateRegisterDependencyConditions(cg, child, 0);
      if (cg->getCurrentEvaluationTreeTop() == comp->getStartTree())
         {
         for (i=0; i<child->getNumChildren(); i++)
            {
            TR::ParameterSymbol *sym = child->getChild(i)->getSymbolReference()->getSymbol()->getParmSymbol();
            if (sym != NULL)
               {
               sym->setAssignedGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getGlobalRegisterNumber()));
               }
            }
         }
      child->decReferenceCount();
      }

   if (node->getLabel() != NULL)
      {
      node->getLabel()->setInstruction(generateLabelInstruction(cg, TR::InstOpCode::label, node, node->getLabel(), deps));
      deps = NULL; // put the dependencies (if any) on either the label or the fence
      }

   TR::Node * fenceNode = TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._startPC);
   TR::Instruction * fence = generateAdminInstruction(cg, TR::InstOpCode::fence, node, deps, fenceNode);

   if (block->isCatchBlock())
      {
      cg->generateCatchBlockBBStartPrologue(node, fence);
      }

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Block * block = node->getBlock();
   TR::Compilation *comp = cg->comp();
   TR::TreeTop *nextTT    = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   TR::Node    *fenceNode = TR::Node::createRelative32BitFenceNode(node, &node->getBlock()->getInstructionBoundaries()._endPC);

   if (NULL == block->getNextBlock())
      {
      // PR108736 If bl jitThrowException is the last instruction, jitGetExceptionTableFromPC fails to find the method.
      TR::Instruction *lastInstruction = cg->getAppendInstruction();
      if (lastInstruction->getOpCodeValue() == TR::InstOpCode::bl
              && lastInstruction->getNode()->getSymbolReference()->getReferenceNumber() == TR_aThrow)
         {
         lastInstruction = generateInstruction(cg, TR::InstOpCode::bad, node, lastInstruction);
         }
      }
   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() > 0 &&
       (!nextTT || !nextTT->getNode()->getBlock()->isExtensionOfPreviousBlock()))
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      deps = generateRegisterDependencyConditions(cg, child, 0);
      child->decReferenceCount();
      }

   // put the dependencies (if any) on the fence
   generateAdminInstruction(cg, TR::InstOpCode::fence, node, deps, fenceNode);

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::conversionAnalyser(TR::Node          *node,
                                                     TR::InstOpCode::Mnemonic    memoryToRegisterOp,
                                                     bool needSignExtend,
                                                     int dstBits,
                                                     TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *sourceRegister;
   TR::Register *targetRegister;

   if (child->getReferenceCount() == 1 && child->getRegister() == NULL && child->getOpCode().isMemoryReference())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, dstBits >> 3, cg);
      if (cg->comp()->target().cpu.isBigEndian() && node->getSize() < child->getSize())
         {
         tempMR->addToOffset(node, child->getSize() - (dstBits>>3), cg);
         }
      targetRegister = cg->allocateRegister();
      generateTrg1MemInstruction(cg, memoryToRegisterOp, node, targetRegister, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      targetRegister = cg->allocateRegister();
      sourceRegister = child->getOpCode().isLong() ? cg->evaluate(child)->getLowOrder() : cg->evaluate(child);
      generateSignOrZeroExtend(node, targetRegister, sourceRegister, needSignExtend, dstBits, cg);
      }
   node->setRegister(targetRegister);
   child->decReferenceCount();

   return targetRegister;
   }


static void generateSignOrZeroExtend(TR::Node *node, TR::Register *dst, TR::Register *src, bool needSignExtend, int32_t bitsInDst, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic opcode = TR::InstOpCode::bad;

   if (cg->comp()->target().cpu.id() >= TR_ARMv6)
      {
      // sxtb/sxth/uxtb/uxth instructions are unavailable in ARMv5 and older
      if (needSignExtend)
         {
         if (bitsInDst == 8) // byte
            {
            opcode = TR::InstOpCode::sxtb;
            }
         else if (bitsInDst == 16) // short
            {
            opcode = TR::InstOpCode::sxth;
            }
         }
      else
         {
         if (bitsInDst == 8) // byte
            {
            opcode = TR::InstOpCode::uxtb;
            }
         else if (bitsInDst == 16) // short
            {
            opcode = TR::InstOpCode::uxth;
            }
         }
      }

   if (opcode != TR::InstOpCode::bad)
      {
      generateTrg1Src1Instruction(cg, opcode, node, dst, src);
      }
   else
      {
      TR_ARMOperand2 *op;

      op = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, src, 32 - bitsInDst);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, dst, op);
      op = new (cg->trHeapMemory()) TR_ARMOperand2((needSignExtend ? ARMOp2RegASRImmed : ARMOp2RegLSRImmed), dst, 32 - bitsInDst);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mov, node, dst, op);
      }
   }
