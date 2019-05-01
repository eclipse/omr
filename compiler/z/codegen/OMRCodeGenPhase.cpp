/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRZCGPhase#C")
#pragma csect(STATIC,"OMRZCGPhase#S")
#pragma csect(TEST,"OMRZCGPhase#T")


#include "codegen/CodeGenPhase.hpp"

#include "infra/Assert.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/LoadExtensions.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "il/TreeTop.hpp"

void
OMR::Z::CodeGenPhase::performMarkLoadAsZeroOrSignExtensionPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   if (TR::Compiler->target.cpu.isZ() && cg->getOptimizationPhaseIsComplete())
      {
      TR::Compilation* comp = cg->comp();
      TR::OptimizationManager *manager = comp->getOptimizer()->getOptimization(OMR::loadExtensions);
      TR_ASSERT(manager, "Load extensions optimization should be initialized.");
      TR_LoadExtensions *loadExtensions = (TR_LoadExtensions *) manager->factory()(manager);
      loadExtensions->perform();
      delete loadExtensions;
      }
   }

void
OMR::Z::CodeGenPhase::performSetBranchOnCountFlagPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *)
   {
   if (!cg->comp()->getOption(TR_DisableBranchOnCount))
      {
      TR::TreeTop * tt;
      vcount_t visitCount = cg->comp()->incVisitCount();

      for (tt = cg->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         cg->setBranchOnCountFlag(tt->getNode(), visitCount);
         }
      }
   }

void
OMR::Z::CodeGenPhase::performPreRAPeepholePhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
    {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(PreRAPeepholePhase);

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->doPreRAPeephole();

   if (comp->getOption(TR_TraceCG))
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Pre Register Assignment Peephole Instructions", false);
    }

void
OMR::Z::CodeGenPhase::performPeepholePhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(PeepholePhase);

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->doPostRAPeephole();

   if (comp->getOption(TR_TraceCG))
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Register Assignment Peephole Instructions", false);
   }

int
OMR::Z::CodeGenPhase::getNumPhases()
   {
   return static_cast<int>(TR::CodeGenPhase::LastOMRZPhase);
   }

const char *
OMR::Z::CodeGenPhase::getName()
   {
   return TR::CodeGenPhase::getName(_currentPhase);
   }

const char *
OMR::Z::CodeGenPhase::getName(PhaseValue phase)
   {
   switch (phase)
      {
      case markLoadAsZeroOrSignExtension:
         return "markLoadAsZeroOrSignExtension";
      case SetBranchOnCountFlagPhase:
         return "SetBranchOnCountFlagPhase";
      case PreRAPeepholePhase:
         return "PreRegisterAllocationPeepholePhase";
      case PeepholePhase:
         return "PostRegisterAllocationPeepholePhase";
      default:
         // call parent class for common phases
         return OMR::CodeGenPhase::getName(phase);
      }
   }
