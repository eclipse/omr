/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "optimizer/OSRDefAnalysis.hpp"

#include <stdint.h>                                   // for int32_t, etc
#include <string.h>                                   // for NULL, memset
#include "codegen/FrontEnd.hpp"                       // for feGetEnv, etc
#include "compile/Compilation.hpp"                    // for Compilation
#include "compile/OSRData.hpp"                        // for TR_OSRPoint, etc
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/arrayof.h"                              // for ArrayOf
#include "cs2/bitvectr.h"                             // for ABitVector, etc
#include "env/TRMemory.hpp"                           // for Allocator, etc
#include "env/jittypes.h"
#include "il/Block.hpp"                               // for Block
#include "il/DataTypes.hpp"                           // for DataTypes, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                               // for ILOpCode
#include "il/Node.hpp"                                // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                              // for Symbol
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                             // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"                            // for TR_Array
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "infra/BitVector.hpp"                        // for TR_BitVector, etc
#include "infra/Cfg.hpp"                              // for CFG
#include "infra/List.hpp"                             // for List, etc
#include "infra/TRCfgEdge.hpp"                        // for CFGEdge
#include "infra/TRCfgNode.hpp"                        // for CFGNode
#include "optimizer/Optimization.hpp"                 // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizer.hpp"                    // for Optimizer
#include "optimizer/DataFlowAnalysis.hpp"             // for TR_Liveness, etc
#include "optimizer/StructuralAnalysis.hpp"
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
   comp()->printMemStatsAfter("computeOSRDefInfo");

   // Iterate through OSR reaching definitions bit vectors and save it in method symbol's data structure.
   TR::SymbolReferenceTable *symRefTab   = comp()->getSymRefTab();
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   for (intptrj_t i = 0; i < methodSymbol->getOSRPoints().size(); ++i)
      {
      TR_OSRPoint *point = (methodSymbol->getOSRPoints())[i];
      if (point == NULL) continue;
      uint32_t osrIndex = point->getOSRIndex();
      TR_ASSERT(osrIndex == i, "something doesn't make sense\n");
      BitVector &info = aux._defsForOSR[osrIndex];
      //one reason that info can be NULL is that the block that contains that i-th osr point has
      //been deleted (e.g., because it was unreachable), and therefore, we have never set _defsForOSR[i] to
      //a non-NULL value
      if (!info.IsNull())
         {
         BitVector::Cursor cursor(info);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t j = cursor;
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
   TR_UseDefInfo::BitVector prevUnionTwoSlotDef(comp()->allocator());
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
         TR_UseDefInfo::BitVector unionDef(comp()->allocator());
         unionDef.GrowTo(getBitVectorSize());
         TR_UseDefInfo::BitVector twoSlotUnionDef(comp()->allocator());
         twoSlotUnionDef.GrowTo(getBitVectorSize());

         for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            const TR_UseDefInfo::BitVector &defs = aux._defsForSymbol[symIndex];
            unionDef |= defs;
            TR::DataType dt = symRef->getSymbol()->getDataType();
            bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
            if (takesTwoSlots)
               twoSlotUnionDef |= defs;
            }

         unionDef |= prevUnionTwoSlotDef;

         for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            // is assignment okay here, before it was reference only?
            aux._defsForSymbol[symIndex] = unionDef;

            if (!prevUnionTwoSlotDef.IsZero())
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
                     aux._defsForSymbol[prevSymIndex] |= unionDef;
                     }
                  }
               }
            }

         // shallow swap bit vectors, causing the contents of prevUnionTwoSlotDef prior to the swap
         // to be discarded, and its new contents from twoSlotUnionDef to be preserved
         CS2::Swap(prevUnionTwoSlotDef, twoSlotUnionDef);
         }
      else
         prevUnionTwoSlotDef.Clear();

      isTwoSlotSymRefAtPrevSlot = isTwoSlotSymRefAtThisSlot;
      }


   TR_Array<List<TR::SymbolReference> > *autosListArray = _methodSymbol->getAutoSymRefs();
   prevUnionTwoSlotDef.Clear();
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
         TR_UseDefInfo::BitVector unionDef(comp()->allocator());
         unionDef.GrowTo(getBitVectorSize());
         TR_UseDefInfo::BitVector twoSlotUnionDef(comp()->allocator());
         twoSlotUnionDef.GrowTo(getBitVectorSize());
         for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            const TR_UseDefInfo::BitVector &defs = aux._defsForSymbol[symIndex];
            unionDef |= defs;
            TR::DataType dt = symRef->getSymbol()->getDataType();
            bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
            if (takesTwoSlots)
               twoSlotUnionDef |= defs;
            }

         unionDef |= prevUnionTwoSlotDef;

         for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
            {
            uint16_t symIndex = symRef->getSymbol()->getLocalIndex();
            // is assignment okay here, before it was reference only?
            aux._defsForSymbol[symIndex] = unionDef;

            if (!prevUnionTwoSlotDef.IsZero())
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
                     aux._defsForSymbol[prevSymIndex] |= unionDef;
                     }
                  }
               }
            }

         // shallow swap bit vectors, causing the contents of prevUnionTwoSlotDef prior to the swap
         // to be discarded, and its new contents from twoSlotUnionDef to be preserved
         CS2::Swap(prevUnionTwoSlotDef, twoSlotUnionDef);
         }
      else
         prevUnionTwoSlotDef.Clear();

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
   aux._defsForOSR.GrowTo(numOSRPoints);

   TR::Block *block;
   TR::TreeTop *treeTop;
   TR_ReachingDefinitions::ContainerType **blockInfo = (TR_ReachingDefinitions::ContainerType**)vblockInfo;
   TR_ReachingDefinitions::ContainerType *analysisInfo = NULL;
   TR_OSRPoint *nextOsrPoint = NULL;

   comp()->incVisitCount();

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
      if (isPotentialOSRPoint && (comp()->getOSRTransitionTarget() == TR::preExecutionOSR
          || comp()->requiresAnalysisOSRPoint(node)))
         {
         osrPoint = _methodSymbol->findOSRPoint(node->getByteCodeInfo());
         TR_ASSERT(osrPoint != NULL, "Cannot find an OSR point for node %p", node);
         }
 
      buildOSRDefs(node, analysisInfo, osrPoint, nextOsrPoint, NULL, aux);
      nextOsrPoint = NULL;
 
      if (isPotentialOSRPoint && comp()->getOSRTransitionTarget() == TR::postExecutionOSR)
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
         node = treeTop->getNode();
         bci.setByteCodeIndex(bci.getByteCodeIndex() + comp()->getOSRInductionOffset(node));
         nextOsrPoint = _methodSymbol->findOSRPoint(bci);
         TR_ASSERT(nextOsrPoint != NULL, "Cannot find an offset OSR point for node %p", node);
         }
      }

   // Print some debugging information
   if (trace())
      {
      traceMsg(comp(), "\nOSR def info:\n");
      }

   for (int i = 0; i < numOSRPoints; ++i)
      {
      BitVector &info = aux._defsForOSR[i];
      //one reason that info can be NULL is that the block that contains that i-th osr point has
      //been deleted (e.g., because it was unreachable), and therefore, we have never set _defsForOSR[i] to
      //a non-NULL value
      if (info.IsNull()) continue;
      TR_ASSERT(!info.IsZero(), "OSR def info at index %d is empty", i);
      if (trace())
         {
         if (info.IsZero())
            {
            traceMsg(comp(), "OSR def info at index %d is empty\n", i);
            continue;
            }
         TR_ByteCodeInfo& bcinfo = _methodSymbol->getOSRPoints()[i]->getByteCodeInfo();
         traceMsg(comp(), "OSR defs at index %d bcIndex %d callerIndex %d\n", i, bcinfo.getByteCodeIndex(), bcinfo.getCallerIndex());
         *comp() << info;
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
      TR_UseDefInfo::BitVector const &defsForSymbol = aux._defsForSymbol[symIndex];
      if (!defsForSymbol.IsZero() &&
         isExpandedDefIndex(expandedNodeIndex) &&
         !sym->isRegularShadow() &&
         !sym->isMethod())
         {
            if (trace())
               {
               traceMsg(comp(), "defs for symbol %d with symref index %d\n", symIndex, symRef->getReferenceNumber());
               (*comp()) << defsForSymbol;
               traceMsg(comp(), "\n");
               }
            *analysisInfo -= defsForSymbol;
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
         Assign(aux._defsForOSR[osrIndex], *analysisInfo);
         if (trace())
            {
            traceMsg(comp(), "_defsForOSR[%d] at node %p \n", osrIndex, node);
            *comp() << aux._defsForOSR[osrIndex];
            traceMsg(comp(), "\n");
            }
         }
      if (osrPoint2 != NULL)
         {
         uint32_t osrIndex = osrPoint2->getOSRIndex();
         Assign(aux._defsForOSR[osrIndex], *analysisInfo);
         if (trace())
            {
            traceMsg(comp(), "_defsForOSR[%d] after node %p \n", osrIndex, node);
            *comp() << aux._defsForOSR[osrIndex];
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
      if (trace())
         {
         traceMsg(comp(), "%s OSR reaching definition analysis is required\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         }
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
   optimizer()->getMethodSymbol()->getFlowGraph()->setStructure(NULL);

   return 0;
   }

//returns true if there is a shared auto slot or a shared pending push temp slot, and returns false otherwise.
bool TR_OSRDefAnalysis::requiresAnalysis()
   {
   TR::ResolvedMethodSymbol *methodSymbol = optimizer()->getMethodSymbol();
   return methodSymbol->sharesStackSlots(comp());
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
     _deadVars(NULL),
     _liveVars(NULL)
   {}

int32_t TR_OSRLiveRangeAnalysis::perform()
   {
   if (comp()->getOption(TR_EnableOSR))
      {
      static const char *disableOSRLiveRangeAnalysis = feGetEnv("TR_DisableOSRLiveRangeAnalysis");
      //if (comp()->getOption(TR_DisableOSRLiveRangeAnalysis)) // add to Options later
      if (disableOSRLiveRangeAnalysis)
         {
         if (comp()->getOption(TR_TraceOSR))
            traceMsg(comp(), "OSR is enabled but OSR live range analysis is not.\n");
         return 0;
         }
      }
   else
      {
      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "OSR Option not enabled -- returning from OSR live range analysis.\n");
      return 0;
      }

   // Determine if the analysis is necessary
   if (!canAffordAnalysis())
      {
      if (comp()->getOption(TR_TraceOSR))
         {
         traceMsg(comp(), "%s OSR live range analysis is not affordable for this method because of the expected cost\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         traceMsg(comp(), "Returning...\n");
         }

      return 0;
      }
   else if (comp()->isPeekingMethod())
      {
      if (comp()->getOption(TR_TraceOSR))
         {
            traceMsg(comp(), "%s OSR live range analysis is not required because we are peeking\n",
               optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         traceMsg(comp(), "Returning...\n");
         }
      return 0;
      }
   else
      {
      if (comp()->getOption(TR_TraceOSR))
         {
         traceMsg(comp(), "%s OSR reaching live range analysis can be done\n",
            optimizer()->getMethodSymbol()->signature(comp()->trMemory()));
         }
      }

   TR_Structure* rootStructure = TR_RegionAnalysis::getRegions(comp(), optimizer()->getMethodSymbol());
   optimizer()->getMethodSymbol()->getFlowGraph()->setStructure(rootStructure);
   TR_ASSERT(rootStructure, "Structure is NULL");

   if (comp()->getOption(TR_TraceOSR))
      {
      traceMsg(comp(), "Starting OSR live range analysis\n");
      comp()->dumpMethodTrees("Before OSR live range analysis", optimizer()->getMethodSymbol());
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Perform liveness analysis
   //
   bool ignoreOSRuses = true;
   TR_Liveness liveLocals(comp(), optimizer(), comp()->getFlowGraph()->getStructure(), ignoreOSRuses);
   _deadVars = new (trStackMemory()) TR_BitVector(liveLocals.getNumberOfBits(), trMemory(), stackAlloc);
   _liveVars = new (trStackMemory()) TR_BitVector(liveLocals.getNumberOfBits(), trMemory(), stackAlloc);
   _pendingPushVars = new (trStackMemory()) TR_BitVector(liveLocals.getNumberOfBits(), trMemory(), stackAlloc);

   //set the structure to NULL so that the inliner (which is applied very soon after) doesn't need
   //update it.
   optimizer()->getMethodSymbol()->getFlowGraph()->setStructure(NULL);

   TR::SymbolReferenceTable *symRefTab   = comp()->getSymRefTab();

   TR_Array<List<TR::SymbolReference> > *autoSymRefs = comp()->getMethodSymbol()->getAutoSymRefs();
   TR_Array<List<TR::SymbolReference> > *pendingPushSymRefs = comp()->getMethodSymbol()->getPendingPushSymRefs();

   TR::TreeTop *firstTree = comp()->getMethodSymbol()->getFirstTreeTop();
   TR::Node *firstNode = firstTree->getNode();
   TR::TreeTop *next = firstTree->getNextTreeTop();
   int32_t numBits = liveLocals.getNumberOfBits();

   if (!autoSymRefs || (numBits == 0))
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

   int32_t maxSymRefNumber = 0;

   int32_t i;
   for (i=0;i<numSlots;i++)
      {
      List<TR::SymbolReference> symRefsAtThisSlot = (*autoSymRefs)[i];

      if (symRefsAtThisSlot.isEmpty()) continue;

      ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
      TR::SymbolReference *nextSymRef;
      for (nextSymRef = symRefsIt.getCurrent(); nextSymRef; nextSymRef=symRefsIt.getNext())
         {
         if (nextSymRef->getSymbol()->isAuto() &&
             !nextSymRef->getSymbol()->castToAutoSymbol()->isLiveLocalIndexUninitialized() &&
             !nextSymRef->getSymbol()->holdsMonitoredObject())
            {
            liveLocalIndexToSymRefNumberMap[nextSymRef->getSymbol()->castToAutoSymbol()->getLiveLocalIndex()] = nextSymRef->getReferenceNumber();
            nextSymRef->getSymbol()->setLocalIndex(0);
            if (nextSymRef->getReferenceNumber() > maxSymRefNumber)
               maxSymRefNumber = nextSymRef->getReferenceNumber();
            }
         }
      }

   for (i=0;i<numPendingSlots;i++)
      {
      List<TR::SymbolReference> symRefsAtThisSlot = (*pendingPushSymRefs)[i];

      if (symRefsAtThisSlot.isEmpty()) continue;

      ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
      TR::SymbolReference *nextSymRef;
      for (nextSymRef = symRefsIt.getCurrent(); nextSymRef; nextSymRef=symRefsIt.getNext())
         {
         if (nextSymRef->getSymbol()->isAuto() && !nextSymRef->getSymbol()->castToAutoSymbol()->isLiveLocalIndexUninitialized())
            {
            liveLocalIndexToSymRefNumberMap[nextSymRef->getSymbol()->castToAutoSymbol()->getLiveLocalIndex()] = nextSymRef->getReferenceNumber();
            _pendingPushVars->set(nextSymRef->getSymbol()->castToAutoSymbol()->getLiveLocalIndex());
            nextSymRef->getSymbol()->setLocalIndex(0);
            if (nextSymRef->getReferenceNumber() > maxSymRefNumber)
               maxSymRefNumber = nextSymRef->getReferenceNumber();
            }
         }
      }

   _pendingSlotValueParents = (NodeParentInfo **)trMemory()->allocateStackMemory(numBits*sizeof(NodeParentInfo *));

   TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findOrCreateOSRMethodData(comp()->getCurrentInlinedSiteIndex(), comp()->getMethodSymbol());

   TR::Block *block = NULL;
   int32_t blockNum = -1;
   TR_BitVector *liveVars = NULL;
   TR::TreeTop *tt;

   vcount_t visitCount = comp()->incVisitCount();
   block = comp()->getStartBlock();
   while (block)
      {
      memset(_pendingSlotValueParents, 0, numBits*sizeof(NodeParentInfo *));

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
            for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
               {
               succ = toBlock((*edge)->getTo());
               *_liveVars |= *liveLocals._blockAnalysisInfo[succ->getNumber()];
               }

            for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
               {
               succ = toBlock((*edge)->getTo());
               *_liveVars |= *liveLocals._blockAnalysisInfo[succ->getNumber()];
               }

            if (comp()->getOption(TR_TraceOSR))
               {
               traceMsg(comp(), "BB_End for block_%d: live vars = ", block->getNumber());
               _liveVars->print(comp());
               traceMsg(comp(), "\n");
               }
            }

         TR_ByteCodeInfo bci = comp()->getMethodSymbol()->getOSRByteCodeInfo(tt->getNode());

         // Check if this treetop is an OSR pending push store or load. If so, it will shift the
         // cursor to before the PPs. The cursor will either point at the first PP or the treetop
         // before it, depending on whether it could be the OSR point for these PP.
         TR::TreeTop *offsetOSRTreeTop = NULL;
         if (comp()->getMethodSymbol()->isOSRRelatedNode(tt->getNode()))
            {
            offsetOSRTreeTop = tt;   
            tt = collectPendingPush(bci, tt, _liveVars);
            TR_ByteCodeInfo osrBCI = comp()->getMethodSymbol()->getOSRByteCodeInfo(tt->getNode());

            // If it is the start of this block or has a different bytecode index, it cannot be
            // the OSR point
            if (tt == firstTT || osrBCI.getCallerIndex() != bci.getCallerIndex()
                || osrBCI.getByteCodeIndex() != bci.getByteCodeIndex())
               tt = tt->getNextTreeTop();
            }

         TR::Node *node = tt->getNode();
         bool isPotentialOSRPoint = comp()->isPotentialOSRPointWithSupport(tt);

         // If the transition point is after the OSR point, build the OSR live info before
         // any PPS are processed.
         if (isPotentialOSRPoint && comp()->getOSRTransitionTarget() == TR::postExecutionOSR)
            {
            TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
            bcInfo.setByteCodeIndex(bcInfo.getByteCodeIndex() + comp()->getOSRInductionOffset(node));
            TR_OSRPoint *offsetOSRPoint = comp()->getMethodSymbol()->findOSRPoint(bcInfo);
            TR_ASSERT(offsetOSRPoint != NULL, "Cannot find an offset OSR point for node %p", node);
            buildOSRLiveRangeInfo(node, _liveVars, offsetOSRPoint, liveLocalIndexToSymRefNumberMap,
               maxSymRefNumber, numBits, osrMethodData, TR::inductionOSR);
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
         if (isPotentialOSRPoint && (comp()->getOSRTransitionTarget() == TR::preExecutionOSR ||
             comp()->requiresAnalysisOSRPoint(node)))
            {
            TR_OSRPoint *osrPoint = comp()->getMethodSymbol()->findOSRPoint(bci);
            TR_ASSERT(osrPoint != NULL, "Cannot find an OSR point for node %p", node);
            buildOSRLiveRangeInfo(node, _liveVars, osrPoint, liveLocalIndexToSymRefNumberMap,
               maxSymRefNumber, numBits, osrMethodData,
               comp()->getOSRTransitionTarget() == TR::postExecutionOSR ? TR::analysisOSR : TR::inductionOSR);
            }
         }

      block = block->getNextBlock();
      }

   } // scope for stack memory region

   return 0;
   }

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
            traceMsg(comp(), "+++ local index %d OSR PPS LIVE\n", localIndex);
         }
      traceMsg(comp(), "Looking at [%p]\n", node);
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
      TR::AutomaticSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
      if (local && !local->isLiveLocalIndexUninitialized())
         {
         int32_t localIndex = local->getLiveLocalIndex();
         TR_ASSERT(localIndex >= 0, "bad local index: %d\n", localIndex);

         // This local is killed only if the live range of any loads of this symbol do not overlap
         // with this store.
         //
         if (local->getLocalIndex() == 0)
            {
            if (_pendingPushVars->get(localIndex))
               {
               if (_pendingSlotValueParents[localIndex])
                  {
                  NodeParentInfo *nodeParentInfo = _pendingSlotValueParents[localIndex];
                  ParentInfo *cursor = nodeParentInfo->_parentInfo;

                  // First time this node has been encountered.
                  //
                  if (node->getFirstChild()->getVisitCount() != visitCount)
                     {
                     node->getFirstChild()->setVisitCount(visitCount);
                     node->getFirstChild()->setLocalIndex(node->getFirstChild()->getReferenceCount());

                     if (node->getFirstChild()->getOpCode().hasSymbolReference() &&
                         node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto())
                        {
                        TR::AutomaticSymbol *rhsLocal = node->getFirstChild()->getSymbolReference()->getSymbol()->getAutoSymbol();
                        if (rhsLocal && !rhsLocal->isLiveLocalIndexUninitialized())
                           {
                           int32_t rhsLocalIndex = rhsLocal->getLiveLocalIndex();
                           TR_ASSERT(rhsLocalIndex >= 0, "bad local index: %d\n", rhsLocalIndex);
                           rhsLocal->setLocalIndex(rhsLocal->getLocalIndex() + node->getFirstChild()->getReferenceCount());
                           }
                         }
                      }

                   while (cursor)
                     {
                     TR::Node *loadParent = cursor->_parent;
                     int32_t loadChildNum = cursor->_childNum;
                     TR::Node *loadNode = loadParent->getChild(loadChildNum);
                     loadParent->setAndIncChild(loadChildNum, node->getFirstChild());
                     loadNode->recursivelyDecReferenceCount();
                     cursor = cursor->_next;
                     }

                  _pendingSlotValueParents[localIndex] = NULL;
                  }
               }

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
      TR::AutomaticSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
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

         if (0 && _pendingPushVars->get(localIndex))
            {
            NodeParentInfo *nodeParentInfo = _pendingSlotValueParents[localIndex];
            if (!nodeParentInfo)
               {
               nodeParentInfo = (NodeParentInfo *) comp()->trMemory()->allocateStackMemory(sizeof(NodeParentInfo));
               nodeParentInfo->_node = node;
               nodeParentInfo->_parentInfo = NULL;
               _pendingSlotValueParents[localIndex] = nodeParentInfo;
               }

            ParentInfo *parentInfo = (ParentInfo *) comp()->trMemory()->allocateStackMemory(sizeof(ParentInfo));
            parentInfo->_parent = parent;
            parentInfo->_childNum = childNum;
            ParentInfo *oldParentInfo = nodeParentInfo->_parentInfo;
            parentInfo->_next = oldParentInfo;
            nodeParentInfo->_parentInfo = parentInfo;
            }

         static const char *disallowOSRPPS3 = feGetEnv("TR_DisallowOSRPPS3");

         if ((!disallowOSRPPS3 || !_pendingPushVars->get(localIndex)) &&
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



void TR_OSRLiveRangeAnalysis::buildOSRLiveRangeInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
   int32_t *liveLocalIndexToSymRefNumberMap, int32_t maxSymRefNumber, int32_t numBits,
   TR_OSRMethodData *osrMethodData, TR::OSRPointType osrPointType)
   {
   TR_ASSERT(liveVars, "live variable info must be available for a block\n");
   _deadVars->setAll(numBits);
   *_deadVars -= *liveVars;

   TR_BitVector *deadSymRefs;
   if (_deadVars->isEmpty())
      deadSymRefs = NULL;
   else
      {
      deadSymRefs = new (trHeapMemory()) TR_BitVector(maxSymRefNumber+1, trMemory(), heapAlloc);
      TR_BitVectorIterator bvi(*_deadVars);
      while (bvi.hasMoreElements())
         {
         int32_t nextDeadVar = bvi.getNextElement();
         if (liveLocalIndexToSymRefNumberMap[nextDeadVar] >= 0)
            deadSymRefs->set(liveLocalIndexToSymRefNumberMap[nextDeadVar]);
         }
      }

   osrMethodData->setNumSymRefs(numBits);
   osrMethodData->addLiveRangeInfo(osrPoint->getByteCodeInfo().getByteCodeIndex(), osrPointType, deadSymRefs);

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




bool TR_OSRLiveRangeAnalysis::canAffordAnalysis()
   {
   if (comp()->getOSRMode() == TR::involuntaryOSR) // save some compile time
      return false;

   if (!comp()->canAffordOSRControlFlow())
      return false;

   return true;
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

   TR::CFGNode *cfgNode;
   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      if (cfgNode->getExceptionSuccessors().empty())
         continue;

      TR::Block *block = toBlock(cfgNode);
      bool seenInduceOSRCall = false;
      TR::TreeTop *treeTop;
      for (treeTop = block->getEntry(); treeTop != block->getExit(); treeTop = treeTop->getNextTreeTop())
         {
         if ((treeTop->getNode()->getNumChildren() > 0) &&
             treeTop->getNode()->getFirstChild()->getOpCode().hasSymbolReference() &&
             (comp()->getSymRefTab()->element(TR_induceOSRAtCurrentPC) == treeTop->getNode()->getFirstChild()->getSymbolReference()))
	    {
            seenInduceOSRCall = true;
            break;
	    }
         }

      if (seenInduceOSRCall)
         continue;

      for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end();)
         {
         TR::Block *catchBlock = toBlock((*edge++)->getTo());
         if (catchBlock->isOSRCatchBlock())
            {
            if (!seenInduceOSRCall &&
                performTransformation(comp(), "%s: Remove redundant exception edge from block_%d at [%p] to OSR catch block_%d at [%p]\n", OPT_DETAILS, block->getNumber(), block, catchBlock->getNumber(), catchBlock))
                cfg->removeEdge(block, catchBlock);
            }
         }
      }

   comp()->setOSRInfrastructureRemoved(true);

   return 1;
   }
