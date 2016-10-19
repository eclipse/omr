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

#include "ras/DebugCounter.hpp"

#include <stdarg.h>                          // for va_list
#include <stdio.h>                           // for sprintf
#include <string.h>                          // for strlen, memset, strncpy
#include <math.h>                            // for log, pow
#include "codegen/FrontEnd.hpp"              // for TR_FrontEnd
#include "compile/Compilation.hpp"           // for Compilation
#include "compile/SymbolReferenceTable.hpp"  // for SymbolReferenceTable
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"       // for TR::Options, etc
#include "env/PersistentInfo.hpp"            // for PersistentInfo
#include "env/jittypes.h"                    // for TR_ByteCodeInfo, etc
#include "env/StackMemoryRegion.hpp"
#include "il/DataTypes.hpp"                  // for DataTypes::Int32
#include "il/ILOpCodes.hpp"                  // for ILOpCodes::iadd, etc
#include "il/Node.hpp"                       // for Node
#include "il/Node_inlines.hpp"               // for Node::createWithSymRef
#include "il/SymbolReference.hpp"            // for SymbolReference
#include "il/symbol/StaticSymbol.hpp"        // for StaticSymbol
#include "infra/Assert.hpp"                  // for TR_ASSERT
#include "ras/Debug.hpp"                     // for TR_Debug
#include "infra/SimpleRegex.hpp"
#include "infra/Assert.hpp"
#include "infra/Monitor.hpp"                 // for createCounter race conditions
#include "infra/CriticalSection.hpp"         // for createCounter race conditions

#if defined(_MSC_VER)
#include <malloc.h> // alloca on Win32
#endif


void
TR::DebugCounter::prependDebugCounterBump(TR::Compilation *comp, TR::TreeTop *nextTreeTop, TR::DebugCounterBase *counter, int32_t delta)
   {
   prependDebugCounterBump(comp, nextTreeTop, counter, TR::Node::iconst(nextTreeTop->getNode(), delta));
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

   TR::DebugCounterAggregation *aggregatedCounters = comp->getPersistentInfo()->getDynamicCounters()->createAggregation(comp);
   aggregatedCounters->aggregateStandardCounters(comp, nextTreeTop->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
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


TR::Node *TR::DebugCounterBase::createBumpCounterNode(TR::Compilation *comp, TR::Node *deltaNode)
   {
   TR::SymbolReference *symref = getBumpCountSymRef(comp);
   deltaNode->setIsDebug(true);
   TR::Node *load = TR::Node::createWithSymRef(deltaNode, TR::iload, 0, symref);
   load->setIsDebug(true);
   TR::Node *add = TR::Node::create(TR::iadd, 2, load, deltaNode);
   add->setIsDebug(true);
   TR::Node *store = TR::Node::createWithSymRef(TR::istore, 1, 1, add, symref);
   store->setIsDebug(true);
   return store;
   }

TR::SymbolReference *TR::DebugCounter::getBumpCountSymRef(TR::Compilation *comp)
   {
   return comp->getSymRefTab()->findOrCreateCounterSymRef(const_cast<char*>(_name), TR::Int32, &_bumpCount);
   }

intptrj_t TR::DebugCounter::getBumpCountAddress()
   {
   return (intptrj_t)&_bumpCount;
   }

void TR::DebugCounter::getInsertionCounterNames(TR::Compilation *comp, TR::Node *node, const char *(&counterNames)[3])
   {
   memset(counterNames, 0, sizeof(counterNames));
   bool insertByteCode = TR::Options::_debugCounterInsertByteCode && TR::SimpleRegex::match(TR::Options::_debugCounterInsertByteCode, getName(), true);
   bool insertJittedBody = TR::Options::_debugCounterInsertJittedBody && TR::SimpleRegex::match(TR::Options::_debugCounterInsertJittedBody, getName(), true);
   bool insertMethod = TR::Options::_debugCounterInsertMethod && TR::SimpleRegex::match(TR::Options::_debugCounterInsertMethod, getName(), true);
   if (!insertByteCode && !insertJittedBody && !insertMethod)
      return;

   TR_ByteCodeInfo &bci = node->getByteCodeInfo();
   const char *method;
   char methodBuf[200];
   if (bci.getCallerIndex() == -1)
      {
      method = comp->signature();
      }
   else
      {
      TR_InlinedCallSite &ics = comp->getInlinedCallSite(bci.getCallerIndex());
      method = comp->fe()->sampleSignature(ics._methodInfo, methodBuf, sizeof(methodBuf), comp->trMemory());
      }
   const char *jittedBody = comp->signature();
   if (insertByteCode)
      {
      counterNames[0] = TR::DebugCounter::debugCounterName(comp, comp->getOptions()->debugCounterInsertedFormat(comp->trMemory(), getName(), "byByteCode.(%s)=%d"), method, bci.getByteCodeIndex());
      }
   if (insertJittedBody)
      {
      counterNames[1] = TR::DebugCounter::debugCounterName(comp, comp->getOptions()->debugCounterInsertedFormat(comp->trMemory(), getName(), "byJittedBody.(%s).%s"), jittedBody, comp->getHotnessName());
      }
   if (insertMethod)
      {
      counterNames[2] = TR::DebugCounter::debugCounterName(comp, comp->getOptions()->debugCounterInsertedFormat(comp->trMemory(), getName(), "byMethod.(%s)"), method);
      }
   }


void TR::DebugCounterAggregation::aggregate(TR::DebugCounter *counter, int32_t delta)
   {
   if (!counter || delta == 0)
      return;
   _counterDeltas->add(new (_mem->trPersistentMemory()) CounterDelta(counter, delta));
   }

void TR::DebugCounterAggregation::aggregateStandardCounters(TR::Compilation *comp, TR::Node *node, const char *counterName, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (delta == 0)
      return;

   TR::DebugCounter *counter = TR::DebugCounter::getDebugCounter(comp, counterName, fidelity, staticDelta);
   if (!counter)
      return;

   aggregate(counter, delta);
   aggregateDebugCounterInsertions(comp, node, counter, delta, fidelity, staticDelta);
   aggregateDebugCounterHistogram(comp, node, counter, delta, fidelity, staticDelta);
   }

void TR::DebugCounterAggregation::aggregateDebugCounterInsertions(TR::Compilation *comp, TR::Node *node, TR::DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   const char *counterNames[3];
   counter->getInsertionCounterNames(comp, node, counterNames);

   if (counter && counter->getDenominator() && counter->contributesToDenominator())
      aggregateDebugCounterInsertions(comp, node, counter->getDenominator(), delta, fidelity, staticDelta);

   for (int i = 0; i < sizeof(counterNames) / sizeof(counterNames[0]); i++)
      {
      if (counterNames[i])
         {
         aggregate(TR::DebugCounter::getDebugCounter(comp, counterNames[i], fidelity, staticDelta), delta);
         }
      }
   }

void TR::DebugCounterAggregation::aggregateDebugCounterHistogram(TR::Compilation *comp, TR::Node *node, TR::DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (comp->getOptions()->debugCounterHistogramIsEnabled(counter->getName(), fidelity))
      {
      TR::DebugCounter *bucketCounter = TR::DebugCounter::getDebugCounter(comp, TR::DebugCounter::debugCounterBucketName(comp, delta, "%s", counter->getName()), fidelity, 1);
      aggregate(bucketCounter, 1);
      }
   }

intptrj_t TR::DebugCounterAggregation::getBumpCountAddress()
   {
   return (intptrj_t)&_bumpCount;
   }

TR::SymbolReference *TR::DebugCounterAggregation::getBumpCountSymRef(TR::Compilation *comp)
   {
   if (_symRef == NULL)
      {
      TR::StaticSymbol *symbol = TR::StaticSymbol::create(_mem->trPersistentMemory(),TR::Int64);
      symbol->setStaticAddress(&_bumpCount);
      _symRef = new (_mem->trPersistentMemory()) TR::SymbolReference(comp->getSymRefTab(), symbol);
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

TR::DebugCounter *TR::DebugCounterGroup::findCounter(const char *nameChars, int32_t nameLength)
   {
   if (nameChars == NULL)
      return NULL;
   char *name = (char*)alloca(nameLength+1);
   strncpy(name, nameChars, nameLength);
   name[nameLength] = 0;

   // There's a race here if we do parallel compilation

   OMR::CriticalSection findCounterLock(_countersMutex);

   CS2::HashIndex hi;
   if (!_countersHashTable.Locate(name, hi))
      return NULL;
   return _countersHashTable[hi];
   }

TR::DebugCounterAggregation *TR::DebugCounterGroup::createAggregation(TR::Compilation *comp)
   {
   TR::DebugCounterAggregation *aggregatedCounters = new (comp->trPersistentMemory()) TR::DebugCounterAggregation(comp->trMemory());
   _aggregations.add(aggregatedCounters);
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
      denominator = findCounter(name, separator-name);
      if (!denominator)
         {
         // Can't be lazy anymore; we really need to make a copy of part of the name string
         //
         char *denominatorName = (char*)persistentMemory->allocatePersistentMemory(separator-name+1);
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
   _counters.add(result);
   
   // There's a race here if we do parallel compilation

   OMR::CriticalSection createCounterLock(_countersMutex);

   _countersHashTable.Add(result->getName(), result);

   return result;
   }

const char *TR::DebugCounterGroup::counterName(TR::Compilation *comp, const char *format, va_list args)
   {
   const char *name = comp->getDebug()->formattedString(NULL, 0, format, args, persistentAlloc);
   TR::DebugCounter *matchingCounter = findCounter(name, strlen(name));
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

   TR::DebugCounter *result = findCounter(name, strlen(name));
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
