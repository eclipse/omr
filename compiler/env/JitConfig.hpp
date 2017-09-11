/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef JITCONFIG_HPP_BThFwv
#define JITCONFIG_HPP_BThFwv

#include <stddef.h>
#include <stdint.h>
#include "env/IO.hpp"

namespace TR
{

class JitConfig
   {
   protected:
   JitConfig();

   public:

   static JitConfig *instance();

   // possibly temporary place for options to be stored?
   struct
      {
      int32_t       codeCacheKB;
      char        * vLogFileName;
      TR::FILE    * vLogFile;
      uint64_t      verboseFlags;
      } options;

   void *getProcessorInfo() { return _processorInfo; }
   void setProcessorInfo(void *buf) { _processorInfo = buf; }

   bool isCodeCacheFull() { return _codeCacheFull; }

   void setInterpreterTOC(size_t interpreterTOC) { _interpreterTOC = interpreterTOC; }
   size_t getInterpreterTOC()                    { return _interpreterTOC; }

   void *getPseudoTOC()               { return _pseudoTOC; }
   void setPseudoTOC(void *pseudoTOC) { _pseudoTOC = pseudoTOC; }

   private:
   char                        _eyecatcher[8];

   void                      * _processorInfo;

   bool                        _codeCacheFull;

   size_t                      _interpreterTOC;

   void                      * _pseudoTOC; // only used on POWER, otherwise should be NULL
   };

} /* namespace TR */

#endif /* JITCONFIG_HPP_BThFwv */
