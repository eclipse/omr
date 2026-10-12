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

#ifndef OMR_FRONTEND_INCL
#define OMR_FRONTEND_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_FRONTEND_CONNECTOR
#define OMR_FRONTEND_CONNECTOR

namespace OMR {
class FrontEnd;
typedef OMR::FrontEnd FrontEndConnector;
} // namespace OMR
#endif

#include "compile/CompilationTypes.hpp"
#include "compiler/env/TRFrontEnd.hpp"
#include "env/CompilerEnv.hpp"
#include "env/JitConfig.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"
#include "runtime/CodeCache.hpp"

namespace TR {
class CodeCacheManager;
class FrontEnd;
} // namespace TR
class TR_ResolvedMethod;

namespace OMR {

class OMR_EXTENSIBLE FrontEnd : public ::TR_FrontEnd {
public:
    FrontEnd();
    virtual ~FrontEnd();

    static TR::FrontEnd *instance();

    virtual TR_Debug *createDebug(TR::Compilation *comp = NULL);

    virtual TR_OpaqueClassBlock *getClassFromSignature(const char *sig, int32_t length, TR_ResolvedMethod *method,
        bool isVettedForAOT = false)
    {
        return NULL;
    }

    virtual TR_OpaqueClassBlock *getClassFromSignature(const char *sig, int32_t length, TR_OpaqueMethodBlock *method,
        bool isVettedForAOT = false)
    {
        return NULL;
    }

    virtual const char *sampleSignature(TR_OpaqueMethodBlock *aMethod, char *buf, int32_t bufLen, TR_Memory *memory)
    {
        return NULL;
    }

    // need this so z codegen can create a sym ref to compare to another sym ref it cannot possibly be equal to
    virtual uintptr_t getOffsetOfIndexableSizeField() { return -1; }

    TR::JitConfig *jitConfig() { return &_config; }

    TR::CodeCacheManager &codeCacheManager() { return *_codeCacheManager; }

    virtual uint8_t *allocateRelocationData(TR::Compilation *comp, uint32_t size);

    virtual TR_PersistentMemory *persistentMemory() { return &_persistentMemory; }

    virtual TR::PersistentInfo *getPersistentInfo() { return _persistentMemory.getPersistentInfo(); }

    virtual void reserveTrampolineIfNecessary(TR::Compilation *comp, TR::SymbolReference *symRef,
        bool inBinaryEncoding);

    virtual intptr_t methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef, void *callSite);

    TR_ResolvedMethod *createResolvedMethod(TR_Memory *trMemory, TR_OpaqueMethodBlock *aMethod,
        TR_ResolvedMethod *owningMethod, TR_OpaqueClassBlock *classForNewInstance);

private:
    TR::JitConfig _config;
    TR::CodeCacheManager *_codeCacheManager;

    // this is deprecated in favour of TR::Allocator
    TR_PersistentMemory _persistentMemory; // global memory

    static TR::FrontEnd *_instance;
};

} // namespace OMR

#endif
