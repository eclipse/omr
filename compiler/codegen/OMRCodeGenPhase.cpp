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

#if defined(J9ZOS390)
//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRCGPhase#C")
#pragma csect(STATIC,"OMRCGPhase#S")
#pragma csect(TEST,"OMRCGPhase#T")
#endif

#include "codegen/CodeGenPhase.hpp"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Peephole.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/OSRData.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/FrontEnd.hpp"
#include "env/IO.hpp"
#include "env/PersistentInfo.hpp"
#include "env/RegionProfiler.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/ILWalk.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/DebuggingCounters.hpp"
#include "optimizer/LoadExtensions.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"

#include <map>
#include <utility>

class TR_BackingStore;
namespace TR { class RegisterCandidate; }
class TR_Structure;

/*
 * We must initialize this static array first before the getListSize() definition
 */
const OMR::CodeGenPhase::PhaseValue OMR::CodeGenPhase::PhaseList[] =
   {
   // Different products and arch will provide this file and
   // they will be included correctly by the include paths
   #include "codegen/CodeGenPhaseToPerform.hpp"
   };

TR::CodeGenPhase *
OMR::CodeGenPhase::self()
   {
   return static_cast<TR::CodeGenPhase*>(this);
   }

/*
 * This getListSize definition must be placed after the static array PhaseList has been initialized
 */
int
OMR::CodeGenPhase::getListSize()
   {
   return sizeof(OMR::CodeGenPhase::PhaseList)/sizeof(OMR::CodeGenPhase::PhaseList[0]);
   }

/*
 * This function pointer table will handle the dispatch
 * of phases to the correct static method.
 */
CodeGenPhaseFunctionPointer
OMR::CodeGenPhase::_phaseToFunctionTable[] =
   {
   /*
    * The entries in this include file for product must be kept
    * in sync with "CodeGenPhaseEnum.hpp" file in that product layer.
    */
   #include "codegen/CodeGenPhaseFunctionTable.hpp"
   };


void
OMR::CodeGenPhase::performAll()
   {
   for(int32_t i = 0; i < TR::CodeGenPhase::getListSize(); i++)
      {
      PhaseValue phaseToDo = PhaseList[i];

      TR::StackMemoryRegion stackMemoryRegion(*_cg->trMemory());
      TR::RegionProfiler rp(_cg->comp()->trMemory()->heapMemoryRegion(), *_cg->comp(), "codegen/%s/%s",
         _cg->comp()->getHotnessName(_cg->comp()->getMethodHotness()), self()->getName(phaseToDo));

      _phaseToFunctionTable[phaseToDo](_cg, self());
      }
   }

void
OMR::CodeGenPhase::reportPhase(PhaseValue phase)
   {
   _currentPhase = phase;
   }

int
OMR::CodeGenPhase::getNumPhases()
   {
   return static_cast<int>(TR::CodeGenPhase::LastOMRPhase);
   }


void
OMR::CodeGenPhase::performProcessRelocationsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();

   if (comp->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      {
      // This must be called before relocations to generate the relocation data for the profiled instructions.
      cg->createHWPRecords();
      }

   phase->reportPhase(ProcessRelocationsPhase);

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->processRelocations();

   cg->trimCodeMemoryToActualSize();
   cg->registerAssumptions();

   cg->syncCode(cg->getBinaryBufferStart(), static_cast<uint32_t>(cg->getBinaryBufferCursor() - cg->getBinaryBufferStart()));

   if (comp->getOption(TR_EnableOSR))
     {
     if (comp->getOption(TR_TraceOSR) && !comp->getOption(TR_DisableOSRSharedSlots))
        {
        (*comp) << "OSRCompilationData is " << *comp->getOSRCompilationData() << "\n";
        }
     }

   if (cg->getAheadOfTimeCompile() && (comp->getOption(TR_TraceRelocatableDataCG) || comp->getOption(TR_TraceRelocatableDataDetailsCG)))
      {
      traceMsg(comp, "\n<relocatableDataCG>\n");
      if (comp->getOption(TR_TraceRelocatableDataDetailsCG)) // verbose output
         {
         uint8_t * relocatableMethodCodeStart = (uint8_t *)comp->getRelocatableMethodCodeStart();
         traceMsg(comp, "Code start = %8x, Method start pc = %x, Method start pc offset = 0x%x\n", relocatableMethodCodeStart, cg->getCodeStart(), cg->getCodeStart() - relocatableMethodCodeStart);
         }
      cg->getAheadOfTimeCompile()->dumpRelocationData();
      traceMsg(comp, "</relocatableDataCG>\n");
      }

   if (debug("dumpCodeSizes"))
      {
      diagnostic("%08d   %s\n", cg->getCodeLength(), comp->signature());
      }

   TR_ASSERT(cg->getCodeLength() <= cg->getEstimatedCodeLength(),
      "Method length estimate must be conservatively large\n"
      "    codeLength = %d, estimatedCodeLength = %d \n",
      cg->getCodeLength(), cg->getEstimatedCodeLength()
   );

   // also trace the interal stack atlas
   cg->getStackAtlas()->close(cg);

   TR::SimpleRegex * regex = comp->getOptions()->getSlipTrap();
   if (regex && TR::SimpleRegex::match(regex, comp->getCurrentMethod()))
      {
      if (cg->comp()->target().is64Bit())
         {
         setDllSlip((const char *)cg->getCodeStart(), (const char *)cg->getCodeStart() + cg->getCodeLength(), "SLIPDLL64", comp);
         }
      else
         {
         setDllSlip((const char *)cg->getCodeStart(), (const char *)cg->getCodeStart() + cg->getCodeLength(), "SLIPDLL31", comp);
         }
      }

   if (comp->getOption(TR_TraceCG))
      {
      const char * title = "Post Relocation Instructions";
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), title, false, true);

      traceMsg(comp,"<snippets>");
      comp->getDebug()->print(comp->getOutFile(), cg->getSnippetList());
      traceMsg(comp,"\n</snippets>\n");

      auto iterator = cg->getSnippetList().begin();
      int32_t estimatedSnippetStart = cg->getEstimatedSnippetStart();
      while (iterator != cg->getSnippetList().end())
         {
         estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
         ++iterator;
         }
      }
   }


void
OMR::CodeGenPhase::performEmitSnippetsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(EmitSnippetsPhase);

   TR::LexicalMemProfiler mp("Emit Snippets", comp->phaseMemProfiler());
   LexicalTimer pt("Emit Snippets", comp->phaseTimer());

   if (cg->getLastWarmInstruction() &&
       comp->getOption(TR_MoveSnippetsToWarmCode))
      {
      // Snippets will follow warm blocks
      uint8_t * oldCursor = cg->getBinaryBufferCursor();
      cg->setBinaryBufferCursor(cg->getWarmCodeEnd());
      cg->emitSnippets();
      cg->setWarmCodeEnd(cg->getBinaryBufferCursor());
      cg->setBinaryBufferCursor(oldCursor);
      }
   else
      {
      cg->emitSnippets();
      }

   if (comp->getOption(TR_EnableOSR))
      {
      comp->getOSRCompilationData()->checkOSRLimits();
      comp->getOSRCompilationData()->compressInstruction2SharedSlotMap();
      }

   if (comp->getOption(TR_TraceCG))
      {
      diagnostic("\nbuffer start = %8x, code start = %8x, buffer length = %d", cg->getBinaryBufferStart(), cg->getCodeStart(), cg->getEstimatedCodeLength());
      diagnostic("\n");
      const char * title = "Post Binary Instructions";

      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), title, false, true);

      traceMsg(comp,"<snippets>");
      comp->getDebug()->print(comp->getOutFile(), cg->getSnippetList());
      traceMsg(comp,"\n</snippets>\n");

      auto iterator = cg->getSnippetList().begin();
      int32_t estimatedSnippetStart = cg->getEstimatedSnippetStart();
      while (iterator != cg->getSnippetList().end())
         {
         estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
         ++iterator;
         }
      int32_t snippetLength = estimatedSnippetStart - cg->getEstimatedSnippetStart();

      diagnostic("\nAmount of code memory allocated for this function        = %d"
                  "\nAmount of code memory consumed for this function         = %d"
                  "\nAmount of snippet code memory consumed for this function = %d\n\n",
                  cg->getEstimatedCodeLength(),
                  cg->getCodeLength(),
                  snippetLength);
      }
   }


void
OMR::CodeGenPhase::performBinaryEncodingPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(BinaryEncodingPhase);

   if (cg->getDebug())
      cg->getDebug()->roundAddressEnumerationCounters();

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->doBinaryEncoding();

   // Instructions have been emitted, and now we know what the entry point is, so update the compilation method symbol
   comp->getMethodSymbol()->setMethodAddress(cg->getCodeStart());

   if (debug("verifyFinalNodeReferenceCounts"))
      {
      if (cg->getDebug())
         cg->getDebug()->verifyFinalNodeReferenceCounts(comp->getMethodSymbol());
      }
   }




void
OMR::CodeGenPhase::performPeepholePhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation* comp = cg->comp();

   if (!comp->getOption(TR_DisablePeephole))
      {
      phase->reportPhase(PeepholePhase);

      TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
      LexicalTimer pt(phase->getName(), comp->phaseTimer());

      TR::Peephole peephole(comp);
      bool performed = peephole.perform();

      if (performed && comp->getOption(TR_TraceCG))
         comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Peephole Instructions", false);
      }
   }





void
OMR::CodeGenPhase::performMapStackPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation* comp = cg->comp();
   cg->remapGCIndicesInInternalPtrFormat();
     {
     TR::LexicalMemProfiler mp("Stackmap", comp->phaseMemProfiler());
     LexicalTimer pt("Stackmap", comp->phaseTimer());

     cg->getLinkage()->mapStack(comp->getJittedMethodSymbol());

     if (comp->getOption(TR_TraceCG))
        comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Stack Map", false);
     }
   cg->setMappingAutomatics();

   }


void
OMR::CodeGenPhase::performRegisterAssigningPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation* comp = cg->comp();
   phase->reportPhase(RegisterAssigningPhase);

   if (cg->getDebug())
      cg->getDebug()->roundAddressEnumerationCounters();

      {
      TR::LexicalMemProfiler mp("RA", comp->phaseMemProfiler());
      LexicalTimer pt("RA", comp->phaseTimer());

      TR_RegisterKinds kindsToAssign = cg->prepareRegistersForAssignment();

      cg->jettisonAllSpills(); // Spill temps used before now may lead to conflicts if also used by register assignment
      cg->doRegisterAssignment(kindsToAssign);

      if (comp->compilationShouldBeInterrupted(AFTER_REGISTER_ASSIGNMENT_CONTEXT))
         {
         comp->failCompilation<TR::CompilationInterrupted>("interrupted after RA");
         }
      }

   if (comp->getOption(TR_TraceCG))
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Register Assignment Instructions", false, true);
   }





void
OMR::CodeGenPhase::performCreateStackAtlasPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   cg->createStackAtlas();
   }


void
OMR::CodeGenPhase::performInstructionSelectionPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation* comp = cg->comp();
   phase->reportPhase(InstructionSelectionPhase);

   if (comp->getOption(TR_TraceCG))
      comp->dumpMethodTrees("Pre Instruction Selection Trees");

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->doInstructionSelection();

   if (comp->getOption(TR_TraceCG))
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Instruction Selection Instructions", false, true);

   // check reference counts
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      for (int r=0; r<NumRegisterKinds; r++)
         {
         if (TO_KIND_MASK(r) & cg->getSupportedLiveRegisterKinds())
            {
            cg->checkForLiveRegisters(cg->getLiveRegisters((TR_RegisterKinds)r));
            }
         }
#endif

   // check interrupt
   if (comp->compilationShouldBeInterrupted(AFTER_INSTRUCTION_SELECTION_CONTEXT))
      {
      comp->failCompilation<TR::CompilationInterrupted>("interrupted after instruction selection");
      }
   }



void
OMR::CodeGenPhase::performSetupForInstructionSelectionPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation *comp = cg->comp();
   if (cg->shouldBuildStructure() &&
       (comp->getFlowGraph()->getStructure() != NULL))
      {
      TR_Structure *rootStructure = TR_RegionAnalysis::getRegions(comp);
      comp->getFlowGraph()->setStructure(rootStructure);
      }

   phase->reportPhase(SetupForInstructionSelectionPhase);

   // Dump preIR
   if (comp->getOption(TR_TraceRegisterPressureDetails) && !comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      traceMsg(comp, "         { Post optimization register pressure simulation\n");
      TR_BitVector emptyBitVector;
      vcount_t vc = comp->incVisitCount();
      cg->initializeRegisterPressureSimulator();
      for (TR::Block *block = comp->getStartBlock(); block; block = block->getNextExtendedBlock())
         {
         TR_LinkHead<TR::RegisterCandidate> emptyCandidateList;
         TR::CodeGenerator::TR_RegisterPressureState state(NULL, 0, emptyBitVector, emptyBitVector, &emptyCandidateList, cg->getNumberOfGlobalGPRs(), cg->getNumberOfGlobalFPRs(), cg->getNumberOfGlobalVRFs(), vc);
         TR::CodeGenerator::TR_RegisterPressureSummary summary(state._gprPressure, state._fprPressure, state._vrfPressure);
         cg->simulateBlockEvaluation(block, &state, &summary);
         }
      traceMsg(comp, "         }\n");
      }

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->setUpForInstructionSelection();
   }




void
OMR::CodeGenPhase::performLowerTreesPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(LowerTreesPhase);

   cg->lowerTrees();

   if (comp->getOption(TR_TraceCG))
      comp->dumpMethodTrees("Post Lower Trees");
   }


void
OMR::CodeGenPhase::performUncommonCallConstNodesPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation* comp = cg->comp();

   if(comp->getOption(TR_DisableCallConstUncommoning))
      {
      traceMsg(comp, "Skipping Uncommon Call Constant Node phase\n");
      return;
      }

   phase->reportPhase(UncommonCallConstNodesPhase);

   if (comp->getOption(TR_TraceCG))
      comp->dumpMethodTrees("Pre Uncommon Call Constant Node Trees");

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->uncommonCallConstNodes();

   if (comp->getOption(TR_TraceCG))
      comp->dumpMethodTrees("Post Uncommon Call Constant Node Trees");
  }

void
OMR::CodeGenPhase::performReserveCodeCachePhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   cg->reserveCodeCache();
   }

void
OMR::CodeGenPhase::performInliningReportPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   if (comp->getOptions()->insertDebuggingCounters()>1)
   TR_DebuggingCounters::inliningReportForMethod(comp);
   }

void
OMR::CodeGenPhase::performCleanUpFlagsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::TreeTop * tt;
   vcount_t visitCount = cg->comp()->incVisitCount();

   for (tt = cg->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      cg->cleanupFlags(tt->getNode());
      }
   }

void
OMR::CodeGenPhase::performFindAndFixCommonedReferencesPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   if (!cg->comp()->useRegisterMaps())
      cg->findAndFixCommonedReferences();
   }

void
OMR::CodeGenPhase::performRemoveUnusedLocalsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation *comp = cg->comp();
   phase->reportPhase(RemoveUnusedLocalsPhase);
   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());
   cg->removeUnusedLocals();
   }

void
OMR::CodeGenPhase::performInsertDebugCountersPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   cg->insertDebugCounters();
   }

void
OMR::CodeGenPhase::performExpandInstructionsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(ExpandInstructionsPhase);

   cg->expandInstructions();

   if (comp->getOption(TR_TraceCG))
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Instruction Expansion Instructions", false, true);
   }

const char *
OMR::CodeGenPhase::getName()
   {
   return TR::CodeGenPhase::getName(_currentPhase);
   }


const char *
OMR::CodeGenPhase::getName(PhaseValue phase)
   {
   switch (phase)
      {
      case UncommonCallConstNodesPhase:
         return "UncommonCallConstNodesPhase";
      case ReserveCodeCachePhase:
         return "ReserveCodeCache";
      case LowerTreesPhase:
         return "LowerTrees";
      case SetupForInstructionSelectionPhase:
         return "SetupForInstructionSelection";
      case InstructionSelectionPhase:
         return "InstructionSelection";
      case CreateStackAtlasPhase:
         return "CreateStackAtlas";
      case RegisterAssigningPhase:
         return "RegisterAssigning";
      case MapStackPhase:
         return "MapStack";
      case PeepholePhase:
         return "Peephole";
      case BinaryEncodingPhase:
         return "BinaryEncoding";
      case EmitSnippetsPhase:
         return "EmitSnippets";
      case ProcessRelocationsPhase:
         return "ProcessRelocations";
      case FindAndFixCommonedReferencesPhase:
	 return "FindAndFixCommonedReferencesPhase";
      case RemoveUnusedLocalsPhase:
	 return "RemoveUnusedLocalsPhase";
      case InliningReportPhase:
	 return "InliningReportPhase";
      case InsertDebugCountersPhase:
	 return "InsertDebugCountersPhase";
      case CleanUpFlagsPhase:
	 return "CleanUpFlagsPhase";
      case ExpandInstructionsPhase:
         return "ExpandInstructionsPhase";
      default:
         TR_ASSERT(false, "TR::CodeGenPhase %d doesn't have a corresponding name.", phase);
         return NULL;
      };
   }

LexicalXmlTag::LexicalXmlTag(TR::CodeGenerator * cg): cg(cg)
   {
   TR::Compilation *comp = cg->comp();
   if (comp->getOption(TR_TraceOptDetails) || comp->getOption(TR_TraceCG))
      {
      const char *hotnessString = comp->getHotnessName(comp->getMethodHotness());
      traceMsg(comp, "<codegen\n"
              "\tmethod=\"%s\"\n"
               "\thotness=\"%s\">\n",
               comp->signature(), hotnessString);
      }
   }

LexicalXmlTag::~LexicalXmlTag()
   {
   TR::Compilation *comp = cg->comp();
   if (comp->getOption(TR_TraceOptDetails) || comp->getOption(TR_TraceCG))
      traceMsg(comp, "</codegen>\n");
   }
