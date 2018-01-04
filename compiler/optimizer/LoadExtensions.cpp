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
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/Optimization_inlines.hpp" // for Optimization inlines
#include "optimizer/Optimizer.hpp"            // for Optimizer
#include "optimizer/UseDefInfo.hpp"           // for TR_UseDefInfo, etc
#include "ras/Debug.hpp"                      // for TR_Debug

#define OPT_DETAILS "O^O LOAD EXTENSIONS: "

TR_LoadExtensions::TR_LoadExtensions(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _counts(0), _seenLoads(0), _useDefInfo(NULL), _excludedNodes(comp()->allocator())
   {
   setTrace(comp()->getOptions()->getOptsToTrace() && TR::SimpleRegex::match(comp()->getOptions()->getOptsToTrace(), "traceLoadExtensions"));
   vcount_t visitCount = comp()->incVisitCount();
   ncount_t nodeCount = 0;
   TR::Node * node;

   // Unfortunately, no globalIndex in the codegen, so need our own index...
   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();
      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in marking load nodes");
      nodeCount = indexNodesForCodegen(node, nodeCount, visitCount);
      }

   _signExtensionFlags = new (trHeapMemory()) TR_BitVector(nodeCount*TR::Node::numSignExtensionFlags, trMemory(), heapAlloc, growable);

   TR::SparseBitVector extendedToInt64GlobalRegisters = cg()->getExtendedToInt64GlobalRegisters();
   extendedToInt64GlobalRegisters.Clear();

   _excludedNodes.Clear();
   }

//#define REUSE_USEDEFS
//#define DISABLE_REGLOADS

int32_t TR_LoadExtensions::perform()
   {


   if (
#ifdef DISABLE_REGLOADS
       0 &&
#endif
       (comp()->getOptLevel() > noOpt) &&
       _seenLoads & 0x2 && optimizer()
       && !optimizer()->cantBuildGlobalsUseDefInfo()
#ifdef REUSE_USEDEFS
        && (NULL == (_useDefInfo = optimizer()->getUseDefInfo()) ||
            !_useDefInfo->hasGlobalsUseDefs() ||
            !_useDefInfo->_useDefForRegs)
#endif
      )
      {
      if (!comp()->getFlowGraph()->getStructure())
         optimizer()->doStructuralAnalysis();

      TR::LexicalMemProfiler mp("use defs (Load Extensions)", comp()->phaseMemProfiler());

      optimizer()->setUseDefInfo(NULL);

      _useDefInfo = new (comp()->allocator()) TR_UseDefInfo(comp(), comp()->getFlowGraph(), optimizer(), false, false, false, true, true);

      if (_useDefInfo->infoIsValid())
         optimizer()->setUseDefInfo(_useDefInfo);
      else
         {
         // release storage for failed _useDefInfo
         delete _useDefInfo;
         _useDefInfo = NULL;
         }
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _counts = (int32_t *)trMemory()->allocateStackMemory(comp()->getNodeCount()*sizeof(int32_t), TR_Memory::CodeGenerator);
   memset(_counts, 0, comp()->getNodeCount()*sizeof(int32_t));

   vcount_t visitCount = comp()->incVisitCount();

   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in marking load nodes");
      countLoadExtensions(node, visitCount);
      }

   visitCount = comp()->incVisitCount();

   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in marking load nodes");
      setPreferredExtension(node, visitCount);
      }

   } // scope for stack memory region

   return 0;
   }

void TR_LoadExtensions::countLoad(TR::Node* load, TR::Node* conversion)
   {
   TR::ILOpCode& opcode = conversion->getOpCode();

   if (opcode.isSignExtension()) // i.e. TR::b2i
      {
      if (trace()) traceMsg(comp(), "\t\tSigned %p %p\n", conversion, load);
      _counts[load->getGlobalIndex()]+=totalFlags;
      }
   else if (conversion->isZeroExtension() || opcode.isUnsigned()) // i.e. TR::bu2i || TR::buadd
      {
      if (trace()) traceMsg(comp(), "\t\tUnsigned %p %p\n", conversion, load);
      _counts[load->getGlobalIndex()]-=totalFlags;
      }
   else // i.e. TR::isub
      {
      if (trace()) traceMsg(comp(), "\t\tSignedd %p %p\n", conversion, load);
      //Do not pennalize loads for opcodes that do not care about sign
      _counts[load->getGlobalIndex()]+=totalFlags;
      }
   }

bool TR_LoadExtensions::supportedConstLoad(TR::Node *load, TR::Compilation * c)
   {
   TR_ASSERT(load->getOpCode().isLoadConst(), "Only meant to be called by const loads.");

   //this line can be removed as part of implemention task 63142
   return false;

   if (load->getSize() > 4 || !load->getType().isIntegral())
      return false;
#define MAX_IMMEDIATE_VAL            32767         // 0x7fff
#define MIN_IMMEDIATE_VAL           -32768         // 0x8000

   int32_t value = MAX_IMMEDIATE_VAL+1;
   switch(load->getOpCodeValue())
      {
      case TR::buconst:    value = load->getByte() & 0xFF; break;
      case TR::bconst:     value = load->getByte(); break;
      case TR::cconst:     value = load->getConst<uint16_t>(); break;
      case TR::sconst:     value = load->getShortInt(); break;
      case TR::iconst:     value = load->getInt(); break;
      default: break;
      }
   if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
      return true;
   return false;
   }

void TR_LoadExtensions::countLoadExtensions(TR::Node *parent, vcount_t visitCount)
   {
   if (parent->getVisitCount() == visitCount)
      return;

   parent->setVisitCount(visitCount);
   TR::ILOpCode& opcode = parent->getOpCode();

   // count how a load is being used. As a signed or unsigned number?
   if (opcode.isConversion() && supportedType(parent))
      {
      TR::Node *load = parent->getFirstChild();
      if ((load->getOpCode().isLoadVar() || load->getOpCode().isLoadReg() || load->getOpCode().isLoadConst()) &&
          supportedType(load) &&
          parent->getSize() != load->getSize())
         {
         if (load->getOpCode().isLoadReg())
            {
            if (optimizer() && _useDefInfo && _useDefInfo->infoIsValid() &&
                load->getUseDefIndex() && _useDefInfo->isUseIndex(load->getUseDefIndex()) &&
                (parent->getSize() < 8)) // On Java disable i2l, because of interferance with HRA
               {
               TR_UseDefInfo::BitVector info(comp()->allocator());
               _useDefInfo->getUseDef(info, load->getUseDefIndex());
               if (!info.IsZero())
                  {
                  if (trace()) traceMsg(comp(), "\t\tPeeking through RegLoad %p for Conversion %p (%s)\n", load, parent, parent->getOpCode().getName());
                  TR_UseDefInfo::BitVector::Cursor cursor(info);
                  int32_t firstDefIndex = _useDefInfo->getFirstRealDefIndex();
                  int32_t firstUseIndex = _useDefInfo->getFirstUseIndex();
                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t defIndex = cursor;
                     if (defIndex >= firstUseIndex)
                        break;

                     // Definition is the method parameter (entry)
                     if (defIndex < firstDefIndex)
                        {
                        setOverrideOpt(parent, regLoadOverride);
                        break;
                        }

                     TR::Node *defNode = _useDefInfo->getNode(defIndex);
                     if (!defNode)
                        continue;

                     TR::Node *realLoad = NULL;
                     if (defNode->getOpCode().isStoreReg() &&
                           NULL!= (realLoad = defNode->getFirstChild()) &&
                           (realLoad->getOpCode().isLoadVar() || (realLoad->getOpCode().isLoadConst() && supportedConstLoad(realLoad, comp()))) &&
                           supportedType(realLoad))
                        {
                        if (trace()) traceMsg(comp(), "\t\tPeeked through %p (%s) and found %p (%s) with child %p (%s), Counting.\n",
                                          load, load->getOpCode().getName(),
                                          defNode, defNode->getOpCode().getName(),
                                          realLoad, realLoad->getOpCode().getName());
                        countLoad(realLoad, parent);
                        }
                     else
                        {
                        if (trace()) traceMsg(comp(), "\t\tPeeked through %p (%s) and found %p (%s) with child %p (%s), setting Ignore.\n",
                                          load, load->getOpCode().getName(),
                                          defNode, defNode->getOpCode().getName(),
                                          realLoad, realLoad ? realLoad->getOpCode().getName() : "NULL");
                        setOverrideOpt(parent, regLoadOverride);
                        }
                     }
                  }
               }
            else
               {
               // No UseDef info, override OPT
               setOverrideOpt(parent, regLoadOverride);
               }
            }
         else if (load->getOpCode().isLoadConst())
            {
            if (supportedConstLoad(load, comp()))
               countLoad(load, parent);
            }
         else
            countLoad(load, parent);
         }
      }


   for (int32_t i = 0; i < parent->getNumChildren(); i++)
      {
      TR::Node *child = parent->getChild(i);

      // See how a conversion is being used. A narrowing conversion may be unneeded as long as it feeds only:
      //  - conversions
      //  - stores
      //
      // (Do counting for all conversions. Whether it is a narrowing conversion is verified at the flag-setting stage)
      if (child->getOpCode().isConversion())
         {
         if (opcode.isStoreReg())
            {
            // TODO: traverse all the defs linked to parent, see all can be ignored
            setOverrideOpt(child, narrowOverride);
            }
         else if (detectReverseNeededConversionPattern(parent, child))
            setOverrideOpt(child, narrowOverride);
        // We counted all the loads feeding into conversions (before the loop), now count the other references to loads
        else if (!opcode.isConversion() && child->getOpCode().isLoadVar() && supportedType(child))
           countLoad(child, parent);
         }

      // See how a load is being used. If a load feeds only to narrowing conversions, we may set couldIgnoreExtend() flag on the load
      // WARNING: "Clever idea" -- set the LSB, to mean that does _not_ feed into a narrowing conversion, use other bits as int
      if (child->getOpCode().isLoadVar() &&
            (!opcode.isConversion() ||
              (opcode.isConversion() && parent->getSize() < child->getSize())))
         {
         //traceMsg(comp, "\t\t\tSigned %p\n", child);
         //_counts[child->getGlobalIndex()] |= 0x0001 setOverrideOpt(child, loadOverride);;
         }

      // Exclude all loads which feed into global register stores which require sign extensions. This must be done 
      // because Load Extensions is a local optimization and it must respect global sign extension decisions made
      // by GRA. Excluding such loads prevents a situation where GRA decided that a particular global register
      // should be sign extended at its definitions however Load Extensions has determined that the same load
      // should be zero extended. If local RA were to pick the same register for the gloabl register as well as
      // the load then we have a conflicting decision which will result in a conversion to be skipped when it is
      // not supposted to be.
      if (opcode.isStoreReg() && parent->needsSignExtension() && child->getOpCode().isLoadVar())
         {
         _excludedNodes[child->getGlobalIndex()] = true;
         }

      countLoadExtensions(child, visitCount);
      }
   }

/**
 * Detect for
 *   parent
 *     conversion (narrowing)
 *
 * This function returns false for patterns we might ignore a narrowing conversion for
 * @param parent
 * @param conversion
 * @return
 */
bool TR_LoadExtensions::detectReverseNeededConversionPattern(TR::Node* parent, TR::Node* conversion)
   {
   TR::ILOpCode& opcode = parent->getOpCode();

   // Many programmers zero out top bits with AND. Calculate the AND mask that will result in conversion being unneeeded
   // i.e.
   // i2s <unneeded>         i2su <unneeded>
   //   iand                   iand
   //     s2i <unneeded>         su2i <unneeded>
   //       sX                     sX
   //     iconst 0x7FFF          iconst 0xFFFF
   if (opcode.isAnd() &&
         (conversion->getSize() - conversion->getFirstChild()->getSize()) > 0) // Must be widening conversion
      {
      TR::Node *constant;
      int64_t andMask = 8 * conversion->getFirstChild()->getSize();
      andMask  = (1ll<<andMask) - 1;
      int64_t andMask2 = andMask>>1ll;
      if ((NULL != (constant = parent->getFirstChild()) &&
             constant->getOpCode().isLoadConst() ||
           NULL != (constant = parent->getSecondChild()) &&
             constant->getOpCode().isLoadConst() &&
             !constant->getType().isAggregate()) &&
          (constant->get64bitIntegralValue() == andMask2  ||
            (conversion->getOpCode().isZeroExtension() &&  (constant->get64bitIntegralValue() == andMask))))
         return false;
      }
   else if (opcode.isConversion() || (opcode.isStore() && !opcode.isStoreReg()))
      return false;

   return true;
   }

bool TR_LoadExtensions::detectUnneededConversionPattern(TR::Node* conversion, TR::Node* child, bool& mustForce64)
   {
   TR::ILOpCode& opcode = conversion->getOpCode();
   TR::ILOpCode& childOpcode = child->getOpCode();
   const bool isConvWide = child->getSize() < conversion->getSize();
   mustForce64 = false;

   TR::Node *grandChild;
   if (trace()) traceMsg(comp(), "\t\tLooking at conversion %p%s%s\n", conversion,
                        isOverriden(conversion, narrowOverride) ? " [narrow]" : "",
                        isOverriden(conversion, regLoadOverride) ? " [regLoad]" : "");

   int64_t andMask = 8*conversion->getSize();
   andMask  = (1ll<<andMask) - 1;
   int64_t andMask2 = andMask>>1ll;
   if (!_excludedNodes[child->getGlobalIndex()] && supportedType(child))
      {
      bool loadIsSigned = countIsSigned(child);
      /*
       * conv  (widening)
       *   load
       * Set flags on conversion, for it to be ignored
       * Exceptions, cannot ignore if:
       *   - sign of load does not match (i.e. RefCnt > 1, load is being used with different signedness, odd)
       * TODO:
       *   - addresses?
       *   - conv
       *       iload <Fixed24>
       *   - if <Frequency!!!!!>
       *       i2l
       *         load <Load-and-test>
       */

      if ((childOpcode.isLoadVar() || (childOpcode.isLoadConst() && supportedConstLoad(child, comp()))) &&
            isConvWide &&
            (conversion->getSize() != 8 || (TR::Compiler->target.is64Bit() || comp()->cg()->use64BitRegsOn32Bit())) &&
            ((loadIsSigned && (loadIsSigned == opcode.isSignExtension())) || (!loadIsSigned && ((!loadIsSigned) == conversion->isZeroExtension())) )&&
            ( opcode.getOpCodeValue() != TR::a2l || ((child->getReferenceCount() == 1) && TR::Compiler->target.is32Bit())))  // 4 byte a2l really means 31->64! careful!
         {
         if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension pattern on conversion and load nodes %p\n", conversion);
         if ((conversion->getSize() == 8 || conversion->useSignExtensionMode()))
            mustForce64 = true;
         return true;
         }
      // See how a conversion is being used. A narrowing conversion may be unneeded as long as it feeds only:
      //  - conversions
      //  - stores
      // If above is not true, counts[node->getGlobalIndex()] would had been set to true/1
      // l2i can be also ignored: will either grab the reg->getLowOrder() or keep on passing GPR64
      else if (!isConvWide && (!isOverriden(conversion, narrowOverride) ||
            // Also ignore a narrowing conversion if:
            // i2b ---------    <<------------------|
            //   bu2i       = --- same size then unneeded
            //     bX ------
            // (probably too conservative but good for now)
            childOpcode.isConversion() &&
            NULL != (grandChild = child->getFirstChild()) &&
            supportedType(grandChild) &&
            grandChild->getSize() == conversion->getSize()))
         {
         if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension pattern on narrowing conversion node %p\n", conversion);
         return true;
         }
      else if (childOpcode.isRightShift() &&
            child->getSecondChild()->getOpCode().isLoadConst() &&
            child->getSecondChild()->getInt() == (child->getSize() - conversion->getSize())) //(16|24|32...?)
         {
         if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension pattern on conversion and right shift nodes %p\n", conversion);
         return true;
         }
      else if (childOpcode.isBooleanCompare() &&
            conversion->getSize() != 8)// Needs a conversion from GPR32 to GPR64 or 2xGPR32
         {
         if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension pattern on conversion and compare nodes %p\n", conversion);
         return true;
         }
      // Many programmers zero out top bits with AND. Calculate the AND mask that will result in conversion being unneeeded
      // i.e.
      // i2s <unneeded>         i2su <unneeded>
      //   iand                   iand
      //     s2i <unneeded>         su2i <unneeded>
      //       sX                     sX
      //     iconst 0x7FFF          iconst 0xFFFF
      else if (!isConvWide && childOpcode.isAnd())
         {
         uint64_t mask = 0;
         uint64_t maskZeroExtend = andMask;
         uint64_t maskSignExtend = andMask2;
         if (child->getFirstChild()->getOpCode().isLoadConst())
            mask = child->getFirstChild()->get64bitIntegralValue();
         else if (child->getSecondChild()->getOpCode().isLoadConst())
            mask = child->getSecondChild()->get64bitIntegralValue();
         if (mask == 0)
            return false;
         else if (opcode.isZeroExtension() && mask <= maskZeroExtend)
            {
            if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension pattern on conversion and AND nodes %p\n", conversion);
            return true;
            }
         else if (mask <= maskSignExtend)
            {
            if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension pattern on conversion and AND nodes %p\n", conversion);
            return true;
            }
         else
            return false;
         }
#if defined(TR_TARGET_S390)
      // Only for  PLX
      else if (isConvWide && conversion->getSize() == 4 && cg()->isUsing32BitEvaluator(child) &&  child->cannotOverflow() &&
               ((opcode.isSignExtension() && !childOpcode.isLoadReg()) ||
                child->isNonNegative()))
         {
         if (trace()) traceMsg(comp(), "\t\tDetected Sign Extension s2i/b2i pattern on ssub/sadd, bsub/badd and/or sRegLoad/bRegLoad %p\n", conversion);
         return true;
         }
#endif
      else
         {
         return false;
         }
      }
   else
      return false;
   }

void TR_LoadExtensions::setPreferredExtension(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   TR::ILOpCode& opcode = node->getOpCode();

   if (supportedType(node))
      {
      if (opcode.isLoadVar() || (opcode.isLoadConst() && supportedConstLoad(node, comp())))
         {
         bool loadIsSigned = countIsSigned(node);

         node->setIsUnsignedLoad(!loadIsSigned);

         //if ((counts[node->getGlobalIndex()] & 0x0001) == 0)
         //   node->setCouldIgnoreExtend(true);
         }
      else if (opcode.isConversion())
         {
         TR::Node *child = node->getFirstChild();
         TR::ILOpCode& childOpcode = child->getOpCode();
         bool isConvWide = child->getSize() < node->getSize();
         bool isUnneeded = false;
         //traceMsg(_compilation, "\t\t\tLooking at Conversion %p\n", node);
         if (child->getSize() == node->getSize())
            {
            // conversion is unneeded, no need to take the high road...
            if (supportedType(child)) // e.g. pd2i,f2i *are* needed
               {
               if (child->getSize() >= 2 &&
                   (node->getOpCodeValue() == TR::bu2i ||
                    node->getOpCodeValue() == TR::b2i))
                  {
                  }
               else if (child->getSize() >= 4 &&
                        (node->getOpCodeValue() == TR::su2i ||
                         node->getOpCodeValue() == TR::s2i))
                  {
                  }
               else if (child->getSize() == 8 &&
                        (node->getOpCodeValue() == TR::iu2l ||
                         node->getOpCodeValue() == TR::i2l))
                  {
                  }
               else
                  {
                  node->setUnneededConversion(true);
                  }
               }
            }
         else if (childOpcode.isLoadReg()
                  && !(node->getSize() > 4 && TR::Compiler->target.is32Bit())
                  && !isOverriden(node, regLoadOverride) && supportedType(child))
            {
            TR_ASSERT(optimizer() && _useDefInfo && _useDefInfo->infoIsValid() && child->getUseDefIndex() && _useDefInfo->isUseIndex(child->getUseDefIndex()),
                           "No UseDef info available, yet override flag not set!\n");

            TR_UseDefInfo::BitVector info(comp()->allocator());
            _useDefInfo->getUseDef(info, child->getUseDefIndex());
            TR_ASSERT(!info.IsZero() || !childOpcode.isLoadReg(), "Could not get UseDef info for RegLoad!");
            TR_UseDefInfo::BitVector::Cursor cursor(info);
            int32_t firstDefIndex = _useDefInfo->getFirstRealDefIndex();
            int32_t firstUseIndex = _useDefInfo->getFirstUseIndex();
            isUnneeded = true;
            bool mustForce64All = false;
            bool mustForce64 = true;
            for (cursor.SetToFirstOne(); isUnneeded && cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t defIndex = cursor;
               if (defIndex >= firstUseIndex)
                  break;

               // Definition is the method parameter (entry)
               if (defIndex < firstDefIndex)
                  continue;

               TR::Node *defNode = _useDefInfo->getNode(defIndex);
               if (!defNode)
                  continue;

               TR::Node *realLoad = defNode->getFirstChild();
               bool mustForce64Case = false;
               isUnneeded &= detectUnneededConversionPattern(node, realLoad, mustForce64Case);
               mustForce64All |= mustForce64Case;
               mustForce64 &= mustForce64Case;
               isUnneeded &= (mustForce64 == mustForce64All); //i.e. break?
               if (trace()) traceMsg(comp(), "\t\tPeeked through %p (%s) and found %p (%s) with child %p (%s), evaluate %s%s.\n",
                              child, child->getOpCode().getName(),
                              defNode, defNode->getOpCode().getName(),
                              realLoad, realLoad->getOpCode().getName(),
                              isUnneeded ? "unneed" : "need",
                              mustForce64 ? " mustForce64" : "");
               }

            if (isUnneeded && performTransformation(comp(), "%sFound Unneeded conversion %p after RegLoad\n", OPT_DETAILS, node))
               {
               if (isUnneeded && mustForce64)
                  {
                  TR_UseDefInfo::BitVector info2(comp()->allocator());
                  _useDefInfo->getUseDef(info2, child->getUseDefIndex());
                  TR_UseDefInfo::BitVector::Cursor cursor2(info2);
                  for (cursor2.SetToFirstOne(); cursor2.Valid(); cursor2.SetToNextOne())
                     {
                     int32_t defIndex = cursor2;
                     if (defIndex >= firstUseIndex)
                        break;

                     // Definition is the method parameter (entry)
                     if (defIndex < firstDefIndex)
                        continue;

                     TR::Node *defNode = _useDefInfo->getNode(defIndex);
                     if (!defNode)
                        continue;
                     if (trace()) traceMsg(comp(), "\t\t\tSetting force64 on %p (%p)\n", defNode->getFirstChild(), child);
                     defNode->getFirstChild()->setForce64BitLoad(true);
                     }
                  }
               node->setUnneededConversion(isUnneeded);
               if (node->getType().isInt64() &&
                   node->getSize() > child->getSize())
                  {
                  if (trace())
                     traceMsg(comp(),"\t\t\tset num %d (%s) in getExtendedToInt64GlobalRegisters for child %s (%p) with parent node %s (%p)\n",
                        child->getGlobalRegisterNumber(),
                        comp()->getDebug()->getGlobalRegisterName(child->getGlobalRegisterNumber()),
                        child->getOpCode().getName(),
                        child,
                        node->getOpCode().getName(),
                        node);
                  // getExtendedToInt64GlobalRegisters is used by the evaluators to force a larger virtual register to be used when
                  // evaluating the regload so any instructions generated by local RA are the correct size to preserve the upper bits
                  cg()->getExtendedToInt64GlobalRegisters()[child->getGlobalRegisterNumber()] = true;
                  }
               }
            }

         if (!isUnneeded)
            {
            bool mustForce64 = false;
            isUnneeded = detectUnneededConversionPattern(node, child, mustForce64);
            if (isUnneeded && performTransformation(comp(), "%sFound Unneeded conversion %p\n", OPT_DETAILS, node))
               {
               if (mustForce64)
                  child->setForce64BitLoad(true);
               node->setUnneededConversion(isUnneeded);
               }
            }
         }
      /*else if (opcode.isIf())
         {

         } */
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      setPreferredExtension(node->getChild(i), visitCount);
      }
   }

/*
 * globalIndex() is not available at codegen, make our own
 */
ncount_t TR_LoadExtensions::indexNodesForCodegen(TR::Node *parent, ncount_t nodeCount, vcount_t visitCount)
   {
   if (parent->getVisitCount() == visitCount)
      return nodeCount;
   parent->setVisitCount(visitCount);

   ncount_t _nodeCount = nodeCount;
   TR::ILOpCode& opcode = parent->getOpCode();
   if ((opcode.isLoadVar() && supportedType(parent))
         || (opcode.isLoadConst() && supportedConstLoad(parent, comp()))
         //|| opcode.isConversion() // can't index conversion nodes, the second child is already reused for other purposes
         )
      {
      if (trace()) traceMsg(cg(), "Marking node %p as %d [children: %d]\n", parent, _nodeCount, parent->getNumChildren());
      TR_ASSERT(parent->getNumChildren()<2, "Cannot reuse the second child slot, already taken\n");
      parent->setSecond(reinterpret_cast<TR::Node *>(_nodeCount));
      _nodeCount++;
      _seenLoads |= 1;
      }

   if (opcode.isLoadReg())
      _seenLoads |= 2;

      // The following line should not be needed, especially in case somebody really wants to mark conversion nodes as unneeded earlier
      // but I am tired of finding bugs with flags incorrectly set (i.e. changing Node opcode and inheriting -incorrect/overloaded- flags)
      // As far as I know, nobody else is using these flags in other optimizations yet.
      // Another option is to move the unneededConversion flag to its own bitvector, as with load flags. That is also more work because
      // second-child in conversion nodes may already be sometimes used by <code>bool TR_DynamicLiteralPool::addNewAloadChild(TR::Node *node)</code>
      // (the second child is being used instead of globalIndex)
   if (!comp()->getOptions()->getDebugEnableFlag(TR_EnableUnneededNarrowIntConversion))
      if (opcode.isConversion())
         parent->setUnneededConversion(false);

   for (int32_t i = 0; i < parent->getNumChildren(); i++)
      {
      _nodeCount = indexNodesForCodegen(parent->getChild(i), _nodeCount, visitCount);
      }

   return _nodeCount;
   }

const char *
TR_LoadExtensions::optDetailString() const throw()
   {
   return "O^O LOAD EXTENSION: ";
   }
