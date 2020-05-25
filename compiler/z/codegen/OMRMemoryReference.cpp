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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRZMemoryReference#C")
#pragma csect(STATIC,"OMRZMemoryReference#S")
#pragma csect(TEST,"OMRZMemoryReference#T")


#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/defines.h"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "z/codegen/EndianConversion.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"

#if J9_PROJECT_SPECIFIC
#include "z/codegen/S390Register.hpp"
#endif

/**
 * Check to see if mem ref is declared during instruction selection phase
 * and if so, if the current treetop node is null
 * if it is, the mem ref can cause an implicit null pointer exception
 */
void OMR::Z::MemoryReference::setupCausesImplicitNullPointerException(TR::CodeGenerator *cg)
   {
      if (cg->getSupportsImplicitNullChecks() &&
         cg->getCurrentEvaluationTreeTop()->getNode()->getOpCode().isNullCheck())
         {
         self()->setCausesImplicitNullPointerException();
         }
   }

/**
 * Aligned memRefs are sometimes created with negative offsets that will eventually be removed after applying bumps for left or right alignment
 * So a premature LAY or LA/AHI fixup is not done for these negative offsets a flag HasTemporaryNegativeOffset is set on the memRef to prevent this fixup
 * However, if there is already a real negative offset then the fixup is required for this part of the negative offset
 * To deal with this first split off the real negative offset by calling enforceSSFormatLimits and then set the HasTemporaryNegativeOffset flag and add in the new negative offset
 */
void OMR::Z::MemoryReference::addToTemporaryNegativeOffset(TR::Node *node, int32_t offset, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (offset < 0)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\taddToTemporaryNegativeOffset : %s (%p) new offset %d, existing mr offset %d (existing mr hasTempNegOffset = %s)\n",
            node?node->getOpCode().getName():"NULL",node,offset,self()->getOffset(),self()->hasTemporaryNegativeOffset()?"yes":"no");
      if (self()->getOffset() < 0 && !self()->hasTemporaryNegativeOffset()) // if there is a 'real' negative offset then deal with this first before adding in the temporary negative offset
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\texisting mr->offset %d < 0 so call enforceSSFormatLimits to clear this up before setting HasTemporaryNegativeOffset\n",self()->getOffset());
         self()->enforceSSFormatLimits(node, cg, NULL);  // call SSFormatLimits to also take this chance to fold in an index register if needed
         }
      self()->addToOffset(offset);
      self()->setHasTemporaryNegativeOffset();
      }
   }

/**
 * Check to see if mem ref is declared during inst selection phase
 * and then enable the check for the binary phase
 */
void OMR::Z::MemoryReference::setupCheckForLongDispFlag(TR::CodeGenerator *cg)
   {
   // Enable a check for the long disp slot in generateBin phase
   if (cg->getCodeGeneratorPhase() == TR::CodeGenPhase::InstructionSelectionPhase)
      {
      self()->setCheckForLongDispSlot();
      }
   }

/**
 * forceFolding is not a good idea to always set as it has the side effect of incrementing the
 * refCounts for all children in the address tree. So if folding is not done then poorer quality
 * code (due to more clobberEvals from the higher refCounts) will be generated.
 * In some simple cases where folding is very likely then forceFolding is set (the higher refCounts do not
 * matter as the evaluators are not actually going to be called)
 * Note that setting forceFolding does not require that folding will be done it only allows it to be possible
 */
bool OMR::Z::MemoryReference::setForceFoldingIfAdvantageous(TR::CodeGenerator * cg, TR::Node * addressChild)
   {

   TR::Compilation *comp = cg->comp();

   // Force folding if it will make a standard memory reference: X(B) or X(I,B)

   if (addressChild->getRegister() != NULL)
      {
      // cannot fold:
      //
      // aiadd (reg)
      //    node/tree
      //    iconst
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp, " inside setForceFoldingIfAdvantageous, addressChild %s (%p) has register %s\n",
                  addressChild->getOpCode().getName(), addressChild,
                  cg->getDebug()->getName(addressChild->getRegister()));
         }
      return false;
      }

   if (!addressChild->getOpCode().isAdd())
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp, " inside setForceFoldingIfAdvantageous, addressChild %s (%p) is not an add type - most likely cannot fold\n",
                  addressChild->getOpCode().getName(), addressChild);
         }
      return false;
      }

   if (!addressChild->getSecondChild()->getOpCode().isLoadConst())
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp, " inside setForceFoldingIfAdvantageous, addressChild %s (%p) second child is not a load const - cannot fold\n",
                  addressChild->getOpCode().getName(), addressChild);
         }
      return false;
      }

   if (addressChild->getSecondChild()->getRegister() != NULL)
      {
      // cannot fold:
      //
      // aiadd
      //    node/tree
      //    iconst (reg)
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp, " inside setForceFoldingIfAdvantageous, addressChild %s (%p) const child %s (%p) has register %s, cannot fold.\n",
                  addressChild->getOpCode().getName(), addressChild,
                  addressChild->getSecondChild()->getOpCode().getName(), addressChild->getSecondChild(),
                  cg->getDebug()->getName(addressChild->getSecondChild()->getRegister()));
         }
      return false;
      }

   if (addressChild->getReferenceCount() != 1)
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp, " inside setForceFoldingIfAdvantageous, addressChild %s (%p) has ref count %d, cannot fold, need value in register.\n",
                  addressChild->getOpCode().getName(), addressChild, addressChild->getReferenceCount());
         }
      return false;
      }

   // Past here, addressChild :
   // - has no register
   // - is an add
   // - second child is a load const and was not evaluated into a register
   // - has ref count 1

   TR::Node * eventualNonConversion = addressChild->getFirstChild();
   TR::Node * foundConvNodeWithRegister = 0;
   bool allConvNodesAreUnneeded = true;
   bool allConvNodesHaveRefCount1 = true;
   bool haveToEvalConvIntoRegister = false;

   // "See through" conversions

   while (eventualNonConversion->getOpCode().isConversion())
      {
      if ((foundConvNodeWithRegister == 0) && (eventualNonConversion->getRegister() != NULL))
         {
         foundConvNodeWithRegister = eventualNonConversion;
         }

      allConvNodesAreUnneeded = allConvNodesAreUnneeded && eventualNonConversion->isUnneededConversion();
      allConvNodesHaveRefCount1 = allConvNodesHaveRefCount1 && (eventualNonConversion->getReferenceCount() == 1);

      haveToEvalConvIntoRegister = haveToEvalConvIntoRegister ||
         ((eventualNonConversion->getReferenceCount() != 1) && (eventualNonConversion->getRegister() == NULL));

      eventualNonConversion = eventualNonConversion->getFirstChild();
      }

   if (haveToEvalConvIntoRegister)
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,
                  " inside setForceFoldingIfAdvantageous, addressChild %s (%p) first child has conv. with ref count > 1 and no register, must evaluate - setForceFolding()\n",
                  addressChild->getOpCode().getName(), addressChild);
         }
      cg->evaluate(addressChild->getFirstChild());
      self()->setForceFolding();
      return true;
      }

   // if conversions are real, don't want to bump up ref counts either for perf reasons
   // only allow folding if conversions are deemed 'unneeded' by LoadExtension step

   if (!allConvNodesAreUnneeded)
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,
                  " inside setForceFoldingIfAdvantageous, addressChild %s (%p) first child has conv. which are needed, folding after evaluation.\n",
                  addressChild->getOpCode().getName(), addressChild);
         }
      cg->evaluate(addressChild->getFirstChild());
      self()->setForceFolding();
      return true;
      }

   // Past here:
   // - each conversion is ref count 1 (with or without register), or ref count > 2 and has a register

   bool foundRegister = eventualNonConversion->getRegister() ||
      (foundConvNodeWithRegister && foundConvNodeWithRegister->getRegister());

   if (foundRegister)
      {
      // aiadd
      //    node/tree (reg)
      //    iconst
      //
      // or
      //
      // aiadd
      //    conv
      //       conv (reg)
      //          loadaddr / aload / aRegLoad (reg)
      //    iconst
      //
      // or
      //
      // aiadd
      //    conv
      //       conv
      //          loadaddr / aload / aRegLoad (reg)
      //    iconst
      if (cg->traceBCDCodeGen())
         {
         TR::Node * dispNode = eventualNonConversion->getRegister() ? eventualNonConversion : foundConvNodeWithRegister;

         traceMsg(comp,"\t\taddressChild %s (%p) node %s (%p) is evaluatedReg+const (%s + %lld) so %s=true\n",
                  addressChild->getOpCode().getName(),addressChild,
                  dispNode->getOpCode().getName(), dispNode,
                  cg->getDebug()->getName(dispNode->getRegister()),
                  addressChild->getSecondChild()->get64bitIntegralValue(),
                  "setForceFirstTimeFolding");
         }
      self()->setForceFirstTimeFolding(); // e.g. fold the top level add but not anything below the add
      return true;
      }

   bool eventualNonConversionIsSuitableForFolding =
      (eventualNonConversion->getOpCodeValue() == TR::loadaddr ||
       eventualNonConversion->getOpCode().isLoad() ||
       eventualNonConversion->getRegister() ||
       eventualNonConversion->getOpCode().isLoadReg());

   if (!eventualNonConversionIsSuitableForFolding)
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,
                  " inside setForceFoldingIfAdvantageous, addressChild %s (%p) eventual non conversion %s (%p) is not suitable for folding.\n",
                  addressChild->getOpCode().getName(), addressChild,
                  eventualNonConversion->getOpCode().getName(), eventualNonConversion);
         }
      // TODO: first time folding here? still desirable axadd form
      return false;
      }

   if (!eventualNonConversion->getOpCode().isIndirect())
      {
      // aiadd
      //    loadaddr/aload/aRegLoad
      //    iconst
      //
      // or
      //
      // aiadd
      //    conv
      //       conv
      //          loadaddr/aload/aRegLoad
      //    iconst
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp," inside setForceFoldingIfAdvantageous, eventualNonConversion %s (%p) has no register and refCount==1 and is a aload+const so setForceFolding=true\n",
                  eventualNonConversion->getOpCode().getName(),eventualNonConversion);
         }
      self()->setForceFolding();
      return true;
      }
   else // if (eventualNonConversion->getReferenceCount() == 1)
      {
      // aiadd
      //    iaload
      //       x
      //    iconst
      //
      // or
      //
      // aiadd
      //    conv
      //       iaload
      //          x
      //    iconst
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,
                  " inside setForceFoldingIfAdvantageous, eventualNonConversion %s (%p) has no register and refCount==1 and is an iaload+const so eval(%s - %p) and setForceFolding=true\n",
                  eventualNonConversion->getOpCode().getName(),eventualNonConversion,
                  eventualNonConversion->getFirstChild()->getOpCode().getName(),eventualNonConversion->getFirstChild());
         }
      self()->setForceFolding();
      cg->evaluate(addressChild->getFirstChild());
      return true;
      }

   return false;
   }

/**
 * Recall that with storage hints, trees are usually traversed / decended twice, both usually with an implied reference.
 *
 * Evaluation will descend the tree until a register is encountered (much like recursivelyIncrementReferenceCount), so evaluation (and therefore
 * the reference count decrementing that evaluators do) will miss nodes that are part of the incremented nodes list and that are under a node that
 * was assigned a register after memory reference population. This function will descend the tree and look for this case, only removing nodes if a
 * node with a register was seen. It is important to only decend down nodes that are part of the incremented nodes list, because that is what the
 * recursivelyIncrementReferenceCount function did before.
 */
void recursivelyDecrementIncrementedNodesIfUnderRegister(TR::CodeGenerator * cg, TR::Node * cur, List<TR::Node> & _incNodesList, List<TR::Node> &nodesToNotRecurse,
   vcount_t vc, bool startRemoving)
   {
   if (cur->getVisitCount() == vc)
      {
      return;
      }

   cur->setVisitCount(vc);

   bool foundCurNode = false;
   TR::Compilation *comp = cg->comp();

   // search for the current node in the list, and remove + decrement if found
   ListIterator<TR::Node> listIt(&_incNodesList);
   TR::Node* listNode;
   for (listNode = listIt.getFirst(); listNode; listNode = listIt.getNext())
      {
      if (listNode == cur)
         {
         foundCurNode = true;

         if (startRemoving)
            {
            if (cg->traceBCDCodeGen())
               {
               traceMsg(comp,"\t decReferenceCount on incremented node %p (%d) (%d->%d)\n",
                        cur, cur->getGlobalIndex(), cur->getReferenceCount(), cur->getReferenceCount()-1);
               }
            cg->decReferenceCount(cur);
            }
         break;
         }
      }

   if (!foundCurNode)
      {
      // Return if the node we are at is not part of the incremented nodes list.
      return;
      }

   // if the node already had a register before recursive increment, then do not recursively decrement its children
   listIt.set(&nodesToNotRecurse);
   for (listNode = listIt.getFirst(); listNode; listNode = listIt.getNext())
      {
      if (listNode == cur)
         return;
      }


   // Do not remove the current node if it has a register, so perform check afterwards.

   if (cur->getRegister() != NULL)
      {
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\t %s [%p] (%d) has a register, so start removing under it\n",
                  cur->getOpCode().getName(), cur, cur->getGlobalIndex());
         }
      startRemoving = true;
      }

   for (size_t i = 0; i < cur->getNumChildren(); i++)
      {
      recursivelyDecrementIncrementedNodesIfUnderRegister(cg, cur->getChild(i), _incNodesList, nodesToNotRecurse, vc, startRemoving);
      }
   }

OMR::Z::MemoryReference::MemoryReference(TR::Node * rootLoadOrStore, TR::CodeGenerator * cg, bool canUseRX, TR_StorageReference *storageReference)
   : _baseRegister(NULL), _baseNode(NULL), _indexRegister(NULL), _indexNode(NULL), _targetSnippet(NULL),
     _symbolReference(rootLoadOrStore->getOpCode().hasSymbolReference()?rootLoadOrStore->getSymbolReference():NULL),
     _originalSymbolReference(rootLoadOrStore->getOpCode().hasSymbolReference()?rootLoadOrStore->getSymbolReference():NULL),
     _listingSymbolReference(NULL),
   _offset(0), _flags(0), _storageReference(storageReference), _fixedSizeForAlignment(0), _leftMostByte(0), _name(NULL),
   _incrementedNodesList(cg->comp()->trMemory())
   {

   TR::SymbolReference * symRef = rootLoadOrStore->getOpCode().hasSymbolReference() ? rootLoadOrStore->getSymbolReference() : NULL;
   TR::Symbol * symbol = symRef ? symRef->getSymbol() : NULL;
   bool isStore = rootLoadOrStore->getOpCode().isStore();
   TR::Register * tempReg = rootLoadOrStore->getRegister();
   bool alloc = false;
   TR::Compilation *comp = cg->comp();
   List<TR::Node> nodesAlreadyEvaluatedBeforeFoldingList(comp->trMemory());

   _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;

   if (tempReg == NULL || tempReg->getKind() == TR_FPR)
      {
      tempReg = cg->allocateRegister();
      alloc = true;
      }

   // Enable a check for the long disp slot in generateBin phase
   self()->setupCheckForLongDispFlag(cg);

   self()->setupCausesImplicitNullPointerException(cg);

   _targetSnippetInstruction = NULL;

   TR_ASSERT(_incrementedNodesList.getHeadData() == 0,
              "_incrementedNodesList size is not zero at beginning of OMR::Z::MemoryReference constructor!");

   self()->tryForceFolding(rootLoadOrStore, cg, storageReference, symRef, symbol, nodesAlreadyEvaluatedBeforeFoldingList);

   if (rootLoadOrStore->getOpCode().isIndirect() || self()->isExposedConstantAddressing())
      {
      TR::Node * subTree = rootLoadOrStore->getFirstChild();

      if (subTree->getOpCodeValue() == TR::aconst)
         {
         if (!self()->ZeroBasePtr_EvaluateSubtree(subTree, cg, this))
            self()->setBaseRegister(cg->evaluate(subTree), cg);
         _baseNode = subTree;
         cg->decReferenceCount(subTree);
         }
      else
         {
         TR::Node *addressChild = rootLoadOrStore->getFirstChild();
         bool tryBID = (addressChild->getOpCodeValue() == TR::aiadd ||
                        addressChild->getOpCodeValue() == TR::aladd) &&
                       _baseRegister == NULL && _indexRegister == NULL &&
                       self()->tryBaseIndexDispl(cg, rootLoadOrStore, addressChild);
         if (!tryBID)
            {
            self()->populateMemoryReference(subTree, cg);
            recursivelyDecrementIncrementedNodesIfUnderRegister(cg, subTree, _incrementedNodesList, nodesAlreadyEvaluatedBeforeFoldingList, comp->incOrResetVisitCount(), false);
            cg->decReferenceCount(subTree);
            }
         }


      if ((symbol && symbol->isStatic()) &&
         rootLoadOrStore->getFirstChild()->getOpCode().hasSymbolReference() &&
         rootLoadOrStore->getFirstChild()->getSymbolReference()->getSymbol()->isStatic()&&
         rootLoadOrStore->getFirstChild()->getSymbolReference()->isUnresolved()&&
         symRef->isUnresolved() && symRef && !symRef->isFromLiteralPool() &&
         rootLoadOrStore->getFirstChild()->getSymbolReference()->isFromLiteralPool() )
         {
         if (alloc)
            {
            cg->stopUsingRegister(tempReg);
            }
         return;
         }
      if ((symbol && symbol->isStatic()) && symRef && symRef->isFromLiteralPool() && !self()->isExposedConstantAddressing())
         {
         TR_ASSERT(cg->supportsOnDemandLiteralPool() == true, "May not be here with Literal Pool On Demand disabled\n");

         if (symRef->isUnresolved())
            {
            self()->createUnresolvedDataSnippetForiaload(rootLoadOrStore, cg, symRef, tempReg, isStore);
            }
         else
            {
            uintptr_t staticAddressValue = (uintptr_t) symbol->getStaticSymbol()->getStaticAddress();
            TR::S390ConstantDataSnippet * targetsnippet;
            if (cg->comp()->target().is64Bit())
               {
               targetsnippet = cg->findOrCreate8ByteConstant(0, staticAddressValue);
               }
            else
               {
               targetsnippet = cg->findOrCreate4ByteConstant(0, staticAddressValue);
               }

            self()->initSnippetPointers(targetsnippet, cg);
            }
         if (alloc)
            {
            cg->stopUsingRegister(tempReg);
            }
         return;
         }
      if (symRef && symRef->isUnresolved())
         {
         self()->createUnresolvedDataSnippet(rootLoadOrStore, cg, symRef, tempReg, isStore);
         }
      }
   else
      {
      // symbol is static
      if (symbol && symbol->isStatic())
         {
         // symbol is unresolved
         if (symRef && symRef->isUnresolved())
            {
            TR::UnresolvedDataSnippet * uds = self()->createUnresolvedDataSnippet(rootLoadOrStore, cg, symRef, tempReg, isStore);
            self()->createPatchableDataInLitpool(rootLoadOrStore, cg, tempReg, uds);
            }
         else
            {
            // symbol is resolved
            if (isStore)
               {
               // Storing to the symbol reference
               TR::Register * tempReg;
               if (cg->comp()->target().is64Bit())
                  tempReg = cg->allocateRegister();
               else
                  tempReg = cg->allocateRegister();
               self()->setBaseRegister(tempReg, cg);
               cg->stopUsingRegister(_baseRegister);
               _baseNode = NULL;
               }
            else
               {
               if (symbol && symbol->isMethodMetaData())
                  self()->setBaseRegister(cg->getMethodMetaDataRealRegister(), cg);
               else
                  self()->setBaseRegister(tempReg, cg);
               }
            genLoadAddressConstant(cg, rootLoadOrStore, (uintptr_t) symRef->getSymbol()->getStaticSymbol()->getStaticAddress(),
               _baseRegister);
            cg->stopUsingRegister(tempReg);
            }
         }
      else if (!symRef)
         {
         if (!rootLoadOrStore->getOpCode().isLoad() && !rootLoadOrStore->getOpCode().isStore())
            {
            // this is a valid path through this constructor, despite what the variable name might mean
            //
            // for example, calling generateMemoryReference([node], cg) on an aiadd node will arrive here.

            self()->populateMemoryReference(rootLoadOrStore, cg);
            if (alloc)
               {
               cg->stopUsingRegister(tempReg);
               }
            return;
            }
         else
            {
            TR_ASSERT(rootLoadOrStore->getDataType() == TR::Address || rootLoadOrStore->getOpCode().isLoadConst(),
               "rootLoadOrStore %s %p should be an address type or const op and not dt %s\n", rootLoadOrStore->getOpCode().getName(), rootLoadOrStore,rootLoadOrStore->getDataType().toString());
            //TR_ASSERT(rootLoadOrStore->getDataType() == TR::Address,
            //   "rootLoadOrStore %s %p should be an address type and not dt %d\n",rootLoadOrStore->getOpCode().getName(),rootLoadOrStore,rootLoadOrStore->getDataType());

            self()->populateThroughEvaluation(rootLoadOrStore, cg);
            if (alloc)
               {
               cg->stopUsingRegister(tempReg);
               }
            return;
            }
         }
      else
         {
         // must be auto, parm or metadata
         if (symbol && !symbol->isMethodMetaData())
            {
            // auto or parm is on stack
            self()->setBaseRegister(cg->getStackPointerRealRegister(symbol), cg);
            }
         else
            {
            // meta data
            self()->setBaseRegister(cg->getMethodMetaDataRealRegister(), cg);
            }

         _baseNode = NULL;
         }
      _indexRegister = NULL;
      }

   if (symRef && symRef->isLiteralPoolAddress())
      {
      TR_ASSERT(cg->supportsOnDemandLiteralPool() == true, "May not be here with Literal Pool On Demand disabled\n");
      TR::S390ConstantDataSnippet * targetsnippet = self()->createLiteralPoolSnippet(rootLoadOrStore, cg);

      self()->initSnippetPointers(targetsnippet, cg);
      }
   else
      {
      if (self()->getUnresolvedSnippet() == NULL)
         {
         if (symRef)
            _offset += symRef->getOffset();
         }

      self()->enforceDisplacementLimit(rootLoadOrStore, cg, NULL);
      }
   if (alloc)
      {
      cg->stopUsingRegister(tempReg);
      }
   // TODO: aliasing sets?
   }

OMR::Z::MemoryReference::MemoryReference(TR::Node *addressChild, bool canUseIndexReg, TR::CodeGenerator *cg)
   : _baseRegister(NULL), _baseNode(NULL), _indexRegister(NULL), _indexNode(NULL), _targetSnippet(NULL), _targetSnippetInstruction(NULL),
     _offset(0), _flags(0), _storageReference(NULL), _fixedSizeForAlignment(0), _leftMostByte(0), _name(NULL), _incrementedNodesList(cg->comp()->trMemory())
   {
   bool done = false;
   if (addressChild->getRegister() == NULL)
      {
      if (addressChild->getReferenceCount() == 1 &&
          addressChild->getOpCode().isAdd() &&
          addressChild->getSecondChild()->getOpCode().isLoadConst())
         {
         TR::Node *first = addressChild->getFirstChild();
         if (canUseIndexReg &&
             first->getOpCode().isAdd())
            {
            _baseNode = first->getFirstChild();
            _baseRegister = cg->evaluate(_baseNode);
            cg->decReferenceCount(_baseNode);

            _indexNode = first->getSecondChild();
            _indexRegister = cg->evaluate(_indexNode);
            cg->decReferenceCount(_indexNode);
            }
         else
            {
            _baseNode = first;
            _baseRegister = cg->evaluate(first);
            }
         _offset = addressChild->getSecondChild()->getIntegerNodeValue<int32_t>();
         if (addressChild->getReferenceCount() == 1)
            {
            cg->decReferenceCount(addressChild->getFirstChild());
            cg->decReferenceCount(addressChild->getSecondChild());
            }
         done = true;
         }
      }
   if (!done)
      {
      _baseRegister = cg->evaluate(addressChild);
      }
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _originalSymbolReference = _symbolReference;
   _listingSymbolReference = _symbolReference;
   self()->setupCausesImplicitNullPointerException(cg);
   }

TR::S390ConstantDataSnippet *
OMR::Z::MemoryReference::createLiteralPoolSnippet(TR::Node * node, TR::CodeGenerator * cg)
   {
   return cg->createLiteralPoolSnippet(node);
   }

OMR::Z::MemoryReference::MemoryReference(TR::Node * node, TR::SymbolReference * symRef, TR::CodeGenerator * cg, TR_StorageReference *storageReference)
   : _baseRegister(NULL), _baseNode(NULL), _indexRegister(NULL), _indexNode(NULL), _targetSnippet(NULL), _targetSnippetInstruction(NULL),
     _flags(0), _storageReference(storageReference), _fixedSizeForAlignment(0), _leftMostByte(0), _name(NULL),
     _incrementedNodesList(cg->comp()->trMemory())
   {

   TR::Symbol * symbol = symRef->getSymbol();
   TR::Register * writableLiteralPoolRegister = NULL;
   TR::Compilation *comp = cg->comp();

   // Enable a check for the long disp slot in generateBin phase
   self()->setupCheckForLongDispFlag(cg);

   self()->setupCausesImplicitNullPointerException(cg);

   if (symbol->isRegisterMappedSymbol())
      {
      // must be either auto, parm, meta or error.
      if (!symbol->isMethodMetaData())
         {
         self()->setBaseRegister(cg->getStackPointerRealRegister(symbol), cg);
         }
      else
         {
         self()->setBaseRegister(cg->getMethodMetaDataRealRegister(), cg);
         }
      _baseNode = NULL;
      }

   if (symRef->isUnresolved())
      {
      self()->createUnresolvedSnippetWithNodeRegister(node, cg, symRef, writableLiteralPoolRegister);
      }

   if (_baseNode != NULL && _baseNode->getOpCodeValue() == TR::loadaddr && self()->getUnresolvedSnippet() != NULL)
      {
      self()->createUnresolvedDataSnippetForBaseNode(cg, writableLiteralPoolRegister);
      }

   _indexRegister = NULL;
   _symbolReference = symRef;
   _originalSymbolReference = symRef;
   _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(symRef, cg)? symRef : NULL;
   if (self()->getUnresolvedSnippet() == NULL)
      {
      _offset = symRef->getOffset();
      }
   else
      {
      _offset = 0;
      }

   self()->enforceDisplacementLimit(node, cg, NULL);
   // TODO: aliasing sets?
   }

void
OMR::Z::MemoryReference::initSnippetPointers(TR::Snippet * s, TR::CodeGenerator * cg)
   {
   if (s->getKind() == TR::Snippet::IsUnresolvedData)
      {
      self()->setUnresolvedSnippet((TR::UnresolvedDataSnippet *) s);
      }
   else if (s->getKind() == TR::Snippet::IsWritableData ||
            s->getKind() == TR::Snippet::IsConstantInstruction ||
            s->getKind() == TR::Snippet::IsConstantData)
      {
      self()->setConstantDataSnippet((TR::S390ConstantDataSnippet *) s);
      }
   }

OMR::Z::MemoryReference::MemoryReference(TR::Snippet * s, TR::Register * indx, int32_t disp, TR::CodeGenerator * cg)
   : _baseNode(NULL), _baseRegister(NULL), _indexRegister(indx), _indexNode(NULL), _targetSnippetInstruction(NULL),
    _offset(disp), _flags(0), _storageReference(NULL), _fixedSizeForAlignment(0), _leftMostByte(0), _name(NULL),
    _incrementedNodesList(cg->comp()->trMemory())
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _originalSymbolReference = _symbolReference;
   _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;

   // Enable a check for the long disp slot in generateBin phase
   self()->setupCheckForLongDispFlag(cg);

   self()->setupCausesImplicitNullPointerException(cg);

   self()->setBaseRegister(cg->getLitPoolRealRegister(), cg);
   self()->initSnippetPointers(s, cg);
   }

OMR::Z::MemoryReference::MemoryReference(TR::Snippet * s, TR::CodeGenerator * cg, TR::Register * base, TR::Node * node)
   : _baseNode(NULL), _baseRegister(NULL), _indexRegister(NULL), _indexNode(NULL), _targetSnippetInstruction(NULL),
   _offset(0), _flags(0), _storageReference(NULL), _fixedSizeForAlignment(0), _leftMostByte(0), _name(NULL),
   _incrementedNodesList(cg->comp()->trMemory())
   {
   TR::Compilation *comp = cg->comp();
   if (s->getKind() == TR::Snippet::IsConstantData)
      _symbolReference = comp->getSymRefTab()->findOrCreateConstantAreaSymbolReference();
   else
      _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(comp->getSymRefTab());
   _originalSymbolReference = _symbolReference;
   _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;

   // Enable a check for the long disp slot in generateBin phase
   self()->setupCheckForLongDispFlag(cg);

   self()->setupCausesImplicitNullPointerException(cg);

   if (base)
      {
      self()->setBaseRegister(base, cg);
      //TR_ASSERT(cg->supportsOnDemandLiteralPool()==true, "May not be here with Literal Pool On Demand disabled\n");
      }
   else
      {
      if (cg->isLiteralPoolOnDemandOn())
         {
         if (cg->comp()->target().is64Bit())
            self()->setBaseRegister(cg->allocateRegister(), cg);
         else
            self()->setBaseRegister(cg->allocateRegister(), cg);
         generateLoadLiteralPoolAddress(cg, node, _baseRegister);
         cg->stopUsingRegister(_baseRegister);
         }
      else
         {
         self()->setBaseRegister(cg->getLitPoolRealRegister(), cg);
         }
      }

   self()->initSnippetPointers(s, cg);
   }

OMR::Z::MemoryReference::MemoryReference(int32_t           disp,
                                               TR::CodeGenerator *cg,
                                               bool              isConstantDataSnippet) :
  _baseRegister(NULL),
  _baseNode(NULL),
  _indexRegister(NULL),
  _indexNode(NULL),
  _targetSnippet(NULL),
  _targetSnippetInstruction(NULL),
  _flags(0),
  _storageReference(NULL),
  _fixedSizeForAlignment(0),
  _leftMostByte(0),
  _name(NULL),
  _incrementedNodesList(cg->comp()->trMemory())
  {
  TR::Compilation *comp = cg->comp();
     {
     _offset = disp;
     _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(comp->getSymRefTab());
     _originalSymbolReference = _symbolReference;
     _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;
     self()->setupCausesImplicitNullPointerException(cg);
     }
  }

OMR::Z::MemoryReference::MemoryReference(TR::CodeGenerator *cg) :
  _baseRegister(NULL),
  _baseNode(NULL),
  _indexRegister(NULL),
  _indexNode(NULL),
  _targetSnippet(NULL),
  _targetSnippetInstruction(NULL),
// this flag is used for 64bit code-gen only under current J9 fat object model
  _flags(0),
  _storageReference(NULL),
  _fixedSizeForAlignment(0),
  _leftMostByte(0),
  _name(NULL),
  _incrementedNodesList(cg->comp()->trMemory())
  {
  _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
  _originalSymbolReference = _symbolReference;
  _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;
  _offset = _symbolReference->getOffset();
  self()->setupCausesImplicitNullPointerException(cg);
  }

OMR::Z::MemoryReference::MemoryReference(TR::Register      *br,
                       int32_t           disp,
                       TR::CodeGenerator *cg,
                       const char *name) :
  _baseRegister(br),
  _baseNode(NULL),
  _indexRegister(NULL),
  _indexNode(NULL),
  _targetSnippet(NULL),
  _targetSnippetInstruction(NULL),
  _offset(disp),
  _flags(0),
  _storageReference(NULL),
  _fixedSizeForAlignment(0),
  _leftMostByte(0),
  _name(name),
  _incrementedNodesList(cg->comp()->trMemory())
  {
  _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
  _originalSymbolReference = _symbolReference;
  _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;
  self()->setupCausesImplicitNullPointerException(cg);

  }

OMR::Z::MemoryReference::MemoryReference(TR::Register      *br,
                       int32_t           disp,
                       TR::SymbolReference *symRef,
                       TR::CodeGenerator *cg) :
  _baseRegister(br),
  _symbolReference(symRef),
  _baseNode(NULL),
  _indexRegister(NULL),
  _indexNode(NULL),
  _targetSnippet(NULL),
  _targetSnippetInstruction(NULL),
  _offset(disp),
  _flags(0),
  _storageReference(NULL),
  _fixedSizeForAlignment(0),
  _leftMostByte(0),
  _name(NULL),
  _incrementedNodesList(cg->comp()->trMemory())
  {
  _originalSymbolReference = _symbolReference;
  _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;
  self()->setupCausesImplicitNullPointerException(cg);

  }

OMR::Z::MemoryReference::MemoryReference(TR::Register      *br,
                       TR::Register      *ir,
                       int32_t           disp,
                       TR::CodeGenerator *cg) :
  _baseRegister(br),
  _baseNode(NULL),
  _indexRegister(ir),
  _indexNode(NULL),
  _targetSnippet(NULL),
  _targetSnippetInstruction(NULL),
  _offset(disp),
  _flags(0),
  _storageReference(NULL),
  _fixedSizeForAlignment(0),
  _leftMostByte(0),
  _name(NULL),
  _incrementedNodesList(cg->comp()->trMemory())
  {
  _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
  _originalSymbolReference = _symbolReference;
  _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;
  self()->setupCausesImplicitNullPointerException(cg);

  }

OMR::Z::MemoryReference::MemoryReference(MemoryReference& mr, int32_t n, TR::CodeGenerator *cg) :
   _incrementedNodesList(cg->comp()->trMemory())
   {
   _baseRegister      = mr._baseRegister;
   _baseNode          = mr._baseNode;
   _indexRegister     = mr._indexRegister;
   _indexNode         = mr._indexNode;
   _symbolReference   = mr._symbolReference;
   _originalSymbolReference = mr._originalSymbolReference;
   _listingSymbolReference = mr._listingSymbolReference;
   _offset            = mr._offset + n;
   _targetSnippet     = mr._targetSnippet;
   _storageReference  = mr._storageReference;
   _fixedSizeForAlignment = mr._fixedSizeForAlignment;
   _leftMostByte      = mr._leftMostByte;
   _flags             = mr._flags;
   _name              = mr._name;
   self()->resetIs2ndMemRef();
   self()->resetMemRefUsedBefore();
   }

/**
 * An unresolved data snippet and constant data snippet are mutually exclusive for
 * a given memory reference.  Hence, they share the same pointer.
 */
TR::UnresolvedDataSnippet *
OMR::Z::MemoryReference::getUnresolvedSnippet()
   {
   return self()->isUnresolvedDataSnippet() ? (TR::UnresolvedDataSnippet *)_targetSnippet : NULL;
   }

TR::UnresolvedDataSnippet *
OMR::Z::MemoryReference::setUnresolvedSnippet(TR::UnresolvedDataSnippet *s)
   {
   self()->setUnresolvedDataSnippet();
   return (TR::UnresolvedDataSnippet *) (_targetSnippet = (TR::Snippet *) s);
   }

TR::S390ConstantDataSnippet *
OMR::Z::MemoryReference::getConstantDataSnippet()
   {
   return self()->isConstantDataSnippet() ? (TR::S390ConstantDataSnippet *)_targetSnippet : NULL;
   }

TR::S390ConstantDataSnippet *
OMR::Z::MemoryReference::setConstantDataSnippet(TR::S390ConstantDataSnippet *s)
   {
   self()->setConstantDataSnippet();
   return (TR::S390ConstantDataSnippet *) (_targetSnippet = s);
   }

void
OMR::Z::MemoryReference::setLeftAlignMemRef(int32_t leftMostByte)
   {
   TR_ASSERT(leftMostByte > 0,"invalid leftMostByte of %d provided in setLeftAlignMemRef()\n");
    _flags.reset(TR_S390MemRef_RightAlignMemRef);
    _flags.set(TR_S390MemRef_LeftAlignMemRef);
   self()->setLeftMostByte(leftMostByte);
   }

bool
OMR::Z::MemoryReference::isAligned()
   {
   return self()->rightAlignMemRef() || self()->leftAlignMemRef();
   }

const bool
OMR::Z::MemoryReference::isLongDisplacementRequired()
   {
   auto displacement = self()->getOffset();

   return ((displacement > MINLONGDISP && displacement < MINDISP) || (displacement >= MAXDISP && displacement < MAXLONGDISP));
   }

const bool
OMR::Z::MemoryReference::isHugeDisplacementRequired()
   {
   auto displacement = self()->getOffset();

   return (displacement <= MINLONGDISP || displacement >= MAXLONGDISP);
   }

void
OMR::Z::MemoryReference::setOriginalSymbolReference(TR::SymbolReference * ref, TR::CodeGenerator * cg)
   { _originalSymbolReference = ref;
   if (!ref->getSymbol()->isShadow())
      {
      _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(ref, cg)? ref : NULL;
      }
   }

void
OMR::Z::MemoryReference::decNodeReferenceCounts(TR::CodeGenerator * cg)
   {
   if (_baseRegister != NULL)
      {
      if (_baseRegister != cg->getLitPoolRealRegister() &&  _baseNode != NULL)
         {
         cg->decReferenceCount(_baseNode);
         }
      else if (_baseRegister != cg->getLitPoolRealRegister())
         {
         cg->stopUsingRegister(_baseRegister);
         }
      }

   if (_indexRegister != NULL)
      {
      if (_indexNode != NULL)
         {
         cg->decReferenceCount(_indexNode);
         }
      else
         {
         cg->stopUsingRegister(_indexRegister);
         }
      }
   }

void
OMR::Z::MemoryReference::stopUsingMemRefRegister(TR::CodeGenerator * cg)
   {
   if ((_baseRegister != NULL) &&
      ((_baseRegister->getRealRegister() == NULL) ||
       (_baseRegister->getRealRegister()->getState() != TR::RealRegister::Locked)))
      {
      cg->stopUsingRegister(_baseRegister);
      }

   if (_indexRegister != NULL)
      {
      cg->stopUsingRegister(_indexRegister);
      }

   // It is the job of OMR::Z::CodeGenerator::StopUsingEscapedMemRefsRegisters to
   // call stopUsingMemRefRegister on s390 memory references that have "escaped",
   // but we do not want to doubly call this function on the same ref.
   cg->RemoveMemRefFromStack(self());
   }


void
OMR::Z::MemoryReference::bookKeepingRegisterUses(TR::Instruction * instr, TR::CodeGenerator * cg)
   {
   if (_baseRegister != NULL)
      {
      instr->useRegister(_baseRegister);
      _baseRegister->setIsUsedInMemRef();
      // Set addressing register to 64 bit data
      // but be careful of instructions using X type operand to specify something else other then memory
      if(cg->comp()->target().is64Bit() && (instr->getOpCode().isLoad() || instr->getOpCode().isStore() ||
                                 instr->getOpCodeValue() == TR::InstOpCode::LA || instr->getOpCodeValue() == TR::InstOpCode::LARL ||
                                 instr->getOpCodeValue() == TR::InstOpCode::LAY || instr->getOpCodeValue() == TR::InstOpCode::LAE ||
                                 instr->getOpCodeValue() == TR::InstOpCode::LAEY) )
         {
         _baseRegister->setIs64BitReg(true);
         }
      }
   if (_indexRegister != NULL)
      {
      instr->useRegister(_indexRegister);
      _indexRegister->setIsUsedInMemRef();
      if(cg->comp()->target().is64Bit() && (instr->getOpCode().isLoad() || instr->getOpCode().isStore() ||
                                 instr->getOpCodeValue() == TR::InstOpCode::LA || instr->getOpCodeValue() == TR::InstOpCode::LARL ||
                                 instr->getOpCodeValue() == TR::InstOpCode::LAY || instr->getOpCodeValue() == TR::InstOpCode::LAE ||
                                 instr->getOpCodeValue() == TR::InstOpCode::LAEY) )
         {
         _indexRegister->setIs64BitReg(true);
         }
      }
   }

/**
 * Add these two function calls to the beginning and end of each "populate*" function that
 * is called externally during evaluation.
 */
void noteAllNodesWithRefCountNotOne(TR_Array< TR::Node * > & nodeArray, TR::Node * n, TR::Compilation * comp)
   {
   // do not track ref cnt = 1 in before tree
   // the function which checks for problematic memory references will check:
   //
   // 1) before ref cnt != 1.
   // 2) after ref cnt = 1.
   //
   // Both full and partial evaluator scope covers only themselves and their children:

   // self
   if (n->getReferenceCount() > 1)
      {
      nodeArray.add(n);
      }

   // children
   for (uint32_t i = 0; i < n->getNumChildren(); i++)
      {
      TR::Node * child = n->getChild(i);
      if (child->getReferenceCount() > 1)
         {
         nodeArray.add(child);
         }
      }
   }

void ArtificiallyInflateReferenceCountWhenNecessary(TR::MemoryReference * mr, const char * functionString, TR_Array< TR::Node * > & nodeArray,
                                                    TR::CodeGenerator * cg, List<TR::Node> * incrementedNodesList)
   {
   TR::Compilation *comp = cg->comp();

   for (uint32_t i = 0; i < nodeArray.size(); i++)
      {
      // Stop registers escaping a partial evaluation with ref count 1.
      if ((nodeArray[i]->getReferenceCount() == 1) && (nodeArray[i]->getRegister() != NULL))
         {
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp,
                     " ArtificiallyInflateReferenceCountWhenNecessary: bug potential with node %p (reg %p %s%s): ref count after %s is 1.\n",
                     nodeArray[i],
                     nodeArray[i]->getRegister(),
                     nodeArray[i]->getRegister() ? ( nodeArray[i]->getRegister()->isPlaceholderReg() ? "D_" : "" ) : "?",
                     comp->getDebug()->getName(nodeArray[i]->getRegister()),
                     functionString);
            }

         if (incrementedNodesList)
            {
            // This node's reference count went from N -> 1 during "evaluation" in one of the populate* functions.
            //
            // Check if this node's reference count was inflated artificially somewhere else - look for the reference count
            // changing like so:
            //
            // 1 -> N -> 1
            //
            // If this is found, do not artificially inflate here:
            //
            // 1 -> N -> 1 -> 2
            //
            // as this can have performance implications - the reference count came into this scope at 1, and by
            // inflating it we are saying that we can't evaluate it destructively.
            //
            ListIterator<TR::Node> listIt(incrementedNodesList);
            TR::Node * listNode = listIt.getFirst();

            for (; listNode; listNode = listIt.getNext())
               {
               if (listNode == nodeArray[i])
                  {
                  break;
                  }
               }

            if (listNode == nodeArray[i])
               {
               // Skip processing the current node because the reference count was already artificially inflated somewhere else.
               if (comp->getOption(TR_TraceCG))
                  {
                  traceMsg(comp,
                           " ArtificiallyInflateReferenceCountWhenNecessary: skip inflating node %p (reg %p %s%s) because it exists in incrementedNodesList.\n",
                           nodeArray[i],
                           nodeArray[i]->getRegister(),
                           nodeArray[i]->getRegister() ? ( nodeArray[i]->getRegister()->isPlaceholderReg() ? "D_" : "" ) : "?",
                           comp->getDebug()->getName(nodeArray[i]->getRegister()),
                           functionString);
                  }

               continue;
               }
            }

         // Add to a stack so that these nodes will have their reference counts decreased after evaluation.
         if (cg->AddArtificiallyInflatedNodeToStack(nodeArray[i]))
            {
            // Artificially inflate reference count by 1. This will not allow destructive evaluation of this node.
            cg->incReferenceCount(nodeArray[i]);

            // Reference count is now 2.
            cg->incRefCountForOpaquePseudoRegister(nodeArray[i]);
            }
         else
            {
            if (comp->getOption(TR_TraceCG))
               {
               comp->getDebug()->trace(" could not add node %p to cg stack\n", nodeArray[i]);
               }
            }
         }
      }
   }


void
OMR::Z::MemoryReference::populateAddTree(TR::Node * subTree, TR::CodeGenerator * cg)
   {
   TR::StackMemoryRegion stackMemoryRegion(*cg->trMemory());

   bool memRefPopulated = false;
   TR_Array< TR::Node * > nodesBefore(cg->trMemory(), 8, true, stackAlloc);
   TR::Compilation *comp = cg->comp();

   noteAllNodesWithRefCountNotOne(nodesBefore, subTree, comp);

   TR::Node * addressChild = subTree->getFirstChild();
   TR::Node * integerChild = subTree->getSecondChild();

   if (integerChild->getOpCode().isLoadConst() &&
       (!comp->useCompressedPointers() ||
       (integerChild->getOpCodeValue() != TR::lconst) ||
       (integerChild->getLongInt() != TR::Compiler->vm.heapBaseAddress())))
      {
      self()->populateMemoryReference(addressChild, cg);
      if ((subTree->getOpCodeValue() != TR::iadd) && (subTree->getOpCodeValue() != TR::aiadd) && (subTree->getOpCodeValue() != TR::isub))
         {
         if (subTree->getOpCode().isSub())
            _offset -= integerChild->getLongInt();
         else
            _offset += integerChild->getLongInt();
         }
      else
         {
         if (subTree->getOpCode().isSub())
            _offset -= integerChild->getInt();
         else
            _offset += integerChild->getInt();
         }
      memRefPopulated = true;
      }
   else if ( _baseRegister == NULL && _indexRegister == NULL &&
            integerChild->getReferenceCount() == 1       &&
            (integerChild->getOpCodeValue() == TR::isub || integerChild->getOpCodeValue() == TR::lsub))
      {
      //
      // catch the pattern of aiadd on iaload <base> / isub of <expression> and -<constant> and
      // convert it into LA Rx,<constant>(R<expression>,R<base>)
      //
      bool usingAladd = (cg->comp()->target().is64Bit()) ? true : false;

      TR::Node * firstSubChild = integerChild->getFirstChild();
      TR::Node * secondSubChild = integerChild->getSecondChild();
      if (secondSubChild->getOpCode().isLoadConst())
         {
         intptr_t value;
         if (usingAladd)
            {
            if (secondSubChild->getType().isInt32())
               {
               value = secondSubChild->getInt();
               }
            else
               {
               value = secondSubChild->getLongInt();
               }
            }
         else
            {
            value = secondSubChild->getInt();
            }

         intptr_t totalOff = _offset - value;

         if (cg->isDispInRange(totalOff) && integerChild->getRegister() == NULL)
            {
            memRefPopulated = true;

            _indexRegister = cg->evaluate(firstSubChild);

            // not necessary to sign extend explicitly anymore due to aladd
            // when aladd is tested, remove the explicit sign extension code below

            self()->setBaseRegister(cg->evaluate(addressChild), cg);
            _offset -= value;

            if (firstSubChild->getReferenceCount() > 1)
               {
               cg->decReferenceCount(firstSubChild);
               }
            else
               {
               // Keep the register alive, as other node may need it later
               firstSubChild->decReferenceCount();
               TR::Register * reg = firstSubChild->getRegister();
               if (reg && reg->isLive())
                  {
                  TR_LiveRegisterInfo * liveRegister = reg->getLiveRegisterInfo();
                  liveRegister->decNodeCount();
                  }
               }
            cg->decReferenceCount(secondSubChild);
            }
         }
      }

   if (!memRefPopulated && (integerChild->getEvaluationPriority(cg) > addressChild->getEvaluationPriority(cg)))
      {
      self()->populateMemoryReference(addressChild, cg);
      self()->populateMemoryReference(integerChild, cg);
      }
   else if (!memRefPopulated)
      {
      self()->populateMemoryReference(addressChild, cg);
      if (_baseRegister != NULL && _indexRegister != NULL)
         {
         self()->consolidateRegisters(subTree, cg);
         }
      self()->populateMemoryReference(integerChild, cg);
      }

   if ((integerChild->getOpCodeValue() == TR::isub || integerChild->getOpCodeValue() == TR::lsub))
      {
      /*
       * arraycopy
       *   aladd
       *     ==>aRegLoad
       *     lsub
       *       ==>lRegLoad
       *       lconst
       *   aladd
       *     ==>aconst NULL
       *     lsub
       *       ==>lRegLoad
       *       ==>lconst
       *
       * With this tree, the first and second aladd get handled slightly differently.
       * The first has a non NULL addressChild so that get's set as the _baseRegister.
       * We will fold the lsub into the aladd's memref so the lRegLoad becomes the _indexRegister
       * and the lconst becomes the offset.
       * For the second aladd, the NULL addressChild means we use the integerChild as the _baseRegister.
       * This is needed to prevent Zero Address Detection events on z/OS which happen when the base register of an instruction is zero.
       * When we fold the lsub the lRegLoad becomes the _baseRegister and the lconst becomes offset.
       * The important point is that in this case the _indexRegister is NULL.
       * Thus we need to take care to select the appropriate register in the code below.
       */

      TR::Register *integerChildRegister = (_indexRegister != NULL) ? _indexRegister : _baseRegister;

      if (integerChild->getRegister() == NULL && integerChildRegister->isLive())
         {
         TR_LiveRegisterInfo * liveRegister = integerChildRegister->getLiveRegisterInfo();
         int32_t owningNodeCount = 0;
         integerChild->setRegister(integerChildRegister);
         owningNodeCount = liveRegister->getNodeCount();
         cg->decReferenceCount(integerChild);
         if (liveRegister->getNodeCount() == owningNodeCount)
            {
            integerChild->unsetRegister();
            }
         }
      else
         {
         cg->decReferenceCount(integerChild);
         }
      cg->decReferenceCount(addressChild);
      }
   else
      {
      cg->decReferenceCount(integerChild);
      cg->decReferenceCount(addressChild);
      }

   ArtificiallyInflateReferenceCountWhenNecessary(self(), "OMR::Z::MemoryReference::populateAddTree", nodesBefore, cg, &_incrementedNodesList);
   }

void
OMR::Z::MemoryReference::populateShiftLeftTree(TR::Node * subTree, TR::CodeGenerator * cg)
   {
   if (_indexRegister != NULL)
      {
      if (_baseRegister != NULL)
         {
         self()->consolidateRegisters(subTree, cg);
         }
      else
         {
         self()->setBaseRegister(_indexRegister, cg);
         _baseNode = _indexNode;
         }
      }

   bool strengthReducedShift = false;

   if (((subTree->getOpCodeValue() == TR::ishl) && (subTree->getSecondChild()->getOpCodeValue() == TR::iconst) && (subTree->getSecondChild()->getInt() == 1)) ||
       ((subTree->getOpCodeValue() == TR::lshl) && (subTree->getSecondChild()->getOpCodeValue() == TR::iconst) && (subTree->getSecondChild()->getInt() == 1)))
      {
      TR::Node *firstChild=subTree->getFirstChild();
      TR::Register *firstRegister = firstChild->getRegister();
      if (firstRegister && !_baseRegister && !_indexRegister)
         {
         strengthReducedShift = true;
         self()->setBaseRegister(firstRegister, cg);
         _baseNode = firstChild;
         _indexRegister = firstRegister;
         _indexNode = firstChild;
         cg->decReferenceCount(firstChild);
         }
      }

   if (!strengthReducedShift)
      {
      _indexRegister = cg->evaluate(subTree);
      _indexNode = subTree;
      }
   }

void
OMR::Z::MemoryReference::populateAloadTree(TR::Node * subTree, TR::CodeGenerator * cg, bool privateArea)
   {
   TR::Register * addReg = NULL;

   if (privateArea)
      addReg = cg->getS390Linkage()->getPrivateStaticBaseRealRegister();
   else
      addReg = cg->getS390Linkage()->getStaticBaseRealRegister();

   if (addReg)
      {
      if (_baseRegister != NULL)
         {
         if (_indexRegister != NULL)
            {
            self()->consolidateRegisters(subTree, cg);
            }
         // switch base and index registers
         _indexRegister = _baseRegister;
         self()->setBaseRegister(addReg, cg);
         _indexNode = NULL;
         }
      else
         {
         self()->setBaseRegister(addReg, cg);
         _baseNode = NULL;
         }
      }
   }

void
OMR::Z::MemoryReference::populateLoadAddrTree(TR::Node * subTree, TR::CodeGenerator * cg)
   {
   TR::Symbol * symbol = subTree->getSymbol();
   TR::Register * addReg = NULL;
   bool noNeedToPropagateSymRef = false;
   TR::Compilation *comp = cg->comp();

   if (symbol->isRegisterMappedSymbol())
      {
      if (symbol->isMethodMetaData())
         {
         addReg = cg->getMethodMetaDataRealRegister();
         }
      else
         {
         addReg = cg->getStackPointerRealRegister(symbol);
         }
      }
   else if (symbol->isStatic())
      {
         {
            {
            addReg = cg->evaluate(subTree);
            }
         }
      }

   if (addReg)
      {

      if (_baseRegister != NULL)
         {
         if (_indexRegister != NULL)
            {
            self()->consolidateRegisters(subTree, cg);
            }
         // switch base and index registers
         _indexRegister = _baseRegister;
         self()->setBaseRegister(addReg, cg);
         _indexNode = NULL;
         }
      else
         {
         self()->setBaseRegister(addReg, cg);
         _baseNode = NULL;
         }
      }
   if (subTree->getSymbol()->isLocalObject())
      {
      _baseNode = subTree;
      }

   // Need to associate symref of loadaddr to the memory reference
   // so that the resolution of any offsets (in particular autos on stack)
   // can be resolved.
   //  i.e.  iaload <o.f>+12
   //           loadaddr <auto>
   // will generate
   //       L  +12+?(GPR5)  <-- <auto> symref will resolve the ? offset.
   //                           despite the proper symref should be <o.f>
   if (!self()->isExposedConstantAddressing() && !noNeedToPropagateSymRef)
      _symbolReference = subTree->getSymbolReference();

   _offset += subTree->getSymbolReference()->getOffset();
   }


bool OMR::Z::MemoryReference::tryBaseIndexDispl(TR::CodeGenerator* cg, TR::Node* loadStore, TR::Node* topAdd)
   {
   TR::Node* addressChild = topAdd->getFirstChild();
   TR::Node* integerChild = topAdd->getSecondChild();
   TR::Node* otherAdd = NULL;
   TR::Node* sub = NULL;
   TR::Node* base = NULL;
   TR::Node* index = NULL;
   intptr_t offset = 0;
   bool big = topAdd->getOpCodeValue() == TR::aladd;
   bool debug = false;
   TR::Register* breg = NULL;
   TR::Register* ireg= NULL;

   // From here, down, stack memory allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*cg->trMemory());

   TR_Array< TR::Node * > nodesBefore(cg->trMemory(), 8, true, stackAlloc);
   TR::Compilation *comp = cg->comp();

   if (debug)
      traceMsg(comp, "&&& TBID load=%llx addr1=%llx addr2=%llx\n",
         loadStore, topAdd, addressChild);

   noteAllNodesWithRefCountNotOne(nodesBefore, topAdd, comp);

   if (!loadStore->getOpCode().isLoadVar() &&
       !loadStore->getOpCode().isStore())
      return false;
   if (topAdd->getRegister() != NULL) return false;
   if (integerChild->getOpCode().isLoadConst()) return false;

   if (addressChild->getOpCodeValue() == TR::aladd ||
       addressChild->getOpCodeValue() == TR::aiadd)
      otherAdd = addressChild;
   if (otherAdd && otherAdd->getSecondChild()->getOpCode().isLoadConst())
      {
      base = otherAdd->getFirstChild();
      if (big)
         offset += otherAdd->getSecondChild()->getLongInt();
      else
         offset += otherAdd->getSecondChild()->getInt();
      }
   else
      base = addressChild;

   if (integerChild->getOpCodeValue() == TR::lsub ||
       integerChild->getOpCodeValue() == TR::isub)
      sub = integerChild;
   if (sub && sub->getRegister() == NULL)
      {
      if (sub->getSecondChild()->getOpCode().isLoadConst())
         {
         index = sub->getFirstChild();
         if (big)
            offset -= sub->getSecondChild()->getLongInt();
         else
            offset -= sub->getSecondChild()->getInt();
         }
      }
   else
      index = topAdd->getSecondChild();

   if (base == NULL || index == NULL) return false;

   if (debug)
      {
      if (base)
         traceMsg(comp, "&&& TBID base %llx count %d\n", base,
            base->getReferenceCount());
      if (index)
         traceMsg(comp, "&&& TBID index %llx count %d\n", index,
            index->getReferenceCount());
      traceMsg(comp, "&&& TBID offset %lld\n", offset);
      }

   if (!cg->isDispInRange(offset)) return false;
   breg = base->getRegister();
   if (breg == NULL && base->getReferenceCount() <= 1) return false;
   ireg = index->getRegister();
   if (ireg == NULL && index->getReferenceCount() <= 1) return false;
   if (breg == NULL)
      breg = cg->evaluate(base);
   if (ireg == NULL)
      ireg = cg->evaluate(index);

   if (topAdd->getReferenceCount() == 1)
      {
      if (debug) traceMsg(comp, "&&& TBID recursive decrement\n");
      cg->recursivelyDecReferenceCount(topAdd);
      }
   else
      cg->decReferenceCount(topAdd);

   if (debug)
      {
      if (sub)
         traceMsg(comp, "&&& TBID sub rc %d\n", sub->getReferenceCount());
      traceMsg(comp, "&&& TBID index rc %d\n", index->getReferenceCount());
      }

   self()->setBaseRegister(breg, cg);
   _indexRegister = ireg;
   self()->setOffset(offset);
   cg->AddFoldedMemRefToStack(self());
   ArtificiallyInflateReferenceCountWhenNecessary(self(), "OMR::Z::MemoryReference::tryBaseIndexDispl", nodesBefore, cg, &_incrementedNodesList);
   return true;
   }

void
OMR::Z::MemoryReference::populateThroughEvaluation(TR::Node * rootLoadOrStore, TR::CodeGenerator * cg)
   {
   _baseNode = NULL;
   _indexRegister = NULL;
   _indexNode = NULL;
   _targetSnippet = NULL;
   _targetSnippetInstruction = NULL;
   _offset = 0;
   _flags = 0;
   _fixedSizeForAlignment = 0;
   _leftMostByte = 0;
   self()->setBaseRegister(cg->evaluate(rootLoadOrStore), cg);
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _originalSymbolReference = _symbolReference;

   _listingSymbolReference = TR::MemoryReference::shouldLabelForRAS(_symbolReference, cg)? _symbolReference : NULL;

   self()->setupCausesImplicitNullPointerException(cg);
   }

bool OMR::Z::MemoryReference::ZeroBasePtr_EvaluateSubtree(TR::Node * subTree, TR::CodeGenerator * cg, OMR::Z::MemoryReference * mr)
   {
   // ZeroBasePtr check: do not evaluate subTree if the base node is a const.
   switch(subTree->getOpCodeValue())
      {
      case TR::aconst:    // load address constant (zero value means NULL)
      case TR::iconst:    // load integer constant (32-bit signed 2's complement)
      case TR::lconst:    // load long integer constant (64-bit signed 2's complement)
      case TR::bconst:    // load byte integer constant (8-bit signed 2's complement)
      case TR::sconst:    // load short integer constant (16-bit signed 2's complement)
         {
         if (subTree->getRegister()) return false;

         int64_t dispVal = (int64_t)mr->getOffset() + getIntegralValue(subTree);
#if TR_HOST_64BIT
         if (!cg->isDispInRange(dispVal))
            {
            return false;
            }
#else // 32 bit
         if ((dispVal >= TR::getMaxSigned<TR::Int32>()) || !cg->isDispInRange((intptr_t)dispVal))
            {
            return false;
            }
#endif
         if (!cg)
            {
            TR_ASSERT(0, "Conversion to S390 Code Generator succeeds\n");
            }

         // If the user has a value of zero as a base of a memory reference, this is a null pointer error, but
         // if the base of a memory reference is explicitly set with "based(0)", then the intention is to base
         // off of the contents of R0, which could be a constant 0. This avoids flagging the instruction as
         // a null pointer error.

         mr->setBaseRegister(NULL, cg);

         // The value of the const node refers to the offset from R0.
         mr->addToOffset( (intptr_t)( getIntegralValue( subTree ) & TR::getMaxUnsigned<TR::Int32>() ) );
         return true;
         }
      default:
         {
         // Fall through
         }
      }

   return false;
   }

void
OMR::Z::MemoryReference::populateMemoryReference(TR::Node * subTree, TR::CodeGenerator * cg)
   {
   TR::StackMemoryRegion stackMemoryRegion(*cg->trMemory());

   TR::Compilation *comp = cg->comp();
   TR::Node *noopNode = NULL;
   TR_Array< TR::Node * > nodesBefore(cg->trMemory(), 8, true, stackAlloc);

   noteAllNodesWithRefCountNotOne(nodesBefore, subTree, comp);

   if (((comp->useCompressedPointers() && subTree->getOpCodeValue() == TR::l2a))
           && (subTree->getReferenceCount() == 1) && (subTree->getRegister() == NULL))
      {
      noopNode = subTree;
      subTree = subTree->getFirstChild();
      }

   if (self()->doEvaluate(subTree, cg))
      {
      if (_baseRegister != NULL)
         {
         if (_indexRegister != NULL)
            {
            self()->consolidateRegisters(subTree, cg);
            }
         self()->setIndexRegister(cg->evaluate(subTree));
         _indexNode = subTree;
         }
      else
         {
         if (!self()->ZeroBasePtr_EvaluateSubtree(subTree, cg, this))
            {
            if (_baseRegister == NULL)
               self()->setBaseRegister(cg->evaluate(subTree), cg);
            }

         _baseNode = subTree;
         }
      }
   else
      {
      // Add this memory reference to a list of "folded" memory references, because it is not the doEvaluate branch
      // and there is the possibility that the mem ref will not be loaded into a register.
      cg->AddFoldedMemRefToStack(self());

      if (subTree->getOpCode().isArrayRef() ||
          (subTree->getOpCodeValue() == TR::iadd || subTree->getOpCodeValue() == TR::ladd ||
           ((subTree->getOpCodeValue() == TR::isub || subTree->getOpCodeValue() == TR::lsub) &&
            subTree->getChild(1)->getOpCode().isLoadConst() &&
            subTree->getChild(1)->getConst<int64_t>() <= 0)))
         {
         self()->populateAddTree(subTree, cg);
         }
      else if ((subTree->getOpCodeValue() == TR::ishl || subTree->getOpCodeValue() == TR::lshl) &&
         subTree->getSecondChild()->getOpCode().isLoadConst())
         {
         self()->populateShiftLeftTree(subTree, cg);
         }
      else if (subTree->getOpCodeValue() == TR::loadaddr)
         {
         self()->populateLoadAddrTree(subTree, cg);
         }
      else if (subTree->getOpCodeValue() == TR::aconst)
         {
         if (cg->comp()->target().is64Bit())
            {
            _offset += subTree->getLongInt();
            }
         else
            {
            _offset += subTree->getInt();
            }
         // set zero based ptr
         self()->setBaseRegister(NULL, cg);
         }
      else
         {
         if (_baseRegister != NULL)
            {
            if (_indexRegister != NULL)
               {
               self()->consolidateRegisters(subTree, cg);
               }
            _indexRegister = cg->evaluate(subTree);
            _indexNode = subTree;
            }
         else
            {
            if (!self()->ZeroBasePtr_EvaluateSubtree(subTree, cg, this))
               {
               if (_baseRegister == NULL)
                  self()->setBaseRegister(cg->evaluate(subTree), cg);
               }
            _baseNode = subTree;
            }
         }
      }

   if (noopNode && noopNode->getRegister() == NULL)
      {
      if (subTree->getRegister() && subTree->getRegister()->getRegisterPair())
         noopNode->setRegister(subTree->getRegister()->getRegisterPair()->getLowOrder());
      else
         noopNode->setRegister(subTree->getRegister());

      if (_baseNode == subTree)
         {
         self()->setBaseRegister(noopNode->getRegister(), cg);
         _baseNode = noopNode;
         }
      else if (_indexNode == subTree)
         {
         self()->setIndexRegister(noopNode->getRegister());
         _indexNode = noopNode;
         }
         // pretend we evaluated the noopNode
      cg->decReferenceCount(subTree);
      }

   ArtificiallyInflateReferenceCountWhenNecessary(self(), "OMR::Z::MemoryReference::populateMemoryReference", nodesBefore, cg, &_incrementedNodesList);
   }

void
OMR::Z::MemoryReference::consolidateRegisters(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * tempTargetRegister;
   if ((_baseRegister && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())) ||
      (_indexRegister && (_indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())))
      {
      if (node && node->isInternalPointer() && node->getPinningArrayPointer())
         {
         if (cg->comp()->target().is64Bit())
            tempTargetRegister = cg->allocateRegister();
         else
            tempTargetRegister = cg->allocateRegister();

         tempTargetRegister->setContainsInternalPointer();
         tempTargetRegister->setPinningArrayPointer(node->getPinningArrayPointer());
         }
      else
         {
         tempTargetRegister = cg->allocateCollectedReferenceRegister();
         }
      }
   else
      {
      if (cg->comp()->target().is64Bit())
         tempTargetRegister = cg->allocateRegister();
      else
         tempTargetRegister = cg->allocateRegister();
      }

   TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);

   interimMemoryReference->setBaseRegister(_baseRegister, cg);
   interimMemoryReference->setIndexRegister(_indexRegister);
   TR::SymbolReference *interinSymRef=interimMemoryReference->getSymbolReference();
   if (self()->isConstantDataSnippet()) interimMemoryReference->setConstantDataSnippet();
   if (self()->getConstantDataSnippet())
      {
      interimMemoryReference->setConstantDataSnippet(self()->getConstantDataSnippet());
      self()->setConstantDataSnippet(NULL);
      }
   self()->propagateAlignmentInfo(interimMemoryReference);
   interimMemoryReference->setSymbolReference(_symbolReference);
   _symbolReference=interinSymRef;
   generateRXInstruction(cg, TR::InstOpCode::LA, node, tempTargetRegister, interimMemoryReference);
   cg->stopUsingRegister(tempTargetRegister);

   self()->setBaseRegister(tempTargetRegister, cg);
   _baseNode = NULL;
   _indexRegister = NULL;
   _targetSnippet = NULL;
   }

bool OMR::Z::MemoryReference::ignoreNegativeOffset()
   {
   if (_offset < 0 &&
       (self()->hasTemporaryNegativeOffset() || self()->symRefHasTemporaryNegativeOffset()))
      {
      return true;
      }
   return false;
   }

TR::Register *OMR::Z::MemoryReference::swapBaseRegister(TR::Register *br, TR::CodeGenerator * cg)
   {
   br->setIsUsedInMemRef();
   self()->setBaseRegister(br, cg);
   return br;
   }

TR::Instruction *
OMR::Z::MemoryReference::handleLargeOffset(TR::Node * node, TR::MemoryReference *interimMemoryReference, TR::Register *tempTargetRegister, TR::CodeGenerator * cg, TR::Instruction * preced)
   {
   TR::InstOpCode::Mnemonic op = TR::InstOpCode::BAD;

   if (_offset > MINLONGDISP && _offset < MAXLONGDISP)
      {
      TR_ASSERT(interimMemoryReference->getOffset() == 0,"interimMemoryReference->getOffset() should be 0 and not %d\n",interimMemoryReference->getOffset());
      interimMemoryReference->setOffset(_offset);

      op=TR::InstOpCode::LAY;

      if (preced)
         preced = generateRXInstruction(cg, op, node, tempTargetRegister, interimMemoryReference, preced);
      else
         generateRXInstruction(cg, op, node, tempTargetRegister, interimMemoryReference);
      }
   else
      {
      op=TR::InstOpCode::LA;

      if (preced)
         preced = generateRXInstruction(cg, op, node, tempTargetRegister, interimMemoryReference, preced);
      else
         generateRXInstruction(cg, op, node, tempTargetRegister, interimMemoryReference);
      generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, tempTargetRegister, tempTargetRegister, (int64_t)_offset);
      }

   return preced;
   }


bool
OMR::Z::MemoryReference::typeNeedsAlignment(TR::Node *node)
   {
   return false;
   }

/**
 * This function will tell us if we should update the memref's listing field with this symref.
 */
bool
OMR::Z::MemoryReference::shouldLabelForRAS(TR::SymbolReference * symRef, TR::CodeGenerator * cg)
   {
   return true;
   }

bool
OMR::Z::MemoryReference::alignmentBumpMayRequire4KFixup(TR::Node * node, TR::CodeGenerator * cg)
   {
   // If the offset is close to MAXDISP then a later alignment bump may push it over the boundary.
   // Detect these corner cases and force an early fixup
   // e.g. _offset=4088 and the node is 10 byte in size so the max possible alignment is 10-1 = 9 giving a max possible offset of 4088+9 = 4097 (>= MAXDISP of 4096)
   //
   if (_offset < MAXDISP &&               // skip if going to fixup naturally anyway
       self()->getFixedSizeForAlignment() > 0 &&  // i.e. do not check temps -- these will have an offset of 0 anyway at instruction selection time
       (self()->rightAlignMemRef() || self()->leftAlignMemRef() || TR::MemoryReference::typeNeedsAlignment(node)))
      {
      int32_t maxAlignmentBump = self()->getFixedSizeForAlignment() - 1; // alignment is getFixedSizeForAlignment() - (length or leftMostByte) and each of these must be at least 1
      if (_offset + maxAlignmentBump >= MAXDISP)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(cg->comp(),"\tz^z : node %s (%p) : _offset %d + maxAlignmentBump %d >= MAXDISP %d : (%d >= %d) -- force 4K fixup\n",
               node?node->getOpCode().getName():"NULL",node,
               _offset,maxAlignmentBump,MAXDISP,_offset + maxAlignmentBump,MAXDISP);
         return true;
         }
      }
   return false;
   }

/**
 * \brief
 *  Enforces format limits on several instruction formats that have a displacement and base register without
 *  an index register. This routine was originally meant for SS formats and is now used on SS, VRS, VRV and VSI formats.
*/
TR::Instruction *
OMR::Z::MemoryReference::enforceSSFormatLimits(TR::Node * node, TR::CodeGenerator * cg, TR::Instruction *preced)
   {
   bool forceDueToAlignmentBump = self()->alignmentBumpMayRequire4KFixup(node, cg);
   // call separateIndexRegister first so any large offset is handled along with the index register folding
   preced = self()->separateIndexRegister(node, cg, true, preced, forceDueToAlignmentBump); // enforce4KDisplacementLimit=true
   preced = self()->enforce4KDisplacementLimit(node, cg, preced, false, forceDueToAlignmentBump);
   return preced;
   }

TR::Instruction *
OMR::Z::MemoryReference::enforceRSLFormatLimits(TR::Node * node, TR::CodeGenerator * cg, TR::Instruction *preced)
   {
   return self()->enforceSSFormatLimits(node, cg, preced);
   }

TR::Instruction *
OMR::Z::MemoryReference::enforceVRXFormatLimits(TR::Node * node, TR::CodeGenerator * cg, TR::Instruction *preced)
   {
   bool forceDueToAlignmentBump = self()->alignmentBumpMayRequire4KFixup(node, cg);
   preced = self()->enforce4KDisplacementLimit(node, cg, preced, false, forceDueToAlignmentBump);
   return preced;
   }

/**
 * This routine checks displacement is within 4K limits.  If it exceeds, use a register to hold
 * it and set the index register.
 * The forceFixup flag will skip the displacement check and always use a register. It is useful
 * for cases where the memref will be reused with an additional offset that will put it above
 * the limit, but there is a reason to do the fixup early (such as an internal control flow
 * region).
 */
TR::Instruction *
OMR::Z::MemoryReference::enforce4KDisplacementLimit(TR::Node * node, TR::CodeGenerator * cg, TR::Instruction * preced, bool forcePLXFixup, bool forceFixup)
   {
   if (self()->ignoreNegativeOffset())
      return preced;

   TR_ASSERT(!forcePLXFixup, "This logic is only used for markAndAdjustForLongDisplacementIfNeeded. You probably want the other flag.");

   if ((_offset < 0 || _offset >= MAXDISP || forcePLXFixup || forceFixup) &&
       !self()->isAdjustedForLongDisplacement())
      {
      TR::Register * tempTargetRegister = NULL;
      if (cg->comp()->target().is64Bit())
         tempTargetRegister = cg->allocateRegister();
      else
         tempTargetRegister = cg->allocateRegister();
      TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);
      if (forcePLXFixup) // avoid double counting of the TotalUseCount on the VRs
         {
         if (_baseRegister) _baseRegister->decTotalUseCount();
         if (_indexRegister) _indexRegister->decTotalUseCount();
         }
      interimMemoryReference->setBaseRegister(_baseRegister, cg);
      interimMemoryReference->setIndexRegister(_indexRegister);
      TR::SymbolReference * interinSymRef = interimMemoryReference->getSymbolReference();
      self()->propagateAlignmentInfo(interimMemoryReference);
      interimMemoryReference->setSymbolReference(_symbolReference);
      _symbolReference = interinSymRef;

      if (self()->isConstantDataSnippet()) interimMemoryReference->setConstantDataSnippet();
      if (self()->getConstantDataSnippet())
         {
         interimMemoryReference->setConstantDataSnippet(self()->getConstantDataSnippet());
         self()->setConstantDataSnippet(NULL);
         }
      preced = self()->handleLargeOffset(node, interimMemoryReference, tempTargetRegister, cg, preced);

      if (forcePLXFixup)
         {
         tempTargetRegister->incTotalUseCount();
         if (self()->getStorageReference() && interimMemoryReference->getSymbolReference()->isTempVariableSizeSymRef())
            {
            }
         }
      cg->stopUsingRegister(tempTargetRegister);

      _baseNode = NULL;
      tempTargetRegister = self()->swapBaseRegister(tempTargetRegister, cg);
      cg->stopUsingRegister(tempTargetRegister);
      _indexRegister = NULL;
      _targetSnippet = NULL;
      _offset = 0;
      self()->setAdjustedForLongDisplacement();
      }
   return preced;
   }

TR::Instruction *
OMR::Z::MemoryReference::markAndAdjustForLongDisplacementIfNeeded(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction * preced, bool *testLongDisp)
   {
   return NULL;
   }

/**
 * This routine checks displacement is within limits.  If it exceeds, use a register to hold
 * it and set the index register.
 * to eliminate the negative displacement
 * it allows 'long' displacement for RXY format instructions on Trex
 */
TR::Instruction *
OMR::Z::MemoryReference::enforceDisplacementLimit(TR::Node * node, TR::CodeGenerator * cg, TR::Instruction * preced)
   {
   if (self()->ignoreNegativeOffset())
      return preced;

   TR::Compilation *comp = cg->comp();


   if (!cg->isDispInRange(_offset))
      {
      TR_ASSERT( node,"node should be non-null for enforceDisplacementLimit\n");
      TR::Register * tempTargetRegister;
      if (cg->comp()->target().is64Bit())
         tempTargetRegister = cg->allocateRegister();
      else
         tempTargetRegister = cg->allocateRegister();

      TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);

      interimMemoryReference->setBaseRegister(_baseRegister, cg);
      interimMemoryReference->setIndexRegister(_indexRegister);
      TR::SymbolReference *interinSymRef = interimMemoryReference->getSymbolReference();
      if (self()->isConstantDataSnippet()) interimMemoryReference->setConstantDataSnippet();
      if (self()->getConstantDataSnippet())
         {
         interimMemoryReference->setConstantDataSnippet(self()->getConstantDataSnippet());
         self()->setConstantDataSnippet(NULL);
         }
      self()->propagateAlignmentInfo(interimMemoryReference);
      interimMemoryReference->setSymbolReference(_symbolReference);
      _symbolReference = interinSymRef;

      preced = self()->handleLargeOffset(node, interimMemoryReference, tempTargetRegister, cg, preced);

      cg->stopUsingRegister(tempTargetRegister);

      _baseNode = NULL;
      tempTargetRegister = self()->swapBaseRegister(tempTargetRegister, cg);
      _indexRegister = NULL;
      _targetSnippet = NULL;
      _offset = 0;

      cg->stopUsingRegister(tempTargetRegister);
      }

   return preced;
   }

/**
 * This routine is used to insert an intermediate AHI instruction before we are loading/storing
 * to eliminate the negative displacement
 */
void
OMR::Z::MemoryReference::eliminateNegativeDisplacement(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (self()->ignoreNegativeOffset())
      return;

   TR::Compilation *comp = cg->comp();


   if (_offset < 0 && !cg->isDispInRange(_offset))
      {

      TR::Register * tempTargetRegister;
      if (cg->comp()->target().is64Bit())
         tempTargetRegister = cg->allocateRegister();
      else
         tempTargetRegister = cg->allocateRegister();
      TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);

      interimMemoryReference->setBaseRegister(_baseRegister, cg);
      interimMemoryReference->setIndexRegister(_indexRegister);
      TR::SymbolReference *interinSymRef=interimMemoryReference->getSymbolReference();
      if (self()->isConstantDataSnippet()) interimMemoryReference->setConstantDataSnippet();
      if (self()->getConstantDataSnippet())
         {
         interimMemoryReference->setConstantDataSnippet(self()->getConstantDataSnippet());
         self()->setConstantDataSnippet(NULL);
         }
      self()->propagateAlignmentInfo(interimMemoryReference);
      interimMemoryReference->setSymbolReference(_symbolReference);
      _symbolReference=interinSymRef;

      self()->handleLargeOffset(node, interimMemoryReference, tempTargetRegister, cg, NULL);

      _baseNode = NULL;
      tempTargetRegister = self()->swapBaseRegister(tempTargetRegister, cg);
      _indexRegister = NULL;
      _targetSnippet = NULL;
      _offset = 0;

      cg->stopUsingRegister(tempTargetRegister);
      }
   }

/**
 * This routine is used to insert an intermediate LA instruction before we are loading/storing
 * a long type
 */
TR::Instruction *
OMR::Z::MemoryReference::separateIndexRegister(TR::Node * node, TR::CodeGenerator * cg, bool enforce4KDisplacementLimit, TR::Instruction * preced, bool forceDueToAlignmentBump)
   {
   TR::Compilation *comp = cg->comp();
   if (_indexRegister != NULL)
      {
      if (_baseRegister == NULL)
         {
         // if baseRegister happens to be NULL, we can just set that to be the old index register and be done
         _baseRegister = _indexRegister;
         _indexRegister = NULL;
         return preced;
         }
      TR::Register * tempTargetRegister = NULL;
      if (cg->comp()->target().is64Bit())
         tempTargetRegister = cg->allocateRegister();
      else
         tempTargetRegister = cg->allocateRegister();
     TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);

      interimMemoryReference->setBaseRegister(_baseRegister, cg);
      interimMemoryReference->setIndexRegister(_indexRegister);
      TR::SymbolReference *interinSymRef=interimMemoryReference->getSymbolReference();
      self()->propagateAlignmentInfo(interimMemoryReference);
      interimMemoryReference->setSymbolReference(_symbolReference);
      _symbolReference = interinSymRef;
      if (self()->isConstantDataSnippet()) interimMemoryReference->setConstantDataSnippet();
      if (self()->getConstantDataSnippet())
         {
         interimMemoryReference->setConstantDataSnippet(self()->getConstantDataSnippet());
         self()->setConstantDataSnippet(NULL);
         }
      if (!self()->ignoreNegativeOffset() &&
               enforce4KDisplacementLimit &&
               (_offset < 0 || _offset >= MAXDISP || forceDueToAlignmentBump))
         {
         preced = self()->handleLargeOffset(node, interimMemoryReference, tempTargetRegister, cg, preced);
         _offset = 0;
         }
      else
         {
         if (preced)
            preced = generateRXInstruction(cg, TR::InstOpCode::LA, node, tempTargetRegister, interimMemoryReference, preced);
         else
            generateRXInstruction(cg, TR::InstOpCode::LA, node, tempTargetRegister, interimMemoryReference);
         }

      _baseNode = NULL;
      tempTargetRegister = self()->swapBaseRegister(tempTargetRegister, cg);
      _indexRegister = NULL;
      _targetSnippet = NULL;

      cg->stopUsingRegister(tempTargetRegister);
      }
   return preced;
   }

void
OMR::Z::MemoryReference::propagateAlignmentInfo(OMR::Z::MemoryReference *newMemRef)
   {
   /* - the new memRef will be used in a LA and therefore doesn't need left or right alignment
   // when a new interim symRef is created for large/negative displacements then all information that will be used for later
   // alignment of the memory reference must be transferred to the newly created memory reference
   if (rightAlignMemRef())
      {
      TR_ASSERT( !leftAlignMemRef(),"a memory reference should not be marked as both left and right aligned\n");
      newMemRef->setRightAlignMemRef();
      }
   if (leftAlignMemRef())
      {
      TR_ASSERT( !rightAlignMemRef(),"a memory reference should not be marked as both right and left aligned\n");
      newMemRef->setLeftAlignMemRef(getLeftMostByte());
      }
   newMemRef->setTotalSizeForAlignment(getTotalSizeForAlignment());
   if (is2ndMemRef()) newMemRef->setIs2ndMemRef();
   */
   }

/**
 * 3 different kids of assignRegisters:
 *      1. TR::Instruction::assignRegisters
 *      2. TR_S390OutOfLineCodeSection::assignRegisters
 *      3. TR_S390RegisterDependencyGroup::assignRegisters
 *      4. OMR::Z::MemoryReference::assignRegisters
 *          TR::Instruction::assignRegisters calls this
 */
void
OMR::Z::MemoryReference::assignRegisters(TR::Instruction * currentInstruction, TR::CodeGenerator * cg)
   {
   TR::Machine *machine = cg->machine();
   TR::RealRegister * assignedBaseRegister;
   TR::RealRegister * assignedIndexRegister;
   TR::Compilation *comp = cg->comp();

   // If the base reg has a virt reg assignment
   if (_baseRegister != NULL && _baseRegister->getRealRegister() == NULL)
      {
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }

      // Assign a real reg to _baseRegister.  This real reg now points to the old virt
      // reg that _baseRegister was.
      // We postpone bookkeeping until after _indexRegister is assigned to avoid assigning
      // the same reg to both _indexRegister and _baseRegister

      if (_baseRegister != NULL && _baseRegister->getRealRegister() == NULL)
         {
         assignedBaseRegister = static_cast<TR::RealRegister*>(machine->assignBestRegisterSingle(_baseRegister, currentInstruction, NOBOOKKEEPING, ~TR::RealRegister::GPR0Mask));
         }
      else if (_baseRegister != NULL && _baseRegister->getRealRegister() != NULL)
         assignedBaseRegister = _baseRegister->getRealRegister();

      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      }

   // If _indexRegister is assigned a virt reg
   if (_indexRegister != NULL && _indexRegister->getRealRegister() == NULL)
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }

      // Assign a real reg to _indexRegister.  This real reg now points to the old virt
      // reg that _indexRegister was.
      assignedIndexRegister = static_cast<TR::RealRegister*>(machine->assignBestRegisterSingle(_indexRegister, currentInstruction, BOOKKEEPING, ~TR::RealRegister::GPR0Mask));

      _indexRegister = assignedIndexRegister;

      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      }

   // Do bookkeeping here after having assigned index register
   if (_baseRegister != NULL && _baseRegister->getRealRegister() == NULL)
      {
      _baseRegister->setIsLive();
      if (_baseRegister->decFutureUseCount() == 0)
         {
         _baseRegister->setAssignedRegister(NULL);
         assignedBaseRegister->setAssignedRegister(NULL);
         assignedBaseRegister->setState(TR::RealRegister::Free);
         cg->traceRegFreed(_baseRegister, assignedBaseRegister);
         }
      self()->setBaseRegister(assignedBaseRegister, cg);
      }
   }

bool
OMR::Z::MemoryReference::needsAdjustDisp(TR::Instruction * instr, OMR::Z::MemoryReference * mRef, TR::CodeGenerator * cg)
   {
   TR_ASSERT( cg->comp()->target().is64Bit(), "needsAdjustDisp() call is for 64bit code-gen only");

   TR::SymbolReference * symRef = mRef->getSymbolReference();
   TR::Compilation *comp = cg->comp();
   if (symRef->stackAllocatedArrayAccess() || (symRef == comp->getSymRefTab()->findOrCreateIndexableSizeSymbolRef()))
      {
      return false;
      }

   TR::Symbol * sym = symRef->getSymbol();
   if (sym != NULL && (sym->isArrayShadowSymbol() || (sym->getDataType() == TR::Float && sym->isParm())))
      {
      return false;
      }

   TR::InstOpCode::Mnemonic opCode = instr->getOpCodeValue();
   bool isValidOp = (opCode == TR::InstOpCode::A ||
      opCode == TR::InstOpCode::AL ||
      opCode == TR::InstOpCode::C ||
      opCode == TR::InstOpCode::CL ||
      opCode == TR::InstOpCode::L ||
      opCode == TR::InstOpCode::M ||
      opCode == TR::InstOpCode::MS ||
      opCode == TR::InstOpCode::N ||
      opCode == TR::InstOpCode::O ||
      opCode == TR::InstOpCode::S ||
      opCode == TR::InstOpCode::ST ||
      opCode == TR::InstOpCode::X ||
      opCode == TR::InstOpCode::AEB ||
      opCode == TR::InstOpCode::CEB ||
      opCode == TR::InstOpCode::DEB ||
      opCode == TR::InstOpCode::AE ||
      opCode == TR::InstOpCode::CE ||
      opCode == TR::InstOpCode::DE ||
      opCode == TR::InstOpCode::LE ||
      opCode == TR::InstOpCode::MEEB ||
      opCode == TR::InstOpCode::MAEB ||
      opCode == TR::InstOpCode::SEB ||
      opCode == TR::InstOpCode::MEE ||
      opCode == TR::InstOpCode::MAE ||
      opCode == TR::InstOpCode::SE ||
      opCode == TR::InstOpCode::STE);
   bool isValidSym =
      comp->getSymRefTab()->isNonHelper(symRef->getReferenceNumber(), TR::SymbolReferenceTable::contiguousArraySizeSymbol) ||
      comp->getSymRefTab()->isNonHelper(symRef->getReferenceNumber(), TR::SymbolReferenceTable::discontiguousArraySizeSymbol) ||
      sym == comp->getSymRefTab()->findGenericIntShadowSymbol();

   return isValidOp && isValidSym;
   }

TR::Snippet *
OMR::Z::MemoryReference::getSnippet()
   {
   TR::Snippet * snippet = NULL;

   if (self()->getUnresolvedSnippet() != NULL)
      {
      self()->setMemRefAndGetUnresolvedData(snippet);
      }
   else if (self()->getConstantDataSnippet() != NULL)
      {
      snippet = self()->getConstantDataSnippet();
      }

   return snippet;
}

int32_t
OMR::Z::MemoryReference::estimateBinaryLength(int32_t  currentEstimate, TR::CodeGenerator * cg, TR::Instruction * instr)
   {
   uint8_t length = 0;
   TR::Compilation *comp = cg->comp();

   // The pesimistic situation occurs when the DISP > 4K.
   if (instr->hasLongDisplacementSupport())
      {
      // On TRex we get an RSY inst instead of an RS inst.
      length = 6;  // Size of RXY, RSY, or SIY type inst
      }
   else
      {
      length = instr->getOpCode().getInstructionLength();
      if (cg->comp()->target().is64Bit())
         {
         // most pessimistic case has LGHI/SLLG/LA/SLLG/LA + LA
         // LGHI/SLLG/LA/SLLG/LA + LA (4+6+4+6+4+4)
         length += 28;
         if (instr->getOpCode().hasTwoMemoryReferences())
            length += 18;

         // STG/LG (6+6)
         length += 12;
         if (instr->getOpCode().hasTwoMemoryReferences())
            length += 12;
         }
      else
         {
         // LHI/SLL/LA/SLL/LA + LA (4+4+4+4+4+4)
         length += 24;
         if (instr->getOpCode().hasTwoMemoryReferences())
            length += 16;

         // ST/L (4+4)
         length += 8;
         if (instr->getOpCode().hasTwoMemoryReferences())
            length += 8;
         }
      }
   instr->setEstimatedBinaryLength(length);

   return currentEstimate + instr->getEstimatedBinaryLength();
   }

const char *
getRegisterName2(TR::Register * reg, TR::CodeGenerator * cg)
   {
   return reg? cg->getDebug()->getName(reg) : "NULL";
   }
bool
OMR::Z::MemoryReference::canUseTargetRegAsScratchReg (TR::Instruction * instr)
   {
   OMR::Z::MemoryReference * memref = instr->getMemoryReference();

   // Rare case, so will restrict to following case to minimize risk.
   // Only reuse target reg as scratch for memrefs that are flagged to not spill
   // e.g. JNI path after native call and before java SP is restored, as we can't know where
   // the longDisp slot is.

   if (!memref->isMemRefMustNotSpill())
      return false;
   switch (instr->getOpCodeValue())
      {
      case TR::InstOpCode::LG:
      case TR::InstOpCode::L:
      case TR::InstOpCode::LY:
      case TR::InstOpCode::LGF:
           {
           TR::RealRegister * base  = (memref->getBaseRegister() ? toRealRegister(memref->getBaseRegister()) : 0);
           TR::RealRegister * index = (memref->getIndexRegister() ? toRealRegister(memref->getIndexRegister()) : 0);
           TR::RealRegister * targetReg = (TR::RealRegister * )((TR::S390RegInstruction *)instr)->getRegisterOperand(1);
           if ( targetReg != base && targetReg != index && targetReg->getRegisterNumber() != TR::RealRegister::GPR0)
              return true;
           else
              return false;
           }
      default:
           return false;
      }
   }

/**
 * Called early (instruction selection time) for non-variable sized symbols to cache the size referenced for a particular memref
 */
void OMR::Z::MemoryReference::setFixedSizeForAlignment(int32_t size)
   {
   if (self()->getOriginalSymbolReference() &&
       self()->getOriginalSymbolReference()->getSymbol() &&
       self()->getOriginalSymbolReference()->getSymbol()->isVariableSizeSymbol())
      {
      TR_ASSERT(false,"variable sized symbols do not use _fixedSizeForAlignment a\n");
      _fixedSizeForAlignment = 0;
      }
   else if (self()->getSymbolReference() &&
            self()->getSymbolReference()->getSymbol() &&
            self()->getSymbolReference()->getSymbol()->isVariableSizeSymbol())
      {
      TR_ASSERT(false,"variable sized symbols do not use _fixedSizeForAlignment b\n");
      _fixedSizeForAlignment = 0;
      }
   else
      {
      TR_ASSERT(size > 0,"_fixedSizeForAlignment %d must be > 0\n",size);
      _fixedSizeForAlignment = size;
      }
   }

/**
 * Called late, during binary encoding, to get either the final max variable symbol size or
 * the earlier (instruction selection time) cached fixed size
 */
int32_t OMR::Z::MemoryReference::getTotalSizeForAlignment()
   {
   if (self()->getOriginalSymbolReference() &&
       self()->getOriginalSymbolReference()->getSymbol() &&
       self()->getOriginalSymbolReference()->getSymbol()->isVariableSizeSymbol())
      {
      return self()->getOriginalSymbolReference()->getSymbol()->getSize();
      }
   else if (self()->getSymbolReference() &&
            self()->getSymbolReference()->getSymbol() &&
            self()->getSymbolReference()->getSymbol()->isVariableSizeSymbol())
      {
      return self()->getSymbolReference()->getSymbol()->getSize();
      }
   else
      {
      TR_ASSERT(_fixedSizeForAlignment > 0,"_fixedSizeForAlignment %d not initialized for fixed size symbol\n",_fixedSizeForAlignment);
      return _fixedSizeForAlignment;
      }
   }

int32_t
OMR::Z::MemoryReference::getRightAlignmentBump(TR::Instruction * instr, TR::CodeGenerator * cg)
   {
   if (!self()->rightAlignMemRef()) return 0;

   int32_t length = -1;

   switch (instr->getKind())
      {
      case TR::Instruction::IsSS1:
         length = toS390SS1Instruction(instr)->getLen()+1;
         break;
      case TR::Instruction::IsSS2:
         if (toS390SS2Instruction(instr)->getMemoryReference() == this)
            length = toS390SS2Instruction(instr)->getLen()+1;
         else if (toS390SS2Instruction(instr)->getMemoryReference2() == this)
            length = toS390SS2Instruction(instr)->getLen2()+1;
         else
            TR_ASSERT( 0,"should be looking at the first or second memRef\n");
         break;
      case TR::Instruction::IsRSL:
         length = toS390RSLInstruction(instr)->getLen()+1;
         break;
      case TR::Instruction::IsRSLb:
         length = toS390RSLbInstruction(instr)->getLen()+1;
         break;
      case TR::Instruction::IsVSI:
         length = static_cast<TR::S390VSIInstruction*>(instr)->getImmediateField3()+1; // I3 is operand 2 length code
         break;
      default:
         break;
      }

   int32_t needsBump = true;
   if (length == -1)
      {
      TR_ASSERT(false,"only SS and bcdLoadaddr instructions should be right aligned (inst %p)\n",instr);
      needsBump = false;
      }

   int32_t bump = 0;
   if (needsBump)
      {
      TR_ASSERT( length != -1,"length should be set at this point\n");
      TR_ASSERT(self()->getTotalSizeForAlignment() > 0,"getTotalSizeForAlignment() %d not initialized in getRightAlignmentBump()\n",self()->getTotalSizeForAlignment());
      int32_t symSize = self()->getTotalSizeForAlignment();
      bump = symSize - length;
      }

   // a negative bump means that the allocated symbol size is not big enough to contain the encoded operation and this is surely wrong.
   TR_ASSERT(bump >= 0,"right alignment bump should not be negative (bump = %d)\n",bump);
   return bump;
   }

int32_t
OMR::Z::MemoryReference::getLeftAlignmentBump(TR::Instruction * instr, TR::CodeGenerator * cg)
   {
   if (!self()->leftAlignMemRef()) return 0;

   TR_ASSERT(self()->getTotalSizeForAlignment() > 0,"getTotalSizeForAlignment() %d not initialized in getLeftAlignmentBump()\n",self()->getTotalSizeForAlignment());
   TR_ASSERT( self()->getLeftMostByte() > 0,"_leftMostByte not initialized in getLeftAlignmentBump()\n");

   int32_t bump = self()->getTotalSizeForAlignment() - self()->getLeftMostByte();

   TR_ASSERT(bump >= 0,"leftAlignmentBump %d should not be negative\n",bump);
   return bump;
   }

int32_t
OMR::Z::MemoryReference::getSizeIncreaseBump(TR::Instruction * instr, TR::CodeGenerator * cg)
   {
   if ((self()->getSymbolReference() == NULL) || !self()->getSymbolReference()->isTempVariableSizeSymRef())
      return 0;
   TR_ASSERT(self()->getTotalSizeForAlignment() > 0,"getTotalSizeForAlignment() %d not initialized in getSizeIncreaseBump()\n",self()->getTotalSizeForAlignment());
   TR_ASSERT( self()->getSymbolReference()->getSymbol()->getSize() > 0,"symbol size not initialized in getSizeIncreaseBump()\n");

   int32_t bump = self()->getSymbolReference()->getSymbol()->getSize() - self()->getTotalSizeForAlignment();

   TR_ASSERT(bump >=0,"symbol size bump %d should not be negative\n",bump);
   return bump;
   }

int32_t
OMR::Z::MemoryReference::calcDisplacement(uint8_t * cursor, TR::Instruction * instr, TR::CodeGenerator * cg)
   {
   TR::Snippet * snippet = self()->getSnippet();
   TR::Symbol * symbol = self()->getSymbolReference()->getSymbol();
   int32_t disp = _offset;
   TR::Compilation *comp = cg->comp();

   if (self()->rightAlignMemRef())
      {
      TR_ASSERT( !self()->leftAlignMemRef(),"A memory reference should not be marked as both right and left aligned\n");
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"instr %p : right aligning memRef with symRef #%d (%s, isTemp %s) _offset %d, totalSize %d : bump = ",
            instr,
            self()->getSymbolReference()->getReferenceNumber(),
            cg->getDebug()->getName(self()->getSymbolReference()->getSymbol()),
            self()->getSymbolReference()->isTempVariableSizeSymRef() ? "yes":"no",
            _offset,
            self()->getTotalSizeForAlignment());
      int32_t oldDisp = disp;
      disp += self()->getRightAlignmentBump(instr, cg);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"%d (disp = %d)\n",disp-oldDisp,disp);
      }

   if (self()->leftAlignMemRef())
      {
      TR_ASSERT( !self()->rightAlignMemRef(),"A memory reference should not be marked as both left and right aligned\n");
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"instr %p : left aligning memRef with symRef #%d (%s, isTemp %s), _offset %d : bump = totalSize - leftMostByte = %d - %d = ",
            instr,
            self()->getSymbolReference()->getReferenceNumber(),
            cg->getDebug()->getName(self()->getSymbolReference()->getSymbol()),
            self()->getSymbolReference()->isTempVariableSizeSymRef() ? "yes":"no",
            _offset,
            self()->getTotalSizeForAlignment(),
            self()->getLeftMostByte());
      int32_t oldDisp = disp;
      disp += self()->getLeftAlignmentBump(instr, cg);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"%d (disp = %d)\n",disp-oldDisp,disp);
      }

   if (self()->getSymbolReference()->isTempVariableSizeSymRef())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"instr %p : bumping tempVariableSizeSymRef memRef with symRef #%d (%s), _offset %d : bump = symSize - totalSize = %d - %d = ",
            instr,
            self()->getSymbolReference()->getReferenceNumber(),
            cg->getDebug()->getName(self()->getSymbolReference()->getSymbol()),
            _offset,
            self()->getSymbolReference()->getSymbol()->getSize(),
            self()->getTotalSizeForAlignment());
      int32_t oldDisp = disp;
      disp += self()->getSizeIncreaseBump(instr, cg);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"%d (disp = %d)\n",disp-oldDisp,disp);
      }

   if (snippet != NULL)
      {
         {
         // Displacement of literals in the lit pool are all known
         // a priori.
         //
         disp = snippet->getCodeBaseOffset() + self()->getOffset();

         // We use the 12bitRelloc code to double check that all disp's
         // are indeed equal pre-bin and post-bin encoding.
         //
         if(cursor != NULL)
            {
            cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative12BitRelocation(cursor, snippet->getSnippetLabel()));
            }
         }
      }
   else if (symbol != NULL)
      {
      if (symbol->isRegisterMappedSymbol())
         {
         disp += symbol->castToRegisterMappedSymbol()->getOffset();
         if (cg->comp()->target().is64Bit() && TR::MemoryReference::needsAdjustDisp(instr, this, cg) && !self()->getDispAdjusted())
            {
            disp += 4;
            }
         self()->setDispAdjusted();
         }
      }

   return disp;
   }


static TR::Instruction *
generateImmToRegister(TR::CodeGenerator * cg, TR::Node * node, TR::Register * targetRegister, intptr_t value, TR::Instruction * cursor)
   {
   intptr_t high = (value >> 15) & 0xffff;
   intptr_t low  = (value & 0xfff);

   if (value>=(int32_t)0xFFFF8000 && value<=(int32_t)0x00007FFF)
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node,  // LHI Rscrtch,value&0xFFFF
             targetRegister, (value&0xFFFF), cursor);
      }
   else if (value>=(int32_t)0xF8000000 && value<=(int32_t)0x07FFFFFF)
      {
      high = (value >> 12) ;
      low  = (value & 0xfff);
      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node,  // LHI Rscrtch, (value>>12)&0xFFFF
                      targetRegister, (high&0xFFFF), cursor);
      if (cg->comp()->target().is64Bit())
        {
        cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG,
                   node, targetRegister, targetRegister, 12, cursor); // SLLG Rscrtch,Rscrtch,12
        }
      else
        {
        cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, // SLL Rscrtch,Rscrtch,12
                node, targetRegister, 12, cursor);
        }
      cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister,         // LA  Rscrtch, value&0xFFF(,Rscrtch)
                   generateS390MemoryReference(targetRegister, low, cg), cursor);
      }
   else
      {
      // first the top 8 bits
      cursor = generateRIInstruction(cg,
                                     TR::InstOpCode::getLoadHalfWordImmOpCode(),
                                     node,
                                     targetRegister,
                                     ((value >> 24) & 0xff),
                                     cursor);  // LHI Rscrtch,value>>24&0xFF
      // now shift that left by 12 bits
      if (cg->comp()->target().is64Bit())
         {
         cursor = generateRSInstruction(cg,
                                        TR::InstOpCode::SLLG,
                                        node,
                                        targetRegister,
                                        targetRegister,
                                        12, cursor); // SLLG Rscrtch,Rscrtch,12
         }
      else
         {
         cursor = generateRSInstruction(cg,
                                        TR::InstOpCode::SLL,
                                        node,
                                        targetRegister,
                                        12, cursor); // SLL Rscrtch,Rscrtch,12
        }
      // now include the next 12 bits (for a total of 20 bits)
      cursor = generateRXInstruction(cg,
                                     TR::InstOpCode::LA,
                                     node,
                                     targetRegister,
                                     generateS390MemoryReference(targetRegister, ((value >> 12) & 0xfff), cg),
                                     cursor);         // LA  Rscrtch, value&0xFFF(,Rscrtch)
      // now shift those 20 bits left by 12 bits
      if (cg->comp()->target().is64Bit())
         {
         cursor = generateRSInstruction(cg,
                                        TR::InstOpCode::SLLG,
                                        node,
                                        targetRegister,
                                        targetRegister,
                                        12, cursor); // SLLG Rscrtch,Rscrtch,12
         }
      else
         {
         cursor = generateRSInstruction(cg,
                                        TR::InstOpCode::SLL,
                                        node,
                                        targetRegister,
                                        12, cursor); // SLL Rscrtch,Rscrtch,12
        }
      // finally, include the bottom 12 bits for a total of 32
      cursor = generateRXInstruction(cg,
                                     TR::InstOpCode::LA,
                                     node,
                                     targetRegister,
                                     generateS390MemoryReference(targetRegister, low, cg),
                                     cursor);         // LA  Rscrtch, value&0xFFF(,Rscrtch)
      }

   return cursor;
   }


/**
 * Generate binary encoding for the first memory reference operand in an instruction
 * for RX format, the may need to insert instructions before it to
 * set up index register when D2 exceeds 12 bit range.
 *
 *   RX, VRV, VRX Format - generate D2(X2,B2)
 *    ________ ____ ____ ____ ____________ ...
 *   |Op Code | R1 | X2 | B2 |     D2     |
 *   |________|____|____|____|____________|...
 *   0         8   12   16   20          31
 *
 *  - or -
 *   RS, VRS Format (LM and STM) 0 generate D2(B2) field
 *    ________ ____ ____ ____ ____________
 *   |Op Code | R1 | R3 | B2 |     D2     |
 *   |________|____|____|____|____________|
 *   0         8   12   16   20          31
 *
 *  - or -
 *   SS Format (e.g. XC) - generate B1(D1) field
 *    ________ _______ ____ _______ ____ __________
 *   |Op Code |   L   | B1 |   D1  | B2 |   D2     |
 *   |________|_______|____|_______|____|__________|
 *   0        8       16   20      32   36         47
 *
 * @return the number of bytes of the inserted instructions
 *
 */
int32_t
OMR::Z::MemoryReference::generateBinaryEncoding(uint8_t * cursor, TR::CodeGenerator * cg, TR::Instruction * instr)
   {
   if (self()->wasCreatedDuringInstructionSelection())
      {
      self()->setOffset(cg->getFrameSizeInBytes());
      }
   uint8_t * instructionStart = cursor;
   TR::RealRegister * base  = (self()->getBaseRegister() ? toRealRegister(self()->getBaseRegister()) : 0);
   TR::RealRegister * index = (self()->getIndexRegister() ? toRealRegister(self()->getIndexRegister()) : 0);

   int32_t nbytes     = 0;
   bool largeDisp     = false;
   TR::Compilation *comp = cg->comp();
   int32_t offsetToLongDispSlot = (cg->getLinkage())->getOffsetToLongDispSlot();

   bool useNormalSP = (cg->getLinkage())->isXPLinkLinkageType() || (cg->getLinkage())->isFastLinkLinkageType() || (cg->getLinkage())->isZLinuxLinkageType();
   TR::RealRegister * scratchReg  = NULL;
   TR::RealRegister * spReg       = cg->getStackPointerRealRegister();
   TR::RealRegister * LongDispSlotBaseReg = useNormalSP ? (cg->getLinkage())->getNormalStackPointerRealRegister():spReg;

   bool spillNeeded = true;
   TR::Instruction * prevInstr = instr->getPrev();
   TR::Instruction * currInstr = prevInstr;
   TR::Node * node = instr->getNode();

   bool is2ndSSMemRef=self()->is2ndMemRef();

   int32_t displacement = self()->calcDisplacement(cursor, instr, cg);

   //add project specific relocations for specific instructions
   self()->addInstrSpecificRelocation(cg, instr, displacement, cursor);

   // We may end up "upgrading" the instruction in case of long displacement, so this variable may change
   auto instructionFormat = instr->getKind();

   // In most cases we will know at instruction selection that the displacement is large. But it is still possible for
   // us to reach binary encoding without knowing a displacement will be large. A typical example of this is when
   // referencing stack symbols whose displacement will be unknown until the stack is mapped; an operation that happens
   // after instruction selection and before binary encoding.
   if (displacement < MINDISP || displacement >= MAXDISP)
      {
      auto longDisplacementMnemonic = TR::InstOpCode::getEquivalentLongDisplacementMnemonic(instr->getOpCodeValue());

      if ((displacement < MAXLONGDISP && displacement > MINLONGDISP)
         && (longDisplacementMnemonic != TR::InstOpCode::BAD
            || instructionFormat == TR::Instruction::IsRXY
            || instructionFormat == TR::Instruction::IsRXYb
            || instructionFormat == TR::Instruction::IsRSY
            || instructionFormat == TR::Instruction::IsSIY))
         {
         // The corresponding RX/RS/SI `generateBinaryEncoding()` function will "upgrade" the instruction to the
         // corresponding long displacement version. Note that the logic here must stay synchronized with the following
         // evaluators which we add here as eyecatchers for future modifications:
         //
         // TR::S390RXInstruction::generateBinaryEncoding()
         // TR::S390RSInstruction::generateBinaryEncoding()
         // TR::S390SIInstruction::generateBinaryEncoding()

         // Nothing to do here, simply "upgrade" the instruction
         // TODO (#2848): We should set this to the actual `longDisplacementMnemonic` format once we unify instruction
         // formats and S390*Instruction formats
         instructionFormat = TR::Instruction::IsRXY;

         if (longDisplacementMnemonic != TR::InstOpCode::BAD)
            {
            TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "z/memref/long-displacement-upgrade/(%s)", comp->signature()));
            }
         }
      else
         {
         //  Find a reg not used by current inst.
         if (TR::MemoryReference::canUseTargetRegAsScratchReg(instr))
            {
            scratchReg = (TR::RealRegister * )((TR::S390RegInstruction *)instr)->getRegisterOperand(1);
            spillNeeded = false;
            }
         else
            {
            TR_ASSERT( !self()->isMemRefMustNotSpill(),"OMR::Z::MemoryReference::generateBinaryEncoding -- This memoryReference must not spill");
            if (!is2ndSSMemRef)
               {
               scratchReg  = instr->assignBestSpillRegister();
               //traceMsg(comp, "Using first spillReg %p \n", instr);
               }
            else
               {
               scratchReg  = instr->assignBestSpillRegister2();
               if (cg->comp()->target().is64Bit())
                  offsetToLongDispSlot += 8;
               else
                  offsetToLongDispSlot += 4;
               //traceMsg(comp, "Using another spillReg, increment spill slot %p \n", instr);
               }
            spillNeeded = true;
            }

         TR_ASSERT(scratchReg!=NULL,
           "OMR::Z::MemoryReference::generateBinaryEncoding -- A scratch reg should always be found.");

         largeDisp = true;
         if (spillNeeded)
            {
            currInstr = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, scratchReg,
               generateS390MemoryReference(LongDispSlotBaseReg, offsetToLongDispSlot, cg), prevInstr);
            currInstr->setEstimatedBinaryLength(currInstr->getOpCode().getInstructionLength());
            cg->setBinaryBufferCursor(cursor = currInstr->generateBinaryEncoding());
            prevInstr = currInstr;
            }
         currInstr = generateImmToRegister(cg, node, scratchReg, (intptr_t)(displacement), prevInstr);
         currInstr->setEstimatedBinaryLength(currInstr->getOpCode().getInstructionLength());
         while(prevInstr != currInstr)
            {
            prevInstr = prevInstr->getNext();
            cg->setBinaryBufferCursor(cursor = prevInstr->generateBinaryEncoding());
            }
         prevInstr = currInstr;
         displacement = 0;

         // Now we need to figure out how to use the manufactured displacement
         if (instructionFormat == TR::Instruction::IsRS ||
             instructionFormat == TR::Instruction::IsRSY ||
             instructionFormat == TR::Instruction::IsRSL ||
             instructionFormat == TR::Instruction::IsSI ||
             instructionFormat == TR::Instruction::IsSIY ||
             instructionFormat == TR::Instruction::IsSIL ||
             instructionFormat == TR::Instruction::IsSS1 ||
             instructionFormat == TR::Instruction::IsSS2 ||
             instructionFormat == TR::Instruction::IsSS4 ||
             instructionFormat == TR::Instruction::IsS   ||
             instructionFormat == TR::Instruction::IsRIS ||
             instructionFormat == TR::Instruction::IsRRS ||
             instructionFormat == TR::Instruction::IsSSE ||
             instructionFormat == TR::Instruction::IsSSF ||
             // Vector instruction formats that have 12-bit displacements and no index registers
             instructionFormat == TR::Instruction::IsVSI ||
             instructionFormat == TR::Instruction::IsVRV ||
             instructionFormat == TR::Instruction::IsVRSa||
             instructionFormat == TR::Instruction::IsVRSb||
             instructionFormat == TR::Instruction::IsVRSc||
             instructionFormat == TR::Instruction::IsVRSd)
            {
            TR_ASSERT_FATAL(base != NULL, "Expected non-NULL base register for long displacement on %s [%p] instruction", instr->getOpCode().getMnemonicName(), instr);

            //   ICM GPRt, DISP(,base)
            // becomes:
            //   LA  Rscrtch, 0(base,Rscrtch)
            //   ICM GPRt, 0(Rscrtch)
            currInstr = generateRXInstruction(cg, TR::InstOpCode::LA, node, scratchReg,
               generateS390MemoryReference(base, scratchReg, 0, cg), prevInstr);
            currInstr->setEstimatedBinaryLength(currInstr->getOpCode().getInstructionLength());
            cg->setBinaryBufferCursor(cursor = currInstr->generateBinaryEncoding());
            prevInstr = currInstr;

            base = scratchReg;
            self()->setBaseRegister(base, cg);
            }
         else if (index!=NULL && base!=NULL) // Neither is NULL, pick one
            {

            //   A  GPRt, DISP(index,base)
            // becomes:
            //   LA Rscrtch, 0(index,Rscrtch)
            //   A  GPRt, 0(Rscrtch,base)
            currInstr = generateRXInstruction(cg, TR::InstOpCode::LA, node, scratchReg,
               generateS390MemoryReference(index, scratchReg, 0, cg), prevInstr);
            currInstr->setEstimatedBinaryLength(currInstr->getOpCode().getInstructionLength());
            cg->setBinaryBufferCursor(cursor = currInstr->generateBinaryEncoding());
            prevInstr = currInstr;

            index = scratchReg;
            self()->setIndexRegister(index);
            }
         else if (index==NULL)                                      // Since index reg is free, use it directly
            {
            TR_ASSERT_FATAL(instructionFormat == TR::Instruction::IsRX ||
                     instructionFormat == TR::Instruction::IsVRX ||
                     instructionFormat == TR::Instruction::IsRXY ||
                     instructionFormat == TR::Instruction::IsRXYb ||
                     instructionFormat == TR::Instruction::IsRXE ||
                     instructionFormat == TR::Instruction::IsRXF,
                  "Unexpected instruction type for long displacement on %s [%p] instruction", instr->getOpCode().getMnemonicName(), instr);

            //    A GPRt, DISP(,base)
            // becomes:
            //    A GPRt, 0(Rscrtch,base)

            index = scratchReg;
            self()->setIndexRegister(index);
            }
         else // base is NULL                                       // Since base reg is free, use it directly
            {
            TR_ASSERT_FATAL(instructionFormat == TR::Instruction::IsRX  ||
                     instructionFormat == TR::Instruction::IsRXY ||
                     instructionFormat == TR::Instruction::IsRXYb ||
                     instructionFormat == TR::Instruction::IsRXE ||
                     instructionFormat == TR::Instruction::IsRXF,
                  "Unexpected instruction type for long displacement on %s [%p] instruction", instr->getOpCode().getMnemonicName(), instr);

            //    A GPRt, DISP(,index)
            // becomes:
            //    A GPRt, 0(index,Rscrtch)

            base = scratchReg;
            self()->setBaseRegister(base, cg);
            }

         if (nbytes == 0)
               nbytes = cursor - instructionStart;

         currInstr->setNext(instr);

         // Mark the instruction as requiring a large disp
         if (is2ndSSMemRef)
            instr->setExtDisp2();
         else
            instr->setExtDisp();

         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "z/memref/huge-displacement-upgrade/(%s)", comp->signature()));

         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "[%p] Long Disp Inst using %s as scratch reg\n", instr, cg->getDebug()->getName(scratchReg));
         }
      }

   // Update the offset now that the correct encded displacement is known
   self()->setOffset(displacement);

   if (instructionFormat == TR::Instruction::IsRXY ||
       instructionFormat == TR::Instruction::IsRXYb ||
       instructionFormat == TR::Instruction::IsRSY ||
       instructionFormat == TR::Instruction::IsSIY)
      {
      TR_ASSERT_FATAL(displacement > MINLONGDISP && displacement < MAXLONGDISP, "Displacement (%d) for RXY,RSY,SIY instruction is out of range", displacement);

      *reinterpret_cast<uint32_t*>(cursor) &= boi(0xFFFFF000);
      *reinterpret_cast<uint32_t*>(cursor) |= boi(displacement & 0x00000FFF);

      *reinterpret_cast<uint32_t*>(cursor + 4) &= boi(0x00FFFFFF);
      *reinterpret_cast<uint32_t*>(cursor + 4) |= boi((displacement & 0x000FF000) << 12);
      }
   else
      {
      TR_ASSERT_FATAL(displacement >= MINDISP && displacement < MAXDISP, "Displacement (%d) is out of range", displacement);

      cursor += is2ndSSMemRef ? 2 : 0;

      *reinterpret_cast<uint32_t*>(cursor) &= boi(0xFFFFF000);
      *reinterpret_cast<uint32_t*>(cursor) |= boi(displacement);
      }

   if (base != NULL)
      {
      base->setBaseRegisterField(reinterpret_cast<uint32_t*>(cursor));
      }

   if (index != NULL)
      {
      index->setIndexRegisterField(reinterpret_cast<uint32_t*>(cursor));
      }

   // Slot must be less than MAXDISP away from SP
   TR_ASSERT(offsetToLongDispSlot < MAXDISP, "Offset to long displacement slot (%d) is out of range", offsetToLongDispSlot);

   // We don't allow stack slot to be used for branch instructions, as gcmaps would be wrong on the gcpoint
   if (largeDisp)
      {
      TR_ASSERT(!instr->getOpCode().isBranchOp(), "We don't handle long disp for branch instruction [%p] using long displacement stack slot", instr);
      }

   //  No one else from inst selection phase using this slot
   if (!largeDisp                    &&
         self()->getCheckForLongDispSlot()     &&
         displacement == offsetToLongDispSlot  &&
         ((base  == LongDispSlotBaseReg && index == NULL) ||
         (index == LongDispSlotBaseReg && base == NULL))
      )
      {
      TR_ASSERT(false, "Bad use of long displacement slot (%d) on the stack for instruction [%p] corresponding to node [%p]", offsetToLongDispSlot, instr, instr->getNode());
      }

   return nbytes;
   }

int32_t
OMR::Z::MemoryReference::generateBinaryEncodingTouchUpForLongDisp(uint8_t *cursor, TR::CodeGenerator *cg, TR::Instruction *instr)
   {
   int32_t nbytes    = 0;
   TR::Compilation *comp = cg->comp();
   uint8_t * instructionStart = cursor;
   bool is2ndSSMemRef = self()->is2ndMemRef();
   bool largeDisp = (is2ndSSMemRef && instr->isExtDisp2()) ||
                    (!is2ndSSMemRef && instr->isExtDisp());
   int32_t offsetToLongDispSlot = (cg->getLinkage())->getOffsetToLongDispSlot();

   bool useNormalSP = (cg->getLinkage())->isXPLinkLinkageType() || (cg->getLinkage())->isFastLinkLinkageType() || (cg->getLinkage())->isZLinuxLinkageType();
   TR::RealRegister * scratchReg = NULL;
   TR::RealRegister * spReg = cg->getStackPointerRealRegister();
   TR::RealRegister * LongDispSlotBaseReg = useNormalSP ? (cg->getLinkage())->getNormalStackPointerRealRegister():spReg;

   bool spillNeeded = true; // LocalLocal alloc would set this to false

   if (TR::MemoryReference::canUseTargetRegAsScratchReg(instr))
      {
      scratchReg = (TR::RealRegister * )((TR::S390RegInstruction *)instr)->getRegisterOperand(1);
      spillNeeded = false;
      }
   else
      {
      TR_ASSERT( !self()->isMemRefMustNotSpill(),"OMR::Z::MemoryReference::generateBinaryEncodingTouchUpForLongDisp -- This memoryReference must not spill");
      if (!is2ndSSMemRef)
         {
         scratchReg = instr->getLocalLocalSpillReg1();
         if (!scratchReg)
            scratchReg = instr->assignBestSpillRegister();
         }
      else
         {
         scratchReg = instr->getLocalLocalSpillReg2();
         if (!scratchReg)
            scratchReg = instr->assignBestSpillRegister2();
         if(cg->comp()->target().is64Bit())
            offsetToLongDispSlot += 8;
         else
            offsetToLongDispSlot += 4;
         }
      spillNeeded = true;
      }

      TR_ASSERT(scratchReg!=NULL,
        "OMR::Z::MemoryReference::generateBinaryEncodingTouchUpForLongDisp -- A scratch reg should always be found.");

   // Restore the scratch register
   if (largeDisp  && spillNeeded)
      {
      TR::Instruction * nextInstr = instr->getNext();
      TR::Instruction * currInstr = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), instr->getNode(), scratchReg,
         generateS390MemoryReference(LongDispSlotBaseReg, offsetToLongDispSlot, cg), instr);
      currInstr->setEstimatedBinaryLength(currInstr->getOpCode().getInstructionLength());

      nbytes += currInstr->getOpCode().getInstructionLength();
      currInstr->setNext(nextInstr);
      }

   return nbytes;
   }
/**
 * See if the subTree should be folded into memory reference without evaluation
 */
bool
OMR::Z::MemoryReference::doEvaluate(TR::Node * subTree, TR::CodeGenerator * cg)
   {
   if (self()->forceEvaluation())
      return true;

   // do not recurse down if _this_ node has a register
   if (subTree->getRegister() != NULL)
      return true;

   if (self()->forceFolding())
      return false;

   TR::Compilation *comp = cg->comp();

   if (self()->forceFirstTimeFolding())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tforceFirstTimeFolding=true for subTree %s (%p) refCount %d, reg %s (reset firstTimeFlag for next doEvaluate)\n",
            subTree->getOpCode().getName(),subTree,subTree->getReferenceCount(),subTree->getRegister()?cg->getDebug()->getName(subTree->getRegister()):"NULL");
      self()->resetForceFirstTimeFolding();
      self()->setForceEvaluation();
      return false;
      }

   if (subTree->getOpCodeValue() != TR::loadaddr && subTree->getReferenceCount() > 1)
      return true;

   // force evaluation of the offset if it canot be added
   // to the disp
   if ((subTree->getOpCode().isArrayRef() || (subTree->getOpCodeValue() == TR::ladd ||
      subTree->getOpCodeValue() == TR::iadd)) &&
      subTree->getSecondChild()->getOpCode().isLoadConst() &&
      !cg->isDispInRange(subTree->getSecondChild()->get64bitIntegralValue()))
      return true;

   // force evaluation of the zero based ptr value that canot be added
   // to the disp
   if (subTree->getOpCodeValue() == TR::aconst && !cg->isDispInRange(subTree->get64bitIntegralValue()))
      return true;


   return false;
   }

///////////////////////////////////////////
// Generate Routines
///////////////////////////////////////////

TR::MemoryReference *
generateS390MemoryReference(TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(cg);
   }

TR::MemoryReference *
generateS390MemoryReference(int32_t iValue, TR::DataType type, TR::CodeGenerator * cg, TR::Register * treg, TR::Node *node)
   {
   TR::S390ConstantDataSnippet * targetsnippet = cg->findOrCreate4ByteConstant(node, iValue);
   return generateS390MemoryReference(targetsnippet, cg, treg, node);
   }

TR::MemoryReference *
generateS390MemoryReference(int64_t iValue, TR::DataType type, TR::CodeGenerator * cg, TR::Register * treg, TR::Node *node)
   {
   TR::S390ConstantDataSnippet * targetsnippet = cg->findOrCreate8ByteConstant(node, iValue);
   return generateS390MemoryReference(targetsnippet, cg, treg, node);
   }

TR::MemoryReference *
generateS390MemoryReference(float fValue, TR::DataType type, TR::CodeGenerator * cg, TR::Node * node)
   {
   union
      {
      float fvalue;
      int32_t ivalue;
      } fival;
   fival.fvalue = fValue;

   return generateS390MemoryReference(fival.ivalue, type, cg, 0, node);
   }

TR::MemoryReference *
generateS390MemoryReference(double dValue, TR::DataType type, TR::CodeGenerator * cg, TR::Node * node)
   {
   union
      {
      double dvalue;
      int64_t lvalue;
      } dlval;
   dlval.dvalue = dValue;

   return generateS390MemoryReference(dlval.lvalue, type, cg, 0, node);
   }
TR::MemoryReference *
generateS390MemoryReference(int32_t disp, TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(disp, cg);
   }

TR::MemoryReference *
generateS390ConstantDataSnippetMemRef(int32_t disp, TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(disp, cg, true);
   }

TR::MemoryReference *
generateS390MemoryReference(TR::Register * br, int32_t disp, TR::CodeGenerator * cg, const char *name)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(br, disp, cg, name);
   }

TR::MemoryReference *
generateS390MemoryReference(TR::Node * node, TR::CodeGenerator * cg, bool canUseRX)
   {
   // The TR::MemoryReference(TR::Node*, TR::CodeGenerator*, bool); constructor (below) is made for store or load nodes
   // that have symbol references.
   // A symbol reference is needed to adjust memory reference displacement (TR::MemoryReference::calcDisplacement).
   // If the node has no sym ref, this memory reference's gets a NULL symbol reference, which can lead to crashes in displacement
   // calculations.
   TR_ASSERT_FATAL(node->getOpCode().hasSymbolReference(), "Memory reference generation API needs a node with symbol reference\n");
   return new (cg->trHeapMemory()) TR::MemoryReference(node, cg, canUseRX);
   }

static TR::SymbolReference * findBestSymRefForArrayCopy(TR::CodeGenerator *cg, TR::Node *arrayCopyNode, TR::Node *srcNode)
   {
   TR::SymbolReference *sym = arrayCopyNode->getSymbolReference();
   TR::Compilation *comp = cg->comp();

   if (srcNode->getOpCodeValue()==TR::aiadd ||
       srcNode->getOpCodeValue()==TR::aladd ||
       srcNode->getOpCodeValue()==TR::asub)
      {
      TR::Node *firstChild = srcNode->getFirstChild();
      TR::ILOpCodes firstChildOp = firstChild->getOpCodeValue();

      if (firstChildOp==TR::loadaddr || firstChildOp==TR::aload)
         {
         sym = firstChild->getSymbolReference();
         }
      }
   else if (srcNode->getOpCodeValue()==TR::loadaddr)
      {
      sym = srcNode->getSymbolReference();
      }

   return sym;
   }

/**
 * Only call the generateS390MemoryReference with symRefForAliasing routine from within this file
 * calls from outside must go through the node based wrapper routine so the correct symRef is
 * pulled off the node
 */
static TR::MemoryReference *
generateS390MemoryReference(TR::CodeGenerator * cg, TR::SymbolReference *symRefForAliasing, TR::Node * baseNode, int32_t disp)
   {
   //TR_ASSERT(baseNode->getType().isAddress(),"baseNode must be type TR::Address and not type %d\n",baseNode->getDataType());
   TR::Node * tempLoad = TR::Node::createWithSymRef(TR::iloadi, 1, 1, baseNode, symRefForAliasing);
   // dec ref count that was inc'ed in the line above so that populatememref code is not confused with refcount >1
   baseNode->decReferenceCount();
   TR::MemoryReference * mr = generateS390MemoryReference(tempLoad, cg);
   mr->setOffset(mr->getOffset()+disp);
   return mr;
   }

/**
 * Generate memory reference from address node
 */
TR::MemoryReference *
generateS390MemoryReference(TR::CodeGenerator * cg, TR::Node *node, TR::Node * baseNode,  int32_t disp, bool forSS)
   {
   TR::Compilation *comp = cg->comp();


   TR::MemoryReference *mr = generateS390MemoryReference(cg, node->getSymbolReference(), baseNode, disp);
   TR::SymbolReference *symRefForAliasing = comp->getSymRefTab()->aliasBuilder.getSymRefForAliasing(node, baseNode);
   mr->setOriginalSymbolReference(symRefForAliasing, cg);
   mr->setIsOriginalSymRefForAliasingOnly(); // do not use this symref to derive any memref offsets but use it only for aliasing for scheduling

   if (forSS)
      mr->enforceSSFormatLimits(node, cg, NULL);
   return mr;
   }

TR::MemoryReference *
generateS390ConstantAreaMemoryReference(TR::Register *br, int32_t disp, TR::Node *node, TR::CodeGenerator *cg, bool forSS)
   {
   TR::Compilation *comp = cg->comp();
   TR::MemoryReference * mr = new (cg->trHeapMemory()) TR::MemoryReference(br, disp, comp->getSymRefTab()->findOrCreateConstantAreaSymbolReference(), cg);
   if (forSS)
      mr->enforceSSFormatLimits(node, cg, NULL);
   return mr;
   }

TR::MemoryReference *
generateS390ConstantAreaMemoryReference(TR::CodeGenerator *cg, TR::Node *addrNode, bool forSS)
   {
   TR::Compilation *comp = cg->comp();
   TR::MemoryReference *mr = generateS390MemoryReference(cg, comp->getSymRefTab()->findOrCreateConstantAreaSymbolReference(), addrNode, 0);
   if (forSS)
      mr->enforceSSFormatLimits(addrNode, cg, NULL);
   return mr;
   }

TR::MemoryReference *
generateS390MemoryReference(TR::Register * br, TR::Register * ir, int32_t disp, TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(br, ir, disp, cg);
   }

TR::MemoryReference *
generateS390MemoryReference(TR::MemoryReference & mr, int32_t n, TR::CodeGenerator * cg)
   {
      return new (cg->trHeapMemory()) TR::MemoryReference(mr, n, cg);
   }

TR::MemoryReference *
reuseS390MemoryReference(TR::MemoryReference *baseMR, int32_t offset, TR::Node *node, TR::CodeGenerator *cg, bool enforceSSLimits)
   {
   //if we are reusing memory reference and offset is not 0, it will generate new memory reference based on current offset
   //and return New MR. This will prevent original Memory reference being modified even if it hasn't been used before.
   TR_ASSERT(baseMR,"baseMR must be non-NULL for node %p\n",node);
   if (baseMR->getMemRefUsedBefore())
      baseMR = new (cg->trHeapMemory()) TR::MemoryReference(*baseMR, offset, cg);
   else
      if (offset != 0)
         baseMR = new (cg->trHeapMemory()) TR::MemoryReference(*baseMR, offset, cg);
   if (enforceSSLimits)
      baseMR->enforceSSFormatLimits(node, cg, NULL);
   return baseMR;
   }

TR::MemoryReference *
generateS390MemoryReference(TR::Node * node, TR::SymbolReference * sr, TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(node, sr, cg);
   }

TR::MemoryReference *
generateS390MemoryReference(TR::Snippet * tas, TR::CodeGenerator * cg, TR::Register * base, TR::Node * node)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(tas, cg, base, node);
   }

TR::MemoryReference *
generateS390MemoryReference(TR::Snippet * tas, TR::Register * indx, int32_t disp, TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(tas, indx, disp, cg);
   }

static TR::MemoryReference *
generateS390AlignedMemoryReference(TR::MemoryReference& baseMR, TR::Node *node, int32_t offset, TR::CodeGenerator *cg, bool enforceSSLimits)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   TR::MemoryReference *memRef = generateS390MemoryReference(baseMR, 0, cg);
   // Set the TemporaryNegativeOffset flag to prevent the LA/AHI or LAY expansion for negative offsets that will
   // ultimately be cancelled out after applying the right alignment bump
   if (offset < 0)
      memRef->addToTemporaryNegativeOffset(node, offset, cg);
   else
      memRef->addToOffset(offset);

   if (baseMR.hasTemporaryNegativeOffset())
      memRef->setHasTemporaryNegativeOffset();

   if (enforceSSLimits)
      memRef->enforceSSFormatLimits(node, cg, NULL);
   return memRef;
   }

TR::MemoryReference *
generateS390RightAlignedMemoryReference(TR::MemoryReference& baseMR, TR::Node *node, int32_t offset, TR::CodeGenerator *cg, bool enforceSSLimits)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   TR::MemoryReference *memRef = generateS390AlignedMemoryReference(baseMR, node, offset, cg, enforceSSLimits);
   memRef->setRightAlignMemRef();
   return memRef;
   }


TR::MemoryReference *
generateS390LeftAlignedMemoryReference(TR::MemoryReference& baseMR, TR::Node *node, int32_t offset, TR::CodeGenerator *cg, int32_t leftMostByte, bool enforceSSLimits)
   {
   TR_ASSERT(!node->getType().isAggregate(),"do not use aligned memrefs for aggrs on node %p\n",node);
   TR::MemoryReference *memRef = generateS390AlignedMemoryReference(baseMR, node, offset, cg, enforceSSLimits);
   memRef->setLeftAlignMemRef(leftMostByte);
   return memRef;
   }

TR::MemoryReference *
generateS390AddressConstantMemoryReference(TR::CodeGenerator *cg, TR::Node *node)
   {
   return NULL;
   }
