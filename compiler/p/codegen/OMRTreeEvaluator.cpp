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
#include <stdio.h>
#include <string.h>
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/PPCEvaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CPU.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/PersistentInfo.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Annotations.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "optimizer/Structure.hpp"
#include "p/codegen/OMRCodeGenerator.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/LoadStoreHandler.hpp"
#include "p/codegen/PPCAOTRelocation.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/Runtime.hpp"

TR::Register*
OMR::Power::TreeEvaluator::BadILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iconstEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iconstEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::floadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::aloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::astoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::astoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::istoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::GotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gotoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::freturnEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::areturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ireturnEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::returnEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::athrowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::icallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::acallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::callEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::baddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iaddEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::saddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iaddEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::isubEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ssubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::isubEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::imulEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::smulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::imulEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iudivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::idivEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ludivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ldivEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iuremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iremEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fnegEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inegEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::snegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inegEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::borEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2aEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2lEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::b2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::b2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::b2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bu2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bu2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::s2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::s2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::su2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmplEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dcmpgEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iflcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpequEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpltuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgeuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpgtuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpleuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifdcmpneEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpltEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpleEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpltEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifbucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpleEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifscmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifscmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifscmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpltEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifscmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifscmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifscmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ificmpleEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifsucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpltEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifsucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifsucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ifsucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifiucmpleEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::aRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gprRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::aselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fselectEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::MethodEnterHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::MethodExitHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::PassThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

// mask evaluators
TR::Register*
OMR::Power::TreeEvaluator::mAnyTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mAllTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mmAnyTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mmAllTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vloadiEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vstoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vstoreiEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mTrueCountEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mFirstTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mLastTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mToLongBitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mLongBitsToMaskEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::mRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::b2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::s2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::i2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::l2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::v2mEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::m2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::m2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::m2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::m2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::m2vEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

// vector evaluators
TR::Register*
OMR::Power::TreeEvaluator::vcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::DataType elementType = node->getDataType().getVectorElementType();

   switch (elementType)
      {
      case TR::Int8:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpequb);
      case TR::Int16:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpequh);
      case TR::Int32:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpequw);
      case TR::Int64:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpequd);
      case TR::Float:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvcmpeqsp);
      case TR::Double:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvcmpeqdp);
      default:
         TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register *vcmpHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, bool complement, bool switchOperands)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *lhsReg = cg->evaluate(firstChild);
   TR::Register *rhsReg = cg->evaluate(secondChild);
   TR::Register *resReg = cg->allocateRegister(TR_VRF);

   node->setRegister(resReg);

   if (switchOperands)
      generateTrg1Src2Instruction(cg, op, node, resReg, rhsReg, lhsReg);
   else
      generateTrg1Src2Instruction(cg, op, node, resReg, lhsReg, rhsReg);

   if (complement)
      generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlnor, node, resReg, resReg, resReg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return resReg;
   }

TR::Register*
OMR::Power::TreeEvaluator::vcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
      TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

      TR::DataType elementType = node->getDataType().getVectorElementType();
      bool p9Plus = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9); // VMX vector NE instructions are only available on P9 and up

      // for types/power versions where no PPC assembly instruction exists for NE, take the complement of EQ instead -> (A != B) == ~(A == B)
      switch (elementType)
         {
         case TR::Int8:
            if (p9Plus)
               return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpneb);
            else
               return vcmpHelper(node, cg, TR::InstOpCode::vcmpequb, true, false);
         case TR::Int16:
            if (p9Plus)
               return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpneh);
            else
               return vcmpHelper(node, cg, TR::InstOpCode::vcmpequh, true, false);
         case TR::Int32:
            if (p9Plus)
               return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpnew);
            else
               return vcmpHelper(node, cg, TR::InstOpCode::vcmpequw, true, false);
         case TR::Int64:
            return vcmpHelper(node, cg, TR::InstOpCode::vcmpequd, true, false);
         case TR::Float:
            return vcmpHelper(node, cg, TR::InstOpCode::xvcmpeqsp, true, false);
         case TR::Double:
            return vcmpHelper(node, cg, TR::InstOpCode::xvcmpeqdp, true, false);
         default:
            TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
         }
   }

TR::Register*
OMR::Power::TreeEvaluator::vcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::DataType elementType = node->getDataType().getVectorElementType();

   // since no PPC assembly instruction exists for LT, switch operands and take GT instead -> (A < B) == (B > A)
   switch (elementType)
      {
      case TR::Int8:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsb, false, true);
      case TR::Int16:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsh, false, true);
      case TR::Int32:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsw, false, true);
      case TR::Int64:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsd, false, true);
      case TR::Float:
         return vcmpHelper(node, cg, TR::InstOpCode::xvcmpgtsp, false, true);
      case TR::Double:
         return vcmpHelper(node, cg, TR::InstOpCode::xvcmpgtdp, false, true);
      default:
         TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register*
OMR::Power::TreeEvaluator::vcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::DataType elementType = node->getDataType().getVectorElementType();

   switch (elementType)
      {
      case TR::Int8:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpgtsb);
      case TR::Int16:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpgtsh);
      case TR::Int32:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpgtsw);
      case TR::Int64:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vcmpgtsd);
      case TR::Float:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvcmpgtsp);
      case TR::Double:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvcmpgtdp);
      default:
         TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register*
OMR::Power::TreeEvaluator::vcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::DataType elementType = node->getDataType().getVectorElementType();

   // since no PPC assembly instruction exists for LE, take complements/switch operands as needed and use other comparison operations instead
   switch (elementType)
      {
      case TR::Int8:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsb, true, false); // (A <= B) == ~(A > B)
      case TR::Int16:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsh, true, false);
      case TR::Int32:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsw, true, false);
      case TR::Int64:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsd, true, false);
      case TR::Float:
         return vcmpHelper(node, cg, TR::InstOpCode::xvcmpgesp, false, true); // (A <= B) == (B >= A)
      case TR::Double:
         return vcmpHelper(node, cg, TR::InstOpCode::xvcmpgedp, false, true);
      default:
         TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register*
OMR::Power::TreeEvaluator::vcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::DataType elementType = node->getDataType().getVectorElementType();

   // for types where no PPC assembly instruction exists for GE, reverse operands and then take the complement of GT instead -> (A >= B) == ~(B > A)
   switch (elementType)
      {
      case TR::Int8:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsb, true, true);
      case TR::Int16:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsh, true, true);
      case TR::Int32:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsw, true, true);
      case TR::Int64:
         return vcmpHelper(node, cg, TR::InstOpCode::vcmpgtsd, true, true);
      case TR::Float:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvcmpgesp);
      case TR::Double:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvcmpgedp);
      default:
         TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register*
OMR::Power::TreeEvaluator::vloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vloadEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vstoreEvaluator(node, cg);
   }

static TR::Register *vreductionAddSubWordHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic sumOp)
   {
   TR::Node *firstChild = node->getFirstChild();

   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *resReg = cg->allocateRegister();

   TR::Register *tempRes = cg->allocateRegister(TR_VRF);
   TR::Register *zeroReg = cg->allocateRegister(TR_VRF);

   node->setRegister(resReg);

   //Evaluate sum across vector operand. Since the operands will be either 8 or 16 bytes long, and thus the results cannot overflow, it is safe to use saturate sum instructions here
   generateTrg1ImmInstruction(cg, TR::InstOpCode::vspltisw, node, zeroReg, 0);
   generateTrg1Src2Instruction(cg, sumOp, node, tempRes, srcReg, zeroReg); //Sum across each word element
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vsumsws, node, tempRes, tempRes, zeroReg); //Sum word elements across entire vector operand

   //Copy result into GPR
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrld, node, resReg, tempRes);
   else
   {
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, tempRes, tempRes, zeroReg, 3); //move sum to upper doubleword element of tempRes
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, resReg, tempRes);
   }

   cg->stopUsingRegister(tempRes);
   cg->stopUsingRegister(zeroReg);
   cg->decReferenceCount(firstChild);

   return resReg;
   }

static TR::Register *vreductionAddWordHelper(TR::Node *node, TR::CodeGenerator *cg, TR::DataType type)
   {
   TR::Node *firstChild = node->getFirstChild();

   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *resReg;

   TR::Register *shiftReg = cg->allocateRegister(TR_VRF);
   TR::Register *temp = cg->allocateRegister(TR_VRF);
   TR::Register *tempGPR;
   TR::Register *tempVSX;

   //choose addOp and allocate registers based on data type
   TR::InstOpCode::Mnemonic addOp;

   if (type == TR::Int32)
   {
      addOp = TR::InstOpCode::vadduwm;
      tempVSX = cg->allocateRegister(TR_VRF);
      tempGPR = cg->allocateRegister();
      resReg = tempGPR;
   }
   else if (type == TR::Float)
   {
      addOp = TR::InstOpCode::xvaddsp;
      tempVSX = cg->allocateRegister(TR_FPR); //since the final answer is a floating point value, we want to make sure that
                                              //the VSX register that is allocated is also an FPR (i.e.: one of VSX 0-31), so
                                              //that we can avoid having to copy the final answer into an FPR at the end
      tempGPR = NULL; //not used for TR::Float
      resReg = tempVSX;
   }
   else
   {
      TR_ASSERT_FATAL(false, "cannot call vreductionAddWordHelper on vector type %s\n", type.toString());
      return NULL;
   }

   node->setRegister(resReg);

   //set shift amount to -32 bits. Since only the lowest 6 bits of shiftReg are used by vrld as an unsigned integer, this will result in a shift of 32 bits
   generateTrg1ImmInstruction(cg, TR::InstOpCode::vspltisw, node, shiftReg, -16);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, shiftReg, shiftReg, shiftReg);

   //Evaluate sum across vector operand by rotating and performing lanewise addition
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vrld, node, temp, srcReg, shiftReg);
   generateTrg1Src2Instruction(cg, addOp, node, tempVSX, srcReg, temp);
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, temp, tempVSX, tempVSX, 2);
   generateTrg1Src2Instruction(cg, addOp, node, tempVSX, tempVSX, temp);

   //Copy result into GPR if data type is Integer, convert to double precision format if data type is Float
   if (type == TR::Int32)
   {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, tempGPR, tempVSX);
      cg->stopUsingRegister(tempVSX);
   }
   else // type === TR::Float
   {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::xscvspdpn, node, tempVSX, tempVSX);
   }

   cg->stopUsingRegister(shiftReg);
   cg->stopUsingRegister(temp);
   cg->decReferenceCount(firstChild);

   return resReg;
   }

static TR::Register *vreductionAddDoubleWordHelper(TR::Node *node, TR::CodeGenerator *cg, TR::DataType type)
   {
   TR::Node *firstChild = node->getFirstChild();

   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *resReg;

   TR::Register *tempGPR;
   TR::Register *tempVSX;

   //choose addOp and allocate registers based on data type
   TR::InstOpCode::Mnemonic addOp;

   if (type == TR::Int64)
   {
      addOp = TR::InstOpCode::vaddudm;
      tempVSX = cg->allocateRegister(TR_VRF);
      tempGPR = cg->allocateRegister();
      resReg = tempGPR;
   }
   else if (type == TR::Double)
   {
      addOp = TR::InstOpCode::xvadddp;
      tempVSX = cg->allocateRegister(TR_FPR); //since the final answer is a floating point value, we want to make sure that
                                              //the VSX register that is allocated is also an FPR (i.e.: one of VSX 0-31), so
                                              //that we can avoid having to copy the final answer into an FPR at the end
      tempGPR = NULL; //not used for TR::Double
      resReg = tempVSX;
   }
   else
   {
      TR_ASSERT_FATAL(false, "cannot call vreductionAddDoubleWordHelper on vector type %s\n", type.toString());
      return NULL;
   }

   node->setRegister(resReg);

   //Evaluate sum across vector operand by rotating and performing lanewise addition
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, tempVSX, srcReg, srcReg, 2);
   generateTrg1Src2Instruction(cg, addOp, node, tempVSX, srcReg, tempVSX);

   //Copy result into GPR if data type is Long Integer
   if (type == TR::Int64)
   {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, tempGPR, tempVSX);
      cg->stopUsingRegister(tempVSX);
   }

   cg->decReferenceCount(firstChild);

   return resReg;
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionAddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getNumChildren() == 1, "vreductionAdd node should have exactly one child");

   TR::Node *firstChild = node->getFirstChild();
   TR::DataType type = firstChild->getDataType().getVectorElementType();

   TR_ASSERT_FATAL_WITH_NODE(node, firstChild->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", firstChild->getDataType().toString());

   switch(type)
     {
     case TR::Int8:
       return vreductionAddSubWordHelper(node, cg, TR::InstOpCode::vsum4sbs); //since the results will never overflow, it is safe to use a saturate sum instruction here
     case TR::Int16:
       return vreductionAddSubWordHelper(node, cg, TR::InstOpCode::vsum4shs); //since the results will never overflow, it is safe to use a saturate sum instruction here
     case TR::Int32:
       return vreductionAddWordHelper(node, cg, type);
     case TR::Int64:
       return vreductionAddDoubleWordHelper(node, cg, type);
     case TR::Float:
       return vreductionAddWordHelper(node, cg, type);
     case TR::Double:
       return vreductionAddDoubleWordHelper(node, cg, type);
     default:
       TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", firstChild->getDataType().toString()); return NULL;
     }
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionAndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionFirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionMinEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionOrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionOrUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreductionXorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vbitselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBitSelectOp(node, cg, TR::InstOpCode::xxsel);
   }

TR::Register*
OMR::Power::TreeEvaluator::vcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::DataType elementType = node->getDataType().getVectorElementType();

   switch (elementType)
      {
      case TR::Int32:
         return TR::TreeEvaluator::visetelemHelper(node, cg);
      case TR::Double:
         return TR::TreeEvaluator::vdsetelemHelper(node, cg);
      default:
         return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
      }
   }

TR::Register*
OMR::Power::TreeEvaluator::vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::d2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::d2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iuEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2iEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::NewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::newvalueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::newarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::anewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::variableNewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::variableNewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::multianewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::contigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::discontigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::icalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::acalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::calliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::luaddhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iaddEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::laddEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lusubhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::imulhEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lmulhEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::CaseEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ResolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::OverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::UnsignedOverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::SpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::long2StringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bitOpMemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::allocationFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::loadFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::storeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fullFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::butestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sutestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::icmpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::scmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::icmpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::icmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iucmpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ificmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ificmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iflcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iflcmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ificmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ificmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iflcmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iflcmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifxcmpoEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iuaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::luaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::laddEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lsubEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::icmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmpsetEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lcmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmpsetEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bztestnsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ibatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::isatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iiatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ilatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::branchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ffloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dfloorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dfloorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dfloorEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::maxEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::iuminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::luminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::fminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::dminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ihbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerHighestOneBit(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ilbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerLowestOneBit(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::inolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNumberOfLeadingZeros(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::inotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNumberOfTrailingZeros(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ipopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerBitCount(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lhbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longHighestOneBit(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::llbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longLowestOneBit(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longNumberOfLeadingZeros(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longNumberOfTrailingZeros(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::longBitCount(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::bbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::sbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::ibitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::Power::TreeEvaluator::lbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;

#define MAX_PPC_ARRAYCOPY_INLINE 256

static inline bool alwaysInlineArrayCopy(TR::CodeGenerator *cg)
   {
   return false;
   }

void loadFloatConstant(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic loadOp, TR::Node *node, TR::DataType type, void *value, TR::Register *trgReg)
   {
   int8_t length;

   switch (type)
      {
      case TR::Float:
         length = 4;
         break;
      case TR::Double:
         length = 8;
         break;
      default:
         TR_ASSERT_FATAL_WITH_NODE(node, false, "Cannot call loadFloatConstant with data type %s", TR::DataType::getName(type));
      }

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      {
      TR::Instruction *loadInstr;
      TR::Register *tmpReg = NULL;

      // Since we're using Trg1Imm instructions (to allow patching of the PC-relative offset), we
      // have to use the prefixed form of the load without relying on MemoryReference expansion or
      // the backend will reject the instruction as invalid.
      switch (loadOp)
         {
         case TR::InstOpCode::lfs:
            loadInstr = generateTrg1ImmInstruction(cg, TR::InstOpCode::plfs, node, trgReg, 0);
            break;
         case TR::InstOpCode::lfd:
            loadInstr = generateTrg1ImmInstruction(cg, TR::InstOpCode::plfd, node, trgReg, 0);
            break;
         case TR::InstOpCode::lxvdsx:
            tmpReg = cg->allocateRegister();
            loadInstr = generateTrg1ImmInstruction(cg, TR::InstOpCode::paddi, node, tmpReg, 0);
            generateTrg1MemInstruction(cg, loadOp, node, trgReg, TR::MemoryReference::createWithIndexReg(cg, NULL, tmpReg, length));
            break;
         default:
            TR_ASSERT_FATAL_WITH_NODE(node, false, "Unhandled load instruction %s in loadFloatConstant", TR::InstOpCode(loadOp).getMnemonicName());
         }

      cg->findOrCreateFloatConstant(value, type, loadInstr, NULL, NULL, NULL);
      if (tmpReg)
         cg->stopUsingRegister(tmpReg);
      return;
      }
   else if (cg->comp()->target().is64Bit())
      {
      int32_t tocOffset;

      switch (type)
         {
         case TR::Float:
            tocOffset = TR_PPCTableOfConstants::lookUp(*reinterpret_cast<float*>(value), cg);
            break;
         case TR::Double:
            tocOffset = TR_PPCTableOfConstants::lookUp(*reinterpret_cast<double*>(value), cg);
            break;
         default:
            TR_ASSERT_FATAL_WITH_NODE(node, false, "Invalid data type %s in loadFloatConstant", TR::DataType::getName(type));
         }

      if (tocOffset != PTOC_FULL_INDEX)
         {
         TR::Register *tmpReg = NULL;
         TR::MemoryReference *memRef;
         if (tocOffset < LOWER_IMMED || tocOffset > UPPER_IMMED)
            {
            tmpReg = cg->allocateRegister();

            TR_ASSERT_FATAL_WITH_NODE(node, 0x00008000 != HI_VALUE(tocOffset), "TOC offset (0x%x) is unexpectedly high. Can not encode upper 16 bits into an addis instruction.", tocOffset);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, tmpReg, cg->getTOCBaseRegister(), HI_VALUE(tocOffset));
            memRef = TR::MemoryReference::createWithDisplacement(cg, tmpReg, LO_VALUE(tocOffset), length);
            }
         else
            {
            memRef = TR::MemoryReference::createWithDisplacement(cg, cg->getTOCBaseRegister(), tocOffset, length);
            }

         // TODO(#5383): We don't yet have a property flag to determine whether a load is
         //              indexed-form or not. For the time being, we only handle lxvdsx, as it's
         //              the only indexed-form load that can get here.
         if (loadOp == TR::InstOpCode::lxvdsx)
            memRef->forceIndexedForm(node, cg);

         generateTrg1MemInstruction(cg, loadOp, node, trgReg, memRef);

         if (tmpReg)
            cg->stopUsingRegister(tmpReg);

         return;
         }
      }

   TR::Register *srcReg = cg->allocateRegister();
   TR::Register *tmpReg = cg->comp()->target().is64Bit() ? cg->allocateRegister() : NULL;

   TR::Instruction *q[4];

   fixedSeqMemAccess(cg, node, 0, q, trgReg, srcReg, loadOp, length, NULL, tmpReg);
   cg->findOrCreateFloatConstant(value, type, q[0], q[1], q[2], q[3]);

   cg->stopUsingRegister(srcReg);
   if (tmpReg)
      cg->stopUsingRegister(tmpReg);
   }

TR::Instruction *loadAddressConstantInSnippet(TR::CodeGenerator *cg, TR::Node * node, intptr_t address, TR::Register *trgReg, TR::Register *tempReg, TR::InstOpCode::Mnemonic opCode, bool isUnloadablePicSite, TR::Instruction *cursor)
   {
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      {
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::Op_pload, node, trgReg, 0, cursor);
      cg->findOrCreateAddressConstant(&address, TR::Address, cursor, NULL, NULL, NULL, node, isUnloadablePicSite);
      }
   else
      {
      TR::Instruction *q[4];

      bool isTmpRegLocal = false;
      if (!tempReg && cg->comp()->target().is64Bit())
         {
         tempReg = cg->allocateRegister();
         isTmpRegLocal = true;
         }

      cursor = fixedSeqMemAccess(cg, node, 0, q, trgReg,  trgReg, opCode, sizeof(intptr_t), cursor, tempReg);
      cg->findOrCreateAddressConstant(&address, TR::Address, q[0], q[1], q[2], q[3], node, isUnloadablePicSite);

      if (isTmpRegLocal)
         cg->stopUsingRegister(tempReg);
      }

   return cursor;
   }


// loadAddressConstant could be merged with loadConstant 64-bit
TR::Instruction *
loadAddressConstant(
      TR::CodeGenerator *cg,
      bool isRelocatable,
      TR::Node * node,
      intptr_t value,
      TR::Register *trgReg,
      TR::Instruction *cursor,
      bool isPicSite,
      int16_t typeAddress)
   {
   if (isRelocatable)
      return cg->loadAddressConstantFixed(node, value, trgReg, cursor, NULL, typeAddress);

   return loadActualConstant(cg, node, value, trgReg, cursor, isPicSite);
   }


// loadAddressConstant could be merged with loadConstant 64-bit
TR::Instruction *
loadAddressConstant(
      TR::CodeGenerator *cg,
      TR::Node * node,
      intptr_t value,
      TR::Register *trgReg,
      TR::Instruction *cursor,
      bool isPicSite,
      int16_t typeAddress)
   {
   if (cg->comp()->compileRelocatableCode())
      return cg->loadAddressConstantFixed(node, value, trgReg, cursor, NULL, typeAddress);

   return loadActualConstant(cg, node, value, trgReg, cursor, isPicSite);
   }


TR::Instruction *loadActualConstant(TR::CodeGenerator *cg, TR::Node * node, intptr_t value, TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite)
   {
   if (cg->comp()->target().is32Bit())
      return loadConstant(cg, node, (int32_t)value, trgReg, cursor, isPicSite);

   return loadConstant(cg, node, (int64_t)value, trgReg, cursor, isPicSite);
   }


TR::Instruction *loadConstant(TR::CodeGenerator *cg, TR::Node * node, int32_t value, TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite)
   {
   TR::Compilation *comp = cg->comp();
   intParts localVal(value);
   int32_t ulit, llit;
   TR::Instruction *temp = cursor;

   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   if ((LOWER_IMMED <= localVal.getValue()) && (localVal.getValue() <= UPPER_IMMED))
      {
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, localVal.getValue(), cursor);
      }
   else
      {
      ulit = localVal.getHighBitsSigned();
      llit = localVal.getLowBits();
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, trgReg, ulit, cursor);
      if (llit != 0 || isPicSite)
         {
         if (isPicSite)
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, value, cursor);
         else
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, llit, cursor);
         }
      }

   if (isPicSite)
      {
      // If the node is a call then value must be a class pointer (interpreter profiling devirtualization)
      if (node->getOpCode().isCall() || node->isClassPointerConstant())
         comp->getStaticPICSites()->push_front(cursor);
      else if (node->isMethodPointerConstant())
         comp->getStaticMethodPICSites()->push_front(cursor);
      }

   if (temp == NULL)
      cg->setAppendInstruction(cursor);

   return(cursor);
   }

TR::Instruction *loadConstant(TR::CodeGenerator *cg, TR::Node * node, int64_t value, TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite, bool useTOC)
   {
   // When loading 64-bit constants in 32-bit builds, we should not be using this function. This
   // function assumes that 64-bit instructions are available and that intptr_t and int64_t are
   // interchangeable.
   TR_ASSERT_FATAL_WITH_NODE(node, cg->comp()->target().is64Bit(), "Should not use 64-bit loadConstant on 32-bit builds");

   if ((TR::getMinSigned<TR::Int32>() <= value) && (value <= TR::getMaxSigned<TR::Int32>()))
      {
      return loadConstant(cg, node, (int32_t)value, trgReg, cursor, isPicSite);
      }

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      return loadAddressConstantInSnippet(cg, node, value, trgReg, NULL, TR::InstOpCode::ld, isPicSite, cursor);

   TR::Compilation *comp = cg->comp();
   TR::Instruction *temp = cursor;

   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   int32_t offset = PTOC_FULL_INDEX;

   bool canUseTOC = (!TR::isJ9() || !isPicSite);
   if (canUseTOC && useTOC)
      offset = TR_PPCTableOfConstants::lookUp((int8_t *)&value, sizeof(int64_t), true, 0, cg);

   if (offset != PTOC_FULL_INDEX)
      {
      offset *= TR::Compiler->om.sizeofReferenceAddress();
      TR_PPCTableOfConstants::setTOCSlot(offset, value);
      if (offset < LOWER_IMMED || offset > UPPER_IMMED)
         {
         TR_ASSERT_FATAL_WITH_NODE(node, 0x00008000 != cg->hiValue(offset), "TOC offset (0x%x) is unexpectedly high. Can not encode upper 16 bits into an addis instruction.", offset);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, trgReg, cg->getTOCBaseRegister(), cg->hiValue(offset), cursor);
         cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, trgReg, TR::MemoryReference::createWithDisplacement(cg, trgReg, LO_VALUE(offset), 8), cursor);
         }
      else
         {
         cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, trgReg, TR::MemoryReference::createWithDisplacement(cg, cg->getTOCBaseRegister(), offset, 8), cursor);
         }
      }
   else
      {
      // Could not use TOC.

      // Break up 64bit value into 4 16bit ones.
      int32_t hhval, hlval, lhval, llval;
      hhval = value >> 48;
      hlval = (value>>32) & 0xffff;
      lhval = (value>>16) & 0xffff;
      llval = value & 0xffff;

      // Load the upper 32 bits.
      if ((hhval == -1 && (hlval & 0x8000) != 0) ||
          (hhval == 0 && (hlval & 0x8000) == 0))
         {
         cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, static_cast<int16_t>(hlval), cursor);
         }
      else
         {
         // lis trgReg, upper 16-bits
         cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, trgReg, static_cast<int16_t>(hhval) , cursor);
         // ori trgReg, trgReg, next 16-bits
         if (hlval != 0)
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, hlval, cursor);
         }

      // Shift upper 32 bits into place.
      // shiftli trgReg, trgReg, 32
      cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, trgReg, trgReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);

      // oris trgReg, trgReg, next 16-bits
      if (lhval != 0)
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, trgReg, trgReg, lhval, cursor);

      // ori trgReg, trgReg, last 16-bits
      if (llval != 0 || isPicSite)
         {
         if (isPicSite)
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, value, cursor);
         else
            cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, llval, cursor);
         }

      if (isPicSite)
         {
         // If the node is a call then value must be a class pointer (interpreter profiling devirtualization)
         if (node->getOpCode().isCall() || node->isClassPointerConstant())
            comp->getStaticPICSites()->push_front(cursor);
         else if (node->isMethodPointerConstant())
            comp->getStaticMethodPICSites()->push_front(cursor);
         }
      }

   if (temp == NULL)
      cg->setAppendInstruction(cursor);

   return cursor;
   }

TR::Instruction *fixedSeqMemAccess(TR::CodeGenerator *cg, TR::Node *node, intptr_t addr, TR::Instruction **nibbles, TR::Register *srcOrTrg, TR::Register *baseReg, TR::InstOpCode::Mnemonic opCode, int32_t opSize, TR::Instruction *cursor, TR::Register *tempReg)
   {
   // Note: tempReg can be the same register as srcOrTrg. Caller needs to make sure it is right.
   TR::Instruction *cursorCopy = cursor;
   TR::InstOpCode    op(opCode);
   intptr_t       hiAddr = cg->hiValue(addr);
   intptr_t       loAddr = LO_VALUE(addr);
   int32_t         idx;
   TR::Compilation *comp = cg->comp();

   nibbles[2] = nibbles[3] = NULL;
   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   if (cg->comp()->target().is32Bit())
      {
      nibbles[0] = cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, baseReg, (int16_t)hiAddr, cursor);
      idx = 1;
      }
   else
      {
      if (tempReg == NULL)
         {
         nibbles[0] = cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, baseReg, (int16_t)(hiAddr>>32), cursor);
         nibbles[1] = cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, baseReg, baseReg, (hiAddr>>16)&0x0000FFFF, cursor);
         cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, baseReg, baseReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
         nibbles[2] = cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, baseReg, baseReg, hiAddr&0x0000FFFF, cursor);
         }
      else
         {
         nibbles[0] = cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, tempReg, (int16_t)(hiAddr>>32), cursor);
         nibbles[2] = cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, baseReg, (int16_t)hiAddr, cursor);
         nibbles[1] = cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tempReg, tempReg, (hiAddr>>16)&0x0000FFFF, cursor);
         cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, baseReg, tempReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
         }
      idx = 3;
      }

   // Can be used for any x-form instruction actually, not just lxvdsx
   if (opCode == TR::InstOpCode::lxvdsx)
      {
      TR::MemoryReference *memRef;
      if (cg->comp()->target().is64Bit() && tempReg)
         {
         nibbles[idx] = cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, ((int32_t)loAddr)<<16>>16, cursor);
         memRef = TR::MemoryReference::createWithIndexReg(cg, baseReg, tempReg, opSize);
         }
      else
         {
         nibbles[idx] = cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, baseReg, baseReg, ((int32_t)loAddr)<<16>>16, cursor);
         memRef = TR::MemoryReference::createWithIndexReg(cg, NULL, baseReg, opSize);
         }
      cursor = generateTrg1MemInstruction(cg, opCode, node, srcOrTrg, memRef, cursor);
      }
   else
      {
      TR::MemoryReference *memRef = TR::MemoryReference::createWithDisplacement(cg, baseReg, ((int32_t)loAddr)<<16>>16, opSize);
      if (!op.isStore())
         nibbles[idx] = cursor = generateTrg1MemInstruction(cg, opCode, node, srcOrTrg, memRef, cursor);
      else
         nibbles[idx] = cursor = generateMemSrc1Instruction(cg, opCode, node, memRef, srcOrTrg, cursor);
      }

   // When using tempReg ensure no register spills occur in the middle of the fixed sequence
   if (tempReg)
      {
      TR::RegisterDependencyConditions *dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg->trMemory());
      if (srcOrTrg != tempReg && srcOrTrg != baseReg)
         dep->addPostCondition(srcOrTrg, TR::RealRegister::NoReg);
      dep->addPostCondition(tempReg, TR::RealRegister::NoReg);
      dep->addPostCondition(baseReg, TR::RealRegister::NoReg, UsesDependentRegister | ExcludeGPR0InAssigner);

      cursor = generateDepLabelInstruction(cg, TR::InstOpCode::label, node, TR::LabelSymbol::create(cg->trHeapMemory(),cg), dep);
      }

   if (cursorCopy == NULL)
      cg->setAppendInstruction(cursor);

   return cursor;
   }



// Also handles iloadi
TR::Register *OMR::Power::TreeEvaluator::iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;

   trgReg = cg->allocateRegister();
   if (node->getSymbol()->isInternalPointer())
      {
      trgReg->setPinningArrayPointer(node->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      trgReg->setContainsInternalPointer();
      }

   TR::LoadStoreHandler::generateLoadNodeSequence(cg, trgReg, node, TR::InstOpCode::lwz, 4);
   node->setRegister(trgReg);
   return trgReg;
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

// Also handles aloadi
TR::Register *OMR::Power::TreeEvaluator::aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   // NEW to check for it feeding to a single vgnop
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
               (checkNode->isNopableInlineGuard() || checkNode->isHCRGuard() || checkNode->isOSRGuard()) &&
               (checkNode->getOpCodeValue() == TR::ificmpne || checkNode->getOpCodeValue() == TR::iflcmpne ||
                checkNode->getOpCodeValue() == TR::ifacmpne) &&
                checkNode->getFirstChild() == node)
               {
               // Try get the virtual guard
               TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(checkNode);
               if (!((comp->performVirtualGuardNOPing() || node->isHCRGuard() || node->isOSRGuard()) &&
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

   TR::Register *trgReg;

   if (!node->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      if (node->getSymbolReference()->getSymbol()->isNotCollected())
         trgReg = cg->allocateRegister();
      else
         trgReg = cg->allocateCollectedReferenceRegister();
      }
   else
      {
      trgReg = cg->allocateRegister();
      trgReg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      trgReg->setContainsInternalPointer();
      }

   // TODO Will this cause problems with evaluators that will inline aload and aloadi nodes?
#ifdef J9_PROJECT_SPECIFIC
   bool isCompressedVft = TR::Compiler->om.generateCompressedObjectHeaders() && node->getSymbol()->isClassObject() && node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef();
#else
   bool isCompressedVft = false;
#endif

   if (cg->comp()->target().is64Bit() && !isCompressedVft)
      TR::LoadStoreHandler::generateLoadNodeSequence(cg, trgReg, node, TR::InstOpCode::ld, 8);
   else
      TR::LoadStoreHandler::generateLoadNodeSequence(cg, trgReg, node, TR::InstOpCode::lwz, 4);

#ifdef J9_PROJECT_SPECIFIC
   if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, trgReg);
#endif

   node->setRegister(trgReg);
   return trgReg;
   }

// Also handles lloadi
TR::Register *OMR::Power::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;

   if (cg->comp()->target().is64Bit())
      {
      trgReg = cg->allocateRegister();
      TR::LoadStoreHandler::generateLoadNodeSequence(cg, trgReg, node, TR::InstOpCode::ld, 8);
      }
   else
      {
      trgReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      TR::LoadStoreHandler::generatePairedLoadNodeSequence(cg, trgReg, node);
      }

   node->setRegister(trgReg);
   return trgReg;
   }

// Also handles bloadi
TR::Register *OMR::Power::TreeEvaluator::bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::LoadStoreHandler::generateLoadNodeSequence(cg, trgReg, node, TR::InstOpCode::lbz, 1);

   node->setRegister(trgReg);
   return trgReg;
   }

// Also handles sloadi
TR::Register *OMR::Power::TreeEvaluator::sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::LoadStoreHandler::generateLoadNodeSequence(cg, trgReg, node, TR::InstOpCode::lha, 2);

   node->setRegister(trgReg);
   return trgReg;
   }

// Also handles istorei
TR::Register *OMR::Power::TreeEvaluator::istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *valueChild;

   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   bool reverseStore = false;
   // TODO(#5684): Re-enable once issues with delayed indexed-form are corrected
   static bool reverseStoreEnabled = feGetEnv("TR_EnableReverseLoadStore");
   if (reverseStoreEnabled && valueChild->getOpCodeValue() == TR::ibyteswap &&
      valueChild->getReferenceCount() == 1 &&
      valueChild->getRegister() == NULL)
      {
      reverseStore = true;

      cg->decReferenceCount(valueChild);
      valueChild = valueChild->getFirstChild();
      }

   // Handle special cases
   //
   if (!reverseStore &&
       valueChild->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1)
      {
      // Special case storing a float value into an int variable
      //
      if (valueChild->getOpCodeValue() == TR::fbits2i &&
          !valueChild->normalizeNanValues())
         {
         TR::LoadStoreHandler::generateStoreNodeSequence(cg, cg->evaluate(valueChild->getFirstChild()), node, TR::InstOpCode::stfs, 4);
         cg->decReferenceCount(valueChild->getFirstChild());
         cg->decReferenceCount(valueChild);
         return NULL;
         }
      }

   TR::Register *valueReg = cg->evaluate(valueChild);

   if (reverseStore)
      TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::stwbrx, 4, true);
   else
      TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::stw, 4);

   cg->decReferenceCount(valueChild);
   if (comp->useCompressedPointers() && node->getOpCode().isIndirect())
      node->setStoreAlreadyEvaluated(true);
   return NULL;
   }

// Also handles astorei
TR::Register *OMR::Power::TreeEvaluator::astoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *valueChild;

   if (node->getOpCode().isIndirect())
      valueChild = node->getSecondChild();
   else
      valueChild = node->getFirstChild();

   TR::Register *valueReg = cg->evaluate(valueChild);

#ifdef J9_PROJECT_SPECIFIC
   bool isCompressedVft = TR::Compiler->om.generateCompressedObjectHeaders() && (node->getSymbol()->isClassObject() || node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef());
#else
   bool isCompressedVft = false;
#endif

   if (comp->target().is64Bit() && !isCompressedVft)
      TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::std, 8);
   else
      TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::stw, 4);

   cg->decReferenceCount(valueChild);
   return NULL;
   }


// Also handles lstorei
TR::Register *OMR::Power::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   bool reverseStore = false;
   // TODO(#5684): Re-enable once issues with delayed indexed-form are corrected
   static bool reverseStoreEnabled = feGetEnv("TR_EnableReverseLoadStore");
   if (reverseStoreEnabled && valueChild->getOpCodeValue() == TR::lbyteswap &&
      valueChild->getReferenceCount() == 1 &&
      valueChild->getRegister() == NULL &&
      cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P7))
      {
      reverseStore = true;

      cg->decReferenceCount(valueChild);
      valueChild = valueChild->getFirstChild();
      }

   // Handle special cases
   //
   if (!reverseStore &&
       valueChild->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1)
      {
      // Special case storing a double value into a long variable
      //
      if (valueChild->getOpCodeValue() == TR::dbits2l &&
          !valueChild->normalizeNanValues())
         {
         TR::LoadStoreHandler::generateStoreNodeSequence(cg, cg->evaluate(valueChild->getFirstChild()), node, TR::InstOpCode::stfd, 8);
         cg->decReferenceCount(valueChild->getFirstChild());
         cg->decReferenceCount(valueChild);
         return NULL;
         }
      }

   TR::Register *valueReg = cg->evaluate(valueChild);

   if (comp->target().is64Bit())
      {
      if (reverseStore)
         TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::stdbrx, 8, true);
      else
         TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::std, 8);
      }
   else
      {
      TR::LoadStoreHandler::generatePairedStoreNodeSequence(cg, valueReg, node);
      }

   cg->decReferenceCount(valueChild);
   return NULL;
   }

// Also handles bstorei
TR::Register *OMR::Power::TreeEvaluator::bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }
   if ((valueChild->getOpCodeValue()==TR::i2b   ||
        valueChild->getOpCodeValue()==TR::s2b) &&
       valueChild->getReferenceCount()==1 && valueChild->getRegister()==NULL)
      {
      cg->decReferenceCount(valueChild);
      valueChild = valueChild->getFirstChild();
      }

   TR::Register *valueReg = cg->evaluate(valueChild);
   TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::stb, 1);

   cg->decReferenceCount(valueChild);
   return NULL;
   }

// also handles isstore
TR::Register *OMR::Power::TreeEvaluator::sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   bool reverseStore = false;
   // TODO(#5684): Re-enable once issues with delayed indexed-form are corrected
   static bool reverseStoreEnabled = feGetEnv("TR_EnableReverseLoadStore");
   if (reverseStoreEnabled && valueChild->getOpCodeValue() == TR::sbyteswap &&
      valueChild->getReferenceCount() == 1 &&
      valueChild->getRegister() == NULL)
      {
      reverseStore = true;

      cg->decReferenceCount(valueChild);
      valueChild = valueChild->getFirstChild();
      }

   if ((valueChild->getOpCodeValue()==TR::i2s) &&
       valueChild->getReferenceCount()==1 && valueChild->getRegister()==NULL)
      {
      cg->decReferenceCount(valueChild);
      valueChild = valueChild->getFirstChild();
      }

   TR::Register *valueReg = cg->evaluate(valueChild);

   if (reverseStore)
      TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::sthbrx, 2, true);
   else
      TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, TR::InstOpCode::sth, 2);

   cg->decReferenceCount(valueChild);
   return NULL;
   }


TR::Register *OMR::Power::TreeEvaluator::vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic opcode;
   TR_RegisterKinds kind;

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
         opcode = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9) ? TR::InstOpCode::lxvb16x : TR::InstOpCode::lxvw4x;
         kind = TR_VRF;
         break;
     case TR::Int16:
         opcode = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9) ? TR::InstOpCode::lxvh8x : TR::InstOpCode::lxvw4x;
         kind = TR_VRF;
         break;
     case TR::Int32:
     case TR::Float:
	 opcode = TR::InstOpCode::lxvw4x;
	 kind = TR_VRF;
	 break;
     case TR::Int64:
	 opcode = TR::InstOpCode::lxvd2x;
	 kind = TR_VRF;
         break;
     case TR::Double:
	 opcode = TR::InstOpCode::lxvd2x;
	 kind = TR_VSX_VECTOR;
	 break;
     default:
	 TR_ASSERT(false, "unknown vector load TRIL: unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }

   TR::Register *dstReg = cg->allocateRegister(kind);

   TR::LoadStoreHandler::generateLoadNodeSequence(cg, dstReg, node, opcode, 16, true);

   node->setRegister(dstReg);
   return dstReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s lenght:%d", node->getDataType().toString(), node->getDataType().getVectorLength());

   TR::InstOpCode::Mnemonic opcode;

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
         opcode = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9) ? TR::InstOpCode::stxvb16x : TR::InstOpCode::stxvw4x;
         break;
     case TR::Int16:
         opcode = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9) ? TR::InstOpCode::stxvh8x : TR::InstOpCode::stxvw4x;
         break;
     case TR::Int32:
     case TR::Float:
         opcode = TR::InstOpCode::stxvw4x;
         break;
     case TR::Int64:
     case TR::Double:
         opcode = TR::InstOpCode::stxvd2x;
         break;
     default:
         TR_ASSERT(false, "unknown vector store TRIL: unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }

   TR::Node *valueChild = node->getOpCode().isStoreDirect() ? node->getFirstChild() : node->getSecondChild();
   TR::Register *valueReg = cg->evaluate(valueChild);

   TR::LoadStoreHandler::generateStoreNodeSequence(cg, valueReg, node, opcode, 16, true);

   cg->decReferenceCount(valueChild);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::astoreEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::astoreEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::inlineVectorUnaryOp(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op) {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = NULL;

   srcReg = cg->evaluate(firstChild);

   TR::Register *resReg = cg->allocateRegister(TR_VRF);
   node->setRegister(resReg);
   generateTrg1Src1Instruction(cg, op, node, resReg, srcReg);
   cg->decReferenceCount(firstChild);
   return resReg;
}


TR::Register *OMR::Power::TreeEvaluator::inlineVectorBinaryOp(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op) {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = NULL;
   TR::Register *lhsReg = NULL, *rhsReg = NULL, *maskReg = NULL;
   bool masked = false;

   lhsReg = cg->evaluate(firstChild);
   rhsReg = cg->evaluate(secondChild);

   if (node->getOpCode().isVectorMasked())
      {
      masked = true;
      thirdChild = node->getThirdChild();
      maskReg = cg->evaluate(thirdChild);
      }

   if (TR::InstOpCode(op).isVMX())
      {
      TR_ASSERT(lhsReg->getKind() == TR_VRF, "unexpected Register kind\n");
      TR_ASSERT(rhsReg->getKind() == TR_VRF, "unexpected Register kind\n");
      }

   TR::Register *resReg;
   if (!TR::InstOpCode(op).isVMX())
      resReg = cg->allocateRegister(TR_VSX_VECTOR);
   else
      resReg = cg->allocateRegister(TR_VRF);

   generateTrg1Src2Instruction(cg, op, node, resReg, lhsReg, rhsReg);

   if (masked)
      {
      // Note: Both VFR and VSX registers are supported by xxsel
      generateTrg1Src3Instruction(cg, TR::InstOpCode::xxsel, node, resReg, lhsReg, resReg, maskReg);
      }

   node->setRegister(resReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   if (masked)
      cg->decReferenceCount(thirdChild);

   return resReg;
}

TR::Register *OMR::Power::TreeEvaluator::inlineVectorBitSelectOp(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op) {
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild  = node->getThirdChild();

   TR::Register *thirdReg  = cg->evaluate(thirdChild);
   TR::Register *firstReg  = cg->evaluate(firstChild);
   TR::Register *secondReg = cg->evaluate(secondChild);

   TR::Register *resReg;

   if (op != TR::InstOpCode::xvmaddadp &&
       op != TR::InstOpCode::xvmsubadp &&
       op != TR::InstOpCode::xvnmsubadp)
      {
      resReg = cg->allocateRegister(TR_VSX_VECTOR);
      generateTrg1Src3Instruction(cg, op, node, resReg, firstReg, secondReg, thirdReg);
      }
   else
      {
      if (cg->canClobberNodesRegister(thirdChild))
      	 {
         resReg = thirdReg;
      	 }
      else
      	 {
         resReg = cg->allocateRegister(TR_VSX_VECTOR);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, thirdReg, thirdReg);
      	 }
      generateTrg1Src2Instruction(cg, op, node, resReg, firstReg, secondReg);
      }

      node->setRegister(resReg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      cg->decReferenceCount(thirdChild);
      return resReg;
}

TR::Register *OMR::Power::TreeEvaluator::vandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::bad;

   switch (node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         opCode = TR::InstOpCode::vand;
         break;
      default:
         opCode = TR::InstOpCode::xxland;
         break;
     }

   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, opCode);
   }

TR::Register *OMR::Power::TreeEvaluator::vorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::bad;

   switch (node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         opCode = TR::InstOpCode::vor;
         break;
      default:
         opCode = TR::InstOpCode::xxlor;
         break;
     }
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, opCode);
   }

TR::Register *OMR::Power::TreeEvaluator::vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::bad;

   switch (node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         opCode = TR::InstOpCode::vxor;
         break;
      default:
         opCode = TR::InstOpCode::xxlxor;
         break;
     }
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, opCode);
   }

TR::Register *OMR::Power::TreeEvaluator::vnotEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = NULL;

   srcReg = cg->evaluate(firstChild);

   TR::Register *resReg = cg->allocateRegister(TR_VRF);
   node->setRegister(resReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vnor, node, resReg, srcReg, srcReg);
   cg->decReferenceCount(firstChild);
   return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static bool disableDirectMove = feGetEnv("TR_disableDirectMove") ? true : false;
   if (!disableDirectMove && cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8) && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX))
      {
      return TR::TreeEvaluator::vgetelemDirectMoveHelper(node, cg);
      }
   else
      {
      return TR::TreeEvaluator::vgetelemMemoryMoveHelper(node, cg); //transfers data through memory instead of via direct move instructions.
      }
   }

TR::Register *OMR::Power::TreeEvaluator::vgetelemDirectMoveHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *resReg = 0;
   TR::Register *highResReg = 0;
   TR::Register *lowResReg = 0;
   TR::Register *srcVectorReg = cg->evaluate(firstChild);
   TR::Register *intermediateResReg = cg->allocateRegister(TR_VSX_VECTOR);
   TR::Register *tempVectorReg = cg->allocateRegister(TR_VSX_VECTOR);

   int32_t elementCount = -1;

   TR_ASSERT_FATAL_WITH_NODE(node, firstChild->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch (firstChild->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
         TR_ASSERT(false, "unsupported vector type %s in vgetelemEvaluator.\n", firstChild->getDataType().toString());
         break;
      case TR::Int32:
         elementCount = 4;
         resReg = cg->allocateRegister();
         break;
      case TR::Int64:
         elementCount = 2;
         if (cg->comp()->target().is32Bit())
            {
            highResReg = cg->allocateRegister();
            lowResReg = cg->allocateRegister();
            resReg = cg->allocateRegisterPair(lowResReg, highResReg);
            }
         else
            {
            resReg = cg->allocateRegister();
            }
         break;
      case TR::Float:
         elementCount = 4;
         resReg = cg->allocateSinglePrecisionRegister();
         break;
      case TR::Double:
         elementCount = 2;
         resReg = cg->allocateRegister(TR_FPR);
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s\n", firstChild->getDataType().toString());
      }

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t elem = secondChild->getInt();

      TR_ASSERT(elem >= 0 && elem < elementCount, "Element can only be 0 to %u\n", elementCount - 1);

      if (elementCount == 4)
         {
         /*
          * If an Int32 is being read out, it needs to be in element 1.
          * If a float is being read out, it needs to be in element 0.
          * A splat is used to put the data in the right slot by copying it to every slot.
          * The splat is skipped if the data is already in the right slot.
          * If the splat is skipped, the input data will be in srcVectorReg instead of intermediateResReg.
          */
         bool skipSplat = (firstChild->getDataType().getVectorElementType() == TR::Int32 && 1 == elem) ||
                          (firstChild->getDataType().getVectorElementType() == TR::Float && 0 == elem);

         if (!skipSplat)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, elem);
            }

         if (firstChild->getDataType().getVectorElementType() == TR::Int32)
            {
            generateMvFprGprInstructions(cg, node, fpr2gprLow, false, resReg, skipSplat ? srcVectorReg : intermediateResReg);
            }
         else // TR::Float
            {
            generateTrg1Src1Instruction(cg, TR::InstOpCode::xscvspdp, node, resReg, skipSplat ? srcVectorReg : intermediateResReg);
            }
         }
      else //elementCount == 2
         {
         /*
          * Element to read out needs to be in element 0
          * If reading element 1, xxsldwi is used to move the data to element 0
          */
         bool readElemOne = (1 == elem);
         if (readElemOne)
            {
            generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxsldwi, node, intermediateResReg, srcVectorReg, srcVectorReg, 0x2);
            }

         if (cg->comp()->target().is32Bit() && firstChild->getDataType().getVectorElementType() == TR::Int64)
            {
            generateMvFprGprInstructions(cg, node, fpr2gprHost32, false, highResReg, lowResReg, readElemOne ? intermediateResReg : srcVectorReg, tempVectorReg);
            }
         else if (firstChild->getDataType().getVectorElementType() == TR::Int64)
            {
            generateMvFprGprInstructions(cg, node, fpr2gprHost64, false, resReg, readElemOne ? intermediateResReg : srcVectorReg);
            }
         else // TR::Double
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, readElemOne ? intermediateResReg : srcVectorReg, readElemOne ? intermediateResReg : srcVectorReg);
            }
         }
      }
   else
      {
      TR::Register    *indexReg = cg->evaluate(secondChild);
      TR::Register    *condReg = cg->allocateRegister(TR_CCR);
      TR::LabelSymbol *jumpLabel0 = generateLabelSymbol(cg);
      TR::LabelSymbol *jumpLabel1 = generateLabelSymbol(cg);
      TR::LabelSymbol *jumpLabel23 = generateLabelSymbol(cg);
      TR::LabelSymbol *jumpLabel3 = generateLabelSymbol(cg);
      TR::LabelSymbol *jumpLabelDone = generateLabelSymbol(cg);

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
      deps->addPostCondition(srcVectorReg, TR::RealRegister::NoReg);
      deps->addPostCondition(intermediateResReg, TR::RealRegister::NoReg);
      deps->addPostCondition(indexReg, TR::RealRegister::NoReg);
      deps->addPostCondition(condReg, TR::RealRegister::NoReg);

      if (firstChild->getDataType().getVectorElementType() == TR::Int32)
         {
         /*
          * Conditional statements are used to determine if the indexReg has the value 0, 1, 2 or 3. Other values are invalid.
          * The element indicated by indexReg needs to be move to element 1 in intermediateResReg.
          * In most cases, this is done by splatting the indicated value into every element of intermediateResReg.
          * If the indicated value is already in element 1 of srcVectorReg, the vector is just copied to intermediateResReg.
          * Finally, a direct move instruction is used to move the data we want from element 1 of the vector into a GPR.
          */
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, indexReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, jumpLabel23, condReg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, jumpLabel0, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, intermediateResReg, srcVectorReg, srcVectorReg);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel0);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, 0x0);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel23);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, indexReg, 3);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel3, condReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, 0x2);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel3);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, 0x3);

         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabelDone, deps);
         generateMvFprGprInstructions(cg, node, fpr2gprLow, false, resReg, intermediateResReg);
         }
      else if (firstChild->getDataType().getVectorElementType() == TR::Float)
         {
         /*
          * Conditional statements are used to determine if the indexReg has the value 0, 1, 2 or 3. Other values are invalid.
          * The element indicated by indexReg needs to be move to element 0 in intermediateResReg.
          * In most cases, this is done by splatting the indicated value into every element of intermediateResReg.
          * If the indicated value is already in element 0 of srcVectorReg, the vector is just copied to intermediateResReg.
          * Finally, a direct move instruction is used to move the data we want from element 0 of the vector into an FPR.
          */
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, indexReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, jumpLabel23, condReg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel1, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, intermediateResReg, srcVectorReg, srcVectorReg);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel1);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, 0x1);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel23);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, indexReg, 3);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel3, condReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, 0x2);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel3);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, intermediateResReg, srcVectorReg, 0x3);

         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabelDone, deps);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::xscvspdp, node, resReg, intermediateResReg);
         }
      else
         {
         /*
          * Conditional statements are used to determine if the indexReg has the value 0 or 1. Other values are invalid.
          * The element indicated by indexReg needs to be move to element 0 in intermediateResReg.
          * If indexReg is 0, the vector is just copied to intermediateResReg.
          * If indexReg is 1, the vector is rotated so element 1 is moved to slot 0.
          * Finally, a direct move instruction is used to move the data we want from element 0 to the resReg.
          * 64 bit int on a 32 bit system is a special case where the result is returned in a pair register.
          */
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, indexReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel1, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, intermediateResReg, srcVectorReg, srcVectorReg);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, jumpLabelDone);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel1);
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxsldwi, node, intermediateResReg, srcVectorReg, srcVectorReg, 0x2);
         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabelDone, deps);

         if (cg->comp()->target().is32Bit() && firstChild->getDataType().getVectorElementType() == TR::Int64)
            {
            generateMvFprGprInstructions(cg, node, fpr2gprHost32, false, highResReg, lowResReg, intermediateResReg, tempVectorReg);
            }
         else if (firstChild->getDataType().getVectorElementType() == TR::Int64)
            {
            generateMvFprGprInstructions(cg, node, fpr2gprHost64, false, resReg, intermediateResReg);
            }
         else
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, intermediateResReg, intermediateResReg);
            }
         }
      cg->stopUsingRegister(condReg);
      }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      cg->stopUsingRegister(intermediateResReg);
      cg->stopUsingRegister(tempVectorReg);
      node->setRegister(resReg);
      return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vgetelemMemoryMoveHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *resReg = cg->allocateRegister();
   TR::Register *fResReg = 0;
   TR::Register *highResReg = 0;
   TR::Register *lowResReg = 0;

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   int32_t elementCount = 4;
   TR::InstOpCode::Mnemonic loadOpCode = TR::InstOpCode::lwz;
   TR::InstOpCode::Mnemonic vecStoreOpCode = TR::InstOpCode::stxvw4x;

   TR_ASSERT_FATAL_WITH_NODE(node, firstChild->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch (firstChild->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
         TR_ASSERT(false, "unsupported vector type %s in vgetelemEvaluator.\n", firstChild->getDataType().toString());
         break;
      case TR::Int32:
         elementCount = 4;
         loadOpCode = TR::InstOpCode::lwz;
         vecStoreOpCode = TR::InstOpCode::stxvw4x;
         break;
      case TR::Int64:
         elementCount = 2;
         if (cg->comp()->target().is64Bit())
            loadOpCode = TR::InstOpCode::ld;
         else
            loadOpCode = TR::InstOpCode::lwz;
         vecStoreOpCode = TR::InstOpCode::stxvd2x;
         break;
      case TR::Float:
         elementCount = 4;
         loadOpCode = TR::InstOpCode::lfs;
         fResReg = cg->allocateSinglePrecisionRegister();
         vecStoreOpCode = TR::InstOpCode::stxvw4x;
         break;
      case TR::Double:
         elementCount = 2;
         loadOpCode = TR::InstOpCode::lfd;
         fResReg = cg->allocateRegister(TR_FPR);
         vecStoreOpCode = TR::InstOpCode::stxvd2x;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s\n", firstChild->getDataType().toString());
      }

   TR::Register *vectorReg = cg->evaluate(firstChild);
   TR::SymbolReference *localTemp = cg->allocateLocalTemp(firstChild->getDataType());
   generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, resReg, TR::MemoryReference::createWithSymRef(cg, node, localTemp, 16));
   generateMemSrc1Instruction(cg, vecStoreOpCode, node, TR::MemoryReference::createWithIndexReg(cg, NULL, resReg, 16), vectorReg);

   if (secondChild->getOpCode().isLoadConst())
      {
      int elem = secondChild->getInt();

      TR_ASSERT(elem >= 0 && elem < elementCount, "Element can only be 0 to %u\n", elementCount - 1);

      if (firstChild->getDataType().getVectorElementType() == TR::Float ||
          firstChild->getDataType().getVectorElementType() == TR::Double)
         {
         generateTrg1MemInstruction(cg, loadOpCode, node, fResReg, TR::MemoryReference::createWithDisplacement(cg, resReg, elem * (16 / elementCount), 16 / elementCount));
         cg->stopUsingRegister(resReg);
         }
      else if (cg->comp()->target().is32Bit() &&
               firstChild->getDataType().getVectorElementType() == TR::Int64)
         {
         if (!cg->comp()->target().cpu.isLittleEndian())
            {
            highResReg = cg->allocateRegister();
            lowResReg = resReg;
            generateTrg1MemInstruction(cg, loadOpCode, node, highResReg, TR::MemoryReference::createWithDisplacement(cg, resReg, elem * 8, 4));
            generateTrg1MemInstruction(cg, loadOpCode, node, lowResReg, TR::MemoryReference::createWithDisplacement(cg, resReg, elem * 8 + 4, 4));
            }
         else
            {
            highResReg = resReg;
            lowResReg = cg->allocateRegister();
            generateTrg1MemInstruction(cg, loadOpCode, node, lowResReg, TR::MemoryReference::createWithDisplacement(cg, resReg, elem * 8, 4));
            generateTrg1MemInstruction(cg, loadOpCode, node, highResReg, TR::MemoryReference::createWithDisplacement(cg, resReg, elem * 8 + 4, 4));
            }
            resReg = cg->allocateRegisterPair(lowResReg, highResReg);
         }
      else
         {
         generateTrg1MemInstruction(cg, loadOpCode, node, resReg, TR::MemoryReference::createWithDisplacement(cg, resReg, elem * (16 / elementCount), 16 / elementCount));
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      if (firstChild->getDataType().getVectorElementType() == TR::Float ||
          firstChild->getDataType().getVectorElementType() == TR::Double)
         {
         node->setRegister(fResReg);
         return fResReg;
         }
      else
         {
         node->setRegister(resReg);
         return resReg;
         }
      }

   TR::Register *idxReg = cg->evaluate(secondChild);
   TR::Register *offsetReg = cg->allocateRegister();
   generateTrg1Src1ImmInstruction (cg, TR::InstOpCode::mulli, node, offsetReg, idxReg, 16 / elementCount);

   if (firstChild->getDataType().getVectorElementType() == TR::Float ||
       firstChild->getDataType().getVectorElementType() == TR::Double)
      {
      generateTrg1MemInstruction(cg, loadOpCode, node, fResReg, TR::MemoryReference::createWithIndexReg(cg, resReg, offsetReg, 16 / elementCount));
      cg->stopUsingRegister(resReg);
      }
   else if (cg->comp()->target().is32Bit() &&
            firstChild->getDataType().getVectorElementType() == TR::Int64)
      {
      if (!cg->comp()->target().cpu.isLittleEndian())
         {
         highResReg = cg->allocateRegister();
         lowResReg = resReg;
         generateTrg1MemInstruction(cg, loadOpCode, node, highResReg, TR::MemoryReference::createWithIndexReg(cg, resReg, offsetReg, 4));
         generateTrg1Src1ImmInstruction (cg, TR::InstOpCode::addi, node, offsetReg, offsetReg, 4);
         generateTrg1MemInstruction(cg, loadOpCode, node, lowResReg, TR::MemoryReference::createWithIndexReg(cg, resReg, offsetReg, 4));
         }
      else
         {
         highResReg = resReg;
         lowResReg = cg->allocateRegister();
         generateTrg1MemInstruction(cg, loadOpCode, node, lowResReg, TR::MemoryReference::createWithIndexReg(cg, resReg, offsetReg, 4));
         generateTrg1Src1ImmInstruction (cg, TR::InstOpCode::addi, node, offsetReg, offsetReg, 4);
         generateTrg1MemInstruction(cg, loadOpCode, node, highResReg, TR::MemoryReference::createWithIndexReg(cg, resReg, offsetReg, 4));
         }
         resReg = cg->allocateRegisterPair(lowResReg, highResReg);
      }
   else
      {
      generateTrg1MemInstruction(cg, loadOpCode, node, resReg, TR::MemoryReference::createWithIndexReg(cg, resReg, offsetReg, 16 / elementCount));
      }

   cg->stopUsingRegister(offsetReg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   if (firstChild->getDataType().getVectorElementType() == TR::Float ||
       firstChild->getDataType().getVectorElementType() == TR::Double)
      {
      node->setRegister(fResReg);
      return fResReg;
      }
   else
      {
      node->setRegister(resReg);
      return resReg;
      }

   }


TR::Register *OMR::Power::TreeEvaluator::visetelemHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();

   TR_ASSERT_FATAL_WITH_NODE(node, firstChild->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = node->getThirdChild();
   TR::Register *vectorReg = cg->evaluate(firstChild);
   TR::Register *valueReg = cg->evaluate(thirdChild);
   TR::Register *resReg = node->setRegister(cg->allocateRegister(TR_VRF));

   TR::Register *addrReg = cg->allocateRegister();
   TR::SymbolReference    *localTemp = cg->allocateLocalTemp(TR::DataType::createVectorType(TR::Int32, TR::VectorLength128));
   generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, addrReg, TR::MemoryReference::createWithSymRef(cg, node, localTemp, 16));
   generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, TR::MemoryReference::createWithIndexReg(cg, NULL, addrReg, 16), vectorReg);

   if (secondChild->getOpCode().isLoadConst())
      {
      int elem = secondChild->getInt();
      TR_ASSERT(elem >= 0 && elem <= 3, "Element can only be 0 to 3\n");

      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(cg, addrReg, elem * 4, 4), valueReg);

      }
   else
      {
      TR::Register *idxReg = cg->evaluate(secondChild);
      TR::Register *offsetReg = cg->allocateRegister();
      generateTrg1Src1ImmInstruction (cg, TR::InstOpCode::mulli, node, offsetReg, idxReg, 4);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, TR::MemoryReference::createWithIndexReg(cg, addrReg, offsetReg, 4), valueReg);
      cg->stopUsingRegister(offsetReg);
      }

   generateTrg1MemInstruction(cg, TR::InstOpCode::lxvw4x, node, resReg, TR::MemoryReference::createWithIndexReg(cg, NULL, addrReg, 16));
   cg->stopUsingRegister(addrReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(thirdChild);
   return resReg;

   }

TR::Register *OMR::Power::TreeEvaluator::vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vaddubm);
     case TR::Int16:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vadduhm);
     case TR::Int32:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vadduwm);
     case TR::Int64:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vaddudm);
     case TR::Float:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvaddsp);
     case TR::Double: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvadddp);
     default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }


TR::Register *OMR::Power::TreeEvaluator::vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vsububm);
     case TR::Int16:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vsubuhm);
     case TR::Int32:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vsubuwm);
     case TR::Int64:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vsubudm);
     case TR::Float:  return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvsubsp);
     case TR::Double: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvsubdp);
     default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *OMR::Power::TreeEvaluator::vnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
       return TR::TreeEvaluator::vnegIntHelper(node, cg, TR::Int8);
     case TR::Int16:
       return TR::TreeEvaluator::vnegIntHelper(node, cg, TR::Int16);
     case TR::Int32:
       return TR::TreeEvaluator::vnegIntHelper(node, cg, TR::Int32);
     case TR::Int64:
       return TR::TreeEvaluator::vnegIntHelper(node, cg, TR::Int64);
     case TR::Float:
       return TR::TreeEvaluator::vnegFloatHelper(node, cg);
     case TR::Double:
       return TR::TreeEvaluator::vnegDoubleHelper(node, cg);
     default:
       TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *OMR::Power::TreeEvaluator::vnegIntHelper(TR::Node *node, TR::CodeGenerator *cg, TR::DataType type)
   {
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      {
      if (type == TR::Int32)
         return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::vnegw);
      else if (type == TR::Int64)
         return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::vnegd);
      }

   TR::Node *firstChild;
   TR::Register *srcReg;
   TR::Register *resReg;

   TR::InstOpCode::Mnemonic sub;

   switch(type)
      {
      case TR::Int8:
         sub = TR::InstOpCode::vsububm;
         break;
      case TR::Int16:
         sub = TR::InstOpCode::vsubuhm;
         break;
      case TR::Int32:
         sub = TR::InstOpCode::vsubuwm;
         break;
      case TR::Int64:
         sub = TR::InstOpCode::vsubudm;
         break;
      default:
         TR_ASSERT_FATAL(false, "vnegIntHelper() can only be called on integer-type vectors");
      }

   firstChild = node->getFirstChild();
   srcReg = cg->evaluate(firstChild);

   resReg = cg->allocateRegister(TR_VRF);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlxor, node, resReg, srcReg, srcReg);
   generateTrg1Src2Instruction(cg, sub, node, resReg, resReg, srcReg);
   node->setRegister(resReg);

   cg->decReferenceCount(firstChild);
   return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vnegFloatHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvnegsp);
   }

TR::Register *OMR::Power::TreeEvaluator::vnegDoubleHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvnegdp);
   }

TR::Register *OMR::Power::TreeEvaluator::vabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
       return TR::TreeEvaluator::vabsIntHelper(node, cg, TR::InstOpCode::vsrab, TR::InstOpCode::vaddubm);
     case TR::Int16:
       return TR::TreeEvaluator::vabsIntHelper(node, cg, TR::InstOpCode::vsrah, TR::InstOpCode::vadduhm);
     case TR::Int32:
       return TR::TreeEvaluator::vabsIntHelper(node, cg, TR::InstOpCode::vsraw, TR::InstOpCode::vadduwm);
     case TR::Int64:
       return TR::TreeEvaluator::vabsIntHelper(node, cg, TR::InstOpCode::vsrad, TR::InstOpCode::vaddudm);
     case TR::Float:
       return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvabssp);
     case TR::Double:
       return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvabsdp);
     default:
       TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *OMR::Power::TreeEvaluator::vabsIntHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic shift, TR::InstOpCode::Mnemonic add)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);

   TR::Register *resReg = cg->allocateRegister(TR_VRF);
   TR::Register *shiftReg = cg->allocateRegister(TR_VRF);
   TR::Register *maskReg = cg->allocateRegister(TR_VRF);

   node->setRegister(resReg);

   //set shift amount to size of operand - 1
   generateTrg1ImmInstruction(cg, TR::InstOpCode::vspltisw, node, shiftReg, -1);

   //set mask to 0000... if src is positive, 1111... if src is negative
   generateTrg1Src2Instruction(cg, shift, node, maskReg, srcReg, shiftReg);

   //res = (mask + src) ^ mask
   generateTrg1Src2Instruction(cg, add, node, resReg, maskReg, srcReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlxor, node, resReg, resReg, maskReg);

   cg->decReferenceCount(firstChild);
   return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Float:
       return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvsqrtsp);
     case TR::Double:
       return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvsqrtdp);
     default:
       TR_ASSERT_FATAL(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

static TR::Register *vminDoubleHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *firstReg = cg->evaluate(firstChild);
   TR::Register *secondReg = cg->evaluate(secondChild);

   TR::Register *resReg = cg->allocateRegister(TR_VRF);

   TR::Register *cmpReg = cg->allocateRegister(TR_VRF);
   TR::Register *tempA = cg->allocateRegister(TR_VRF);
   TR::Register *tempB = cg->allocateRegister(TR_VRF);

   node->setRegister(resReg);

   /*
   To match with java library behavior, vmin should return NaN if EITHER of the operands is NaN.
   However, since the VSX minimum instruction only returns NaN when BOTH operands are NaN, some extra steps are needed.
   */

   //Check the first operand for NaN elements, and convert corresponding elements in second operand to NaN
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xvcmpeqdp, node, cmpReg, firstReg, firstReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlorc, node, tempA, secondReg, cmpReg);

   //Check the second operand for NaN elements, and convert corresponding elements in first operand to NaN
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xvcmpeqdp, node, cmpReg, secondReg, secondReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlorc, node, tempB, firstReg, cmpReg);

   //VSX minimum (will return NaN if both operands are NaN)
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xvmindp, node, resReg, tempA, tempB);

   cg->stopUsingRegister(cmpReg);
   cg->stopUsingRegister(tempA);
   cg->stopUsingRegister(tempB);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vminsb);
     case TR::Int16:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vminsh);
     case TR::Int32:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vminsw);
     case TR::Int64:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vminsd);
     case TR::Float:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vminfp);
     case TR::Double:
       return vminDoubleHelper(node, cg);
     default:
       TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *vmaxDoubleHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *firstReg = cg->evaluate(firstChild);
   TR::Register *secondReg = cg->evaluate(secondChild);

   TR::Register *resReg = cg->allocateRegister(TR_VRF);

   TR::Register *cmpReg = cg->allocateRegister(TR_VRF);
   TR::Register *tempA = cg->allocateRegister(TR_VRF);
   TR::Register *tempB = cg->allocateRegister(TR_VRF);

   node->setRegister(resReg);

   /*
   To match with java library behavior, vmax should return NaN if EITHER of the operands is NaN.
   However, since the VSX maximum instruction only returns NaN when BOTH operands are NaN, some extra steps are needed.
   */

   //Check the first operand for NaN elements, and convert corresponding elements in second operand to NaN
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xvcmpeqdp, node, cmpReg, firstReg, firstReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlorc, node, tempA, secondReg, cmpReg);

   //Check the second operand for NaN elements, and convert corresponding elements in first operand to NaN
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xvcmpeqdp, node, cmpReg, secondReg, secondReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlorc, node, tempB, firstReg, cmpReg);

   //VSX maximum (will return NaN if both operands are NaN)
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xvmaxdp, node, resReg, tempA, tempB);

   cg->stopUsingRegister(cmpReg);
   cg->stopUsingRegister(tempA);
   cg->stopUsingRegister(tempB);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmaxsb);
     case TR::Int16:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmaxsh);
     case TR::Int32:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmaxsw);
     case TR::Int64:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmaxsd);
     case TR::Float:
       return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmaxfp);
     case TR::Double:
       return vmaxDoubleHelper(node, cg);
     default:
       TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *OMR::Power::TreeEvaluator::vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int8:
       return TR::TreeEvaluator::vmulInt8Helper(node, cg);
     case TR::Int16:
       return TR::TreeEvaluator::vmulInt16Helper(node,cg);
     case TR::Int32:
       return TR::TreeEvaluator::vmulInt32Helper(node,cg);
     case TR::Int64:
       return TR::TreeEvaluator::vmulInt64Helper(node,cg);
     case TR::Float:
       return TR::TreeEvaluator::vmulFloatHelper(node,cg);
     case TR::Double:
       return TR::TreeEvaluator::vmulDoubleHelper(node,cg);
     default:
       TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *OMR::Power::TreeEvaluator::vmulInt8Helper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   TR::Register *lhsReg, *rhsReg;
   TR::Register *productReg;
   TR::Register *temp;
   TR::Register *shiftReg;

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();

   lhsReg = cg->evaluate(firstChild);
   rhsReg = cg->evaluate(secondChild);

   productReg = cg->allocateRegister(TR_VRF);
   temp = cg->allocateRegister(TR_VRF);
   shiftReg = cg->allocateRegister(TR_VRF);

   node->setRegister(productReg);

   //set shift amount to 1 byte
   generateTrg1ImmInstruction(cg, TR::InstOpCode::vspltisb, node, shiftReg, 8);

   //multiply even bytes and shift to correct location
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuleub, node, productReg, lhsReg, rhsReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vslh, node, productReg, productReg, shiftReg);

   //multiply odd byte and shift left (to discard upper byte of product) and then right (to relocate to correct position)
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuloub, node, temp, lhsReg, rhsReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vslh, node, temp, temp, shiftReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vsrh, node, temp, temp, shiftReg);

   //add odd and even bytes together to get full vector of products
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vaddubm, node, productReg, productReg, temp);

   cg->stopUsingRegister(temp);
   cg->stopUsingRegister(shiftReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return productReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vmulInt16Helper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   TR::Register *lhsReg, *rhsReg;
   TR::Register *productReg;
   TR::Register *temp;

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();

   lhsReg = cg->evaluate(firstChild);
   rhsReg = cg->evaluate(secondChild);

   productReg = cg->allocateRegister(TR_VRF);
   temp = cg->allocateRegister(TR_VRF);

   node->setRegister(productReg);

   generateTrg1ImmInstruction(cg, TR::InstOpCode::vspltish, node, temp, 0);
   generateTrg1Src3Instruction(cg, TR::InstOpCode::vmladduhm, node, productReg, lhsReg, rhsReg, temp);

   cg->stopUsingRegister(temp);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return productReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vmulInt32Helper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmuluwm);
   }

TR::Register *OMR::Power::TreeEvaluator::vmulInt64Helper(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::vmulld);

   /*
   To perform the multiplication without the vmulld instruction, we will break it into parts.

   Suppose we have two long integers (64 bits), AB and CD, where each of A, B, C, and D is one word (32 bits).
   To get their product, AB * CD, we can do the following:

   AB * CD = B*D + 2^32*(A*D + B*C) + 2^64*(A*C)

   Since we are only keeping the lower 64 bits of the result, this is equivalent to:

   AB * CD = B*D + 2^32*(A*D + B*C)
   */

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *lhsReg = cg->evaluate(firstChild);
   TR::Register *rhsReg = cg->evaluate(secondChild);

   TR::Register *productReg = cg->allocateRegister(TR_VRF);
   TR::Register *tempA = cg->allocateRegister(TR_VRF);
   TR::Register *tempB = cg->allocateRegister(TR_VRF);
   TR::Register *shiftReg = cg->allocateRegister(TR_VRF);

   node->setRegister(productReg);

   //set shift amount to -32 bits. Since only the lowest 6 bits of shiftReg are used by vrld and vsld as an unsigned integer, this will result in a shift of 32 bits
   generateTrg1ImmInstruction(cg, TR::InstOpCode::vspltisw, node, shiftReg, -16);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, shiftReg, shiftReg, shiftReg);

   //productReg = B*D
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vmulouw, node, productReg, lhsReg, rhsReg);

   //tempB = 2^32*(A*D + B*C)
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vrld, node, tempB, rhsReg, shiftReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vmulouw, node, tempA, lhsReg, tempB);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuleuw, node, tempB, lhsReg, tempB);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vaddudm, node, tempB, tempA, tempB);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vsld, node, tempB, tempB, shiftReg);

   //add everything together to get final product
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vaddudm, node, productReg, productReg, tempB);

   cg->stopUsingRegister(tempA);
   cg->stopUsingRegister(tempB);
   cg->stopUsingRegister(shiftReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return productReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vmulFloatHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvmulsp);
   }

TR::Register *OMR::Power::TreeEvaluator::vmulDoubleHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvmuldp);
   }

TR::Register *OMR::Power::TreeEvaluator::vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   switch(node->getDataType().getVectorElementType())
     {
     case TR::Int32:
	return TR::TreeEvaluator::vdivInt32Helper(node, cg);
     case TR::Float:
	return TR::TreeEvaluator::vdivFloatHelper(node, cg);
     case TR::Double:
	return TR::TreeEvaluator::vdivDoubleHelper(node, cg);
     default:
       TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
     }
   }

TR::Register *OMR::Power::TreeEvaluator::vdivFloatHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvdivsp);
   }

TR::Register *OMR::Power::TreeEvaluator::vdivDoubleHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::xvdivdp);
   }

TR::Register *OMR::Power::TreeEvaluator::vdivInt32Helper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *lhsReg = NULL, *rhsReg = NULL;

   lhsReg = cg->evaluate(firstChild);
   rhsReg = cg->evaluate(secondChild);

   TR::Register *srcV1IdxReg = cg->allocateRegister();
   TR::Register *srcV2IdxReg = cg->allocateRegister();

   TR::SymbolReference    *srcV1 = cg->allocateLocalTemp(TR::DataType::createVectorType(TR::Int32, TR::VectorLength128));
   TR::SymbolReference    *srcV2 = cg->allocateLocalTemp(TR::DataType::createVectorType(TR::Int32, TR::VectorLength128));

   generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, srcV1IdxReg, TR::MemoryReference::createWithSymRef(cg, node, srcV1, 16));
   generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, srcV2IdxReg, TR::MemoryReference::createWithSymRef(cg, node, srcV2, 16));

   // store the src regs to memory
   generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, TR::MemoryReference::createWithIndexReg(cg, NULL, srcV1IdxReg, 16), lhsReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stxvw4x, node, TR::MemoryReference::createWithIndexReg(cg, NULL, srcV2IdxReg, 16), rhsReg);

   // load each pair and compute division
   int i;
   for (i = 0; i < 4; i++)
      {
      TR::Register *srcA1Reg = cg->allocateRegister();
      TR::Register *srcA2Reg = cg->allocateRegister();
      TR::Register *trgAReg = cg->allocateRegister();
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwa, node, srcA1Reg, TR::MemoryReference::createWithDisplacement(cg, srcV1IdxReg, i * 4, 4));
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwa, node, srcA2Reg, TR::MemoryReference::createWithDisplacement(cg, srcV2IdxReg, i * 4, 4));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::divw, node, trgAReg, srcA1Reg, srcA2Reg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(cg, srcV1IdxReg, i * 4, 4), trgAReg);
      cg->stopUsingRegister(srcA1Reg);
      cg->stopUsingRegister(srcA2Reg);
      cg->stopUsingRegister(trgAReg);
      }

   // load result
   TR::Register *resReg = cg->allocateRegister(TR_VRF);
   generateTrg1MemInstruction(cg, TR::InstOpCode::lxvw4x, node, resReg, TR::MemoryReference::createWithIndexReg(cg, NULL, srcV1IdxReg, 16));

   cg->stopUsingRegister(srcV1IdxReg);
   cg->stopUsingRegister(srcV2IdxReg);
   node->setRegister(resReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return resReg;
   }

TR::Register* OMR::Power::TreeEvaluator::vfmaEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::DataType type = node->getDataType().getVectorElementType();
   if (type != TR::Float && type != TR::Double)
      {
      TR_ASSERT(false, "unsupported vector type %s\n", type.toString());
      return NULL;
      }

   TR::InstOpCode::Mnemonic op;

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild  = node->getThirdChild();

   //resReg = firstReg * secondReg + thirdReg
   TR::Register *firstReg  = cg->evaluate(firstChild);
   TR::Register *secondReg = cg->evaluate(secondChild);
   TR::Register *thirdReg  = cg->evaluate(thirdChild);

   TR::Register *resReg;

   /* Since the VSX FMA instruction reuses one of its source registers as the target register,
      we need to make sure the selected target register is clobberable */
   if (cg->canClobberNodesRegister(thirdChild))
      {
      resReg = thirdReg;
      op = (type == TR::Float) ? TR::InstOpCode::xvmaddasp : TR::InstOpCode::xvmaddadp;
      generateTrg1Src2Instruction(cg, op, node, thirdReg, firstReg, secondReg);
      }
   else if (cg->canClobberNodesRegister(firstChild))
      {
      resReg = firstReg;
      op = (type == TR::Float) ? TR::InstOpCode::xvmaddmsp : TR::InstOpCode::xvmaddmdp;
      generateTrg1Src2Instruction(cg, op, node, firstReg, secondReg, thirdReg);
      }
   else if (cg->canClobberNodesRegister(secondChild))
      {
      resReg = secondReg;
      op = (type == TR::Float) ? TR::InstOpCode::xvmaddmsp : TR::InstOpCode::xvmaddmdp;
      generateTrg1Src2Instruction(cg, op, node, secondReg, firstReg, thirdReg);
      }
   else //if no source registers can be clobbered, copy the contents of one of the sources into a new register and use it as the target
      {
      resReg = cg->allocateRegister(TR_VSX_VECTOR);
      op = (type == TR::Float) ? TR::InstOpCode::xvmaddasp : TR::InstOpCode::xvmaddadp;
      generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, thirdReg, thirdReg);
      generateTrg1Src2Instruction(cg, op, node, resReg, firstReg, secondReg);
      }

   node->setRegister(resReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(thirdChild);
   return resReg;
   }

TR::Register *OMR::Power::TreeEvaluator::vconvEvaluator(TR::Node *node, TR::CodeGenerator *cg)
    {
    TR_ASSERT_FATAL(node->getOpCode().getVectorSourceDataType().getVectorElementType() == TR::Int64 &&
                    node->getOpCode().getVectorResultDataType().getVectorElementType() == TR::Double,
                   "Only vector Long to vector Double is currently supported\n");

    return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::xvcvsxddp);
    }

TR::Register *OMR::Power::TreeEvaluator::vindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::vorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::vfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return vaddEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmfmaEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmnotEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionAddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionAndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionFirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionMinEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionOrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionOrUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmreductionXorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vcompressEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vexpandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::mcompressEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vbitswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmbitswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register* OMR::Power::TreeEvaluator::vmexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

static void inlineArrayCopy(TR::Node *node, int64_t byteLen, TR::Register *src, TR::Register *dst, TR::CodeGenerator *cg)
   {
   if (byteLen == 0)
      return;

   TR::Register *regs[4] = {NULL, NULL, NULL, NULL};
   TR::Register *fpRegs[4] = {NULL, NULL, NULL, NULL};
   int32_t groups, residual, regIx=0, ix=0, fpRegIx=0;
   uint8_t numDeps = 0;
   int32_t memRefSize;
   TR::InstOpCode::Mnemonic load, store;
   TR::Compilation* comp = cg->comp();

   /* Some machines have issues with 64bit loads/stores in 32bit mode, ie. sicily, do not check for is64BitProcessor() */
   memRefSize = TR::Compiler->om.sizeofReferenceAddress();
   load =TR::InstOpCode::Op_load;
   store =TR::InstOpCode::Op_st;

   bool  postP10CopyInline = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10) && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX);

   static bool disableLEArrayCopyInline  = (feGetEnv("TR_disableLEArrayCopyInline") != NULL);
   bool  supportsLEArrayCopyInline = (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8)) && !disableLEArrayCopyInline && cg->comp()->target().cpu.isLittleEndian() && cg->comp()->target().cpu.hasFPU() && cg->comp()->target().is64Bit();

   TR::RealRegister::RegNum tempDep, srcDep, dstDep, cndDep;
   tempDep = TR::RealRegister::NoReg;
   srcDep = TR::RealRegister::NoReg;
   dstDep = TR::RealRegister::NoReg;
   cndDep = TR::RealRegister::NoReg;
   if (supportsLEArrayCopyInline)
      numDeps = 7+4;
   else
      numDeps = 7;

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numDeps, numDeps, cg->trMemory());

   TR::addDependency(conditions, src, srcDep, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   TR::addDependency(conditions, dst, dstDep, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();

   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   if (postP10CopyInline)
      {
      // byteLen reg
      regs[0] = cg->allocateRegister(TR_GPR);
      TR::addDependency(conditions, regs[0], TR::RealRegister::NoReg, TR_GPR, cg);

      // First VSX vector reg
      regs[1] = cg->allocateRegister(TR_VSX_VECTOR);
      TR::addDependency(conditions, regs[1], TR::RealRegister::NoReg, TR_VSX_VECTOR, cg);

      int32_t iteration64 = byteLen >> 6, residue64 = byteLen & 0x3F, standingOffset = 0;

      if (iteration64 > 0)
         {
         TR::LabelSymbol *loopStart;
         TR::Register    *cndReg;

         if (iteration64 > 1)
            {
            generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, regs[0], iteration64);
            generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, regs[0]);

            cndReg = cg->allocateRegister(TR_CCR);
            TR::addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);

            loopStart = generateLabelSymbol(cg);
            generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStart);
            }

         generateTrg1MemInstruction(cg, TR::InstOpCode::lxv, node, regs[1], TR::MemoryReference::createWithDisplacement(cg, src, 0, 16));
         generateMemSrc1Instruction(cg, TR::InstOpCode::stxv, node, TR::MemoryReference::createWithDisplacement(cg, dst, 0, 16), regs[1]);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lxv, node, regs[1], TR::MemoryReference::createWithDisplacement(cg, src, 16, 16));
         generateMemSrc1Instruction(cg, TR::InstOpCode::stxv, node, TR::MemoryReference::createWithDisplacement(cg, dst, 16, 16), regs[1]);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lxv, node, regs[1], TR::MemoryReference::createWithDisplacement(cg, src, 32, 16));
         generateMemSrc1Instruction(cg, TR::InstOpCode::stxv, node, TR::MemoryReference::createWithDisplacement(cg, dst, 32, 16), regs[1]);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lxv, node, regs[1], TR::MemoryReference::createWithDisplacement(cg, src, 48, 16));
         generateMemSrc1Instruction(cg, TR::InstOpCode::stxv, node, TR::MemoryReference::createWithDisplacement(cg, dst, 48, 16), regs[1]);

         if (iteration64 > 1)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src, src, 64);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, dst, dst, 64);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopStart, cndReg);
            }
         else
            standingOffset = 64;
         }

      for (int32_t i = 0; i < (residue64>>4); i++)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lxv, node, regs[1], TR::MemoryReference::createWithDisplacement(cg, src, standingOffset+i*16, 16));
         generateMemSrc1Instruction(cg, TR::InstOpCode::stxv, node, TR::MemoryReference::createWithDisplacement(cg, dst, standingOffset+i*16, 16), regs[1]);
         }

      if ((residue64 & 0xF) != 0)
         {
         standingOffset += residue64 & 0x30;
         switch (residue64 & 0xF)
            {
            case 1:
               generateTrg1MemInstruction(cg, TR::InstOpCode::lbz, node, regs[0], TR::MemoryReference::createWithDisplacement(cg, src, standingOffset, 1));
               generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node, TR::MemoryReference::createWithDisplacement(cg, dst, standingOffset, 1), regs[0]);
               break;
            case 2:
               generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, regs[0], TR::MemoryReference::createWithDisplacement(cg, src, standingOffset, 2));
               generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node, TR::MemoryReference::createWithDisplacement(cg, dst, standingOffset, 2), regs[0]);
               break;
            case 4:
               generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, regs[0], TR::MemoryReference::createWithDisplacement(cg, src, standingOffset, 4));
               generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(cg, dst, standingOffset, 4), regs[0]);
               break;
            case 8:
               generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, regs[0], TR::MemoryReference::createWithDisplacement(cg, src, standingOffset, 8));
               generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dst, standingOffset, 8), regs[0]);
               break;
            default:
               generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, regs[0], residue64 & 0xF);
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, regs[0], regs[0],
                                               56, CONSTANT64(0xff00000000000000));
               if (standingOffset != 0)
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src, src, standingOffset);
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, dst, dst, standingOffset);
                  }
               generateTrg1Src2Instruction(cg, TR::InstOpCode::lxvl, node, regs[1], src, regs[0]);
               generateSrc3Instruction(cg, TR::InstOpCode::stxvl, node, regs[1], dst, regs[0]);
               break;
            }
         }
      }
   else
      {
      TR::Register *cndReg = cg->allocateRegister(TR_CCR);
      TR::addDependency(conditions, cndReg, cndDep, TR_CCR, cg);

      if (cg->comp()->target().is64Bit())
         {
         groups = byteLen >> 5;
         residual = byteLen & 0x0000001F;
         }
      else
         {
         groups = byteLen >> 4;
         residual = byteLen & 0x0000000F;
         }

      regs[0] = cg->allocateRegister(TR_GPR);
      TR::addDependency(conditions, regs[0], tempDep, TR_GPR, cg);

      if (groups != 0)
         {
         regs[1] = cg->allocateRegister(TR_GPR);
         regs[2] = cg->allocateRegister(TR_GPR);
         regs[3] = cg->allocateRegister(TR_GPR);
         TR::addDependency(conditions, regs[1], TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(conditions, regs[2], TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(conditions, regs[3], TR::RealRegister::NoReg, TR_GPR, cg);
         }

      if (supportsLEArrayCopyInline)
         {
         fpRegs[0] = cg->allocateRegister(TR_FPR);
         fpRegs[1] = cg->allocateRegister(TR_FPR);
         fpRegs[2] = cg->allocateRegister(TR_FPR);
         fpRegs[3] = cg->allocateRegister(TR_FPR);

         TR::addDependency(conditions, fpRegs[0], TR::RealRegister::NoReg, TR_FPR, cg);
         TR::addDependency(conditions, fpRegs[1], TR::RealRegister::NoReg, TR_FPR, cg);
         TR::addDependency(conditions, fpRegs[2], TR::RealRegister::NoReg, TR_FPR, cg);
         TR::addDependency(conditions, fpRegs[3], TR::RealRegister::NoReg, TR_FPR, cg);
         }

      if (groups != 0)
         {
         TR::LabelSymbol *loopStart;

         if (groups != 1)
            {
            if (groups <= UPPER_IMMED)
               generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, regs[0], groups);
            else
               loadConstant(cg, node, groups, regs[0]);
            generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, regs[0]);

            loopStart = generateLabelSymbol(cg);
            generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStart);
            }
         if (supportsLEArrayCopyInline)
            {
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fpRegs[3], TR::MemoryReference::createWithDisplacement(cg, src, 0, memRefSize));
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fpRegs[2], TR::MemoryReference::createWithDisplacement(cg, src, memRefSize, memRefSize));
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fpRegs[1], TR::MemoryReference::createWithDisplacement(cg, src, 2*memRefSize, memRefSize));
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fpRegs[0], TR::MemoryReference::createWithDisplacement(cg, src, 3*memRefSize, memRefSize));
            }
         else
            {
            generateTrg1MemInstruction(cg, load, node, regs[3], TR::MemoryReference::createWithDisplacement(cg, src, 0, memRefSize));
            generateTrg1MemInstruction(cg, load, node, regs[2], TR::MemoryReference::createWithDisplacement(cg, src, memRefSize, memRefSize));
            generateTrg1MemInstruction(cg, load, node, regs[1], TR::MemoryReference::createWithDisplacement(cg, src, 2*memRefSize, memRefSize));
            generateTrg1MemInstruction(cg, load, node, regs[0], TR::MemoryReference::createWithDisplacement(cg, src, 3*memRefSize, memRefSize));
            }
         if (groups != 1)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, src, src, 4*memRefSize);
         if (supportsLEArrayCopyInline)
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, TR::MemoryReference::createWithDisplacement(cg, dst, 0, memRefSize), fpRegs[3]);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, TR::MemoryReference::createWithDisplacement(cg, dst, memRefSize, memRefSize), fpRegs[2]);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, TR::MemoryReference::createWithDisplacement(cg, dst, 2*memRefSize, memRefSize), fpRegs[1]);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, TR::MemoryReference::createWithDisplacement(cg, dst, 3*memRefSize, memRefSize), fpRegs[0]);
            }
         else
            {
            generateMemSrc1Instruction(cg, store, node, TR::MemoryReference::createWithDisplacement(cg, dst, 0, memRefSize), regs[3]);
            generateMemSrc1Instruction(cg, store, node, TR::MemoryReference::createWithDisplacement(cg, dst, memRefSize, memRefSize), regs[2]);
            generateMemSrc1Instruction(cg, store, node, TR::MemoryReference::createWithDisplacement(cg, dst, 2*memRefSize, memRefSize), regs[1]);
            generateMemSrc1Instruction(cg, store, node, TR::MemoryReference::createWithDisplacement(cg, dst, 3*memRefSize, memRefSize), regs[0]);
            }
         if (groups != 1)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, dst, dst, 4*memRefSize);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopStart, cndReg);
            }
         else
            {
            ix = 4*memRefSize;
            }
         }

      for (; residual>=memRefSize; residual-=memRefSize, ix+=memRefSize)
         {
         if (supportsLEArrayCopyInline)
            {
            TR::Register *oneReg = fpRegs[fpRegIx++];
            if (fpRegIx>3 || fpRegs[fpRegIx]==NULL)
               fpRegIx = 0;
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, oneReg, TR::MemoryReference::createWithDisplacement(cg, src, ix, memRefSize));
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, TR::MemoryReference::createWithDisplacement(cg, dst, ix, memRefSize), oneReg);
            }
         else
            {
            TR::Register *oneReg = regs[regIx++];
            if (regIx>3 || regs[regIx]==NULL)
               regIx = 0;
            generateTrg1MemInstruction(cg, load, node, oneReg, TR::MemoryReference::createWithDisplacement(cg, src, ix, memRefSize));
            generateMemSrc1Instruction(cg, store, node, TR::MemoryReference::createWithDisplacement(cg, dst, ix, memRefSize), oneReg);
            }
         }

      if (residual != 0)
         {
         if (residual & 4)
            {
            if (supportsLEArrayCopyInline)
               {
               TR::Register *oneReg = fpRegs[fpRegIx++];
               if (fpRegIx>3 || fpRegs[fpRegIx]==NULL)
                  fpRegIx = 0;
               generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, oneReg, TR::MemoryReference::createWithDisplacement(cg, src, ix, 4));
               generateMemSrc1Instruction(cg, TR::InstOpCode::stfs, node, TR::MemoryReference::createWithDisplacement(cg, dst, ix, 4), oneReg);
               }
            else
               {
               TR::Register *oneReg = regs[regIx++];
               if (regIx>3 || regs[regIx]==NULL)
                  regIx = 0;
               generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, oneReg, TR::MemoryReference::createWithDisplacement(cg, src, ix, 4));
               generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(cg, dst, ix, 4), oneReg);
               }
            ix += 4;
            }
         if (residual & 2)
            {
            TR::Register *oneReg = regs[regIx++];
            if (regIx>3 || regs[regIx]==NULL)
               regIx = 0;
            generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, oneReg, TR::MemoryReference::createWithDisplacement(cg, src, ix, 2));
            generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node, TR::MemoryReference::createWithDisplacement(cg, dst, ix, 2), oneReg);
            ix += 2;
            }
         if (residual & 1)
            {
            generateTrg1MemInstruction(cg, TR::InstOpCode::lbz, node, regs[regIx], TR::MemoryReference::createWithDisplacement(cg, src, ix, 1));
            generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node, TR::MemoryReference::createWithDisplacement(cg, dst, ix, 1), regs[regIx]);
            }
         }
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   conditions->stopUsingDepRegs(cg);
   return;
   }

TR::Register *OMR::Power::TreeEvaluator::arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   // tree looks as follows:
   // arraytranslate
   //    input ptr
   //    output ptr
   //    translation table (dummy for PPC TRTO255)
   //    stop character (terminal character, either 0xff00ff00 (ISO8859) or 0xff80ff80 (ASCII)
   //    input length (in elements)
   //
   // Number of elements translated is returned

   //sourceByte == true iff the source operand is a byte array
   bool sourceByte = node->isSourceByteArrayTranslate();
   bool arraytranslateOT = false;
   TR::Compilation *comp = cg->comp();
   bool arraytranslateTRTO255 = false;
   if (sourceByte)
      {
      if ((node->getChild(3)->getOpCodeValue() == TR::iconst) && (node->getChild(3)->getInt() == 0))
         arraytranslateOT = true;
      }
#ifndef __LITTLE_ENDIAN__
   else
      {
      if (cg->getSupportsArrayTranslateTRTO255() &&
          node->getChild(3)->getOpCodeValue() == TR::iconst &&
          node->getChild(3)->getInt() == 0x0ff00ff00)
         arraytranslateTRTO255 = true;
      }
#endif

   static bool forcePPCTROT = (feGetEnv("TR_forcePPCTROT") != NULL);
   if(forcePPCTROT)
      arraytranslateOT = true;

   static bool verbosePPCTRTOOT = (feGetEnv("TR_verbosePPCTRTOOT") != NULL);
   if(verbosePPCTRTOOT)
      fprintf(stderr, "%s @ %s [isSourceByte: %d] [isOT: %d] [%d && %d && %d]: arraytranslate\n",
         comp->signature(),
         comp->getHotnessName(comp->getMethodHotness()),
         sourceByte,
         arraytranslateOT,
         sourceByte,
         (node->getChild(3)->getOpCodeValue() == TR::iconst),
         (node->getChild(3)->getInt() == 0)
         );

   TR::Register *inputReg = cg->gprClobberEvaluate(node->getChild(0));
   TR::Register *outputReg = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register *stopCharReg = arraytranslateTRTO255 ? NULL : cg->gprClobberEvaluate(node->getChild(3));
   TR::Register *inputLenReg = cg->gprClobberEvaluate(node->getChild(4));
   TR::Register *outputLenReg = cg->allocateRegister();
   TR::Register *condReg = cg->allocateRegister(TR_CCR);

   int noOfDependecies = 14 + (sourceByte ?  4 + (arraytranslateOT ?  3 : 0 ) : 5);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, noOfDependecies, cg->trMemory());

   deps->addPreCondition(inputReg, TR::RealRegister::gr3);

   deps->addPostCondition(outputLenReg, TR::RealRegister::gr3);
   deps->addPostCondition(outputReg, TR::RealRegister::gr4);
   deps->addPostCondition(inputLenReg, TR::RealRegister::gr5);
   if (!arraytranslateTRTO255)
      deps->addPostCondition(stopCharReg, TR::RealRegister::gr8);

   // Clobbered by the helper
   TR::Register *clobberedReg;
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::gr6);
   cg->stopUsingRegister(clobberedReg);
   if (!arraytranslateTRTO255)
      {
      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::gr7);
      cg->stopUsingRegister(clobberedReg);
      }

   //CCR.
   deps->addPostCondition(condReg, TR::RealRegister::cr0);

   //Trampoline
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::gr11);
   cg->stopUsingRegister(clobberedReg);

   //VR's
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr0);
   cg->stopUsingRegister(clobberedReg);
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr1);
   cg->stopUsingRegister(clobberedReg);
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr2);
   cg->stopUsingRegister(clobberedReg);
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr3);
   cg->stopUsingRegister(clobberedReg);

   //FP/VSR
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vsr0);
   cg->stopUsingRegister(clobberedReg);
   deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vsr1);
   cg->stopUsingRegister(clobberedReg);

   if(sourceByte)
      {
      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::gr9);
      cg->stopUsingRegister(clobberedReg);
      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::gr10);
      cg->stopUsingRegister(clobberedReg);

      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vsr2);
      cg->stopUsingRegister(clobberedReg);
      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vsr3);
      cg->stopUsingRegister(clobberedReg);

      if(arraytranslateOT)
         {
         deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::cr6);
         cg->stopUsingRegister(clobberedReg);

         deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr4);
         cg->stopUsingRegister(clobberedReg);
         deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr5);
         cg->stopUsingRegister(clobberedReg);
         }
      }
   else
      {
      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::cr6);
      cg->stopUsingRegister(clobberedReg);

      deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr4);
      cg->stopUsingRegister(clobberedReg);
      if (!arraytranslateTRTO255)
         {
         deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr5);
         cg->stopUsingRegister(clobberedReg);
         deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr6);
         cg->stopUsingRegister(clobberedReg);
         deps->addPostCondition(clobberedReg = cg->allocateRegister(), TR::RealRegister::vr7);
         cg->stopUsingRegister(clobberedReg);
         }
      }

   TR::LabelSymbol *labelArrayTranslateStart  = generateLabelSymbol(cg);
   TR::LabelSymbol *labelNonZeroLengthInput   = generateLabelSymbol(cg);
   TR::LabelSymbol *labelArrayTranslateDone   = generateLabelSymbol(cg);
   labelArrayTranslateStart->setStartInternalControlFlow();
   labelArrayTranslateDone->setEndInternalControlFlow();

   generateLabelInstruction(cg, TR::InstOpCode::label, node, labelArrayTranslateStart);

   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, condReg, inputLenReg, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, labelNonZeroLengthInput, condReg);


      {  //Zero length input, return value in outputLenReg.
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, inputReg, 0);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, labelArrayTranslateDone);
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, labelNonZeroLengthInput);

      {  //Array Translate helper call, greater-than-zero length input.
         TR_RuntimeHelper helper;
         if (sourceByte)
            {
            TR_ASSERT(!node->isTargetByteArrayTranslate(), "Both source and target are byte for array translate");
            if (arraytranslateOT)
            {
               helper = TR_PPCarrayTranslateTROT;
            }
            else
               helper = TR_PPCarrayTranslateTROT255;
            }
         else
            {
            TR_ASSERT(node->isTargetByteArrayTranslate(), "Both source and target are word for array translate");
            helper = arraytranslateTRTO255 ? TR_PPCarrayTranslateTRTO255 : TR_PPCarrayTranslateTRTO;
            }
         TR::SymbolReference *helperSym = cg->symRefTab()->findOrCreateRuntimeHelper(helper);
         uintptr_t          addr = (uintptr_t)helperSym->getMethodAddress();
         generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, addr, deps, helperSym);
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelArrayTranslateDone, deps);

   for (uint32_t i = 0; i < node->getNumChildren(); ++i)
      cg->decReferenceCount(node->getChild(i));

   cg->stopUsingRegister(condReg);

   if (inputReg != node->getChild(0)->getRegister())
      cg->stopUsingRegister(inputReg);

   if (outputReg != node->getChild(1)->getRegister())
      cg->stopUsingRegister(outputReg);

   if (!arraytranslateTRTO255 && stopCharReg != node->getChild(3)->getRegister())
      cg->stopUsingRegister(stopCharReg);

   if (inputLenReg != node->getChild(4)->getRegister())
      cg->stopUsingRegister(inputLenReg);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   node->setRegister(outputLenReg);
   return outputLenReg;
   }

static bool loadValue2(TR::Node *node, int valueSize, int length, bool constFill, TR::Register*& tmp1Reg, TR::CodeGenerator *cg)
   {
   bool retVal;

   TR::Node *secondChild = node->getSecondChild();  // Value

   // Value can only be 1 or 2-bytes long
   if (valueSize == 1)
      {
      tmp1Reg = cg->allocateRegister();
      retVal = true;

      if (constFill)
         {
         uint8_t  oneByteFill = secondChild->getByte();
         uint16_t twoByteFill = (oneByteFill << 8) | oneByteFill;
         loadConstant(cg, node, (int32_t) twoByteFill, tmp1Reg);
         }
      else
         {
         // Make up the memset source
         TR::Register *valReg;
         valReg = cg->evaluate(secondChild);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tmp1Reg, valReg, 0);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  8, 0xff00);
         }
      }
   else
      {
      TR_ASSERT(valueSize == length, "Wrong data size.");
      retVal = false;

      tmp1Reg = cg->evaluate(secondChild);
      }

   return retVal;
   }

static bool loadValue4(TR::Node *node, int valueSize, int length, bool constFill, TR::Register*& tmp1Reg, TR::CodeGenerator *cg)
   {
   bool retVal;

   TR::Node *secondChild = node->getSecondChild();  // Value

   // Value can only be 1, 2 or 4-bytes long
   if (valueSize == 1)
      {
      tmp1Reg = cg->allocateRegister();
      retVal = true;

      if (constFill)
         {
         uint8_t  oneByteFill  = secondChild->getByte();
         uint16_t twoByteFill  = (oneByteFill << 8) | oneByteFill;
         uint32_t fourByteFill = (twoByteFill << 16) | twoByteFill;
         loadConstant(cg, node, (int32_t)fourByteFill, tmp1Reg);
         }
      else
         {
         // Make up the memset source
         TR::Register *valReg;
         valReg = cg->evaluate(secondChild);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tmp1Reg, valReg, 0);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  8, 0xff00);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  16, 0xffff0000);
         }
      }
   else if (valueSize == 2)
      {
      tmp1Reg = cg->allocateRegister();
      retVal = true;

      if (constFill)
         {
         uint16_t twoByteFill  = secondChild->getShortInt();
         uint32_t fourByteFill = (twoByteFill << 16) | twoByteFill;
         loadConstant(cg, node, (int32_t)fourByteFill, tmp1Reg);
         }
      else
         {
         // Make up the memset source
         TR::Register *valReg;
         valReg = cg->evaluate(secondChild);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tmp1Reg, valReg, 0);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  16, 0xffff0000);
         }
      }
   else
      {
      TR_ASSERT(valueSize == length, "Wrong data size.");
      retVal = false;

      tmp1Reg = cg->evaluate(secondChild);
      }

   return retVal;
   }

static bool loadValue8(TR::Node *node, int valueSize, int length, bool constFill, TR::Register*& tmp1Reg, TR::CodeGenerator *cg)
   {
   bool retVal;

   TR::Node *secondChild = node->getSecondChild();  // Value

   // Value can only be 1, 2, 4 or 8-bytes long
   if (valueSize == 1)
      {
      tmp1Reg = cg->allocateRegister();
      retVal = true;

      if (constFill)
         {
         uint8_t  oneByteFill  = secondChild->getByte();
         uint16_t twoByteFill  = (oneByteFill << 8) | oneByteFill;
         uint32_t fourByteFill = (twoByteFill << 16) | twoByteFill;
         uint64_t eightByteFill = ((uint64_t)fourByteFill << 32) | fourByteFill;
         loadConstant(cg, node, (int64_t)eightByteFill, tmp1Reg);
         }
      else
         {
         // Make up the memset source
         TR::Register *valReg;
         valReg = cg->evaluate(secondChild);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tmp1Reg, valReg, 0);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  8, 0xff00);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  16, 0xffff0000);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, tmp1Reg, tmp1Reg,  32, 0xffffffff00000000);
         }
      }
   else if (valueSize == 2)
      {
      tmp1Reg = cg->allocateRegister();
      retVal = true;

      if (constFill)
         {
         uint16_t twoByteFill  = secondChild->getShortInt();
         uint32_t fourByteFill = (twoByteFill << 16) | twoByteFill;
         uint64_t eightByteFill = ((uint64_t)fourByteFill << 32) | fourByteFill;
         loadConstant(cg, node, (int64_t)eightByteFill, tmp1Reg);
         }
      else
         {
         // Make up the memset source
         TR::Register *valReg;
         valReg = cg->evaluate(secondChild);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tmp1Reg, valReg, 0);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmp1Reg, tmp1Reg,  16, 0xffff0000);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, tmp1Reg, tmp1Reg,  32, 0xffffffff00000000);
         }
      }
   else if (valueSize == 4)
      {
      tmp1Reg = cg->allocateRegister();
      retVal = true;

      if (constFill)
         {
         uint32_t fourByteFill = secondChild->getInt();
         uint64_t eightByteFill = ((uint64_t)fourByteFill << 32) | fourByteFill;
         loadConstant(cg, node, (int64_t)eightByteFill, tmp1Reg);
         }
      else
         {
         // Make up the memset source
         TR::Register *valReg;
         valReg = cg->evaluate(secondChild);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tmp1Reg, valReg, 0);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, tmp1Reg, tmp1Reg,  32, 0xffff0000);
         }
      }
   else
      {
      TR_ASSERT(valueSize == length, "Wrong data size.");
      retVal = false;

      tmp1Reg = cg->evaluate(secondChild);
      }

   return retVal;
   }


static inline void loadArrayCmpSources(TR::Node *node, TR::InstOpCode::Mnemonic Op_load, uint32_t loadSize, uint32_t byteLen, TR::Register *src1, TR::Register *src2, TR::Register *src1Addr, TR::Register *src2Addr, TR::CodeGenerator *cg)
   {
   generateTrg1MemInstruction (cg,TR::InstOpCode::Op_load, node, src1, TR::MemoryReference::createWithDisplacement(cg, src1Addr, 0, loadSize));
   generateTrg1MemInstruction (cg,TR::InstOpCode::Op_load, node, src2, TR::MemoryReference::createWithDisplacement(cg, src2Addr, 0, loadSize));
   if (loadSize != byteLen)
      {
      TR_ASSERT(loadSize == 4 || loadSize == 8,"invalid load size\n");
      if (loadSize == 4)
         {
         generateShiftRightLogicalImmediate(cg, node, src1, src1, (loadSize-byteLen)*8);
         generateShiftRightLogicalImmediate(cg, node, src2, src2, (loadSize-byteLen)*8);
         }
      else
         {
         generateShiftRightLogicalImmediateLong(cg, node, src1, src1, (loadSize-byteLen)*8);
         generateShiftRightLogicalImmediateLong(cg, node, src2, src2, (loadSize-byteLen)*8);
         }
      }
   }

static TR::Register *inlineArrayCmpP10(TR::Node *node, TR::CodeGenerator *cg, bool isArrayCmpLen)
   {
   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   TR::Register *src1AddrReg = cg->evaluate(src1AddrNode);
   TR::Register *src2AddrReg = cg->evaluate(src2AddrNode);
   TR::Register *indexReg = cg->allocateRegister(TR_GPR);
   TR::Register *returnReg =  cg->allocateRegister(TR_GPR);
   TR::Register *tempReg = cg->gprClobberEvaluate(lengthNode);
   TR::Register *temp2Reg = cg->allocateRegister(TR_GPR);
   TR::Register *pairReg = nullptr;

   TR::Register *vec0Reg = cg->allocateRegister(TR_VRF);
   TR::Register *vec1Reg = cg->allocateRegister(TR_VRF);
   TR::Register *condReg = cg->allocateRegister(TR_CCR);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *loopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *residueStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *resultLabel = generateLabelSymbol(cg);

   bool is64bit = cg->comp()->target().is64Bit();

   if (isArrayCmpLen && !is64bit)
      {
      pairReg = tempReg;
      tempReg = tempReg->getLowOrder();
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
   startLabel->setStartInternalControlFlow();

   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, indexReg, 0);
   generateTrg1Src1ImmInstruction(cg, (is64bit && isArrayCmpLen) ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpli4, node, condReg, tempReg, 16);

   // We don't need length anymore as we can calculate the appropriate index by using indexReg and the remainder
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, returnReg, tempReg, 0, 0xF);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, residueStartLabel, condReg);

   if (is64bit && isArrayCmpLen)
      {
      generateShiftRightLogicalImmediateLong(cg, node, tempReg, tempReg, 4);
      }
   else
      {
      generateShiftRightLogicalImmediate(cg, node, tempReg, tempReg, 4);
      }
   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStartLabel);

   // main-loop
   generateTrg1Src2Instruction(cg, TR::InstOpCode::lxvb16x, node, vec0Reg, indexReg, src1AddrReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::lxvb16x, node, vec1Reg, indexReg, src2AddrReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vcmpneb_r, node, vec0Reg, vec0Reg, vec1Reg);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, resultLabel, condReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, indexReg, indexReg, 16);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopStartLabel, condReg);
   // main-loop end

   // residue start
   generateLabelInstruction(cg, TR::InstOpCode::label, node, residueStartLabel);

   generateShiftLeftImmediateLong(cg, node, temp2Reg, returnReg, 56);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, tempReg, src1AddrReg, indexReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::lxvll, node, vec0Reg, tempReg, temp2Reg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, tempReg, src2AddrReg, indexReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::lxvll, node, vec1Reg, tempReg, temp2Reg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vcmpneb, node, vec0Reg, vec0Reg, vec1Reg);
   // residue end

   // result
   generateLabelInstruction(cg, TR::InstOpCode::label, node, resultLabel);

   generateTrg1Src1Instruction(cg, TR::InstOpCode::vclzlsbb, node, tempReg, vec0Reg);

   if (!isArrayCmpLen)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, returnReg, returnReg, -1);
      }

   // offset = matched-byte-count == 16 ? remainder : match-byte-count
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, tempReg, 16);
   generateTrg1Src3Instruction(cg, TR::InstOpCode::isellt, node, returnReg, tempReg, returnReg, condReg);

   // index = index + offset, if we need to return unmatched index, then we are done here
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, returnReg, indexReg, returnReg);

   if (!isArrayCmpLen)
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::lbzx, node, tempReg, returnReg, src1AddrReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::lbzx, node, indexReg, returnReg, src2AddrReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, tempReg, indexReg);
      // result = -1,0,1
      generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, tempReg, condReg);
      // convert -1,0,1 to 1,0,2 to match current arraycmp return value
      generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, tempReg, tempReg);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, returnReg, tempReg, 2, 3);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, returnReg, returnReg, tempReg);
      }
   else if (!is64bit)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, temp2Reg, 0);
      }

   int32_t numRegs = 9;

   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numRegs, cg->trMemory());
   dependencies->addPostCondition(src1AddrReg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(src2AddrReg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(tempReg, TR::RealRegister::NoReg);
   dependencies->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0();
   dependencies->addPostCondition(returnReg, TR::RealRegister::NoReg);
   dependencies->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0();
   dependencies->addPostCondition(condReg, TR::RealRegister::cr6);
   dependencies->addPostCondition(vec0Reg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(vec1Reg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(indexReg, TR::RealRegister::NoReg);
   dependencies->getPostConditions()->getRegisterDependency(7)->setExcludeGPR0();
   dependencies->addPostCondition(temp2Reg, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, dependencies);
   endLabel->setEndInternalControlFlow();

   if (isArrayCmpLen && !is64bit)
      {
      TR::Register *lowReturnReg = returnReg;
      returnReg = cg->allocateRegisterPair(returnReg, temp2Reg);
      node->setRegister(returnReg);
      TR::Register *liveRegs[4] = { src1AddrReg, src2AddrReg, lowReturnReg, temp2Reg };
      dependencies->stopUsingDepRegs(cg, 4, liveRegs);
      cg->stopUsingRegister(pairReg);
      }
   else
      {
      node->setRegister(returnReg);
      TR::Register *liveRegs[3] = { src1AddrReg, src2AddrReg, returnReg };
      dependencies->stopUsingDepRegs(cg, 3, liveRegs);
      }
   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode);
   cg->decReferenceCount(lengthNode);

   return returnReg;
   }


static TR::Register *inlineArrayCmp(TR::Node *node, TR::CodeGenerator *cg, bool isArrayCmpLen)
   {
   static char *disableP10ArrayCmp = feGetEnv("TR_DisableP10ArrayCmp");
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10) && (disableP10ArrayCmp == NULL))
      return inlineArrayCmpP10(node, cg, isArrayCmpLen);

   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   int32_t byteLen = -1;
   TR::Register *byteLenRegister = NULL;
   TR::Register *condReg = NULL;
   TR::Register *condReg2 = NULL;
   TR::Register *byteLenRemainingRegister = NULL;
   TR::Register *tempReg = NULL;

   TR::LabelSymbol *startLabel = NULL;
   TR::LabelSymbol *loopStartLabel = NULL;
   TR::LabelSymbol *residueStartLabel = NULL;
   TR::LabelSymbol *residueLoopStartLabel = NULL;
   TR::LabelSymbol *residueEndLabel = NULL;
   TR::LabelSymbol *resultLabel = NULL;
   TR::LabelSymbol *cntrLabel = NULL;
   TR::LabelSymbol *midLabel = NULL;
   TR::LabelSymbol *mid2Label = NULL;
   TR::LabelSymbol *result2Label = NULL;

   TR::Register *src1AddrReg = cg->gprClobberEvaluate(src1AddrNode);
   TR::Register *src2AddrReg = cg->gprClobberEvaluate(src2AddrNode);

   bool is64bit = cg->comp()->target().is64Bit();

   if (is64bit)
      {
      byteLen = 8;
      }
   else
      {
      byteLen = 4;
      }

   byteLenRegister = cg->evaluate(lengthNode);
   if (isArrayCmpLen && !is64bit)
      {
      byteLenRegister = byteLenRegister->getLowOrder();
      }
   byteLenRemainingRegister = cg->allocateRegister(TR_GPR);
   tempReg = cg->allocateRegister(TR_GPR);

   generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, byteLenRemainingRegister, byteLenRegister);

   startLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

   residueStartLabel = generateLabelSymbol(cg);
   residueLoopStartLabel = generateLabelSymbol(cg);

   condReg =  cg->allocateRegister(TR_CCR);
   condReg2 =  cg->allocateRegister(TR_CCR);

   mid2Label = generateLabelSymbol(cg);
   generateTrg1Src1ImmInstruction(cg, (is64bit && isArrayCmpLen) ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpli4, node, condReg2, byteLenRemainingRegister, byteLen);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, mid2Label, condReg2);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src1AddrReg, src1AddrReg, -1*byteLen);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src2AddrReg, src2AddrReg, -1*byteLen);

   if (is64bit && isArrayCmpLen)
      {
      generateShiftRightLogicalImmediateLong(cg, node, tempReg, byteLenRemainingRegister, (byteLen == 8) ? 3 : 2);
      }
   else
      {
      generateShiftRightLogicalImmediate(cg, node, tempReg, byteLenRemainingRegister, (byteLen == 8) ? 3 : 2);
      }
   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);

   loopStartLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStartLabel);

   residueEndLabel = generateLabelSymbol(cg);
   resultLabel = generateLabelSymbol(cg);
   cntrLabel = generateLabelSymbol(cg);
   midLabel = generateLabelSymbol(cg);
   result2Label = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();

   TR::Register *src1Reg = cg->allocateRegister(TR_GPR);
   TR::Register *src2Reg = cg->allocateRegister(TR_GPR);

   if (byteLen == 4)
      {
      generateTrg1MemInstruction (cg, TR::InstOpCode::lwzu, node, src1Reg, TR::MemoryReference::createWithDisplacement(cg, src1AddrReg, 4, 4));
      generateTrg1MemInstruction (cg, TR::InstOpCode::lwzu, node, src2Reg, TR::MemoryReference::createWithDisplacement(cg, src2AddrReg, 4, 4));
      }
   else
      {
      generateTrg1MemInstruction (cg, TR::InstOpCode::ldu, node, src1Reg, TR::MemoryReference::createWithDisplacement(cg, src1AddrReg, 8, 8));
      generateTrg1MemInstruction (cg, TR::InstOpCode::ldu, node, src2Reg, TR::MemoryReference::createWithDisplacement(cg, src2AddrReg, 8, 8));
      }

   TR::Register *ccReg = nullptr;
   TR::Register *lowReturnReg = nullptr;
   TR::Register *highReturnReg = nullptr;

   if (!is64bit && isArrayCmpLen)
      {
      lowReturnReg = cg->allocateRegister(TR_GPR);
      highReturnReg = cg->allocateRegister(TR_GPR);
      ccReg = cg->allocateRegisterPair(lowReturnReg, highReturnReg);
      }
   else
      {
      ccReg = cg->allocateRegister(TR_GPR);
      }


   generateTrg1Src2Instruction(cg, (byteLen == 8) ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmp4, node, condReg, src1Reg, src2Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, residueStartLabel, condReg);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopStartLabel, condReg);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src1AddrReg, src1AddrReg, byteLen);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src2AddrReg, src2AddrReg, byteLen);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, residueStartLabel);

   generateTrg1Instruction(cg, TR::InstOpCode::mfctr, node, byteLenRemainingRegister);

   generateTrg1Src1ImmInstruction(cg, (is64bit && isArrayCmpLen) ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpli4, node, condReg2, byteLenRemainingRegister, 0);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, byteLenRemainingRegister, byteLenRemainingRegister, tempReg);

   if (is64bit && isArrayCmpLen)
      generateShiftLeftImmediateLong(cg, node, byteLenRemainingRegister, byteLenRemainingRegister, (byteLen == 8) ? 3 : 2);
   else
      generateShiftLeftImmediate(cg, node, byteLenRemainingRegister, byteLenRemainingRegister, (byteLen == 8) ? 3 : 2);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, midLabel, condReg2);

   generateTrg1Src2Instruction(cg, (is64bit && isArrayCmpLen) ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmpl4, node, condReg2, byteLenRemainingRegister, byteLenRegister);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, midLabel);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, byteLenRemainingRegister, byteLenRemainingRegister, byteLenRegister);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, mid2Label);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, resultLabel, condReg2);

   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, byteLenRemainingRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src1AddrReg, src1AddrReg, -1);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, src2AddrReg, src2AddrReg, -1);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, residueLoopStartLabel);

   generateTrg1MemInstruction (cg, TR::InstOpCode::lbzu, node, src1Reg, TR::MemoryReference::createWithDisplacement(cg, src1AddrReg, 1, 1));
   generateTrg1MemInstruction (cg, TR::InstOpCode::lbzu, node, src2Reg, TR::MemoryReference::createWithDisplacement(cg, src2AddrReg, 1, 1));

   generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, src1Reg, src2Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, cntrLabel, condReg);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, residueLoopStartLabel, condReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, cntrLabel);

   generateTrg1Instruction(cg, TR::InstOpCode::mfctr, node, byteLenRemainingRegister);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, resultLabel);

   if (isArrayCmpLen)
      {
      if (is64bit)
         {
         generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, ccReg, byteLenRemainingRegister, byteLenRegister);
         }
      else
         {
         generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, lowReturnReg, byteLenRemainingRegister, byteLenRegister);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, highReturnReg, 0);
         }
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg2, byteLenRemainingRegister, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, result2Label, condReg2);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, ccReg, 0);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, residueEndLabel);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, result2Label);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, ccReg, 1);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, residueEndLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, ccReg, 2);
      }

   int32_t numRegs = 10;
   if (!is64bit && isArrayCmpLen)
      {
      numRegs = 11;
      }

   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numRegs, cg->trMemory());
   dependencies->addPostCondition(src1Reg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(src2Reg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(src1AddrReg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(src2AddrReg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(byteLenRegister, TR::RealRegister::NoReg);
   dependencies->addPostCondition(byteLenRemainingRegister, TR::RealRegister::NoReg);
   dependencies->addPostCondition(tempReg, TR::RealRegister::NoReg);
   if (!is64bit && isArrayCmpLen)
      {
      dependencies->addPostCondition(lowReturnReg, TR::RealRegister::NoReg);
      dependencies->addPostCondition(highReturnReg, TR::RealRegister::NoReg);
      }
   else
      {
      dependencies->addPostCondition(ccReg, TR::RealRegister::NoReg);
      }
   dependencies->addPostCondition(condReg, TR::RealRegister::NoReg);
   dependencies->addPostCondition(condReg2, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, residueEndLabel, dependencies);
   residueEndLabel->setEndInternalControlFlow();

   node->setRegister(ccReg);

   cg->stopUsingRegister(src1Reg);
   cg->stopUsingRegister(src2Reg);
   if (condReg != NULL)
      cg->stopUsingRegister(condReg);
   if (condReg2 != NULL)
      cg->stopUsingRegister(condReg2);
   if (byteLenRemainingRegister != NULL)
      cg->stopUsingRegister(byteLenRemainingRegister);
    if (tempReg != NULL)
      cg->stopUsingRegister(tempReg);

   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode);
   cg->decReferenceCount(lengthNode);

   cg->stopUsingRegister(src1AddrReg);
   cg->stopUsingRegister(src2AddrReg);

   return ccReg;
   }

TR::Register *OMR::Power::TreeEvaluator::arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineArrayCmp(node, cg, false);
   }

TR::Register *OMR::Power::TreeEvaluator::arraycmplenEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineArrayCmp(node, cg, true);
   }

bool OMR::Power::TreeEvaluator::stopUsingCopyReg(
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

         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, copyReg, reg);
         reg = copyReg;
         return true;
         }
      }

   return false;
   }

TR::Instruction*
OMR::Power::TreeEvaluator::generateHelperBranchAndLinkInstruction(
      TR_RuntimeHelper helperIndex,
      TR::Node* node,
      TR::RegisterDependencyConditions *conditions,
      TR::CodeGenerator* cg)
   {
   TR::SymbolReference *helperSym =
      cg->symRefTab()->findOrCreateRuntimeHelper(helperIndex);

   return generateDepImmSymInstruction(
      cg, TR::InstOpCode::bl, node,
      (uintptr_t)helperSym->getMethodAddress(),
      conditions, helperSym);
   }

TR::Register *OMR::Power::TreeEvaluator::setmemoryEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node             *dstAddrNode, *lengthNode, *valueNode;
   dstAddrNode = node->getChild(0);
   lengthNode = node->getChild(1);
   valueNode = node->getChild(2);

   TR::Register         *dstAddrReg, *lengthReg, *valueReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2 = false, stopUsingCopyReg3 = false;

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyReg(dstAddrNode, dstAddrReg, cg);

   lengthReg = cg->evaluate(lengthNode);
   if (!cg->canClobberNodesRegister(lengthNode))
      {
	  TR::Register *lenCopyReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, lengthNode, lenCopyReg, lengthReg);
      lengthReg = lenCopyReg;
      stopUsingCopyReg2 = true;
      }

   valueReg = cg->evaluate(valueNode);
   if (!cg->canClobberNodesRegister(valueNode))
      {
	  TR::Register *valCopyReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, valueNode, valCopyReg, valueReg);
      valueReg = valCopyReg;
      stopUsingCopyReg3 = true;
      }

   TR::LabelSymbol * residualLabel =  generateLabelSymbol(cg);
   TR::LabelSymbol * loopStartLabel =  generateLabelSymbol(cg);
   TR::LabelSymbol * doneLabel =  generateLabelSymbol(cg);
   TR::LabelSymbol * label8aligned =  generateLabelSymbol(cg);
   TR::LabelSymbol * label4aligned =  generateLabelSymbol(cg);
   TR::LabelSymbol * label2aligned =  generateLabelSymbol(cg);
   TR::LabelSymbol * label1aligned =  generateLabelSymbol(cg);

   TR::RegisterDependencyConditions *conditions;
   int32_t numDeps = 5;
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numDeps, numDeps, cg->trMemory());
   TR::Register *cndReg = cg->allocateRegister(TR_CCR);
   TR::addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   TR::addDependency(conditions, dstAddrReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, lengthReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, valueReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::Register * tempReg = cg->allocateRegister();
   TR::addDependency(conditions, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);

   // assemble the double word value from byte value
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, valueReg, valueReg,  8, 0xff00);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, valueReg, valueReg,  16, 0xffff0000);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, valueReg, valueReg,  32, 0xffffffff00000000);

   generateTrg1Src1ImmInstruction(cg, lengthNode->getType().isInt32() ? TR::InstOpCode::cmpli4 : TR::InstOpCode::cmpli8, node, cndReg, lengthReg, 32);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, residualLabel, cndReg);

   generateTrg1Src1ImmInstruction(cg, lengthNode->getType().isInt32() ? TR::InstOpCode::srawi : TR::InstOpCode::sradi, node, tempReg, lengthReg, 5);
   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStartLabel);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 0, 8), valueReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 8, 8), valueReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 16, 8), valueReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 24, 8), valueReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstAddrReg, dstAddrReg, 32);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopStartLabel, cndReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, residualLabel); //check 16 aligned
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lengthReg, 16);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label8aligned, cndReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 0, 8), valueReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 8, 8), valueReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstAddrReg, dstAddrReg, 16);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, label8aligned); //check 8 aligned
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lengthReg, 8);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label4aligned, cndReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 0, 8), valueReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstAddrReg, dstAddrReg, 8);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, label4aligned); //check 4 aligned
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lengthReg, 4);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label2aligned, cndReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 0, 4), valueReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstAddrReg, dstAddrReg, 4);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, label2aligned); //check 2 aligned
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lengthReg, 2);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label1aligned, cndReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 0, 2), valueReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstAddrReg, dstAddrReg, 2);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, label1aligned); //check 1 aligned
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lengthReg, 1);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node, TR::MemoryReference::createWithDisplacement(cg, dstAddrReg, 0, 1), valueReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   if (stopUsingCopyReg1)
      cg->stopUsingRegister(dstAddrReg);
   if (stopUsingCopyReg2)
      cg->stopUsingRegister(lengthReg);
   if (stopUsingCopyReg3)
      cg->stopUsingRegister(valueReg);

   cg->stopUsingRegister(cndReg);
   cg->stopUsingRegister(tempReg);

   cg->decReferenceCount(dstAddrNode);
   cg->decReferenceCount(lengthNode);
   cg->decReferenceCount(valueNode);

   return(NULL);
   }

TR::Register *OMR::Power::TreeEvaluator::arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (debug("noArrayCopy"))
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = TR::TreeEvaluator::directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   bool simpleCopy = (node->getNumChildren()==3);
   bool arrayStoreCheckIsNeeded = !simpleCopy && !node->chkNoArrayStoreCheckArrayCopy();

   // Evaluate for fast arrayCopy implementation. For simple arrayCopy:
   //      child 0 ------  Source byte address;
   //      child 1 ------  Destination byte address;
   //      child 2 ------  Copy length in byte;
   // Otherwise:
   //      child 0 ------  Source array object;
   //      child 1 ------  Destination array object;
   //      child 2 ------  Source byte address;
   //      child 3 ------  Destination byte address;
   //      child 4 ------  Copy length in byte;

   if (arrayStoreCheckIsNeeded)
      {
      // call the "C" helper, handle the exception case
#ifdef J9_PROJECT_SPECIFIC
      TR::TreeEvaluator::genArrayCopyWithArrayStoreCHK(node, cg);
#endif
      return(NULL);
      }
   TR::Compilation *comp = cg->comp();
   TR::Instruction      *gcPoint;
   TR::Node             *srcObjNode, *dstObjNode, *srcAddrNode, *dstAddrNode, *lengthNode;
   TR::Register         *srcObjReg=NULL, *dstObjReg=NULL, *srcAddrReg, *dstAddrReg, *lengthReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4, stopUsingCopyReg5 = false;

   if (simpleCopy)
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

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyReg(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyReg(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyReg(srcAddrNode, srcAddrReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyReg(dstAddrNode, dstAddrReg, cg);

   // Inline forward arrayCopy with constant length, call the wrtbar if needed after the copy
   if ((simpleCopy || !arrayStoreCheckIsNeeded) &&
        (node->isForwardArrayCopy() || alwaysInlineArrayCopy(cg)) && lengthNode->getOpCode().isLoadConst())
      {
      int64_t len = (lengthNode->getType().isInt32() ?
                     lengthNode->getInt() : lengthNode->getLongInt());

      // inlineArrayCopy is not currently capable of handling very long lengths correctly. Under some circumstances, it
      // will generate an li instruction with an out-of-bounds immediate, which triggers an assert in the binary
      // encoder.
      if (len>=0 && len < MAX_PPC_ARRAYCOPY_INLINE)
         {
         inlineArrayCopy(node, len, srcAddrReg, dstAddrReg, cg);
         if (!simpleCopy)
           {
#ifdef J9_PROJECT_SPECIFIC
           TR::TreeEvaluator::genWrtbarForArrayCopy(node, srcObjReg, dstObjReg, cg);
#endif
           cg->decReferenceCount(srcObjNode);
           cg->decReferenceCount(dstObjNode);
           }
         if (stopUsingCopyReg1)
            cg->stopUsingRegister(srcObjReg);
         if (stopUsingCopyReg2)
            cg->stopUsingRegister(dstObjReg);
         if (stopUsingCopyReg3)
            cg->stopUsingRegister(srcAddrReg);
         if (stopUsingCopyReg4)
            cg->stopUsingRegister(dstAddrReg);
         cg->decReferenceCount(srcAddrNode);
         cg->decReferenceCount(dstAddrNode);
         cg->decReferenceCount(lengthNode);
         return NULL;
         }
      }

   lengthReg = cg->evaluate(lengthNode);
   if (!cg->canClobberNodesRegister(lengthNode))
      {
      TR::Register *lenCopyReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, lengthNode, lenCopyReg, lengthReg);
      lengthReg = lenCopyReg;
      stopUsingCopyReg5 = true;
      }

   bool  postP10Copy = cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10) && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX);

   static bool disableVSXArrayCopy  = (feGetEnv("TR_disableVSXArrayCopy") != NULL);
   bool  copyUsingVSX = (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8)) && !disableVSXArrayCopy && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX);

   // Disabling this or checking for FPU doesn't make senses at all, since VSX supercedes FPU
   // And, LE only starts with POWER8 (so, always falling under copyUsingVSX)
   //    On POWER8, unaligned integer accesses in LE mode are potentially micro-coded, breaking
   //    the guarantee of data atomicity. Instead, we are using floating point or vector access
   //    to replace it.
   bool  additionalLERequirement = cg->comp()->target().cpu.isLittleEndian();

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   static bool verboseArrayCopy = (feGetEnv("TR_verboseArrayCopy") != NULL);       //Check which helper is getting used.
   if(verboseArrayCopy)
      fprintf(stderr, "arraycopy [0x%p] isReferenceArrayCopy:[%d] isForwardArrayCopy:[%d] isHalfWordElementArrayCopy:[%d] isWordElementArrayCopy:[%d] %s @ %s\n",
               node,
               0,
               node->isForwardArrayCopy(),
               node->isHalfWordElementArrayCopy(),
               node->isWordElementArrayCopy(),
               comp->signature(),
               comp->getHotnessName(comp->getMethodHotness())
               );
#endif

   TR::RegisterDependencyConditions *conditions;
   int32_t numDeps = 0;
   if (postP10Copy)
      {
      numDeps = 10;
      }
   else if(copyUsingVSX)
      {
      numDeps = cg->comp()->target().is64Bit() ? 10 : 13;
      if (additionalLERequirement)
         {
         numDeps += 4;
         }
      }
   else if (cg->comp()->target().is64Bit())
      {
      if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
         numDeps = 12;
      else
         numDeps = 8;
      }
   else if (cg->comp()->target().cpu.hasFPU())
      numDeps = 15;
   else
      numDeps = 11;

   TR_ASSERT(numDeps != 0, "numDeps not set correctly\n");
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numDeps, numDeps, cg->trMemory());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *tmp1Reg = cg->allocateRegister(TR_GPR);
   TR::Register *tmp2Reg = cg->allocateRegister(TR_GPR);
   TR::Register *tmp4Reg = cg->allocateRegister(TR_GPR);

   TR::Register *tmp3Reg = NULL;
   TR::Register *fp1Reg = NULL;
   TR::Register *fp2Reg = NULL;
   TR::Register *vec0Reg = NULL;
   TR::Register *vec1Reg = NULL;

   // Build up the dependency conditions
   TR::addDependency(conditions, cndRegister, TR::RealRegister::cr0, TR_CCR, cg);
   TR::addDependency(conditions, lengthReg, TR::RealRegister::gr7, TR_GPR, cg);
   TR::addDependency(conditions, srcAddrReg, TR::RealRegister::gr8, TR_GPR, cg);
   TR::addDependency(conditions, dstAddrReg, TR::RealRegister::gr9, TR_GPR, cg);
   // trampoline kills gr11
   TR::addDependency(conditions, tmp4Reg, TR::RealRegister::gr11, TR_GPR, cg);

   if (postP10Copy)
      {
      TR::addDependency(conditions, NULL, TR::RealRegister::vsr32, TR_VSX_VECTOR, cg);
      TR::addDependency(conditions, NULL, TR::RealRegister::vsr33, TR_VSX_VECTOR, cg);
      TR::addDependency(conditions, NULL, TR::RealRegister::vsr8, TR_VSX_VECTOR, cg);
      TR::addDependency(conditions, NULL, TR::RealRegister::vsr9, TR_VSX_VECTOR, cg);
      if (!node->isForwardArrayCopy())
         {
         TR::addDependency(conditions, NULL, TR::RealRegister::gr5, TR_GPR, cg);
         }
      }
   else
      {
      tmp3Reg = cg->allocateRegister(TR_GPR);
      TR::addDependency(conditions, tmp3Reg, TR::RealRegister::gr0, TR_GPR, cg);

      // Call the right version of arrayCopy code to do the job
      TR::addDependency(conditions, tmp1Reg, TR::RealRegister::gr5, TR_GPR, cg);
      TR::addDependency(conditions, tmp2Reg, TR::RealRegister::gr6, TR_GPR, cg);

      if (copyUsingVSX)
         {
         vec0Reg = cg->allocateRegister(TR_VRF);
         vec1Reg = cg->allocateRegister(TR_VRF);
         TR::addDependency(conditions, vec0Reg, TR::RealRegister::vr0, TR_VRF, cg);
         TR::addDependency(conditions, vec1Reg, TR::RealRegister::vr1, TR_VRF, cg);
         if (cg->comp()->target().is32Bit())
            {
            TR::addDependency(conditions, NULL, TR::RealRegister::gr3, TR_GPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::gr4, TR_GPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::gr10, TR_GPR, cg);
            }
         if (additionalLERequirement)
            {
            fp1Reg = cg->allocateSinglePrecisionRegister();
            fp2Reg = cg->allocateSinglePrecisionRegister();
            TR::addDependency(conditions, fp1Reg, TR::RealRegister::fp8, TR_FPR, cg);
            TR::addDependency(conditions, fp2Reg, TR::RealRegister::fp9, TR_FPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::fp10, TR_FPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::fp11, TR_FPR, cg);
            }
         }
      else if (cg->comp()->target().is32Bit())
         {
         TR::addDependency(conditions, NULL, TR::RealRegister::gr3, TR_GPR, cg);
         TR::addDependency(conditions, NULL, TR::RealRegister::gr4, TR_GPR, cg);
         TR::addDependency(conditions, NULL, TR::RealRegister::gr10, TR_GPR, cg);
         if (cg->comp()->target().cpu.hasFPU())
            {
            TR::addDependency(conditions, NULL, TR::RealRegister::fp8, TR_FPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::fp9, TR_FPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::fp10, TR_FPR, cg);
            TR::addDependency(conditions, NULL, TR::RealRegister::fp11, TR_FPR, cg);
            }
         }
      else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
         {
         // stfdp arrayCopy used
         TR::addDependency(conditions, NULL, TR::RealRegister::fp8, TR_FPR, cg);
         TR::addDependency(conditions, NULL, TR::RealRegister::fp9, TR_FPR, cg);
         TR::addDependency(conditions, NULL, TR::RealRegister::fp10, TR_FPR, cg);
         TR::addDependency(conditions, NULL, TR::RealRegister::fp11, TR_FPR, cg);
         }
      }

   TR_RuntimeHelper helper;

   if (node->isForwardArrayCopy())
      {
      if (postP10Copy)
         {
         helper = TR_PPCpostP10ForwardCopy;
         }
      else if (copyUsingVSX)
         {
         helper = TR_PPCforwardQuadWordArrayCopy_vsx;
         }
      else if (node->isWordElementArrayCopy())
         {
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
            helper = TR_PPCforwardWordArrayCopy_dp;
         else
            helper = TR_PPCforwardWordArrayCopy;
         }
      else if (node->isHalfWordElementArrayCopy())
         {
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
            helper = TR_PPCforwardHalfWordArrayCopy_dp;
         else
            helper = TR_PPCforwardHalfWordArrayCopy;
         }
      else
         {
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
            helper = TR_PPCforwardArrayCopy_dp;
         else
            {
            helper = TR_PPCforwardArrayCopy;
            }
         }
      }
   else // We are not sure it is forward or we have to do backward
      {
      if (postP10Copy)
         {
         helper = TR_PPCpostP10GenericCopy;
         }
      else if (copyUsingVSX)
         {
         helper = TR_PPCquadWordArrayCopy_vsx;
         }
      else if (node->isWordElementArrayCopy())
         {
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
            helper = TR_PPCwordArrayCopy_dp;
         else
            helper = TR_PPCwordArrayCopy;
         }
      else if (node->isHalfWordElementArrayCopy())
         {
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
            helper = TR_PPChalfWordArrayCopy_dp;
         else
            helper = TR_PPChalfWordArrayCopy;
         }
      else
         {
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
            helper = TR_PPCarrayCopy_dp;
         else
            {
            helper = TR_PPCarrayCopy;
            }
         }
      }
   TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(helper, node, conditions, cg);

   conditions->stopUsingDepRegs(cg);

#ifdef J9_PROJECT_SPECIFIC
   if (!simpleCopy)
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
   if (stopUsingCopyReg5)
      cg->stopUsingRegister(lengthReg);
   if (tmp1Reg)
      cg->stopUsingRegister(tmp1Reg);
   if (tmp2Reg)
      cg->stopUsingRegister(tmp2Reg);
   if (tmp3Reg)
      cg->stopUsingRegister(tmp3Reg);
   if (tmp4Reg)
      cg->stopUsingRegister(tmp4Reg);
   if (fp1Reg)
      cg->stopUsingRegister(fp1Reg);
   if (fp2Reg)
      cg->stopUsingRegister(fp2Reg);
   if (vec0Reg)
      cg->stopUsingRegister(vec0Reg);
   if (vec1Reg)
      cg->stopUsingRegister(vec1Reg);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   return(NULL);
   }

static TR::Register *inlineFixedTrg1Src1(
      TR::Node *node,
      TR::InstOpCode::Mnemonic op,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineFixedTrg1Src1");

   TR::Node        *firstChild  = node->getFirstChild();
   TR::Register    *srcRegister = cg->evaluate(firstChild);
   TR::Register    *targetRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, op, node, targetRegister, srcRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineLongNumberOfLeadingZeros(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineLongNumberOfLeadingZeros");

   TR::Node *firstChild  = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Register *tempRegister = cg->allocateRegister();
   TR::Register *maskRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister->getLowOrder());
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, maskRegister, targetRegister, 27, 0x1);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, maskRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tempRegister, tempRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, targetRegister, targetRegister, tempRegister);
   // An alternative to generating this right shift/neg/and sequence is:
   // mask = targetRegister << 26
   // mask = mask >> 31 (algebraic shift to replicate sign bit)
   // tempRegister &= mask
   // add targetRegister, targetRegister, tempRegister
   // Of course, there is also the alternative of:
   // cmpwi srcRegister->getHighOrder(), 0
   // bne over
   // add targetRegister, targetRegister, tempRegister
   // over:

   cg->stopUsingRegister(tempRegister);
   cg->stopUsingRegister(maskRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineNumberOfTrailingZeros(TR::Node *node, TR::InstOpCode::Mnemonic op, int32_t subfconst, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineNumberOfTrailingZeros");

   TR::Node        *firstChild  = node->getFirstChild();
   TR::Register    *srcRegister = cg->evaluate(firstChild);
   TR::Register    *targetRegister = cg->allocateRegister();

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, targetRegister, srcRegister, -1);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, targetRegister, targetRegister, srcRegister);
   generateTrg1Src1Instruction(cg, op, node, targetRegister, targetRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, targetRegister, targetRegister, subfconst);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineLongBitCount(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineLongBitCount");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Register *tempRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::popcntw, node, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src1Instruction(cg, TR::InstOpCode::popcntw, node, tempRegister, srcRegister->getLowOrder());
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, targetRegister, targetRegister, tempRegister);

   cg->stopUsingRegister(tempRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineLongNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineLongNumberOfTrailingZeros");

   TR::Node        *firstChild  = node->getFirstChild();
   TR::Register    *srcRegister = cg->evaluate(firstChild);
   TR::Register    *targetRegister = cg->allocateRegister();
   TR::Register    *tempRegister = cg->allocateRegister();
   TR::Register    *maskRegister = cg->allocateRegister();

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, tempRegister, srcRegister->getLowOrder(), -1);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::addme, node, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, tempRegister, tempRegister, srcRegister->getLowOrder());
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, targetRegister, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, targetRegister, targetRegister);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, tempRegister);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, maskRegister, targetRegister, 27, 0x1);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, maskRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tempRegister, tempRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, targetRegister, targetRegister, tempRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, targetRegister, targetRegister, 64);

   cg->stopUsingRegister(tempRegister);
   cg->stopUsingRegister(maskRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineIntegerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineIntegerHighestOneBit");

   TR::Node        *firstChild  = node->getFirstChild();
   TR::Register    *srcRegister = cg->evaluate(firstChild);
   TR::Register    *targetRegister = cg->allocateRegister();
   TR::Register    *tempRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister, 0xffff8000u);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::srw, node, targetRegister, targetRegister, tempRegister);

   cg->stopUsingRegister(tempRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineLongHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineLongHighestOneBit");

   TR::Node        *firstChild  = node->getFirstChild();
   TR::Register    *srcRegister = cg->evaluate(firstChild);

   if (cg->comp()->target().is64Bit())
      {
      TR::Register    *targetRegister = cg->allocateRegister();
      TR::Register    *tempRegister = cg->allocateRegister();

      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzd, node, tempRegister, srcRegister);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister, 0xffff8000u);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, targetRegister, targetRegister, 32, CONSTANT64(0x8000000000000000));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::srd, node, targetRegister, targetRegister, tempRegister);

      cg->stopUsingRegister(tempRegister);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);

      return targetRegister;
      }
   else
      {
      TR::RegisterPair *targetRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      TR::Register    *tempRegister = cg->allocateRegister();
      TR::Register    *condReg = cg->allocateRegister(TR_CCR);
      TR::LabelSymbol *jumpLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, srcRegister->getHighOrder(), 0);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister->getHighOrder());
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister->getHighOrder(), 0xffff8000u);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, targetRegister->getLowOrder(), 0);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::srw, node, targetRegister->getHighOrder(), targetRegister->getHighOrder(), tempRegister);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister->getLowOrder());
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister->getLowOrder(), 0xffff8000u);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, targetRegister->getHighOrder(), 0);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::srw, node, targetRegister->getLowOrder(), targetRegister->getLowOrder(), tempRegister);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

      cg->stopUsingRegister(tempRegister);
      cg->stopUsingRegister(condReg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);

      return targetRegister;
      }
   }

TR::Register *OMR::Power::TreeEvaluator::PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT( node->getNumChildren() == 4, "TR::Prefetch should contain 4 child nodes");

   TR::Compilation *comp = cg->comp();
   TR::Node  *firstChild  =  node->getFirstChild();
   TR::Node  *secondChild =  node->getChild(1);
   TR::Node  *sizeChild =  node->getChild(2);
   TR::Node  *typeChild =  node->getChild(3);

   static char * disablePrefetch = feGetEnv("TR_DisablePrefetch");
   if (disablePrefetch)
      {
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      cg->recursivelyDecReferenceCount(sizeChild);
      cg->recursivelyDecReferenceCount(typeChild);
      return NULL;
      }

   // Size
   cg->recursivelyDecReferenceCount(sizeChild);

   // Type
   uint32_t type = typeChild->getInt();
   cg->recursivelyDecReferenceCount(typeChild);

   TR::InstOpCode::Mnemonic prefetchOp = TR::InstOpCode::bad;

   if (type == PrefetchLoad)
      {
      prefetchOp = TR::InstOpCode::dcbt;
      }
#ifdef NTA_ENABLED
   else if (type == PrefetchLoadNonTemporal)
      {
      prefetchOp = TR::InstOpCode::dcbtt;
      }
#endif
   else if (type == PrefetchStore)
      {
      prefetchOp = TR::InstOpCode::dcbtst;
      }
#ifdef NTA_ENABLED
   else if (type == PrefetchStoreNonTemporal)
      {
      prefetchOp = TR::InstOpCode::dcbtst;
      }
#endif
   else
      {
      traceMsg(comp,"Prefetching for type %d not implemented/supported on PPC.\n",type);
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      return NULL;
      }

   TR::Register *baseReg = cg->evaluate(firstChild);
   TR::Register *indexReg = NULL;

   if (secondChild->getOpCode().isLoadConst())
      {
      if (secondChild->getInt() != 0)
         {
         indexReg = cg->allocateRegister();
         loadConstant(cg, node, secondChild->getInt(), indexReg);
         }
      }
   else
      indexReg = cg->evaluate(secondChild);

   TR::MemoryReference *memRef = indexReg ?
      TR::MemoryReference::createWithIndexReg(cg, baseReg, indexReg, sizeChild->getInt()) :
      TR::MemoryReference::createWithIndexReg(cg, NULL, baseReg, sizeChild->getInt());

   generateMemInstruction(cg, prefetchOp, node, memRef);

   if (secondChild->getOpCode().isLoadConst() && secondChild->getInt() != 0)
      cg->stopUsingRegister(indexReg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return NULL;
   }

// handles: TR::call, TR::acall, TR::icall, TR::lcall, TR::fcall, TR::dcall
TR::Register *OMR::Power::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *resultReg;

   if (!cg->inlineDirectCall(node, resultReg))
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::SymbolReferenceTable *symRefTab = cg->comp()->getSymRefTab();

      // Non-helpers supported by code gen. are expected to be inlined
      if (symRefTab->isNonHelper(symRef))
         {
         TR_ASSERT(!cg->supportsNonHelper(symRefTab->getNonHelperSymbol(symRef)),
                   "Non-helper %d was not inlined, but was expected to be.\n",
                   symRefTab->getNonHelperSymbol(symRef));
         }

      TR::MethodSymbol *callee = symRef->getSymbol()->castToMethodSymbol();
      TR::Linkage *linkage = cg->getLinkage(callee->getLinkageConvention());
      resultReg = linkage->buildDirectDispatch(node);
      }

   return resultReg;
   }

static TR::Register *inlineSimpleAtomicUpdate(TR::Node *node, bool isAddOp, bool isLong, bool isGetThenUpdate, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "Atomic non-helpers are only supported in 64-bit mode\n");

   TR::Node *valueAddrChild = node->getFirstChild();
   TR::Node *deltaChild = NULL;

   TR::Register *valueAddrReg = cg->evaluate(valueAddrChild);
   TR::Register *deltaReg = NULL;
   TR::Register *newValueReg = NULL;
   TR::Register *oldValueReg = cg->allocateRegister();
   TR::Register *cndReg = cg->allocateRegister(TR_CCR);
   TR::Register *resultReg = NULL;

   bool isDeltaImmediate = false;
   bool isDeltaImmediateShifted = false;
   bool localDeltaReg = false;

   int32_t numDeps = 3;

   int32_t delta = 0;

   // Memory barrier --- NOTE: we should be able to do a test upfront to save this barrier,
   //                          but Hursley advised to be conservative due to lack of specification.
   generateInstruction(cg, TR::InstOpCode::lwsync, node);

   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   loopLabel->setStartInternalControlFlow();
   deltaChild = node->getSecondChild();

   // Determine whether immediate instruction can be used.  For 64 bit operand,
   // value has to fit in 32 bits to be considered for immediate operation
   if (deltaChild->getOpCode().isLoadConst()
          && !deltaChild->getRegister()
          && (deltaChild->getDataType() == TR::Int32
                 || (deltaChild->getDataType() == TR::Int64
                        && deltaChild->getLongInt() <= 0x000000007FFFFFFFL
                        && deltaChild->getLongInt() >= 0xFFFFFFFF80000000L)))
      {
      const int64_t deltaLong = (deltaChild->getDataType() == TR::Int64)
                                   ? deltaChild->getLongInt()
                                   : (int64_t) deltaChild->getInt();

      delta = (int32_t) deltaChild->getInt();

      //determine if the constant can be represented as an immediate
      if (deltaLong <= UPPER_IMMED && deltaLong >= LOWER_IMMED)
         {
         // avoid evaluating immediates for add operations
         isDeltaImmediate = true;
         }
      else if ((delta == deltaLong) && (0 == (deltaLong & 0xFFFF)))
         {
         // avoid evaluating shifted immediates for add operations
         isDeltaImmediate = true;
         isDeltaImmediateShifted = true;
         }
      else
         {
         numDeps++;
         deltaReg = cg->evaluate(deltaChild);
         }
      }
   else
      {
      numDeps++;
      deltaReg = cg->evaluate(deltaChild);
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

   if (isDeltaImmediate && !isAddOp)
      {
      // If argument is immediate, but the operation is not an add,
      // the value must still be loaded into a register
      // If argument is an immediate value and operation is an add,
      // it will be used as an immediate operand in an add immediate instruction
      numDeps++;
      deltaReg = cg->allocateRegister();
      loadConstant(cg, node, delta, deltaReg);
      localDeltaReg = true;
      }

   const uint8_t len = isLong ? 8 : 4;

   generateTrg1MemInstruction(cg, isLong ? TR::InstOpCode::ldarx : TR::InstOpCode::lwarx, node, oldValueReg,
         TR::MemoryReference::createWithIndexReg(cg, 0, valueAddrReg, len));

   if (isAddOp)
      {
      numDeps++;
      newValueReg = cg->allocateRegister();

      if (isDeltaImmediateShifted)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, newValueReg, oldValueReg, delta >> 16);
      else if (isDeltaImmediate)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, newValueReg, oldValueReg, delta);
      else
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, newValueReg, oldValueReg, deltaReg);
      }
   else
      {
      newValueReg = deltaReg;
      deltaReg = NULL;
      }

   generateMemSrc1Instruction(cg, isLong ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r, node, TR::MemoryReference::createWithIndexReg(cg, 0, valueAddrReg, len),
         newValueReg);

   // We expect this store is usually successful, i.e., the following branch will not be taken
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, cndReg);

   // We deviate from the VM helper here: no-store-no-barrier instead of always-barrier
   generateInstruction(cg, TR::InstOpCode::sync, node);

   TR::RegisterDependencyConditions *conditions;

   //Set the conditions and dependencies
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, (uint16_t) numDeps, cg->trMemory());

   conditions->addPostCondition(valueAddrReg, TR::RealRegister::NoReg);
   conditions->addPostCondition(oldValueReg, TR::RealRegister::NoReg);
   conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();
   conditions->addPostCondition(cndReg, TR::RealRegister::cr0);
   numDeps = numDeps - 3;

   if (newValueReg)
      {
      conditions->addPostCondition(newValueReg, TR::RealRegister::NoReg);
      numDeps--;
      }

   if (deltaReg)
      {
      conditions->addPostCondition(deltaReg, TR::RealRegister::NoReg);
      numDeps--;
      }

   doneLabel->setEndInternalControlFlow();
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   cg->decReferenceCount(valueAddrChild);
   cg->stopUsingRegister(cndReg);

   if (deltaChild)
      {
      cg->decReferenceCount(deltaChild);
      }

   if (isGetThenUpdate)
      {
      // For Get And Op, the result is in the register with the old value
      // The new value is no longer needed
      resultReg = oldValueReg;
      node->setRegister(oldValueReg);
      cg->stopUsingRegister(newValueReg);
      }
   else
      {
      // For Op And Get, we will store the return value in the register that
      // contains the sum.  The register with the old value is no longer needed
      resultReg = newValueReg;
      node->setRegister(newValueReg);
      cg->stopUsingRegister(oldValueReg);
      }

   if (localDeltaReg)
      {
      cg->stopUsingRegister(deltaReg);
      }

   TR_ASSERT(numDeps == 0, "Missed a register dependency in inlineSimpleAtomicOperations - numDeps == %d\n", numDeps);

   return resultReg;
   }

bool OMR::Power::CodeGenerator::inlineDirectCall(TR::Node *node, TR::Register *&resultReg)
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference* symRef = node->getSymbolReference();
   bool doInline = false;

   if (symRef && symRef->getSymbol()->castToMethodSymbol()->isInlinedByCG())
      {
      bool isAddOp = false;
      bool isLong = false;
      bool isGetThenUpdate = false;

      if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicAddSymbol))
         {
         isAddOp = true;
         isLong = !node->getChild(1)->getDataType().isInt32();
         isGetThenUpdate = false;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicFetchAndAddSymbol))
         {
         isAddOp = true;
         isLong = !node->getChild(1)->getDataType().isInt32();
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicFetchAndAdd8BitSymbol))
         {
         isAddOp = true;
         isLong = false;
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicFetchAndAdd16BitSymbol))
         {
         isAddOp = true;
         isLong = false;
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicFetchAndAdd32BitSymbol))
         {
         isAddOp = true;
         isLong = false;
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicFetchAndAdd64BitSymbol))
         {
         isAddOp = true;
         isLong = true;
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicSwapSymbol))
         {
         isAddOp = false;
         isLong = !node->getChild(1)->getDataType().isInt32();
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicSwap32BitSymbol))
         {
         isAddOp = false;
         isLong = false;
         isGetThenUpdate = true;
         doInline = true;
         }
      else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicSwap64BitSymbol))
         {
         isAddOp = false;
         isLong = true;
         isGetThenUpdate = true;
         doInline = true;
         }

      if (doInline)
         {
         resultReg = inlineSimpleAtomicUpdate(node, isAddOp, isLong, isGetThenUpdate, cg);
         }
      }

   return doInline;
   }

TR::Register *OMR::Power::TreeEvaluator::performCall(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg)
   {
   TR::Linkage      *linkage = cg->deriveCallingLinkage(node, isIndirect);
   TR::Register *returnRegister;

   if (isIndirect)
      returnRegister = linkage->buildIndirectDispatch(node);
   else
      returnRegister = linkage->buildDirectDispatch(node);

   return returnRegister;
   }

// handles: TR::icalli, TR::acalli, TR::fcalli, TR::dcalli, TR::lcalli, TR::calli
TR::Register *OMR::Power::TreeEvaluator::indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());

   return linkage->buildIndirectDispatch(node);
   }

TR::Register *OMR::Power::TreeEvaluator::treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
   return tempReg;
   }


TR::Register *OMR::Power::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, TR::InstOpCode::fence, node, node);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *resultReg;
   TR::Symbol *sym = node->getSymbol();
   TR::MemoryReference *mref;
   TR::Compilation *comp = cg->comp();

   mref = TR::MemoryReference::createWithSymRef(cg, node, node->getSymbolReference(), 4);

   if (mref->getUnresolvedSnippet() != NULL)
      {
      resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
      if (mref->useIndexedForm())
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::add, node, resultReg, mref);
         }
      else
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, resultReg, mref);
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
         int64_t  offset = mref->getOffset(*comp);
         if (mref->hasDelayedOffset()
             || !mref->getBaseRegister()
             || mref->getLabel()
             || offset!=0
             || comp->compileRelocatableCode())
            {
            resultReg = sym->isLocalObject() ? cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
            generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, resultReg, mref);
           }
           else
              {
              resultReg = mref->getBaseRegister();
              if (resultReg == cg->getMethodMetaDataRegister())
                 {
                 resultReg = cg->allocateRegister();
                 generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultReg, mref->getBaseRegister());
                 }
              }
           }
       }
   node->setRegister(resultReg);
   mref->decNodeReferenceCounts(cg);
   return resultReg;
   }

TR::Register *OMR::Power::TreeEvaluator::gprRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (OMR_LIKELY(globalReg != NULL))
      return globalReg;

   if (OMR_LIKELY(node->getOpCodeValue() == TR::aRegLoad))
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
      }
   else
      {
#ifdef TR_TARGET_32BIT
      if (OMR_LIKELY(node->getOpCodeValue() != TR::lRegLoad))
         globalReg = cg->allocateRegister();
      else
         globalReg = cg->allocateRegisterPair(cg->allocateRegister(),
                                              cg->allocateRegister());
#else
      globalReg = cg->allocateRegister();
#endif
      }

   node->setRegister(globalReg);
   return globalReg;
   }

TR::Register *OMR::Power::TreeEvaluator::gprRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);

   if (node->getOpCodeValue() != TR::lRegLoad && node->needsSignExtension())
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsw, node, globalReg, globalReg);

   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *OMR::Power::TreeEvaluator::GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int i;

   for (i=0; i<node->getNumChildren(); i++)
      {
      cg->evaluate(node->getChild(i));
      cg->decReferenceCount(node->getChild(i));
      }
   return(NULL);
   }

TR::Register *OMR::Power::TreeEvaluator::passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::Compilation *comp = cg->comp();
   TR::Node * currentTop = cg->getCurrentEvaluationTreeTop()->getNode();
   bool skipCopy = (node->getOpCodeValue()==TR::a2i && node->getReferenceCount()==1 &&
                    currentTop->getOpCode().isIf() &&
                    (currentTop->getFirstChild()==node || currentTop->getSecondChild()==node));

   if ((child->getReferenceCount()>1 && node->getOpCodeValue()!=TR::PassThrough && !skipCopy && cg->useClobberEvaluate())
        || (node->getOpCodeValue() == TR::PassThrough
            && node->isCopyToNewVirtualRegister()
            && srcReg->getKind() == TR_GPR))
      {
      TR::Register *copyReg;
      TR_RegisterKinds kind = srcReg->getKind();
      TR_ASSERT(kind == TR_GPR || kind == TR_FPR, "passThrough does not work for this type of register\n");
      TR::InstOpCode::Mnemonic move_opcode = (srcReg->getKind() == TR_GPR) ? TR::InstOpCode::mr: TR::InstOpCode::fmr;

      if (srcReg->containsInternalPointer() || !srcReg->containsCollectedReference())
         {
         copyReg = cg->allocateRegister(kind);
         if (srcReg->containsInternalPointer())
            {
            copyReg->setPinningArrayPointer(srcReg->getPinningArrayPointer());
            copyReg->setContainsInternalPointer();
            }
         }
      else
         {
         copyReg = cg->allocateCollectedReferenceRegister();
         }

      if (srcReg->getRegisterPair())
         {
         TR::Register *copyRegLow = cg->allocateRegister(kind);
         generateTrg1Src1Instruction(cg, move_opcode , node, copyReg, srcReg->getHighOrder());
         generateTrg1Src1Instruction(cg, move_opcode , node, copyRegLow, srcReg->getLowOrder());
         copyReg = cg->allocateRegisterPair(copyRegLow, copyReg);
         }
      else
         {
         generateTrg1Src1Instruction(cg, move_opcode , node, copyReg, srcReg);
         }

      srcReg = copyReg;
      }

   node->setRegister(srcReg);
   cg->decReferenceCount(child);
   return srcReg;
   }

TR::Register *OMR::Power::TreeEvaluator::BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Block *block = node->getBlock();
   cg->setCurrentBlock(block);
   TR::Compilation *comp = cg->comp();

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
            TR::ParameterSymbol *sym = child->getChild(i)->getSymbol()->getParmSymbol();
            if (sym != NULL)
               {
               if (cg->comp()->target().is64Bit() || !sym->getType().isInt64())
                  sym->setAssignedGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getGlobalRegisterNumber()));
               else
                  {
                  sym->setAssignedHighGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getHighGlobalRegisterNumber()));
                  sym->setAssignedLowGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getLowGlobalRegisterNumber()));
                  }
               }
            }
         }
      cg->decReferenceCount(child);
      }

   TR::Instruction * fence = generateAdminInstruction(cg, TR::InstOpCode::fence, node,
                               TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._startPC));

   if (block->firstBlockInLoop() && !block->isCold())
      generateAlignmentNopInstruction(cg, node, cg->getHotLoopAlignment());

   TR::Instruction *labelInst = NULL;

   if (node->getLabel() != NULL)
      {
      if (deps == NULL)
         {
         node->getLabel()->setInstruction(labelInst = generateLabelInstruction(cg, TR::InstOpCode::label, node, node->getLabel()));
         }
      else
         {
         node->getLabel()->setInstruction(labelInst = generateDepLabelInstruction(cg, TR::InstOpCode::label, node, node->getLabel(), deps));
         }
      }
   else
      {
      TR::LabelSymbol *label = generateLabelSymbol(cg);
      node->setLabel(label);
      if (deps == NULL)
         node->getLabel()->setInstruction(labelInst = generateLabelInstruction(cg, TR::InstOpCode::label, node, label));
      else
         node->getLabel()->setInstruction(labelInst = generateDepLabelInstruction(cg, TR::InstOpCode::label, node, label, deps));
      }

   block->setFirstInstruction(labelInst);
   TR_PrefetchInfo *pf  = comp->findExtraPrefetchInfo(node, true); // Try to find delay prefetch
   if (pf)
      {
      TR::Register *srcReg = pf->_addrNode->getRegister();;
      int32_t offset = pf->_offset;

      if (srcReg && offset)
         {
         TR::Register *tempReg = cg->allocateRegister();
         if (cg->comp()->target().is64Bit() && !comp->useCompressedPointers())
            {
            TR::MemoryReference *tempMR = TR::MemoryReference::createWithDisplacement(cg, srcReg, offset, 8);
            generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, tempReg, tempMR);
            }
         else
            {
            TR::MemoryReference *tempMR = TR::MemoryReference::createWithDisplacement(cg, srcReg, offset, 4);
            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, tempReg, tempMR);
            }
         TR::MemoryReference *targetMR = TR::MemoryReference::createWithDisplacement(cg, tempReg, (int32_t)0, 4);
         targetMR->forceIndexedForm(node, cg);
         generateMemInstruction(cg, TR::InstOpCode::dcbt, node, targetMR);
         cg->stopUsingRegister(tempReg);
         }
      }

   if (OMR_UNLIKELY(block->isCatchBlock()))
      {
      cg->generateCatchBlockBBStartPrologue(node, fence);
      }

   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Block * block = node->getBlock();
   TR::Compilation *comp = cg->comp();

   if (NULL == block->getNextBlock())
      {
      TR::Instruction *lastInstruction = cg->getAppendInstruction();
      if (lastInstruction->getOpCodeValue() == TR::InstOpCode::bl
              && lastInstruction->getNode()->getSymbolReference()->getReferenceNumber() == TR_aThrow)
         {
         cg->generateNop(node, lastInstruction);
         }
      }

   TR::TreeTop *nextTT   = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();

   TR::Instruction *lastInst = generateAdminInstruction(cg, TR::InstOpCode::fence, node,
                                 TR::Node::createRelative32BitFenceNode(node, &node->getBlock()->getInstructionBoundaries()._endPC));
   if ((!nextTT || !nextTT->getNode()->getBlock()->isExtensionOfPreviousBlock()) && node->getNumChildren()>0)
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      lastInst = generateDepLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg),
            generateRegisterDependencyConditions(cg, child, 0));
      cg->decReferenceCount(child);
      }

   if ((!nextTT || !nextTT->getNode()->getBlock()->isExtensionOfPreviousBlock()))
      {
      // Generate a end of EBB ASSOCREG instruction to track the current associations
      // Also, clear the Real->Virtual map since we have ended this EBB
      int numAssoc=0;
      TR::RegisterDependencyConditions *assoc;
      TR::Machine *machine = cg->machine();
      assoc = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, TR::RealRegister::NumRegisters, cg->trMemory());
      for( TR::RealRegister::RegNum regNum=TR::RealRegister::FirstGPR ; regNum < TR::RealRegister::NumRegisters ; regNum=(TR::RealRegister::RegNum)(regNum+TR::RealRegister::FirstGPR) )
         {
         if( machine->getVirtualAssociatedWithReal(regNum) != 0 )
            {
            assoc->addPostCondition( machine->getVirtualAssociatedWithReal(regNum), regNum );
            machine->getVirtualAssociatedWithReal(regNum)->setAssociation(0);
            machine->setVirtualAssociatedWithReal( regNum, NULL );
            numAssoc++;
            }
         }
      // Emit an assocreg instruction to track the previous association
      if( numAssoc > 0 )
         {
         assoc->setNumPostConditions(numAssoc, cg->trMemory());
         lastInst = generateDepInstruction( cg, TR::InstOpCode::assocreg, node, assoc );
         }
      }

   block->setLastInstruction(lastInst);
   return NULL;
   }


TR::Register *OMR::Power::TreeEvaluator::NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }


TR::Register *OMR::Power::TreeEvaluator::unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if defined(DEBUG)
   printf("\nTreeEvaluator: bad il op or undefined/unimplemented for PPC \n");
   int *i = NULL;
   *i = 42; // cause a failure
#else
   TR_ASSERT(false, "Unimplemented evaluator for opcode %d\n", node->getOpCodeValue());
#endif
   return NULL;
   }

void OMR::Power::TreeEvaluator::postSyncConditions(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Register *dataReg,
      TR::MemoryReference *memRef,
      TR::InstOpCode::Mnemonic syncOp,
      bool lazyVolatile)
   {
   TR::Instruction *iPtr;
   TR::SymbolReference *symRef = memRef->getSymbolReference();
   TR::Register *baseReg = memRef->getBaseRegister();
   TR::Compilation *comp = cg->comp();

   // baseReg has to be the current memRef base, while index can be the previous one (
   // NULL) or the current one.

   if (symRef->isUnresolved())
      {
      if (cg->comp()->target().is64Bit() && symRef->getSymbol()->isStatic())
         {
         iPtr = cg->getAppendInstruction()->getPrev();
         if (syncOp == TR::InstOpCode::sync)  // This is a store
            iPtr = iPtr->getPrev();
         memRef = iPtr->getMemoryReference();
         }
      }

   if (!lazyVolatile)
      generateInstruction(cg, syncOp, node);

   if (symRef->isUnresolved() && memRef->getUnresolvedSnippet() != NULL)
      {
      TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
      TR::Register *tempReg = cg->allocateRegister();

      if (baseReg != NULL)
         {
         TR::addDependency(conditions, baseReg, TR::RealRegister::NoReg, TR_GPR, cg);
         conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
         conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
         }
      TR::addDependency(conditions, tempReg, TR::RealRegister::gr11, TR_GPR, cg);
      TR::addDependency(conditions, dataReg, TR::RealRegister::NoReg, dataReg->getKind(), cg);
      if (memRef->getIndexRegister() != NULL)
         TR::addDependency(conditions, memRef->getIndexRegister(), TR::RealRegister::NoReg, TR_GPR, cg);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg), conditions);
      cg->stopUsingRegister(tempReg);

#ifdef J9_PROJECT_SPECIFIC
      if (!lazyVolatile)
         memRef->getUnresolvedSnippet()->setInSyncSequence();
#endif
      }
   }

// handles icmpset lcmpset
TR::Register *OMR::Power::TreeEvaluator::cmpsetEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::lcmpset ||
          node->getOpCodeValue() == TR::icmpset, "only icmpset and lcmpset nodes are supported");
   int8_t size = node->getOpCodeValue() == TR::icmpset ? 4 : 8;

   if (size == 8)
      TR_ASSERT(cg->comp()->target().is64Bit(), "lcmpset is only supported on ppc64");

   TR::Node *pointer = node->getChild(0);
   TR::Node *cmpVal  = node->getChild(1);
   TR::Node *repVal  = node->getChild(2);

   TR::Register *ptrReg = cg->evaluate(pointer);
   TR::Register *cmpReg = cg->evaluate(cmpVal);
   TR::Register *repReg = cg->evaluate(repVal);

   TR::Register *result  = cg->allocateRegister();
   TR::Register *tmpReg  = cg->allocateRegister();
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::Register *cr0     = cg->allocateRegister(TR_CCR);

   TR::MemoryReference *ldMemRef = TR::MemoryReference::createWithIndexReg(cg, 0, ptrReg, size);
   TR::MemoryReference *stMemRef = TR::MemoryReference::createWithIndexReg(cg, 0, ptrReg, size);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel   = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();
   TR::RegisterDependencyConditions *deps
      = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7, cg->trMemory());
   deps->addPostCondition(result, TR::RealRegister::NoReg);
   deps->addPostCondition(repReg, TR::RealRegister::NoReg);
   deps->addPostCondition(tmpReg, TR::RealRegister::NoReg);
   deps->addPostCondition(cmpReg, TR::RealRegister::NoReg);
   deps->addPostCondition(ptrReg, TR::RealRegister::NoReg);
   deps->addPostCondition(condReg,TR::RealRegister::NoReg);
   deps->addPostCondition(cr0,    TR::RealRegister::cr0);

   TR::InstOpCode::Mnemonic cmpOp = size == 8 ? TR::InstOpCode::cmp8    : TR::InstOpCode::cmp4;
   TR::InstOpCode::Mnemonic ldrOp = size == 8 ? TR::InstOpCode::ldarx   : TR::InstOpCode::lwarx;
   TR::InstOpCode::Mnemonic stcOp = size == 8 ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r;

   // li     result, 1
   // lwarx  tmpReg, [ptrReg]    ; or ldarx
   // cmpw   tmpReg, cmpReg      ; or cmpd
   // bne-   Ldone
   // stwcx. repReg, [ptrReg]    ; or stdcx.
   // bne-   Ldone
   // li     result, 0
   // Ldone:
   //
   generateDepLabelInstruction            (cg, TR::InstOpCode::label,  node, startLabel, deps);
   generateTrg1ImmInstruction             (cg, TR::InstOpCode::li,     node, result, 1);
   generateTrg1MemInstruction             (cg, ldrOp,        node, tmpReg, ldMemRef);
   generateTrg1Src2Instruction            (cg, cmpOp,        node, condReg, tmpReg, cmpReg);
   generateConditionalBranchInstruction   (cg, TR::InstOpCode::bne,    PPCOpProp_BranchUnlikely, node, endLabel, condReg);
   generateMemSrc1Instruction             (cg, stcOp,        node, stMemRef, repReg);
   generateConditionalBranchInstruction   (cg, TR::InstOpCode::bne,    PPCOpProp_BranchUnlikely, node, endLabel, cr0);
   generateTrg1ImmInstruction             (cg, TR::InstOpCode::li,     node, result, 0);
   generateDepLabelInstruction            (cg, TR::InstOpCode::label,  node, endLabel, deps);


   cg->stopUsingRegister(cr0);
   cg->stopUsingRegister(tmpReg);
   cg->stopUsingRegister(condReg);

   node->setRegister(result);
   cg->decReferenceCount(pointer);
   cg->decReferenceCount(cmpVal);
   cg->decReferenceCount(repVal);

   return result;
   }

bool
TR_PPCComputeCC::setCarryBorrow(
      TR::Node *flagNode,
      bool invertValue,
      TR::Register **flagReg,
      TR::CodeGenerator *cg)
   {
   *flagReg = NULL;

   // do nothing, except evaluate child
   *flagReg = cg->evaluate(flagNode);
   cg->decReferenceCount(flagNode);
   return true;
   }



TR::Register *OMR::Power::TreeEvaluator::integerHighestOneBit(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   return inlineIntegerHighestOneBit(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg){ TR_UNIMPLEMENTED(); return NULL; }

TR::Register *OMR::Power::TreeEvaluator::integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineFixedTrg1Src1(node, TR::InstOpCode::cntlzw, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineNumberOfTrailingZeros(node, TR::InstOpCode::cntlzw, 32, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::integerBitCount(TR::Node *node, TR::CodeGenerator *cg)
{
   return inlineFixedTrg1Src1(node, TR::InstOpCode::popcntw, cg);
}

TR::Register *OMR::Power::TreeEvaluator::longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineLongHighestOneBit(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::longLowestOneBit(TR::Node *node, TR::CodeGenerator *cg){ TR_UNIMPLEMENTED(); return NULL; }

TR::Register *OMR::Power::TreeEvaluator::longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return inlineFixedTrg1Src1(node, TR::InstOpCode::cntlzd, cg);
   else
      return inlineLongNumberOfLeadingZeros(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
  {
   if (cg->comp()->target().is64Bit())
      return inlineNumberOfTrailingZeros(node, TR::InstOpCode::cntlzd, 64, cg);
   else
      return inlineLongNumberOfTrailingZeros(node, cg);
  }

TR::Register *OMR::Power::TreeEvaluator::longBitCount(TR::Node *node, TR::CodeGenerator *cg)
{
   if (cg->comp()->target().is64Bit())
	  return inlineFixedTrg1Src1(node, TR::InstOpCode::popcntd, cg);
   else
	  return inlineLongBitCount(node, cg);
}

void OMR::Power::TreeEvaluator::preserveTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
{
   TR::Instruction *cursor = cg->getAppendInstruction();
   TR::Compilation* comp = cg->comp();

   //We need to preserve the JIT TOC whenever we call out. We're saving this on the caller TOC slot as defined by the ABI.
   TR::Register * grTOCReg       = cg->machine()->getRealRegister(TR::RealRegister::gr2);
   TR::Register * grSysStackReg   = cg->machine()->getRealRegister(TR::RealRegister::gr1);

   int32_t callerSaveTOCOffset = (cg->comp()->target().cpu.isBigEndian() ? 5 : 3) *  TR::Compiler->om.sizeofReferenceAddress();

   cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, TR::MemoryReference::createWithDisplacement(cg, grSysStackReg, callerSaveTOCOffset, TR::Compiler->om.sizeofReferenceAddress()), grTOCReg, cursor);

   cg->setAppendInstruction(cursor);
}

void OMR::Power::TreeEvaluator::restoreTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
{
   //Here we restore the JIT TOC after returning from a call out. We're restoring from the caller TOC slot as defined by the ABI.
   TR::Register * grTOCReg       = cg->machine()->getRealRegister(TR::RealRegister::gr2);
   TR::Register * grSysStackReg   = cg->machine()->getRealRegister(TR::RealRegister::gr1);
   TR::Compilation* comp = cg->comp();

   int32_t callerSaveTOCOffset = (cg->comp()->target().cpu.isBigEndian() ? 5 : 3) *  TR::Compiler->om.sizeofReferenceAddress();

   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, grTOCReg, TR::MemoryReference::createWithDisplacement(cg, grSysStackReg, callerSaveTOCOffset, TR::Compiler->om.sizeofReferenceAddress()));
}

TR::Register *OMR::Power::TreeEvaluator::retrieveTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
{
   return cg->machine()->getRealRegister(TR::RealRegister::gr2);
}

TR::Register *OMR::Power::TreeEvaluator::sbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *tgtRegister = cg->allocateRegister();

   TR::Node *firstNonConversionOpCodeNode = node->getFirstChild();
   TR::DataType nodeType = firstNonConversionOpCodeNode->getType();

   // TODO(#5684): Re-enable once issues with delayed indexed-form are corrected
   static bool reverseLoadEnabled = feGetEnv("TR_EnableReverseLoadStore");

   //Move through descendants until a non conversion opcode is reached,
   //while making sure all nodes have a ref count of 1 and the types are between 2-8 bytes
   while (firstNonConversionOpCodeNode->getOpCode().isConversion() &&
          firstNonConversionOpCodeNode->getReferenceCount() == 1 &&
          (nodeType.isInt16() || nodeType.isInt32() || nodeType.isInt64()))
      {
      firstNonConversionOpCodeNode = firstNonConversionOpCodeNode->getFirstChild();
      nodeType = firstNonConversionOpCodeNode->getType();
      }

   if (reverseLoadEnabled && !firstNonConversionOpCodeNode->getRegister() &&
       firstNonConversionOpCodeNode->getOpCode().isMemoryReference() &&
       firstNonConversionOpCodeNode->getReferenceCount() == 1 &&
       (nodeType.isInt16() || nodeType.isInt32() || nodeType.isInt64()))
      {
      int64_t extraOffset = 0;

#ifndef __LITTLE_ENDIAN__
      if (nodeType.isInt32())
         extraOffset = 2;
      else if (nodeType.isInt64())
         extraOffset = 6;
#endif

      TR::LoadStoreHandler::generateLoadNodeSequence(cg, tgtRegister, firstNonConversionOpCodeNode, TR::InstOpCode::lhbrx, 2, true, extraOffset);

      //Decrement Ref count for the intermediate conversion nodes
      firstNonConversionOpCodeNode = node->getFirstChild();
      while (firstNonConversionOpCodeNode->getOpCode().isConversion())
         {
         cg->decReferenceCount(firstNonConversionOpCodeNode);
         firstNonConversionOpCodeNode = firstNonConversionOpCodeNode->getFirstChild();
         }
      }
   else
      {
      TR::Register *srcRegister = cg->evaluate(firstChild);

      if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::brh, node, tgtRegister, srcRegister);
         }
      else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8))
         {
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister, srcRegister, 24, CONSTANT64(0x00000000ff));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister, srcRegister, 8,  CONSTANT64(0x000000ff00));
         }
      else
         {
         TR::Register *tmpRegister = cg->allocateRegister();

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcRegister, 24, CONSTANT64(0x00000000ff));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmpRegister, srcRegister, 8,  CONSTANT64(0x000000ff00));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmpRegister);

         cg->stopUsingRegister(tmpRegister);
         }
      cg->decReferenceCount(firstChild);
      }

   generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, tgtRegister, tgtRegister);

   node->setRegister(tgtRegister);
   return tgtRegister;
   }

TR::Register * OMR::Power::TreeEvaluator::ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *tgtRegister = cg->allocateRegister();

   // TODO(#5684): Re-enable once issues with delayed indexed-form are corrected
   static bool reverseLoadEnabled = feGetEnv("TR_EnableReverseLoadStore");

   if (reverseLoadEnabled && !firstChild->getRegister() &&
       firstChild->getOpCode().isMemoryReference() &&
       firstChild->getReferenceCount() == 1)
      {
      TR::LoadStoreHandler::generateLoadNodeSequence(cg, tgtRegister, firstChild, TR::InstOpCode::lwbrx, 4, true);
      }
   else
      {
      TR::Register *srcRegister = cg->evaluate(firstChild);

      if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::brw, node, tgtRegister, srcRegister);
         }
      else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8))
         {
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcRegister, 24, CONSTANT64(0x00ffffff00));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister, srcRegister, 8,  CONSTANT64(0x0000ff0000));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister, srcRegister, 8,  CONSTANT64(0x00000000ff));
         }
      else
         {
         TR::Register *tmp1Register = cg->allocateRegister();

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcRegister, 8,  CONSTANT64(0x00000000ff));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister, 8, CONSTANT64(0x0000ff0000));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister, 24, CONSTANT64(0x000000ff00));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister, 24, CONSTANT64(0x00ff000000));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);

         cg->stopUsingRegister(tmp1Register);
         }
      cg->decReferenceCount(firstChild);
      }

   node->setRegister(tgtRegister);
   return tgtRegister;
}

TR::Register *OMR::Power::TreeEvaluator::lbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->target().is64Bit())
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Register *tgtRegister = cg->allocateRegister();

      // TODO(#5684): Re-enable once issues with delayed indexed-form are corrected
      static bool reverseLoadEnabled = feGetEnv("TR_EnableReverseLoadStore");

      if (reverseLoadEnabled && comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P7) &&
          !firstChild->getRegister() &&
          firstChild->getOpCode().isMemoryReference() &&
          firstChild->getReferenceCount() == 1)
         {
         TR::LoadStoreHandler::generateLoadNodeSequence(cg, tgtRegister, firstChild, TR::InstOpCode::ldbrx, 8, true);
         }
      else
         {
         TR::Register *srcLRegister = cg->evaluate(firstChild);

         if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
            {
            generateTrg1Src1Instruction(cg, TR::InstOpCode::brd, node, tgtRegister, srcLRegister);
            }
         else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8))
            {
            TR::Register *srcHRegister = cg->allocateRegister();
            TR::Register *tmpRegister = cg->allocateRegister();

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, srcHRegister, srcLRegister, 32, CONSTANT64(0x00ffffffff));

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmpRegister, srcLRegister, 24, CONSTANT64(0x00ffffff00));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcHRegister, 24, CONSTANT64(0x00ffffff00));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmpRegister, srcLRegister, 8,  CONSTANT64(0x0000ff0000));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister, srcHRegister, 8,  CONSTANT64(0x0000ff0000));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tmpRegister, srcLRegister, 8,  CONSTANT64(0x00000000ff));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister, srcHRegister, 8,  CONSTANT64(0x00000000ff));

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, tgtRegister, tmpRegister, 32, CONSTANT64(0xffffffff00000000));

            cg->stopUsingRegister(srcHRegister);
            cg->stopUsingRegister(tmpRegister);
            }
         else
            {
            TR::Register *srcHRegister = cg->allocateRegister();
            TR::Register *tgtHRegister = cg->allocateRegister();
            TR::Register *tmp1Register = cg->allocateRegister();
            TR::Register *tmp2Register = cg->allocateRegister();

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, srcHRegister, srcLRegister, 32, CONSTANT64(0x00ffffffff));

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcHRegister, 8,  CONSTANT64(0x00000000ff));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtHRegister, srcLRegister, 8, CONSTANT64(0x00000000ff));

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcHRegister, 8, CONSTANT64(0x0000ff0000));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcLRegister, 8, CONSTANT64(0x0000ff0000));
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtHRegister, tgtHRegister, tmp2Register);

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcHRegister, 24, CONSTANT64(0x000000ff00));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcLRegister, 24, CONSTANT64(0x000000ff00));
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtHRegister, tgtHRegister, tmp2Register);

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcHRegister, 24, CONSTANT64(0x00ff000000));
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcLRegister, 24, CONSTANT64(0x00ff000000));
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtHRegister, tgtHRegister, tmp2Register);

            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, tgtRegister, tgtHRegister, 32, CONSTANT64(0xffffffff00000000));

            cg->stopUsingRegister(tmp2Register);
            cg->stopUsingRegister(tmp1Register);
            cg->stopUsingRegister(tgtHRegister);
            cg->stopUsingRegister(srcHRegister);
            }
         cg->decReferenceCount(firstChild);
         }

      node->setRegister(tgtRegister);
      return tgtRegister;
      }
   else //32-Bit Target
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::RegisterPair *tgtRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      TR::Register *srcRegister = cg->evaluate(firstChild);

      if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::brw, node, tgtRegister->getLowOrder(), srcRegister->getHighOrder());
         generateTrg1Src1Instruction(cg, TR::InstOpCode::brw, node, tgtRegister->getHighOrder(), srcRegister->getLowOrder());
         }
      else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8))
         {
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister->getLowOrder(), srcRegister->getHighOrder(), 24, CONSTANT64(0x00ffffff00));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister->getHighOrder(), srcRegister->getLowOrder(), 24, CONSTANT64(0x00ffffff00));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister->getLowOrder(), srcRegister->getHighOrder(), 8,  CONSTANT64(0x0000ff0000));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister->getHighOrder(), srcRegister->getLowOrder(), 8,  CONSTANT64(0x0000ff0000));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister->getLowOrder(), srcRegister->getHighOrder(), 8,  CONSTANT64(0x00000000ff));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, tgtRegister->getHighOrder(), srcRegister->getLowOrder(), 8,  CONSTANT64(0x00000000ff));
         }
      else
         {
         TR::Register *tmp1Register = cg->allocateRegister();
         TR::Register *tmp2Register = cg->allocateRegister();

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister->getLowOrder(), srcRegister->getHighOrder(), 8, CONSTANT64(0x00000000ff));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister->getHighOrder(), srcRegister->getLowOrder(), 8, CONSTANT64(0x00000000ff));

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister->getHighOrder(), 8, CONSTANT64(0x0000ff0000));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcRegister->getLowOrder(), 8,  CONSTANT64(0x0000ff0000));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getLowOrder(), tgtRegister->getLowOrder(), tmp1Register);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getHighOrder(), tgtRegister->getHighOrder(), tmp2Register);

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister->getHighOrder(), 24, CONSTANT64(0x000000ff00));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcRegister->getLowOrder(), 24,  CONSTANT64(0x000000ff00));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getLowOrder(), tgtRegister->getLowOrder(), tmp1Register);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getHighOrder(), tgtRegister->getHighOrder(), tmp2Register);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister->getHighOrder(), 24, CONSTANT64(0x00ff000000));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcRegister->getLowOrder(), 24,  CONSTANT64(0x00ff000000));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getLowOrder(), tgtRegister->getLowOrder(), tmp1Register);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getHighOrder(), tgtRegister->getHighOrder(), tmp2Register);

         cg->stopUsingRegister(tmp2Register);
         cg->stopUsingRegister(tmp1Register);
         }
      cg->decReferenceCount(firstChild);

      node->setRegister(tgtRegister);
      return tgtRegister;
      }
   }
