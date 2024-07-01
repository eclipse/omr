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

#include "ras/DebugCounter.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "env/StackMemoryRegion.hpp"
#include "env/VerboseLog.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/Assert.hpp"
#include "infra/Monitor.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Bit.hpp"

#if defined(_MSC_VER)
#include <malloc.h> // alloca on Windows
#endif


void
TR::DebugCounter::prependDebugCounterBump(TR::Compilation *comp, TR::TreeTop *nextTreeTop, TR::DebugCounterBase *counter, int32_t delta)
   {
   // Use long operations in 64 bit platforms and int operations in 32 bit platforms
   if (comp->target().is64Bit())
      {
      prependDebugCounterBump(comp, nextTreeTop, counter, TR::Node::lconst(nextTreeTop->getNode(), delta));
      }
   else
      {
      prependDebugCounterBump(comp, nextTreeTop, counter, TR::Node::iconst(nextTreeTop->getNode(), delta));
      }
   }

void
TR::DebugCounter::prependDebugCounterBump(TR::Compilation *comp, TR::TreeTop *nextTreeTop, TR::DebugCounterBase *counter, TR::Node *deltaNode)
   {
   if (!nextTreeTop || !deltaNode)
      return;
   TR::Node *bumpNode = counter->createBumpCounterNode(comp, deltaNode);
   if (bumpNode)
      nextTreeTop->insertBefore(TR::TreeTop::create(comp, bumpNode));
   }

void
TR::DebugCounter::prependDebugCounter(TR::Compilation *comp, const char *name, TR::TreeTop *nextTreeTop, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (!comp->getOptions()->enableDebugCounters())
      return;
   if (!nextTreeTop)
      return;
   if (delta == 0)
      return;

   TR::DebugCounterAggregation *aggregatedCounters = comp->getPersistentInfo()->getDynamicCounters()->createAggregation(comp, name);
   aggregatedCounters->aggregateStandardCounters(comp, nextTreeTop->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return;

   if (comp->compileRelocatableCode() && !aggregatedCounters->initializeReloData(comp, delta, fidelity, staticDelta))
      return;

   prependDebugCounterBump(comp, nextTreeTop, aggregatedCounters, 1);
   }

void
TR::DebugCounter::prependDebugCounter(TR::Compilation *comp, const char *name, TR::TreeTop *nextTreeTop, TR::Node *deltaNode, int8_t fidelity, int32_t staticDelta)
   {
   if (!comp->getOptions()->enableDebugCounters())
      return;
   if (!nextTreeTop)
      return;

   TR::DebugCounter *counter = TR::DebugCounter::getDebugCounter(comp, name, fidelity, staticDelta);
   if (!counter)
      return;

   if (comp->compileRelocatableCode() && !counter->initializeReloData(comp, 0, fidelity, staticDelta))
      return;

   prependDebugCounterBump(comp, nextTreeTop, counter, deltaNode);
   }

TR::DebugCounter *
TR::DebugCounter::getDebugCounter(TR::Compilation *comp, const char *name, int8_t fidelity, int32_t staticDelta)
   {
   if (comp->getOptions()->staticDebugCounterIsEnabled(name, fidelity))
      {
      TR::DebugCounter *staticCounter = comp->getPersistentInfo()->getStaticCounters()->getCounter(comp, name, fidelity);
      staticCounter->increment(staticDelta);
      }
   if (comp->getOptions()->dynamicDebugCounterIsEnabled(name, fidelity)
      && performTransformation(comp, "O^O DEBUG COUNTER: '%s'\n", name))
      {
      return comp->getPersistentInfo()->getDynamicCounters()->getCounter(comp, name, fidelity);
      }
   else
      {
      return NULL;
      }
   }

void
TR::DebugCounter::generateRelocation(TR::Compilation *comp, uint8_t *location, TR::Node *node, TR::DebugCounterBase *counter)
   {
   counter->finalizeReloData(comp, node);

   comp->cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         location,
         (uint8_t *)counter,
         TR_DebugCounter,
         comp->cg()),
      __FILE__,
      __LINE__,
      node);
   }

void
TR::DebugCounter::generateRelocation(TR::Compilation *comp, TR::Instruction *firstInstruction, TR::Node *node, TR::DebugCounterBase *counter, uint8_t seqKind)
   {
   counter->finalizeReloData(comp, node, seqKind);

   TR::BeforeBinaryEncodingExternalRelocation *r = new (comp->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                                                                     firstInstruction,
                                                                                     (uint8_t *)counter,
                                                                                     TR_DebugCounter,
                                                                                     comp->cg());

   comp->cg()->addExternalRelocation(r, __FILE__, __LINE__, node);
   }

void
TR::DebugCounter::generateRelocation(TR::Compilation *comp, TR::Instruction *firstInstruction, TR::Instruction *secondInstruction, TR::Node *node, TR::DebugCounterBase *counter, uint8_t seqKind)
   {
   counter->finalizeReloData(comp, node, seqKind);

   TR::ExternalOrderedPair32BitRelocation *r = new (comp->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(
                                                                                     (uint8_t *)firstInstruction,
                                                                                     (uint8_t *)secondInstruction,
                                                                                     (uint8_t *)counter,
                                                                                     TR_DebugCounter,
                                                                                     comp->cg());

   comp->cg()->addExternalRelocation(r, __FILE__, __LINE__, node);
   }

bool
TR::DebugCounter::relocatableDebugCounter(TR::Compilation *comp)
   {
   return comp->compileRelocatableCode();
   }

const char *TR::DebugCounter::debugCounterName(TR::Compilation *comp, const char *format, ...)
   {

   if (!comp->getOptions()->enableDebugCounters())
      {
      return NULL;
      }

   va_list args;
   va_start(args, format);
   const char *result = comp->getPersistentInfo()->getStaticCounters()->counterName(comp, format, args);
   va_end(args);
   return result;
   }


const char *TR::DebugCounter::debugCounterBucketName(TR::Compilation *comp, int32_t value, const char *format, ...)
   {

   if (!comp->getOptions()->enableDebugCounters())
      {
      return NULL;
      }

   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());
   char *bucketFormat = (char*)comp->trMemory()->allocateStackMemory(strlen(format) + 40); // appending "=%d..%d" where each %d could be 11 characters

   int32_t low  = value;
   int32_t high = value;

   if (value != 0 && comp->getOptions()->getDebugCounterBucketGranularity() >= 1)
      {
      const int32_t magnitude = abs(value);
      low = high = magnitude;
      const int32_t granularity = comp->getOptions()->getDebugCounterBucketGranularity();
      const double  bucketRatio = pow(2.0, 1.0 / granularity); // TODO: calculate once
      const int32_t logLow  = (int)(log((double)magnitude)/log(bucketRatio));

      // Figure out which bucket sizing algorithm to use
      //
      const int32_t log2magnitude = 31-leadingZeroes(magnitude); // floor
      const int32_t doublingInterval = 1 << log2magnitude;
      if (doublingInterval <= granularity)
         {
         // Tiny buckets degenerate to one value per bucket
         high = low;
         }
      else
         {
         // We do this with some buckets of size smallBucketSize, plus
         // some that are 1 larger.
         // We'd like to make sure make sure the smaller ones come
         // before bigger ones (eg. we want to see 8-9, 10-12, 13-15
         // rather than 8-10, 11-12, 13-15)
         //
         low = 1 << log2magnitude; // power-of-two starting point
         int32_t smallBucketSize      = doublingInterval / granularity;
         int32_t numBigBuckets        = doublingInterval - smallBucketSize * granularity;
         int32_t numSmallBuckets      = granularity - numBigBuckets;
         int32_t totalSmallBucketSize = numSmallBuckets * smallBucketSize;
         int32_t offset               = magnitude-low;
         if (offset < totalSmallBucketSize)
            {
            low += offset - offset % smallBucketSize;
            high = low + smallBucketSize - 1;
            }
         else
            {
            offset -= totalSmallBucketSize;
            low += totalSmallBucketSize + offset - offset % (smallBucketSize+1);
            high = low + smallBucketSize;
            }
         }

      TR_ASSERT(low <= magnitude && magnitude <= high, "Range (%d..%d) must contain %d\n", low, high, magnitude);

      if (value < 0)
         {
         low  = -low;
         high = -high;
         }
      }

   if (low == high)
      sprintf(bucketFormat, "%s=%d", format, low);
   else
      sprintf(bucketFormat, "%s=%d..%d", format, low, high);

   va_list args;
   va_start(args, format);
   const char *result = comp->getPersistentInfo()->getStaticCounters()->counterName(comp, bucketFormat, args);
   va_end(args);

   return result;
   }

/** Use long operations in 64 bit platforms and int operations in 32 bit platforms */
TR::Node *TR::DebugCounterBase::createBumpCounterNode(TR::Compilation *comp, TR::Node *deltaNode)
   {
   TR::SymbolReference *symref = getBumpCountSymRef(comp);
   TR::Node *load = TR::Node::createWithSymRef(deltaNode, comp->target().is64Bit() ? TR::lload : TR::iload, 0, symref);
   TR::Node *add = TR::Node::create(comp->target().is64Bit() ? TR::ladd : TR::iadd, 2, load, deltaNode);
   TR::Node *store = TR::Node::createWithSymRef(comp->target().is64Bit() ? TR::lstore : TR::istore, 1, 1, add, symref);

   if (comp->compileRelocatableCode())
      comp->mapStaticAddressToCounter(symref, this);

   return store;
   }

bool
TR::DebugCounterBase::initializeReloData(TR::Compilation *comp, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (!_reloData)
      {
      _reloData = new (comp->trPersistentMemory()) TR::DebugCounterReloData(delta, fidelity, staticDelta);
      }
   return _reloData != NULL;
   }

void
TR::DebugCounterBase::finalizeReloData(TR::Compilation *comp, TR::Node *node, uint8_t seqKind)
   {
   TR::DebugCounterReloData *reloData = getReloData();
   if (!reloData)
      comp->failCompilation<TR::CompilationException>("Failed to finalizeReloData\n");

   TR_ByteCodeInfo &bci = node->getByteCodeInfo();
   reloData->_callerIndex = bci.getCallerIndex();
   reloData->_bytecodeIndex = bci.getByteCodeIndex();
   reloData->_seqKind = seqKind;
   }

TR::SymbolReference *TR::DebugCounter::getBumpCountSymRef(TR::Compilation *comp)
   {
   TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateCounterSymRef(const_cast<char*>(_name), comp->target().is64Bit() ? TR::Int64 : TR::Int32, &_bumpCount);
   symRef->getSymbol()->setIsDebugCounter();
   return symRef;
   }

intptr_t TR::DebugCounter::getBumpCountAddress()
   {
   return (intptr_t)&_bumpCount;
   }

void TR::DebugCounter::getInsertionCounterNames(TR::Compilation *comp, TR_OpaqueMethodBlock *method, int32_t bytecodeIndex, const char *(&counterNames)[3])
   {
   memset(counterNames, 0, sizeof(counterNames));
   bool insertByteCode = TR::Options::_debugCounterInsertByteCode && TR::SimpleRegex::match(TR::Options::_debugCounterInsertByteCode, getName(), true);
   bool insertJittedBody = TR::Options::_debugCounterInsertJittedBody && TR::SimpleRegex::match(TR::Options::_debugCounterInsertJittedBody, getName(), true);
   bool insertMethod = TR::Options::_debugCounterInsertMethod && TR::SimpleRegex::match(TR::Options::_debugCounterInsertMethod, getName(), true);
   if (!insertByteCode && !insertJittedBody && !insertMethod)
      return;

   const char *methodName;
   char methodBuf[200];
   if (method == NULL)
      {
      methodName = comp->signature();
      }
   else
      {
      methodName = comp->fe()->sampleSignature(method, methodBuf, sizeof(methodBuf), comp->trMemory());
      }
   const char *jittedBody = comp->signature();
   if (insertByteCode)
      {
      counterNames[0] = TR::DebugCounter::debugCounterName(comp, comp->getOptions()->debugCounterInsertedFormat(comp->trMemory(), getName(), "byByteCode.(%s)=%d"), methodName, bytecodeIndex);
      }
   if (insertJittedBody)
      {
      counterNames[1] = TR::DebugCounter::debugCounterName(comp, comp->getOptions()->debugCounterInsertedFormat(comp->trMemory(), getName(), "byJittedBody.(%s).%s"), jittedBody, comp->getHotnessName());
      }
   if (insertMethod)
      {
      counterNames[2] = TR::DebugCounter::debugCounterName(comp, comp->getOptions()->debugCounterInsertedFormat(comp->trMemory(), getName(), "byMethod.(%s)"), methodName);
      }
   }


void TR::DebugCounterAggregation::aggregate(TR::DebugCounter *counter, int32_t delta)
   {
   if (!counter || delta == 0)
      return;
   _counterDeltas->add(new (_mem) CounterDelta(counter, delta));
   }

void TR::DebugCounterAggregation::aggregateStandardCounters(TR::Compilation *comp, TR::Node *node, const char *counterName, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   TR_ByteCodeInfo &bci = node->getByteCodeInfo();
   int16_t callerIndex = bci.getCallerIndex();
   int32_t bytecodeIndex = bci.getByteCodeIndex();
   TR_OpaqueMethodBlock *method = NULL;

   if (callerIndex > -1)
      {
      TR_InlinedCallSite &ics = comp->getInlinedCallSite(callerIndex);
      method = ics._methodInfo;
      }

   aggregateStandardCounters(comp, method, bytecodeIndex, counterName, delta, fidelity, staticDelta);
   }

void TR::DebugCounterAggregation::aggregateStandardCounters(TR::Compilation *comp, TR_OpaqueMethodBlock *method, int32_t bytecodeIndex, const char *counterName, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (delta == 0 || method == (TR_OpaqueMethodBlock *)-1)
      return;

   TR::DebugCounter *counter = TR::DebugCounter::getDebugCounter(comp, counterName, fidelity, staticDelta);
   if (!counter)
      return;

   aggregate(counter, delta);
   aggregateDebugCounterInsertions(comp, method, bytecodeIndex, counter, delta, fidelity, staticDelta);
   aggregateDebugCounterHistogram(comp, counter, delta, fidelity, staticDelta);
   }

void TR::DebugCounterAggregation::aggregateDebugCounterInsertions(TR::Compilation *comp, TR_OpaqueMethodBlock *method, int32_t bytecodeIndex, TR::DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   const char *counterNames[3];
   counter->getInsertionCounterNames(comp, method, bytecodeIndex, counterNames);

   if (counter && counter->getDenominator() && counter->contributesToDenominator())
      aggregateDebugCounterInsertions(comp, method, bytecodeIndex, counter->getDenominator(), delta, fidelity, staticDelta);

   for (int i = 0; i < sizeof(counterNames) / sizeof(counterNames[0]); i++)
      {
      if (counterNames[i])
         {
         aggregate(TR::DebugCounter::getDebugCounter(comp, counterNames[i], fidelity, staticDelta), delta);
         }
      }
   }

void TR::DebugCounterAggregation::aggregateDebugCounterHistogram(TR::Compilation *comp, TR::DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (comp->getOptions()->debugCounterHistogramIsEnabled(counter->getName(), fidelity))
      {
      TR::DebugCounter *bucketCounter = TR::DebugCounter::getDebugCounter(comp, TR::DebugCounter::debugCounterBucketName(comp, delta, "%s", counter->getName()), fidelity, 1);
      aggregate(bucketCounter, 1);
      }
   }

intptr_t TR::DebugCounterAggregation::getBumpCountAddress()
   {
   return (intptr_t)&_bumpCount;
   }

TR::SymbolReference *TR::DebugCounterAggregation::getBumpCountSymRef(TR::Compilation *comp)
   {
   if (_symRef == NULL)
      {
      TR::StaticSymbol *symbol = TR::StaticSymbol::create(_mem,TR::Int64);
      TR_ASSERT(symbol, "StaticSymbol *symbol must not be null. Ensure availability of persistent memory");
      symbol->setStaticAddress(&_bumpCount);
      symbol->setNotDataAddress();
      _symRef = new (_mem) TR::SymbolReference(comp->getSymRefTab(), symbol);
      TR_ASSERT(_symRef, "SymbolReference *_symRef must not be null. Ensure availability of persistent memory");
      symbol->setIsDebugCounter();
      }
   return _symRef;
   }

void TR::DebugCounterAggregation::accumulate()
   {
   int64_t bumpCountCopy = _bumpCount;
   int64_t increment = bumpCountCopy - _lastBumpCount;
   _lastBumpCount = bumpCountCopy;
   ListIterator<CounterDelta> it(_counterDeltas);
   for (CounterDelta *counterDelta = it.getFirst(); counterDelta; counterDelta = it.getNext())
      {
      counterDelta->counter->accumulate(increment * counterDelta->delta);
      }
   }


int64_t TR::DebugCounterAggregation::getCount()
   {
   int64_t count = 0;
   ListIterator<CounterDelta> it(_counterDeltas);

   for (CounterDelta *counterDelta = it.getFirst(); counterDelta; counterDelta = it.getNext())
      {
      count += counterDelta->counter->getCount();
      }

   return count;
   }

void TR::DebugCounterAggregation::printCounters(bool printZeroCounters)
   {
   ListIterator<CounterDelta> it(_counterDeltas);

   for (CounterDelta *counterDelta = it.getFirst(); counterDelta; counterDelta = it.getNext())
      {
      TR::DebugCounter *counter = counterDelta->counter;
      int64_t count = counter->getCount();

      if (count || printZeroCounters)
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Counter count=%d %s", count, counter->getName());
         }
      }
   }

TR::DebugCounter *TR::DebugCounterGroup::findCounter(const char *nameChars, int32_t nameLength)
   {
   if (nameChars == NULL)
      return NULL;
   char *name = (char*)alloca(nameLength+1);
   strncpy(name, nameChars, nameLength);
   name[nameLength] = 0;

   OMR::CriticalSection findCounterLock(_countersMutex);

   CS2::HashIndex hi;
   if (!_countersHashTable.Locate(name, hi))
      return NULL;
   return _countersHashTable[hi];
   }

TR::DebugCounterAggregation *TR::DebugCounterGroup::findAggregation(const char *nameChars, int32_t nameLength)
   {
   if (nameChars == NULL)
      return NULL;
   char *name = (char*)alloca(nameLength+1);
   strncpy(name, nameChars, nameLength);
   name[nameLength] = 0;

   OMR::CriticalSection findAggregationLock(_countersMutex);

   CS2::HashIndex hi;
   if (!_aggregateCountersHashTable.Locate(name, hi))
      return NULL;
   return _aggregateCountersHashTable[hi];
   }


TR::DebugCounterAggregation *TR::DebugCounterGroup::createAggregation(TR::Compilation *comp, const char * name)
   {
   TR::DebugCounterAggregation *aggregatedCounters = new (comp->trPersistentMemory()) TR::DebugCounterAggregation(name, comp->trPersistentMemory());
   _aggregations.add(aggregatedCounters);

   OMR::CriticalSection createAggregationCS(_countersMutex);

   _aggregateCountersHashTable.Add(aggregatedCounters->getName(), aggregatedCounters);

   return aggregatedCounters;
   }

TR::DebugCounter *TR::DebugCounterGroup::createCounter(const char *name, int8_t fidelity, TR_PersistentMemory *persistentMemory)
   {
   // Get the denominator counter, if any, by looking for the rightmost separator character.
   //
   TR::DebugCounter *denominator = NULL;
   const char *separator = NULL;
   int verbatimDepth = 0;
   for (const char *c = name; c[0]; c++)
      {
      if (c[0] == VERBATIM_START)
         verbatimDepth++;
      else if (c[0] == VERBATIM_STOP)
         verbatimDepth--;

      if (verbatimDepth == 0)
         {
         if (c[0] == FRACTION_SEPARATOR || c[0] == RATIO_SEPARATOR)
            separator = c;
         }
      }
   if (separator) // There is a denominator counter
      {
      denominator = findCounter(name, static_cast<int32_t>(separator-name));
      if (!denominator)
         {
         // Can't be lazy anymore; we really need to make a copy of part of the name string
         //
         char *denominatorName = (char*)persistentMemory->allocatePersistentMemory(separator-name+1);
         TR_ASSERT(denominatorName, "char *denominatorName must not be null. Ensure availability of persistent memory");
         sprintf(denominatorName, "%.*s", (int)(separator-name), name);

         // Counter has negligible cost, assuming it's only used as a denominator
         //
         denominator = createCounter(denominatorName, TR::DebugCounter::Free, persistentMemory);
         }
      }

   // Create the desired counter
   //
   int8_t flags = 0;
   if (separator && separator[0] == FRACTION_SEPARATOR)
      {
      TR_ASSERT(denominator, "Debug counter with a fraction separator character must have a denominator");
      flags |= TR::DebugCounter::ContributesToDenominator;
      }
   TR::DebugCounter *result = new (persistentMemory) TR::DebugCounter(name, fidelity, denominator, flags);
   TR_ASSERT(result, "DebugCounter *result must not be null. Ensure availability of persistent memory");
   _counters.add(result);

   OMR::CriticalSection createCounterLock(_countersMutex);

   _countersHashTable.Add(result->getName(), result);

   return result;
   }

const char *TR::DebugCounterGroup::counterName(TR::Compilation *comp, const char *format, va_list args)
   {
   const char *name = comp->getDebug()->formattedString(NULL, 0, format, args, persistentAlloc);
   TR::DebugCounter *matchingCounter = findCounter(name, static_cast<int32_t>(strlen(name)));
   if (matchingCounter)
      {
      comp->trMemory()->jitPersistentFree((void*)name);
      name = matchingCounter->getName();
      }
   return name;
   }

TR::DebugCounter *TR::DebugCounterGroup::getCounter(TR::Compilation *comp, const char *name, int8_t fidelity)
   {
   // Note: we assume the name string passed in is safe to keep persistently.
   // If this is not the case, the caller must make a copy of it into
   // persistent memory.

   TR::DebugCounter *result = findCounter(name, static_cast<int32_t>(strlen(name)));
   if (!result)
      result = createCounter(name, fidelity, comp->trPersistentMemory());

   TR_ASSERT(result->getFidelity() >= fidelity, "Request for counter at fidelity %d is too high; counter only has fidelity %d: %s\n", fidelity, result->getFidelity(), name);
   result->_fidelity = fidelity;

   return result;
   }

void TR::DebugCounterGroup::accumulate()
   {
   // TODO: Note that this is O(k * n); we should be able to do this better with
   // a breadth-first visit from the leaves upward.
   ListIterator<TR::DebugCounter> li(&_counters);
   for (TR::DebugCounter *counter = li.getCurrent(); counter; counter = li.getNext())
      counter->accumulate();

   if (!_counters.isEmpty())
      {
      ListIterator<TR::DebugCounterAggregation> li2(&_aggregations);
      for (TR::DebugCounterAggregation *aggregatedCounters = li2.getCurrent(); aggregatedCounters; aggregatedCounters = li2.getNext())
         aggregatedCounters->accumulate();
      }
   }

void TR::DebugCounterGroup::resetAll()
   {
   ListIterator<TR::DebugCounter> li(&_counters);
   for (TR::DebugCounter *counter = li.getCurrent(); counter; counter = li.getNext())
      counter->reset();

   // TODO: Aggregations??
   }

void OMR::PersistentInfo::createCounters(TR_PersistentMemory *mem)
   {
   _staticCounters  = new (mem) TR::DebugCounterGroup(mem);
   _dynamicCounters = new (mem) TR::DebugCounterGroup(mem);
   }
