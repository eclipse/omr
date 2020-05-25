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

#ifndef OMR_OPTIONS_INLINE_INCL
#define OMR_OPTIONS_INLINE_INCL

#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

/*
 * Performance sensitive implementations are included here
 * to support inlining.
 *
 * Every function in this file needs to be marked as inline
 * or else there will be multiple copies.
 */
inline TR::Options *
OMR::Options::self()
   {
   return static_cast<TR::Options*>(this);
   }

inline const  TR::Options *
OMR::Options::self() const
   {
   return static_cast<const TR::Options*>(this);
   }

inline bool
OMR::Options::getOption(uint32_t mask)
   {
   return self()->getAnyOption(mask);
   }

inline bool
OMR::Options::dynamicDebugCounterIsEnabled(const char *name, int8_t fidelity)
   {
   return self()->counterIsEnabled(name, fidelity, _enabledDynamicCounterNames);
   }

inline bool
OMR::Options::staticDebugCounterIsEnabled(const char *name, int8_t fidelity)
   {
   return self()->counterIsEnabled(name, fidelity, _enabledStaticCounterNames);
   }

inline bool
OMR::Options::debugCounterHistogramIsEnabled(const char *name, int8_t fidelity)
   {
   return self()->counterIsEnabled(name, fidelity, _counterHistogramNames);
   }


inline int32_t
OMR::Options::getInlinerArgumentHeuristicFraction() const
   {
   return self()->getOptLevel() > warm ?
        _inlinerArgumentHeuristicFractionBeyondWarm
      : _inlinerArgumentHeuristicFractionUpToWarm;
   }

inline int32_t
OMR::Options::getNumLimitedGRARegsWithheld()
      {
      int32_t regsToWithhold = TR_MAX_AVAIL_LIMITED_GRA_REGS - self()->getMaxLimitedGRARegs();
      if (regsToWithhold < 0)
         return 0;
      else
         return regsToWithhold;
      }

inline bool
OMR::Options::isDisabledForAllMethods (OMR::Optimizations o)
   {
   return TR::Options::checkDisableFlagForAllMethods(o, false);
   }

inline bool
OMR::Options::isDisabledForAnyMethod (OMR::Optimizations o)
   {
   return TR::Options::checkDisableFlagForAllMethods(o, true);
   }

inline bool
OMR::Options::isAnyVerboseOptionSet(TR_VerboseFlags op1)
   {
   return TR::Options::getVerboseOption(op1);
   }

inline bool
OMR::Options::isAnyVerboseOptionSet(TR_VerboseFlags op1, TR_VerboseFlags op2)
   {
   return TR::Options::getVerboseOption(op1) || TR::Options::getVerboseOption(op2);
   }

inline bool
OMR::Options::isAnyVerboseOptionSet(TR_VerboseFlags op1, TR_VerboseFlags op2, TR_VerboseFlags op3)
   {
   return TR::Options::isAnyVerboseOptionSet(op1,op3) || TR::Options::getVerboseOption(op2);
   }

inline bool
OMR::Options::isAnyVerboseOptionSet(TR_VerboseFlags op1, TR_VerboseFlags op2, TR_VerboseFlags op3, TR_VerboseFlags op4)
   {
   return TR::Options::isAnyVerboseOptionSet(op1,op3) || TR::Options::isAnyVerboseOptionSet(op2,op4);
   }

inline bool
OMR::Options::isAnyVerboseOptionSet(TR_VerboseFlags op1, TR_VerboseFlags op2, TR_VerboseFlags op3, TR_VerboseFlags op4, TR_VerboseFlags op5)
   {
   return TR::Options::isAnyVerboseOptionSet(op1,op3,op5) || TR::Options::isAnyVerboseOptionSet(op2,op4);
   }

inline bool
OMR::Options::isAnyVerboseOptionSet(TR_VerboseFlags op1, TR_VerboseFlags op2, TR_VerboseFlags op3, TR_VerboseFlags op4, TR_VerboseFlags op5, TR_VerboseFlags op6)
   {
   return TR::Options::isAnyVerboseOptionSet(op1,op3,op5) || TR::Options::isAnyVerboseOptionSet(op2,op4,op6);
   }

inline void
OMR::Options::setCanJITCompile(bool f)
   {
   _canJITCompile = f;
   if (!f) TR::Options::disableSamplingThread();
   }

inline TR_Debug *
OMR::Options::findOrCreateDebug()
   {
   if (!_debug) TR::Options::createDebug();
   return _debug;
   }

#endif
