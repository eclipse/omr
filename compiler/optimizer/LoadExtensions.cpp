/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "optimizer/LoadExtensions.hpp"

#include <stdint.h>                           // for uint16_t
#include <string.h>                           // for NULL, memset
#include "codegen/CodeGenerator.hpp"          // for CodeGenerator
#include "compile/Compilation.hpp"            // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"        // for TR::Options, etc
#include "cs2/bitvectr.h"                     // for ABitVector<>::Cursor
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "il/DataTypes.hpp"                   // for TR::DataType, etc
#include "il/ILOpCodes.hpp"                   // for ILOpCodes::a2l, etc
#include "il/ILOps.hpp"                       // for TR::ILOpCode, ILOpCode
#include "il/Node.hpp"                        // for Node, etc
#include "il/Node_inlines.hpp"                // for Node::getUseDefIndex, etc
#include "il/Symbol.hpp"                      // for Symbol
#include "il/TreeTop.hpp"                     // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"                   // for TR_ASSERT
#include "infra/BitVector.hpp"                // for TR_BitVector, etc
#include "infra/Cfg.hpp"                      // for CFG
#include "infra/SimpleRegex.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/Optimization_inlines.hpp" // for Optimization inlines
#include "optimizer/Optimizer.hpp"            // for Optimizer
#include "optimizer/UseDefInfo.hpp"           // for TR_UseDefInfo, etc
#include "ras/Debug.hpp"                      // for TR_Debug

TR_LoadExtensions::TR_LoadExtensions(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     excludedNodes(NULL), loadExtensionPreference(NULL)
   {
   setTrace(comp()->getOptions()->getOptsToTrace() != NULL && TR::SimpleRegex::match(comp()->getOptions()->getOptsToTrace(), "traceLoadExtensions"));

   cg()->getExtendedToInt64GlobalRegisters().Clear();
   }

int32_t TR_LoadExtensions::perform()
   {
   if (comp()->getOptLevel() >= warm && !optimizer()->cantBuildGlobalsUseDefInfo())
      {
      if (!comp()->getFlowGraph()->getStructure())
         {
         optimizer()->doStructuralAnalysis();
         }

      TR::LexicalMemProfiler memoryProfiler("Load Extensions: Usedef calculation", comp()->phaseMemProfiler());

      optimizer()->setUseDefInfo(NULL);

      TR_UseDefInfo* useDefInfo = new (comp()->allocator()) TR_UseDefInfo(comp(), comp()->getFlowGraph(), optimizer(), false, false, false, true, true);

      if (useDefInfo->infoIsValid())
         {
         optimizer()->setUseDefInfo(useDefInfo);
         }
      else
         {
         delete useDefInfo;
         }
      }

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   excludedNodes = new (stackMemoryRegion) NodeToIntTable(NodeToIntTableComparator(), NodeToIntTableAllocator(stackMemoryRegion));
   loadExtensionPreference = new (stackMemoryRegion) NodeToIntTable(NodeToIntTableComparator(), NodeToIntTableAllocator(stackMemoryRegion));

   for (TR::PreorderNodeIterator iter(comp()->getStartTree(), comp()); iter.currentTree() != NULL; ++iter)
      {
      findPreferredLoadExtensions(iter.currentNode());
      }

   for (TR::PreorderNodeIterator iter(comp()->getStartTree(), comp()); iter.currentTree() != NULL; ++iter)
      {
      flagPreferredLoadExtensions(iter.currentNode());
      }

   return 0;
   }

const bool TR_LoadExtensions::canSkipConversion(TR::Node* conversion, TR::Node* child, bool& forceExtension)
   {
   bool result = false;

   // Assume we are not forcing the load (if the child is really a load) to be zero/sign extended
   forceExtension = false;

   if (trace())
      {
      traceMsg(comp(), "\t\tExamining conversion %s [%p]\n", 
         conversion->getOpCode().getName(), 
         conversion);
      }

   if (isSupportedType(child) && excludedNodes->count(child) == 0)
      {
      const int32_t preference = getExtensionPreference(child);

      const bool loadPrefersSignExtension = preference > 0;
      const bool loadPrefersZeroExtension = preference < 0;

      TR::ILOpCode& conversionOpCode = conversion->getOpCode();

      if (isSupportedLoad(child) &&

         // Only consider widening conversions
         conversion->getSize() > child->getSize() &&

         // Ensure we do not use register pairs for 64-bit loads on 32-bit platforms
         (TR::Compiler->target.is64Bit() || comp()->cg()->use64BitRegsOn32Bit() || conversion->getSize() != 8) &&

         // Ensure the conversion matches our preferred extension on the load
         (loadPrefersSignExtension && loadPrefersSignExtension == conversionOpCode.isSignExtension() ||
          loadPrefersZeroExtension && loadPrefersZeroExtension == conversion->isZeroExtension()))
         {
         if (trace())
            {
            traceMsg(comp(), "\t\tDetected sign extension pattern on widening conversion %s [%p] and load %s [%p]\n", 
               conversion->getOpCode().getName(), 
               conversion, 
               child->getOpCode().getName(), 
               child);
            }

         forceExtension = true;
         result = true;
         }

      if (conversion->getSize() < child->getSize())
         {
         // TODO (Issue #2213): Determine whether this case is ever needed and why? Shouldn't the simplifier have eliminated such IL?
         if (child->getOpCode().isConversion())
            {
            TR::Node* grandChild = child->getFirstChild();

            if (isSupportedLoad(grandChild) &&

               // Conversion is narrowing down to the original width (i.e. stacked conversion which is a NOP)
               conversion->getSize() == grandChild->getSize())
               {
               if (trace())
                  {
                  traceMsg(comp(), "\t\tDetected sign extension pattern on narrowing conversion %s [%p] and load %s [%p]\n", 
                     conversion->getOpCode().getName(), 
                     conversion, 
                     child->getOpCode().getName(), 
                     child);
                  }

               result = true;
               }
            }
         }
      }

   return result;
   }

const bool TR_LoadExtensions::isSupportedType(TR::Node* node) const
   {
   bool result = node->getType().isIntegral() || node->getType().isAddress();

   // Disallow static integral loads of size smaller than an int
   if (!TR::comp()->cg()->getAccessStaticsIndirectly() &&
      node->getOpCode().isLoadDirect() &&
      node->getOpCode().hasSymbolReference() && node->getSymbol()->isStatic() &&
      !node->getOpCode().isInt() && !node->getOpCode().isLong())
      {
      result = false;
      }

   return result;
   }

const bool TR_LoadExtensions::isSupportedLoad(TR::Node* node) const
   {
   bool result = false;

   // We don't need to handle constant loads because simplifier would have taken care of these already
   if (node->getOpCode().isLoadVar())
      {
      result = true;
      }

   return result;
   }

void TR_LoadExtensions::findPreferredLoadExtensions(TR::Node* parent)
   {
   TR::ILOpCode& parentOpCode = parent->getOpCode();

   // count how a load is being used. As a signed or unsigned number?
   if (isSupportedType(parent) && parentOpCode.isConversion())
      {
      TR::Node* child = parent->getFirstChild();

      // Only examine non-trivial conversions
      if (isSupportedType(child) && parent->getSize() != child->getSize())
         {
         if (isSupportedLoad(child))
            {
            setExtensionPreference(child, parent);
            }
         else if (child->getOpCode().isLoadReg())
            {
            TR::Node* useRegLoad = child;

            TR_UseDefInfo* useDefInfo = optimizer()->getUseDefInfo();

            // If we have usedef info we can traverse all defs of this particular use and if all the defs are stores
            // of supported counted loads then we can count such loads as well. If this criteria is not met then there
            // exists at least one def (store) of this particular use which feeds from a non-load operation (an
            // addition for example). These are not candidates for skipping extension because we cannot easily extend
            // a non-load operation.
            if (useDefInfo != NULL && useDefInfo->infoIsValid() && useRegLoad->getUseDefIndex() != 0 && useDefInfo->isUseIndex(useRegLoad->getUseDefIndex() != 0))
               {
               TR_UseDefInfo::BitVector info(comp()->allocator());
               if (useDefInfo->getUseDef(info, useRegLoad->getUseDefIndex()))
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "\t\tPeeking through RegLoad %p for conversion %s [%p]\n", 
                        useRegLoad, 
                        parentOpCode.getName(), 
                        parent);
                     }

                  TR_UseDefInfo::BitVector::Cursor cursor(info);

                  int32_t firstDefIndex = useDefInfo->getFirstRealDefIndex();
                  int32_t firstUseIndex = useDefInfo->getFirstUseIndex();

                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t defIndex = cursor;

                     // We've examined all the defs of this particular use
                     if (defIndex >= firstUseIndex)
                        {
                        break;
                        }

                     // Do not consider defs that correspond to method arguments as we cannot force extension on those
                     if (defIndex < firstDefIndex)
                        {
                        (*excludedNodes)[parent] = true;
                        break;
                        }

                     TR::Node* defRegLoad = useDefInfo->getNode(defIndex);

                     if (defRegLoad != NULL)
                        {
                        TR::Node* defRegLoadChild = defRegLoad->getFirstChild();

                        if (defRegLoad->getOpCode().isStoreReg() && isSupportedType(defRegLoadChild) && isSupportedLoad(defRegLoadChild))
                           {
                           if (trace())
                              {
                              traceMsg(comp(), "\t\tPeeked through use %s [%p] and found def %s [%p] with child %s [%p] - Counting [%p]\n",
                                 useRegLoad->getOpCode().getName(), 
                                 useRegLoad,
                                 defRegLoad->getOpCode().getName(), 
                                 defRegLoad,
                                 defRegLoadChild->getOpCode().getName(), 
                                 defRegLoadChild, 
                                 defRegLoadChild);
                              }

                           setExtensionPreference(defRegLoadChild, parent);
                           }
                        else
                           {
                           if (trace())
                              {
                              traceMsg(comp(), "\t\tPeeked through use %s [%p] and found def %s [%p] with child %s [%p] - Excluding [%p]\n",
                                 useRegLoad->getOpCode().getName(), 
                                 useRegLoad,
                                 defRegLoad->getOpCode().getName(), 
                                 defRegLoad,
                                 defRegLoadChild != NULL ? 
                                    defRegLoadChild->getOpCode().getName() : 
                                    "NULL", 
                                 defRegLoadChild, 
                                 parent);
                              }

                           (*excludedNodes)[parent] = true;
                           }
                        }
                     }
                  }
               }
            else
               {
               (*excludedNodes)[parent] = true;
               }
            }
         }
      }


   // Exclude all loads which feed into global register stores which require sign extensions. This must be done 
   // because Load Extensions is a local optimization and it must respect global sign extension decisions made
   // by GRA. Excluding such loads prevents a situation where GRA decided that a particular global register
   // should be sign extended at its definitions however Load Extensions has determined that the same load
   // should be zero extended. If local RA were to pick the same register for the global register as well as
   // the load then we have a conflicting decision which will result in a conversion to be skipped when it is
   // not supposed to be.
   if (parentOpCode.isStoreReg() && parent->needsSignExtension() && parent->getFirstChild()->getOpCode().isLoadVar())
      {
      (*excludedNodes)[parent->getFirstChild()] = true;
      }
   }

void TR_LoadExtensions::flagPreferredLoadExtensions(TR::Node* parent)
   {
   if (isSupportedType(parent) && parent->getOpCode().isConversion())
      {
      TR::Node* child = parent->getFirstChild();

      bool canSkipConversion = false;

      if (isSupportedType(child))
         {
         if (parent->getSize() == child->getSize())
            {
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "codegen/LoadExtensions/success/unneededConversion/%s", comp()->signature()));

            parent->setUnneededConversion(true);
            }
         else
            {
            TR::ILOpCode& childOpCode = child->getOpCode();

            if (childOpCode.isLoadReg()
               && !(parent->getSize() > 4 && TR::Compiler->target.is32Bit())
               && excludedNodes->count(parent) == 0)
               {
               TR::Node* useRegLoad = child;

               TR_UseDefInfo* useDefInfo = optimizer()->getUseDefInfo();

               if (useDefInfo != NULL && useDefInfo->infoIsValid() && useRegLoad->getUseDefIndex() != 0 && useDefInfo->isUseIndex(useRegLoad->getUseDefIndex() != 0))
                  {
                  TR_UseDefInfo::BitVector info(comp()->allocator());

                  if (useDefInfo->getUseDef(info, useRegLoad->getUseDefIndex()))
                     {
                     TR_UseDefInfo::BitVector::Cursor cursor(info);

                     int32_t firstDefIndex = useDefInfo->getFirstRealDefIndex();
                     int32_t firstUseIndex = useDefInfo->getFirstUseIndex();

                     canSkipConversion = true;

                     bool forceExtensionOnAnyLoads = false;
                     bool forceExtensionOnAllLoads = true;

                     for (cursor.SetToFirstOne(); cursor.Valid() && canSkipConversion; cursor.SetToNextOne())
                        {
                        int32_t defIndex = cursor;

                        // We've examined all the defs of this particular use
                        if (defIndex >= firstUseIndex)
                           {
                           break;
                           }

                        // Do not consider defs that correspond to method arguments as we cannot force extension on those
                        if (defIndex < firstDefIndex)
                           {
                           continue;
                           }

                        TR::Node* defRegLoad = useDefInfo->getNode(defIndex);

                        if (defRegLoad != NULL)
                           {
                           TR::Node* defRegLoadChild = defRegLoad->getFirstChild();

                           bool forceExtension = false;
                           canSkipConversion = TR_LoadExtensions::canSkipConversion(parent, defRegLoadChild, forceExtension);

                           forceExtensionOnAnyLoads |= forceExtension;
                           forceExtensionOnAllLoads &= forceExtension;

                           // If we have to force extension on any loads which feed a def of this use ensure we must also 
                           // force extension on all such loads. Conversely the conversion can be skipped if none of the
                           // loads feeding the def of this use need to be extended. This ensures either all loads feeding
                           // into defs of this use should be extended or none of them.
                           canSkipConversion &= forceExtensionOnAllLoads == forceExtensionOnAnyLoads;

                           if (trace())
                              {
                              traceMsg(comp(), "\t\tPeeked through %s [%p] and found %s [%p] with child %s [%p] - conversion %s be skipped\n",
                                 useRegLoad->getOpCode().getName(), 
                                 useRegLoad,
                                 defRegLoad->getOpCode().getName(), 
                                 defRegLoad,
                                 defRegLoadChild->getOpCode().getName(), 
                                 defRegLoadChild,
                                 canSkipConversion ? 
                                    "can" :
                                    "cannot");
                              }
                           }
                        }

                     if (canSkipConversion && performTransformation(comp(), "%sSkipping conversion %s [%p] after RegLoad\n", optDetailString(), parent->getOpCode().getName(), parent))
                        {
                        TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "codegen/LoadExtensions/success/unneededConversion/GRA/%s", comp()->signature()));

                        parent->setUnneededConversion(true);

                        if (forceExtensionOnAllLoads)
                           {
                           TR_UseDefInfo::BitVector info(comp()->allocator());

                           if (useDefInfo->getUseDef(info, useRegLoad->getUseDefIndex()))
                              {
                              TR_UseDefInfo::BitVector::Cursor cursor(info);

                              for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                                 {
                                 int32_t defIndex = cursor;

                                 // We've examined all the defs of this particular use
                                 if (defIndex >= firstUseIndex)
                                    {
                                    break;
                                    }

                                 // Do not consider defs that correspond to method arguments as we cannot force extension on those
                                 if (defIndex < firstDefIndex)
                                    {
                                    continue;
                                    }

                                 TR::Node *defRegLoad = useDefInfo->getNode(defIndex);

                                 if (defRegLoad != NULL)
                                    {
                                    TR::Node* defRegLoadChild = defRegLoad->getFirstChild();

                                    const int32_t preference = getExtensionPreference(defRegLoadChild);

                                    if (preference > 0)
                                       {
                                       if (trace())
                                          {
                                          traceMsg(comp(), "\t\t\tForcing sign extension on %s [%p]\n",
                                             defRegLoadChild->getOpCode().getName(),
                                             defRegLoadChild);
                                          }

                                       if (parent->getSize() == 8 || parent->useSignExtensionMode())
                                          {
                                          defRegLoadChild->setSignExtendTo64BitAtSource(true);
                                          }
                                       else
                                          {
                                          defRegLoadChild->setSignExtendTo32BitAtSource(true);
                                          }
                                       }

                                    if (preference < 0)
                                       {
                                       if (trace())
                                          {
                                          traceMsg(comp(), "\t\t\tForcing zero extension on %s [%p]\n",
                                             defRegLoadChild->getOpCode().getName(),
                                             defRegLoadChild);
                                          }

                                       if (parent->getSize() == 8 || parent->useSignExtensionMode())
                                          {
                                          defRegLoadChild->setZeroExtendTo64BitAtSource(true);
                                          }
                                       else
                                          {
                                          defRegLoadChild->setZeroExtendTo32BitAtSource(true);
                                          }
                                       }
                                    }
                                 }
                              }
                           }

                        if (parent->getType().isInt64() && parent->getSize() > child->getSize())
                           {
                           if (trace())
                              {
                              traceMsg(comp(), "\t\t\tSet global register %s in getExtendedToInt64GlobalRegisters for child %s [%p] with parent node %s [%p]\n",
                                 comp()->getDebug()->getGlobalRegisterName(child->getGlobalRegisterNumber()),
                                 child->getOpCode().getName(),
                                 child,
                                 parent->getOpCode().getName(),
                                 parent);
                              }

                           // getExtendedToInt64GlobalRegisters is used by the evaluators to force a larger virtual register to be used when
                           // evaluating the regload so any instructions generated by local RA are the correct size to preserve the upper bits
                           cg()->getExtendedToInt64GlobalRegisters()[child->getGlobalRegisterNumber()] = true;
                           }
                        }
                     }
                  }
               }
            }
         }

      if (!canSkipConversion)
         {
         bool forceExtension = false;
         canSkipConversion = TR_LoadExtensions::canSkipConversion(parent, child, forceExtension);

         if (canSkipConversion && performTransformation(comp(), "%sSkipping conversion %s [%p]\n", optDetailString(), parent->getOpCode().getName(), parent))
            {
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "codegen/LoadExtensions/success/unneededConversion/%s", comp()->signature()));

            parent->setUnneededConversion(true);

            if (forceExtension)
               {
               const int32_t preference = getExtensionPreference(child);

               if (preference > 0)
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "\t\t\tForcing sign extension on %s [%p]\n",
                        child->getOpCode().getName(),
                        child);
                     }

                  if (parent->getSize() == 8 || parent->useSignExtensionMode())
                     {
                     child->setSignExtendTo64BitAtSource(true);
                     }
                  else
                     {
                     child->setSignExtendTo32BitAtSource(true);
                     }
                  }

               if (preference < 0)
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "\t\t\tForcing zero extension on %s [%p]\n",
                        child->getOpCode().getName(),
                        child);
                     }

                  if (parent->getSize() == 8 || parent->useSignExtensionMode())
                     {
                     child->setZeroExtendTo64BitAtSource(true);
                     }
                  else
                     {
                     child->setZeroExtendTo32BitAtSource(true);
                     }
                  }
               }
            }
         }
      }
   }

const int32_t TR_LoadExtensions::getExtensionPreference(TR::Node* load) const
   {
   int32_t result;

   if (load->getType().isAddress())
      {
      // Addresses are always unsigned, and thus their preference as well
      result = -1;
      }
   else
      {
      result = (*loadExtensionPreference)[load];
      }

   return result;
   }

const int32_t TR_LoadExtensions::setExtensionPreference(TR::Node* load, TR::Node* conversion)
   {
   int32_t result;

   if (conversion->isZeroExtension() || conversion->getOpCode().isUnsigned())
      {
      if (trace())
         {
         traceMsg(comp(), "\t\tCounting unsigned load %s [%p] under %s [%p]\n", 
            load->getOpCode().getName(), 
            load, 
            conversion->getOpCode().getName(), 
            conversion);
         }

      // i.e. TR::bu2i || TR::iu2l
      result = --(*loadExtensionPreference)[load];
      }
   else
      {
      if (trace())
         {
         traceMsg(comp(), "\t\tCounting signed load %s [%p] under %s [%p]\n", 
            load->getOpCode().getName(), 
            load, 
            conversion->getOpCode().getName(), 
            conversion);
         }

      // i.e. TR::i2l || TR::b2i
      result = ++(*loadExtensionPreference)[load];
      }

   return result;
   }
