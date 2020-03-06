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

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/CompileMethod.hpp"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ConcreteFE.hpp"
#include "env/FEBase.hpp"
#include "env/IO.hpp"
#include "env/JitConfig.hpp"
#include "env/jittypes.h"
#include "compile/CompilationException.hpp"
#include "il/ILOps.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

TR::FECommon::FECommon()
   : TR_FrontEnd()
   {}


TR_Debug *
TR::FECommon::createDebug( TR::Compilation *comp)
   {
   return createDebugObject(comp);
   }

extern "C" {

// use libc for all this stuff


void TR_VerboseLog::vlogAcquire()
   {
   //NOT IMPLEMENTED for now
   }

void TR_VerboseLog::vlogRelease()
   {
   //NOT IMPLEMENTED for now
   }

void TR_VerboseLog::vwrite(const char *format, va_list args)
   {
   // not implemented yet
   TR::FILE *fileID = TR::JitConfig::instance()->options.vLogFile;
   if (fileID != NULL)
      vfprintf((::FILE*) fileID, format, args);
   else
      vprintf(format, args);
   }

void TR_VerboseLog::writeTimeStamp()
   {
   //NOT IMPLEMENTED for now
   }

}


#undef getenv
char *feGetEnv(const char *s)
   {
   return getenv(s);
   }


// Options stuff
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"


TR::OptionTable OMR::Options::_feOptions[] =
   {
   {"code=",              "C<nnn>\tsize of a single code cache, in KB",
    TR::Options::set32BitNumericInJitConfig, offsetof(OMR::FrontEnd::JitConfig, options.codeCacheKB), 0, " %d (KB)"},
   {"exclude=",   "D{regexp}\tdo not compile methods matching regexp", TR::Options::limitOption, 1, 0, "P%s"},
   {"limit=",     "D{regexp}\tonly compile methods matching regexp", TR::Options::limitOption, 0, 0, "P%s"},
   {"limitfile=", "D<filename>\tfilter methods as defined in filename.  "
                  "Use limitfile=(filename,firstLine,lastLine) to limit lines considered from firstLine to lastLine", TR::Options::limitfileOption, 0, 0, "P%s"},
   {"verbose",    "L\twrite compiled method names to vlog file or stdout in limitfile format",
    TR::Options::setVerboseBits, offsetof(TR::JitConfig, options.verboseFlags), (1<<TR_VerboseCompileStart)|(1<<TR_VerboseCompileEnd), "F=1"},
   {"verbose=",   "L{regex}\tlist of verbose output to write to vlog or stdout",
    TR::Options::setVerboseBits, offsetof(TR::JitConfig, options.verboseFlags), 0, "F"},
   {"version",     "M\tprint out JIT version", TR::Options::versionOption, 0, 0, "F"},
   {"vlog=",      "L<filename>\twrite verbose output to filename",
    TR::Options::setStringInJitConfig, offsetof(TR::JitConfig, options.vLogFileName), 0, "P%s"},
   {0}
   };


#include "control/Recompilation.hpp"


// S390 specific fucntion - FIXME: make this only be a problem when HOST is s390.  Also, use a better
// name for this
void setDllSlip(char*CodeStart,char*CodeEnd,char*dllName,  TR::Compilation *comp) { TR_UNIMPLEMENTED(); }

// runtime assumptions
#ifdef J9_PROJECT_SPECIFIC
// FIXME:
#include "runtime/RuntimeAssumptions.hpp"
void TR::PatchNOPedGuardSite::compensate(TR_FrontEnd *fe, bool isSMP, uint8_t *location, uint8_t *destination) { TR_UNIMPLEMENTED(); }
void TR_PersistentClassInfo::removeASubClass(TR_PersistentClassInfo *) { TR_UNIMPLEMENTED(); }
bool isOrderedPair(uint8_t recordType) { TR_UNIMPLEMENTED(); return false; }
void OMR::RuntimeAssumption::addToRAT(TR_PersistentMemory * persistentMemory, TR_RuntimeAssumptionKind kind, TR_FrontEnd *fe, OMR::RuntimeAssumption** sentinel) { TR_UNIMPLEMENTED(); }
void OMR::RuntimeAssumption::dumpInfo(char *subclassName) { TR_UNIMPLEMENTED(); }
void TR_PatchJNICallSite::compensate(TR_FrontEnd*, bool, void *) { TR_UNIMPLEMENTED(); }
void TR_PreXRecompile::compensate(TR_FrontEnd*, bool, void *) { TR_UNIMPLEMENTED(); }
TR_PatchNOPedGuardSiteOnClassPreInitialize *TR_PatchNOPedGuardSiteOnClassPreInitialize::make(TR_FrontEnd *fe, TR_PersistentMemory *, char*, unsigned int, unsigned char*, unsigned char*, OMR::RuntimeAssumption**) { TR_UNIMPLEMENTED(); return 0; }
#endif


#if defined(LINUX)
/* Hack - to satisfy the runtime linker we need a definition of the following functions.
   Should never be called.  This code can be removed when we permit the JIT to be linked
   against libstdc++ on Linux.
*/
extern "C" {
void __pure_virtual()
   {
   TR_ASSERT(0, "Error: Pure Virtual Function called");
   }
void __cxa_pure_virtual()
   {
   __pure_virtual();
   }
}
#endif


void TR_Debug::print(J9JITExceptionTable *, TR_ResolvedMethod *, bool) { }
void TR_Debug::printAnnotationInfoEntry(J9AnnotationInfo *,J9AnnotationInfoEntry *,int32_t) { }
void TR_Debug::printByteCodeAnnotations() { }
void TR_Debug::printByteCodeStack(int32_t, uint16_t, char *) { }
