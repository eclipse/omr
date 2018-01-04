/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRCGPhase#C")
#pragma csect(STATIC,"OMRCGPhase#S")
#pragma csect(TEST,"OMRCGPhase#T")


#include "codegen/OMRCodeGenPhase.hpp"

#include <stdarg.h>                                   // for va_list, etc
#include <stddef.h>                                   // for NULL
#include <stdint.h>                                   // for int32_t, etc
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenPhase.hpp"                   // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"                  // for CodeGenerator, etc
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/FrontEnd.hpp"                       // for TR_FrontEnd, etc
#include "codegen/GCStackAtlas.hpp"                   // for GCStackAtlas
#include "codegen/Linkage.hpp"                        // for Linkage
#include "codegen/RegisterConstants.hpp"
#include "codegen/Snippet.hpp"                        // for Snippet
#include "compile/Compilation.hpp"                    // for Compilation, etc
#include "compile/OSRData.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                                 // for IO
#include "env/PersistentInfo.hpp"                     // for PersistentInfo
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                               // for Block
#include "il/Node.hpp"                                // for vcount_t
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "infra/BitVector.hpp"                        // for TR_BitVector
#include "infra/ILWalk.hpp"
#include "infra/Cfg.hpp"                              // for CFG
#include "infra/Link.hpp"                             // for TR_LinkHead
#include "infra/List.hpp"                             // for ListIterator, etc
#include "infra/SimpleRegex.hpp"
#include "optimizer/DebuggingCounters.hpp"
#include "optimizer/LoadExtensions.hpp"
#include "optimizer/Optimization.hpp"                 // for Optimization
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                    // for Optimizer
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "ras/Debug.hpp"                              // for TR_DebugBase, etc
#include "runtime/Runtime.hpp"                        // for setDllSlip
#include "env/RegionProfiler.hpp"

#include <map>
#include <utility>

class TR_BackingStore;
class TR_RegisterCandidate;
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
   int i = 0;

   for(; i < TR::CodeGenPhase::getListSize(); i++)
      {
      PhaseValue phaseToDo = PhaseList[i];
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

   cg->resizeCodeMemory();
   cg->registerAssumptions();

   cg->syncCode(cg->getBinaryBufferStart(), cg->getBinaryBufferCursor() - cg->getBinaryBufferStart());

   if (comp->getOption(TR_EnableOSR))
     {
     if (comp->getOption(TR_TraceOSR) && !comp->getOption(TR_DisableOSRSharedSlots))
        {
        (*comp) << "OSRCompilationData is " << *comp->getOSRCompilationData() << "\n";
        }
     }

   if (comp->getOption(TR_AOT) && (comp->getOption(TR_TraceRelocatableDataCG) || comp->getOption(TR_TraceRelocatableDataDetailsCG) || comp->getOption(TR_TraceReloCG)))
     {
     traceMsg(comp, "\n<relocatableDataCG>\n");
     if (comp->getOption(TR_TraceRelocatableDataDetailsCG)) // verbose output
        {
        uint8_t * aotMethodCodeStart = (uint8_t *)comp->getAotMethodCodeStart();
        traceMsg(comp, "Code start = %8x, Method start pc = %x, Method start pc offset = 0x%x\n", aotMethodCodeStart, cg->getCodeStart(), cg->getCodeStart() - aotMethodCodeStart);
        }
     cg->getAheadOfTimeCompile()->dumpRelocationData();
     traceMsg(comp, "</relocatableDataCG>\n");
     }

     if (debug("dumpCodeSizes"))
        {
        diagnostic("%08d   %s\n", cg->getCodeLength(), comp->signature());
        }

     if (comp->getCurrentMethod() == NULL)
        {
        comp->getMethodSymbol()->setMethodAddress(cg->getBinaryBufferStart());
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
        if (TR::Compiler->target.is64Bit())
        {
        setDllSlip((char*)cg->getCodeStart(),(char*)cg->getCodeStart()+cg->getCodeLength(),"SLIPDLL64", comp);
        }
     else
        {
        setDllSlip((char*)cg->getCodeStart(),(char*)cg->getCodeStart()+cg->getCodeLength(),"SLIPDLL31", comp);
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

   cg->emitSnippets();

   if (comp->getOption(TR_EnableOSR))
      {
      comp->getOSRCompilationData()->checkOSRLimits();
      comp->getOSRCompilationData()->compressInstruction2SharedSlotMap();
      }

   if (comp->getOption(TR_TraceCG) || comp->getOptions()->getTraceCGOption(TR_TraceCGPostBinaryEncoding))
      {
      diagnostic("\nbuffer start = %8x, code start = %8x, buffer length = %d", cg->getBinaryBufferStart(), cg->getCodeStart(), cg->getEstimatedCodeLength());
      diagnostic("\n");
      const char * title = "Post Binary Instructions";

      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), title, false, true);

      traceMsg(comp,"<snippets>");
      comp->getDebug()->print(comp->getOutFile(), cg->getSnippetList());
      traceMsg(comp,"</snippets>\n");

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

   if (debug("verifyFinalNodeReferenceCounts"))
      {
      if (cg->getDebug())
         cg->getDebug()->verifyFinalNodeReferenceCounts(comp->getMethodSymbol());
      }
   }




void
OMR::CodeGenPhase::performPeepholePhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation * comp = cg->comp();
   phase->reportPhase(PeepholePhase);

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->doPeephole();

   if (comp->getOption(TR_TraceCG))
      comp->getDebug()->dumpMethodInstrs(comp->getOutFile(), "Post Peephole Instructions", false);
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

     if (comp->getOption(TR_TraceCG) || comp->getOptions()->getTraceCGOption(TR_TraceEarlyStackMap))
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

      TR_RegisterKinds colourableKindsToAssign;
      TR_RegisterKinds nonColourableKindsToAssign = cg->prepareRegistersForAssignment();

      cg->jettisonAllSpills(); // Spill temps used before now may lead to conflicts if also used by register assignment

      // Do local register assignment for non-colourable registers.
      //
      if(cg->getTraceRAOption(TR_TraceRAListing))
         if(cg->getDebug()) cg->getDebug()->dumpMethodInstrs(comp->getOutFile(),"Before Local RA",false);

      cg->doRegisterAssignment(nonColourableKindsToAssign);

      if (comp->compilationShouldBeInterrupted(AFTER_REGISTER_ASSIGNMENT_CONTEXT))
         {
         comp->failCompilation<TR::CompilationInterrupted>("interrupted after RA");
         }
      }

   if (comp->getOption(TR_TraceCG) || comp->getOptions()->getTraceCGOption(TR_TraceCGPostRegisterAssignment))
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

   if (comp->getOption(TR_TraceCG) || comp->getOption(TR_TraceTrees) || comp->getOptions()->getTraceCGOption(TR_TraceCGPreInstructionSelection))
      comp->dumpMethodTrees("Pre Instruction Selection Trees");

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->doInstructionSelection();

   if (comp->getOption(TR_TraceCG) || comp->getOptions()->getTraceCGOption(TR_TraceCGPostInstructionSelection))
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

   if (TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads())
      {
      // TODO (GuardedStorage): We need to come up with a better solution than anchoring aloadi's
      // to enforce certain evaluation order
      traceMsg(comp, "GuardedStorage: in performSetupForInstructionSelectionPhase\n");

      auto mapAllocator = getTypedAllocator<std::pair<TR::TreeTop*, TR::TreeTop*> >(comp->allocator());

      std::map<TR::TreeTop*, TR::TreeTop*, std::less<TR::TreeTop*>, TR::typed_allocator<std::pair<TR::TreeTop* const, TR::TreeTop*>, TR::Allocator> >
         currentTreeTopToappendTreeTop(std::less<TR::TreeTop*> (), mapAllocator);

      TR_BitVector *unAnchorableAloadiNodes = comp->getBitVectorPool().get();

      for (TR::PreorderNodeIterator iter(comp->getStartTree(), comp); iter != NULL; ++iter)
         {
         TR::Node *node = iter.currentNode();

         traceMsg(comp, "GuardedStorage: Examining node = %p\n", node);

         // isNullCheck handles both TR::NULLCHK and TR::ResolveAndNULLCHK
         // both of which do not operate on their child but their
         // grandchild (or greatgrandchild).
         if (node->getOpCode().isNullCheck())
            {
            // An aloadi cannot be anchored if there is a Null Check on
            // its child. There are two situations where this occurs.
            // The first is when doing an aloadi off some node that is
            // being NULLCHK'd (see Ex1). The second is when doing an
            // icalli in which case the aloadi loads the VFT of an
            // object that must be NULLCHK'd (see Ex2).
            //
            // Ex1:
            //    n1n NULLCHK on n3n
            //    n2n    aloadi f    <-- First Child And Parent of Null Chk'd Node
            //    n3n       aload O
            //
            // Ex2:
            //    n1n NULLCHK on n4n
            //    n2n    icall foo        <-- First Child
            //    n3n       aloadi <vft>  <-- Parent of Null Chk'd Node
            //    n4n          aload O
            //    n4n       ==> aload O

            TR::Node *nodeBeingNullChkd = node->getNullCheckReference();
            if (nodeBeingNullChkd)
               {
               TR::Node *firstChild = node->getFirstChild();
               TR::Node *parentOfNullChkdNode = NULL;

               if (firstChild->getOpCode().isCall() &&
                   firstChild->getOpCode().isIndirect())
                  {
                  parentOfNullChkdNode = firstChild->getFirstChild();
                  }
               else
                  {
                  parentOfNullChkdNode = firstChild;
                  }

               if (parentOfNullChkdNode &&
                   parentOfNullChkdNode->getOpCodeValue() == TR::aloadi &&
                   parentOfNullChkdNode->getNumChildren() > 0 &&
                   parentOfNullChkdNode->getFirstChild() == nodeBeingNullChkd)
                  {
                  unAnchorableAloadiNodes->set(parentOfNullChkdNode->getGlobalIndex());
                  traceMsg(comp, "GuardedStorage: Cannot anchor  %p\n", firstChild);
                  }
               }
            }
         else if (node->getOpCodeValue() == TR::aloadi &&
                  !unAnchorableAloadiNodes->isSet(node->getGlobalIndex()))
            {
            TR::TreeTop* anchorTreeTop = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, node));
            TR::TreeTop* appendTreeTop = iter.currentTree();

            if (currentTreeTopToappendTreeTop.count(appendTreeTop) > 0)
               {
               appendTreeTop = currentTreeTopToappendTreeTop[appendTreeTop];
               }

            // Anchor the aloadi before the current treetop
            appendTreeTop->insertBefore(anchorTreeTop);
            currentTreeTopToappendTreeTop[iter.currentTree()] = anchorTreeTop;

            traceMsg(comp, "GuardedStorage: Anchored  %p to treetop = %p\n", node, anchorTreeTop);
            }
         }

      comp->getBitVectorPool().release(unAnchorableAloadiNodes);
      }

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
         TR_LinkHead<TR_RegisterCandidate> emptyCandidateList;
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

   if (comp->getOption(TR_TraceCG) || comp->getOption(TR_TraceTrees))
      comp->dumpMethodTrees("Pre Uncommon Call Constant Node Trees");

   TR::LexicalMemProfiler mp(phase->getName(), comp->phaseMemProfiler());
   LexicalTimer pt(phase->getName(), comp->phaseTimer());

   cg->uncommonCallConstNodes();

   if (comp->getOption(TR_TraceCG) || comp->getOption(TR_TraceTrees))
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
OMR::CodeGenPhase::performShrinkWrappingPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   TR::Compilation *comp = cg->comp();
   if (comp->getOptimizer())
      comp->getOptimizer()->performVeryLateOpts();
   }

void
OMR::CodeGenPhase::performInsertDebugCountersPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase)
   {
   cg->insertDebugCounters();
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
      case ShrinkWrappingPhase:
	 return "ShrinkWrappingPhase";
      case InliningReportPhase:
	 return "InliningReportPhase";
      case InsertDebugCountersPhase:
	 return "InsertDebugCountersPhase";
      case CleanUpFlagsPhase:
	 return "CleanUpFlagsPhase";
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
