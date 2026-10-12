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

#include <stddef.h>
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/CompileMethod.hpp"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/FrontEnd.hpp"
#include "env/JitConfig.hpp"
#include "env/VerboseLog.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"
#include "ras/Logger.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/SymbolReference.hpp"

#if defined(OMR_OS_WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#endif /* OMR_OS_WINDOWS */

TR::FrontEnd *OMR::FrontEnd::_instance = NULL;

OMR::FrontEnd::FrontEnd()
    : ::TR_FrontEnd()
    , _config()
    , _codeCacheManager(NULL)
    , _persistentMemory(jitConfig(), TR::Compiler->persistentAllocator())
{
    TR_ASSERT_FATAL(!_instance, "FrontEnd must be initialized only once");
    _instance = static_cast<TR::FrontEnd *>(this);
    ::trPersistentMemory = &_persistentMemory;

    TR::CodeCacheManager *m = reinterpret_cast<TR::CodeCacheManager *>(
        _persistentMemory.allocatePersistentMemory(sizeof(TR::CodeCacheManager)));
    _codeCacheManager = new (m) TR::CodeCacheManager(TR::Compiler->rawAllocator);
}

OMR::FrontEnd::~FrontEnd()
{
    _persistentMemory.freePersistentMemory(_codeCacheManager);
    _codeCacheManager = NULL;
}

TR::FrontEnd *OMR::FrontEnd::instance()
{
    TR_ASSERT_FATAL(_instance, "FrontEnd not initialized");
    return _instance;
}

TR_Debug *OMR::FrontEnd::createDebug(TR::Compilation *comp) { return createDebugObject(comp); }

void OMR::FrontEnd::reserveTrampolineIfNecessary(TR::Compilation *comp, TR::SymbolReference *symRef,
    bool inBinaryEncoding)
{
    TR_OpaqueMethodBlock *method
        = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier();

    auto err = comp->cg()->getCodeCache()->reserveResolvedTrampoline(method, inBinaryEncoding);

    TR_ASSERT(err == CodeCacheErrorCode::ERRORCODE_SUCCESS, "Failed to reserve trampoline!");
}

intptr_t OMR::FrontEnd::methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef, void *callSite)
{
    TR_ASSERT(!symRef->isUnresolved(), "No need to lookup trampolines for unresolved methods.\n");

    reserveTrampolineIfNecessary(comp, symRef, false);

    TR_OpaqueMethodBlock *method
        = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier();

    OMR::CodeCacheTrampolineCode *trampoline = TR::CodeCacheManager::instance()->findMethodTrampoline(method, callSite);

    TR_ASSERT(trampoline != nullptr, "It should not fail since it is reserved first.\n");

    return reinterpret_cast<intptr_t>(trampoline);
}

TR_ResolvedMethod *OMR::FrontEnd::createResolvedMethod(TR_Memory *trMemory, TR_OpaqueMethodBlock *aMethod,
    TR_ResolvedMethod *owningMethod, TR_OpaqueClassBlock *classForNewInstance)
{
    return new (trMemory->trHeapMemory()) TR::ResolvedMethod(aMethod);
}

// This code does not really belong here (along with allocateRelocationData, really)
// We should be relying on the port library to allocate memory, but this connection
// has not yet been made, so as a quick workaround for platforms like OS X <= 10.9,
// where MAP_ANONYMOUS is not defined, is to map MAP_ANON to MAP_ANONYMOUS ourselves
#if defined(__APPLE__)
#if !defined(MAP_ANONYMOUS)
#define NO_MAP_ANONYMOUS
#if defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#else
#error unexpectedly, no MAP_ANONYMOUS or MAP_ANON definition
#endif
#endif
#endif /* defined(__APPLE__) */

uint8_t *OMR::FrontEnd::allocateRelocationData(TR::Compilation *comp, uint32_t size)
{
    /* FIXME: using an mmap without much thought into whether that is the best
       way to allocate this */
    if (size == 0)
        return 0;
    TR_ASSERT(size >= 2048, "allocateRelocationData should be used for whole-sale memory allocation only");

#if defined(OMR_OS_WINDOWS)
    return reinterpret_cast<uint8_t *>(VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE));
// TODO: Why is there no OMR_OS_ZOS? Or any other OS for that matter?
#elif defined(J9ZOS390)
    // TODO: This is an absolute hack to get z/OS JITBuilder building and even remotely close to working. We really
    // ought to be using the port library to allocate such memory. This was the quickest "workaround" I could think
    // of to just get us off the ground.
    return reinterpret_cast<uint8_t *>(__malloc31(size));
#else
    return reinterpret_cast<uint8_t *>(mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
#endif /* OMR_OS_WINDOWS */
}

// keep the impact of this fix localized
#if defined(NO_MAP_ANONYMOUS)
#undef MAP_ANONYMOUS
#undef NO_MAP_ANONYMOUS
#endif

extern "C" {

// use libc for all this stuff

void TR_VerboseLog::vlogAcquire()
{
    // NOT IMPLEMENTED for now
}

void TR_VerboseLog::vlogRelease()
{
    // NOT IMPLEMENTED for now
}

void TR_VerboseLog::vwrite(const char *format, va_list args)
{
    // not implemented yet
    TR::FILE *fileID = TR::JitConfig::instance()->options.vLogFile;
    if (fileID != NULL)
        vfprintf((::FILE *)fileID, format, args);
    else
        vprintf(format, args);
}

void TR_VerboseLog::writeTimeStamp()
{
    // NOT IMPLEMENTED for now
}
}

#undef getenv

char *feGetEnv(const char *s) { return getenv(s); }

#if defined(LINUX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#elif defined(OSX)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#elif defined(AIXPPC) || defined(J9ZOS390)
#pragma report(disable, "CCN6281")
#endif

TR::OptionTable OMR::Options::_feOptions[] = {
    { "code=", "C<nnn>\tsize of a single code cache, in KB", TR::Options::set32BitNumericInJitConfig,
     offsetof(TR::JitConfig, options.codeCacheKB), 0, " %d (KB)" },
    { "exclude=", "D{regexp}\tdo not compile methods matching regexp", TR::Options::limitOption, 1, 0, "P%s" },
    { "limit=", "D{regexp}\tonly compile methods matching regexp", TR::Options::limitOption, 0, 0, "P%s" },
    { "limitfile=",
     "D<filename>\tfilter methods as defined in filename.  "
        "Use limitfile=(filename,firstLine,lastLine) to limit lines considered from firstLine to lastLine", TR::Options::limitfileOption, 0, 0, "P%s" },
    { "verbose", "L\twrite compiled method names to vlog file or stdout in limitfile format",
     TR::Options::setVerboseBits, offsetof(TR::JitConfig, options.verboseFlags),
     (1 << TR_VerboseCompileStart) | (1 << TR_VerboseCompileEnd), "F=1" },
    { "verbose=", "L{regex}\tlist of verbose output to write to vlog or stdout", TR::Options::setVerboseBits,
     offsetof(TR::JitConfig, options.verboseFlags), 0, "F" },
    { "version", "M\tprint out JIT version", TR::Options::versionOption, 0, 0, "F" },
    { "vlog=", "L<filename>\twrite verbose output to filename", TR::Options::setStringInJitConfig,
     offsetof(TR::JitConfig, options.vLogFileName), 0, "P%s" },
    { 0 }
};

#if defined(LINUX)
#pragma GCC diagnostic pop
#elif defined(OSX)
#pragma clang diagnostic pop
#elif defined(AIXPPC) || defined(J9ZOS390)
#pragma report(enable, "CCN6281")
#endif

#include "control/Recompilation.hpp"

// S390 specific function - FIXME: make this only be a problem when HOST is s390.  Also, use a better
// name for this
void setDllSlip(const char *CodeStart, const char *CodeEnd, const char *dllName, TR::Compilation *comp)
{
    TR_UNIMPLEMENTED();
}

// runtime assumptions
#ifdef J9_PROJECT_SPECIFIC
// FIXME:
#include "runtime/RuntimeAssumptions.hpp"

void TR::PatchNOPedGuardSite::compensate(TR_FrontEnd *fe, bool isSMP, uint8_t *location, uint8_t *destination)
{
    TR_UNIMPLEMENTED();
}

void TR_PersistentClassInfo::removeASubClass(TR_PersistentClassInfo *) { TR_UNIMPLEMENTED(); }

bool isOrderedPair(uint8_t recordType)
{
    TR_UNIMPLEMENTED();
    return false;
}

void OMR::RuntimeAssumption::addToRAT(TR_PersistentMemory *persistentMemory, TR_RuntimeAssumptionKind kind,
    TR_FrontEnd *fe, OMR::RuntimeAssumption **sentinel)
{
    TR_UNIMPLEMENTED();
}

void OMR::RuntimeAssumption::dumpInfo(const char *subclassName) { TR_UNIMPLEMENTED(); }

void TR_PatchJNICallSite::compensate(TR_FrontEnd *, bool, void *) { TR_UNIMPLEMENTED(); }

void TR_PreXRecompile::compensate(TR_FrontEnd *, bool, void *) { TR_UNIMPLEMENTED(); }

TR_PatchNOPedGuardSiteOnClassPreInitialize *TR_PatchNOPedGuardSiteOnClassPreInitialize::make(TR_FrontEnd *fe,
    TR_PersistentMemory *, char *, unsigned int, unsigned char *, unsigned char *, OMR::RuntimeAssumption **)
{
    TR_UNIMPLEMENTED();
    return 0;
}
#endif

#if defined(LINUX)
/* Hack - to satisfy the runtime linker we need a definition of the following functions.
   Should never be called.  This code can be removed when we permit the JIT to be linked
   against libstdc++ on Linux.
*/
extern "C" {
void __pure_virtual() { TR_ASSERT(0, "Error: Pure Virtual Function called"); }

void __cxa_pure_virtual() { __pure_virtual(); }
}
#endif

void TR_Debug::print(OMR::Logger *log, J9JITExceptionTable *, TR_ResolvedMethod *, bool) {}

void TR_Debug::printAnnotationInfoEntry(OMR::Logger *log, J9AnnotationInfo *, J9AnnotationInfoEntry *, int32_t) {}

void TR_Debug::printByteCodeAnnotations(OMR::Logger *log) {}

void TR_Debug::printByteCodeStack(OMR::Logger *log, int32_t, uint16_t, size_t *) {}
