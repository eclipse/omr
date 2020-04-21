/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef DEBUGGING_COUNTERS_INCL
#define DEBUGGING_COUNTERS_INCL

#include <stdint.h>
#include <stdio.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"

namespace TR { class Compilation; }
namespace TR { class Method; }
namespace TR { class TreeTop; }

// This is the type of the debugging counters.
#define SMALL_COUNTER_TYPE uint32_t
#define LARGE_COUNTER_TYPE uint64_t

/**
 * Simple list to track already instrumented call sites.
 */
struct CountedCallSite
  {
  TR::TreeTop * callTreeTop;
  char name[64];
  bool flag;
  int32_t size;
  int32_t frequency;
  int32_t numLocals;
  TR_OpaqueMethodBlock *opaque_method;
  TR::Method *method;
  TR_ByteCodeInfo bcInfo;
  CountedCallSite * _next;
  };

/**
 *  Structure to hold all data for a given named counter.
 */
struct NamedCounterInfo
  {
  // Named of this counter
  char * counterName;

  // A small counter that is incremented without being corrupted by
  // races (may drop samples)
  SMALL_COUNTER_TYPE smallCount;

  // A large counter that won't overflow.  Updated periodically from
  // the interval count
  LARGE_COUNTER_TYPE totalCount;

  // Counter to track how many different locations in the code (at
  // compile time) increment this counter.
  SMALL_COUNTER_TYPE compilationCount;

  // Linked list of infos
  NamedCounterInfo * _next;

  int32_t delta;

  int32_t bucketSize;
  };

/**
 *  Class to contain functionality specific to debugging counters.
 *  For now, all methods and data are currently static.  Eventually
 *  the data could probably become instance-specific to support reentrant
 *  compilation.
 */
class TR_DebuggingCounters
  {
  public:
  TR_ALLOC(TR_Memory::TR_DebuggingCounters)
  static void initializeCompilation();
  static void transferSmallCountsToTotalCounts();
  static void report();

  private:
  // Linked list of counter infos
  static NamedCounterInfo * namedCounterInfos;

  // Methods
  static NamedCounterInfo* getOrCreateNamedCounter(TR::Compilation *comp, const char * name, int32_t d, int32_t bucketSize);
  static void insertCounter(const char * name, TR::Compilation * comp, TR::TreeTop * tt, int32_t d);
  static bool insertIfMissing(TR::Compilation * comp, const char *name, bool flag, TR::TreeTop * tt, int32_t size = 0, int32_t numLocals=0);

  /*
   *  The routines below are specific to inlining counters, but use
   *  the generic debugging counters above as the implementation
   *
   *  This could be done through inheritance, but it doesn't seem worth it at this point.
   */
  public:
  static void insertInliningCounter(const char * name, TR::Compilation * comp, TR::TreeTop * tt, int32_t size = 0, int32_t delta = 0, int32_t numLocals=0);
  static void insertInliningCounterOK(const char * name, TR::Compilation * comp, TR::TreeTop *origTT, TR::TreeTop * tt, int32_t size = 0, int32_t numLocals=0);
  static void inliningReportForMethod(TR::Compilation *comp);

  private:
  static CountedCallSite * countedCallSiteList;
  static FILE *output;
  };

#endif
