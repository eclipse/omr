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

#include "optimizer/PartialRedundancy.hpp"

#include <limits.h>                                 // for USHRT_MAX
#include <stddef.h>                                 // for size_t
#include <stdint.h>                                 // for int32_t
#include <stdlib.h>                                 // for atoi
#include <string.h>                                 // for NULL, memset
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/Linkage.hpp"                      // for Linkage
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"              // for TR::Options, etc
#include "control/Recompilation.hpp"                // for TR_Recompilation
#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"                         // for TR_Memory, etc
#include "env/jittypes.h"                           // for TR_ByteCodeInfo
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for TR::DataType, etc
#include "il/ILOps.hpp"                             // for TR::ILOpCode, etc
#include "il/Node.hpp"                              // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode, etc
#include "il/symbol/MethodSymbol.hpp"               // for MethodSymbol
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/Link.hpp"                           // for TR_LinkHead
#include "infra/List.hpp"                           // for ListElement, etc
#include "infra/CfgEdge.hpp"                        // for CFGEdge
#include "infra/CfgNode.hpp"                        // for CFGNode
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/InductionVariable.hpp"          // for TR_PrimaryInductionVariable
#include "optimizer/LocalAnalysis.hpp"
#include "optimizer/Optimization.hpp"               // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                  // for Optimizer
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase
#ifdef J9_PROJECT_SPECIFIC
#include "runtime/J9Profiler.hpp"                   // for TR_ValueProfiler
#endif


class TR_RegisterCandidate;

static int32_t numIterations = 0;

#define OPT_DETAILS "O^O PARTIAL REDUNDANCY ELIMINATION: "

#define BOTH_SAME 0
#define FIRST_LARGER 1
#define SECOND_LARGER 2
#define BOTH_UNRELATED 3

#define NUMBER_OF_NODES_PER_PROFILING_EXPANSION 20

#define NUM_ITERATIONS 2

// The list of supported opcodes MUST be the same as that supported by local
// analyses, so use their method.
//

inline bool TR_PartialRedundancy::isSupportedOpCode(TR::Node *node, TR::Node *parent)
   {
   return (TR_LocalAnalysis::isSupportedNode(node, comp(), parent));
   }

static void resetChildrensVisitCounts(TR::Node *node, vcount_t count)
   {
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      resetChildrensVisitCounts(child, count);
      child->setVisitCount(count);
      }
   }

#ifdef J9_PROJECT_SPECIFIC
static void correctDecimalPrecision(TR::Node *store, TR::Node *child, TR::Compilation *comp)
   {
   if (child->getType().isBCD() &&
       child->getDecimalPrecision() != store->getDecimalPrecision())
      {
      TR::ILOpCodes modPrecOp = TR::ILOpCode::modifyPrecisionOpCode(child->getDataType());
      TR_ASSERT(modPrecOp != TR::BadILOp,"no bcd modify precision opcode found\n");
      TR::Node *modPrecNode = TR::Node::create(child, modPrecOp, 1);
      bool isTruncation = store->getDecimalPrecision() < child->getDecimalPrecision();
      if (comp->cg()->traceBCDCodeGen())
         traceMsg(comp,"%screating %s (%p) to correctDecimalPrecision (%d->%d : isTruncation=%s) on node %s (%p)\n", OPT_DETAILS,
            modPrecNode->getOpCode().getName(),modPrecNode,
            child->getDecimalPrecision(),store->getDecimalPrecision(),
            isTruncation ? "yes":"no",
            child->getOpCode().getName(),child);
      modPrecNode->setChild(0, child);
      modPrecNode->setDecimalPrecision(store->getDecimalPrecision());
      modPrecNode->transferSignState(child, isTruncation);
      store->setAndIncValueChild(modPrecNode);
      }
   }
#endif

static bool shouldAllowOptimalSubNodeReplacement(TR::Compilation *comp)
   {
   if ((comp->getOptions()->getLastOptTransformationIndex() < comp->getOptions()->getMaxLastOptTransformationIndex()) ||
       (comp->getOptions()->getFirstOptTransformationIndex() > comp->getOptions()->getMinFirstOptTransformationIndex()))
      {
      if (!comp->getOption(TR_TracePREForOptimalSubNodeReplacement))
         return false;
      }

   return true;
   }


static bool shouldAllowLimitingOfOptimalPlacement(TR::Compilation *comp)
   {
   if (shouldAllowOptimalSubNodeReplacement(comp))
      {
      if ((comp->getOptions()->getLastOptTransformationIndex() < comp->getOptions()->getMaxLastOptTransformationIndex()) ||
          (comp->getOptions()->getFirstOptTransformationIndex() > comp->getOptions()->getMinFirstOptTransformationIndex()))
         return false;
      }

   return true;
   }


TR_PartialRedundancy::TR_PartialRedundancy(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   static const char *e = feGetEnv("TR_loadaddrPRE");
   _loadaddrPRE = e ? (atoi(e) != 0) : false;

   _ignoreLoadaddrOfLitPool = false;
   if (_loadaddrPRE && _ignoreLoadaddrOfLitPool)
      _loadaddrPRE = false; // need to check each loadaddr and cannot ignore them outright
   }

TR_PartialRedundancy::ContainerType *TR_PartialRedundancy::allocateContainer(int32_t size)
   {
   return new (trStackMemory())TR_BitVector(size, trMemory(), stackAlloc);
   }

static bool ignoreValueOfMultipleReturnNode(TR::Node *node, TR::SparseBitVector &seenNodes)
   {
   auto npIdx = node->getGlobalIndex();
   if (seenNodes[npIdx])
      return false;
   seenNodes[npIdx] = 1;

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      if (ignoreValueOfMultipleReturnNode(node->getChild(i), seenNodes))
         return true;
   return false;
   }

bool TR_PartialRedundancy::ignoreNode (TR::Node *node)
   {
   TR::ILOpCodes        op      = node->getOpCodeValue();
   TR::CodeGenerator   *cg      = comp()->cg();
   TR::Linkage         *linkage = cg->getLinkage();
   TR::SymbolReference *symRef  = node->getOpCode().hasSymbolReference()?node->getSymbolReference():NULL;

   TR::SparseBitVector seenNodes(comp()->allocator());
   if (ignoreValueOfMultipleReturnNode(node, seenNodes)) return true;

   if (op == TR::loadaddr)
      {
      if (_loadaddrPRE)
         return false; // loadaddrs of any symRefs are always allowed

      if (_ignoreLoadaddrOfLitPool)
         {
            {
            return false;  // and allow all other symRefs
            }
         }

      return symRef != NULL;
      }

   return false;
   }

int32_t TR_PartialRedundancy::perform()
   {
   // Calculate the number of expressions commoned by PRE that are allowed
   // to be profiled. This is to keep the value profiling related code expansion
   // within some limits
   //
   _numProfilingsAllowed = (((USHRT_MAX/2 - 1000) - comp()->getNodeCount())/NUMBER_OF_NODES_PER_PROFILING_EXPANSION);
   if (_numProfilingsAllowed < 0)
     _numProfilingsAllowed = 0;

   // PRE is too expensive when profiling with HCR on, due to the number
   // of blocks and temps this configuration creates. Disabling it
   // won't affect performance if there are no profilings allowed.
   if (comp()->getProfilingMode() == JitProfiling && comp()->getHCRMode() != TR::none && _numProfilingsAllowed == 0)
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // setAlteredCode(false);

   TR::CFG *cfg = comp()->getFlowGraph();
   TR_Structure *rootStructure = cfg->getStructure();

   // for particularly big methods, see also comments in LocalAnalysis.cpp
   //comp()->resetVisitCounts(0);

   if (trace())
      {
      comp()->incOrResetVisitCount();
      TR::TreeTop *currentTree = comp()->getStartTree();
      while (!(currentTree == NULL))
         {
         getDebug()->print(comp()->getOutFile(), currentTree);
         currentTree = currentTree->getNextTreeTop();
         }
      }

   // Call to constructor of Isolatedness results in successive calls
   // down the call chain to constructors of analyses that are prereqs.
   // Thus PartialRedundancy would be done after Isolatedness which would
   // be done after Latestness and so on
   //
   _isolatedness = new (comp()->allocator()) TR_Isolatedness(comp(), optimizer(), rootStructure, trace());

   // Obtain local anticipatable info; used in redundant set calculation
   //
   TR_LocalAnticipatability &localAnticipatability = _isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability;

   if (trace())
      traceMsg(comp(), "Starting PartialRedundancy\n");

   _numNodes = cfg->getNextNodeNumber();
   TR_ASSERT(_numNodes > 0, "Partial Redundancy, node numbers not assigned");

   if (trace())
      comp()->dumpMethodTrees("Trees after PRE node numbering\n");

   int i;
   _numberOfBits = _isolatedness->_latestness->_numberOfBits;
   _optSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_optSetInfo, 0, _numNodes*sizeof(ContainerType *));
   _rednSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_rednSetInfo, 0, _numNodes*sizeof(ContainerType *));
   _unavailableSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_unavailableSetInfo, 0, _numNodes*sizeof(ContainerType *));
   _origOptSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_origOptSetInfo, 0, _numNodes*sizeof(ContainerType *));
   _globalRegisterWeights = (int32_t *)trMemory()->allocateStackMemory(_numNodes*sizeof(int32_t));
   memset(_globalRegisterWeights, 0, _numNodes*sizeof(int32_t));

   // These bit vectors are for storing some summary information
   // over all the blocks; e.g. is an expression repeated or redundant
   // in any block whatsoever or not. This information is useful to know
   // whether we really need to introduce a new temp for this expression
   // in global transformations (PRE) or would local CSE common some of these
   // case without introducing any temps
   //
   ContainerType *redundants = allocateContainer(_numberOfBits);
   ContainerType *optimals = allocateContainer(_numberOfBits);
   ContainerType *temp = allocateContainer(_numberOfBits);

   TR::CFGNode *nextNode;
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR_Structure *blockStructure = (toBlock(nextNode))->getStructureOf();
      if (blockStructure == NULL)
         continue;
      i = blockStructure->getNumber();
      _optSetInfo[i] = allocateContainer(_numberOfBits);
      _origOptSetInfo[i] = allocateContainer(_numberOfBits);
      _rednSetInfo[i] = allocateContainer(_numberOfBits);
      _unavailableSetInfo[i] = allocateContainer(_numberOfBits);
      _unavailableSetInfo[i]->setAll(_numberOfBits);
      }

   // Nodes (expressions) that require a symbol to be created
   //
   _symOptimalNodes = allocateContainer(_numberOfBits);
   _temp = allocateContainer(_numberOfBits);

   TR::Node **supportedNodesAsArray = _isolatedness->_supportedNodesAsArray;

   // These data structures keeps track of new temps created by PRE
   //
   _newSymbols = (TR::Symbol **)trMemory()->allocateStackMemory(_numberOfBits*sizeof(TR::Symbol *));
   _newSymbolReferences = (TR::SymbolReference **)trMemory()->allocateStackMemory(_numberOfBits*sizeof(TR::SymbolReference *));
   _newSymbolsMap = (int32_t*) trMemory()->allocateStackMemory(_numberOfBits*sizeof(int32_t));
   _registerCandidates = (TR_RegisterCandidate **)trMemory()->allocateStackMemory(_numberOfBits*sizeof(TR_RegisterCandidate *));

   memset(_newSymbols, 0, _numberOfBits*sizeof(TR::Symbol *));
   memset(_newSymbolReferences, 0, _numberOfBits*sizeof(TR::SymbolReference *));
   memset(_registerCandidates, 0, _numberOfBits*sizeof(TR_RegisterCandidate *));

   // Initialize the map to -1 values
   //
   memset(_newSymbolsMap, 0xFF, _numberOfBits*sizeof(int32_t));
   _useAliasSetsNotGuaranteedToBeCorrect = false;

   ContainerType *negation = allocateContainer(_numberOfBits);

   //
   // Now computing optimality and redundancy information
   // based on the analysis results
   //
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR::Block *block = toBlock(nextNode);
      TR_Structure *blockStructure = block->getStructureOf();
      if (blockStructure == NULL)
         continue;
      i = blockStructure->getNumber();
      //negation.setAll(_numberOfBits);

      // Note : Modification to use latestness info rather than isolatedness which
      // we don't perform
      //
      ///////negation -= *(_isolatedness->_outSetInfo[i]);
      _isolatedness->copyFromInto(_isolatedness->_latestness->_inSetInfo[i], _optSetInfo[i]);

      //*(_optSetInfo[i]) &= negation;

      negation->setAll(_numberOfBits);
      _isolatedness->copyFromInto(_isolatedness->_latestness->_inSetInfo[i], temp);
      ///////temp |= *(_isolatedness->_outSetInfo[i]);
      *negation -= *temp;

      // _isolatedness->copyFromInto(localAnticipatability.getAnalysisInfo(blockStructure->asBlock()->getBlock()->getNumber()), _rednSetInfo[i]);
      *_rednSetInfo[i] = *localAnticipatability.getDownwardExposedAnalysisInfo(blockStructure->asBlock()->getBlock()->getNumber());

      //if (!block->isCold())
         *(_rednSetInfo[i]) &= *negation;
      //else
      //         _rednSetInfo[i]->empty();

      *redundants |= *(_rednSetInfo[i]);

      if (trace())
         {
         traceMsg(comp(), "\nOptimality info for block : %d \n", i);
         (*(_optSetInfo+i))->print(comp());

         traceMsg(comp(), "\nRedundancy info for block : %d \n", i);
         (*(_rednSetInfo+i))->print(comp());

         }
      }

   // Compute the selective optimality and redundancy information
   // (which is the original optimality and redundancy information but
   // filtered with the information whether an computation really needs
   // to placed (using a temp) or whether it can be handled by local CSE).
   // We basically only introduce a temp if the value is redundant outside
   // of the block where the computation is placed optimally; else we trust
   // local CSE to do the required commoning.
   //
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR_Structure *blockStructure = (toBlock(nextNode))->getStructureOf();
      if (blockStructure == NULL)
         continue;
      i = blockStructure->getNumber();

      *(_origOptSetInfo[i]) = *(_optSetInfo[i]);

      (*(_optSetInfo[i])) &= *redundants;

      *optimals |= *(_optSetInfo[i]);

      if (trace())
         {
         traceMsg(comp(), "\nSelective Optimality info for block : %d \n", i);
         (*(_optSetInfo+i))->print(comp());

         traceMsg(comp(), "\nSelective Redundancy info for block : %d \n", i);
         (*(_rednSetInfo+i))->print(comp());
         }
      }


   // Collect info about which expressions need a symbol
   //
    _symOptimalNodes->setAll(_numberOfBits);
    ContainerType::Cursor bvi(*optimals);
    for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
      {
      int32_t nextOptimalComputation = bvi;

      if (nextOptimalComputation != 0)
         {
         TR::Node *nextOptimalNode = supportedNodesAsArray[nextOptimalComputation];

         if (nextOptimalNode->getOpCode().isCheck())
            _symOptimalNodes->reset(nextOptimalComputation);
         }
      }

   // Perform expression dominance and redundant expression adjustment
   // to obtain 'real' solution for checks. Solution till this point is
   // optimistic in that it ignores exception-ordering constraints in Java.
   // This step figures out if code motion for checks can in fact be done.
   //
   _exceptionCheckMotion = NULL;
   if (!optimals->isEmpty())
      {
      _exceptionCheckMotion = new (comp()->allocator()) TR_ExceptionCheckMotion(comp(), optimizer(), this);
      _exceptionCheckMotion->perform();
      _optSetInfo = _exceptionCheckMotion->getActualOptSetInfo();
      _rednSetInfo = _exceptionCheckMotion->getActualRednSetInfo();
      _orderedOptNumbersList = _exceptionCheckMotion->getOrderedOptNumbersList();
      }

   redundants->empty();
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR_Structure *blockStructure = (toBlock(nextNode))->getStructureOf();
      if (blockStructure == NULL)
         continue;
      i = blockStructure->getNumber();
      *redundants |= *(_rednSetInfo[i]);
      }

   optimals->empty();
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR_Structure *blockStructure = (toBlock(nextNode))->getStructureOf();
      if (blockStructure == NULL)
         continue;
      i = blockStructure->getNumber();
      (*(_optSetInfo[i])) &= *redundants;
      *optimals |= *(_optSetInfo[i]);
      _globalRegisterWeights[i] = 1;
      calculateGlobalRegisterWeightsBasedOnStructure(blockStructure, (_globalRegisterWeights+i));

      if (trace())
         {
         traceMsg(comp(), "\nFinal Selective Optimality info for block : %d \n", i);
         (*(_optSetInfo+i))->print(comp());

         traceMsg(comp(), "\nFinal Selective Redundancy info for block : %d \n", i);
         (*(_rednSetInfo+i))->print(comp());

         traceMsg(comp(), "\nFinal Unavailable info for block : %d \n", i);
         (*(_unavailableSetInfo+i))->print(comp());

         traceMsg(comp(), "\nGlobal Register weight for block : %d is %d\n", i, _globalRegisterWeights[i]);
         }
      }


   // Duplicate the nodes so that expressions don't change while being
   // moved around and placed optimally/eliminated.
   //
   ContainerType::Cursor bvi1(*redundants);
   for (bvi1.SetToFirstOne(); bvi1.Valid(); bvi1.SetToNextOne())
      {
      int32_t nextRedundantComputation = bvi1;

      if (nextRedundantComputation != 0)
         {
         supportedNodesAsArray[nextRedundantComputation] =  supportedNodesAsArray[nextRedundantComputation]->duplicateTreeWithCommoning(comp()->allocator());
         supportedNodesAsArray[nextRedundantComputation]->resetFlagsForCodeMotion();
         }
      }

   // If temps were created, we may want to do GCP later
   // as field privatization helped subsequent GCP.
   //
   bool createdTemp = false;

   // Create all the temps that are going to be used right at the
   // outset. This is in fact required as we could have a subtree (smaller expr)
   // being placed as optimal in some block B and a tree (larger expr)
   // containing the subtree being placed optimally in some block that follows
   // the block B in the cfg. In this case we need to replace the subtree
   // by the temp in the tree (larger expression) before we place it optimally
   // so as to use the value of the subtree that is guaranteed to be available
   // in a temp rather than recompute the subtree.
   //
   //
   ContainerType::Cursor bvi2(*optimals);
    for (bvi2.SetToFirstOne(); bvi2.Valid(); bvi2.SetToNextOne())
      {
      int32_t nextOptimalComputation = bvi2;

      if (nextOptimalComputation != 0)
         {
         TR::Node *nextOptimalNode = supportedNodesAsArray[nextOptimalComputation];

         if ((_newSymbolsMap[nextOptimalComputation] < 0) &&
              !nextOptimalNode->getOpCode().isCheck() &&
              !nextOptimalNode->getOpCode().isAnchor() &&
              !(nextOptimalNode->getOpCodeValue() == TR::treetop))
            {
            if (nextOptimalNode->getOpCode().isLoadVarDirect() &&
                !nextOptimalNode->getSymbol()->isStatic() &&
                !nextOptimalNode->getSymbol()->isMethodMetaData())
               {
               TR::SymbolReference *optimalSymRef = nextOptimalNode->getSymbolReference();
               _newSymbols[nextOptimalComputation] = optimalSymRef->getSymbol();
               _newSymbolsMap[nextOptimalComputation] = optimalSymRef->getReferenceNumber();
               _newSymbolReferences[nextOptimalComputation] = optimalSymRef;
               }
            else if ((nextOptimalNode->getOpCode().isLoadVarDirect() &&
                      (nextOptimalNode->getSymbol()->isStatic() || nextOptimalNode->getSymbol()->isMethodMetaData())) ||
                     !isNodeAnImplicitNoOp(nextOptimalNode))
               {
               if (trace())
                  {
                  traceMsg(comp(), "Creating new symbol for optimal expr number %d node %p\n", nextOptimalComputation, nextOptimalNode);

                  }
               TR::DataType dataType = nextOptimalNode->getDataType();
               // set the datatype of the temporary created below to
               // be at least OMR::Int ; otherwise the opcode used to create
               // the store (to initialize) and the datatype on the symbol
               // are inconsistent
               //
               if (fe()->dataTypeForLoadOrStore(nextOptimalNode->getDataType()) != nextOptimalNode->getDataType() &&  // only do it for Java where we use istore for small intergral types
                  nextOptimalNode->getSize() < 4)
                  dataType = TR::Int32;

               size_t size = 0;
               //if (nextOptimalNode->getType().isBCD())
                  size = nextOptimalNode->getSize();
               TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType, false, size);
               TR::Symbol *newAutoSym = newSymbolReference->getSymbol();

               comp()->AddCopyPropagationRematerializationCandidate(newSymbolReference);

               if (nextOptimalNode->getOpCode().hasSymbolReference() && nextOptimalNode->getSymbolReference()->getSymbol()->isNotCollected())
                  newAutoSym->setNotCollected();

               _newSymbols[nextOptimalComputation] = newAutoSym;
               _newSymbolsMap[nextOptimalComputation] = newSymbolReference->getReferenceNumber();
               _newSymbolReferences[nextOptimalComputation] = newSymbolReference;
               createdTemp = true;
               }
            }
         }
      }

    // Avoid creating a store inside a loop for a computation
    // outside a loop if it is'nt likely that it will be placed in
    // global register. Check for likelihood of obtaining a global register
    // using above test (similar to test done in LocalAnalysis.cpp)
    //
    ContainerType::Cursor bvi3(*optimals);
    for (bvi3.SetToFirstOne(); bvi3.Valid(); bvi3.SetToNextOne())
       {
       int32_t nextOptimalComputation = bvi3;
       int32_t cost = 0;
       int32_t benefit = 0;

       TR::Node *nextOptimalNode = supportedNodesAsArray[nextOptimalComputation];
       if (nextOptimalNode->getOpCode().isCheck() ||
           (comp()->requiresSpineChecks() && nextOptimalNode->getOpCode().hasSymbolReference() && nextOptimalNode->getSymbol()->isArrayShadowSymbol()))
          continue;

       TR::CFGNode *nextNode;
       for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
          {
          if (!nextNode->asBlock()->isCold())
             {
             if (_rednSetInfo[nextNode->getNumber()]->get(nextOptimalComputation))
                {
                TR_Structure *containingLoop = NULL;
                int32_t weight = 1;
                nextNode->asBlock()->getStructureOf()->calculateFrequencyOfExecution(&weight);
                if (nextNode->getFrequency() >= 0)
                   weight = weight * nextNode->getFrequency();

                if (trace())
                   traceMsg(comp(), "Benefit block_%d benefit %d\n", nextNode->getNumber(), weight);
                benefit = benefit + weight;
                }

             if (_optSetInfo[nextNode->getNumber()]->get(nextOptimalComputation))
                {
                int32_t weight = 1;
                nextNode->asBlock()->getStructureOf()->calculateFrequencyOfExecution(&weight);

                if (nextNode->getFrequency() >= 0)
                   weight = weight * nextNode->getFrequency();

                if (trace())
                   traceMsg(comp(), "Cost block_%d cost %d\n", nextNode->getNumber(), weight);
                cost = cost + weight;
                }
             }

          if (((_rednSetInfo[nextNode->getNumber()]->get(nextOptimalComputation)) ||
          	   (_optSetInfo[nextNode->getNumber()]->get(nextOptimalComputation))) &&
              (
              false ))
             {
             invalidateOptimalComputation(nextOptimalComputation);
             }
          }

       if (cost > benefit)
          {
          if (trace())
             traceMsg(comp(), "Cost-benefit analysis (cost %d > benefit %d) said ignore expression #%d\n", cost, benefit, nextOptimalComputation);
          invalidateOptimalComputation(nextOptimalComputation);
          }
       }

    int32_t preIndex = 1000000;
    static char *c1 = feGetEnv("TR_PreIndex");
    if (c1)
       preIndex = atoi(c1);

   _counter = 0;

   // Go through the blocks and place computations optimally and
   // then replace occurrences of the expression by the temp in the optimal
   // block (if there are any).
   //
   bool methodContainsCatchBlocks = false;

   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR::Block *block = toBlock(nextNode);
      if (!block->getExceptionPredecessors().empty())
         methodContainsCatchBlocks = true;

      if (block->getEntry() != NULL)
         {
         // Use for debugging
         //
         if (_counter > preIndex)
            break;

         _counter++;

         placeComputationsOptimally(block, &supportedNodesAsArray);
         }
      }

   //
   // Go through the blocks and place computations optimally and
   // remove redundant computations in the block (or replace by load
   // of a temp).
   //
   _visitCount = comp()->incOrResetVisitCount();
   TR::Block *block;
   for (block = comp()->getStartBlock(); block!=NULL; block = block->getNextBlock())
      {
      // Do not eliminate redundancy from cold blocks as this may result
      // in a store remaining in a non-cold block simply for this solitary
      // load. Note we still have to process the cold block because it MAY
      // be executed if the cold block is actually reached somehow; in that case
      // a privatization store would still need to be inserted along the cold
      // path. We never allow loads of PRE temps in cold blocks though. So we
      // still avoid inserting a store in a non-cold block simply for a load in a cold block.
      //
      if ((block->getEntry() != NULL) /* &&
                                         !block->isCold() */)
         {
         // Use for debugging
         //
         if (_counter > preIndex)
            break;

         _counter++;

         eliminateRedundantComputations(block, supportedNodesAsArray, _rednSetInfo, block->getEntry());
         }
      }

   if (trace())
      {
      traceMsg(comp(), "\nEnding PartialRedundancy\n");
      printTrees();
      comp()->dumpMethodTrees("Trees after PRE done\n");
      }

   if (manager()->getAlteredCode())
      {
      optimizer()->enableAllLocalOpts();

      if (createdTemp)
         optimizer()->setRequestOptimization(OMR::globalValuePropagation, true);

      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      optimizer()->setRequestOptimization(OMR::globalDeadStoreElimination, true);
      }


   if (_useAliasSetsNotGuaranteedToBeCorrect && methodContainsCatchBlocks)
      optimizer()->setAliasSetsAreValid(false);

   return 10; // actual cost
   }

void TR_PartialRedundancy::invalidateOptimalComputation(int32_t nextOptimalComputation)
   {
   TR::CFGNode *nextNode;
   TR::CFG * cfg = comp()->getFlowGraph();
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      _rednSetInfo[nextNode->getNumber()]->reset(nextOptimalComputation);
      _optSetInfo[nextNode->getNumber()]->reset(nextOptimalComputation);
      }
   _newSymbols[nextOptimalComputation] = NULL;
   _newSymbolsMap[nextOptimalComputation] = -1;
   _newSymbolReferences[nextOptimalComputation] = NULL;
   }



// Routine used in duplicating nodes for optimal placement; dulpicates respecting
// visitCounts. Unlike TR::Node::duplicateTree()
//
TR::Node *TR_PartialRedundancy::duplicateExact(TR::Node *node, List<TR::Node> *seenNodes, List<TR::Node> *duplicateNodes, vcount_t visitCount)
   {
   node->setVisitCount(visitCount);

   TR::Node *newRoot = TR::Node::copy(node);
   if (node->getOpCode().hasSymbolReference())
      newRoot->setSymbolReference(node->getSymbolReference());
   seenNodes->add(node);
   duplicateNodes->add(newRoot);

   newRoot->setReferenceCount(1);

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      if (node->getChild(i)->getVisitCount() != visitCount)
         newRoot->setChild(i, duplicateExact(node->getChild(i), seenNodes, duplicateNodes, visitCount));
      else
         {
         // We've seen this node before - find its duplicate
         //
         ListIterator<TR::Node> seenNodesIt(seenNodes);
         ListIterator<TR::Node> duplicateNodesIt(duplicateNodes);
         TR::Node *nextDuplicateNode = duplicateNodesIt.getFirst();
         TR::Node *nextNode;
         for (nextNode = seenNodesIt.getFirst(); nextNode != NULL; nextNode = seenNodesIt.getNext())
            {
            if (nextNode == node->getChild(i))
               {
               nextDuplicateNode->incReferenceCount();
               newRoot->setChild(i, nextDuplicateNode);
               }
            nextDuplicateNode = duplicateNodesIt.getNext();
            }
         }
      }

   return newRoot;
   }


void TR_PartialRedundancy::calculateGlobalRegisterWeightsBasedOnStructure(TR_Structure *structure, int32_t *currentWeight)
   {
   structure->calculateFrequencyOfExecution(currentWeight);
   }


bool TR_PartialRedundancy::isNodeAnImplicitNoOp(TR::Node *node)
   {
   // The following allows strider to introduce fewer induction variables
   // Applies to WCode only since address expressions are not commoned in Java
   if (node->getOpCode().isArrayRef() &&
       node->getSecondChild()->getOpCode().isLoadConst() &&
       !comp()->cg()->isMaterialized(node->getSecondChild()))
      return true;

   if (ignoreNode(node))
      return true;

   if (TR::ILOpCode::isOpCodeAnImplicitNoOp(node->getOpCode())) //FIXME: disables 190546!!!
      return true;

   // loads and stores of structures that cannot fit into register
   if (!node->canEvaluate() &&
       (node->getOpCode().isLoad() || node->getOpCode().isStore()))
      return true;


   if (node->getType().isAggregate() && node->getSize() > 8)
      {
      // if (trace())
      //   traceMsg(comp(),"skipping placing aggr %s (%p)\n",node->getOpCode().getName(),node);
      return true;
      }


   if (TR::TransformUtil::isNoopConversion(comp(), node))
      return true;

#ifdef J9_PROJECT_SPECIFIC
   if (!TR::Compiler->cls.romClassObjectsMayBeCollected() &&
       node->getOpCode().hasSymbolReference() &&
       ((node->getSymbolReference() == comp()->getSymRefTab()->findArrayClassRomPtrSymbolRef()) ||
        (node->getSymbolReference() == comp()->getSymRefTab()->findClassRomPtrSymbolRef())))
      return true;
#endif

   return false;
   }


// Some opcodes don't really generate any code; for these kinds of
// expressions it is wasteful to introduce a temp.
//
bool TR_PartialRedundancy::isOpCodeAnImplicitNoOp(TR::ILOpCode &opCode)
   {
   return comp()->cg()->opCodeIsNoOp(opCode);
   }


// Sign state does not have to be tracked on BCD nodes. This can lead to a BCD node with a particular
// sign setting to be stored to a temp and then the load of this temp will lose this sign info and
// subsequent generated code may clobber an unsigned sign code (0xf -> 0xc for example)
// pdSetSignA sign=?
//    x
// ...
// pdSetSignB
//    x
// After PRE:
// pdstore <temp1>
//    pdSetSignA sign=?
// ...
// pdload <temp1> sign=?
//
// To prevent this mark the pdload (reused from the original pdSetSignB) as containing some sign state info
// so the code generator will conservatively not alter this sign code.
void TR_PartialRedundancy::processReusedNode(TR::Node *node, TR::ILOpCodes newOpCode, TR::SymbolReference *newSymRef,
      int newNumChildren)
   {
#ifdef J9_PROJECT_SPECIFIC
   bool wasBCDNonLoad = node->getType().isBCD() && !node->getOpCode().isLoad();
#endif

   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"reusing %s (%p) as op ",node->getOpCode().getName(),node);

   node->setNumChildren(newNumChildren);
   if (newSymRef)
      node = TR::Node::recreateWithSymRef(node, newOpCode, newSymRef);
   else
      node = TR::Node::recreate(node, newOpCode);

   if(node->getOpCode().isLoadVarDirect()) node->setIsNodeCreatedByPRE();

   if (comp()->cg()->traceBCDCodeGen()) traceMsg(comp(),"%s",node->getOpCode().getName());

#ifdef J9_PROJECT_SPECIFIC
   if (wasBCDNonLoad && node->getOpCode().isBCDLoad())
      {
      node->setHasSignStateOnLoad(true);
      if (comp()->cg()->traceBCDCodeGen()) traceMsg(comp()," and setting hasSignState flag to true\n");
      }
   else
      {
      if (comp()->cg()->traceBCDCodeGen()) traceMsg(comp(),"\n");
      }
#endif
   }



TR::TreeTop *TR_PartialRedundancy::placeComputationsOptimally(TR::Block *block, TR::Node ***supportedNodesAsArray)
   {
   if (_optSetInfo[block->getStructureOf()->getNumber()]->isEmpty())
      return NULL;

   TR::TreeTop *placeToInsertOptimalComputations = block->getEntry();
   TR::TreeTop *placeToInsertUnanticipatableOptimalComputations = NULL;

   if (trace())
      traceMsg(comp(), "Placing computations optimally in block number %d\n", block->getStructureOf()->getNumber());

   ContainerType *anticipatabilityInfo = _isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getDownwardExposedAnalysisInfo(block->getNumber());
   int32_t numOptimalElements =_optSetInfo[block->getNumber()]->elementCount();
   int32_t seenOptimalElements = 0;
   int32_t currOptimalElement = 0;
   int32_t *orderedOptNumbersList = _orderedOptNumbersList[block->getNumber()];
   _prevTree = NULL;
   int32_t nextOptimalComputation;
   bool seenFirstOptimalCheck = false;
   for (nextOptimalComputation = orderedOptNumbersList[currOptimalElement]; seenOptimalElements < numOptimalElements; currOptimalElement++)
      {
      nextOptimalComputation = orderedOptNumbersList[currOptimalElement];
      if (trace())
         traceMsg(comp(), "Optimal computation %d in block number %d insert after %p\n", nextOptimalComputation, block->getStructureOf()->getNumber(), placeToInsertOptimalComputations->getNode());

      if (_optSetInfo[block->getNumber()]->get(nextOptimalComputation))
         {
         seenOptimalElements++;

         // Bit 0 is just a dummy placeholder
         //
         if (nextOptimalComputation == 0)
            continue;
         }
      else
         continue;

      // Its a local variable/parm load, in which case nothing to do
      //
      TR::Node *nextOptimalNode = (*supportedNodesAsArray)[nextOptimalComputation];
      if (
          (nextOptimalNode->getOpCode().isLoadVarDirect() &&
           !nextOptimalNode->getSymbol()->isStatic() &&
           !nextOptimalNode->getSymbol()->isMethodMetaData())
            ||
          (ignoreNode(nextOptimalNode))
          )
         continue;

      if (nextOptimalNode->getOpCode().isCheck())
         {
         if (!seenFirstOptimalCheck)
            placeToInsertOptimalComputations = block->getEntry();
         seenFirstOptimalCheck = true;
         }

      bool changeUnanticipatableOptimalComputations = false;

      // If this expression is anticipatable in this block
      // i.e. it is evaluated in this block in original program,
      // then find the evaluation point
      //
      if (anticipatabilityInfo->get(nextOptimalComputation))
         {
         if (!nextOptimalNode->getOpCode().isCheck())
            comp()->incOrResetVisitCount();

         TR::TreeTop *lastInsertionMade = NULL, *notLastInsertionInBlock = NULL;
         bool lastInsertionCanBeRemoved = false;
         TR::TreeTop *exitTree = block->getExit();
         //if (placeToInsertOptimalComputations == block->getExit())
            placeToInsertOptimalComputations = block->getEntry();

         TR::TreeTop *startingPoint = placeToInsertOptimalComputations;
         while (placeToInsertOptimalComputations != exitTree)
            {
            if (nextOptimalNode->getOpCode().isCheck())
               {
               TR::Node *nextNode = placeToInsertOptimalComputations->getNode();
               if (nextNode->getOpCode().isCheck())
                   {
                   if (nextNode->getLocalIndex() == nextOptimalComputation)
                      {
                      if (nextNode->getOpCode().isNullCheck())
                         {
                         if (nextNode->getFirstChild()->exceptionsRaised() != 0)
                            {
                            TR::TreeTop *prevTreeTop = placeToInsertOptimalComputations->getPrevTreeTop();

                            //vcount_t visitCount1 = comp()->incVisitCount();
                            //List<TR::Node> seenNodes, duplicateNodes;
                            //TR::Node *duplicateOptimalNode = duplicateExact(nextOptimalNode->getNullCheckReference(), &seenNodes, &duplicateNodes, visitCount1);

                            //duplicateOptimalNode->setReferenceCount(0);
                            TR::SymbolReference *newSymRef = nextOptimalNode->getSymbolReference();
                            TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, nextNode->getNullCheckReference());
                            TR::Node *nodeInTreeTop = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passThroughNode, newSymRef);
                            TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), nodeInTreeTop);
                            nodeInTreeTop->setLocalIndex(nextOptimalNode->getLocalIndex());

                            if (!shouldAllowLimitingOfOptimalPlacement(comp()) || performTransformation(comp(), "%sPlacing NULLCHK optimally : %p\n", OPT_DETAILS, nodeInTreeTop))
                               {
                               TR::TreeTop *translateTT = NULL;
                               if (comp()->useCompressedPointers())
                                  {
                                  TR::Node *reference = passThroughNode->getFirstChild();
                                  if (reference->getOpCode().isLoadIndirect() &&
                                       reference->getDataType() == TR::Address &&
                                       TR::TransformUtil::fieldShouldBeCompressed(reference, comp()))
                                     {
                                     translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(reference), NULL, NULL);
                                    }
                                 }
                               prevTreeTop->join(duplicateOptimalTree);
                               // if comp()->useCompressedPointers()
                               // place the translateTT after the nullchk
                               //
                               if (translateTT)
                                  {
                                  duplicateOptimalTree->join(translateTT);
                                  translateTT->join(placeToInsertOptimalComputations);
                                  }
                               else
                                  duplicateOptimalTree->join(placeToInsertOptimalComputations);
                               placeToInsertOptimalComputations = duplicateOptimalTree;

                               manager()->setAlteredCode(true);
                               }
                            else
                               nodeInTreeTop->getFirstChild()->recursivelyDecReferenceCount();
                            }
                         }
                      break;
                      }
                   }
                }
             else if (!isNodeAnImplicitNoOp(nextOptimalNode))
                {
                bool rhsOfStore = false;
                TR::Node *optimalNode = getAlreadyPresentOptimalNode(placeToInsertOptimalComputations->getNode(), nextOptimalComputation, comp()->getVisitCount(), rhsOfStore);


               // don't create stores for a void type
               if (optimalNode && optimalNode->getDataType() == TR::NoType)
                  continue;

                if (optimalNode &&
                    ((optimalNode->getLocalIndex() != nextOptimalComputation) ||
                     rhsOfStore ||
                     !lastInsertionMade))
                   {
                   //
                   // Insert a store if none has been inserted already;
                   // if a store has been inserted already then only insert a store
                   // for privatization of global stores (for whom the optimalNode returned
                   // will have a different index than nextOptimalComputation
                   //
                   TR::SymbolReference *newSymbolReference = _newSymbolReferences[nextOptimalComputation];

                   if (!shouldAllowLimitingOfOptimalPlacement(comp()) || performTransformation(comp(), "%s0Placing %s optimally : %p using symRef #%d\n", OPT_DETAILS, optimalNode->getOpCode().getName(), optimalNode, newSymbolReference->getReferenceNumber()))
                      {
                      _useAliasSetsNotGuaranteedToBeCorrect = true;

                      TR::Node *convertedOptimalNode = optimalNode;
                      if (fe()->dataTypeForLoadOrStore(nextOptimalNode->getDataType()) != nextOptimalNode->getDataType())
                         {
                         TR::ILOpCodes conversionOpCode =
                            TR::ILOpCode::getProperConversion(nextOptimalNode->getDataType(),
                        				     fe()->dataTypeForLoadOrStore(nextOptimalNode->getDataType()),
                                                             false /* !wantZeroExtension */);
                         convertedOptimalNode = TR::Node::create(conversionOpCode, 1, optimalNode);
                         }

                      TR::TreeTop *translateTT = NULL;
                      if (comp()->useCompressedPointers())
                         {
                         if (convertedOptimalNode->getOpCode().isLoadIndirect() &&
                              convertedOptimalNode->getDataType() == TR::Address &&
                              TR::TransformUtil::fieldShouldBeCompressed(convertedOptimalNode, comp()))
                            {
                            translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(convertedOptimalNode), NULL, NULL);
                            }
                         }

                      TR::Node *storeForCommonedNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(nextOptimalNode->getDataType()), 1, 1, convertedOptimalNode, newSymbolReference);

#ifdef J9_PROJECT_SPECIFIC
                      correctDecimalPrecision(storeForCommonedNode, convertedOptimalNode, comp());
#endif

                      TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), storeForCommonedNode);

                      //dumpOptDetails(comp(), "%s0Placing %s optimally : %p using symRef #%d\n", OPT_DETAILS, optimalNode->getOpCode().getName(), optimalNode, newSymbolReference->getReferenceNumber());

                      manager()->setAlteredCode(true);

                      TR::Node *placeHolderNode = placeToInsertOptimalComputations->getNode();
                      if (placeHolderNode->getOpCode().isResolveOrNullCheck() ||
                          (placeHolderNode->getOpCodeValue() == TR::treetop))
                         placeHolderNode = placeHolderNode->getFirstChild();

                      TR::ILOpCode &placeHolderOpCode = placeHolderNode->getOpCode();
                      if (placeHolderOpCode.isBranch() ||
                          placeHolderOpCode.isJumpWithMultipleTargets() ||
                          placeHolderOpCode.isReturn() ||
                          placeHolderOpCode.getOpCodeValue() == TR::athrow /* ||
                          ((placeToInsertOptimalComputations->getNode()->getOpCode().isResolveOrNullCheck()) &&
                          (placeHolderNode->exceptionsRaised() != 0)) */)
                         {
                         TR::TreeTop *prevTreeInBlock = placeToInsertOptimalComputations->getPrevTreeTop();
                         // if comp()->useCompressedPointers()
                         if (translateTT)
                            {
                            prevTreeInBlock->join(translateTT);
                            translateTT->join(placeToInsertOptimalComputations);
                            prevTreeInBlock = translateTT;
                            }
                         prevTreeInBlock->join(duplicateOptimalTree);
                         duplicateOptimalTree->join(placeToInsertOptimalComputations);
                         placeToInsertOptimalComputations = translateTT ? translateTT : duplicateOptimalTree;
                         _prevTree = translateTT ? translateTT : duplicateOptimalTree;
                         }
                      else
                         {
                         TR::TreeTop *nextTreeInBlock = placeToInsertOptimalComputations->getNextTreeTop();
                         // if comp()->useCompressedPointers()
                         if (translateTT)
                            {
                            placeToInsertOptimalComputations->join(translateTT);
                            translateTT->join(nextTreeInBlock);
                            placeToInsertOptimalComputations = translateTT;
                            }
                         placeToInsertOptimalComputations->join(duplicateOptimalTree);
                         duplicateOptimalTree->join(nextTreeInBlock);
                         placeToInsertOptimalComputations = duplicateOptimalTree;
                         _prevTree = duplicateOptimalTree;
                         }

                      if (notLastInsertionInBlock)
                         TR::Node::recreate(notLastInsertionInBlock->getNode(), TR::treetop);
                      lastInsertionMade = placeToInsertOptimalComputations;
                      if (optimalNode->getLocalIndex() != nextOptimalComputation)
                         lastInsertionCanBeRemoved = false;
                      else
                         lastInsertionCanBeRemoved = true;
                      notLastInsertionInBlock = lastInsertionMade;
                      //break;
                      }
                   }
                }

            placeToInsertOptimalComputations = placeToInsertOptimalComputations->getNextRealTreeTop();
            if (placeToInsertOptimalComputations == startingPoint)
               break;

            if (placeToInsertOptimalComputations == block->getExit())
               {
               if (startingPoint == block->getEntry())
                  break;

               if (!lastInsertionCanBeRemoved)
                  notLastInsertionInBlock = NULL;
               placeToInsertOptimalComputations = block->getEntry();
               if (!nextOptimalNode->getOpCode().isCheck())
                  comp()->incOrResetVisitCount();
               }
            } // end of while (placeToInsertOptimalComputations != exitTree)

         if (lastInsertionMade)
           {
           placeToInsertOptimalComputations = lastInsertionMade;
           placeToInsertUnanticipatableOptimalComputations = NULL;
           }

         continue;
         }
      else
         {
         changeUnanticipatableOptimalComputations = true;
         if (!placeToInsertUnanticipatableOptimalComputations)
            placeToInsertOptimalComputations = block->getExit();
         else
            placeToInsertOptimalComputations = placeToInsertUnanticipatableOptimalComputations;
         }

      if (placeToInsertOptimalComputations == block->getExit())
         {
         placeToInsertOptimalComputations = block->getLastRealTreeTop();
         TR::Node *relevantNode = placeToInsertOptimalComputations->getNode();

         if (relevantNode->getOpCode().isResolveOrNullCheck() ||
             (relevantNode->getOpCodeValue() == TR::treetop))
            relevantNode = relevantNode->getFirstChild();

         TR::ILOpCode &placeHolderOpCode = relevantNode->getOpCode();
         if (placeHolderOpCode.isBranch() ||
             placeHolderOpCode.isJumpWithMultipleTargets() ||
             placeHolderOpCode.isReturn() ||
             placeHolderOpCode.getOpCodeValue() == TR::athrow)
            placeToInsertOptimalComputations = placeToInsertOptimalComputations->getPrevTreeTop();

         }

      if (nextOptimalNode->getOpCode().isStore())
         {
         // An entire store (not just the value on the RHS can be commoned);
         // not enabled yet
         //
         ////List<TR::Node> additionalTreeTopsToBeDuplicated;
         ////TR::Node *duplicateOptimalNode = nextOptimalNode->duplicateTree();
         ////TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), duplicateOptimalNode, NULL, NULL);
         ////dumpOptDetails(comp(), "%sPlacing store optimally : %p\n", OPT_DETAILS, duplicateOptimalNode);
         ////setAlteredCode(true);
         ////duplicateOptimalTree->getNode()->setLocalIndex(nextOptimalNode->getLocalIndex());
         ////TR::TreeTop *secondTreeInBlock = placeToInsertOptimalComputations->getNextTreeTop();
         ////placeToInsertOptimalComputations->join(duplicateOptimalTree);
         ////duplicateOptimalTree->join(secondTreeInBlock);
         ////placeToInsertOptimalComputations = duplicateOptimalTree;
         }
      else if (nextOptimalNode->getOpCode().isCheck() || (nextOptimalNode->getOpCodeValue() == TR::treetop))
         {
         // We test for both a check node and a treetop because if the
         // node that we have chosen as the representative for a given
         // expression is actually transformed (made into a treetop in the
         // case of a NULLCHK) because it was redundant in its block, then
         // we don't have the opcode we require (treetop instead of NULLCHK)
         // when we reach this optimal block. So we need to be careful and
         // insert a NULLCHK and not a treetop as our optimal node.
         //
         if ((nextOptimalNode->getOpCodeValue() == TR::NULLCHK) || (nextOptimalNode->getOpCodeValue() == TR::treetop))
            {
            // Placing a NULLCHK optimally is not similar to placing other
            // checks optimally. We need to have a PassThrough node in this
            // case whereas its a simple duplication for BNDCHK etc.
            //
            bool isOriginallyTreeTop = false;
            if (nextOptimalNode->getOpCodeValue() == TR::treetop)
               isOriginallyTreeTop = true;

            if (isOriginallyTreeTop)
               TR::Node::recreate(nextOptimalNode, TR::NULLCHK);

            vcount_t visitCount1 = comp()->incOrResetVisitCount();
            TR_ScratchList<TR::Node> seenNodes(trMemory()), duplicateNodes(trMemory());
            TR::Node *duplicateOptimalNode = duplicateExact(nextOptimalNode->getNullCheckReference(), &seenNodes, &duplicateNodes, visitCount1);

            bool replacedNullCheckReference = false;

            // This piece of code replaces any subtrees that are available
            // in temps by the load of the temp before placing the computation
            // optimally.
            //
            TR::ILOpCode &nullCheckReferenceOpCode = duplicateOptimalNode->getOpCode();
            TR::DataType nullCheckReferenceDataType = duplicateOptimalNode->getDataType();

            TR::Node *nullCheckReferenceNode = nextOptimalNode->getNullCheckReference();
            if (isSupportedOpCode(nullCheckReferenceNode, NULL) && !nullCheckReferenceOpCode.isLoadVarDirect())
               {
               if (((nullCheckReferenceNode->getLocalIndex() != MAX_SCOUNT) && (nullCheckReferenceNode->getLocalIndex() != 0)))
                  {
                  if ((_newSymbolsMap[nullCheckReferenceNode->getLocalIndex()] > -1) &&
                     (((!_unavailableSetInfo[block->getNumber()]->get(nullCheckReferenceNode->getLocalIndex())) &&
                        (_rednSetInfo[block->getNumber()]->get(nullCheckReferenceNode->getLocalIndex()) || _optSetInfo[block->getNumber()]->get(nullCheckReferenceNode->getLocalIndex()))) ||
                       (comp()->requiresSpineChecks() && nullCheckReferenceNode->getOpCode().hasSymbolReference() && nullCheckReferenceNode->getSymbol()->isArrayShadowSymbol())))
                     {
                     replacedNullCheckReference = true;
                     TR::SymbolReference *newSymbolReference = _newSymbolReferences[nullCheckReferenceNode->getLocalIndex()];
                     TR::Node *newLoad = NULL;
                     newLoad = TR::Node::createWithSymRef(nullCheckReferenceNode, comp()->il.opCodeForDirectLoad(nullCheckReferenceDataType), 0, newSymbolReference);
                     newLoad->setLocalIndex(-1);

                     if (trace())
                        traceMsg(comp(), "Duplicate null check had its new null check reference %p replaced by %p with symRef #%d\n", duplicateOptimalNode, newLoad, newLoad->getSymbolReference()->getReferenceNumber());
                     duplicateOptimalNode = newLoad;
                     }
                  }
               }

            if (!replacedNullCheckReference)
               {
               if (trace())
                  traceMsg(comp(), "Trying to replace OPTIMAL SUBNODES in : %p\n", nextOptimalNode);
               int32_t k;
               for (k=0;k<nextOptimalNode->getNullCheckReference()->getNumChildren();k++)
                  {
                  vcount_t visitCount4 = comp()->incVisitCount();
                  placeToInsertOptimalComputations = replaceOptimalSubNodes(placeToInsertOptimalComputations, nextOptimalNode->getNullCheckReference(), nextOptimalNode->getNullCheckReference()->getChild(k), k, duplicateOptimalNode, duplicateOptimalNode->getChild(k), false, block->getNumber(), visitCount4);
                  }
               }

            duplicateOptimalNode->setReferenceCount(0);
            if (isOriginallyTreeTop)
               TR::Node::recreate(nextOptimalNode, TR::treetop);

            TR::SymbolReference *newSymRef = nextOptimalNode->getSymbolReference();
            TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, duplicateOptimalNode);
            TR::Node *nodeInTreeTop = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passThroughNode, newSymRef);
            TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), nodeInTreeTop);
            nodeInTreeTop->setLocalIndex(nextOptimalNode->getLocalIndex());

            if (!shouldAllowLimitingOfOptimalPlacement(comp()) || performTransformation(comp(), "%sPlacing NULLCHK optimally : %p\n", OPT_DETAILS, duplicateOptimalNode))
               {
               TR::TreeTop *translateTT = NULL;
               if (comp()->useCompressedPointers())
                  {
                  TR::Node *reference = passThroughNode->getFirstChild();
                  if (reference->getOpCode().isLoadIndirect() &&
                        reference->getDataType() == TR::Address &&
                        TR::TransformUtil::fieldShouldBeCompressed(reference, comp()))
                     {
                     translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(reference), NULL, NULL);
                     }
                  }
               manager()->setAlteredCode(true);
               TR::TreeTop *secondTreeInBlock = placeToInsertOptimalComputations->getNextTreeTop();
               placeToInsertOptimalComputations->join(duplicateOptimalTree);
               duplicateOptimalTree->join(secondTreeInBlock);
               // if comp()->useCompressedPointers()
               // place the translateTT after the nullchk
               //
               if (translateTT)
                  {
                  duplicateOptimalTree->join(translateTT);
                  translateTT->join(secondTreeInBlock);
                  placeToInsertOptimalComputations = translateTT;
                  }
               else
                  placeToInsertOptimalComputations = duplicateOptimalTree;
               }
            else
               nodeInTreeTop->getFirstChild()->recursivelyDecReferenceCount();
            }
         else
            {
            // Its a check but not a NULLCHK
            //
            vcount_t visitCount1 = comp()->incOrResetVisitCount();
            TR_ScratchList<TR::Node> seenNodes(trMemory()), duplicateNodes(trMemory());
            TR::Node *duplicateOptimalNode = duplicateExact(nextOptimalNode, &seenNodes, &duplicateNodes, visitCount1);

            // This piece of code replaces any subtrees that are available
            // in temps by the load of the temp before placing the computation
            // optimally.
            //
            // Cannot do this naively as the check's optimal placement now PRECEDES
            // ALL the subnodes' optimal placement; therefore the temp for a subnode
            // might only be placed (created) AFTER the check's optimal placement
            //
            if (trace())
                traceMsg(comp(), "Trying to replace OPTIMAL SUBNODES in : %p\n", nextOptimalNode);

            int32_t k;
            for (k=0;k<nextOptimalNode->getNumChildren();k++)
               {
               vcount_t visitCount4 = comp()->incVisitCount();
               placeToInsertOptimalComputations = replaceOptimalSubNodes(placeToInsertOptimalComputations, nextOptimalNode, nextOptimalNode->getChild(k), k, duplicateOptimalNode, duplicateOptimalNode->getChild(k), false, block->getNumber(), visitCount4);
               }

            duplicateOptimalNode->setReferenceCount(0);
            TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), duplicateOptimalNode, NULL, NULL);
            duplicateOptimalNode->setLocalIndex(nextOptimalNode->getLocalIndex());

            if (!shouldAllowLimitingOfOptimalPlacement(comp()) || performTransformation(comp(), "%sPlacing %s optimally : %p\n", OPT_DETAILS, duplicateOptimalTree->getNode()->getOpCode().getName(), duplicateOptimalTree->getNode()))
               {
               TR::TreeTop *translateTT = NULL;
               if (comp()->useCompressedPointers())
                  {
                  TR::Node *reference = duplicateOptimalTree->getNode();
                  if (reference->getOpCode().isLoadIndirect() &&
                         reference->getDataType() == TR::Address &&
                         TR::TransformUtil::fieldShouldBeCompressed(reference, comp()))
                     {
                     translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(reference), NULL, NULL);
                     }
                  }
               TR::TreeTop *secondTreeInBlock = placeToInsertOptimalComputations->getNextTreeTop();
               // if comp()->useCompressedPointers()
               //
               if (translateTT)
                  {
                  placeToInsertOptimalComputations->join(translateTT);
                  translateTT->join(secondTreeInBlock);
                  placeToInsertOptimalComputations = translateTT;
                  }
               manager()->setAlteredCode(true);
               placeToInsertOptimalComputations->join(duplicateOptimalTree);
               duplicateOptimalTree->join(secondTreeInBlock);
               placeToInsertOptimalComputations = duplicateOptimalTree;
               }
            else
               duplicateOptimalNode->getFirstChild()->recursivelyDecReferenceCount();
            }
         }
      else if (isSupportedOpCode(nextOptimalNode, NULL) &&
               (!isNodeAnImplicitNoOp(nextOptimalNode)))
         {
         //
         // Its neither a store nor a check; its some computation that
         // needs a temp.
         //
         _useAliasSetsNotGuaranteedToBeCorrect = true;

         TR_ScratchList<TR::Node> seenNodes(trMemory()), duplicateNodes(trMemory());
         vcount_t visitCount2 = comp()->incOrResetVisitCount();
         TR::Node *duplicateOptimalNode = duplicateExact(nextOptimalNode, &seenNodes, &duplicateNodes, visitCount2);

         // This piece of code replaces any subtrees that are available
         // in temps by the load of the temp before placing the computation
         // optimally.
         //
         if (trace())
            traceMsg(comp(), "Trying to replace OPTIMAL SUBNODES in : %p\n", nextOptimalNode);

         int32_t k;
         for (k=0;k<nextOptimalNode->getNumChildren();k++)
            {
            vcount_t visitCount4 = comp()->incVisitCount();
            placeToInsertOptimalComputations = replaceOptimalSubNodes(placeToInsertOptimalComputations, nextOptimalNode, nextOptimalNode->getChild(k), k, duplicateOptimalNode, duplicateOptimalNode->getChild(k), false, block->getNumber(), visitCount4);
            }

         TR::SymbolReference *newSymbolReference = _newSymbolReferences[nextOptimalComputation];
         TR::Node *convertedOptimalNode = duplicateOptimalNode;
         if (fe()->dataTypeForLoadOrStore(duplicateOptimalNode->getDataType()) != duplicateOptimalNode->getDataType())
            {
            TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(duplicateOptimalNode->getDataType(), fe()->dataTypeForLoadOrStore(duplicateOptimalNode->getDataType()), false /* !wantZeroExtension */);
            duplicateOptimalNode->setReferenceCount(0);
            convertedOptimalNode = TR::Node::create(conversionOpCode, 1, duplicateOptimalNode);
            }

         TR::DataType type = nextOptimalNode->getDataType();
         TR::ILOpCodes storeOp = type == TR::NoType ? TR::treetop : comp()->il.opCodeForDirectStore(type);

         TR::Node *storeForCommonedNode = TR::Node::createWithSymRef(storeOp, 1, 1, convertedOptimalNode, newSymbolReference);
         convertedOptimalNode->setReferenceCount(1);
#ifdef J9_PROJECT_SPECIFIC
         correctDecimalPrecision(storeForCommonedNode, convertedOptimalNode, comp());
#endif

         TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), storeForCommonedNode);

         dumpOptDetails(comp(), "%s1Placing %s optimally : %p using symRef #%d\n", OPT_DETAILS, convertedOptimalNode->getOpCode().getName(), convertedOptimalNode, newSymbolReference->getReferenceNumber());


         manager()->setAlteredCode(true);

         convertedOptimalNode->setLocalIndex(nextOptimalNode->getLocalIndex());

         TR::TreeTop *secondTreeInBlock = placeToInsertOptimalComputations->getNextTreeTop();

         TR::TreeTop *translateTT = NULL;
         if (comp()->useCompressedPointers())
            {
            TR::Node *reference = convertedOptimalNode;
            if (reference->getOpCode().isLoadIndirect() ||
                  reference->getOpCode().isArrayLength())
               {
               if (reference->getFirstChild()->getOpCode().isLoadIndirect() &&
                           reference->getFirstChild()->getDataType() == TR::Address &&
                           TR::TransformUtil::fieldShouldBeCompressed(reference->getFirstChild(), comp()))
                  {
                  translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(reference->getFirstChild()), NULL, NULL);
                  placeToInsertOptimalComputations->join(translateTT);
                  translateTT->join(secondTreeInBlock);
                  placeToInsertOptimalComputations = translateTT;
                  }

               if (reference->getDataType() == TR::Address &&
                      TR::TransformUtil::fieldShouldBeCompressed(reference, comp()))
                  {
                  translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(reference), NULL, NULL);
                  placeToInsertOptimalComputations->join(translateTT);
                  translateTT->join(secondTreeInBlock);
                  placeToInsertOptimalComputations = translateTT;
                  }
               }
            }

         placeToInsertOptimalComputations->join(duplicateOptimalTree);
         duplicateOptimalTree->join(secondTreeInBlock);

         if (duplicateOptimalNode->getOpCode().isCall() &&
             !duplicateOptimalNode->getSymbolReference()->isUnresolved() &&
             duplicateOptimalNode->getSymbol()->castToMethodSymbol()->isPureFunction())
            {
            TR::TreeTop *anchorTT = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, duplicateOptimalNode), NULL, NULL);
            placeToInsertOptimalComputations->join(anchorTT);
            anchorTT->join(duplicateOptimalTree);
            }

         placeToInsertOptimalComputations = duplicateOptimalTree;
         _prevTree = duplicateOptimalTree;
         }

      if (changeUnanticipatableOptimalComputations)
         placeToInsertUnanticipatableOptimalComputations = placeToInsertOptimalComputations;
      }

   return placeToInsertOptimalComputations->getNextTreeTop();
   }



// Find a node having a given PRE index in the subtree; return NULL if
// not found
//
TR::Node *TR_PartialRedundancy::getAlreadyPresentOptimalNode(TR::Node *node, int32_t nextOptimalComputation, vcount_t visitCount, bool &rhsOfStore)
   {
   if (node->getVisitCount() == visitCount)
      return NULL;

   node->setVisitCount(visitCount);

   if (node->getLocalIndex() == nextOptimalComputation)
      {
      if (node->getOpCode().isStoreIndirect())
         {
         rhsOfStore = true;
         return node->getSecondChild();
         }
      else if (node->getOpCode().isStore())
        {
        rhsOfStore = true;
        return node->getFirstChild();
        }
      return node;
      }

   int32_t childNum;
   for (childNum=0;childNum<node->getNumChildren();childNum++)
      {
      TR::Node *child = getAlreadyPresentOptimalNode(node->getChild(childNum), nextOptimalComputation, visitCount, rhsOfStore);
      if (child)
         return child;
      }

   return NULL;
   }



static bool isExpressionRedundant(TR::Node *node, TR_PartialRedundancy::ContainerType *redundantComputations, TR_PartialRedundancy::ContainerType *anticipatable)
   {
    int32_t preIndex2 = 1000000;
    static char *c1 = feGetEnv("TR_PreIndex2");
    if (c1)
       preIndex2 = atoi(c1);

   if (redundantComputations &&
       node->getLocalIndex() != MAX_SCOUNT &&
       node->getLocalIndex() != 0 &&
       redundantComputations->get(node->getLocalIndex()) &&
       (node->getOpCode().isStore() || anticipatable->get(node->getLocalIndex())))
     {
     if (node->getLocalIndex() < preIndex2)
        return true;
     }
   return false;
   }


// Eliminates redundant computations in specified block.
//
void TR_PartialRedundancy::eliminateRedundantComputations(TR::Block *block, TR::Node **supportedNodesAsArray, ContainerType **rednSetInfo, TR::TreeTop *startTree)
   {
   if (!block->isExtensionOfPreviousBlock())
      {
      // Each block uses up 2 visit counts
      comp()->incVisitCount();
       _visitCount = comp()->incOrResetVisitCount();
      }

   vcount_t visitCount = _visitCount;

   if (trace())
      traceMsg(comp(), "Eliminating redundant computations in block number %d visit count %d\n", block->getStructureOf()->getNumber(), visitCount);

   _profilingWalk = true;
   bool walkedTreesAtLeastOnce = false;

   ContainerType *redundantComputations = rednSetInfo[block->getStructureOf()->getNumber()];
   ContainerType *anticipatabilityInfo = _isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getAnalysisInfo(block->getNumber());

      {
      walkedTreesAtLeastOnce = false;
      TR::TreeTop *currentTree = startTree;
      TR::TreeTop *exitTree = block->getExit();
      bool shouldHaveRedundantComputations = !(block->isCold());
#if defined(ENABLE_GPU)
      if (comp()->getOptions()->getEnableGPU(TR_EnableGPU))
         {
         TR_BlockStructure *blockStructure = block->getStructureOf();
         if (blockStructure)
            {
            if (blockStructure->isLoopInvariantBlock())
               shouldHaveRedundantComputations = true;
            else
               {
               TR_Structure *parent = blockStructure->getParent();
               while (parent)
                  {
                  TR_RegionStructure *region = parent->asRegion();
                  TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();
                  if (region->isNaturalLoop() && piv)
                     {
                     TR::Block *branchBlock = piv->getBranchBlock();
                     TR::Node *node = branchBlock->getLastRealTreeTop()->getNode();
                     int16_t callerIndex = node->getInlinedSiteIndex();
                     TR_ResolvedMethod *method = NULL;
                     if (callerIndex != -1)
                        do
                           {
                           method = comp()->getInlinedResolvedMethod(callerIndex);
                           if (method->getRecognizedMethod() == TR::java_util_stream_IntPipeline_forEach)
                              {
                              shouldHaveRedundantComputations = true;
                              break;
                              }
                           TR_InlinedCallSite site = comp()->getInlinedCallSite(callerIndex);
                           callerIndex = site._byteCodeInfo.getCallerIndex();
                           } while (callerIndex != -1);
                     else
                        {
                        method = comp()->getCurrentMethod();
                        if (method->getRecognizedMethod() == TR::java_util_stream_IntPipeline_forEach)
                           shouldHaveRedundantComputations = true;
                        }
                     if (method != NULL)
                        break;
                     }
                  parent = parent->getParent();
                  }
               }
            }
         }
#endif

      while (currentTree != exitTree)
         {
         TR::ILOpCode &firstOpCodeInTree = currentTree->getNode()->getOpCode();

         // Replace any expressions that can be commoned by
         // loads of temps in this subtree
         //
         eliminateRedundantSupportedNodes(NULL, currentTree->getNode(), false, currentTree, block->getNumber(), visitCount, shouldHaveRedundantComputations ? redundantComputations : NULL, anticipatabilityInfo, supportedNodesAsArray);


         // Now handle stores
         //
         // Stores could be either treetops themselves or be under a TR::treetop or a null check/resolve check
         //
         if ((firstOpCodeInTree.isStore() &&
              !currentTree->getNode()->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
             ((firstOpCodeInTree.isResolveOrNullCheck() ||
              (firstOpCodeInTree.getOpCodeValue() == TR::ArrayStoreCHK) ||
               firstOpCodeInTree.isAnchor() ||
               (firstOpCodeInTree.getOpCodeValue() == TR::treetop)) &&
              (currentTree->getNode()->getFirstChild()->getOpCode().isStore() &&
               !currentTree->getNode()->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm())))
            {
            // If this is a redundant expression
            //
            if (isExpressionRedundant(currentTree->getNode(), redundantComputations, anticipatabilityInfo))
               {
               scount_t nodeIndex = currentTree->getNode()->getLocalIndex();
               TR::Node *redundantNode = supportedNodesAsArray[nodeIndex];

               //
               // Handle the check first
               //
               if (!currentTree->getNode()->getOpCode().isStore())
                  {
                  if (0 && performTransformation(comp(), "%sEliminating redundant check computation (resolve or null check) : %p\n", OPT_DETAILS, currentTree->getNode())) // Look at check-in comment for version 1.296
                     {
                     if (firstOpCodeInTree.getOpCodeValue() == TR::NULLCHK)
                        TR::Node::recreate(currentTree->getNode(), TR::treetop);
                     else if (firstOpCodeInTree.getOpCodeValue() == TR::ResolveAndNULLCHK)
                        TR::Node::recreate(currentTree->getNode(), TR::ResolveCHK);

                     if (firstOpCodeInTree.getOpCodeValue() == TR::ResolveCHK)
                        TR::Node::recreate(currentTree->getNode(), TR::treetop);

                     currentTree->getNode()->setLocalIndex(-1);
                     currentTree->getNode()->setNumChildren(1);

                     //dumpOptDetails(comp(), "%sEliminating redundant check computation (resolve or null check) : %p\n", OPT_DETAILS, currentTree->getNode());
                     }
                  }
               else
                  {
                  TR::SymbolReference *newSymbolReference = _newSymbolReferences[nodeIndex];
                  if (newSymbolReference /* &&
		      performTransformation(comp(), "%sEliminating redundant computation (store) : %p\n", OPT_DETAILS, currentTree->getNode()) */)
                     {
                     // Eliminate the store which is a treetop itself
                     //
                     //printf("Privatized store %p in %s\n", currentTree->getNode(), signature(comp()->getCurrentMethod()));
                     TR::ILOpCodes opCodeValue = comp()->il.opCodeForDirectStore(redundantNode->getDataType());

                     TR::Node *storeForCommonedNode = NULL;
                     if (currentTree->getNode()->getOpCode().isStoreIndirect())
                        {
                        TR::Node *rhsOfStore = currentTree->getNode()->getSecondChild();
                        TR::Node *convertedOptimalNode = rhsOfStore;
                        if (fe()->dataTypeForLoadOrStore(rhsOfStore->getDataType()) != rhsOfStore->getDataType())
                           {
                           TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(rhsOfStore->getDataType(), fe()->dataTypeForLoadOrStore(rhsOfStore->getDataType()), false /* !wantZeroExtension */);
                           convertedOptimalNode = TR::Node::create(conversionOpCode, 1, rhsOfStore);
                           }

                        storeForCommonedNode = TR::Node::createWithSymRef(opCodeValue, 1, 1, convertedOptimalNode, newSymbolReference);
                        }
                     else
                        storeForCommonedNode = TR::Node::createWithSymRef(opCodeValue, 1, 1, currentTree->getNode()->getFirstChild(), newSymbolReference);
#ifdef J9_PROJECT_SPECIFIC
                     correctDecimalPrecision(storeForCommonedNode, storeForCommonedNode->getFirstChild(), comp());
#endif

                     TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), storeForCommonedNode);
                     TR::TreeTop *nextTree = currentTree->getNextTreeTop();

                     dumpOptDetails(comp(), "%sEliminating redundant computation (store) : %p\n", OPT_DETAILS, currentTree->getNode());

                     if (storeForCommonedNode->getDataType() != TR::Address)
                        optimizer()->setRequestOptimization(OMR::loopVersionerGroup, true);

                     storeForCommonedNode->setLocalIndex(-1);
                     currentTree->join(duplicateOptimalTree);
                     duplicateOptimalTree->join(nextTree);
                     currentTree = duplicateOptimalTree;
                     }
                  }
               manager()->setAlteredCode(true);
               }

            if ((currentTree->getNode()->getNumChildren() > 0) &&
                isExpressionRedundant(currentTree->getNode()->getFirstChild(), redundantComputations, anticipatabilityInfo) &&
                currentTree->getNode()->getFirstChild()->getOpCode().isStore())
              {
              scount_t nodeIndex = currentTree->getNode()->getFirstChild()->getLocalIndex();
              TR::Node *redundantNode = supportedNodesAsArray[nodeIndex];

              TR::SymbolReference *newSymbolReference = _newSymbolReferences[nodeIndex];
              if (newSymbolReference /* &&
		  performTransformation(comp(), "%sEliminating redundant computation (store) : %p\n", OPT_DETAILS, currentTree->getNode()->getFirstChild()) */)
                 {
                 //printf("Privatized store %p in %s\n", currentTree->getNode(), signature(comp()->getCurrentMethod()));
                 TR::ILOpCodes opCodeValue = comp()->il.opCodeForDirectStore(redundantNode->getDataType());

                 TR::Node *storeForCommonedNode = NULL;
                 if (currentTree->getNode()->getFirstChild()->getOpCode().isStoreIndirect())
                    {
                    TR::Node *rhsOfStore = currentTree->getNode()->getFirstChild()->getSecondChild();
                    TR::Node *convertedOptimalNode = rhsOfStore;
                    if (fe()->dataTypeForLoadOrStore(rhsOfStore->getDataType()) != rhsOfStore->getDataType())
                       {
                       TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(rhsOfStore->getDataType(), fe()->dataTypeForLoadOrStore(rhsOfStore->getDataType()), false /* !wantZeroExtension */);
                       convertedOptimalNode = TR::Node::create(conversionOpCode, 1, rhsOfStore);
                       }

                    storeForCommonedNode = TR::Node::createWithSymRef(opCodeValue, 1, 1, convertedOptimalNode, newSymbolReference);
                    }
                 else
                    storeForCommonedNode = TR::Node::createWithSymRef(opCodeValue, 1, 1, currentTree->getNode()->getFirstChild()->getFirstChild(), newSymbolReference);
#ifdef J9_PROJECT_SPECIFIC
                 correctDecimalPrecision(storeForCommonedNode, storeForCommonedNode->getFirstChild(), comp());
#endif

                 TR::TreeTop *duplicateOptimalTree = TR::TreeTop::create(comp(), storeForCommonedNode);
                 TR::TreeTop *nextTree = currentTree->getNextTreeTop();

                 dumpOptDetails(comp(), "%sEliminating redundant computation (store) : %p\n", OPT_DETAILS, currentTree->getNode()->getFirstChild());

                 if (storeForCommonedNode->getDataType() != TR::Address)
                    optimizer()->setRequestOptimization(OMR::loopVersionerGroup, true);

                 storeForCommonedNode->setLocalIndex(-1);
                 currentTree->join(duplicateOptimalTree);
                 duplicateOptimalTree->join(nextTree);
                 currentTree = duplicateOptimalTree;
                 manager()->setAlteredCode(true);
                 }
              }
            }
         else if (0 && firstOpCodeInTree.isCheck()) // Look at check-in comment for version 1.296
            {
            // There are no stores involved in this check
            //
            if (isExpressionRedundant(currentTree->getNode(), redundantComputations, anticipatabilityInfo))
               {
               if (firstOpCodeInTree.isResolveOrNullCheck())
                  {
                  if (performTransformation(comp(), "%sEliminating redundant check computation (resolve or null check) : %p\n", OPT_DETAILS, currentTree->getNode()))
                     {
                     if (firstOpCodeInTree.getOpCodeValue() == TR::NULLCHK)
                        TR::Node::recreate(currentTree->getNode(), TR::treetop);
                     else if (firstOpCodeInTree.getOpCodeValue() == TR::ResolveAndNULLCHK)
                        TR::Node::recreate(currentTree->getNode(), TR::ResolveCHK);

                    if (firstOpCodeInTree.getOpCodeValue() == TR::ResolveCHK)
                       TR::Node::recreate(currentTree->getNode(), TR::treetop);

                     currentTree->getNode()->setNumChildren(1);

                     //dumpOptDetails(comp(), "%sEliminating redundant check computation (resolve or null check) : %p\n", OPT_DETAILS, currentTree->getNode());
                     }
                  }
               else
                  {
                  if (performTransformation(comp(), "%sEliminating redundant check computation (%s) : %p\n", OPT_DETAILS, currentTree->getNode()->getOpCode().getName(), currentTree->getNode()))
                     {
                     // Eliminate other checks (e.g bound checks etc.)
                     //
                     TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
                     TR::TreeTop *nextTree = currentTree->getNextTreeTop();

                     vcount_t newVisitCount = comp()->incVisitCount();
                     TR_ScratchList<TR::Node> anchoredNodes(trMemory());
                     collectAllNodesToBeAnchored(&anchoredNodes, currentTree->getNode(), newVisitCount);
                     ListIterator<TR::Node> anchorIt(&anchoredNodes);
                     TR::Node *nextAnchor;
                     for (nextAnchor = anchorIt.getFirst(); nextAnchor != NULL; nextAnchor = anchorIt.getNext())
                        {
                        TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, nextAnchor));
                        TR::TreeTop *tempPrevTree = currentTree->getPrevTreeTop();
                        anchorTreeTop->join(currentTree);
                        tempPrevTree->join(anchorTreeTop);
                        prevTree = anchorTreeTop;
                        }

                     currentTree->getNode()->recursivelyDecReferenceCount();

                     ///dumpOptDetails(comp(), "%sEliminating redundant check computation (%s) : %p\n", OPT_DETAILS, currentTree->getNode()->getOpCode().getName(), currentTree->getNode());

                     prevTree->join(nextTree);
                     currentTree = prevTree;
                     }
                  }
               manager()->setAlteredCode(true);
               }
           }
         currentTree = currentTree->getNextTreeTop();
         }

      if (_exceptionCheckMotion)
         _profilingWalk = false;
      }

#if 0
   // No need to walk trees again since we already walked through all of them
   if (!walkedTreesAtLeastOnce)
      {
      TR::TreeTop *currentTree = startTree;
      TR::TreeTop *exitTree = block->getExit();
      vcount_t visitCount = comp()->incOrResetVisitCount();
      while (currentTree != exitTree)
         {
         TR::ILOpCode &firstOpCodeInTree = currentTree->getNode()->getOpCode();

         // Try to profile some exprs
         //
         if (!block->isCold())
            eliminateRedundantSupportedNodes(NULL, currentTree->getNode(), false, currentTree, block->getNumber(), visitCount, NULL, anticipatabilityInfo, supportedNodesAsArray);

         currentTree = currentTree->getNextTreeTop();
         }
      }
#endif

   }





void TR_PartialRedundancy::printTrees()
   {
   comp()->incVisitCount();

   TR::TreeTop *currentTree = comp()->getStartTree();

   while (!(currentTree == NULL))
      {
      if (trace())
         getDebug()->print(comp()->getOutFile(), currentTree);

      currentTree = currentTree->getNextTreeTop();
      }
   }




// All nodes having reference count > 1 must be placed under a
// treetop because if they are allowed to swing down to next use,
// program semantics may be altered (in case the symbol is written
// in that range)
//
void TR_PartialRedundancy::collectAllNodesToBeAnchored(List<TR::Node> *anchoredNodes, TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getReferenceCount() > 1)
      anchoredNodes->add(node);
   else
      {
      int32_t i;
      for (i = 0 ; i < node->getNumChildren(); i++)
         collectAllNodesToBeAnchored(anchoredNodes, node->getChild(i), visitCount);
      }
   }



// Called to replace redundant nodes other than checks and stores;
// basically all these opcodes require a temp to be commoned up.
//
bool TR_PartialRedundancy::eliminateRedundantSupportedNodes(TR::Node *parent, TR::Node *node, bool firstComputation, TR::TreeTop *currentTree, int32_t blockNum, vcount_t visitCount, ContainerType *redundantComputations, ContainerType *anticipatabilityInfo, TR::Node **supportedNodesAsArray)
   {
   // If the node has been seen before, its visit count could be visitCount or visitCount+1
   if (visitCount <= node->getVisitCount())
      {
      if (isExpressionRedundant(node, redundantComputations, anticipatabilityInfo))
         {
         //
         // If the indirect access under the null check was eliminated
         // it must be replaced by a direct load, so change the null
         // check to a treetop
         //
         if (parent->getOpCode().isNullCheck())
            TR::Node::recreate(parent, TR::treetop);
         }
      return true;
      }

   node->setVisitCount(visitCount);

   bool flag = firstComputation;
   TR::ILOpCode &opCode = node->getOpCode();
   TR::DataType nodeDataType = node->getDataType();

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (!eliminateRedundantSupportedNodes(node, child, flag, currentTree, blockNum, visitCount, redundantComputations, anticipatabilityInfo, supportedNodesAsArray))
         flag = false;
      }


   if (isSupportedOpCode(node, parent))
      {
#ifdef J9_PROJECT_SPECIFIC
      if (_profilingWalk &&
          ((_exceptionCheckMotion &&
            _exceptionCheckMotion->getOptimisticRednSetInfo()[blockNum]->get(node->getLocalIndex())) ||
            isExpressionRedundant(node, redundantComputations, anticipatabilityInfo)))
         {
         // Nodes that can be commoned may be useful to profile as they may
         // turn out to be constants that never change. In such cases we
         // insert profiling trees to track their values. In the subsequent
         // comp() (that will use value profiling info), loop versioning
         // may be able to specialize a loop based on the (possibly loop invariant) values
         // being profiled here
         //
         if (comp()->isProfilingCompilation() &&
             ((node->getType().isInt32()) ||
              ((node->getType().isInt64()))) &&  /* &&
               node->getOpCode().isLoadVar() &&
               node->getSymbolReference()->getSymbol()->isAutoOrParm())) && */
             !node->getOpCode().isLoadConst() &&
             (_numProfilingsAllowed > 0) &&
             !node->getByteCodeInfo().doNotProfile())
            {
            _numProfilingsAllowed--;
            TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
            TR_ValueProfiler *valueProfiler = comp()->getRecompilationInfo()->getValueProfiler();

            TR::Node *currentNode = currentTree->getNode();
            if (currentNode->getOpCode().isResolveOrNullCheck() ||
                (currentNode->getOpCodeValue() == TR::treetop))
               currentNode = currentNode->getFirstChild();

            static char *profileLongParms = feGetEnv("TR_ProfileLongParms");
            if ((node->getType().isInt64()) &&
                profileLongParms)
               {
               // Switch this compile to profiling comp() in case a
               // potential opportunity is seen for specializing long autos is seen;
               // in this case aggressively try to profile and recompile (reduced
               // the profiling count)
               //
               if (comp()->getMethodHotness() == hot &&
                   comp()->getRecompilationInfo())
                  optimizer()->switchToProfiling();
               }
            else if (!node->getType().isInt64())
               {
               TR::ILOpCode &currentOpCode = currentNode->getOpCode();
               if (currentOpCode.isBranch() ||
                   currentOpCode.isJumpWithMultipleTargets() ||
                   currentOpCode.isReturn() ||
                   currentOpCode.getOpCodeValue() == TR::athrow)
                  valueProfiler->addProfilingTrees(node, prevTree);
               else
                  valueProfiler->addProfilingTrees(node, currentTree);

               if (trace())
                  traceMsg(comp(), "Added profiling instrumentation for %p(%d)\n", node, node->getByteCodeIndex());
               }
            }
         }
#endif

      // If this node is simply a load of an auto or parm,
      // then there is nothing to do
      //
      if ((node->getOpCode().isLoadVarDirect() &&
          !node->getSymbol()->isStatic() &&
          !node->getSymbol()->isMethodMetaData())
            ||
          (ignoreNode(node))
          )
          return flag;


      if (isExpressionRedundant(node, redundantComputations, anticipatabilityInfo))
         {
         scount_t nodeIndex = node->getLocalIndex();
         TR::Node *nextRedundantNode = supportedNodesAsArray[nodeIndex];


         if (!isNodeAnImplicitNoOp(node))
            {
            if (flag)
               return false;

            // Don't use a new visit count for each set of children
            //vcount_t visitCount1 = comp()->incVisitCount();
            resetChildrensVisitCounts(node, visitCount);
            TR_ScratchList<TR::Node> anchoredNodes(trMemory());
            int32_t j;
            for (j=0;j<node->getNumChildren();j++)
               collectAllNodesToBeAnchored(&anchoredNodes, node->getChild(j), visitCount+1);

            ListIterator<TR::Node> anchorIt(&anchoredNodes);
            TR::Node *nextAnchor;
            for (nextAnchor = anchorIt.getFirst(); nextAnchor != NULL; nextAnchor = anchorIt.getNext())
               {
               TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, nextAnchor));
               TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
               anchorTreeTop->join(currentTree);
               prevTree->join(anchorTreeTop);
               }

            if (node->getOpCode().isLoadConst())
               {
               if ((parent->getOpCode().isDiv() ||
                    parent->getOpCode().isRem()) &&
                   (parent->getSecondChild() == node))
                  return flag;
               else
                  optimizer()->setRequestOptimization(OMR::globalValuePropagation, true);
               }

            //dumpOptDetails(comp(), "%sEliminating redundant computation (%s) : %p\n", OPT_DETAILS, node->getOpCode().getName(), node);

            TR::SymbolReference *newSymbolReference = _newSymbolReferences[nodeIndex];
            if (newSymbolReference &&
                performTransformation(comp(), "%sEliminating redundant computation (%s) : %p in block_%d, visit count %d\n", OPT_DETAILS, node->getOpCode().getName(), node, blockNum, node->getVisitCount()))
               {
               if (node->getDataType() == TR::NoType)
                  {
                  currentTree->unlink(true); // don't create loads of a void type
                  }
               else if (fe()->dataTypeForLoadOrStore(node->getDataType()) != node->getDataType())
                  {
                  if (parent &&
                      parent->getOpCode().isConversion() &&
                      (parent->getOpCodeValue()==TR::ILOpCode::getProperConversion(parent->getDataType(),node->getDataType(), false /*!wantZeroExtension*/)))
                     {
                     if (node->getReferenceCount() > 1)
                        {
                        TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node));
                        TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
                        anchorTreeTop->join(currentTree);
                        prevTree->join(anchorTreeTop);
                        }

                     for (j=0;j<parent->getNumChildren();j++)
                        parent->getChild(j)->recursivelyDecReferenceCount();

                     //node->decReferenceCount();

                     processReusedNode(parent, comp()->il.opCodeForDirectLoad(nodeDataType), newSymbolReference, 0);
                     }
                  else
                     {
                     for (j=0;j<node->getNumChildren();j++)
                        node->getChild(j)->recursivelyDecReferenceCount();

                     TR::Node *newLoad = TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(node->getDataType()), 0, newSymbolReference);

                     TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(newLoad->getDataType(), node->getDataType(), false /* !wantZeroExtension */);
                     if (conversionOpCode == TR::v2v)
                        {
                        node = TR::Node::createVectorConversion(newLoad, node->getDataType());
                        }
                     else
                        {
                        processReusedNode(node, conversionOpCode, NULL, 1);
#ifdef J9_PROJECT_SPECIFIC
                        if (node->getType().isBCD())
                           node->resetSignState();
#endif
                        node->setAndIncChild(0, newLoad);
                        }
                     }
                  }
               else
                  {
                  for (j=0;j<node->getNumChildren();j++)
                     node->getChild(j)->recursivelyDecReferenceCount();

                  processReusedNode(node, comp()->il.opCodeForDirectLoad(nodeDataType), newSymbolReference, 0);
                  }

               manager()->setAlteredCode(true);

               if (parent->getOpCode().isNullCheck())
                  TR::Node::recreate(parent, TR::treetop);
               }
            }
         }
      }

   return flag;
   }



bool TR_PartialRedundancy::isNotPrevTreeStoreIntoFloatTemp(TR::Symbol *symbol)
   {
   if (_prevTree && (_prevTree->getNode()->getOpCode().isFloat() || _prevTree->getNode()->getOpCode().isDouble()) && (_prevTree->getNode()->getSymbol() == symbol))
      return false;

   return true;
   }



// Replace all subnodes that are already available in some temp
// before placing this optimal computation; basically insert temps
// of appropriate available expressions into this optimal computation
//
TR::TreeTop *TR_PartialRedundancy::replaceOptimalSubNodes(TR::TreeTop *curTree, TR::Node *parent, TR::Node *node, int32_t childNum, TR::Node *duplicateParent, TR::Node *duplicateOptimalNode, bool isNullCheck, int32_t blockNum, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return curTree;

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();
   TR::DataType nodeDataType = node->getDataType();

   if (isSupportedOpCode(node, parent) &&
       (!(opCode.isLoadVarDirect() &&
          !node->getSymbol()->isStatic() && !node->getSymbol()->isMethodMetaData())) &&
       (!isNodeAnImplicitNoOp(node)))
      {
      if (trace())
         traceMsg(comp(), "Node %p has parent %p and we are considering replacing it\n", node, parent);
      if (((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0)) && !(isNullCheck && (_nullCheckNode->getNullCheckReference() == node)))
         {
         // We do not want to do optimal subnode replacement under tracePRE because it can cause spurious failures as a result
         // of earlier optimal node placement not having been done (because of opt transformation index) that interfere with last opt transformation index based search.
         // This is done because optimal sub node replacement is rarely the cause of bugs on it's own and this issue is just making it harder to
         // debug problems in other PRE transformations.
         // We created another option TR_TracePREForOptimalSubNodeReplacement to continue to allow optimal sub node replacement to be done regardless
         // but in order to make this more useful we turn off limiting optimal placements when this new option is in effect (so that it becomes safe to narrow down to a
         // optimal sub node replacement in the unlikely event that this is the cause of some failure)
         //
         if (shouldAllowOptimalSubNodeReplacement(comp()) &&
             (((_newSymbolsMap[node->getLocalIndex()] > -1)  && ((/* _optSetInfo[blockNum]->get(node->getLocalIndex()) || */  ((!parent || !parent->getOpCode().isSpineCheck() || (childNum != 0)) && comp()->requiresSpineChecks() && node->getOpCode().hasSymbolReference() && node->getSymbol()->isArrayShadowSymbol()) || !_unavailableSetInfo[blockNum]->get(node->getLocalIndex()))))))
            {
            TR::SymbolReference *newSymbolReference = _newSymbolReferences[node->getLocalIndex()];
            TR::Node *newLoad = TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(nodeDataType), 0, newSymbolReference);
            if (fe()->dataTypeForLoadOrStore(node->getDataType()) != node->getDataType())
               newLoad = TR::Node::create(TR::ILOpCode::getProperConversion(newLoad->getDataType(), node->getDataType(), false /* !wantZeroExtension */), 1, newLoad);

            newLoad->setReferenceCount(1);
            newLoad->setLocalIndex(-1);
            duplicateOptimalNode->recursivelyDecReferenceCount();
            duplicateParent->setChild(childNum, newLoad);

            if (trace())
               traceMsg(comp(), "Duplicate parent %p had its old child %p replaced by %p with symRef #%d\n", duplicateParent, duplicateOptimalNode, newLoad, newLoad->getSymbolReference()->getReferenceNumber());
            if (duplicateParent->getOpCode().isNullCheck())
               TR::Node::recreate(duplicateParent, TR::treetop);
            }
         else
            {
            if (trace())
               traceMsg(comp(), "Note : Duplicate parent %p wanted to replace its child %p by possibly already available symRef but FAILED to do so\n", duplicateParent, duplicateOptimalNode);
            int32_t i;
            for (i = 0; i < node->getNumChildren(); i++)
               {
               TR::Node *child = node->getChild(i);
               TR::Node *duplicateChild = duplicateOptimalNode->getChild(i);
               curTree = replaceOptimalSubNodes(curTree, node, child, i, duplicateOptimalNode, duplicateChild, isNullCheck, blockNum, visitCount);
               }

            if (node->getOpCode().isCall())
                {
                TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, duplicateOptimalNode));
                TR::TreeTop *nextTree = curTree->getNextTreeTop();
                anchorTreeTop->join(nextTree);
                curTree->join(anchorTreeTop);
                curTree = anchorTreeTop;
                }
            }
         }
      else
         {
         int32_t i;
         for (i = 0; i < node->getNumChildren(); i++)
            {
            TR::Node *child = node->getChild(i);
            TR::Node *duplicateChild = duplicateOptimalNode->getChild(i);
            curTree = replaceOptimalSubNodes(curTree, node, child, i, duplicateOptimalNode, duplicateChild, isNullCheck, blockNum, visitCount);
            }
         }
      }
   else
      {
      int32_t i;
      for (i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);
         TR::Node *duplicateChild = duplicateOptimalNode->getChild(i);
         curTree = replaceOptimalSubNodes(curTree, node, child, i, duplicateOptimalNode, duplicateChild, isNullCheck, blockNum, visitCount);
         }
      }

   return curTree;
   }

const char *
TR_PartialRedundancy::optDetailString() const throw()
   {
   return "O^O PARTIAL REDUNDANCY ELIMINATION: ";
   }


// Analysis for adjusting the optimal info/redundant info sets for each block
// computed by PRE. This is required as exception ordering constraints have
// been ignored by PRE in its analysis (it was optimistic in its approach).
// This pass is required to correctly pessimise the info computed by PRE
// based on exception ordering constraints; this also specifies which
// global register candidate should be chosen for which blocks.
//
TR_ExceptionCheckMotion::TR_ExceptionCheckMotion(TR::Compilation *comp, TR::Optimizer *optimizer, TR_PartialRedundancy *partialRedundancy)
   : TR_DataFlowAnalysis(comp, comp->getFlowGraph(), optimizer, partialRedundancy->trace()),
     _partialRedundancy(partialRedundancy), _workingList(comp->trMemory())
   {
   _partialRedundancy->manager()->setRequiresStructure(true);

   TR::CFG * cfg = comp->getFlowGraph();

   _numberOfBits = partialRedundancy->getNumberOfBits();
   _numNodes = partialRedundancy->getNumNodes();
   _numberOfNodes = cfg->getNextNodeNumber();
   _optimisticOptSetInfo = _partialRedundancy->getOptSetInfo();
   _optimisticRednSetInfo = _partialRedundancy->getRednSetInfo();
   _pendingList = NULL;

   _orderedOptNumbersList  = (int32_t **)trMemory()->allocateStackMemory(_numberOfNodes*sizeof(int32_t *));
   memset(_orderedOptNumbersList, 0, _numberOfNodes*sizeof(int32_t *));

   _actualOptSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_actualOptSetInfo, 0, _numNodes*sizeof(ContainerType *));
   _actualRednSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_actualRednSetInfo, 0, _numNodes*sizeof(ContainerType *));
   _killedGenExprs = (ContainerType **)trMemory()->allocateStackMemory(_numNodes*sizeof(ContainerType *));
   memset(_killedGenExprs, 0, _numNodes*sizeof(ContainerType *));
   //_seenNodes = allocateContainer(_numNodes);
   _nodesInCycle = allocateContainer(_numNodes);

   TR::CFGNode *nextNode;
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR_Structure *blockStructure = (toBlock(nextNode))->getStructureOf();
      if (blockStructure == NULL)
         continue;
      int32_t i = blockStructure->getNumber();
      _actualOptSetInfo[i] = partialRedundancy->allocateContainer(_numberOfBits);
      _actualRednSetInfo[i] = partialRedundancy->allocateContainer(_numberOfBits);
      _killedGenExprs[i] = allocateContainer(_numberOfBits);
      ///// Changed to initialize here too to save multiple calls to elementCount ... Dave S
      /////_orderedOptNumbersList[i] = (int32_t *)trMemory()->allocateStackMemory((_optimisticOptSetInfo[i]->elementCount()+_optimisticRednSetInfo[i]->elementCount())*sizeof(int32_t));
      //
      // Conservatively this is the maximum number of elements
      // that could possibly be in the actual optimal set for this
      // block after expression dominance. All elements in optimistic opt
      // list info can obviously be there and elements in redundant list
      // may also be there eventually if they could NOT be moved to the
      // program point that is optimal (according to PRE) because of expression
      // ordering constraints
      //
      int32_t numElements = _optimisticOptSetInfo[i]->elementCount()+_optimisticRednSetInfo[i]->elementCount();
      _orderedOptNumbersList[i] = (int32_t *)trMemory()->allocateStackMemory(numElements*sizeof(int32_t));
      memset(_orderedOptNumbersList[i], -1, numElements*sizeof(int32_t));
      }

   _tempContainer = allocateContainer(_numberOfBits);
   //_temp = new (trStackMemory()) TR_BitVector(_numberOfBits, trMemory(), stackAlloc);
   }

TR_DataFlowAnalysis::Kind TR_ExceptionCheckMotion::getKind() {return ExceptionCheckMotion;}


void TR_ExceptionCheckMotion::setBlockFenceStatus(TR::Block *block)
   {
   int32_t blockNum = block->getNumber();
   for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
      {
      TR::CFGNode *pred = (*nextEdge)->getFrom();
      int32_t result = areExceptionSuccessorsIdentical(block, pred);
      switch (result)
         {
         case BOTH_SAME :
            break;
         case FIRST_LARGER :
            {
            _blockWithFencesAtEntry->set(blockNum);
            if (trace())
               traceMsg(comp(), "Fence at entry to %d\n", blockNum);
            break;
            }
         case SECOND_LARGER :
            {
            _blockWithFencesAtExit->set(pred->getNumber());
            if (trace())
               traceMsg(comp(), "Fence at exit from %d\n", pred->getNumber());
            break;
            }
         case BOTH_UNRELATED :
            {
            _blockWithFencesAtEntry->set(blockNum);
            if (trace())
               traceMsg(comp(), "Fence at entry to %d\n", blockNum);
            _blockWithFencesAtExit->set(pred->getNumber());
            if (trace())
               traceMsg(comp(), "Fence at exit from %d\n", pred->getNumber());
            break;
            }
         }
      }
   }




int32_t TR_ExceptionCheckMotion::areExceptionSuccessorsIdentical(TR::CFGNode *node1, TR::CFGNode *node2)
   {
   int32_t result = -1;
   _firstSucc->empty();
   _secondSucc->empty();
   _comparison->empty();
   for (auto nextEdge = node1->getExceptionSuccessors().begin(); nextEdge != node1->getExceptionSuccessors().end(); ++nextEdge)
      {
      TR::CFGNode *succ = (*nextEdge)->getTo();
      _firstSucc->set(succ->getNumber());
      }

   for (auto nextEdge = node2->getExceptionSuccessors().begin(); nextEdge != node2->getExceptionSuccessors().end(); ++nextEdge)
      {
      TR::CFGNode *succ = (*nextEdge)->getTo();
      _secondSucc->set(succ->getNumber());
      }

   if (*_firstSucc == *_secondSucc)
      result = BOTH_SAME;
   else
      {
      *_comparison = *_firstSucc;
      *_comparison -= *_secondSucc;
      if (_comparison->isEmpty())
         result = SECOND_LARGER;
      else
         {
         *_comparison = *_secondSucc;
         *_comparison -= *_firstSucc;
         if (_comparison->isEmpty())
            result = FIRST_LARGER;
         else
            result = BOTH_UNRELATED;
         }
      }

   return result;
   }



int32_t TR_ExceptionCheckMotion::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR::CFG *cfg = comp()->getFlowGraph();
   TR_Structure *rootStructure = cfg->getStructure();

   int32_t arraySize = _numberOfNodes*sizeof(List<TR::Node> *);

   _orderedOptList  = (List<TR::Node> **)trMemory()->allocateStackMemory(arraySize);
   memset(_orderedOptList, 0, arraySize);

   // These are bit vectors that keep track of where fences would
   // have been placed in the old IL scheme; this is used to prevent
   // PRE from moving exception causing nodes across catch boundaries
   //
   _blockWithFencesAtEntry = allocateContainer(_numberOfNodes);
   _blockWithFencesAtExit = allocateContainer(_numberOfNodes);
   _firstSucc = allocateContainer(_numberOfNodes);
   _secondSucc = allocateContainer(_numberOfNodes);
   _comparison = allocateContainer(_numberOfNodes);

   TR::CFGNode *nextNode;
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR::Block *block = toBlock(nextNode);
      if (block->getStructureOf() == NULL)
         continue;
      setBlockFenceStatus(block);
      }



   ContainerType *exprsNotRedundant = NULL;
   _firstIteration = true;
   _tryAnotherIteration = true;
   _trySecondBestSolution = false;
   //
   // Try at most NUM_ITERATIONS iterations of expression dominance and redundant expression
   // adjustment; basically keep iterating till some list changes
   // If there is no change in less than NUM_ITERATIONS iterations we will
   // detect that we have a converged solution and return
   //
   int32_t numIterations;
   for (numIterations=0;numIterations<NUM_ITERATIONS;numIterations++)
      {
      if (!_tryAnotherIteration)
         break;

      _tryAnotherIteration = false;

      if (_firstIteration)
         {
         //
         // Allocate the data structures required for this
         // dataflow analysis
         //
         _currentList = (List<TR::Node> **)trMemory()->allocateStackMemory(arraySize);
         int32_t i;
         for (i=0;i<_numberOfNodes;i++)
            _currentList[i] = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());

         _indirectAccessesThatSurvive = allocateContainer(_numberOfBits);
         _arrayAccessesThatSurvive = allocateContainer(_numberOfBits);
         _dividesThatSurvive = allocateContainer(_numberOfBits);
         _unresolvedAccessesThatSurvive = allocateContainer(_numberOfBits);
         _knownIndirectAccessesThatSurvive = allocateContainer(_numberOfBits);
         _knownArrayAccessesThatSurvive = allocateContainer(_numberOfBits);
         _knownDividesThatSurvive = allocateContainer(_numberOfBits);
         _knownUnresolvedAccessesThatSurvive = allocateContainer(_numberOfBits);
         _exprsUnaffectedByOrder = allocateContainer(_numberOfBits);
         _exprsContainingIndirectAccess = allocateContainer(_numberOfBits);
         _exprsContainingArrayAccess = allocateContainer(_numberOfBits);
         _exprsContainingDivide = allocateContainer(_numberOfBits);
         _exprsContainingUnresolvedAccess = allocateContainer(_numberOfBits);
         _appendHelper = allocateContainer(_numberOfBits);
         _composeHelper = allocateContainer(_numberOfBits);
         _definitelyNotKilled = allocateContainer(_numberOfBits);
         exprsNotRedundant = allocateContainer(_numberOfBits);
         }
      else
         {
         //
         // Reset the data structures required for this dataflow
         // analysis
         //
         int32_t i;
         for (i=0;i<_numberOfNodes;i++)
            _currentList[i]->deleteAll();

         _indirectAccessesThatSurvive->empty();
         _arrayAccessesThatSurvive->empty();
         _dividesThatSurvive->empty();
         _unresolvedAccessesThatSurvive->empty();
         _knownIndirectAccessesThatSurvive->empty();
         _knownArrayAccessesThatSurvive->empty();
         _knownDividesThatSurvive->empty();
         _knownUnresolvedAccessesThatSurvive->empty();
         _exprsUnaffectedByOrder->empty();
         _exprsContainingIndirectAccess->empty();
         _exprsContainingArrayAccess->empty();
         _exprsContainingDivide->empty();
         _exprsContainingUnresolvedAccess->empty();
         _appendHelper->empty();
         _composeHelper->empty();
         _definitelyNotKilled->empty();
         exprsNotRedundant->empty();
         }

      // Actually do the expression dominance analysis
      //
      bool redundantSetAdjustmentRequired = false;
      initializeGenAndKillSetInfo();
      rootStructure->resetAnalyzedStatus();
      rootStructure->resetAnalysisInfo();
      rootStructure->doDataFlowAnalysis(this, false);

      //
      // Construct the orderedOptNumbers list at this stage;
      // this contains the ordered list of optimal expressions
      //
      for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
         {
         TR::Block *block = toBlock(nextNode);
         if (block->getStructureOf() == NULL)
            continue;
         int32_t i = block->getNumber();
         *_tempContainer = *_optimisticOptSetInfo[i];
         *_tempContainer &= *_exprsUnaffectedByOrder;

         if (trace())
            traceMsg(comp(), "Block Number (ordered list) : %d\n", i);

         //
         // First add the expressions that are unaffected by order;
         // these expressions have nothing to do with checks and related
         // expressions
         //
         int32_t size = 0;
         if (!_tempContainer->isEmpty())
            {
            ///// Removed by Dave S to save calls to elementCount
            /////size = _temp->elementCount();
            /////int32_t j = 0;
            ContainerType::Cursor bvi(*_tempContainer);
            for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
               {
               int32_t nextExpressionUnaffectedByOrder = bvi;
               _orderedOptNumbersList[i][size] = nextExpressionUnaffectedByOrder;
               if (trace())
                  traceMsg(comp(), "Unaffected by order <%d>\n", nextExpressionUnaffectedByOrder);
               size++;
               }
            }

         // Now add the other optimal expressions for this block
         // (dependent on ordering) as computed by this iteration of
         // expression dominance
         //
        if (_orderedOptList[i] != NULL)
            {
            int32_t orderedListSize = _orderedOptList[i]->getSize();
            if (orderedListSize > 0)
               {
               ListElement<TR::Node> *nextElement = _orderedOptList[i]->getListHead();
               orderedListSize = orderedListSize+size;
               int32_t numElements2 = _optimisticOptSetInfo[i]->elementCount()+_optimisticRednSetInfo[i]->elementCount();
               int32_t j;
               for (j=size;j<orderedListSize;j++)
                  {
                  if (trace())
                     traceMsg(comp(), "Affected by order <%d>\n", nextElement->getData()->getLocalIndex());
                  _orderedOptNumbersList[i][j] = nextElement->getData()->getLocalIndex();
                  nextElement = nextElement->getNextElement();
                  }
               TR_ASSERT(orderedListSize <= numElements2  , "Overwriting elements! orderedListSize = %d numElements2 = %d\n",orderedListSize,numElements2 );
               }
            }

        // If there is a difference between the optimistic optimal info
        // computed by PRE and the actual optimal info computed as a result of applying
        // expression ordering constraints in expression dominance, then
        // we need to adjust the optimistic redundant set info for some blocks
        // as some expression(s) that were assumed to be optimal in this block
        // are no longer optimal
        //
         *_actualOptSetInfo[i] |= *_tempContainer;
         if (!(*_optimisticOptSetInfo[i] == *_actualOptSetInfo[i]))
            redundantSetAdjustmentRequired = true;
         *_actualRednSetInfo[i] = *_optimisticRednSetInfo[i];
         }

      // We try to compute the second best solution if
      // we cannot do any more iterations; the second best solution
      // is to place it optimally in those blocks (where the expression was redundant)
      // that dominate other blocks where it is redundant
      //
      // We also compute global register assignment info in this final pass
      // before leaving expression dominance; this uses the
      // availability information computed by redundant expression
      // adjustment
      //
      if (!_tryAnotherIteration || (numIterations == (NUM_ITERATIONS-1)))
         _trySecondBestSolution = true;

      if (redundantSetAdjustmentRequired || _trySecondBestSolution)
         {
         //
         // Adjust the redundant set info to account for the fact that
         // some expressions would no longer be redundant if some
         // expressions were not optimal in a pred block anymore
         //
         ContainerType **unavailableSetInfo = _partialRedundancy->getUnavailableSetInfo();
         TR_RedundantExpressionAdjustment *redundantExpressionAdjustment = new (comp()->allocator()) TR_RedundantExpressionAdjustment(comp(), optimizer(), comp()->getFlowGraph()->getStructure(), this);

         //TR_BitVector **globalRegisterSetInfo = NULL;
         //if (_trySecondBestSolution)
         //   globalRegisterSetInfo = _partialRedundancy->getGlobalRegisterSetInfo();
         //
         //TR_BitVector **globalRegisterSetInfo = _partialRedundancy->getGlobalRegisterSetInfo();

         for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
            {
            TR::Block *block = toBlock(nextNode);
            if (block->getStructureOf() == NULL)
               continue;
            int32_t i = block->getNumber();
            *_actualRednSetInfo[i] &= *(redundantExpressionAdjustment->_blockAnalysisInfo[i]);
            *_actualRednSetInfo[i] -= *(_actualOptSetInfo[i]);
            unavailableSetInfo[i]->setAll(_partialRedundancy->getNumberOfBits());
            *unavailableSetInfo[i] -= *(redundantExpressionAdjustment->_blockAnalysisInfo[i]);
            //if (_trySecondBestSolution)
            //   {
            //   *(globalRegisterSetInfo[i]) = *(redundantExpressionAdjustment->_blockAnalysisInfo[i]);
            //   }
            }
         }

      _firstIteration = false;
      }

   if (trace())
      {
      comp()->incVisitCount();
      TR::TreeTop *currentTree = comp()->getStartTree();
      while (!(currentTree == NULL))
         {
         getDebug()->print(comp()->getOutFile(), currentTree);
         currentTree = currentTree->getNextTreeTop();
         }
      }

   return 1;
   }





void TR_ExceptionCheckMotion::copyListFromInto(List<TR::Node> *fromList, List<TR::Node> *toList)
   {
   if (!fromList->isEmpty())
      {
      ListElement<TR::Node> *oldNode = fromList->getListHead();
      ListElement<TR::Node> *newNode = toList->getListHead();
      ListElement<TR::Node> *prevNode = NULL;
      for (;oldNode != NULL;)
         {
         if (newNode == NULL)
            {
            newNode = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
            newNode->setNextElement(NULL);
            if (prevNode)
               prevNode->setNextElement(newNode);
            else
               toList->setListHead(newNode);
            }

         newNode->setData(oldNode->getData());

         prevNode = newNode;
         oldNode = oldNode->getNextElement();
         newNode = newNode->getNextElement();
         }

      prevNode->setNextElement(NULL);
      }
   else
      toList->deleteAll();
   }








void TR_ExceptionCheckMotion::composeLists(List<TR::Node> *targetList, List<TR::Node> *sourceList, ContainerType *availableExprs)
   {
   _composeHelper->empty();
   if (!sourceList->isEmpty() &&
       !targetList->isEmpty())
      {
      ListElement<TR::Node> *oldNode = targetList->getListHead();
      ListElement<TR::Node> *prevNode = NULL;
      ListElement<TR::Node> *cutoffNode = NULL;
      ListElement<TR::Node> *markerNode = NULL;
      _definitelyNotKilled->empty();
      while (oldNode &&
             !oldNode->getData()->getOpCode().isCheck())
         {
         _composeHelper->set(oldNode->getData()->getLocalIndex());
         if (!markerNode)
            markerNode = oldNode;

         prevNode = oldNode;
         oldNode = oldNode->getNextElement();
         }

      ListElement<TR::Node> *cursorOldNode;
      ListElement<TR::Node> *newNode;

      for (newNode = sourceList->getListHead(); newNode != NULL;)
         {
         cursorOldNode = NULL;
         if (oldNode)
            cursorOldNode = oldNode->getNextElement();

         while (newNode &&
                !newNode->getData()->getOpCode().isCheck())
            {
            if (!_composeHelper->get(newNode->getData()->getLocalIndex()))
               {
               ListElement<TR::Node> *newSourceNode = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
               newSourceNode->setNextElement(NULL);
               if (prevNode)
                  prevNode->setNextElement(newSourceNode);
               else
                  targetList->setListHead(newSourceNode);

               if (!markerNode)
                  markerNode = newSourceNode;

               prevNode = newSourceNode;
               newSourceNode->setData(newNode->getData());
               }
            else
               _definitelyNotKilled->set(newNode->getData()->getLocalIndex());
            newNode = newNode->getNextElement();
            }

         if ((oldNode == NULL) || (newNode ==  NULL) || (oldNode->getData()->getLocalIndex() != newNode->getData()->getLocalIndex()))
            {
            ListElement<TR::Node> *prevCursorNode = cutoffNode;
            ListElement<TR::Node> *cursorNode = cutoffNode ? cutoffNode->getNextElement() : markerNode;
            while (cursorNode)
               {
               if (_definitelyNotKilled->get(cursorNode->getData()->getLocalIndex()) ||
                   ((cursorNode->getData()->getOpCode().isIndirect() ||
                     cursorNode->getData()->getOpCode().isArrayLength()) &&
                    availableExprs->get(cursorNode->getData()->getFirstChild()->getLocalIndex())))
                  prevCursorNode = cursorNode;
               else
                  {
                  if (prevCursorNode)
                     prevCursorNode->setNextElement(cursorNode->getNextElement());
                  else
                     targetList->setListHead(cursorNode->getNextElement());
                  }

               if (cursorNode == prevNode)
                  break;

               cursorNode = cursorNode->getNextElement();
               }


            oldNode = cursorOldNode;
            while (oldNode &&
                   ((oldNode->getData()->getOpCode().isIndirect() ||
                     oldNode->getData()->getOpCode().isArrayLength()) &&
                     availableExprs->get(oldNode->getData()->getFirstChild()->getLocalIndex())))
               {
               ListElement<TR::Node> *nextOldNode = oldNode->getNextElement();
               if (!_composeHelper->get(oldNode->getData()->getLocalIndex()))
                  {
                  _composeHelper->set(oldNode->getData()->getLocalIndex());
                  oldNode->setNextElement(NULL);
                  if (prevCursorNode)
                     {
                     prevCursorNode->setNextElement(oldNode);
                     //dumpOptDetails(comp(), "prevCursorNode %x %x oldNode %x %x\n", prevCursorNode, prevCursorNode->getData(), oldNode, oldNode->getData());
                     }
                  else
                     targetList->setListHead(oldNode);

                  prevCursorNode = oldNode;
                  }
               oldNode = nextOldNode;
               }

            if (newNode)
               newNode = newNode->getNextElement();
            while (newNode &&
                   ((newNode->getData()->getOpCode().isIndirect() ||
                     newNode->getData()->getOpCode().isArrayLength()) &&
                    availableExprs->get(newNode->getData()->getFirstChild()->getLocalIndex())))
               {
               if (!_composeHelper->get(newNode->getData()->getLocalIndex()))
                  {
                  ListElement<TR::Node> *newSourceNode = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
                  newSourceNode->setNextElement(NULL);
                  if (prevCursorNode)
                     prevCursorNode->setNextElement(newSourceNode);
                  else
                     targetList->setListHead(newSourceNode);

                  prevCursorNode = newSourceNode;
                  newSourceNode->setData(newNode->getData());
                  }
               newNode = newNode->getNextElement();
               }

            break;
            }

         prevNode = oldNode;
         _composeHelper->set(oldNode->getData()->getLocalIndex());
         cutoffNode = oldNode;
         oldNode = oldNode->getNextElement();
         markerNode = oldNode;

         while (oldNode &&
                !oldNode->getData()->getOpCode().isCheck())
            {
            _composeHelper->set(oldNode->getData()->getLocalIndex());
            prevNode = oldNode;
            oldNode = oldNode->getNextElement();
            }

         newNode = newNode->getNextElement();
         }

      if (prevNode)
         {
         prevNode->setNextElement(NULL);
         }
      else
         {
         targetList->deleteAll();
         }
      }
   else if (!targetList->isEmpty())
      {
      ListElement<TR::Node> *prevCursorNode = NULL;
      ListElement<TR::Node> *oldNode = targetList->getListHead();
      while (oldNode &&
             ((oldNode->getData()->getOpCode().isIndirect() ||
               oldNode->getData()->getOpCode().isArrayLength()) &&
               availableExprs->get(oldNode->getData()->getFirstChild()->getLocalIndex())))
         {
         ListElement<TR::Node> *nextOldNode = oldNode->getNextElement();
         //if (!_composeHelper->get(oldNode->getData()->getLocalIndex()))
            {
            oldNode->setNextElement(NULL);
            if (prevCursorNode)
               prevCursorNode->setNextElement(oldNode);
            else
               targetList->setListHead(oldNode);

            prevCursorNode = oldNode;
            }
         oldNode = nextOldNode;
         }
      if (!prevCursorNode)
         targetList->deleteAll();
      }
   else if (!sourceList->isEmpty())
      {
      ListElement<TR::Node> *prevCursorNode = NULL;
      ListElement<TR::Node> *newNode = sourceList->getListHead();
      while (newNode &&
             ((newNode->getData()->getOpCode().isIndirect() ||
               newNode->getData()->getOpCode().isArrayLength()) &&
               availableExprs->get(newNode->getData()->getFirstChild()->getLocalIndex())))
         {
           //if (!_composeHelper->get(newNode->getData()->getLocalIndex()))
            {
            ListElement<TR::Node> *newSourceNode = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
            newSourceNode->setNextElement(NULL);
            if (prevCursorNode)
               prevCursorNode->setNextElement(newSourceNode);
            else
               targetList->setListHead(newSourceNode);

            prevCursorNode = newSourceNode;
            newSourceNode->setData(newNode->getData());
            }
         newNode = newNode->getNextElement();
         }

      if (!prevCursorNode)
         targetList->deleteAll();
      }
   else
      {
      targetList->deleteAll();
      }
   }





void TR_ExceptionCheckMotion::appendLists(List<TR::Node> *firstList, List<TR::Node> *secondList)
   {
   if (!secondList->isEmpty())
      {
      ListElement<TR::Node> *secondNode = secondList->getListHead();
      ListElement<TR::Node> *firstNode = firstList->getListHead();
      ListElement<TR::Node> *lastElementInFirstList = NULL;
      ListElement<TR::Node> *newFirstNode = NULL;
      _appendHelper->empty();
      for (;firstNode != NULL;firstNode = firstNode->getNextElement())
          {
          lastElementInFirstList = firstNode;
          _appendHelper->set(firstNode->getData()->getLocalIndex());
          }

      ListElement<TR::Node> *prevNode = lastElementInFirstList;
      for (;secondNode != NULL;)
         {
         if (!_appendHelper->get(secondNode->getData()->getLocalIndex()))
            {
            newFirstNode = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
            newFirstNode->setNextElement(NULL);
            if (prevNode)
               prevNode->setNextElement(newFirstNode);
            else
               firstList->setListHead(newFirstNode);

            newFirstNode->setData(secondNode->getData());
            _appendHelper->set(secondNode->getData()->getLocalIndex());

            prevNode = newFirstNode;
            }
         secondNode = secondNode->getNextElement();
         }
      }
   }




bool TR_ExceptionCheckMotion::compareLists(List<TR::Node> *firstList, List<TR::Node> *secondList)
   {
   bool same = true;
   if (firstList->getSize() != secondList->getSize())
      same = false;
   else
      {
      if (!secondList->isEmpty())
         {
         ListIterator<TR::Node> firstListIt(firstList);
         ListIterator<TR::Node> secondListIt(secondList);
         TR::Node *firstNode = firstListIt.getFirst();
         TR::Node *secondNode;
         for (secondNode = secondListIt.getFirst(); secondNode != NULL;)
            {
            if (firstNode->getLocalIndex() != secondNode->getLocalIndex())
               {
               same = false;
               break;
               }
            secondNode = secondListIt.getNext();
            firstNode = firstListIt.getNext();
            }
         }
      }

   return same;
   }





TR_ExceptionCheckMotion::ExprDominanceInfo *TR_ExceptionCheckMotion::getAnalysisInfo(TR_Structure *s)
   {
   ExprDominanceInfo *analysisInfo = (ExprDominanceInfo *)s->getAnalysisInfo();
   if (!s->hasBeenAnalyzedBefore())
      {
      if (analysisInfo == NULL)
         {
         analysisInfo = createAnalysisInfo();
         initializeAnalysisInfo(analysisInfo, s);
         s->setAnalysisInfo(analysisInfo);
         }
      else
         {
         int32_t i;
         for (i=0;i<_numberOfNodes;i++)
            {
            if (analysisInfo->_outList[i] != NULL)
               analysisInfo->_outList[i]->deleteAll();
            }

         analysisInfo->_inList->deleteAll();
         }
      }
   return analysisInfo;
   }




TR_ExceptionCheckMotion::ExprDominanceInfo *TR_ExceptionCheckMotion::createAnalysisInfo()
   {
   ExprDominanceInfo *analysisInfo = new (trStackMemory()) ExprDominanceInfo;
   analysisInfo->_outList = (List<TR::Node> **)trMemory()->allocateStackMemory(_numberOfNodes*sizeof(List<TR::Node> *));
   memset(analysisInfo->_outList, 0, _numberOfNodes*sizeof(List<TR::Node> *));
   return analysisInfo;
   }





void TR_ExceptionCheckMotion::initializeAnalysisInfo(ExprDominanceInfo *info, TR_Structure *s)
   {
   TR_RegionStructure *region = s->asRegion();
   if (region)
      initializeAnalysisInfo(info, region);
   else
      initializeAnalysisInfo(info, s->asBlock()->getBlock());
   }






void TR_ExceptionCheckMotion::initializeAnalysisInfo(ExprDominanceInfo *info, TR::Block *block)
   {
   info->_inList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
   TR::CFGNode *succBlock;
   for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
      {
      succBlock = (*succ)->getTo();
      info->_outList[succBlock->getNumber()] = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
      }

   for (auto succ = block->getExceptionSuccessors().begin(); succ != block->getExceptionSuccessors().end(); ++succ)
      {
      succBlock = (*succ)->getTo();
      info->_outList[succBlock->getNumber()] = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
      }
   }




TR_ExceptionCheckMotion::ContainerType *TR_ExceptionCheckMotion::allocateContainer(int32_t size)
   {
   return new (trStackMemory())TR_BitVector(size, trMemory(), stackAlloc);
   }

void TR_ExceptionCheckMotion::initializeAnalysisInfo(ExprDominanceInfo *info, TR_RegionStructure *region)
   {
   ContainerType *exitNodes = allocateContainer(_numberOfNodes);

   info->_inList = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
   //
   // Copy the current out set for comparison the next time we analyze this region
   //
   //
   if (region != comp()->getFlowGraph()->getStructure())
      {
      ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
      TR::CFGEdge *edge;
      for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
         {
         int32_t toStructureNumber = edge->getTo()->getNumber();
         if (!exitNodes->get(toStructureNumber))
            {
            info->_outList[toStructureNumber] = new (trStackMemory()) TR_ScratchList<TR::Node>(trMemory());
            exitNodes->set(toStructureNumber);
            }
         }
      }
   }





void TR_ExceptionCheckMotion::initializeGenAndKillSetInfo()
   {
   // For each block in the CFG build the gen and kill set for this analysis
   //
   if (_firstIteration)
      {
      int32_t arraySize = _numberOfNodes*sizeof(List<TR::Node> *);
      _regularGenSetInfo  = (List<TR::Node> **)trMemory()->allocateStackMemory(arraySize);
      memset(_regularGenSetInfo, 0, arraySize);
      _blockInfo = (List<TR::Node> **)trMemory()->allocateStackMemory(arraySize);
      memset(_blockInfo, 0, arraySize);
      _nullCheckKilled = allocateContainer(_numberOfNodes);
      _resolveCheckKilled = allocateContainer(_numberOfNodes);
      _boundCheckKilled = allocateContainer(_numberOfNodes);
      _divCheckKilled = allocateContainer(_numberOfNodes);
      _arrayStoreCheckKilled = allocateContainer(_numberOfNodes);
      _arrayCheckKilled = allocateContainer(_numberOfNodes);
      _checkCastKilled = allocateContainer(_numberOfNodes);
      _unresolvedAccessesKilled = allocateContainer(_numberOfNodes);
      _indirectAccessesKilled = allocateContainer(_numberOfNodes);
      _arrayAccessesKilled = allocateContainer(_numberOfNodes);
      _dividesKilled = allocateContainer(_numberOfNodes);
      _relevantNodes = allocateContainer(_partialRedundancy->getNumberOfBits());
      _genSetHelper = allocateContainer(_partialRedundancy->getNumberOfBits());
      }
   else
      {
      _nullCheckKilled->empty();
      _resolveCheckKilled->empty();
      _boundCheckKilled->empty();
      _divCheckKilled->empty();
      _arrayStoreCheckKilled->empty();
      _arrayCheckKilled->empty();
      _checkCastKilled->empty();
      _indirectAccessesKilled->empty();
      _unresolvedAccessesKilled->empty();
      _arrayAccessesKilled->empty();
      _dividesKilled->empty();
      _relevantNodes->empty();
      _genSetHelper->empty();
      }

   vcount_t visitCount = comp()->incVisitCount();
   TR::CFGNode *cfgNode;
   for (cfgNode = comp()->getFlowGraph()->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      TR::Block *block     = toBlock(cfgNode);
      int32_t blockNum    = block->getNumber();
      if (_firstIteration)
         {
         _regularGenSetInfo[blockNum] = new (trStackMemory())TR_ScratchList<TR::Node>(trMemory());
         _blockInfo[blockNum] = new (trStackMemory())TR_ScratchList<TR::Node>(trMemory());
         }
      else
         {
         _regularGenSetInfo[blockNum]->deleteAll();
         _blockInfo[blockNum]->deleteAll();
         }

      _lastGenSetElement = NULL;
      _hasExceptionSuccessor = false;
      if (!block->getExceptionSuccessors().empty())
         _hasExceptionSuccessor = true;

      _moreThanOneExceptionPoint = false;
      _atLeastOneExceptionPoint = false;
      _seenImmovableExceptionPoint = false;
      TR::TreeTop *treeTop = block->getEntry();
      if (treeTop == NULL)
         continue;

      _genSetHelper->empty();
      if (_blockWithFencesAtEntry->get(blockNum))
         {
         _seenImmovableExceptionPoint = true;
         _nullCheckKilled->set(blockNum);
         _boundCheckKilled->set(blockNum);
         _divCheckKilled->set(blockNum);
         _arrayStoreCheckKilled->set(blockNum);
         _arrayCheckKilled->set(blockNum);
         _checkCastKilled->set(blockNum);
         }

      _indirectAccessesThatSurvive->empty();
      _arrayAccessesThatSurvive->empty();
      _dividesThatSurvive->empty();
      _unresolvedAccessesThatSurvive->empty();
      _killedGenExprs[block->getNumber()]->empty();

      for ( ; treeTop != block->getExit(); treeTop = treeTop->getNextTreeTop())
         {
         TR::Node *node = treeTop->getNode();
         if (_hasExceptionSuccessor)
            {
            if (node->exceptionsRaised() != 0)
               _atLeastOneExceptionPoint = true;

            if (_atLeastOneExceptionPoint)
               {
               _nullCheckKilled->set(blockNum);
               _boundCheckKilled->set(blockNum);
               _divCheckKilled->set(blockNum);
               _arrayStoreCheckKilled->set(blockNum);
               _arrayCheckKilled->set(blockNum);
               _checkCastKilled->set(blockNum);
               //break;
               }
            }

         analyzeNodeToInitializeGenAndKillSets(treeTop, visitCount, block->getStructureOf());
         //if (_hasExceptionSuccessor && _atLeastOneExceptionPoint)
   //   break;
         }

      if (_blockWithFencesAtExit->get(blockNum))
         {
         _seenImmovableExceptionPoint = true;
         _nullCheckKilled->set(blockNum);
         _boundCheckKilled->set(blockNum);
         _divCheckKilled->set(blockNum);
         _arrayStoreCheckKilled->set(blockNum);
         _arrayCheckKilled->set(blockNum);
         _checkCastKilled->set(blockNum);
         }

      if (trace())
         {
         if (!_regularGenSetInfo[blockNum]->isEmpty())
            {
            ListElement<TR::Node> *listElem;
            for (listElem = _regularGenSetInfo[blockNum]->getListHead(); listElem != NULL; listElem = listElem->getNextElement())
               traceMsg(comp(), "Expr %d (representative) Node %p in Block : %d\n", listElem->getData()->getLocalIndex(), listElem->getData(), blockNum);
            }
         else
            traceMsg(comp(), "Block : %d has NO expr gened\n", blockNum);
         }
      }
   }









void TR_ExceptionCheckMotion::analyzeNodeToInitializeGenAndKillSets(TR::TreeTop *tt, vcount_t visitCount, TR_BlockStructure *blockStructure)
   {
   TR::Node * node = tt->getNode();
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);
   TR::Block *block = blockStructure->getBlock();
   int32_t blockNum = block->getNumber();
   bool checkNode = false;

   if (treeHasChecks(tt) || node->getOpCode().isStore())
      {
      //if (_hasExceptionSuccessor && _atLeastOneExceptionPoint)
        {
        //_atLeastOneExceptionPoint = true;
        //return;
        }

     if (node->getOpCode().isCheck())
        {
        TR::ILOpCodes opCodeValue = node->getOpCodeValue();
        if (opCodeValue == TR::NULLCHK)
           {
           checkNode = true;
           // Also check this as the child could be a call/call like node
           //
           bool childHasChecks = treeHasChecks(tt, node->getFirstChild());

           includeRelevantNodes(node->getNullCheckReference(), visitCount, blockNum);

           bool nodeKilledByChild = false;

           if (isNodeKilledByChild(node, node->getNullCheckReference(), blockNum))
              nodeKilledByChild = true;

           if (!_atLeastOneExceptionPoint)
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                   _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 //markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
                 }
              }

           if (((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */) ||
                 _nullCheckKilled->get(blockNum) ||
                 nodeKilledByChild) &&
	      !checkIfNodeCanSurvive(node->getNullCheckReference(), _indirectAccessesThatSurvive))
              {
              _killedGenExprs[blockNum]->set(node->getLocalIndex());
              _resolveCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              _indirectAccessesKilled->set(blockNum);
              _seenImmovableExceptionPoint = true;
              }
           else
              {
              // Fix4
              //
              markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
              }

           if (childHasChecks)
              {
              _seenImmovableExceptionPoint = true;
              _nullCheckKilled->set(blockNum);
              _resolveCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              }
           }
        else if (node->getOpCode().isResolveCheck())
           {
           checkNode = true;
           // Also check this as the child could be a call/call like node
           //
           bool childHasChecks = treeHasChecks(tt, node->getFirstChild());

           int32_t firstChildNum = 0;
           if ((opCodeValue == TR::ResolveAndNULLCHK) &&
               (node->getFirstChild()->getOpCode().isCallIndirect()))
              firstChildNum = 1;

           int32_t i;
           for (i=firstChildNum;i<node->getFirstChild()->getNumChildren();i++)
              includeRelevantNodes(node->getFirstChild()->getChild(i), visitCount, blockNum);

           bool nodeKilledByChild = false;
           for (i=firstChildNum;i<node->getFirstChild()->getNumChildren();i++)
              {
              if (isNodeKilledByChild(node, node->getFirstChild()->getChild(i), blockNum))
                 {
                 nodeKilledByChild = true;
                 break;
                 }
              }

           if (!_atLeastOneExceptionPoint)
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                       _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 //markNodeAsSurvivor(node->getFirstChild(), _unresolvedAccessesThatSurvive);
                 }
              }

           if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                     !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())*/ ||
                _resolveCheckKilled->get(blockNum) ||
		nodeKilledByChild))
              {
              _killedGenExprs[blockNum]->set(node->getLocalIndex());
              _nullCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              _unresolvedAccessesKilled->set(blockNum);
              if (opCodeValue == TR::ResolveAndNULLCHK)
                 {
                 _indirectAccessesKilled->set(blockNum);
                 _resolveCheckKilled->set(blockNum);
                 }
              _seenImmovableExceptionPoint = true;
              }
           else
              markNodeAsSurvivor(node->getFirstChild(), _unresolvedAccessesThatSurvive);

           if (childHasChecks)
              {
              _seenImmovableExceptionPoint = true;
              /////_arrayAccessesKilled->set(blockNum);
              /////_indirectAccessesKilled->set(blockNum);
              /////_dividesKilled->set(blockNum);
              _nullCheckKilled->set(blockNum);
              _resolveCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              }
           }
        else if (node->getOpCode().isBndCheck() || node->getOpCode().isSpineCheck())
           {
           checkNode = true;
           bool nodeKilledByChild = false;

           int32_t i = 0;
           if (node->getOpCode().isSpineCheck())
              i = 1;

           for (;i<node->getNumChildren();i++)
              {
              includeRelevantNodes(node->getChild(i), visitCount, blockNum);
              if (isNodeKilledByChild(node, node->getChild(i), blockNum))
                 nodeKilledByChild = true;
              }

           if (!_atLeastOneExceptionPoint)
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                       _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 }
              }

           if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                     !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */) ||
                 _boundCheckKilled->get(blockNum) ||
                 nodeKilledByChild)
              {
              _killedGenExprs[blockNum]->set(node->getLocalIndex());
              _nullCheckKilled->set(blockNum);
              _resolveCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              _arrayAccessesKilled->set(blockNum);
              _seenImmovableExceptionPoint = true;
              }
           }
        else if (opCodeValue == TR::DIVCHK)
           {
           checkNode = true;
           int32_t i;
           for (i=0;i<node->getFirstChild()->getNumChildren();i++)
              includeRelevantNodes(node->getFirstChild()->getChild(i), visitCount, blockNum);

           bool nodeKilledByChild = false;
           for (i=0;i<node->getFirstChild()->getNumChildren();i++)
              {
              if (isNodeKilledByChild(node, node->getFirstChild()->getChild(i), blockNum))
                 {
                 nodeKilledByChild = true;
                 break;
                 }
              }

           if (!_atLeastOneExceptionPoint)
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                       _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 //markNodeAsSurvivor(node->getFirstChild()->getSecondChild(), _dividesThatSurvive);
                 }
              }

           if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                     !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */ ||
                 _divCheckKilled->get(blockNum) ||
                 nodeKilledByChild) &&
                !checkIfNodeCanSurvive(node->getFirstChild()->getSecondChild(), _dividesThatSurvive))
              {
              _killedGenExprs[blockNum]->set(node->getLocalIndex());
              _nullCheckKilled->set(blockNum);
              _resolveCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              _dividesKilled->set(blockNum);
              _seenImmovableExceptionPoint = true;
              }
           else
              markNodeAsSurvivor(node->getFirstChild()->getSecondChild(), _dividesThatSurvive);


           includeRelevantNodes(node->getFirstChild(), visitCount, blockNum);
           }
        else if (opCodeValue == TR::ArrayStoreCHK)
           {
           checkNode = true;
           bool nodeKilledByChild = false;
           int32_t i;
           for (i=0;i<node->getNumChildren();i++)
              {
              includeRelevantNodes(node->getChild(i), visitCount, blockNum);
              if (isNodeKilledByChild(node, node->getChild(i), blockNum))
                 nodeKilledByChild = true;
              }

          if (!_atLeastOneExceptionPoint)
             {
             if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                      _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                   (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                   (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                   (!_genSetHelper->get(node->getLocalIndex())))
                {
                createAndAddListElement(node, blockNum);
                }
             }

          if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                    !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */) ||
                _arrayStoreCheckKilled->get(blockNum) ||
                nodeKilledByChild)
             {
             _killedGenExprs[blockNum]->set(node->getLocalIndex());
             _resolveCheckKilled->set(blockNum);
             _nullCheckKilled->set(blockNum);
             _boundCheckKilled->set(blockNum);
             _divCheckKilled->set(blockNum);
             _checkCastKilled->set(blockNum);
             _arrayCheckKilled->set(blockNum);
             _seenImmovableExceptionPoint = true;
             }
           }
        else if (opCodeValue == TR::ArrayCHK)
           {
           checkNode = true;
           bool nodeKilledByChild = false;
           int32_t i;
           for (i=0;i<node->getNumChildren();i++)
              {
              includeRelevantNodes(node->getChild(i), visitCount, blockNum);
              if (isNodeKilledByChild(node, node->getChild(i), blockNum))
                 nodeKilledByChild = true;
              }

           if (!_atLeastOneExceptionPoint)
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                       _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 }
              }

           if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                     !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */) ||
                 _arrayCheckKilled->get(blockNum) ||
                 nodeKilledByChild)
              {
              _killedGenExprs[blockNum]->set(node->getLocalIndex());
              _resolveCheckKilled->set(blockNum);
              _nullCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _checkCastKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _seenImmovableExceptionPoint = true;
              }
           }
        else if (node->getOpCode().isCheckCast()) //(opCodeValue == TR::checkcast)
           {
           checkNode = true;
           bool nodeKilledByChild = false;
           int32_t i;
           for (i=0;i<node->getNumChildren();i++)
              {
              includeRelevantNodes(node->getChild(i), visitCount, blockNum);
              if (isNodeKilledByChild(node, node->getChild(i), blockNum))
                 nodeKilledByChild = true;
              }

           if (!_atLeastOneExceptionPoint)
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                       _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 }
              }

           bool alreadyKilled = false;
           if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                     !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */) ||
                 _checkCastKilled->get(blockNum) ||
                 nodeKilledByChild)
              {
              _killedGenExprs[blockNum]->set(node->getLocalIndex());
              _resolveCheckKilled->set(blockNum);
              _nullCheckKilled->set(blockNum);
              _boundCheckKilled->set(blockNum);
              _divCheckKilled->set(blockNum);
              _arrayStoreCheckKilled->set(blockNum);
              _arrayCheckKilled->set(blockNum);
              _indirectAccessesKilled->set(blockNum);
              _seenImmovableExceptionPoint = true;
              alreadyKilled = true;
              }

           if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
              {
              if ((!_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) /* &&
                                                                                        !_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()) */) ||
                    _nullCheckKilled->get(blockNum) ||
                    nodeKilledByChild)
                 {
                 if (!alreadyKilled)
                    {
                    _killedGenExprs[blockNum]->set(node->getLocalIndex());
                    _resolveCheckKilled->set(blockNum);
                    _boundCheckKilled->set(blockNum);
                    _divCheckKilled->set(blockNum);
                    _arrayStoreCheckKilled->set(blockNum);
                    _arrayCheckKilled->set(blockNum);
                    }
                 _checkCastKilled->set(blockNum);
                 _indirectAccessesKilled->set(blockNum);
                 _seenImmovableExceptionPoint = true;
                 }
              else if (!alreadyKilled)
                 {
                 // Fix4
                 //
                 markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
                 }
              }
           }
        else
           {
           _seenImmovableExceptionPoint = true;
           _nullCheckKilled->set(blockNum);
           _resolveCheckKilled->set(blockNum);
           _boundCheckKilled->set(blockNum);
           _divCheckKilled->set(blockNum);
           _arrayStoreCheckKilled->set(blockNum);
           _arrayCheckKilled->set(blockNum);
           _checkCastKilled->set(blockNum);
           }

        if (_hasExceptionSuccessor)
           _atLeastOneExceptionPoint = true;
        }
     else if (node->getOpCode().isStore())
        {
        if ((node->getSymbol()->isStatic() || node->getSymbol()->isShadow()) ||
            !node->getSymbolReference()->getUseonlyAliases().isZero(comp()))
           {
           if (!_atLeastOneExceptionPoint && node->getOpCode().isIndirect() &&
              !node->getSymbolReference()->getSymbol()->isArrayletShadowSymbol() &&
              (!node->getFirstChild()->isThisPointer() ||
               !node->getFirstChild()->isNonNull()))
              {
              if ((_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
                       _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
                    (!_genSetHelper->get(node->getLocalIndex())))
                 {
                 createAndAddListElement(node, blockNum);
                 }
              }

           _seenImmovableExceptionPoint = true;
           _nullCheckKilled->set(blockNum);
           _resolveCheckKilled->set(blockNum);
           _boundCheckKilled->set(blockNum);
           _divCheckKilled->set(blockNum);
           _arrayStoreCheckKilled->set(blockNum);
           _arrayCheckKilled->set(blockNum);
           _checkCastKilled->set(blockNum);
           }
        }
     }

   //if (!_atLeastOneExceptionPoint)
      {
      int32_t i;
      bool childRelevant = false;
      for (i=0;i<node->getNumChildren();i++)
         {
         if (!_atLeastOneExceptionPoint ||
             !checkNode)
            {
            if (includeRelevantNodes(node->getChild(i), visitCount, blockNum))
               childRelevant = true;
            }
         else
            {
            TR::Node *child = node->getChild(i);
            if (checkNode &&
                (child->getVisitCount() != visitCount))
               {
               child->setVisitCount(visitCount);
               if (((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0)))
                  _relevantNodes->set(child->getLocalIndex());
               }
            }
         }
      }

#if 0
   if (((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0)) && node->getOpCode().isStore())
      {
      if (!childRelevant)
         {
         _exprsUnaffectedByOrder->set(node->getLocalIndex());
         //if (node->getOpCode().isIndirect())
         //   _indirectAccessesThatSurvive->set(node->getFirstChild()->getLocalIndex());
         //else
         _indirectAccessesThatSurvive->set(node->getLocalIndex());
         _arrayAccessesThatSurvive->set(node->getLocalIndex());
         _unresolvedAccessesThatSurvive->set(node->getLocalIndex());
         _dividesThatSurvive->set(node->getLocalIndex());
         }
      else
         {
          bool containsIndirectAccess = false, containsArrayAccess = false, containsDivide = false, containsUnresolvedAccess = false;
         for (i=0;i<node->getNumChildren();i++)
             {
             TR::Node *child = node->getChild(i);
             if (child->isInternalPointer())
                {
               TR::Node *firstChild = child->getFirstChild();
               TR::Node *secondChild = child->getSecondChild();
               if (((firstChild->getLocalIndex() != MAX_SCOUNT) && (firstChild->getLocalIndex() != 0)) /* && (!firstChild->getOpCode().isStore()) */)
                  {
                  if (_exprsContainingIndirectAccess->get(firstChild->getLocalIndex()))
                     containsIndirectAccess = true;
                  if (_exprsContainingArrayAccess->get(firstChild->getLocalIndex()))
                     containsArrayAccess = true;
                  if (_exprsContainingDivide->get(firstChild->getLocalIndex()))
                     containsDivide = true;
                  if (_exprsContainingUnresolvedAccess->get(firstChild->getLocalIndex()))
                     containsUnresolvedAccess = true;
                  }

               if (((secondChild->getLocalIndex() != MAX_SCOUNT) && (secondChild->getLocalIndex() != 0)) /* && (!secondChild->getOpCode().isStore()) */)
                  {
                  if (_exprsContainingIndirectAccess->get(secondChild->getLocalIndex()))
                     containsIndirectAccess = true;
                  if (_exprsContainingArrayAccess->get(secondChild->getLocalIndex()))
                     containsArrayAccess = true;
                  if (_exprsContainingDivide->get(secondChild->getLocalIndex()))
                     containsDivide = true;
                  if (_exprsContainingUnresolvedAccess->get(secondChild->getLocalIndex()))
                     containsUnresolvedAccess = true;
                  }
               }
            else
               {
               if (_exprsContainingIndirectAccess->get(child->getLocalIndex()))
                  containsIndirectAccess = true;
               if (_exprsContainingArrayAccess->get(child->getLocalIndex()))
                  containsArrayAccess = true;
               if (_exprsContainingDivide->get(child->getLocalIndex()))
                  containsDivide = true;
               if (_exprsContainingUnresolvedAccess->get(child->getLocalIndex()))
                  containsUnresolvedAccess = true;
               }
            }

         _relevantNodes->set(node->getLocalIndex());
         if (containsIndirectAccess)
            _exprsContainingIndirectAccess->set(node->getLocalIndex());
         if (containsArrayAccess)
            _exprsContainingArrayAccess->set(node->getLocalIndex());
         if (containsDivide)
           _exprsContainingDivide->set(node->getLocalIndex());
         if (containsUnresolvedAccess)
            _exprsContainingUnresolvedAccess->set(node->getLocalIndex());
         }
      }
#endif

   }









void TR_ExceptionCheckMotion::createAndAddListElement(TR::Node *node, int32_t blockNum)
   {
   ListElement<TR::Node> *newElement = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
   newElement->setData(node);
   newElement->setNextElement(NULL);
   if (_lastGenSetElement)
      _lastGenSetElement->setNextElement(newElement);
   else
      _regularGenSetInfo[blockNum]->setListHead(newElement);

   _genSetHelper->set(node->getLocalIndex());
   _lastGenSetElement = newElement;
   }








bool TR_ExceptionCheckMotion::isNodeKilledByChild(TR::Node *parent, TR::Node *child, int32_t blockNum)
   {
   bool parentKilled = false;

   if (((child->getLocalIndex() != MAX_SCOUNT) && (child->getLocalIndex() != 0)) /* && (!child->getOpCode().isStore()) */)
      {
      if (_exprsContainingIndirectAccess->get(child->getLocalIndex()))
         {
         _exprsContainingIndirectAccess->set(parent->getLocalIndex());
         if (_indirectAccessesKilled->get(blockNum))
            {
            if (!checkIfNodeCanSomehowSurvive(child, _indirectAccessesThatSurvive))
               parentKilled = true;
            }
         }

      if (_exprsContainingArrayAccess->get(child->getLocalIndex()))
         {
         _exprsContainingArrayAccess->set(parent->getLocalIndex());
         if (_arrayAccessesKilled->get(blockNum))
            {
            if (!checkIfNodeCanSomehowSurvive(child, _arrayAccessesThatSurvive))
               parentKilled = true;
            }
         }

      if (_exprsContainingDivide->get(child->getLocalIndex()))
         {
         _exprsContainingDivide->set(parent->getLocalIndex());
         if (_dividesKilled->get(blockNum))
            {
            if (!checkIfNodeCanSomehowSurvive(child, _dividesThatSurvive))
               parentKilled = true;
            }
         }

      if (_exprsContainingUnresolvedAccess->get(child->getLocalIndex()))
         {
         _exprsContainingUnresolvedAccess->set(parent->getLocalIndex());
         if (_unresolvedAccessesKilled->get(blockNum))
            {
            if (!checkIfNodeCanSomehowSurvive(child, _unresolvedAccessesThatSurvive))
               parentKilled = true;
            }
         }
      }
   else if (child->getOpCode().isTwoChildrenAddress())
      {
      if (isNodeKilledByChild(parent, child->getFirstChild(), blockNum) || isNodeKilledByChild(parent, child->getSecondChild(), blockNum))
         parentKilled = true;
      }

   return parentKilled;
   }





bool TR_ExceptionCheckMotion::includeRelevantNodes(TR::Node *node, vcount_t visitCount, int32_t blockNum)
   {
   if (node->getVisitCount() == visitCount)
      {
      if (((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0)) /* && (!node->getOpCode().isStore()) */)
         {
         if (_relevantNodes->get(node->getLocalIndex()))
            return true;
         }
      else if (node->getOpCode().isTwoChildrenAddress())
         {
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();
         if (((firstChild->getLocalIndex() != MAX_SCOUNT) && (firstChild->getLocalIndex() != 0)) /* && (!firstChild->getOpCode().isStore()) */)
            {
            if (_relevantNodes->get(firstChild->getLocalIndex()))
               return true;
            }

         if (((secondChild->getLocalIndex() != MAX_SCOUNT) && (secondChild->getLocalIndex() != 0)) /* && (!secondChild->getOpCode().isStore()) */)
            {
            if (_relevantNodes->get(secondChild->getLocalIndex()))
               return true;
            }
         }
       return false;
       }

   node->setVisitCount(visitCount);

   int32_t rhsChild = -1;
   if (node->getOpCode().isStore())
      rhsChild = (node->getNumChildren() - ((node->getOpCode().isWrtBar()) ? 2 : 1));

   bool childRelevant = false, containsIndirectAccess = false, containsArrayAccess = false, containsDivide = false, containsUnresolvedAccess = false;
   int32_t i;
   for (i=0;i<node->getNumChildren();i++)
      {
      if (includeRelevantNodes(node->getChild(i), visitCount, blockNum) &&
          (!node->getOpCode().isStore() ||
           (i != rhsChild)))
         {
         TR::Node *child = node->getChild(i);
         childRelevant = true;
         if (child->getOpCode().isTwoChildrenAddress())
            {
            TR::Node *firstChild = child->getFirstChild();
            TR::Node *secondChild = child->getSecondChild();
            if (((firstChild->getLocalIndex() != MAX_SCOUNT) && (firstChild->getLocalIndex() != 0)) /* && (!firstChild->getOpCode().isStore()) */)
               {
               if (_exprsContainingIndirectAccess->get(firstChild->getLocalIndex()))
                  containsIndirectAccess = true;
               if (_exprsContainingArrayAccess->get(firstChild->getLocalIndex()))
                  containsArrayAccess = true;
               if (_exprsContainingDivide->get(firstChild->getLocalIndex()))
                  containsDivide = true;
               if (_exprsContainingUnresolvedAccess->get(firstChild->getLocalIndex()))
                  containsUnresolvedAccess = true;
               }

            if (((secondChild->getLocalIndex() != MAX_SCOUNT) && (secondChild->getLocalIndex() != 0)) /* && (!secondChild->getOpCode().isStore()) */)
               {
               if (_exprsContainingIndirectAccess->get(secondChild->getLocalIndex()))
                  containsIndirectAccess = true;
               if (_exprsContainingArrayAccess->get(secondChild->getLocalIndex()))
                  containsArrayAccess = true;
               if (_exprsContainingDivide->get(secondChild->getLocalIndex()))
                  containsDivide = true;
               if (_exprsContainingUnresolvedAccess->get(secondChild->getLocalIndex()))
                  containsUnresolvedAccess = true;
               }
            }
         else
            {
            if (_exprsContainingIndirectAccess->get(child->getLocalIndex()))
               containsIndirectAccess = true;
            if (_exprsContainingArrayAccess->get(child->getLocalIndex()))
               containsArrayAccess = true;
            if (_exprsContainingDivide->get(child->getLocalIndex()))
               containsDivide = true;
            if (_exprsContainingUnresolvedAccess->get(child->getLocalIndex()))
               containsUnresolvedAccess = true;
            }
         }
      }

   TR::ILOpCode &opCode = node->getOpCode();
   TR::DataType nodeDataType = node->getDataType();

   if (((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0)) /* && (!opCode.isStore()) */)
      {
      if ((!_actualRednSetInfo[blockNum]->get(node->getLocalIndex())) &&
          (!_actualOptSetInfo[blockNum]->get(node->getLocalIndex())) &&
          (childRelevant ||
           ((opCode.isIndirect() && (opCode.isLoadVar() || opCode.isStore()) &&
             (!(opCode.hasSymbolReference() &&
                node->getSymbolReference()->getSymbol()->isArrayletShadowSymbol())) &&
             (!node->getFirstChild()->isThisPointer() ||
              !node->getFirstChild()->isNonNull())) ||
              opCode.isArrayLength()) ||
              (node->getOpCode().isTwoChildrenAddress()) ||
              (opCode.hasSymbolReference() && node->getSymbolReference()->isUnresolved()) ||
              (opCode.isDiv() || opCode.isRem())))
         {
         _relevantNodes->set(node->getLocalIndex());
         bool relevantNodeKilled = false;
         if (containsIndirectAccess || ((opCode.isIndirect() && (opCode.isLoadVar() || opCode.isStore())) || (opCode.isArrayLength())))
            {
            _exprsContainingIndirectAccess->set(node->getLocalIndex());
            if (_indirectAccessesKilled->get(blockNum))
               {
               if (!checkIfNodeCanSomehowSurvive(node, _indirectAccessesThatSurvive))
                  relevantNodeKilled = true;
               }
            }

         if (containsArrayAccess || node->getOpCode().isTwoChildrenAddress())
            {
            _exprsContainingArrayAccess->set(node->getLocalIndex());
            if (_arrayAccessesKilled->get(blockNum))
               {
               //if (!checkIfNodeCanSomehowSurvive(node, _indirectAccessesThatSurvive))
                  relevantNodeKilled = true;
               }
            }

         if (containsDivide || (opCode.isDiv() || opCode.isRem()))
            {
            _exprsContainingDivide->set(node->getLocalIndex());
            if (_dividesKilled->get(blockNum))
               {
               if (!checkIfNodeCanSomehowSurvive(node, _dividesThatSurvive))
                  relevantNodeKilled = true;
               }
            }

         if (containsUnresolvedAccess || (opCode.hasSymbolReference() && node->getSymbolReference()->isUnresolved()))
            {
            _exprsContainingUnresolvedAccess->set(node->getLocalIndex());
            if (_unresolvedAccessesKilled->get(blockNum))
               {
               if (!checkIfNodeCanSomehowSurvive(node, _unresolvedAccessesThatSurvive))
                  relevantNodeKilled = true;
               }
            }

         if (relevantNodeKilled)
            _killedGenExprs[blockNum]->set(node->getLocalIndex());

         if  (!_genSetHelper->get(node->getLocalIndex()) &&
             (_optimisticRednSetInfo[blockNum]->get(node->getLocalIndex()) ||
              _optimisticOptSetInfo[blockNum]->get(node->getLocalIndex())))
            {
            ListElement<TR::Node> *newElement = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
            newElement->setData(node);
            newElement->setNextElement(NULL);
            if (_lastGenSetElement)
               _lastGenSetElement->setNextElement(newElement);
            else
               _regularGenSetInfo[blockNum]->setListHead(newElement);

            _genSetHelper->set(node->getLocalIndex());
            _lastGenSetElement = newElement;
            }
         return true;
         }
      else
         {
         if (!(childRelevant ||
               ((opCode.isIndirect() && (opCode.isLoadVar() || opCode.isStore()) &&
                (!(opCode.hasSymbolReference() &&
                   node->getSymbolReference()->getSymbol()->isArrayletShadowSymbol())) &&
                 (!node->getFirstChild()->isThisPointer() || !node->getFirstChild()->isNonNull())) ||
                opCode.isArrayLength()) ||
             (node->getOpCode().isTwoChildrenAddress()) ||
             (opCode.hasSymbolReference() && node->getSymbolReference()->isUnresolved()) ||
             (opCode.isDiv() || opCode.isRem())))
            {
            _exprsUnaffectedByOrder->set(node->getLocalIndex());

            /*
            if (opCode.isIndirect())
               _indirectAccessesThatSurvive->set(node->getFirstChild()->getLocalIndex());
            else
               _indirectAccessesThatSurvive->set(node->getLocalIndex());

            _arrayAccessesThatSurvive->set(node->getLocalIndex());
            _unresolvedAccessesThatSurvive->set(node->getLocalIndex());
            _dividesThatSurvive->set(node->getLocalIndex());
            */
            }
         else
            {
            _relevantNodes->set(node->getLocalIndex());
            if (containsIndirectAccess || ((opCode.isIndirect() && (opCode.isLoadVar() || opCode.isStore())) || opCode.isArrayLength()))
               _exprsContainingIndirectAccess->set(node->getLocalIndex());
            if (containsArrayAccess || node->getOpCode().isTwoChildrenAddress())
               _exprsContainingArrayAccess->set(node->getLocalIndex());
            if (containsDivide || (opCode.isDiv() || opCode.isRem()))
               _exprsContainingDivide->set(node->getLocalIndex());
            if (containsUnresolvedAccess || (opCode.hasSymbolReference() && node->getSymbolReference()->isUnresolved()))
               _exprsContainingUnresolvedAccess->set(node->getLocalIndex());

            return true;
            }
         }
      }
   else if (node->getOpCode().isTwoChildrenAddress())
      {
      if (childRelevant)
         return true;
      }

   return false;
   }









void TR_ExceptionCheckMotion::initializeOutLists(List<TR::Node> **lists)
   {
   int32_t i;
   for (i=0;i<_numberOfNodes;i++)
      lists[i]->deleteAll();
   }


void TR_ExceptionCheckMotion::initializeOutList(List<TR::Node> *list)
   {
   list->deleteAll();
   }




bool TR_ExceptionCheckMotion::analyzeRegionStructure(TR_RegionStructure *regionStructure, bool checkForChange)
   {
   ExprDominanceInfo *analysisInfo = getAnalysisInfo(regionStructure);

   // Use information from last time we analyzed this structure; if
   // analyzing for the first time, initialize information
   //
   if (!regionStructure->hasBeenAnalyzedBefore())
      regionStructure->setAnalyzedStatus(true);
   else
      {
      if (trace())
         {
         traceMsg(comp(), "\nSkipping re-analysis of Region : %p numbered %d\n", regionStructure, regionStructure->getNumber());
         }
      return false;
      }

   ContainerType *exitNodes = allocateContainer(_numberOfNodes);
   TR_RegionStructure::Cursor si(*regionStructure);
   ListIterator<TR::CFGEdge> ei(&regionStructure->getExitEdges());
   TR_StructureSubGraphNode *subNode;
   TR::CFGEdge *edge;

   //
   // Copy the current out set for comparison the next time we analyze this region
   //
   //
   /////if (regionStructure != comp()->getFlowGraph()->getStructure())
      {
      ei.reset();
      for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
         {
         int32_t fromStructureNumber = edge->getFrom()->getNumber();
         int32_t toStructureNumber = edge->getTo()->getNumber();
         if (analysisInfo->_outList[toStructureNumber] != NULL)
            {
            copyListFromInto(_currentList[toStructureNumber], analysisInfo->_outList[toStructureNumber]);
            }
         exitNodes->set(fromStructureNumber);
         }
      }

      {
      si.reset();
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         if (subNode->getSuccessors().empty() && subNode->getExceptionSuccessors().empty())
            exitNodes->set(subNode->getNumber());
         }
      }

   TR_BitVector *pendingList = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
   TR_BitVector *originalPendingList = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);

   si.reset();
   for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
      {
      pendingList->set(subNode->getNumber());
      }

   bool changed = true;
   int32_t numIterations = 1;
   _firstIteration = true;

   while (changed)
      {
      //comp()->incVisitCount();
      _nodesInCycle->empty();
      changed = false;
      *pendingList |= *originalPendingList;

      if (trace())
         traceMsg(comp(), "\nREGION : %p NUMBER : %d ITERATION NUMBER : %d\n", regionStructure, regionStructure->getNumber(), numIterations);

      numIterations++;

         {
         ei.reset();
         for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
            {
            TR_StructureSubGraphNode *edgeFrom = toStructureSubGraphNode(edge->getFrom());
            addToAnalysisQueue(edgeFrom, 0);
            if (analyzeNodeIfSuccessorsAnalyzed(edgeFrom, regionStructure, pendingList, exitNodes))
               changed = true;
            }
         }

         {
         si.reset();
         for (subNode = si.getCurrent(); subNode; subNode = si.getNext())
            {
            if (subNode->getSuccessors().empty() && subNode->getExceptionSuccessors().empty())
               {
               addToAnalysisQueue(subNode, 0);
               if (analyzeNodeIfSuccessorsAnalyzed(subNode, regionStructure, pendingList, exitNodes))
                  changed = true;
               }
            }
         }

      _firstIteration = false;
      }

   List<TR::Node> *entryList = getAnalysisInfo(regionStructure->getEntry()->getStructure())->_inList;
   if (checkForChange && !compareLists(entryList, analysisInfo->_inList))
      changed = true;

   copyListFromInto(entryList, analysisInfo->_inList);

   return changed;
   }







bool TR_ExceptionCheckMotion::analyzeNodeIfSuccessorsAnalyzed(TR::CFGNode *cfgNode, TR_RegionStructure *regionStructure, ContainerType *pendingList, ContainerType *exitNodes)
   {
   bool anyNodeChanged = false;

   while (_analysisQueue.getListHead() &&
          (_analysisQueue.getListHead()->getData()->getStructure() != regionStructure))
      {
      if (!_analysisInterrupted)
   {
         numIterations++;
         if ((numIterations % 20) == 0)
      {
      numIterations = 0;
            if (comp()->compilationShouldBeInterrupted(PRE_ANALYZE_CONTEXT))
               _analysisInterrupted = true;
      }
   }

      if (_analysisInterrupted)
         {
         comp()->failCompilation<TR::CompilationInterrupted>("interrupted in forward bit vector analysis");
         }

      TR::CFGNode *node = _analysisQueue.getListHead()->getData();
      TR_StructureSubGraphNode *nodeStructure = (TR_StructureSubGraphNode *) node;

      if (!pendingList->get(nodeStructure->getStructure()->getNumber()) &&
          (*(_changedSetsQueue.getListHead()->getData()) == 0))
         {
         removeHeadFromAnalysisQueue();
         continue;
         }


      if (trace())
         traceMsg(comp(), "Begin analyzing node %p numbered %d\n", node, node->getNumber());

      bool alreadyVisitedNode = false;
      //if (nodeStructure->getVisitCount() == comp()->getVisitCount())
      if (_nodesInCycle->get(nodeStructure->getNumber()))
         alreadyVisitedNode = true;

      //nodeStructure->setVisitCount(comp()->getVisitCount());
      _nodesInCycle->set(nodeStructure->getNumber());

      ExprDominanceInfo *analysisInfo = getAnalysisInfo(nodeStructure->getStructure());

      bool isNaturalLoop = regionStructure->isNaturalLoop();

      bool stopAnalyzingThisNode = false;
      ////////initializeOutLists(_currentList);
      TR::BitVector seenNodes(comp()->allocator());
      //////bool examiningFirstSuccessor = true;
         {
         for (auto succ = nodeStructure->getSuccessors().begin(); succ != nodeStructure->getSuccessors().end(); ++succ)
            {
            if (!regionStructure->isExitEdge(*succ))
               {
               TR_StructureSubGraphNode *succNode = toStructureSubGraphNode((*succ)->getTo());
               TR_Structure *succStructure = succNode->getStructure();

               if (!(isNaturalLoop && (succNode == regionStructure->getEntry())))
                  {
                  if (pendingList->get(succNode->getNumber()) && (!alreadyVisitedNode))
                     {
                     removeHeadFromAnalysisQueue();
                     addToAnalysisQueue(succNode, 0);
                     stopAnalyzingThisNode = true;
                     break;
                     //return analyzeNodeIfSuccessorsAnalyzed(succNode, regionStructure, pendingList, exitNodes);
                     }


                  if (!(pendingList->get(succNode->getNumber()) && !succStructure->hasBeenAnalyzedBefore()))
                     {
                     if (!seenNodes.ValueAt(succStructure->getNumber()))
                        initializeOutList(_currentList[succNode->getNumber()]);
                     List<TR::Node> *succInList = getAnalysisInfo(succStructure)->_inList;
                     copyListFromInto(succInList, _currentList[succNode->getNumber()]);
                     }
                  else
                     _currentList[succNode->getNumber()]->deleteAll();
                  }

               seenNodes[succStructure->getNumber()] = true;
               }
            }

         if (stopAnalyzingThisNode)
            continue;

         for (auto succ = nodeStructure->getExceptionSuccessors().begin(); succ != nodeStructure->getExceptionSuccessors().end(); ++succ)
            {
            if (!regionStructure->isExitEdge(*succ))
               {
               TR_StructureSubGraphNode *succNode = toStructureSubGraphNode((*succ)->getTo());
               TR_Structure *succStructure = succNode->getStructure();

               if (!(isNaturalLoop && (succNode == regionStructure->getEntry())))
                  {
                  if (pendingList->get(succNode->getNumber()) && (!alreadyVisitedNode))
                     {
                     removeHeadFromAnalysisQueue();
                     addToAnalysisQueue(succNode, 0);
                     stopAnalyzingThisNode = true;
                     break;
                     //return analyzeNodeIfSuccessorsAnalyzed(succNode, regionStructure, pendingList, exitNodes);
                     }

                  if (!(pendingList->get(succNode->getNumber()) && !succStructure->hasBeenAnalyzedBefore()))
                     {
                     if (!seenNodes.ValueAt(succStructure->getNumber()))
                        initializeOutList(_currentList[succNode->getNumber()]);
                     List<TR::Node> *esuccInList = getAnalysisInfo(succStructure)->_inList;
                     copyListFromInto(esuccInList, _currentList[succNode->getNumber()]);
                     }
                  else
                     _currentList[succNode->getNumber()]->deleteAll();
                  }

               seenNodes[succStructure->getNumber()] = true;
               }
           }
         }

         if (stopAnalyzingThisNode)
            continue;

      if (exitNodes->get(nodeStructure->getNumber()))
         {
         if (!(nodeStructure->getSuccessors().empty() && nodeStructure->getExceptionSuccessors().empty()))
            {
            ExprDominanceInfo *regionExtraAnalysisInfo = getAnalysisInfo(regionStructure);
            int32_t i;
            for (i=0;i<_numberOfNodes;i++)
               {
               if ((regionExtraAnalysisInfo->_outList[i] != NULL) && (analysisInfo->_outList[i] != NULL))
                  {
                  copyListFromInto(regionExtraAnalysisInfo->_outList[i], _currentList[i]);
                  }
               }
            }
         else
            {
            int32_t i;
            for (i=0;i<_numberOfNodes;i++)
               {
               if (analysisInfo->_outList[i] != NULL)
                  _currentList[i]->deleteAll();
               }
            }
         }

      bool checkForChange  = !regionStructure->isAcyclic();
      _nodesInCycle->empty();
      bool inSetChanged = nodeStructure->getStructure()->doDataFlowAnalysis(this, checkForChange);
      if (pendingList->get(node->getNumber()))
         inSetChanged = true;
      pendingList->reset(node->getNumber());
      removeHeadFromAnalysisQueue();

      if (trace())
         {
         traceMsg(comp(), "\nOut List for Region or Block : %x numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
         int32_t j;
         for (j=0;j<_numberOfNodes;j++)
            {
            if (analysisInfo->_outList[j] != NULL)
               {
               traceMsg(comp(), "Exit or Succ numbered %d : ", j);
               ListIterator<TR::Node> outListIt(analysisInfo->_outList[j]);
               TR::Node *listNode;
               for (listNode = outListIt.getFirst(); listNode != NULL;)
                  {
                  traceMsg(comp(), " ,%d ", listNode->getLocalIndex());
                  listNode = outListIt.getNext();
                  }
               traceMsg(comp(), "\n");
               }
            }
         traceMsg(comp(), "\n");

         traceMsg(comp(), "In List for Region or Block : %x numbered %d is : \n", nodeStructure->getStructure(), nodeStructure->getStructure()->getNumber());
         if (analysisInfo->_inList != NULL)
            {
            ListIterator<TR::Node> inListIt(analysisInfo->_inList);
            TR::Node *listNode;
            for (listNode = inListIt.getFirst(); listNode != NULL;)
               {
               traceMsg(comp(), " ,%d ", listNode->getLocalIndex());
               listNode = inListIt.getNext();
               }
            traceMsg(comp(), "\n");
            }
         }

      bool changed = inSetChanged;

      for (auto pred = nodeStructure->getPredecessors().begin(); pred != nodeStructure->getPredecessors().end(); ++pred)
         {
         TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) (*pred)->getFrom();
         TR_Structure *predStructure = predNode->getStructure();
         if (pendingList->get(predStructure->getNumber()) || inSetChanged)
           {
     //comp()->incVisitCount();
           _nodesInCycle->empty();
           //if (analyzeNodeIfSuccessorsAnalyzed(pred->getFrom(), regionStructure, pendingList, exitNodes))
           //   changed = true;

           if (inSetChanged)
              addToAnalysisQueue(toStructureSubGraphNode(predNode), 1);
           else
              addToAnalysisQueue(toStructureSubGraphNode(predNode), 0);
            }
         }

      for (auto pred = nodeStructure->getExceptionPredecessors().begin(); pred != nodeStructure->getExceptionPredecessors().end(); ++pred)
         {
         TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) (*pred)->getFrom();
         TR_Structure *predStructure = predNode->getStructure();
         if (pendingList->get(predStructure->getNumber()) || inSetChanged)
            {
      //comp()->incVisitCount();
            _nodesInCycle->empty();
            //if (analyzeNodeIfSuccessorsAnalyzed(pred->getFrom(), regionStructure, pendingList, exitNodes))
            //   changed = true;

            if (inSetChanged)
               addToAnalysisQueue(toStructureSubGraphNode(predNode), 1);
            else
               addToAnalysisQueue(toStructureSubGraphNode(predNode), 0);
            }
         }

      if (changed)
         anyNodeChanged = true;
      }

   return anyNodeChanged;
   }








bool TR_ExceptionCheckMotion::analyzeBlockStructure(TR_BlockStructure *blockStructure, bool checkForChange)
   {
   _workingList.deleteAll();

   // Use information from last time we analyzed this structure; if
   // analyzing for the first time, initialize information
   //
   ExprDominanceInfo *analysisInfo = getAnalysisInfo(blockStructure);
   int32_t blockNum = blockStructure->getNumber();

   if (!blockStructure->hasBeenAnalyzedBefore())
      blockStructure->setAnalyzedStatus(true);
   else
      {
      if (trace())
         {
         traceMsg(comp(), "\nSkipping re-analysis of Block : %p numbered %d\n", blockStructure, blockStructure->getNumber());
         }
      return false;
      }


   // Find which indirect accesses can survive by looking at NULLCHKs
   // that are available at this program point
   //
   ContainerType **unavailableSetInfo = _partialRedundancy->getUnavailableSetInfo();
   _tempContainer->setAll(_partialRedundancy->getNumberOfBits());
   *_tempContainer -= *(unavailableSetInfo[blockNum]);
   *_tempContainer &= *_partialRedundancy->_isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localTransparency.getAnalysisInfo(blockNum);
   TR::Node **supportedNodesAsArray = _partialRedundancy->_isolatedness->_supportedNodesAsArray;
   _composeHelper->empty();
   ContainerType::Cursor bvi(*_tempContainer);
   for (bvi.SetToFirstOne(); bvi.Valid(); bvi.SetToNextOne())
      {
      int32_t nextAvailableComputation = bvi;
      if (nextAvailableComputation == 0)
         continue;

      TR::Node *nextAvailableNode = supportedNodesAsArray[nextAvailableComputation];
      TR::ILOpCode &opCode = nextAvailableNode->getOpCode();

      if (opCode.getOpCodeValue() == TR::NULLCHK)
         {
         _composeHelper->set(nextAvailableNode->getNullCheckReference()->getLocalIndex());
         }
      }

   *_tempContainer = *_composeHelper;

   bool examiningFirstSuccessor = true;
   int32_t i;
   for (i=0;i<_numberOfNodes;i++)
      {
      if (analysisInfo->_outList[i] != NULL)
         {
         copyListFromInto(_currentList[i], analysisInfo->_outList[i]);
         if (examiningFirstSuccessor)
            {
            examiningFirstSuccessor = false;
            copyListFromInto(_currentList[i], &_workingList);
            }
         else
            {
            composeLists(&_workingList, _currentList[i], _tempContainer);
            }
         }
      }

   bool changed = false;

   // Accumalate the kill information for this block
   // Set flags for those kinds of checks that are known to be killed by
   // this block based on information collected during initialization
   // of gen and kill information
   //
   bool nullCheckKilled = false;
   if (_nullCheckKilled->get(blockNum))
      nullCheckKilled = true;
   bool resolveCheckKilled = false;
   if (_resolveCheckKilled->get(blockNum))
      resolveCheckKilled = true;
   bool boundCheckKilled = false;
   if (_boundCheckKilled->get(blockNum))
       boundCheckKilled = true;
   bool divCheckKilled = false;
   if (_divCheckKilled->get(blockNum))
      divCheckKilled = true;
   bool checkCastKilled = false;
   if (_checkCastKilled->get(blockNum))
      checkCastKilled = true;
   bool indirectAccessesKilled = false;
   if (_indirectAccessesKilled->get(blockNum))
      indirectAccessesKilled = true;
   bool arrayAccessesKilled = false;
   if (_arrayAccessesKilled->get(blockNum))
      arrayAccessesKilled = true;
   bool unresolvedAccessesKilled = false;
   if (_unresolvedAccessesKilled->get(blockNum))
      unresolvedAccessesKilled = true;
   bool dividesKilled = false;
   if (_dividesKilled->get(blockNum))
      dividesKilled = true;
   bool arrayStoreCheckKilled = false;
   if (_arrayStoreCheckKilled->get(blockNum))
      arrayStoreCheckKilled = true;
   bool arrayCheckKilled = false;
   if (_arrayCheckKilled->get(blockNum))
      arrayCheckKilled = true;

   _knownIndirectAccessesThatSurvive->empty();
   _knownArrayAccessesThatSurvive->empty();
   _knownDividesThatSurvive->empty();
   _knownUnresolvedAccessesThatSurvive->empty();

   _tempContainer->setAll(_partialRedundancy->getNumberOfBits());
   *_tempContainer -= *(unavailableSetInfo[blockNum]);
   *_tempContainer &= *_partialRedundancy->_isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localTransparency.getAnalysisInfo(blockNum);

   ///// Changed by Dave S to save call to elementCount
   /////int32_t j = 0;
   /////int32_t numAvailExprs = _temp->elementCount();
   /////for (nextAvailableComputation = bvi.getFirstElement(); j < numAvailExprs; nextAvailableComputation = bvi.getNextElement())
   int32_t nextAvailableComputation;
   ContainerType::Cursor bvi2(*_tempContainer);
   for (bvi2.SetToFirstOne(); bvi2.Valid(); bvi2.SetToNextOne())
      {
      nextAvailableComputation = bvi2;
      if (nextAvailableComputation == 0)
         continue;

      TR::Node *nextAvailableNode = supportedNodesAsArray[nextAvailableComputation];
      TR::ILOpCode &opCode = nextAvailableNode->getOpCode();

      if (opCode.getOpCodeValue() == TR::NULLCHK)
         {
         markNodeAsSurvivor(nextAvailableNode->getNullCheckReference(), _knownIndirectAccessesThatSurvive);
         }
      else if (opCode.isIndirect() || opCode.isArrayLength())
         {
         markNodeAsSurvivor(nextAvailableNode->getFirstChild(), _knownIndirectAccessesThatSurvive);
         }
      else if (opCode.isResolveCheck())
         {
         markNodeAsSurvivor(nextAvailableNode->getFirstChild(), _knownUnresolvedAccessesThatSurvive);
         }
      else if (opCode.getOpCodeValue() == TR::DIVCHK)
         {
         markNodeAsSurvivor(nextAvailableNode->getFirstChild()->getSecondChild(), _knownDividesThatSurvive);
         }
      else if (opCode.isDiv() || opCode.isRem())
         {
         markNodeAsSurvivor(nextAvailableNode->getSecondChild(), _knownDividesThatSurvive);
         }
      /*
      else if (opCode.isBndCheck())
         {
         }
      else if (opCode.getOpCodeValue() == TR::ArrayStoreCHK)
         {
         }
      else if (opCode.getOpCodeValue() == TR::ArrayCHK)
         {
         }
      else if (opCode.getOpCodeValue() == TR::checkcast)
         {
         }
      */
      }


   *_indirectAccessesThatSurvive = *_knownIndirectAccessesThatSurvive;
   *_arrayAccessesThatSurvive = *_knownArrayAccessesThatSurvive;
   *_dividesThatSurvive = *_knownDividesThatSurvive;
   *_unresolvedAccessesThatSurvive = *_knownUnresolvedAccessesThatSurvive;

   ////dumpOptDetails(comp(), "Nodes that survive\n");
   ////_nodesThatSurvive->print(comp());

   // Use the information about this block collected earlier (during gen/kill list
   // generation) to prune the incoming ordered list from the successors.
   //
   ListElement<TR::Node> *prevElem = NULL;
   ListElement<TR::Node> *listElem;
   for (listElem = _workingList.getListHead(); listElem != NULL; listElem = listElem->getNextElement())
      {
      TR::Node *node = listElem->getData();
      TR::ILOpCode &opCode = node->getOpCode();
      bool checkKilledByChild = false;

      // Handle non checks first and later handle checks
      //
      // If this expression contains an indirect access and the block
      // kills indirect accesses then remove this expression from the incoming list;
      // also mark this expression as one that will kill its parent (because it was
      // killed) if the parent is a check taking part in this analysis
      //
      if (_exprsContainingIndirectAccess->get(node->getLocalIndex()))
         {
         if (indirectAccessesKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _indirectAccessesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  removeFromList(listElem, &_workingList, prevElem);
               }
            }
         }

      // Do the same as above for expressions containing unresolved access
      //
      if (_exprsContainingUnresolvedAccess->get(node->getLocalIndex()))
         {
         if (unresolvedAccessesKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _unresolvedAccessesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  removeFromList(listElem, &_workingList, prevElem);
               }
            }
         }

      // Do the same as above for expressions containing array access
      //
      if (_exprsContainingArrayAccess->get(node->getLocalIndex()))
         {
         if (arrayAccessesKilled)
            {
            ///if (!checkIfNodeCanSomehowSurvive(node, _arrayAccessesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  removeFromList(listElem, &_workingList, prevElem);
               }
            }
         }

      // Do the same as above for expressions containing division
      //
      if (_exprsContainingDivide->get(node->getLocalIndex()))
         {
         if (dividesKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _dividesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  removeFromList(listElem, &_workingList, prevElem);
               }
            }
         }


      // Now handle checks
      //
      // If null checks are killed by this block or this particular null check is
      // killed by its child (for reasons mentioned earlier), then checks of other
      // kinds cannot move past this point. The code below implements this kind of logic
      // for all the different check opcodes.
      //
      if (opCode.getOpCodeValue() == TR::NULLCHK)
         {
         if (nullCheckKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            resolveCheckKilled = true;
            boundCheckKilled = true;
            divCheckKilled = true;
            arrayStoreCheckKilled = true;
            arrayCheckKilled = true;
            checkCastKilled = true;
            indirectAccessesKilled = true;
            }
         else
            markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
         }
      else if (opCode.isResolveCheck())
         {
         bool nodeKilled = resolveCheckKilled;
         if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
            {
            if (nullCheckKilled)
              nodeKilled = true;
            }

         if (nodeKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            nullCheckKilled = true;
            boundCheckKilled = true;
            divCheckKilled = true;
            arrayStoreCheckKilled = true;
            arrayCheckKilled = true;
            checkCastKilled = true;
            unresolvedAccessesKilled = true;
            if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
               {
               resolveCheckKilled = true;
               indirectAccessesKilled = true;
               }
            }
         else
            markNodeAsSurvivor(node->getFirstChild(), _unresolvedAccessesThatSurvive);
         }
      else if (opCode.isBndCheck() || opCode.isSpineCheck())
         {
         if (boundCheckKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            nullCheckKilled = true;
            resolveCheckKilled = true;
            divCheckKilled = true;
            arrayStoreCheckKilled = true;
            arrayCheckKilled = true;
            checkCastKilled = true;
            arrayAccessesKilled = true;
            }
         }
      else if (opCode.getOpCodeValue() == TR::DIVCHK)
         {
         if (divCheckKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            nullCheckKilled = true;
            resolveCheckKilled = true;
            boundCheckKilled = true;
            arrayStoreCheckKilled = true;
            arrayCheckKilled = true;
            checkCastKilled = true;
            dividesKilled = true;
            }
         else
            markNodeAsSurvivor(node->getFirstChild()->getSecondChild(), _dividesThatSurvive);
         }
      else if (opCode.getOpCodeValue() == TR::ArrayStoreCHK)
         {
         if (arrayStoreCheckKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            nullCheckKilled = true;
            resolveCheckKilled = true;
            boundCheckKilled = true;
            checkCastKilled = true;
            divCheckKilled = true;
            arrayCheckKilled = true;
            }
         }
      else if (opCode.getOpCodeValue() == TR::ArrayCHK)
         {
         if (arrayCheckKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            nullCheckKilled = true;
            resolveCheckKilled = true;
            boundCheckKilled = true;
            checkCastKilled = true;
            divCheckKilled = true;
            arrayStoreCheckKilled = true;
            }
         }
      else if (opCode.isCheckCast())///opCode.getOpCodeValue() == TR::checkcast)
         {
   bool alreadyRemoved = false;
         if (checkCastKilled || checkKilledByChild)
            {
            removeFromList(listElem, &_workingList, prevElem);
            nullCheckKilled = true;
            resolveCheckKilled = true;
            boundCheckKilled = true;
            divCheckKilled = true;
            arrayStoreCheckKilled = true;
            arrayCheckKilled = true;
            indirectAccessesKilled = true;
            alreadyRemoved = true;
            }

         if (opCode.getOpCodeValue() == TR::checkcastAndNULLCHK)
      {
            if (nullCheckKilled || checkKilledByChild)
               {
         if (!alreadyRemoved)
      {
                  removeFromList(listElem, &_workingList, prevElem);
                  resolveCheckKilled = true;
                  boundCheckKilled = true;
                  divCheckKilled = true;
                  arrayStoreCheckKilled = true;
                  arrayCheckKilled = true;
      }
               checkCastKilled = true;
               indirectAccessesKilled = true;
               }
            else if (!alreadyRemoved)
               markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
      }

         //else
         //   markNodeAsSurvivor(node->getFirstChild());
         }

      if ((prevElem && (prevElem->getNextElement() == listElem)) || (_workingList.getListHead() == listElem))
         prevElem = listElem;
      }

   //
   // Having pruned the incoming ordered list now (IN - KILL in dataflow terms)
   // we now union it with the ordered list generated by this block
   // (GEN U (IN-KILL) in dataflow terms); note that the ordering
   // puts the checks generated by this block before (earlier than) the
   // expressions coming in because of dataflow
   //

    if (trace())
       {
       traceMsg(comp(), "\nIncoming List for Block : %p numbered %d is : \n", blockStructure, blockStructure->getNumber());
       //if (_regularGenSetInfo[blockNum] != NULL)
           {
           ListIterator<TR::Node> workListIt(&_workingList);
           TR::Node *listNode;
           for (listNode = workListIt.getFirst(); listNode != NULL;)
              {
              traceMsg(comp(), " ,%d ", listNode->getLocalIndex());
              listNode = workListIt.getNext();
              }
           traceMsg(comp(), "\n");
           }
        }


   copyListFromInto(_regularGenSetInfo[blockNum], _blockInfo[blockNum]);
   appendLists(_blockInfo[blockNum], &_workingList);

   //
   // We are now ready to add whichever expressions are optimal in this block
   // into the ordered opt list for this block. Note that there may be some
   // elements in the ordered opt list for this block already (from prev
   // iterations of expression dominance) in which case we would like to append the
   // current solution to the already existing opt list. Also we would like
   // to prune the ordered list going out of this block (OUT in dataflow terms)
   // based on the check expressions already in the ordered opt list for this block; this
   // is reqd as these expressions in the existing ordered opt list for this block
   // were not taken into account in gen and kill generation/initialization time
   //
   // Collect information for which kinds of checks/expressions would be
   // killed by the existing opt list; basically set appropriate flags
   //
   bool nullCheckKilledByOptList = false;
   bool resolveCheckKilledByOptList = false;
   bool boundCheckKilledByOptList = false;
   bool divCheckKilledByOptList = false;
   bool arrayStoreCheckKilledByOptList = false;
   bool arrayCheckKilledByOptList = false;
   bool checkCastKilledByOptList = false;
   bool indirectAccessesKilledByOptList = false;
   bool arrayAccessesKilledByOptList = false;
   bool dividesKilledByOptList = false;
   bool unresolvedAccessesKilledByOptList = false;

   if (_orderedOptList[blockNum])
      {
      ListElement<TR::Node> *nextOptElement;
      for (nextOptElement = _orderedOptList[blockNum]->getListHead(); nextOptElement != NULL; nextOptElement = nextOptElement->getNextElement())
         {
         TR::ILOpCode &opCode = nextOptElement->getData()->getOpCode();
         if (opCode.isCheck())
            {
            if (opCode.getOpCodeValue() == TR::NULLCHK)
               {
               resolveCheckKilledByOptList = true;
               boundCheckKilledByOptList = true;
               divCheckKilledByOptList = true;
               arrayStoreCheckKilledByOptList = true;
               arrayCheckKilledByOptList = true;
               checkCastKilledByOptList = true;
               indirectAccessesKilledByOptList = true;
               }
            else if (opCode.isResolveCheck())
               {
               nullCheckKilledByOptList = true;
               boundCheckKilledByOptList = true;
               divCheckKilledByOptList = true;
               arrayStoreCheckKilledByOptList = true;
               arrayCheckKilledByOptList = true;
               checkCastKilledByOptList = true;
               unresolvedAccessesKilledByOptList = true;
               if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
                  {
                  indirectAccessesKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  }
               }
            else if (opCode.isBndCheck() || opCode.isSpineCheck())
               {
               nullCheckKilledByOptList = true;
               resolveCheckKilledByOptList = true;
               divCheckKilledByOptList = true;
               arrayStoreCheckKilledByOptList = true;
               arrayCheckKilledByOptList = true;
               checkCastKilledByOptList = true;
               arrayAccessesKilledByOptList = true;
               }
            else if (opCode.getOpCodeValue() == TR::DIVCHK)
               {
               nullCheckKilledByOptList = true;
               resolveCheckKilledByOptList = true;
               boundCheckKilledByOptList = true;
               arrayStoreCheckKilledByOptList = true;
               arrayCheckKilledByOptList = true;
               checkCastKilledByOptList = true;
               dividesKilledByOptList = true;
               }
            else if (opCode.getOpCodeValue() == TR::ArrayStoreCHK)
               {
               nullCheckKilledByOptList = true;
               resolveCheckKilledByOptList = true;
               boundCheckKilledByOptList = true;
               checkCastKilledByOptList = true;
               divCheckKilledByOptList = true;
               arrayCheckKilledByOptList = true;
               }
            else if (opCode.getOpCodeValue() == TR::ArrayCHK)
               {
               nullCheckKilledByOptList = true;
               resolveCheckKilledByOptList = true;
               boundCheckKilledByOptList = true;
               checkCastKilledByOptList = true;
               divCheckKilledByOptList = true;
               arrayStoreCheckKilledByOptList = true;
               }
            else if (opCode.isCheckCast())///opCode.getOpCodeValue() == TR::checkcast)
               {
               nullCheckKilledByOptList = true;
               resolveCheckKilledByOptList = true;
               boundCheckKilledByOptList = true;
               arrayStoreCheckKilledByOptList = true;
               divCheckKilledByOptList = true;
               arrayCheckKilledByOptList = true;
               indirectAccessesKilledByOptList = true;
               if (opCode.getOpCodeValue() == TR::checkcastAndNULLCHK)
                  checkCastKilledByOptList = true;
               }
            }
         }
      }

   *_indirectAccessesThatSurvive = *_knownIndirectAccessesThatSurvive;
   *_arrayAccessesThatSurvive = *_knownArrayAccessesThatSurvive;
   *_dividesThatSurvive = *_knownDividesThatSurvive;
   *_unresolvedAccessesThatSurvive = *_knownUnresolvedAccessesThatSurvive;
   //
   // Now consider the checks in the (solution) ordered list for this
   // block in order to find which expressions can be appended to the
   // existing opt list
   //
   nullCheckKilled = false;
   resolveCheckKilled = false;
   boundCheckKilled = false;
   divCheckKilled = false;
   indirectAccessesKilled = false;
   checkCastKilled = false;
   unresolvedAccessesKilled = false;
   arrayAccessesKilled = false;
   dividesKilled = false;
   arrayStoreCheckKilled = false;
   bool optimalIndirectAccessesKilled = false;
   bool optimalUnresolvedAccessesKilled = false;
   bool optimalArrayAccessesKilled = false;
   bool optimalDividesKilled = false;

   if (trace())
      {
      if (_blockInfo[blockNum]->isEmpty())
         traceMsg(comp(), "IN ORDER Block : %d has NO expr\n", blockNum);
      }

   prevElem = NULL;
   ListElement<TR::Node> *nextOptElement = NULL;
   bool placedEarlierCheckOptimally = false;
   for (listElem = _blockInfo[blockNum]->getListHead(); listElem != NULL; listElem = listElem->getNextElement())
      {
      bool checkElement = false;
      TR::Node *node = listElem->getData();

      if (node->getOpCode().isCheck())
         {
         checkElement = true;
         }

      if (!_optimisticOptSetInfo[blockNum]->get(node->getLocalIndex()))
         {
         //
         // This expression is NOT an opt candidate for this block
         // as it is not even in the optimistic solution (set) computed
         // by PRE
         //
         TR::ILOpCode &opCode = node->getOpCode();
         bool checkKilledByChild = false;

         // Handle non check expressions first; then check expressions
         //
         if (placedEarlierCheckOptimally)
            {
            //
            // If a null check has been placed as an opt expression earlier in this block
            // then kill this expression if it contains an indirect access as it is not
            // allowed to cross this block (it is not allowed to move past the check)
            //
            if (_exprsContainingIndirectAccess->get(node->getLocalIndex()))
               {
               if (indirectAccessesKilled)
                  {
                  if (!checkIfNodeCanSomehowSurvive(node, _indirectAccessesThatSurvive))
                     {
                     checkKilledByChild = true;
                     if (!opCode.isCheck())
                        removeFromList(listElem, _blockInfo[blockNum], prevElem);
                     }
                   }
                }

            // Similar logic as above for resolve check and unresolved access
            //
            if (_exprsContainingUnresolvedAccess->get(node->getLocalIndex()))
               {
               if (unresolvedAccessesKilled)
                  {
                  if (!checkIfNodeCanSomehowSurvive(node, _unresolvedAccessesThatSurvive))
                     {
                     checkKilledByChild = true;
                     if (!opCode.isCheck())
                        removeFromList(listElem, _blockInfo[blockNum], prevElem);
                     }
                  }
               }

            // Similar logic as above for array bound check and array access
            //
            if (_exprsContainingArrayAccess->get(node->getLocalIndex()))
               {
               if (arrayAccessesKilled)
                  {
                  ///if (!checkIfNodeCanSomehowSurvive(node, _arrayAccessesThatSurvive))
                     {
                     checkKilledByChild = true;
                     if (!opCode.isCheck())
                        removeFromList(listElem, _blockInfo[blockNum], prevElem);
                     }
                   }
                }

            // Similar logic as above for div check and division expression
            //
            if (_exprsContainingDivide->get(node->getLocalIndex()))
               {
               if (dividesKilled)
                  {
                  if (!checkIfNodeCanSomehowSurvive(node, _dividesThatSurvive))
                     {
                     checkKilledByChild = true;
                     if (!opCode.isCheck())
                        removeFromList(listElem, _blockInfo[blockNum], prevElem);
                     }
                  }
               }
            }

         // Now handle checks
         //
         // If this null check in NOT an opt expression in this block
         // then kill this expression if another different kind of check has
         // already been placed; this is because this null check is then not
         // allowed to move past the other check. Note that as a consequence
         // all other kinds of checks must be killed now as they cannot move
         // past this null check
         //
         // Note the code that follows for the other checks has similar logic
         // that specified above
         //
         if (opCode.getOpCodeValue() == TR::NULLCHK)
            {
            optimalIndirectAccessesKilled = true;

            if (placedEarlierCheckOptimally && (nullCheckKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               boundCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               indirectAccessesKilled = true;
               }
            else
               markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
            }
         else if (opCode.isResolveCheck())
            {
            optimalUnresolvedAccessesKilled = true;

            bool nodeKilled = resolveCheckKilled;
            if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
               {
               optimalIndirectAccessesKilled = true;
               if (nullCheckKilled)
                  nodeKilled = true;
               }

            if (placedEarlierCheckOptimally && (nodeKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               nullCheckKilled = true;
               boundCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               unresolvedAccessesKilled = true;
               if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
                  {
                  resolveCheckKilled = true;
                  indirectAccessesKilled = true;
                  }
               }
            else
               markNodeAsSurvivor(node->getFirstChild(), _unresolvedAccessesThatSurvive);
            }
         else if (opCode.isBndCheck() || opCode.isSpineCheck())
            {
            optimalArrayAccessesKilled = true;

            if (placedEarlierCheckOptimally && (boundCheckKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               nullCheckKilled = true;
               resolveCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               arrayAccessesKilled = true;
               }
            }
         else if (opCode.getOpCodeValue() == TR::DIVCHK)
            {
            optimalDividesKilled = true;

            if (placedEarlierCheckOptimally && (divCheckKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               resolveCheckKilled = true;
               nullCheckKilled = true;
               boundCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               dividesKilled = true;
               }
            else
               markNodeAsSurvivor(node->getFirstChild()->getSecondChild(), _dividesThatSurvive);
            }
         else if (opCode.getOpCodeValue() == TR::ArrayCHK)
            {
            if (placedEarlierCheckOptimally && (arrayStoreCheckKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               nullCheckKilled = true;
               resolveCheckKilled = true;
               boundCheckKilled = true;
               divCheckKilled = true;
               checkCastKilled = true;
               arrayStoreCheckKilled = true;
               }
            }
         else if (opCode.getOpCodeValue() == TR::ArrayStoreCHK)
            {
            if (placedEarlierCheckOptimally && (arrayStoreCheckKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               nullCheckKilled = true;
               resolveCheckKilled = true;
               boundCheckKilled = true;
               divCheckKilled = true;
               checkCastKilled = true;
               arrayCheckKilled = true;
               }
            }
         else if (opCode.isCheckCast())///opCode.getOpCodeValue() == TR::checkcast)
            {
      bool alreadyRemoved = false;
            if (placedEarlierCheckOptimally && (checkCastKilled || checkKilledByChild))
               {
               removeFromList(listElem, _blockInfo[blockNum], prevElem);
               resolveCheckKilled = true;
               nullCheckKilled = true;
               boundCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               indirectAccessesKilled = true;
               alreadyRemoved = true;
               }

            if (opCode.getOpCodeValue() == TR::checkcastAndNULLCHK)
         {
               optimalIndirectAccessesKilled = true;
               if (placedEarlierCheckOptimally && (nullCheckKilled || checkKilledByChild))
                  {
            if (!alreadyRemoved)
         {
                     removeFromList(listElem, _blockInfo[blockNum], prevElem);
                     boundCheckKilled = true;
                     divCheckKilled = true;
                     arrayStoreCheckKilled = true;
                     arrayCheckKilled = true;
         }
                  checkCastKilled = true;
                  indirectAccessesKilled = true;
                  }
               else if (!alreadyRemoved)
                  markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
         }

            //else
            //   markNodeAsSurvivor(node->getFirstChild());
            }
         }
      else
         {
         // This expression is an opt candidate for this block
         // as it belongs in the optimistic solution (set) computed
         // by PRE
         //
         bool canPlaceOptimally = true;
         //
         // Cannot place optimally if the expression has a sub-expression
         // that requires a check (and it has not been placed optimally in this block).
         // Note that if the required check has been placed optimally in this block
         // or earlier, then we will go ahead and place this expression optimally
         // in this block. This is driven by the optimalACCESSKilled flags rather than the
         // ACCESSKilled flags (which are really used to limit which expressions can stay
         // in the out list for this block eventually)
         //
         if (_exprsContainingIndirectAccess->get(listElem->getData()->getLocalIndex()) && optimalIndirectAccessesKilled)
            canPlaceOptimally = false;
         else if (_exprsContainingArrayAccess->get(listElem->getData()->getLocalIndex()) && optimalArrayAccessesKilled)
            canPlaceOptimally = false;
         else if (_exprsContainingDivide->get(listElem->getData()->getLocalIndex()) && optimalDividesKilled)
            canPlaceOptimally = false;
         else if (_exprsContainingUnresolvedAccess->get(listElem->getData()->getLocalIndex()) && optimalUnresolvedAccessesKilled)
            canPlaceOptimally = false;

         if (checkElement)
            {
            TR::ILOpCodes opCodeValue = listElem->getData()->getOpCodeValue();
            placedEarlierCheckOptimally = true;
            if (opCodeValue == TR::NULLCHK)
               {
               resolveCheckKilled = true;
               boundCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               indirectAccessesKilled = true;
               }
            else if (listElem->getData()->getOpCode().isResolveCheck())
               {
               nullCheckKilled = true;
               boundCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               unresolvedAccessesKilled = true;
               if (opCodeValue == TR::ResolveAndNULLCHK)
                  {
                  resolveCheckKilled = true;
                  indirectAccessesKilled = true;
                  }
               }
            else if (listElem->getData()->getOpCode().isBndCheck() || listElem->getData()->getOpCode().isSpineCheck())
               {
               nullCheckKilled = true;
               resolveCheckKilled = true;
               divCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               arrayAccessesKilled = true;
               }
            else if (opCodeValue == TR::DIVCHK)
               {
               nullCheckKilled = true;
               resolveCheckKilled = true;
               boundCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               checkCastKilled = true;
               dividesKilled = true;
               }
            else if (opCodeValue == TR::ArrayStoreCHK)
               {
               nullCheckKilled = true;
               resolveCheckKilled = true;
               boundCheckKilled = true;
               checkCastKilled = true;
               arrayCheckKilled = true;
               divCheckKilled = true;
               }
            else if (opCodeValue == TR::ArrayCHK)
               {
               nullCheckKilled = true;
               resolveCheckKilled = true;
               boundCheckKilled = true;
               checkCastKilled = true;
               arrayStoreCheckKilled = true;
               divCheckKilled = true;
               }
            else if (listElem->getData()->getOpCode().isCheckCast())///opCodeValue == TR::checkcast)
               {
               nullCheckKilled = true;
               resolveCheckKilled = true;
               boundCheckKilled = true;
               arrayStoreCheckKilled = true;
               arrayCheckKilled = true;
               divCheckKilled = true;
               indirectAccessesKilled = true;
               if (listElem->getData()->getOpCodeValue() == TR::checkcastAndNULLCHK)
            checkCastKilled = true;
               }
            }

         // Actually add this expression into the optimal list for this block
         // in the correct order
         //
         if (canPlaceOptimally && !_actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            _tryAnotherIteration = true;
            if (trace())
               traceMsg(comp(), "IN ORDER Expr %d (representative) Node %p (listElem %p) in Block : %d\n", listElem->getData()->getLocalIndex(), listElem->getData(), listElem, blockNum);

            if (checkElement)
               {
               TR::ILOpCodes opCodeValue = listElem->getData()->getOpCodeValue();
               placedEarlierCheckOptimally = true;
               if (opCodeValue == TR::NULLCHK)
                  {
                  resolveCheckKilledByOptList = true;
                  boundCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
                  checkCastKilledByOptList = true;
                  indirectAccessesKilledByOptList = true;
                  }
               else if (listElem->getData()->getOpCode().isResolveCheck())
                  {
                  nullCheckKilledByOptList = true;
                  boundCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
                  checkCastKilledByOptList = true;
                  unresolvedAccessesKilledByOptList = true;
                  if (opCodeValue == TR::ResolveAndNULLCHK)
                     {
                     resolveCheckKilledByOptList = true;
                     indirectAccessesKilledByOptList = true;
                     }
                  }
               else if (listElem->getData()->getOpCode().isBndCheck() || listElem->getData()->getOpCode().isBndCheck())
                  {
                  nullCheckKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
                  checkCastKilledByOptList = true;
                  arrayAccessesKilledByOptList = true;
                  }
               else if (opCodeValue == TR::DIVCHK)
                  {
                  nullCheckKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  boundCheckKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
                  checkCastKilledByOptList = true;
                  dividesKilledByOptList = true;
                  }
               else if (opCodeValue == TR::ArrayStoreCHK)
                  {
                  nullCheckKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  boundCheckKilledByOptList = true;
                  checkCastKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  }
               else if (opCodeValue == TR::ArrayCHK)
                  {
                  nullCheckKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  boundCheckKilledByOptList = true;
                  checkCastKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  }
               else if (listElem->getData()->getOpCode().isCheckCast())///opCodeValue == TR::checkcast)
                  {
                  nullCheckKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  boundCheckKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  indirectAccessesKilledByOptList = true;
                  if (listElem->getData()->getOpCodeValue() == TR::checkcastAndNULLCHK)
                     checkCastKilledByOptList = true;
                  }
               }

            _actualOptSetInfo[blockNum]->set(listElem->getData()->getLocalIndex());

            if (!_orderedOptList[blockNum])
               _orderedOptList[blockNum] = new (trStackMemory())TR_ScratchList<TR::Node>(trMemory());

            if (!nextOptElement)
               {
               ListElement<TR::Node> *prevOptElement = NULL;
               for (nextOptElement = _orderedOptList[blockNum]->getListHead(); nextOptElement != NULL; nextOptElement = nextOptElement->getNextElement())
                  prevOptElement = nextOptElement;
               nextOptElement = prevOptElement;
               }

            ListElement<TR::Node> *newElement = (ListElement<TR::Node>*)trMemory()->allocateStackMemory(sizeof(ListElement<TR::Node>));
            newElement->setData(listElem->getData());
            newElement->setNextElement(NULL);

            if (nextOptElement)
               nextOptElement->setNextElement(newElement);
            else
               {
               _orderedOptList[blockNum]->setListHead(newElement);
               }

            nextOptElement = newElement;
            //
            // Add into the result list that maintains the opt exprs in order
            //
            }
         }


      if ((prevElem && (prevElem->getNextElement() == listElem)) || (_blockInfo[blockNum]->getListHead() == listElem))
         prevElem = listElem;
      }

   *_indirectAccessesThatSurvive = *_knownIndirectAccessesThatSurvive;
   *_arrayAccessesThatSurvive = *_knownArrayAccessesThatSurvive;
   *_dividesThatSurvive = *_knownDividesThatSurvive;
   *_unresolvedAccessesThatSurvive = *_knownUnresolvedAccessesThatSurvive;

   if (trace())
      {
      traceMsg(comp(), "Following gen exprs for block_%d cannot survive\n", blockNum);
      _killedGenExprs[blockNum]->print(comp());
      traceMsg(comp(), "Known indirect exprs for block_%d that survive\n", blockNum);
      _knownIndirectAccessesThatSurvive->print(comp());
      }

   // This is the list for this block; note that this is not what we can flow backwards from this point
   // Some expressions that are computed in this block are okay to keep in this block but not okay to "move"
   // any further back.
   //
    _workingList.deleteAll();
   copyListFromInto(_blockInfo[blockNum], &_workingList);

   // For the remaining expressions in the ordered list at this block,
   // prune the list based on what gets killed by existing ordered list
   //
   prevElem = NULL;
   for (listElem = _blockInfo[blockNum]->getListHead(); listElem != NULL; listElem = listElem->getNextElement())
      {
      TR::Node *node = listElem->getData();
      TR::ILOpCode &opCode = listElem->getData()->getOpCode();
      bool checkKilledByChild = false;
      bool genExprThatIsKilled = false;
      if (_killedGenExprs[blockNum]->get(listElem->getData()->getLocalIndex()))
         genExprThatIsKilled = true;

      if (_exprsContainingIndirectAccess->get(listElem->getData()->getLocalIndex()))
         {
         if (indirectAccessesKilledByOptList ||
             genExprThatIsKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _indirectAccessesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  {
                  removeFromList(listElem, _blockInfo[blockNum], prevElem);
                  }
               }
            }
         }


      if (_exprsContainingUnresolvedAccess->get(listElem->getData()->getLocalIndex()))
         {
         if (unresolvedAccessesKilledByOptList ||
             genExprThatIsKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _unresolvedAccessesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  {
                  removeFromList(listElem, _blockInfo[blockNum], prevElem);
                  }
               }
            }
         }


      if (_exprsContainingArrayAccess->get(listElem->getData()->getLocalIndex()))
         {
         if (arrayAccessesKilledByOptList ||
             genExprThatIsKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _arrayAccessesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  {
                  removeFromList(listElem, _blockInfo[blockNum], prevElem);
                  }
               }
            }
         }

      if (_exprsContainingDivide->get(listElem->getData()->getLocalIndex()))
         {
         if (dividesKilledByOptList ||
             genExprThatIsKilled)
            {
            if (!checkIfNodeCanSomehowSurvive(node, _dividesThatSurvive))
               {
               checkKilledByChild = true;
               if (!opCode.isCheck())
                  removeFromList(listElem, _blockInfo[blockNum], prevElem);
               }
            }
         }

      if (opCode.getOpCodeValue() == TR::NULLCHK)
         {
         if (nullCheckKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            boundCheckKilledByOptList = true;
            resolveCheckKilledByOptList = true;
            divCheckKilledByOptList = true;
            arrayStoreCheckKilledByOptList = true;
            arrayCheckKilledByOptList = true;
            checkCastKilledByOptList = true;
            indirectAccessesKilledByOptList = true;
            }
         else
            markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
         }
      else if (opCode.isResolveCheck())
         {
         bool nodeKilledByOptList = resolveCheckKilledByOptList;
         if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
            {
            if (nullCheckKilledByOptList)
               nodeKilledByOptList = true;
            }

         if (nodeKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            boundCheckKilledByOptList = true;
            nullCheckKilledByOptList = true;
            divCheckKilledByOptList = true;
            arrayStoreCheckKilledByOptList = true;
            arrayCheckKilledByOptList = true;
            checkCastKilledByOptList = true;
            unresolvedAccessesKilledByOptList = true;
            if (opCode.getOpCodeValue() == TR::ResolveAndNULLCHK)
               {
               resolveCheckKilledByOptList = true;
               indirectAccessesKilledByOptList = true;
               }
            }
         else
            markNodeAsSurvivor(node->getFirstChild(), _unresolvedAccessesThatSurvive);
         }
      else if (opCode.isBndCheck() || opCode.isSpineCheck())
         {
         if (boundCheckKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            nullCheckKilledByOptList = true;
            resolveCheckKilledByOptList = true;
            divCheckKilledByOptList = true;
            arrayStoreCheckKilledByOptList = true;
            arrayCheckKilledByOptList = true;
            checkCastKilledByOptList = true;
            arrayAccessesKilledByOptList = true;
            }
         }
      else if (opCode.getOpCodeValue() == TR::DIVCHK)
         {
         if (divCheckKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            nullCheckKilledByOptList = true;
            resolveCheckKilledByOptList = true;
            boundCheckKilledByOptList = true;
            arrayStoreCheckKilledByOptList = true;
            arrayCheckKilledByOptList = true;
            checkCastKilledByOptList = true;
            dividesKilledByOptList = true;
            }
         else
            markNodeAsSurvivor(node->getFirstChild()->getSecondChild(), _dividesThatSurvive);
         }
      else if (opCode.getOpCodeValue() == TR::ArrayStoreCHK)
         {
         if (arrayStoreCheckKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            nullCheckKilledByOptList = true;
            resolveCheckKilledByOptList = true;
            boundCheckKilledByOptList = true;
            arrayCheckKilledByOptList = true;
            checkCastKilledByOptList = true;
            divCheckKilledByOptList = true;
            }
         }
      else if (opCode.getOpCodeValue() == TR::ArrayCHK)
         {
         if (arrayStoreCheckKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            nullCheckKilledByOptList = true;
            resolveCheckKilledByOptList = true;
            boundCheckKilledByOptList = true;
            checkCastKilledByOptList = true;
            arrayStoreCheckKilledByOptList = true;
            divCheckKilledByOptList = true;
            }
         }
      else if (opCode.isCheckCast())///opCode.getOpCodeValue() == TR::checkcast)
         {
   bool alreadyRemoved = false;
         if (checkCastKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
             _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
            {
            removeFromList(listElem, _blockInfo[blockNum], prevElem);
            nullCheckKilledByOptList = true;
            resolveCheckKilledByOptList = true;
            boundCheckKilledByOptList = true;
            divCheckKilledByOptList = true;
            arrayStoreCheckKilledByOptList = true;
            arrayCheckKilledByOptList = true;
            indirectAccessesKilledByOptList = true;
            alreadyRemoved = true;
            }

         if (opCode.getOpCodeValue() == TR::checkcastAndNULLCHK)
      {
            if (nullCheckKilledByOptList || checkKilledByChild || genExprThatIsKilled ||
                _actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))
               {
         if (!alreadyRemoved)
      {
                  removeFromList(listElem, _blockInfo[blockNum], prevElem);
                  boundCheckKilledByOptList = true;
                  resolveCheckKilledByOptList = true;
                  divCheckKilledByOptList = true;
                  arrayStoreCheckKilledByOptList = true;
                  arrayCheckKilledByOptList = true;
      }
               checkCastKilledByOptList = true;
               indirectAccessesKilledByOptList = true;
               }
            else if (!alreadyRemoved)
               markNodeAsSurvivor(node->getNullCheckReference(), _indirectAccessesThatSurvive);
      }

         //else
         //   markNodeAsSurvivor(node->getFirstChild());
         }

      if ((prevElem && (prevElem->getNextElement() == listElem)) || (_blockInfo[blockNum]->getListHead() == listElem))
         prevElem = listElem;
      }

   if (checkForChange && !compareLists(analysisInfo->_inList, _blockInfo[blockNum]))
      changed = true;

   //
   // Copy the solution list as the in list now
   //
   copyListFromInto(_blockInfo[blockNum], analysisInfo->_inList);

   copyListFromInto(_regularGenSetInfo[blockNum], _blockInfo[blockNum]);
   appendLists(_blockInfo[blockNum], &_workingList);

   if (trace())
      {
      traceMsg(comp(), "\nGen List for Block : %p numbered %d is : \n", blockStructure, blockStructure->getNumber());
      if (_regularGenSetInfo[blockNum] != NULL)
         {
         ListIterator<TR::Node> genListIt(_regularGenSetInfo[blockNum]);
         TR::Node *listNode;
         for (listNode = genListIt.getFirst(); listNode != NULL;)
            {
            traceMsg(comp(), " ,%d ", listNode->getLocalIndex());
            listNode = genListIt.getNext();
            }
         traceMsg(comp(), "\n");
         }
      }

   return changed;
   }



bool TR_ExceptionCheckMotion::checkIfNodeCanSomehowSurvive(TR::Node *node, ContainerType *nodesThatSurvive)
   {
   if (checkIfNodeCanSurvive(node, nodesThatSurvive))
      {
      return true;
      }

   if (!_exprsUnaffectedByOrder->get(node->getLocalIndex()))
      {
      TR::ILOpCode &opCode = node->getOpCode();

      if (((opCode.isIndirect() || (opCode.isArrayLength())) &&
           (!node->getFirstChild()->isThisPointer() || !node->getFirstChild()->isNonNull()) &&
            (!(opCode.hasSymbolReference() &&
                node->getSymbolReference()->getSymbol()->isArrayletShadowSymbol())) &&
           !checkIfNodeCanSurvive(node->getFirstChild(), nodesThatSurvive))
          || (node->getOpCode().isTwoChildrenAddress()) ||
          (opCode.hasSymbolReference() && node->getSymbolReference()->isUnresolved()) ||
    ((opCode.isDiv() || opCode.isRem()) && !checkIfNodeCanSurvive(node->getSecondChild(), nodesThatSurvive)))
         return false;
      }

   int32_t i;
   bool allChildrenSurvive = true;
   for (i=0;i<node->getNumChildren();i++)
      {
      TR::Node *child = node->getChild(i);
      if (child->getOpCode().isTwoChildrenAddress())
         {
         TR::Node *firstChild = child->getFirstChild();
         if (!checkIfNodeCanSurvive(firstChild, nodesThatSurvive))
            {
            allChildrenSurvive = false;
            break;
            }

         TR::Node *secondChild = child->getSecondChild();
         if (!checkIfNodeCanSurvive(secondChild, nodesThatSurvive))
            {
            allChildrenSurvive = false;
            break;
            }
         }
      else if ((i==1) &&
               (node->getOpCode().isDiv() ||
                node->getOpCode().isRem()) &&
                isNodeValueZero(child))
         {
         allChildrenSurvive = false;
         break;
         }
      /*
      else if (!_exprsUnaffectedByOrder->get(node->getLocalIndex()))
         {
         if (!checkIfNodeCanSurvive(child, nodesThatSurvive))
            {
            allChildrenSurvive = false;
            break;
            }
         }
      */
      else if (!checkIfNodeCanSurvive(child, nodesThatSurvive))
         {
         allChildrenSurvive = false;
         break;
         }
      }

   if ((allChildrenSurvive) &&
       (node->getNumChildren() > 0))
      {
      nodesThatSurvive->set(node->getLocalIndex());
      }

   return allChildrenSurvive;
   }



bool TR_ExceptionCheckMotion::checkIfNodeCanSurvive(TR::Node *node, ContainerType *nodesThatSurvive)
   {
   if ((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0))
      {
      //
      //Survival cannot guaranteed when divisor is 0;
      //this node may in fact fail the div check; the only
      //time div will have const child
      //
      if ((node->getOpCode().isDiv() ||
           node->getOpCode().isRem()) &&
           isNodeValueZero(node->getSecondChild()))
         return false;
      else if (nodesThatSurvive->get(node->getLocalIndex()))
         {
         return true;
         }

      return false;
      }
   else if ((node->getOpCodeValue() == TR::aconst) &&
            (node->getAddress() == 0))
      {
      //
      //Survival cannot guaranteed when it is an aconst0;
      //this node may in fact fail the null check
      //
      return false;
      }

   return true;
   }



void TR_ExceptionCheckMotion::markNodeAsSurvivor(TR::Node *node, ContainerType *nodesThatSurvive)
   {
   if ((node->getLocalIndex() != MAX_SCOUNT) && (node->getLocalIndex() != 0))
      {
      nodesThatSurvive->set(node->getLocalIndex());
      }
   }


bool TR_ExceptionCheckMotion::isNodeValueZero(TR::Node *node)
   {
   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.isConversion())
      return isNodeValueZero(node->getFirstChild());
   else if (opCode.isLoadConst())
      {
      switch (node->getDataType())
         {
         case TR::Int8:
            if (node->getByte() == 0)
               return true;
            break;
         case TR::Int16:
            if (node->getShortInt() == 0)
               return true;
            break;
         case TR::Int32:
            if (node->getInt() == 0)
               return true;
            break;
         case TR::Int64:
            if (node->getLongInt() == 0L)
               return true;
            break;
         case TR::Float:
            if (node->getFloat() == 0.0)
               return true;
            break;
         case TR::Double:
            if (node->getDouble() == 0.0)
               return true;
            break;
         case TR::Address:
            if (node->getAddress() == 0)
               return true;
            break;
         default:
         	break;
         }
      }

   return false;
   }




void TR_ExceptionCheckMotion::removeFromList(ListElement<TR::Node> *listElem, List<TR::Node> *list, ListElement<TR::Node> *prevElem)
   {
   if (prevElem)
      prevElem->setNextElement(listElem->getNextElement());
   else
      list->setListHead(listElem->getNextElement());
   }



TR_DataFlowAnalysis::Kind TR_RedundantExpressionAdjustment::getKind()
   {
   return RedundantExpressionAdjustment;
   }

TR_RedundantExpressionAdjustment *TR_RedundantExpressionAdjustment::asRedundantExpressionAdjustment()
   {
   return this;
   }

bool TR_RedundantExpressionAdjustment::supportsGenAndKillSets()
   {
   return true;
   }

bool TR_RedundantExpressionAdjustment::supportsGenAndKillSetsForStructures()
   {
   return false;
   }

int32_t TR_RedundantExpressionAdjustment::getNumberOfBits()
   {
   return _partialRedundancy->getNumberOfBits();
   }

void TR_RedundantExpressionAdjustment::analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, ContainerType *)
   {
   }




// Analysis for adjusting the redundant info sets corresponding to the dynamic
// changes being made to the optimal info sets (because of expression ordering
// constraints). Done after each iteration of expression dominance to keep the
// optimal/redundant sets correct at each stage of the iterative process
//
TR_RedundantExpressionAdjustment::TR_RedundantExpressionAdjustment(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure, TR_ExceptionCheckMotion *exceptionCheckMotion)
   : TR_IntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, exceptionCheckMotion->trace())
   {
   if (trace())
      traceMsg(comp, "Starting Redundant expression adjustment\n");

   int32_t i;

   _exceptionCheckMotion = exceptionCheckMotion;
   _partialRedundancy = exceptionCheckMotion->getPartialRedundancy();
   _numberOfNodes = comp->getFlowGraph()->getNextNodeNumber();

   allocateContainer(&_optSetHelper);

   // Allocate the block info before setting the stack mark - it will be used by
   // the caller
   //
   initializeBlockInfo();

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   performAnalysis(rootStructure, false);

   if (trace())
      {
      for (i = 1; i < _numberOfNodes; ++i)
         {
         if (_blockAnalysisInfo[i])
            {
            traceMsg(comp, "\nAvailable optimal expressions for block_%d: ",i);
            _blockAnalysisInfo[i]->print(comp);
            }
         }
      traceMsg(comp, "\nEnding Redundant expression adjustment\n");
      }
   } // scope for stack memory region

   }


bool TR_RedundantExpressionAdjustment::postInitializationProcessing()
   {
   if (trace())
      {
      int32_t i;
      for (i = 1; i < _numberOfNodes; ++i)
         {
         traceMsg(comp(), "\nGen and kill sets for block_%d: ",i);
         if (_regularGenSetInfo[i])
            {
            traceMsg(comp(), " gen set ");
            _regularGenSetInfo[i]->print(comp());
            }
         if (_regularKillSetInfo[i])
            {
            traceMsg(comp(), " kill set ");
            _regularKillSetInfo[i]->print(comp());
            }
         if (_exceptionGenSetInfo[i])
            {
            traceMsg(comp(), " exception gen set ");
            _exceptionGenSetInfo[i]->print(comp());
            }
         if (_exceptionKillSetInfo[i])
            {
            traceMsg(comp(), " exception kill set ");
            _exceptionKillSetInfo[i]->print(comp());
            }
         }
      }
   return true;
   }

void TR_RedundantExpressionAdjustment::initializeGenAndKillSetInfo()
   {
   // For each block in the CFG build the gen and kill set for this analysis.
   //
   ContainerType **actualOptSetInfo = _exceptionCheckMotion->getActualOptSetInfo();
   ContainerType **origOptSetInfo = _partialRedundancy->getOrigOptSetInfo();

   int32_t i;
   for (i = 1; i < _numberOfNodes; ++i)
      {
      allocateContainer(_regularGenSetInfo+i);
      allocateContainer(_regularKillSetInfo+i);
      allocateContainer(_exceptionGenSetInfo+i);
      allocateContainer(_exceptionKillSetInfo+i);
      if (actualOptSetInfo[i])
         {
         //
         // The gen sets are all those expressions that are actually optimal in this
         // block, thus making them available in the successor blocks
         // where some of those expressions would be redundant
         //
         *_regularGenSetInfo[i] = *(origOptSetInfo[i]);
         *_regularGenSetInfo[i] -= *(_partialRedundancy->getSymOptimalNodes());
         *_regularGenSetInfo[i] &= *_partialRedundancy->_isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getDownwardExposedAnalysisInfo(i);
         *_regularGenSetInfo[i] |= *(actualOptSetInfo[i]);

         _optSetHelper->empty();
         *_exceptionGenSetInfo[i] = *_optSetHelper;
         //
         // The kill sets are those expressions that were optimistically
         // optimal at this block but NOT actually optimal, meaning they should
         // NOT be available to their successor blocks (otherwise we would
         // wrongly make these expressions redundant in the successor blocks).
         //
         _optSetHelper->setAll(_partialRedundancy->getNumberOfBits());
         *_exceptionKillSetInfo[i] = *_optSetHelper;
         *_optSetHelper -= *_partialRedundancy->_isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localTransparency.getAnalysisInfo(i);

         // We ran into a problem where we were caching the array length too early and later
         //              reallocating a larger array thus resulting an a stale assumption which produces an AIOOB.
         // *_optSetHelper -= *_partialRedundancy->_isolatedness->_latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getDownwardExposedAnalysisInfo(i);

         *_regularKillSetInfo[i] = *_optSetHelper;
         *_optSetHelper = *(origOptSetInfo[i]);
         *_optSetHelper -= *(actualOptSetInfo[i]);
         *_regularKillSetInfo[i] |= *_optSetHelper;

         /////dumpOptDetails(comp(), "Kill set info for block_%d\n", i);
         /////_regularKillSetInfo[i]->print(comp());
         }
      }
   }








bool TR_RedundantExpressionAdjustment::analyzeBlockStructure(TR_BlockStructure *blockStructure, bool checkForChange)
   {
   initializeInfo(_regularInfo);
   initializeInfo(_exceptionInfo);
   //
   // Use information from last time we analyzed this structure; if
   // analyzing for the first time, initialize information
   //
   ExtraAnalysisInfo *analysisInfo = getAnalysisInfo(blockStructure);
   if (!blockStructure->hasBeenAnalyzedBefore())
      blockStructure->setAnalyzedStatus(true);
   else
      {
      if (*_currentInSetInfo == *analysisInfo->_inSetInfo)
         {
         if (trace())
            {
            traceMsg(comp(), "\nSkipping re-analysis of Block : %p numbered %d\n", blockStructure, blockStructure->getNumber());
            }
         return false;
         }
      }

   // Copy the current in set for comparison the next time we analyze this region
   //
   copyFromInto(_currentInSetInfo, analysisInfo->_inSetInfo);
   int32_t blockNum = blockStructure->getNumber();

   if (_exceptionCheckMotion->getTrySecondBestSolution())
      {
      ContainerType **actualOptSetInfo = _exceptionCheckMotion->getActualOptSetInfo();
      int32_t **orderedOptNumbersList = _exceptionCheckMotion->getOrderedOptNumbersList();
      List<TR::Node> **genSetInfo =  _exceptionCheckMotion->getGenSetList();
      ContainerType **optimisticRednSetInfo = _partialRedundancy->getRednSetInfo();
      ContainerType **optimisticOptSetInfo = _partialRedundancy->getOptSetInfo();
      ContainerType *exprsUnaffectedByOrder = _exceptionCheckMotion->getExprsUnaffectedByOrder();
      int32_t totalMaxSize = optimisticOptSetInfo[blockNum]->elementCount() + optimisticRednSetInfo[blockNum]->elementCount();

      int32_t j = 0;
      int32_t oldMaxSize = 0;
      int32_t unaffectedSize = 0;
      while ((j < totalMaxSize) && (orderedOptNumbersList[blockNum][j] > -1))
         {
         if (exprsUnaffectedByOrder->get(orderedOptNumbersList[blockNum][j]))
            unaffectedSize++;

         j++;
         oldMaxSize++;
         }

      int32_t *orderedOldOptNumbersList = (int32_t *)trMemory()->allocateStackMemory(oldMaxSize*sizeof(int32_t));
      j=0;
      _optSetHelper->empty();
      while (j < oldMaxSize)
         {
         orderedOldOptNumbersList[j] = orderedOptNumbersList[blockNum][j];
         _optSetHelper->set(orderedOldOptNumbersList[j]);
         j++;
         }

      ListElement<TR::Node> *prevElem = NULL;
      ListElement<TR::Node> *listElem;
      for (listElem = genSetInfo[blockNum]->getListHead(); listElem != NULL; listElem = listElem->getNextElement())
         {
         /*
         int32_t weight = 1;
         blockStructure->calculateFrequencyOfExecution(&weight);
         if (weight > 1)
            break;
         */
         //
         // For expressions that are optimistically redundant in this block
         // but NOT actually so (because the corresponding optimal placment did not
         // occur because of exception ordering constraints), place the expression
         // optimally at this point so that it dominates any further
         // blocks where this expression is redundant
         //
         if (trace())
               traceMsg(comp(), "CONSIDERING Expr %d (representative) Node %p (listElem %p) in Block : %d\n", listElem->getData()->getLocalIndex(), listElem->getData(), listElem, blockNum);
         if (/*_optSetHelper->get(listElem->getData()->getLocalIndex()) || */
             ((optimisticRednSetInfo[blockNum]->get(listElem->getData()->getLocalIndex())) &&
             (!_currentInSetInfo->get(listElem->getData()->getLocalIndex())) &&
             (!actualOptSetInfo[blockNum]->get(listElem->getData()->getLocalIndex()))))
            {
            if (trace())
               traceMsg(comp(), "IN ORDER Expr %d (representative) Node %p (listElem %p) in Block : %d\n", listElem->getData()->getLocalIndex(), listElem->getData(), listElem, blockNum);

            actualOptSetInfo[blockNum]->set(listElem->getData()->getLocalIndex());
            _regularGenSetInfo[blockNum]->set(listElem->getData()->getLocalIndex());
            _exceptionGenSetInfo[blockNum]->set(listElem->getData()->getLocalIndex());
            _optSetHelper->reset(listElem->getData()->getLocalIndex());

            if (trace())
               traceMsg(comp(), "Affected by order <%d> will be at index %d\n", listElem->getData()->getLocalIndex(), j);
            orderedOptNumbersList[blockNum][j++] = listElem->getData()->getLocalIndex();
            }

         if ((prevElem && (prevElem->getNextElement() == listElem)) || (genSetInfo[blockNum]->getListHead() == listElem))
             prevElem = listElem;
         }

      for (;j<totalMaxSize;)
         orderedOptNumbersList[blockNum][j++] = -1;
      }

   if (blockStructure->getNumber() == 0)
      analyzeBlockZeroStructure(blockStructure);
   else
      {
      int32_t blockNum = blockStructure->getNumber();

      if (_regularGenSetInfo)
         {
         copyFromInto(_currentInSetInfo, _regularInfo);
         copyFromInto(_currentInSetInfo, _exceptionInfo);

         // Done first so that kill set can be applied
         // on this soln; if an expr is both gened and killed
         // in the block, then it must be killed on the way
         // out of the block as it means it is not available
         //
         if (_regularGenSetInfo[blockNum])
            *_regularInfo |= *_regularGenSetInfo[blockNum];
         if (_regularKillSetInfo[blockNum])
            *_regularInfo -= *_regularKillSetInfo[blockNum];

         // Done first so that kill set can be applied
         // on this soln; if an expr is both gened and killed
         // in the block, then it must be killed on the way
         // out of the block as it means it is not available
         //
         //if (_exceptionGenSetInfo[blockNum])
         //   *_exceptionInfo |= *_exceptionGenSetInfo[blockNum];
         //if (_exceptionKillSetInfo[blockNum])
         //   *_exceptionInfo -= *_exceptionKillSetInfo[blockNum];

         copyFromInto(analysisInfo->_inSetInfo, _blockAnalysisInfo[blockStructure->getNumber()]);
         }
      else
         analyzeTreeTopsInBlockStructure(blockStructure);
      }

   _exceptionInfo->empty();

   TR::Block *block = blockStructure->getBlock();
   TR::Block *nullConstraintBlock = NULL;
   int32_t nullCheckNumber = -1;
   if ((block->getSuccessors().size() == 2) && block->getExit())
      {
      TR::Node *branch = block->getLastRealTreeTop()->getNode();
      if (((branch->getOpCodeValue() == TR::ifacmpne) ||
           (branch->getOpCodeValue() == TR::ifacmpeq)) &&
          (branch->getSecondChild()->getOpCodeValue() == TR::aconst) &&
          (branch->getSecondChild()->getAddress() == 0))
         {
         TR::Node *nullCheckReference = branch->getFirstChild();
         scount_t nullCheckReferenceIndex = nullCheckReference->getLocalIndex();
         if (((nullCheckReferenceIndex != MAX_SCOUNT) && (nullCheckReferenceIndex != 0)))
            {
            if (_regularInfo->get(nullCheckReferenceIndex))
               {
               TR::Node **supportedNodesAsArray = _partialRedundancy->_isolatedness->_supportedNodesAsArray;
               int32_t numberOfBits = _partialRedundancy->getNumberOfBits();
               int32_t j=1;
               while (j < numberOfBits)
                  {
                  TR::Node *nextOptimalNode = supportedNodesAsArray[j];
                  if (nextOptimalNode->getOpCodeValue() == TR::NULLCHK)
                     {
                     if (nextOptimalNode->getNullCheckReference()->getLocalIndex() == nullCheckReferenceIndex)
                        {
                        nullCheckNumber = j;
                        if (branch->getOpCodeValue() == TR::ifacmpne)
                           nullConstraintBlock = branch->getBranchDestination()->getNode()->getBlock();
                        else
                           nullConstraintBlock = block->getNextBlock();
                        }
                     }

                  j++;
                  }
               }
            }
         }
      }

   ContainerType *outSetInfo;
   bool changed = false;
   for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
      {
      outSetInfo = analysisInfo->getContainer(analysisInfo->_outSetInfo, (*succ)->getTo()->getNumber());
      bool addedNullConstraint = false;

      if (nullConstraintBlock &&
          (nullConstraintBlock == (*succ)->getTo()) &&
          !_regularInfo->get(nullCheckNumber))
         {
         addedNullConstraint = true;
         _regularInfo->set(nullCheckNumber);
         //printf("Propagating null constraint in %s\n", signature(comp()->getCurrentMethod()));
         }

      if (checkForChange && !changed && !(*_regularInfo == *outSetInfo))
         changed = true;
      *outSetInfo = *_regularInfo;

      if (addedNullConstraint)
         _regularInfo->reset(nullCheckNumber);
      }

   for (auto succ = block->getExceptionSuccessors().begin(); succ != block->getExceptionSuccessors().end(); ++succ)
      {
      outSetInfo = analysisInfo->getContainer(analysisInfo->_outSetInfo, (*succ)->getTo()->getNumber());
      if (checkForChange && !changed && !(*_exceptionInfo == *outSetInfo))
         changed = true;
      *outSetInfo = *_exceptionInfo;
      }

  if (trace())
      {
      traceMsg(comp(), "\nIn Set Info for Block : %p numbered %d is : \n", blockStructure, blockStructure->getNumber());
      analysisInfo->_inSetInfo->print(comp());
      traceMsg(comp(), "\nOut Set Info for Block : %p numbered %d is : \n", blockStructure, blockStructure->getNumber());
      TR_BasicDFSetAnalysis<ContainerType*>::TR_ContainerNodeNumberPair *pair;
      for (pair = analysisInfo->_outSetInfo->getFirst(); pair; pair = pair->getNext())
         {
         traceMsg(comp(), "Exit or Succ numbered %d : ", pair->_nodeNumber);
         pair->_container->print(comp());
         traceMsg(comp(), "\n");
         }
      traceMsg(comp(), "\n");
      }

   return changed;
   }
