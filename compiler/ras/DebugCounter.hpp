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

#ifndef DEBUGCOUNTER_INCL
#define DEBUGCOUNTER_INCL

#include "ras/Debug.hpp"

#include <stdarg.h>          // for va_list
#include <stddef.h>          // for NULL
#include <stdint.h>          // for int32_t, int8_t, uint32_t, int64_t, etc
#include "cs2/hashtab.h"     // for HashTable
#include "env/TRMemory.hpp"  // for TR_MemoryBase::ObjectType::DebugCounter, etc
#include "env/jittypes.h"    // for intptrj_t
#include "infra/Flags.hpp"   // for flags8_t
#include "infra/List.hpp"    // for TR_PersistentList
#include "infra/Monitor.hpp" // for createCounter race conditions

namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class SymbolReference; }

namespace TR
{

enum DebugCounterInjectionPoint
   {
   TR_BeforeCodegen,
   TR_BeforeRegAlloc,
   TR_AfterRegAlloc
   };

class DebugCounterBase
   {
public:
   virtual intptrj_t getBumpCountAddress() = 0;
   virtual TR::SymbolReference *getBumpCountSymRef(TR::Compilation *comp) = 0;
   TR::Node *createBumpCounterNode(TR::Compilation *comp, TR::Node *deltaNode);

   virtual void accumulate() = 0;
   };

class DebugCounter : public DebugCounterBase
   {
   friend class DebugCounterGroup;
   TR_ALLOC(TR_Memory::DebugCounter)

   uint64_t            _totalCount;
   const char         *_name;
   DebugCounter    *_denominator;
   uint32_t            _bumpCount;     // The counter to be incremented directly
   uint32_t            _bumpCountBase; // The last value of bumpCount that was accumulated into totalCount
   int8_t              _fidelity;      // (See the Fidelities enumeration)
   flags8_t            _flags;

   enum Flags
      {
      ContributesToDenominator = 0x01,   // Increments to _bumpCount are also added to _denominator->_totalCount
      IsDenominator = 0x02,
      };

   DebugCounter(const char *name, int8_t fidelity, DebugCounter *denominator=NULL, int8_t flags=0):
      _name(name), _fidelity(fidelity),_denominator(denominator), _flags(flags),
      _totalCount(0),_bumpCount(0),_bumpCountBase(0)
      {
      if (_denominator != NULL)
         {
         _denominator->_flags.set(IsDenominator);
         }
      }

   public:

   // These create printf-style formatted counters name efficiently.
   // Counter names must be either static strings or be allocated in persistent memory;
   // these functions will scan existing counter names and reuse them where possible
   // to avoid unnecessary allocation.
   //
   static const char *debugCounterName(TR::Compilation *comp, const char *format, ...);
   static const char *debugCounterBucketName(TR::Compilation *comp, int32_t value, const char *format, ...);

   // To do something manually with a counter, get it with this.
   // If static counters are enabled, this is what bumps them.
   // This bypasses the histogram facility.
   //
   static DebugCounter *getDebugCounter(TR::Compilation *comp, const char *name, int8_t fidelity, int32_t staticDelta);

   static void incStaticDebugCounter(TR::Compilation *comp, const char *name, int32_t staticDelta=1){ getDebugCounter(comp, name, DebugCounter::Free, staticDelta); }

   // Most users of debug counters will want to call this first function, or TR::CodeGenerator::generateDebugCounter.
   //
   static void prependDebugCounter(TR::Compilation *comp, const char *name, TR::TreeTop *nextTreeTop, int32_t delta=1, int8_t fidelity=DebugCounter::Undetermined)
      { prependDebugCounter(comp, name, nextTreeTop, delta, fidelity, delta); }
   static void prependDebugCounter(TR::Compilation *comp, const char *name, TR::TreeTop *nextTreeTop, int32_t delta, int8_t fidelity, int32_t staticDelta);

   static void prependDebugCounter(TR::Compilation *comp, const char *name, TR::TreeTop *nextTreeTop, TR::Node *deltaNode, int8_t fidelity=DebugCounter::Undetermined)
      { prependDebugCounter(comp, name, nextTreeTop, deltaNode, fidelity, 1); }
   static void prependDebugCounter(TR::Compilation *comp, const char *name, TR::TreeTop *nextTreeTop, TR::Node *deltaNode, int8_t fidelity, int32_t staticDelta);

   static void prependDebugCounterBump(TR::Compilation *comp, TR::TreeTop *nextTreeTop, DebugCounterBase *counter, int32_t delta);
   static void prependDebugCounterBump(TR::Compilation *comp, TR::TreeTop *nextTreeTop, DebugCounterBase *counter, TR::Node *deltaNode);



   enum Fidelities // Signal-to-noise ratio (SNR) in decibels
      {
      Free       = 30, // Time spent maintaining counters is undetectable
      Cheap      = 20, // " < 1%
      Moderate   = 10, // " < 10%   Highest fidelity without design approval
      Expensive  =  0, // " < 50%   (~2x slower)
      Exorbitant =-10, // " < 90%  (~10x slower)
      Punitive   =-20, // " < 99% (~100x slower)

      Undetermined = 0, // The default.  Use another value only if you have reason to believe it's justified.

      // Note that you can also use explicit numbers if you want.
      // These names are just here to make code self-documenting.
      // For instance, if your counter makes things run 3x slower (so the SNR
      // would be 1/2) you could use -3; or if it makes things run 1000x
      // slower, use -30.
      };

   // Queries
   //
   const char      *getName()                  { return _name; }
   int8_t           getFidelity()              { return _fidelity; }
   int64_t          getCount()                 { return _totalCount; }
   DebugCounter *getDenominator()           { return _denominator; }
   bool             isDenominator()            { return _flags.testAny(IsDenominator); }
   bool             contributesToDenominator() { return _flags.testAny(ContributesToDenominator); }

   void             getInsertionCounterNames(TR::Compilation *comp, TR::Node *node, const char *(&counterNames)[3]);

   TR::SymbolReference *getBumpCountSymRef(TR::Compilation *comp);
   intptrj_t getBumpCountAddress();

   // Commands
   //
   void increment(int64_t i) { accumulate(i); }

   void accumulate()
      {
      // This function is intended to be called from just one thread (the
      // sampling thread) that does not increment _bumpCount.  The idea is that
      // the fields written to by this thread are disjoint from those written
      // by the app thread, thereby eliminating race conditions between them.
      //
      // TODO: This is an n^2 algorithm; we should be able to do better
      //
      uint64_t count = _bumpCount;
      accumulate(count - _bumpCountBase);
      _bumpCountBase = count;
      }

   void accumulate(int64_t increment)
      {
      _totalCount += increment;
      if (contributesToDenominator())
         _denominator->accumulate(increment);
      }

   void reset()
      {
      _totalCount = 0;
      _bumpCountBase = _bumpCount;
      }

   };

// It's common for multiple related counter bumps to be adjacent to each other.
// DebugCounterAggregation provides a mechanism to allow those to be represented
// by a single actual bump in order to improve performance.
class DebugCounterAggregation : public DebugCounterBase
   {
   friend class DebugCounterGroup;

   struct CounterDelta
      {
      TR_ALLOC(TR_Memory::DebugCounter)
      CounterDelta(DebugCounter *counter, int32_t delta)
            : counter(counter), delta(delta) {}
      DebugCounter *counter;
      int32_t delta;
      };

   TR_Memory *_mem;
   TR::SymbolReference *_symRef;
   TR_PersistentList<CounterDelta> *_counterDeltas;
   int64_t _bumpCount;
   int64_t _lastBumpCount;

   void aggregateDebugCounterInsertions(TR::Compilation *comp, TR::Node *node, DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta);
   void aggregateDebugCounterHistogram (TR::Compilation *comp, TR::Node *node, DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta);

protected:
   DebugCounterAggregation(TR_Memory *mem)
         : _mem(mem), _counterDeltas(new (mem->trPersistentMemory()) TR_PersistentList<CounterDelta>()),
           _symRef(NULL), _bumpCount(0), _lastBumpCount(0){}
public:
   TR_ALLOC(TR_Memory::DebugCounter)

   void aggregate(DebugCounter *counter, int32_t delta);
   void aggregateStandardCounters(TR::Compilation *comp, TR::Node *node, const char *counterName, int32_t delta, int8_t fidelity, int32_t staticDelta);

   bool hasAnyCounters() { return !_counterDeltas->isEmpty(); }

   intptrj_t getBumpCountAddress();
   TR::SymbolReference *getBumpCountSymRef(TR::Compilation *comp);

   void accumulate();
   };

class DebugCounterGroup
   {
   CS2::HashTable<const char*, DebugCounter*, TRPersistentMemoryAllocator> _countersHashTable;
   TR_PersistentList<DebugCounter> _counters;
   TR_PersistentList<DebugCounterAggregation> _aggregations;
   DebugCounter *createCounter (const char *name, int8_t fidelity, TR_PersistentMemory *mem);
   DebugCounter *findCounter   (const char *name, int32_t nameLength);
   TR::Monitor *_countersMutex;

   friend class ::TR_Debug;

   public:
   TR_ALLOC(TR_MemoryBase::DebugCounter)

   DebugCounterGroup(TR_PersistentMemory *mem)
         : _countersHashTable(TRPersistentMemoryAllocator(mem))
         {
         _countersMutex = TR::Monitor::create("countersMutex");
         }

   const char *counterName(TR::Compilation *comp, const char *format, va_list args);

   DebugCounter *getCounter(TR::Compilation *comp, const char *name, int8_t fidelity=DebugCounter::Undetermined); // Returns NULL if counter is disabled

   DebugCounterAggregation *createAggregation(TR::Compilation *comp);

   void accumulate();
   void resetAll();

   enum SpecialCharacters
      {
      RATIO_SEPARATOR     = ':', // Counter has a denominator counter
      FRACTION_SEPARATOR  = '/', // Counter has a denominator counter and contributes to it
      NUMERIC_SEPARATOR   = '=', // Remainder of string sorted numerically rather than alphabetically
      HISTOGRAM_INDICATOR = '#', // Counter is suitable for producing a histogram
      VERBATIM_START      = '(', // Start of a 'verbatim' region, within which special characters will be ignored
      VERBATIM_STOP       = ')', // End of a 'verbatim' region; verbatim regions can be nested
      LOCATION_POINT      = '~', // Preferred place to insert code coordinate information
      };
   };

}

#endif
