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

#include "codegen/TreeEvaluator.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "x/codegen/ConstantDataSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"

TR::Register*
OMR::X86::AMD64::TreeEvaluator::BadILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::floadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::aloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::astoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::astoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::istoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::GotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gotoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::areturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::returnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::athrowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::callEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::laddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::isubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::asubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::imulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::smulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::idivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iudivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ludivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iuremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::inegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNegEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNegEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAbsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::labsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAbsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerShlEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerShlEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ishrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerShrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerShrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerUshrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerUshrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::irolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRolEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRolEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::i2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::i2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::l2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::b2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::b2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::s2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::s2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::s2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::su2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::su2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::a2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::a2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::a2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::scmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::scmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifacmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifacmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifacmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifacmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifacmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifbcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifbcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ifscmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifscmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::aRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::aselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::MethodEnterHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::MethodExitHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::PassThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vnotEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDsplatsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unaryVectorArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vconvEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDvgetelemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDRegStoreEvaluator(node, cg);
   }


TR::Register*
OMR::X86::AMD64::TreeEvaluator::vindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::vfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::f2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::f2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::f2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::d2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::NewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::newvalueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::newarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::anewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::variableNewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::variableNewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::multianewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::contigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::discontigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::acalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::calliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::luaddhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lusubhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulhEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulhEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulhEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulhEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::CaseEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ResolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::OverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::overflowCHKEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::UnsignedOverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::overflowCHKEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::SpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::long2StringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bitOpMemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::allocationFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::loadFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::storeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fullFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ificmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iflcmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iuaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::luaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::icmpsetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ibatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::isatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iiatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ilatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::branchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpSqrtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ffloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minmaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minmaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minmaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iuminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minmaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::luminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::fminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::dminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ihbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ilbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::inolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::inotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ipopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lhbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::llbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ibyteswapEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::ibitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compressbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::scompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compressbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::icompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compressbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compressbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::bexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::expandbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::sexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::expandbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::iexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::expandbitsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::AMD64::TreeEvaluator::lexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::expandbitsEvaluator(node, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::loadConstant(node, node->getLongInt(), TR_RematerializableAddress, cg);

   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::loadConstant(node, node->getLongInt(), TR_RematerializableLong, cg);

   node->setRegister(targetRegister);
   return targetRegister;
   }

// TODO:AMD64: Could this be combined with istoreEvaluator without too much ugliness?
TR::Register *OMR::X86::AMD64::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *valueChild;
   TR::Compilation* comp = cg->comp();

   if (node->getOpCode().isIndirect())
      valueChild = node->getSecondChild();
   else
      valueChild = node->getFirstChild();

   // Handle special cases
   //
   if (valueChild->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1)
      {
      // Special case storing a double value into long variable
      //
      if (valueChild->getOpCodeValue() == TR::dbits2l &&
          !valueChild->normalizeNanValues())
         {
         if (node->getOpCode().isIndirect())
            {
            node->setChild(1, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::dstorei);
            TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
            node->setChild(1, valueChild);
            TR::Node::recreate(node, TR::lstorei);
            }
         else
            {
            node->setChild(0, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::dstore);
            TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
            node->setChild(0, valueChild);
            TR::Node::recreate(node, TR::lstore);
            }
         cg->decReferenceCount(valueChild);
         return NULL;
         }
      }

   return TR::TreeEvaluator::integerStoreEvaluator(node, cg);
   }

// also handles lloadi
TR::Register *OMR::X86::AMD64::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register *reg = TR::TreeEvaluator::loadMemory(node, sourceMR, TR_RematerializableLong, node->getOpCode().isIndirect(), cg);

   reg->setMemRef(sourceMR);
   node->setRegister(reg);
   sourceMR->decNodeReferenceCounts(cg);
   return reg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[landOpPackage], cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[lorOpPackage], cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[lxorOpPackage], cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      TR::Register *targetRegister = cg->allocateRegister();

      generateRegImmInstruction(TR::InstOpCode::MOV8RegImm4, node, targetRegister, node->getFirstChild()->getInt(), cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(node->getFirstChild());

      return targetRegister;
      }
   else
      {
      // In theory, because iRegStore has chosen to disregard needsSignExtension,
      // we must disregard skipSignExtension here for correctness.
      //
      // However, in fact, it is actually safe to obey skipSignExtension so
      // long as the optimizer only uses it on nodes known to be non-negative
      // when the i2l occurs.  We do already have isNonNegative for that
      // purpose, but it may not always be set by the optimizer if a node known
      // to be non-negative at one point in a block is commoned up above the
      // BNDCHK or branch that determines the node's non-negativity.  The
      // codegen does set the flag during tree evaluation, but the
      // skipSignExtension flag is set by the optimizer with more global
      // knowledge than the tree evaluator, so we will trust it.
      //
      TR::InstOpCode::Mnemonic regMemOpCode,regRegOpCode;
      if(   node->isNonNegative()
         || (node->skipSignExtension() && performTransformation(comp, "TREE EVALUATION: skipping sign extension on node %s despite lack of isNonNegative\n", comp->getDebug()->getName(node))))
         {
         // We prefer these plain (zero-extending) opcodes because the analyser can often eliminate them
         //
         regMemOpCode = TR::InstOpCode::L4RegMem;
         regRegOpCode = TR::InstOpCode::MOVZXReg8Reg4;
         }
      else
         {
         regMemOpCode = TR::InstOpCode::MOVSXReg8Mem4;
         regRegOpCode = TR::InstOpCode::MOVSXReg8Reg4;
         }
      return TR::TreeEvaluator::conversionAnalyser(node, regMemOpCode, regRegOpCode, cg);
      }
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *reg   = cg->evaluate(child);
   if (child->getReferenceCount() > 1)
      {
      // This catches two scenarios:
      //
      // 1) A longClobberEvaluate (or any other register-clobbering logic) on
      // the l2i node could see a refcount of 1, and hence won't make a copy.
      // If child's refcount is more than 1, we do in fact need a copy, so we'd
      // better do it here.
      //
      // 2) If the child is commoned, and the l2i node is also commoned, then
      // we may end up with a situation where the last evaluation of the child
      // is a clobberEvaluate.  By that time, the child's refcount would be 1,
      // so no copy is made, and the register would be clobbered.  Therefore,
      // the l2i node can't return that same register, or else the other uses
      // of the node will end up getting the clobbered value.
      //
      // Note that case 2 is conservative, in that it presumes that the child's
      // register will be clobbered by another node.  If this does not occur,
      // then the copy we're about to make is unnecessary.
      //
      TR::Register *childReg = reg;
      reg = cg->allocateRegister();
      // to support signExtension in GRA, need to preserve upper word
      // in this move
      generateRegRegInstruction(TR::InstOpCode::MOV8RegReg, node, reg, childReg, cg);
      }

   node->setRegister(reg);
   cg->decReferenceCount(child);

   if (cg->enableRegisterInterferences() && node->getOpCode().getSize() == 1)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(node->getRegister());

   return reg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      TR::Register *targetRegister = cg->allocateRegister();

      generateRegImmInstruction(TR::InstOpCode::MOV4RegImm4, node, targetRegister, node->getFirstChild()->getInt(), cg); // implicitly zero extended

      node->setRegister(targetRegister);
      cg->decReferenceCount(node->getFirstChild());

      return targetRegister;
      }
   else
      return TR::TreeEvaluator::conversionAnalyser(node, TR::InstOpCode::L4RegMem, TR::InstOpCode::MOVZXReg8Reg4, cg); // This zero-extends on AMD64
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::conversionAnalyser(node, TR::InstOpCode::MOVSXReg8Mem1, TR::InstOpCode::MOVSXReg8Reg1, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::conversionAnalyser(node, TR::InstOpCode::MOVZXReg8Mem1, TR::InstOpCode::MOVZXReg8Reg1, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::conversionAnalyser(node, TR::InstOpCode::MOVSXReg8Mem2, TR::InstOpCode::MOVSXReg8Reg2, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::conversionAnalyser(node, TR::InstOpCode::MOVZXReg8Mem2, TR::InstOpCode::MOVZXReg8Reg2, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::conversionAnalyser(node, TR::InstOpCode::MOVZXReg8Mem2, TR::InstOpCode::MOVZXReg8Reg2, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *leftRegister  = cg->evaluate(firstChild);
   TR::Register *rightRegister = cg->evaluate(secondChild);

   // Compare left and right operands, all finished with the operands after this.
   generateRegRegInstruction(TR::InstOpCode::CMP8RegReg, node, leftRegister, rightRegister, cg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   TR::Register *isLessThanReg = cg->allocateRegister();
   TR::Register *isNotEqualReg = cg->allocateRegister();
   cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(isLessThanReg);
   cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(isNotEqualReg);

   // The state of things in each possible case after each instruction:
   //                       left < right    left = right    left > right
   // Processor flags:      NE=1 LT=1       NE=0  LT=0      NE=1 LT=0
   generateRegInstruction(TR::InstOpCode::SETL1Reg, node, isLessThanReg, cg);
   // isLessThanReg:        00000001        00000000        00000000
   generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, isNotEqualReg, cg);
   // isNotEqualReg:        00000001        00000000        00000001
   generateRegInstruction(TR::InstOpCode::NEG1Reg, node, isLessThanReg, cg);
   // isLessThanReg:        11111111        00000000        00000000
   generateRegRegInstruction(TR::InstOpCode::OR1RegReg, node, isNotEqualReg, isLessThanReg, cg);
   // isNotEqualReg:        11111111        00000000        00000001

   generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, isNotEqualReg, isNotEqualReg, cg);

   node->setRegister(isNotEqualReg);
   cg->stopUsingRegister(isLessThanReg);

   return isNotEqualReg;
   }

static TR::Register *l2fd(TR::Node *node, TR::RealRegister *target, TR::InstOpCode::Mnemonic opRegMem8, TR::InstOpCode::Mnemonic opRegReg8, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL &&
       child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      tempMR = generateX86MemoryReference(child, cg);
      generateRegMemInstruction(opRegMem8, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);
      generateRegRegInstruction(opRegReg8, node, target, intReg, cg);
      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return l2fd(node, toRealRegister(cg->allocateSinglePrecisionRegister(TR_FPR)), TR::InstOpCode::CVTSI2SSRegMem8, TR::InstOpCode::CVTSI2SSRegReg8, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return l2fd(node, toRealRegister(cg->allocateRegister(TR_FPR)), TR::InstOpCode::CVTSI2SDRegMem8, TR::InstOpCode::CVTSI2SDRegReg8, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: Peepholing
   TR::Node      *child  = node->getFirstChild();
   TR::Register  *sreg   = cg->evaluate(child);
   TR::Register  *treg   = cg->allocateRegister(TR_FPR);
   generateRegRegInstruction(TR::InstOpCode::MOVQRegReg8, node, treg, sreg, cg);
   node->setRegister(treg);
   cg->decReferenceCount(child);
   return treg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: Peepholing
   TR::Node      *child  = node->getFirstChild();
   TR::Register  *sreg   = cg->evaluate(child);
   TR::Register  *treg   = cg->allocateRegister(TR_GPR);
   generateRegRegInstruction(TR::InstOpCode::MOVQReg8Reg, node, treg, sreg, cg);
   if (node->normalizeNanValues())
      {
      static char *disableFastNormalizeNaNs = feGetEnv("TR_disableFastNormalizeNaNs");
      if (disableFastNormalizeNaNs)
         {
         // This one is not clever, but it is simple, and it's based directly
         // on the IA32 version which is known to work, so is safer.
         //
         TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         deps->addPostCondition(treg, TR::RealRegister::NoReg, cg);

         TR::MemoryReference* nan1MR     = generateX86MemoryReference(cg->findOrCreate8ByteConstant(node, DOUBLE_NAN_1_LOW), cg);
         TR::MemoryReference* nan2MR     = generateX86MemoryReference(cg->findOrCreate8ByteConstant(node, DOUBLE_NAN_2_LOW), cg);

         TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *normalizeLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         endLabel  ->setEndInternalControlFlow();

         generateLabelInstruction(   TR::InstOpCode::label,       node, startLabel,               cg);
         generateRegMemInstruction(  TR::InstOpCode::CMP8RegMem,  node, treg, nan1MR,             cg);
         generateLabelInstruction(   TR::InstOpCode::JGE4,        node, normalizeLabel,           cg);
         generateRegMemInstruction(  TR::InstOpCode::CMP8RegMem,  node, treg, nan2MR,             cg);
         generateLabelInstruction(   TR::InstOpCode::JB4,         node, endLabel,                 cg);
         generateLabelInstruction(   TR::InstOpCode::label,       node, normalizeLabel,           cg);
         generateRegImm64Instruction( TR::InstOpCode::MOV8RegImm64, node, treg, DOUBLE_NAN,         cg);
         generateLabelInstruction(   TR::InstOpCode::label,       node, endLabel,           deps, cg);
         }
      else
         {
         // A bunch of bookkeeping
         //
         uint64_t nanDetector = DOUBLE_NAN_2_LOW;

         TR::RegisterDependencyConditions  *internalControlFlowDeps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         internalControlFlowDeps->addPostCondition(treg, TR::RealRegister::NoReg, cg);

         TR::RegisterDependencyConditions  *helperDeps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         helperDeps->addPreCondition( treg, TR::RealRegister::eax, cg);
         helperDeps->addPostCondition(treg, TR::RealRegister::eax, cg);

         TR::MemoryReference* nanDetectorMR = generateX86MemoryReference(cg->findOrCreate8ByteConstant(node, nanDetector),  cg);

         TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *slowPathLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *normalizeLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         endLabel  ->setEndInternalControlFlow();

         // Fast path: if subtracting nanDetector leaves CF=0 or OF=1, then it
         // must be a NaN.
         //
         generateLabelInstruction(  TR::InstOpCode::label,       node, startLabel,           cg);
         generateRegMemInstruction( TR::InstOpCode::CMP8RegMem,  node, treg, nanDetectorMR,  cg);
         generateLabelInstruction(  TR::InstOpCode::JAE4,        node, slowPathLabel,        cg);
         generateLabelInstruction(  TR::InstOpCode::JO4,         node, slowPathLabel,        cg);

         // Slow path
         //
         {
         TR_OutlinedInstructionsGenerator og(slowPathLabel, node, cg);
         generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, node, treg, DOUBLE_NAN, cg);
         generateLabelInstruction(TR::InstOpCode::JMP4, node, endLabel, cg);
         og.endOutlinedInstructionSequence();
         }

         // Merge point
         //
         generateLabelInstruction(TR::InstOpCode::label, node, endLabel, internalControlFlowDeps, cg);
         }
      }
   node->setRegister(treg);
   cg->decReferenceCount(child);
   return treg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register *
OMR::X86::AMD64::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register *
OMR::X86::AMD64::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }
