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

#include <stdint.h>                      // for int32_t, uint8_t
#include "compile/CompilationTypes.hpp"  // for TR_Hotness
#include "env/ConcreteFE.hpp"            // for FrontEnd
#include "env/SystemSegmentProvider.hpp"

struct OMR_VMThread;
class TR_ResolvedMethod;
class TR_FilterBST;
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class JitConfig; }

int32_t init_options(TR::JitConfig *jitConfig, char * cmdLineOptions);
int32_t commonJitInit(OMR::FrontEnd &fe, char * cmdLineOptions);
uint8_t *compileMethod(OMR_VMThread *omrVMThread, TR_ResolvedMethod &compilee, TR_Hotness hotness, int32_t &rc);
uint8_t *compileMethodFromDetails(OMR_VMThread *omrVMThread, TR::IlGeneratorMethodDetails &details, TR_Hotness hotness, int32_t &rc);


/** 
 * If anyone objects to the compilation of this method, 
 * let them speak now or forever hold their peace. 
 *
 * @returns true iff the method can be compiled. 
 */
bool methodCanBeCompiled(OMR::FrontEnd *fe,
                         TR_ResolvedMethod &method,
                         TR_FilterBST *&filter,
                         TR_Memory *trMemory);

/**
 * Try and compile a method. 
 *
 * If successfull, the address of the start of the method will be returned, otherwise,
 * the return-code will indicate the failure reason. 
 */
uint8_t*
tryAndCompileCompilation(TR::Compilation& compiler,
                         TR_ResolvedMethod& compilee,
                         TR::Options* options,
                         TR_Memory& trMemory,
                         TR::SegmentAllocator &scratchSegmentProvider,
                         uint64_t translationStartTime,
                         int32_t& rc);
