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

#include "optimizer/OSRDefAnalysis.hpp"

#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/OSRData.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/arrayof.h"
#include "cs2/bitvectr.h"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "runtime/Runtime.hpp"

#define OPT_DETAILS "O^O OSR LIVE RANGE ANALYSIS: "

class TR_Structure;

// The following assumptions simplify the reaching definitions analysis for OSR:
// 1. OSR only operates on autos, parms, and pending pushes in Java
// 2. Loads are not treated as defs
// 3. Exception handling is already done

// class TR_OSRDefInfo
TR_OSRDefInfo::TR_OSRDefInfo(TR::OptimizationManager *manager)
   : TR_UseDefInfo(manager->comp(), manager->optimizer()->getMethodSymbol()->getFlowGraph(), manager->optimizer(),
      false /* !requiresGlobals */, false /* !prefersGlobals */, false /* !loadsShouldBeDefs */,
      false /* !cannotOmitTrivialDefs */, false /* !conversionRegsOnly */, false /* !doCompletion */)
   {
   // need to call prepareUseDefInfo here not in base constructor. If called in base constructor, base class
   // instances of virtual functions are called, because the derived object would not yet have been set up.
   // ie prepareUseDefInfo will call base function TR_UseDefInfo::prepareAnalysisback NOT
   // derived function TR_OSRDefInfo:prepareAnalysis
   prepareUseDefInfo(false /* !requiresGlobals */, false /* !prefersGlobals */, false /* !cannotOmitTrivialDefs */, false /* !conversionRegsOnly */);
   }

bool TR_OSRDefInfo::performAnalysis(AuxiliaryData &aux)
   {
   _methodSymbol = optimizer()->getMethodSymbol();
   if (!infoIsValid())
      return false;
   addSharingInfo(aux);
   bool result = TR_UseDefInfo::performAnalysis(aux);
   performFurtherAnalysis(aux);
   return result;
   }

void TR_OSRDefInfo::performFurtherAnalysis(AuxiliaryData &aux)
   {
   if (!infoIsValid())
      {
      //TR_ASSERT(0, "osr def analysis failed. need to undo whatever we did for OSR and disable OSR but we don't. Implementation is not completed.\n");
      traceMsg(comp(), "compilation failed for %s because osr def analysis failed\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
      comp()->failCompilation<TR::ILGenFailure>("compilation failed because osr def analysis failed");
      }

   // Iterate through OSR reaching definitions bit vectors and save it in method symbol's data structure.
   TR::SymbolReferenceTable *symRefTab   = comp()->getSymRefTab();
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   for (intptr_t i = 0; i < methodSymbol->getOSRPoints().size(); ++i)
      {
      TR_OSRPoint *point = (methodSymbol->getOSRPoints())[i];
      if (point == NULL) continue;
      uint32_t osrIndex = point->getOSRIndex();
      TR_ASSERT(osrIndex == i, "something doesn't make sense\n");
      TR_BitVector *info = aux._defsForOSR[osrIndex];
      //one reason that info can be NULL is that the block that contains that i-th osr point has
      //been deleted (e.g., because it was unreachable), and therefore, we have never set _defsForOSR[i] to
      //a non-NULL value
      if (info)
         {
         TR_BitVectorIterator cursor(*info);
         while (cursor.hasMoreElements())
            {
            int32_t j = cursor.getNextElement();
            if (j < getNumExpandedDefsOnEntry()) continue;
            int32_t jj = aux._sideTableToUseDefMap[j];

            TR::Node *defNode = getNode(jj);
            if (defNode == NULL) continue;
            TR::SymbolReference *defSymRef = defNode->getSymbolReference();
            if (defSymRef == NULL) continue;

            // Only interested in parameters, autos and pending pushes that can be shared
            int32_t slot = defSymRef->getCPIndex();
            if (slot >= methodSymbol->getFirstJitTempIndex()) continue;
            int32_t symRefNum = defSymRef->getReferenceNumber();
            TR::DataType dt = defSymRef->getSymbol()->getDataType();
            bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;

            if (methodSymbol->sharesStackSlot(defSymRef))
               {
               List<TR::SymbolReference> *list = NULL;
               if (slot < 0)
                  list = &methodSymbol->getPendingPushSymRefs()->element(-slot-1);
               else
                  list = &methodSymbol->getAutoSymRefs()->element(slot);
               ListIterator<TR::SymbolReference> listIt(list);
               //find the order (index) of the symref in the list
               int symRefOrder = 0;
               for (TR::SymbolReference* symRef = listIt.getFirst(); symRef; symRef = listIt.getNext(), symRefOrder++)
                  if (symRef == defSymRef)
                     break;
               TR_ASSERT(symRefOrder < list->getSize(), "symref #%d on node n%dn not found\n", defSymRef->getReferenceNumber(), defNode->getGlobalIndex());
               comp()->getOSRCompilationData()->addSlotSharingInfo(point->getByteCodeInfo(),
                     slot, symRefNum, symRefOrder, defSymRef->getSymbol()->getSize(), takesTwoSlots);
               if (trace())
                  {
                  TR_ByteCodeInfo& bcInfo = point->getByteCodeInfo();
                  traceMsg(comp(), "added (callerIndex=%d, bcIndex=%d)->(slot=%d, ref#=%d) at OSR point %d side %d def %d\n",
                        bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex(), slot, symRefNum, i, j, jj);
                  }
               }
            }
         }
      comp()->getOSRCompilationData()->ensureSlotSharingInfoAt(point->getByteCodeInfo());
      }
   }


void TR_OSRDefInfo::addSharingInfo(AuxiliaryData &aux)
   {
   //For each slot shared by auto/parm/pps, let S be the set of symbols that are mapped to that slot.
   //we compute the union of the def bit-vectors of the symbols in S and assign it to the def bit-vector
   //of every symbol in S.

   TR_Array<List<TR::SymbolReference> > *ppsListArray = _methodSymbol->getPendingPushSymRefs();
   TR_BitVector *prevTwoSlotUnionDef = new (comp()->trMemory()->currentStackRegion()) TR_BitVector(getBitVectorSize(), comp()->trMemory()->currentStackRegion());
   TR_BitVector *twoSlotUnionDef = new (comp()->trMemory()->currentStackRegion()) TR_BitVector(getBitVectorSize(), comp()->trMemory()->currentStackRegion());
   TR_BitVector unionDef(getBitVectorSize(), comp()->trMemory()->currentStackRegion());

   bool isTwoSlotSymRefAtPrevSlot = false;
   for (int i = 0; ppsListArray && i < ppsListArray->size(); ++i)
      {
      List<TR::SymbolReference> ppsList = (*ppsListArray)[i];
      ListIterator<TR::SymbolReference> ppsIt(&ppsList);
      bool isTwoSlotSymRefAtThisSlot = false;
      for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
         {
         TR::DataType dt = symRef->getSymbol()->getDataType();
         bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
         if (takesTwoSlots)
            {
            isTwoSlotSymRefAtThisSlot = true;
            break;
            }
         }

      if ((ppsList.getSize() > 1) ||
          isTwoSlotSymRefAtThisSlot ||
          isTwoSlotSymRefAtPrevSlot)
         {
         unionDef.empty();
         twoSlotUnionDef->empty();

         for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            TR_BitVector *defs = aux._defsForSymbol[symIndex];
            unionDef |= *defs;
            TR::DataType dt = symRef->getSymbol()->getDataType();
            bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
            if (takesTwoSlots)
               *twoSlotUnionDef |= *defs;
            }

         unionDef |= *prevTwoSlotUnionDef;

         for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            // is assignment okay here, before it was reference only?
            *aux._defsForSymbol[symIndex] = unionDef;

            if (!prevTwoSlotUnionDef->isEmpty())
               {
               List<TR::SymbolReference> prevppsList = (*ppsListArray)[i-1];
               ListIterator<TR::SymbolReference> prevppsIt(&prevppsList);
               for (TR::SymbolReference* prevSymRef = prevppsIt.getFirst(); prevSymRef; prevSymRef = prevppsIt.getNext())
                  {
                  TR::DataType prevdt = prevSymRef->getSymbol()->getDataType();
                  bool doesPrevTakesTwoSlots = prevdt == TR::Int64 || prevdt == TR::Double;
                  if (doesPrevTakesTwoSlots)
                     {
                     uint16_t prevSymIndex = prevSymRef->getSymbol()->getLocalIndex();
                     *aux._defsForSymbol[prevSymIndex] |= unionDef;
                     }
                  }
               }
            }

         TR_BitVector *swap = prevTwoSlotUnionDef;
         prevTwoSlotUnionDef = twoSlotUnionDef;
         twoSlotUnionDef = swap;
         }
      else
         prevTwoSlotUnionDef->empty();

      isTwoSlotSymRefAtPrevSlot = isTwoSlotSymRefAtThisSlot;
      }


   TR_Array<List<TR::SymbolReference> > *autosListArray = _methodSymbol->getAutoSymRefs();
   prevTwoSlotUnionDef->empty();
   isTwoSlotSymRefAtPrevSlot = false;
   for (int i = 0; autosListArray && i < autosListArray->size(); ++i)
      {
      List<TR::SymbolReference> autosList = (*autosListArray)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      bool isTwoSlotSymRefAtThisSlot = false;
      for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
         {
         TR::DataType dt = symRef->getSymbol()->getDataType();
         bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
         if (takesTwoSlots)
            {
            isTwoSlotSymRefAtThisSlot = true;
            break;
            }
         }

      if ((autosList.getSize() > 1) ||
          isTwoSlotSymRefAtThisSlot ||
          isTwoSlotSymRefAtPrevSlot)
         {
         if (!autosIt.getFirst() || (autosIt.getFirst()->getCPIndex() >= _methodSymbol->getFirstJitTempIndex()))
            continue;
         unionDef.empty();
         twoSlotUnionDef->empty();

         for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            TR_BitVector *defs = aux._defsForSymbol[symIndex];
            unionDef |= *defs;
            TR::DataType dt = symRef->getSymbol()->getDataType();
            bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
            if (takesTwoSlots)
               *twoSlotUnionDef |= *defs;
            }

         unionDef |= *prevTwoSlotUnionDef;

         for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            // is assignment okay here, before it was reference only?
            *aux._defsForSymbol[symIndex] = unionDef;

            if (!prevTwoSlotUnionDef->isEmpty())
               {
               List<TR::SymbolReference> prevautosList = (*autosListArray)[i-1];
               ListIterator<TR::SymbolReference> prevautosIt(&prevautosList);
               for (TR::SymbolReference* prevSymRef = prevautosIt.getFirst(); prevSymRef; prevSymRef = prevautosIt.getNext())
                  {
                  TR::DataType prevdt = prevSymRef->getSymbol()->getDataType();
                  bool doesPrevTakesTwoSlots = prevdt == TR::Int64 || prevdt == TR::Double;
                  if (doesPrevTakesTwoSlots)
                     {
                     uint16_t prevSymIndex = prevSymRef->getSymbol()->getLocalIndex();
                     *aux._defsForSymbol[prevSymIndex] |= unionDef;
                     }
                  }
               }
            }

         TR_BitVector *swap = prevTwoSlotUnionDef;
         prevTwoSlotUnionDef = twoSlotUnionDef;
         twoSlotUnionDef = swap;
         }
      else
         prevTwoSlotUnionDef->empty();

      isTwoSlotSymRefAtPrevSlot = isTwoSlotSymRefAtThisSlot;
      }
   }

void TR_OSRDefInfo::processReachingDefinition(void* vblockInfo, AuxiliaryData &aux)
   {
   buildOSRDefs(vblockInfo, aux);
   }

// Build OSR def information from the bit vector information from reaching defs
void TR_OSRDefInfo::buildOSRDefs(void *vblockInfo, AuxiliaryData &aux)
   {

/*
   for (treeTop = comp()->getStartTree(); treeTop != NULL; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         TR_ReachingDefinitions::ContainerType* analysisInfo = reachingDefinitions._blockAnalysisInfo[block->getNumber()];
         if (trace())
            {
            traceMsg(comp(), "analysisInfo at node %p \n", node);
            analysisInfo->print(comp());
            traceMsg(comp(), "\n");
            }
         continue;
         }

      }
*/
   if (trace())
      traceMsg(comp(), "Just before buildOSRDefs\n");


   // Allocate the array of bit vectors that will represent live definitions at OSR points
   //
   int32_t numOSRPoints = _methodSymbol->getNumOSRPoints();
   aux._defsForOSR.resize(numOSRPoints, NULL);

   TR::Block *block;
   TR::TreeTop *treeTop;
   TR_ReachingDefinitions::ContainerType **blockInfo = (TR_ReachingDefinitions::ContainerType**)vblockInfo;
   TR_ReachingDefinitions::ContainerType *analysisInfo = NULL;
   TR_OSRPoint *nextOsrPoint = NULL;

   comp()->incVisitCount();

   // Build UseDef info for the implicit OSR point at method entry
   //
   if (comp()->isOutermostMethod() && comp()->getHCRMode() == TR::osr)
      {
      TR_ByteCodeInfo bci;
      bci.setCallerIndex(-1);
      bci.setByteCodeIndex(0);
      nextOsrPoint = _methodSymbol->findOSRPoint(bci);
      TR_ASSERT(nextOsrPoint != NULL, "Cannot find a OSR point for method entry");
      buildOSRDefs(comp()->getStartTree()->getNode(), blockInfo[comp()->getStartTree()->getNode()->getBlock()->getNumber()],
         NULL, nextOsrPoint, NULL, aux);
      nextOsrPoint = NULL;
      }

   for (treeTop = comp()->getStartTree(); treeTop != NULL; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         if (blockInfo)
            analysisInfo = blockInfo[block->getNumber()];
         continue;
         }

      TR_OSRPoint *osrPoint = NULL;
      bool isPotentialOSRPoint = comp()->isPotentialOSRPointWithSupport(treeTop);

      // If we reach an OSR point and we require a transition point at it,
      // build the defs immediately
      if (isPotentialOSRPoint && (comp()->isOSRTransitionTarget(TR::preExecutionOSR)
          || comp()->requiresAnalysisOSRPoint(node)))
         {
         osrPoint = _methodSymbol->findOSRPoint(node->getByteCodeInfo());
         TR_ASSERT(osrPoint != NULL, "Cannot find a pre OSR point for node %p", node);
         }

      buildOSRDefs(node, analysisInfo, osrPoint, nextOsrPoint, NULL, aux);
      nextOsrPoint = NULL;

      if (isPotentialOSRPoint && comp()->isOSRTransitionTarget(TR::postExecutionOSR))
         {
         // Skip to the end of the OSR region, processing all treetops along the way
         TR::TreeTop *pps = treeTop->getNextTreeTop();
         TR_ByteCodeInfo bci = _methodSymbol->getOSRByteCodeInfo(treeTop->getNode());
         while (pps && _methodSymbol->isOSRRelatedNode(pps->getNode(), bci))
            {
            buildOSRDefs(pps->getNode(), analysisInfo, NULL, NULL, NULL, aux);
            treeTop = pps;
            pps = pps->getNextTreeTop();
            }

         // If we require a induction point after the OSR point, store the OSR point
         // to be processed on the next call to buildOSRDefs
         bci.setByteCodeIndex(bci.getByteCodeIndex() + comp()->getOSRInductionOffset(node));
         nextOsrPoint = _methodSymbol->findOSRPoint(bci);
         TR_ASSERT(nextOsrPoint != NULL, "Cannot find a post OSR point for node %p", node);
         }
      }

   // Print some debugging information
   if (trace())
      {
      traceMsg(comp(), "\nOSR def info:\n");
      }

   for (int i = 0; i < numOSRPoints; ++i)
      {
      TR_BitVector *info = aux._defsForOSR[i];

      //one reason that info can be NULL is that the block that contains that i-th osr point has
      //been deleted (e.g., because it was unreachable), and therefore, we have never set _defsForOSR[i] to
      //a non-NULL value
      if (info == NULL) continue;

      TR_ASSERT(!info->isEmpty(), "OSR def info at index %d is empty", i);
      if (trace())
         {
         if (info->isEmpty())
            {
            traceMsg(comp(), "OSR def info at index %d is empty\n", i);
            continue;
            }
         TR_ByteCodeInfo& bcinfo = _methodSymbol->getOSRPoints()[i]->getByteCodeInfo();
         traceMsg(comp(), "OSR defs at index %d bcIndex %d callerIndex %d\n", i, bcinfo.getByteCodeIndex(), bcinfo.getCallerIndex());
         info->print(comp());
         traceMsg(comp(), "\n");
         }
      }
   }

void TR_OSRDefInfo::buildOSRDefs(TR::Node *node, void *vanalysisInfo, TR_OSRPoint *osrPoint, TR_OSRPoint *osrPoint2, TR::Node *parent, AuxiliaryData &aux)
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (node->getVisitCount() == visitCount)
      return;
   node->setVisitCount(visitCount);
   TR_ReachingDefinitions::ContainerType *analysisInfo = (TR_ReachingDefinitions::ContainerType*)vanalysisInfo;

   // Look in the children first.
   //
   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      buildOSRDefs(node->getChild(i), analysisInfo, osrPoint, osrPoint2, node, aux);
      }

   scount_t expandedNodeIndex = node->getLocalIndex(); //node->getUseDefIndex();
   if (expandedNodeIndex != NULL_USEDEF_SYMBOL_INDEX && expandedNodeIndex != 0)
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *sym = symRef->getSymbol();
      uint16_t symIndex = sym->getLocalIndex();
      TR_BitVector *defsForSymbol = aux._defsForSymbol[symIndex];
      if (!defsForSymbol->isEmpty() &&
         isExpandedDefIndex(expandedNodeIndex) &&
         !sym->isRegularShadow() &&
         !sym->isMethod())
         {
            if (trace())
               {
               traceMsg(comp(), "defs for symbol %d with symref index %d\n", symIndex, symRef->getReferenceNumber());
               defsForSymbol->print(comp());
               traceMsg(comp(), "\n");
               }
            *analysisInfo -= *defsForSymbol;
            analysisInfo->set(expandedNodeIndex);
         }
      }

   if (parent == NULL)
      {
      if (trace())
         {
         traceMsg(comp(), "analysisInfo at node %p \n", node);
         analysisInfo->print(comp());
         traceMsg(comp(), "\n");
         }

      if (osrPoint != NULL)
         {
         uint32_t osrIndex = osrPoint->getOSRIndex();
         aux._defsForOSR[osrIndex] = new (aux._region) TR_BitVector(aux._region);
         *aux._defsForOSR[osrIndex] |= *analysisInfo;
         if (trace())
            {
            traceMsg(comp(), "_defsForOSR[%d] at node %p \n", osrIndex, node);
            aux._defsForOSR[osrIndex]->print(comp());
            traceMsg(comp(), "\n");
            }
         }
      if (osrPoint2 != NULL)
         {
         uint32_t osrIndex = osrPoint2->getOSRIndex();
         aux._defsForOSR[osrIndex] = new (aux._region) TR_BitVector(aux._region);
         *aux._defsForOSR[osrIndex] |= *analysisInfo;
         if (trace())
            {
            traceMsg(comp(), "_defsForOSR[%d] after node %p \n", osrIndex, node);
            aux._defsForOSR[osrIndex]->print(comp());
            traceMsg(comp(), "\n");
            }
         }
      }
   }


// class TR_OSRDefAnalysis
TR_OSRDefAnalysis::TR_OSRDefAnalysis(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_OSRDefAnalysis::perform()
   {
   if (comp()->getOption(TR_EnableOSR))
      {
      if (comp()->getOption(TR_DisableOSRSharedSlots))
         {
         if (trace())
            traceMsg(comp(), "OSR is enabled but OSR def analysis is not.\n");
         return 0;
         }

      if (!comp()->canAffordOSRControlFlow())
         {
         if (trace())
            traceMsg(comp(), "OSR is enabled but no longer in use for this compilation.\n");
         return 0;
         }
      }
   else
      {
      if (trace())
         traceMsg(comp(), "Options is not enabled -- returning from OSR reaching definitions analysis.\n");
      return 0;
      }

   // Determine if the analysis is necessary
   if (!requiresAnalysis())
      {
      if (trace())
         {
         traceMsg(comp(), "%s OSR reaching definitions analysis is not required because there is no sharing\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         traceMsg(comp(), "Returning...\n");
         }

      return 0;
      }
   else if (!comp()->supportsInduceOSR())
      {
      if (comp()->getOption(TR_TraceOSR))
         {
         traceMsg(comp(), "%s OSR reaching definitions analysis is not required because OSR is not supported\n",
               optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         traceMsg(comp(), "Returning...\n");
         }
      return 0;
      }
   else if (comp()->isPeekingMethod())
      {
      if (trace())
         {
         traceMsg(comp(), "%s OSR reaching definition analysis is not required because we are peeking\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         traceMsg(comp(), "Returning...\n");
         }
      return 0;
      }
   else
      {
      TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findOrCreateOSRMethodData(comp()->getCurrentInlinedSiteIndex(), comp()->getMethodSymbol());
      if (osrMethodData->hasSlotSharingOrDeadSlotsInfo())
         {
         if (trace())
            {
            traceMsg(comp(), "%s OSR reaching definition analysis is not required as it has already been calculated\n",
               optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
            traceMsg(comp(), "Returning...\n");
            }
         return 0;
         }
      }

   if (trace())
      {
      traceMsg(comp(), "%s OSR reaching definition analysis is required\n",
         optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
      }

   TR_Structure* rootStructure = TR_RegionAnalysis::getRegions(comp(), optimizer()->getMethodSymbol());
   optimizer()->getMethodSymbol()->getFlowGraph()->setStructure(rootStructure);
   TR_ASSERT(rootStructure, "Structure is NULL");

   if (trace())
      {
      traceMsg(comp(), "Starting OSR reaching definitions analysis\n");
      comp()->dumpMethodTrees("Before OSR reaching definitions analysis", optimizer()->getMethodSymbol());
      }

   {
   TR::LexicalMemProfiler mp("osr defs", comp()->phaseMemProfiler());

   TR_OSRDefInfo osrDefInfo(manager());
   }

   //set the structure to NULL so that the inliner (which is applied very soon after) doesn't need
   //update it.
   optimizer()->getMethodSymbol()->getFlowGraph()->invalidateStructure();

   return 0;
   }

//returns true if there is a shared auto slot or a shared pending push temp slot, and returns false otherwise.
bool TR_OSRDefAnalysis::requiresAnalysis()
   {
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   return methodSymbol->sharesStackSlots(comp());
   }

const char *
TR_OSRDefAnalysis::optDetailString() const throw()
   {
   return "O^O OSR DEF ANALYSIS: ";
   }

//Not needed anymore. I'll keep it commented out just in case it's needed in the future.
//Check whether the first real non-profiling tree top in the block
//is an OSR Helper call. If it is, return that treetop. Otherwise return null.
/*
TR::TreeTop* findOSRHelperCall(TR::Compilation* comp, TR::Block * osrCodeBlock)
   {
   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();
   TR::SymbolReference *osrHelper = symRefTab->findOrCreateRuntimeHelper(TR_prepareForOSR, false, false, true);
   TR::TreeTop *callTree = NULL;
   for (TR::TreeTop* t = osrCodeBlock->getFirstRealTreeTop(); t; t = t->getNextTreeTop())
      {
      TR::Node* n = t->getNode();
      if (n->isProfilingCode()) continue;
      if (n->getOpCodeValue() == TR::treetop)
         n = n->getFirstChild();
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "checking node %p in OSR code block\n", n);
      if (n->getOpCodeValue() == TR::call && n->getSymbolReference() == osrHelper)
         {
         //OSR Helper call found
         callTree = t;
         break;
         }
      else
         {
         //TR_ASSERT(false, "first treetop in OSR code block %x is not an OSR helper call", osrBlock);
         }
      }
   return callTree;
   }
*/




// class TR_OSRLiveRangeAnalysis
TR_OSRLiveRangeAnalysis::TR_OSRLiveRangeAnalysis(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _liveVars(NULL),
     _pendingPushSymRefs(NULL),
     _sharedSymRefs(NULL),
     _workBitVector(NULL),
     _workDeadSymRefs(NULL),
     _visitedBCI(NULL)
   {}

bool TR_OSRLiveRangeAnalysis::shouldPerformAnalysis()
   {
   if (!comp()->getOption(TR_EnableOSR))
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "Should not perform OSRLiveRangeAnalysis -- OSR Option not enabled\n");
      return false;
      }
   else if (comp()->isPeekingMethod())
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "Should not perform OSRLiveRangeAnalysis -- Not required because we are peeking\n");
      return false;
      }
   else if (!comp()->supportsInduceOSR())
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "Should not perform OSRLiveRangeAnlysis -- OSR is not supported under the current configuration\n");
      return false;
      }
   else if (comp()->getOSRMode() == TR::involuntaryOSR)
      {
      static const char* disableOSRPointDeadslotsBookKeeping = feGetEnv("TR_DisableOSRPointDeadslotsBookKeeping");
      if (comp()->getOption(TR_MimicInterpreterFrameShape))
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "No need to perform OSRLiveRangeAnlysis under mimic interpreter frame shape\n");
         return false;
         }
      else if (disableOSRPointDeadslotsBookKeeping) // save some compile time
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "Dead slots bookkeeping is disabled and therefore OSRLiveRangeAnlysis is not needed\n");
         return false;
         }
      }

   // Skip optimization if there are no OSR points
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   if (methodSymbol->getNumOSRPoints() == 0)
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "No OSR points, skip liveness\n");
      return false;
      }

   return true;
   }

int32_t TR_OSRLiveRangeAnalysis::perform()
   {
   // Check if the opt should be performed
   if (!shouldPerformAnalysis())
      return 0;

   if (comp()->getOption(TR_TraceOSR))
      traceMsg(comp(), "OSR reaching live range analysis can be done\n");

   // Initalize BitVectors that exist only for this optimization
   _pendingPushSymRefs = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
   _sharedSymRefs = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
   _workBitVector = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
   _workDeadSymRefs = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);
   _visitedBCI = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc);

   bool containsAuto = false, sharesParm = false, containsPendingPush = false;
   TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findOSRMethodData(
      comp()->getCurrentInlinedSiteIndex(), comp()->getMethodSymbol());

   // Detect autos, pending push temps and whether there is a shared parm slot
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   TR_Array<List<TR::SymbolReference>> *autosListArray = methodSymbol->getAutoSymRefs();
   if (comp()->getOSRMode() == TR::involuntaryOSR && autosListArray)
      {
      TR_BitVector *symRefs = new (trHeapMemory()) TR_BitVector(0, trMemory(), heapAlloc);
      osrMethodData->setSymRefs(symRefs);
      }

   for (uint32_t i = 0; autosListArray && i < autosListArray->size(); ++i)
      {
      List<TR::SymbolReference> &autosList = (*autosListArray)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      for (TR::SymbolReference *symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
         {
         if (symRef->getSymbol()->isParm())
            {
            if (methodSymbol->sharesStackSlot(symRef))
               {
               sharesParm = true;
               _sharedSymRefs->set(symRef->getReferenceNumber());
               }
            }
         else if (symRef->getCPIndex() < methodSymbol->getFirstJitTempIndex())
            {
            containsAuto = true;
            if (methodSymbol->sharesStackSlot(symRef))
               _sharedSymRefs->set(symRef->getReferenceNumber());
            }

         if (comp()->getOSRMode() == TR::involuntaryOSR && osrMethodData->getSymRefs())
            osrMethodData->getSymRefs()->set(symRef->getReferenceNumber());
         }
      }

   TR_Array<List<TR::SymbolReference>> *pendingPushSymRefs = comp()->getMethodSymbol()->getPendingPushSymRefs();
   for (int32_t i = 0; pendingPushSymRefs && i < pendingPushSymRefs->size(); ++i)
      {
      List<TR::SymbolReference> &symRefsAtThisSlot = (*pendingPushSymRefs)[i];
      ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
      for (TR::SymbolReference *symRef = symRefsIt.getCurrent(); symRef; symRef = symRefsIt.getNext())
         {
         containsPendingPush = true;
         _pendingPushSymRefs->set(symRef->getReferenceNumber());
         if (comp()->getMethodSymbol()->sharesStackSlot(symRef))
            _sharedSymRefs->set(symRef->getReferenceNumber());
         }
      }

   if (comp()->getOSRMode() == TR::involuntaryOSR && containsPendingPush)
      {
      if (!osrMethodData->getSymRefs())
         {
         TR_BitVector *symRefs = new (trHeapMemory()) TR_BitVector(0, trMemory(), heapAlloc);
         osrMethodData->setSymRefs(symRefs);
         }
      *osrMethodData->getSymRefs() |= *_pendingPushSymRefs;
      }

   if (comp()->getOption(TR_DisableOSRLiveRangeAnalysis))
      {
      if (comp()->getOption(TR_TraceOSR))
         {
         if (!_sharedSymRefs->isEmpty())
            traceMsg(comp(), "OSRLiveRangeAnalysis is disabled but it is required for correctness. OSR transitions may not work.\n");
         else
            traceMsg(comp(), "OSRLiveRangeAnalysis is disabled.\n");
         }
      return 0;
      }

   // If pending push liveness was tracked in IlGen, process it before
   // the full analysis, to cheapen it or avoid it entirely
   if (containsPendingPush && comp()->pendingPushLivenessDuringIlgen())
      {
      partialAnalysis();
      containsPendingPush = false;
      }

   if (containsAuto || sharesParm || containsPendingPush)
      return fullAnalysis(sharesParm, containsPendingPush);

   return 0;
   }

/*
 * The partial analysis is only concerned with solving which pending push temps
 * are live at certain OSR points.
 */
int32_t TR_OSRLiveRangeAnalysis::partialAnalysis()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_ASSERT(comp()->pendingPushLivenessDuringIlgen(), "Partial analysis is only possible if liveness has been solved in Ilgen");

   if (comp()->getOption(TR_TraceOSR))
      traceMsg(comp(), "Starting partial OSRLiveRangeAnalysis\n");

   TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findOSRMethodData(
      comp()->getCurrentInlinedSiteIndex(), comp()->getMethodSymbol());

   // Visit every OSR point and create slot sharing information
   // Process in same order as liveness
   for (TR::TreeTop *tt = comp()->getMethodSymbol()->getLastTreeTop(); tt; tt = tt->getPrevTreeTop())
      {
      TR::Node *node = tt->getNode();
      bool isPotentialOSRPoint = comp()->isPotentialOSRPointWithSupport(tt);

      TR_ByteCodeInfo &bci = node->getByteCodeInfo();

      if (isPotentialOSRPoint && comp()->isOSRTransitionTarget(TR::postExecutionOSR))
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "Analysing post OSR point for n%dn at %d:%d offset by %d\n", node->getGlobalIndex(),
               bci.getCallerIndex(), bci.getByteCodeIndex(), comp()->getOSRInductionOffset(node));

         TR_ByteCodeInfo bcInfo = bci;
         bcInfo.setByteCodeIndex(bcInfo.getByteCodeIndex() + comp()->getOSRInductionOffset(node));
         TR_OSRPoint *offsetOSRPoint = comp()->getMethodSymbol()->findOSRPoint(bcInfo);
         TR_ASSERT(offsetOSRPoint != NULL, "Cannot find a post OSR point for node %p", node);

         TR_BitVector *ppInfo = osrMethodData->getPendingPushLivenessInfo(bcInfo.getByteCodeIndex());
         if (comp()->getOption(TR_TraceOSR))
            {
            traceMsg(comp(), "Existing liveness information:\n");
            if (ppInfo)
               ppInfo->print(comp());
            else
               traceMsg(comp(), "NULL");
            traceMsg(comp(), "\n");
            }

         pendingPushLiveRangeInfo(node, ppInfo, _pendingPushSymRefs, offsetOSRPoint, osrMethodData);
         pendingPushSlotSharingInfo(node, ppInfo, _sharedSymRefs, offsetOSRPoint);
         }

      if (isPotentialOSRPoint && (comp()->isOSRTransitionTarget(TR::preExecutionOSR) ||
          comp()->requiresAnalysisOSRPoint(node)))
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "Analysing pre OSR point for n%dn at %d:%d\n", node->getGlobalIndex(),
               bci.getCallerIndex(), bci.getByteCodeIndex());

         TR_OSRPoint *osrPoint = comp()->getMethodSymbol()->findOSRPoint(bci);
         TR_ASSERT(osrPoint != NULL, "Cannot find a pre OSR point for node %p", node);

         TR_BitVector *ppInfo = osrMethodData->getPendingPushLivenessInfo(bci.getByteCodeIndex());
         if (comp()->getOption(TR_TraceOSR))
            {
            traceMsg(comp(), "Existing liveness information:\n");
            if (ppInfo)
               ppInfo->print(comp());
            else
               traceMsg(comp(), "NULL");
            traceMsg(comp(), "\n");
            }

         pendingPushLiveRangeInfo(node, ppInfo, _pendingPushSymRefs, osrPoint, osrMethodData);
         pendingPushSlotSharingInfo(node, ppInfo, _sharedSymRefs, osrPoint);
         if (comp()->getOption(TR_FullSpeedDebug))
            buildDeadPendingPushSlotsInfo(node, ppInfo, osrPoint);
         }
      }

   // Certain modes require the method entry to be an OSR point
   if (comp()->isOutermostMethod() && comp()->getHCRMode() == TR::osr)
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "Analysing OSR point at method entry\n");

      TR_ByteCodeInfo bci;
      bci.setCallerIndex(-1);
      bci.setByteCodeIndex(0);
      TR_OSRPoint *osrPoint = comp()->getMethodSymbol()->findOSRPoint(bci);
      TR_ASSERT(osrPoint != NULL, "Cannot find a OSR point for method entry");

      // There should be no slot sharing at the method entry
      pendingPushLiveRangeInfo(comp()->getStartTree()->getNode(), NULL, _pendingPushSymRefs, osrPoint, osrMethodData);
      pendingPushSlotSharingInfo(comp()->getStartTree()->getNode(), NULL, _sharedSymRefs, osrPoint);
      }

   return 1;
   }

/*
 * Allocate the BitVector of dead symbol references against the provided OSR point.
 */
void TR_OSRLiveRangeAnalysis::pendingPushLiveRangeInfo(TR::Node *node, TR_BitVector *liveSymRefs,
   TR_BitVector *allPendingPushSymRefs, TR_OSRPoint *osrPoint, TR_OSRMethodData *osrMethodData)
   {
   int32_t byteCodeIndex = osrPoint->getByteCodeInfo().getByteCodeIndex();
   TR_BitVector *deadBV = NULL;

   // Calculate the dead symbol references, allocating a new BitVector if its required
   _workBitVector->empty();
   *_workBitVector |= *allPendingPushSymRefs;
   if (liveSymRefs)
      *_workBitVector -= *liveSymRefs;
   if (!_workBitVector->isEmpty())
      {
      deadBV = new (trHeapMemory()) TR_BitVector(0, trMemory(), heapAlloc);
      *deadBV = *_workBitVector;
      osrMethodData->addLiveRangeInfo(byteCodeIndex, deadBV);
      }

   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "Live PP variables at OSR point %p of %p bytecode offset %d\n",
         node, osrMethodData, byteCodeIndex);
      if (!liveSymRefs)
         traceMsg(comp(), " NULL");
      else
         liveSymRefs->print(comp());
      traceMsg(comp(), "\n");
      }
   }

/*
 * Solve slot sharing based on the pending push symrefs
 */
void TR_OSRLiveRangeAnalysis::pendingPushSlotSharingInfo(TR::Node *node, TR_BitVector *liveSymRefs,
   TR_BitVector *slotSharingSymRefs, TR_OSRPoint *osrPoint)
   {
   if (liveSymRefs && !liveSymRefs->isEmpty())
      {
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
      TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();

      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "Shared PP slots at OSR point [%p] at %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

      _workBitVector->empty();
      *_workBitVector |= *liveSymRefs;
      *_workBitVector &= *slotSharingSymRefs;
      TR_BitVectorIterator bvi(*_workBitVector);
      while (bvi.hasMoreElements())
         {
         int32_t symRefNum = bvi.getNextElement();

         TR::SymbolReference *symRef = symRefTab->getSymRef(symRefNum);
         int32_t slot = symRef->getCPIndex();
         TR::DataType dt = symRef->getSymbol()->getDataType();
         bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;

         List<TR::SymbolReference> *list = NULL;
         if (slot < 0)
            list = &comp()->getMethodSymbol()->getPendingPushSymRefs()->element(-slot-1);
         else
            list = &comp()->getMethodSymbol()->getAutoSymRefs()->element(slot);

         ListIterator<TR::SymbolReference> listIt(list);
         int symRefOrder = 0;
         for (TR::SymbolReference* slotSymRef = listIt.getFirst(); slotSymRef; slotSymRef = listIt.getNext(), symRefOrder++)
            if (symRef == slotSymRef)
               break;
         TR_ASSERT(symRefOrder < list->getSize(), "symref #%d not found in list of shared slots\n", symRef->getReferenceNumber());

         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "  Slot:%d SymRef:%d TwoSlots:%d\n", slot, symRefNum, takesTwoSlots);

         comp()->getOSRCompilationData()->addSlotSharingInfo(osrPoint->getByteCodeInfo(),
            slot, symRefNum, symRefOrder, symRef->getSymbol()->getSize(), takesTwoSlots);
         }
      }

   comp()->getOSRCompilationData()->ensureSlotSharingInfoAt(osrPoint->getByteCodeInfo());
   }

/*
 * figure out dead pending push slots for this osrPoint and bookkeep the information in OSRMethodMetaData
 */
void TR_OSRLiveRangeAnalysis::buildDeadPendingPushSlotsInfo(TR::Node *node, TR_BitVector *livePendingPushSymRefs, TR_OSRPoint *osrPoint)
   {
   TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
   if (trace())
      traceMsg(comp(), "buildDeadPendingPushSlotsInfo: OSR point [%p] at %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

   TR_Array<List<TR::SymbolReference> > *pendingPushSymRefs = comp()->getMethodSymbol()->getPendingPushSymRefs();
   uint32_t numOfPPSSlots = 0;
   TR_BitVector *deadPPSSlots = NULL;
   if (pendingPushSymRefs)
      {
      numOfPPSSlots = pendingPushSymRefs->size();
      deadPPSSlots = new (trStackMemory()) TR_BitVector(numOfPPSSlots, trMemory(), stackAlloc);
      deadPPSSlots->setAll(numOfPPSSlots);
      }

   if (livePendingPushSymRefs && !livePendingPushSymRefs->isEmpty())
      {
      TR_BitVectorIterator bvi(*livePendingPushSymRefs);
      while (bvi.hasMoreElements())
         {
         int32_t symRefNum = bvi.getNextElement();
         TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
         TR::SymbolReference *symRef = symRefTab->getSymRef(symRefNum);
         int32_t slot = symRef->getCPIndex();
         TR::DataType dt = symRef->getSymbol()->getDataType();
         bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;

         List<TR::SymbolReference> *list = NULL;
         if (trace())
            traceMsg(comp(), "pending push slot %d is live with symref %d takesTwoSlots %d\n", slot, symRefNum, takesTwoSlots);

         deadPPSSlots->reset(-slot-1);
         if (takesTwoSlots)
            deadPPSSlots->reset(-slot);
         }
      }

   if (deadPPSSlots && trace())
      {
      TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
      traceMsg(comp(), "deadppslots at node %p %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
      deadPPSSlots->print(comp());
      traceMsg(comp(), "\n");
      }

   if (deadPPSSlots && !deadPPSSlots->isEmpty())
      {
      TR_BitVectorIterator bvi(*deadPPSSlots);
      while (bvi.hasMoreElements())
         {
         int32_t slot =  -bvi.getNextElement()-1;
         if (trace())
            traceMsg(comp(), "ppslot %d is dead at %d:%d\n", slot, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
         //symRefNum = -1 and symRefOrder = -1 indicate those slots should be zeroed in prepareForOSR
         comp()->getOSRCompilationData()->addSlotSharingInfo(osrPoint->getByteCodeInfo(),
            slot, -1 /* symRefNum */, -1 /* symRefOrder */, TR::Compiler->om.sizeofReferenceAddress() /*symSize*/ , false /*takesTwoSlots*/);
         }
      }

   comp()->getOSRCompilationData()->ensureSlotSharingInfoAt(osrPoint->getByteCodeInfo());
   }

//calculate the intersection of existing deadslots
void
TR_OSRLiveRangeAnalysis::intersectWithExistingDeadSlots (TR_OSRPoint *osrPoint, TR_BitVector *deadPPSSlots, TR_BitVector *deadAutoSlots, bool containsPendingPushes)
   {
   TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
   if (!_visitedBCI->isSet(bcInfo.getByteCodeIndex()))
      return;
   TR_OSRSlotSharingInfo *slotsInfo = comp()->getOSRCompilationData()->getSlotsInfo(bcInfo);

   TR_BitVector existingDeadPPSSlots(comp()->trMemory()->currentStackRegion());
   existingDeadPPSSlots.empty();
   TR_BitVector existingDeadAutoSlots(comp()->trMemory()->currentStackRegion());
   existingDeadAutoSlots.empty();
   if (deadPPSSlots)
      {
      traceMsg(comp(), "deadPPSSlots:");
      deadPPSSlots->print(comp());
      }
   if (deadAutoSlots)
      {
      traceMsg(comp(), "deadAutoSlots:");
      deadAutoSlots->print(comp());
      }

   if (slotsInfo)// remove every dead slot that's not in the current dead symbols bit vector from slotInfos
      {
      TR_Array<TR_OSRSlotSharingInfo::TR_SlotInfo>& slotInfos = slotsInfo->getSlotInfos();
      for (int i= slotInfos.size()-1 ; i >= 0; i--)
         {
         TR_OSRSlotSharingInfo::TR_SlotInfo &slotInfo = slotInfos[i];
         if (slotInfo.symRefOrder == -1)
            {
            TR_ASSERT(slotInfo.symRefNum == -1, "slotInfos[%d].symRefOrder==%d.  Expected symRefNum==-1; actually %d\n", i, slotInfos[i].symRefOrder, slotInfos[i].symRefNum);
            if (slotInfo.slot < 0 && containsPendingPushes)
               {
               TR_ASSERT(deadPPSSlots, "with slotInfo.slot < 0  and containsPendingPushes, there must be at least on pending push symbol in this method");
               int32_t ppsIndex =  -slotInfo.slot - 1;
               existingDeadPPSSlots.set(ppsIndex);
               if (!deadPPSSlots->isSet(ppsIndex))
                  slotInfos.remove(i);
               }

            if (slotInfo.slot >= 0)
               {
               TR_ASSERT_FATAL(deadAutoSlots, "with slotInfos.slot >= 0, there must be at least on auto symbol in this method");
               existingDeadAutoSlots.set(slotInfo.slot);
               if (!deadAutoSlots->isSet(slotInfo.slot))
                  slotInfos.remove(i);
               }
            }
         }
      }

   if (deadPPSSlots)
      *deadPPSSlots &= existingDeadPPSSlots;
   if (deadAutoSlots)
      *deadAutoSlots &= existingDeadAutoSlots;
   }

/*
 * figure out dead slots for this osrPoint and bookkeep the information in OSRMethodMetaData
 */
void TR_OSRLiveRangeAnalysis::buildDeadSlotsInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint, int32_t *liveLocalIndexToSymRefNumberMap, bool containsPendingPushes)
   {

   TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
   if (trace())
      traceMsg(comp(), "buildOSRSlotSharingInfoForDeadSlots: OSR point [%p] at %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

   TR_Array<List<TR::SymbolReference> > *pendingPushSymRefs = NULL;
   uint32_t numOfPPSSlots = 0;
   TR_BitVector *deadPPSSlots = NULL;
   if (containsPendingPushes)
      {
      pendingPushSymRefs = comp()->getMethodSymbol()->getPendingPushSymRefs();
      if (pendingPushSymRefs)
         {
         numOfPPSSlots = pendingPushSymRefs->size();
         deadPPSSlots = new (trStackMemory()) TR_BitVector(numOfPPSSlots, trMemory(), stackAlloc);
         deadPPSSlots->setAll(numOfPPSSlots);
         }
      }

   uint32_t numOfAutoSlots = 0;
   TR_BitVector *deadAutoSlots = NULL;
   numOfAutoSlots = comp()->getMethodSymbol()->getFirstJitTempIndex();
   if (numOfAutoSlots > 0)
      {
      deadAutoSlots = new (trStackMemory()) TR_BitVector(numOfAutoSlots, trMemory(), stackAlloc);
      deadAutoSlots->setAll(numOfAutoSlots);
      }

   if (!liveVars->isEmpty())
      {
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
      TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();

      TR_BitVectorIterator bvi(*_liveVars);
      while (bvi.hasMoreElements())
         {
         int32_t index = bvi.getNextElement();
         int32_t symRefNum = liveLocalIndexToSymRefNumberMap[index];
         if (symRefNum < 0)
            continue;

         TR::SymbolReference *symRef = symRefTab->getSymRef(symRefNum);
         int32_t slot = symRef->getCPIndex();
         if (slot >= comp()->getMethodSymbol()->getFirstJitTempIndex())
            continue;

         TR::DataType dt = symRef->getSymbol()->getDataType();
         bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;

         List<TR::SymbolReference> *list = NULL;
         if (trace())
            traceMsg(comp(), "slot %d is live with symref %d takesTwoSlots %d\n", slot, symRefNum, takesTwoSlots);

         if (slot < 0)
            {
            deadPPSSlots->reset(-slot-1);
            if (takesTwoSlots)
               deadPPSSlots->reset(-slot);
            }
         else
            {
            deadAutoSlots->reset(slot);
            if (takesTwoSlots)
               deadAutoSlots->reset(slot+1);
            }
         }
      }

   intersectWithExistingDeadSlots(osrPoint, deadPPSSlots, deadAutoSlots, containsPendingPushes);

   if (deadPPSSlots && trace())
      {
      TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
      traceMsg(comp(), "deadppslots at node %p %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
      deadPPSSlots->print(comp());
      traceMsg(comp(), "\n");
      }

   if (deadPPSSlots && !deadPPSSlots->isEmpty())
      {
      TR_BitVectorIterator bvi(*deadPPSSlots);
      while (bvi.hasMoreElements())
         {
         TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
         int32_t slot =  -bvi.getNextElement()-1;
         if (trace())
            traceMsg(comp(), "ppslot %d is dead at %d:%d\n", slot, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
         //symRefNum = -1 and symRefOrder = -1 indicate those slots should be zeroed in prepareForOSR
         comp()->getOSRCompilationData()->addSlotSharingInfo(osrPoint->getByteCodeInfo(),
            slot, -1 /* symRefNum */, -1 /* symRefOrder */, TR::Compiler->om.sizeofReferenceAddress() /*symSize*/ , false /*takesTwoSlots*/);
         }
      }

   if (deadAutoSlots && trace())
      {
      TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();
      traceMsg(comp(), "deadAutoSlots at node %p %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());
      deadAutoSlots->print(comp());
      traceMsg(comp(), "\n");
      }

   if (deadAutoSlots && !deadAutoSlots->isEmpty())
      {
      TR_BitVectorIterator bvi(*deadAutoSlots);
      while (bvi.hasMoreElements())
         {
         int32_t slot =  bvi.getNextElement();
         //symRefNum = -1 and symRefOrder = -1 indicate those slots should be zeroed in prepareForOSR
         comp()->getOSRCompilationData()->addSlotSharingInfo(osrPoint->getByteCodeInfo(),
            slot, -1 /* symRefNum */, -1 /* symRefOrder */, TR::Compiler->om.sizeofReferenceAddress() /*symSize*/ , false /*takesTwoSlots*/);
         }
      }

   comp()->getOSRCompilationData()->ensureSlotSharingInfoAt(osrPoint->getByteCodeInfo());
   }

int32_t TR_OSRLiveRangeAnalysis::fullAnalysis(bool includeParms, bool containsPendingPushes)
   {
   TR_Structure* rootStructure = TR_RegionAnalysis::getRegions(comp(), optimizer()->getMethodSymbol());
   optimizer()->getMethodSymbol()->getFlowGraph()->setStructure(rootStructure);
   TR_ASSERT(rootStructure, "Structure is NULL");

   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "Starting full OSRLiveRangeAnalysis\n");
      comp()->dumpMethodTrees("Before full OSRLiveRangeAnalysis", optimizer()->getMethodSymbol());
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Perform liveness analysis
   //
   bool ignoreOSRuses = true;
   bool includeParms = true;
   TR_Liveness liveLocals(comp(), optimizer(), comp()->getFlowGraph()->getStructure(), ignoreOSRuses, NULL, false, includeParms);
   _liveVars = new (trStackMemory()) TR_BitVector(liveLocals.getNumberOfBits(), trMemory(), stackAlloc);

   //set the structure to NULL so that the inliner (which is applied very soon after) doesn't need
   //update it.
   optimizer()->getMethodSymbol()->getFlowGraph()->invalidateStructure();

   TR::SymbolReferenceTable *symRefTab   = comp()->getSymRefTab();

   TR_Array<List<TR::SymbolReference> > *autoSymRefs = comp()->getMethodSymbol()->getAutoSymRefs();
   TR_Array<List<TR::SymbolReference> > *pendingPushSymRefs = comp()->getMethodSymbol()->getPendingPushSymRefs();

   TR::TreeTop *firstTree = comp()->getMethodSymbol()->getFirstTreeTop();
   TR::Node *firstNode = firstTree->getNode();
   TR::TreeTop *next = firstTree->getNextTreeTop();
   int32_t numBits = liveLocals.getNumberOfBits();

   if (numBits == 0)
      {
      return 0;
      }

   int32_t numSlots = 0;
   if (autoSymRefs)
      numSlots = autoSymRefs->size();

   int32_t numPendingSlots = 0;
   if (pendingPushSymRefs)
      numPendingSlots = pendingPushSymRefs->size();

   int32_t *liveLocalIndexToSymRefNumberMap = (int32_t *) trMemory()->allocateStackMemory(numBits*sizeof(int32_t));
   memset(liveLocalIndexToSymRefNumberMap, 0xFF, numBits*sizeof(int32_t));

   int32_t i;
   for (i=0;i<numSlots;i++)
      {
      List<TR::SymbolReference> &symRefsAtThisSlot = (*autoSymRefs)[i];

      if (symRefsAtThisSlot.isEmpty()) continue;

      ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
      TR::SymbolReference *nextSymRef;
      for (nextSymRef = symRefsIt.getCurrent(); nextSymRef; nextSymRef=symRefsIt.getNext())
         {
         uint16_t liveLocalIndex;
         if (nextSymRef->getSymbol()->isAuto() &&
             !nextSymRef->getSymbol()->castToAutoSymbol()->isLiveLocalIndexUninitialized())
            liveLocalIndex = nextSymRef->getSymbol()->castToAutoSymbol()->getLiveLocalIndex();
         else if (nextSymRef->getSymbol()->isParm() &&
             !nextSymRef->getSymbol()->castToParmSymbol()->isLiveLocalIndexUninitialized())
            liveLocalIndex = nextSymRef->getSymbol()->castToParmSymbol()->getLiveLocalIndex();
         else
            {
            continue;
            }

         if (!nextSymRef->getSymbol()->holdsMonitoredObject())
            {
            liveLocalIndexToSymRefNumberMap[liveLocalIndex] = nextSymRef->getReferenceNumber();
            nextSymRef->getSymbol()->setLocalIndex(0);
            }
         }
      }

   if (containsPendingPushes)
      {
      for (i=0;i<numPendingSlots;i++)
         {
         List<TR::SymbolReference> &symRefsAtThisSlot = (*pendingPushSymRefs)[i];

         if (symRefsAtThisSlot.isEmpty()) continue;

         ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
         TR::SymbolReference *nextSymRef;
         for (nextSymRef = symRefsIt.getCurrent(); nextSymRef; nextSymRef=symRefsIt.getNext())
            {
            if (nextSymRef->getSymbol()->isAuto() && !nextSymRef->getSymbol()->castToAutoSymbol()->isLiveLocalIndexUninitialized())
               {
               liveLocalIndexToSymRefNumberMap[nextSymRef->getSymbol()->castToAutoSymbol()->getLiveLocalIndex()] = nextSymRef->getReferenceNumber();
               nextSymRef->getSymbol()->setLocalIndex(0);
               }
            }
         }
      }

   TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findOrCreateOSRMethodData(comp()->getCurrentInlinedSiteIndex(), comp()->getMethodSymbol());

   TR::Block *block = NULL;
   int32_t blockNum = -1;
   TR_BitVector *liveVars = NULL;
   TR::TreeTop *tt;

   vcount_t visitCount = comp()->incVisitCount();
   block = comp()->getStartBlock();
   _visitedBCI->empty();
   int32_t callerIndex = comp()->getCurrentInlinedSiteIndex();

   while (block)
      {
      blockNum    = block->getNumber();
      TR::TreeTop *firstTT = block->getEntry();
      for (tt = block->getExit(); tt != firstTT; tt = tt->getPrevTreeTop())
         {
         if (tt->getNode()->getOpCodeValue() == TR::BBEnd)
            {
            // Compose the live-on-exit vector from the union of the live-on-entry
            // vectors of this block's successors.
            //
            _liveVars->empty();
            TR::Block *succ;
            auto edge = block->getSuccessors().begin();
            while (edge != block->getSuccessors().end())
               {
               succ = toBlock((*edge)->getTo());
               *_liveVars |= *liveLocals._blockAnalysisInfo[succ->getNumber()];
               ++edge;
               }

            // If the block contains an OSR point, it should have an exception edge
            // to an OSR catch block
            edge = block->getExceptionSuccessors().begin();
            while (edge != block->getExceptionSuccessors().end())
               {
               if (toBlock((*edge)->getTo())->isOSRCatchBlock())
                  break;
               ++edge;
               }
            if (edge == block->getExceptionSuccessors().end())
               {
               if (comp()->getOption(TR_TraceOSR))
                  traceMsg(comp(), "skipping block_%d without OSR points", block->getNumber());
               break;
               }

            if (comp()->getOption(TR_TraceOSR))
               {
               traceMsg(comp(), "BB_End for block_%d: live vars = ", block->getNumber());
               _liveVars->print(comp());
               traceMsg(comp(), "\n");
               }
            }

         TR_ByteCodeInfo &bci = comp()->getMethodSymbol()->getOSRByteCodeInfo(tt->getNode());

         // Check if this treetop is an OSR pending push store or load. If so, it will shift the
         // cursor to before the PPs. The cursor will either point at the first PP or the treetop
         // before it, depending on whether it could be the OSR point for these PP.
         TR::TreeTop *offsetOSRTreeTop = NULL;
         if (comp()->isOSRTransitionTarget(TR::postExecutionOSR) &&
             comp()->getMethodSymbol()->isOSRRelatedNode(tt->getNode()))
            {
            offsetOSRTreeTop = tt->getNextTreeTop();
            tt = collectPendingPush(bci, tt, _liveVars);
            TR_ByteCodeInfo &osrBCI = comp()->getMethodSymbol()->getOSRByteCodeInfo(tt->getNode());

            // If it is the start of this block or has a different bytecode index, it cannot be
            // the OSR point
            if (tt == firstTT || osrBCI.getCallerIndex() != bci.getCallerIndex()
                || osrBCI.getByteCodeIndex() != bci.getByteCodeIndex())
               tt = tt->getNextTreeTop();
            offsetOSRTreeTop = offsetOSRTreeTop->getPrevTreeTop();
            }

         TR::Node *node = tt->getNode();
         bool isPotentialOSRPoint = comp()->isPotentialOSRPointWithSupport(tt);

         // If the transition point is after the OSR point, build the OSR live info before
         // any PPS are processed.
         if (isPotentialOSRPoint && comp()->isOSRTransitionTarget(TR::postExecutionOSR))
            {
            TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
            bcInfo.setByteCodeIndex(bcInfo.getByteCodeIndex() + comp()->getOSRInductionOffset(node));
            TR_OSRPoint *offsetOSRPoint = comp()->getMethodSymbol()->findOSRPoint(bcInfo);
            TR_ASSERT(offsetOSRPoint != NULL, "Cannot find a post OSR point for node %p", node);

            buildOSRLiveRangeInfo(node, _liveVars, offsetOSRPoint, liveLocalIndexToSymRefNumberMap,
               numBits, osrMethodData, containsPendingPushes);
            if (!_sharedSymRefs->isEmpty())
               buildOSRSlotSharingInfo(node, _liveVars, offsetOSRPoint, liveLocalIndexToSymRefNumberMap,
                  _sharedSymRefs);
            }

         // Maintain liveness across post pending pushes and the OSR point itself
         while (offsetOSRTreeTop && offsetOSRTreeTop != tt)
            {
            maintainLiveness(offsetOSRTreeTop->getNode(), NULL, -1, visitCount, &liveLocals, _liveVars, block);
            offsetOSRTreeTop = offsetOSRTreeTop->getPrevTreeTop();
            }
         maintainLiveness(node, NULL, -1, visitCount, &liveLocals, _liveVars, block);

         // If the transition point is before the OSR point, build the OSR live info after
         // the post PPS and the OSR point are processed.
         if ((isPotentialOSRPoint ||
             //maintainLiveness only kicks in for the first evaluation point.
             //In involuntaryOSR mode, there are cases where the first evaluation point of a node is under a treetop
             //that is not an OSRPoint but another treetop with the same bci is an OSRPoint. Need to call buildOSRLiveRangeInfo
             //in this case for the first evaluation point to make sure the liveness info is bookkept correctly for this bci
             (_visitedBCI->isSet(node->getByteCodeInfo().getByteCodeIndex()) && comp()->getOSRMode() == TR::involuntaryOSR))
            && (comp()->isOSRTransitionTarget(TR::preExecutionOSR) || comp()->requiresAnalysisOSRPoint(node)))
            {
            TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
            TR_ASSERT_FATAL(bcInfo.getCallerIndex() == callerIndex, "Found node with invalid caller index");
            TR_OSRPoint *osrPoint = comp()->getMethodSymbol()->findOSRPoint(bcInfo);
            TR_ASSERT_FATAL(osrPoint != NULL, "Cannot find a pre OSR point for node %p", node);

            buildOSRLiveRangeInfo(node, _liveVars, osrPoint, liveLocalIndexToSymRefNumberMap,
               numBits, osrMethodData, containsPendingPushes);
            if (!_sharedSymRefs->isEmpty())
               buildOSRSlotSharingInfo(node, _liveVars, osrPoint, liveLocalIndexToSymRefNumberMap,
                  _sharedSymRefs);
            if (comp()->getOption(TR_FullSpeedDebug))
               buildDeadSlotsInfo(node, _liveVars, osrPoint, liveLocalIndexToSymRefNumberMap, containsPendingPushes);
            }

         if (isPotentialOSRPoint)
            _visitedBCI->set(node->getByteCodeInfo().getByteCodeIndex());
         }

      // Store liveness data for the method entry if it is an implicit OSR point
      //
      if (comp()->getStartTree()->getNode()->getBlock() == block && comp()->isOutermostMethod() && comp()->getHCRMode() == TR::osr)
         {
         TR_ByteCodeInfo bci;
         bci.setCallerIndex(-1);
         bci.setByteCodeIndex(0);
         TR_OSRPoint *osrPoint = comp()->getMethodSymbol()->findOSRPoint(bci);
         TR_ASSERT(osrPoint != NULL, "Cannot find a OSR point for method entry");

         buildOSRLiveRangeInfo(comp()->getStartTree()->getNode(), _liveVars, osrPoint, liveLocalIndexToSymRefNumberMap,
            numBits, osrMethodData, containsPendingPushes);
         if (!_sharedSymRefs->isEmpty())
            buildOSRSlotSharingInfo(comp()->getStartTree()->getNode(), _liveVars, osrPoint, liveLocalIndexToSymRefNumberMap,
               _sharedSymRefs);
         }

      block = block->getNextBlock();
      }

   } // scope for stack memory region

   return 0;
   }

//
// To reduce the impact of pending push temps in postExecutionOSR, the live pending push symrefs can be stored or
// referenced in anchored loads, between the OSR point and the transition. In the event that the anchored loads
// are not commoned, they will be removed.
//
TR::TreeTop *TR_OSRLiveRangeAnalysis::collectPendingPush(TR_ByteCodeInfo bci, TR::TreeTop *tt, TR_BitVector *liveVars)
   {
   while (comp()->getMethodSymbol()->isOSRRelatedNode(tt->getNode(), bci))
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isStoreDirect())
         {
         TR::AutomaticSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
         int32_t localIndex = local->getLiveLocalIndex();
         _liveVars->set(localIndex);
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "+++ local index %d OSR PENDING PUSH STORE LIVE\n", localIndex);
         }
      else if (node->getOpCodeValue() == TR::treetop
          && node->getFirstChild()->getOpCode().isLoad()
          && node->getFirstChild()->getOpCode().hasSymbolReference())
         {
         if (node->getFirstChild()->getReferenceCount() == 1)
            {
            TR::AutomaticSymbol *local = node->getFirstChild()->getSymbolReference()->getSymbol()->getAutoSymbol();
            int32_t localIndex = local->getLiveLocalIndex();
            _liveVars->set(localIndex);
            if (comp()->getOption(TR_TraceOSR))
               traceMsg(comp(), "+++ local index %d OSR PENDING PUSH LOAD LIVE\n", localIndex);
            TR::TransformUtil::removeTree(comp(), tt);
            }
         }
      else
         {
         TR_ASSERT(0, "Unexpected OSR related node %p found, should be either pending push store or anchored load", node);
         }
      tt = tt->getPrevTreeTop();
      }

   return tt;
   }

void TR_OSRLiveRangeAnalysis::maintainLiveness(TR::Node *node,
                                               TR::Node *parent,
                                               int32_t childNum,
                                         vcount_t  visitCount,
                                         TR_Liveness *liveLocals,
                                         TR_BitVector *liveVars,
                                         TR::Block    *block)
   {
   // First time this node has been encountered.
   //
   if (node->getVisitCount() != visitCount)
      {
      node->setVisitCount(visitCount);
      node->setLocalIndex(node->getReferenceCount());
      }

   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "---> visiting node %p\n", node);
      }

   if (node->getOpCode().isStoreDirect())
      {
      TR::RegisterMappedSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
      if (!local)
         local = node->getSymbolReference()->getSymbol()->getParmSymbol();
      if (local && !local->isLiveLocalIndexUninitialized())
         {
         int32_t localIndex = local->getLiveLocalIndex();
         TR_ASSERT(localIndex >= 0, "bad local index: %d\n", localIndex);

         // This local is killed only if the live range of any loads of this symbol do not overlap
         // with this store.
         //
         if (local->getLocalIndex() == 0)
            {
            liveVars->reset(localIndex);
            if (comp()->getOption(TR_TraceOSR))
               {
               traceMsg(comp(), "--- local index %d KILLED\n", localIndex);
               }
            }
         }
      }
   else if (node->getOpCode().isLoadVarDirect() || node->getOpCodeValue() == TR::loadaddr)
      {
      TR::RegisterMappedSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
      if (!local)
         local = node->getSymbolReference()->getSymbol()->getParmSymbol();

      if (local && !local->isLiveLocalIndexUninitialized())
         {
         int32_t localIndex = local->getLiveLocalIndex();

         TR_ASSERT(localIndex >= 0, "bad local index: %d\n", localIndex);

         // First visit to this node.
         //
         if (node->getLocalIndex() == node->getReferenceCount())
            {
            local->setLocalIndex(local->getLocalIndex() + node->getReferenceCount());
            }

         static const char *disallowOSRPPS3 = feGetEnv("TR_DisallowOSRPPS3");

         if ((!disallowOSRPPS3 || !_pendingPushSymRefs->get(node->getSymbolReference()->getReferenceNumber())) &&
             ((node->getLocalIndex() == 1 || node->getOpCodeValue() == TR::loadaddr) && !liveVars->isSet(localIndex)))
            {
            // First evaluation point of this node or loadaddr.
            //
            liveVars->set(localIndex);

            if (comp()->getOption(TR_TraceOSR))
               {
               traceMsg(comp(), "+++ local index %d LIVE\n", localIndex);
               }
            }

         local->setLocalIndex(local->getLocalIndex()-1);
         node->setLocalIndex(node->getLocalIndex()-1);
         return;
         }
      }
   else if (node->exceptionsRaised() &&
           (node->getLocalIndex() <= 1))
      {
      TR::Block *succ;
      for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
         {
         succ = toBlock((*edge)->getTo());
         *liveVars |= *((*liveLocals)._blockAnalysisInfo[succ->getNumber()]);
         }
      }

   if (node->getLocalIndex() != 0)
      {
      node->setLocalIndex(node->getLocalIndex()-1);
      }

   // This is not the first evaluation point of this node.
   //
   if (node->getLocalIndex() > 0)
      return;

   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      maintainLiveness(node->getChild(i), node, i, visitCount, liveLocals, liveVars, block);
   }

/**
 *  Based on the BitVector of currently live symbol references, calculate which of the shared slots
 *  should be used for a transition at this OSR point.
 */
void TR_OSRLiveRangeAnalysis::buildOSRSlotSharingInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
   int32_t *liveLocalIndexToSymRefNumberMap, TR_BitVector *sharedSymRefs)
   {
   TR_ByteCodeInfo& bcInfo = osrPoint->getByteCodeInfo();

   if (!liveVars->isEmpty())
      {
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

      if (trace())
         traceMsg(comp(), "Shared slots at OSR point [%p] at %d:%d\n", node, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex());

      TR_BitVectorIterator bvi(*liveVars);
      while (bvi.hasMoreElements())
         {
         int32_t index = bvi.getNextElement();
         int32_t symRefNum = liveLocalIndexToSymRefNumberMap[index];
         if (symRefNum < 0 || !sharedSymRefs->get(symRefNum))
            continue;

         TR::SymbolReference *symRef = symRefTab->getSymRef(symRefNum);
         int32_t slot = symRef->getCPIndex();
         TR::DataType dt = symRef->getSymbol()->getDataType();
         bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;

         List<TR::SymbolReference> *list = NULL;
         if (slot < 0)
            list = &comp()->getMethodSymbol()->getPendingPushSymRefs()->element(-slot-1);
         else
            list = &comp()->getMethodSymbol()->getAutoSymRefs()->element(slot);

         ListIterator<TR::SymbolReference> listIt(list);
         int symRefOrder = 0;
         for (TR::SymbolReference* slotSymRef = listIt.getFirst(); slotSymRef; slotSymRef = listIt.getNext(), symRefOrder++)
            if (symRef == slotSymRef)
               break;
         TR_ASSERT(symRefOrder < list->getSize(), "symref #%d not found in list of shared slots\n", symRef->getReferenceNumber());

         if (trace())
            traceMsg(comp(), "  Slot:%d SymRef:%d TwoSlots:%d\n", slot, symRefNum, takesTwoSlots);

         comp()->getOSRCompilationData()->addSlotSharingInfo(osrPoint->getByteCodeInfo(),
            slot, symRefNum, symRefOrder, symRef->getSymbol()->getSize(), takesTwoSlots);
         }
      }

   comp()->getOSRCompilationData()->ensureSlotSharingInfoAt(bcInfo);
   }

void TR_OSRLiveRangeAnalysis::buildOSRLiveRangeInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
   int32_t *liveLocalIndexToSymRefNumberMap, int32_t numBits, TR_OSRMethodData *osrMethodData, bool containsPendingPushes)
   {
   int32_t byteCodeIndex = osrPoint->getByteCodeInfo().getByteCodeIndex();
   bool newlyAllocated = false;

   _workBitVector->empty();
   _workBitVector->setAll(numBits);
   *_workBitVector -= *liveVars;


   _workDeadSymRefs->empty();
   TR_BitVector *deadSymRefs = NULL;

   if (!_workBitVector->isEmpty())
      {
      TR_BitVectorIterator bvi(*_workBitVector);
      while (bvi.hasMoreElements())
         {
         int32_t nextDeadVar = bvi.getNextElement();
         if (liveLocalIndexToSymRefNumberMap[nextDeadVar] >= 0)
            _workDeadSymRefs->set(liveLocalIndexToSymRefNumberMap[nextDeadVar]);
         }

      // Load the existing BitVector
      deadSymRefs = osrMethodData->getLiveRangeInfo(byteCodeIndex);

      //There are cases where one bci has several OSRPoints with different liveness.
      //A symbol is only dead if it's dead at all the OSRPoints
      if (_visitedBCI->isSet(byteCodeIndex))
         {
         if (!containsPendingPushes)
            *_workDeadSymRefs |= *_pendingPushSymRefs;
         if (deadSymRefs)
            *deadSymRefs &= *_workDeadSymRefs;
         }
      else
         {
         if (!deadSymRefs && !_workDeadSymRefs->isEmpty())
            {
            deadSymRefs = new (trHeapMemory()) TR_BitVector(0, trMemory(), heapAlloc);
            newlyAllocated = true;
            }

         if (deadSymRefs)
            *deadSymRefs |= *_workDeadSymRefs;
         }

      if (newlyAllocated && deadSymRefs && !deadSymRefs->isEmpty())
         osrMethodData->addLiveRangeInfo(byteCodeIndex, deadSymRefs);
      }

   osrMethodData->setNumSymRefs(numBits);

   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "Dead variables at OSR point %p of %p bytecode offset %d\n", node, osrMethodData, osrPoint->getByteCodeInfo().getByteCodeIndex());
      if (deadSymRefs)
         deadSymRefs->print(comp());
      else
         traceMsg(comp(), " NULL");
      traceMsg(comp(), "\n");
      }
   }

const char *
TR_OSRLiveRangeAnalysis::optDetailString() const throw()
   {
   return "O^O OSR LIVE RANGE ANALYSIS: ";
   }

/**
 * Collect the dead stores in the provided block. Either initialize the provided bitvector with
 * the dead stores symrefs that were found or take the intersection of the two, filtering down to
 * symrefs that are always dead.
 */
bool TR_OSRExceptionEdgeRemoval::addDeadStores(TR::Block* osrBlock, TR_BitVector& dead, bool inited)
   {
   TR_ASSERT(osrBlock->isOSRCodeBlock() || osrBlock->isOSRInduceBlock(), "It is only safe to identify dead stores in OSRCodeBlocks and OSRInduceBlocks");

   _seenDeadStores->empty();
   for (TR::TreeTop *tt = osrBlock->getFirstRealTreeTop(); tt != osrBlock->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isStoreDirect() && node->getSymbol()->isAutoOrParm() && node->storedValueIsIrrelevant())
         _seenDeadStores->set(node->getSymbolReference()->getReferenceNumber());
      }

   if (inited)
      dead &= *_seenDeadStores;
   else
      dead |= *_seenDeadStores;

   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "Identified dead stores for block_%d:\n", osrBlock->getNumber());
      _seenDeadStores->print(comp());
      traceMsg(comp(), "\nRemaining dead stores:\n");
      dead.print(comp());
      traceMsg(comp(), "\n");
      }

   return !_seenDeadStores->isEmpty();
   }

/**
 * If loads of a symbol along a OSR paths are zeroed, the dead stores can be removed
 * as well. Given an OSRCodeBlock or an OSRInduceBlock and a bitvector of dead symbol
 * references, this function will remove them.
 */
void TR_OSRExceptionEdgeRemoval::removeDeadStores(TR::Block* osrBlock, TR_BitVector& dead)
   {
   TR_ASSERT(osrBlock->isOSRCodeBlock() || osrBlock->isOSRInduceBlock(), "It is only safe to remove dead stores from OSRCodeBlocks and OSRInduceBlocks");

   for (TR::TreeTop *tt = osrBlock->getFirstRealTreeTop(); tt != osrBlock->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isStoreDirect() && node->getSymbol()->isAutoOrParm() && node->storedValueIsIrrelevant() &&
          dead.get(node->getSymbolReference()->getReferenceNumber()))
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "Removing dead store n%dn of symref #%d\n", node->getGlobalIndex(), node->getSymbolReference()->getReferenceNumber());
         TR::TransformUtil::removeTree(comp(), tt);
         }
      }
   }

int32_t TR_OSRExceptionEdgeRemoval::perform()
   {
   if (comp()->getOption(TR_EnableOSR))
      {
      //if (comp()->getOption(TR_DisableOSRExceptionEdgeRemoval)) // add to Options later
      if (false)
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "OSR is enabled but OSR exception edge removal is not.\n");
         return 0;
         }
      }
   else
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "OSR Option not enabled -- returning from OSR exception edge removal analysis.\n");
      return 0;
      }

   if (comp()->getOSRMode() == TR::involuntaryOSR)
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "Involuntary OSR enabled -- returning from OSR exception edge removal analysis since we implement involuntary OSR with exception edges.\n");
      return 0;
      }

   if (comp()->osrInfrastructureRemoved())
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "OSR infrastructure removed -- returning from OSR exception edge removal analysis since we have already removed OSR infrastructure.\n");
      return 0;
      }

   if (comp()->isPeekingMethod())
      {
      if (comp()->getOption(TR_TraceOSR))
         {
            traceMsg(comp(), "%s OSR exception edge removal is not required because we are peeking\n",
               optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         traceMsg(comp(), "Returning...\n");
         }
      return 0;
      }
   else
      {
      if (comp()->getOption(TR_TraceOSR))
         {
         traceMsg(comp(), "%s OSR exception edge analysis can be done\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         }
      }

   TR::CFG *cfg = comp()->getFlowGraph();

   // Remove edges to OSR catch blocks that are no longer necessary
   TR::CFGNode *cfgNode;
   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      TR::Block *block = toBlock(cfgNode);
      TR_ASSERT(block->verifyOSRInduceBlock(comp()), "osr induce calls can only exist in osr induce blocks");
      if (cfgNode->getExceptionSuccessors().empty() || block->isOSRInduceBlock())
         continue;

      for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end();)
         {
         TR::Block *catchBlock = toBlock((*edge++)->getTo());
         if (catchBlock->isOSRCatchBlock()
             && performTransformation(comp(), "%s: Remove redundant exception edge from block_%d at [%p] to OSR catch block_%d at [%p]\n", OPT_DETAILS, block->getNumber(), block, catchBlock->getNumber(), catchBlock))
            cfg->removeEdge(block, catchBlock);
         }
      }

   // Under voluntary OSR, it is common for all paths to an OSR code block to contain
   // a dead store for some of the prepareForOSR loads. As this is a cold path and
   // OSR infrastructure is commonly ignored, the optimizer won't eliminate these loads.
   //
   // As this is cheap to do and will reduce the overhead of analysis like UseDef and
   // CompactLocals, identify the loads that have dead stores on all paths
   // and zero them.
   //
   static const char *disableDeadStoreCleanup = feGetEnv("TR_DisableDeadStoreCleanup");
   if (!disableDeadStoreCleanup)
      {
      // A stack of blocks is used to stash blocks that will be revisited for cleanup
      TR_Stack<TR::Block *> osrBlocks(trMemory(), 8, false, stackAlloc);

      TR_BitVector alwaysDead(comp()->trMemory()->currentStackRegion());
      _seenDeadStores = new (trStackMemory()) TR_BitVector(comp()->trMemory()->currentStackRegion());

      // OSR induce and code blocks will contain dead stores for the next
      // frame.
      for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
         {
         TR::Block *block = toBlock(cfgNode);
         if (!block->isOSRCodeBlock())
            continue;

         // Find the prepareForOSR call
         TR::TreeTop *tt = block->getFirstRealTreeTop();
         while (tt &&
             !((tt->getNode()->getOpCode().isCheck() || tt->getNode()->getOpCodeValue() == TR::treetop) &&
             tt->getNode()->getFirstChild()->getOpCode().isCall() &&
             tt->getNode()->getFirstChild()->getSymbolReference()->getReferenceNumber() == TR_prepareForOSR))
            {
            tt = tt->getNextTreeTop();
            }
         TR_ASSERT(tt, "OSRCodeBlocks must contain a call to prepareForOSR and no other calls");
         TR::Node *prepareForOSR = tt->getNode()->getFirstChild();

         // Collect symrefs that are always dead
         alwaysDead.empty();
         bool firstPass = true;
         for (auto edge = block->getPredecessors().begin(); edge != block->getPredecessors().end(); ++edge)
            {
            TR::Block *osrBlock = toBlock((*edge)->getFrom());
            if (osrBlock->isOSRCodeBlock() && addDeadStores(osrBlock, alwaysDead, !firstPass))
               osrBlocks.push(osrBlock);
            else if (osrBlock->isOSRCatchBlock())
               {
               for (auto exEdge = osrBlock->getExceptionPredecessors().begin(); exEdge != osrBlock->getExceptionPredecessors().end(); ++exEdge)
                  {
                  TR::Block *exBlock = toBlock((*exEdge)->getFrom());
                  TR_ASSERT(exBlock && exBlock->isOSRInduceBlock(), "Predecessors to an OSR catch block should be OSR induce blocks");
                  if (addDeadStores(exBlock, alwaysDead, !firstPass))
                     osrBlocks.push(exBlock);
                  firstPass = false;
                  }
               }
            else
               {
               // Another block was found in the OSR infrastructure, give up
               alwaysDead.empty();
               break;
               }
            firstPass = false;
            }

         if (alwaysDead.isEmpty())
            continue;

         // Remove loads that are always dead
         for (int32_t i = 0 ; i < prepareForOSR->getNumChildren(); ++i)
            {
            TR::Node *child = prepareForOSR->getChild(i);
            if (child->getOpCode().isLoadVar() && alwaysDead.get(child->getSymbolReference()->getReferenceNumber()))
               {
               if (performTransformation(comp(), "%s: Remove dead OSR load [%p] from OSR code block_%d\n", OPT_DETAILS, child, block->getNumber()))
                  {
                  TR::Node::recreate(prepareForOSR->getChild(i), comp()->il.opCodeForConst(prepareForOSR->getChild(i)->getDataType()));
                  prepareForOSR->getChild(i)->setConstValue(0);
                  }
               else
                  {
                  alwaysDead.reset(child->getSymbolReference()->getReferenceNumber());
                  }
               }
            }

         // Remove the original dead stores
         while (!osrBlocks.isEmpty())
            removeDeadStores(osrBlocks.pop(), alwaysDead);
         }
      }

   comp()->setOSRInfrastructureRemoved(true);

   return 1;
   }

const char *
TR_OSRExceptionEdgeRemoval::optDetailString() const throw()
   {
   return "O^O OSR EXCEPTION EDGE REMOVAL: ";
   }
