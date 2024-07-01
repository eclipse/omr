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

#ifndef DEBUGCOUNTER_INCL
#define DEBUGCOUNTER_INCL

#include "ras/Debug.hpp"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "cs2/hashtab.h"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "infra/Monitor.hpp"

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

enum DebugCounterType
   {
   NORMAL,
   AGGREGATE
   };

struct DebugCounterReloData
   {
   TR_ALLOC(TR_Memory::DebugCounter)
   DebugCounterReloData(int32_t delta, int8_t fidelity, int32_t staticDelta)
      : _seqKind(0), _fidelity(fidelity), _callerIndex(-1), _bytecodeIndex(0),
        _delta(delta), _staticDelta(staticDelta)
      {}
   uint8_t              _seqKind;
   int8_t               _fidelity;
   int16_t              _callerIndex;
   int32_t              _bytecodeIndex;
   int32_t              _delta;
   int32_t              _staticDelta;
   };

class DebugCounterBase
   {
public:
   DebugCounterBase(const char *name) : _name(name), _reloData(NULL) {}
   virtual intptr_t getBumpCountAddress() = 0;
   virtual TR::SymbolReference *getBumpCountSymRef(TR::Compilation *comp) = 0;
   TR::Node *createBumpCounterNode(TR::Compilation *comp, TR::Node *deltaNode);
   bool initializeReloData(TR::Compilation *comp, int32_t delta, int8_t fidelity, int32_t staticDelta);
   void finalizeReloData(TR::Compilation *comp, TR::Node *node, uint8_t seqKind=0);

   virtual void accumulate() = 0;

   const char *getName() { return _name; }

   DebugCounterReloData *getReloData() { return _reloData; }

protected:
   const char           *_name;
   DebugCounterReloData *_reloData;
   };

class DebugCounter : public DebugCounterBase
   {
   friend class DebugCounterGroup;
   TR_ALLOC(TR_Memory::DebugCounter)

   uint64_t            _totalCount;
   DebugCounter       *_denominator;
   uint64_t            _bumpCount;     // The counter to be incremented directly
   uint64_t            _bumpCountBase; // The last value of bumpCount that was accumulated into totalCount
   int8_t              _fidelity;      // (See the Fidelities enumeration)
   flags8_t            _flags;

   enum Flags
      {
      ContributesToDenominator = 0x01,   // Increments to _bumpCount are also added to _denominator->_totalCount
      IsDenominator = 0x02,
      };

   DebugCounter(const char *name, int8_t fidelity, DebugCounter *denominator=NULL, int8_t flags=0):
      DebugCounterBase(name),
      _fidelity(fidelity),_denominator(denominator), _flags(flags),
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

   static void generateRelocation(TR::Compilation *comp, uint8_t *location, TR::Node *node, TR::DebugCounterBase *counter);
   static void generateRelocation(TR::Compilation *comp, TR::Instruction *firstInstruction, TR::Node *node, TR::DebugCounterBase *counter, uint8_t seqKind=0);
   static void generateRelocation(TR::Compilation *comp, TR::Instruction *firstInstruction, TR::Instruction *secondInstruction, TR::Node *node, TR::DebugCounterBase *counter, uint8_t seqKind=0);

   static bool relocatableDebugCounter(TR::Compilation *comp);

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
   int8_t           getFidelity()              { return _fidelity; }
   /** On 32-bit platforms, dereference the address as uint32 pointer, since we use 32-bit operations*/
   int64_t          getCount()                 { return TR::Compiler->target.is64Bit() ? _totalCount : *(reinterpret_cast<uint32_t*>(&_totalCount)); }
   DebugCounter *getDenominator()           { return _denominator; }
   bool             isDenominator()            { return _flags.testAny(IsDenominator); }
   bool             contributesToDenominator() { return _flags.testAny(ContributesToDenominator); }

   void             getInsertionCounterNames(TR::Compilation *comp, TR_OpaqueMethodBlock *method, int32_t bytecodeIndex, const char *(&counterNames)[3]);

   TR::SymbolReference *getBumpCountSymRef(TR::Compilation *comp);
   intptr_t getBumpCountAddress();

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

   TR_PersistentMemory *_mem;
   TR::SymbolReference *_symRef;
   TR_PersistentList<CounterDelta> *_counterDeltas;
   int64_t _bumpCount;
   int64_t _lastBumpCount;

   void aggregateDebugCounterInsertions(TR::Compilation *comp, TR_OpaqueMethodBlock *method, int32_t bytecodeIndex, TR::DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta);
   void aggregateDebugCounterHistogram (TR::Compilation *comp, DebugCounter *counter, int32_t delta, int8_t fidelity, int32_t staticDelta);

protected:
   DebugCounterAggregation(const char * name, TR_PersistentMemory *mem)
         : DebugCounterBase(name),
           _mem(mem), _counterDeltas(new (mem) TR_PersistentList<CounterDelta>()),
           _symRef(NULL), _bumpCount(0), _lastBumpCount(0) {}
public:
   TR_ALLOC(TR_Memory::DebugCounter)

   void aggregate(DebugCounter *counter, int32_t delta);
   void aggregateStandardCounters(TR::Compilation *comp, TR::Node *node, const char *counterName, int32_t delta, int8_t fidelity, int32_t staticDelta);
   void aggregateStandardCounters(TR::Compilation *comp, TR_OpaqueMethodBlock *method, int32_t bytecodeIndex, const char *counterName, int32_t delta, int8_t fidelity, int32_t staticDelta);

   bool hasAnyCounters() { return !_counterDeltas->isEmpty(); }

   intptr_t getBumpCountAddress();
   TR::SymbolReference *getBumpCountSymRef(TR::Compilation *comp);

   void accumulate();
   int64_t getCount();
   void printCounters(bool printZeroCounters = true);
   };

class DebugCounterGroup
   {
   CS2::HashTable<const char*, DebugCounter*, TRPersistentMemoryAllocator> _countersHashTable;
   CS2::HashTable<const char*, DebugCounterAggregation*, TRPersistentMemoryAllocator> _aggregateCountersHashTable;
   TR_PersistentList<DebugCounter> _counters;
   TR_PersistentList<DebugCounterAggregation> _aggregations;
   DebugCounter *createCounter (const char *name, int8_t fidelity, TR_PersistentMemory *mem);
   DebugCounter *findCounter   (const char *name, int32_t nameLength);
   TR::Monitor *_countersMutex; /**< Monitor used to synchronize read/write actions to _countersHashTable, otherwise we may have a race */

   friend class ::TR_Debug;

   public:
   TR_ALLOC(TR_MemoryBase::DebugCounter)

   DebugCounterGroup(TR_PersistentMemory *mem)
         : _countersHashTable(TRPersistentMemoryAllocator(mem)), _aggregateCountersHashTable(TRPersistentMemoryAllocator(mem))
         {
         _countersMutex = TR::Monitor::create("countersMutex");
         }

   const char *counterName(TR::Compilation *comp, const char *format, va_list args);

   DebugCounter *getCounter(TR::Compilation *comp, const char *name, int8_t fidelity=DebugCounter::Undetermined); // Returns NULL if counter is disabled

   DebugCounterAggregation *createAggregation(TR::Compilation *comp, const char * name);
   DebugCounterAggregation * findAggregation(const char *nameChars, int32_t nameLength);

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
