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
 ******************************************************************************/

#include <stddef.h>
#include <unistd.h> // for getpid
#include <sys/time.h>
#include <sys/types.h> // for getpid
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
#include "il/OMRILOps.hpp"
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


//                 .o.                          oooo
//                .888.                         `888
//  oooooooo     .8"888.     oooo d8b  .ooooo.   888 .oo.
// d'""7d8P     .8' `888.    `888""8P d88' `"Y8  888P"Y88b
//   .d8P'     .88ooo8888.    888     888        888   888
// .d8P'  .P  .8'     `888.   888     888   .o8  888   888
//d8888888P  o88o     o8888o d888b    `Y8bod8P' o888o o888o

/* Keep in sync with enum TR_S390MachineType in env/Processors.hpp */
static const int S390MachineTypes[] =
   {
   TR_FREEWAY, TR_Z800, TR_MIRAGE, TR_MIRAGE2, TR_TREX, TR_Z890, TR_GOLDEN_EAGLE, TR_DANU_GA2, TR_Z9BC,
   TR_Z10, TR_Z10BC, TR_ZG, TR_ZGMR, TR_ZEC12, TR_ZEC12MR, TR_ZG_RESERVE, TR_ZEC12_RESERVE,
   TR_Z13, TR_Z13s, TR_ZNEXT, TR_ZNEXTs,
   TR_ZH, TR_DATAPOWER, TR_ZH_RESERVE1, TR_ZH_RESERVE2
   };

static const int S390UnsupportedMachineTypes[] =
   {
   TR_G5, TR_MULTIPRISE7000
   };

static TR_S390MachineType
portLib_get390zLinuxMachineType()
   {
   char line[80];
   const int LINE_SIZE = sizeof(line) - 1;
   const char procHeader[] = "processor ";
   const int PROC_LINE_SIZE = 69;
   const int PROC_HEADER_SIZE = sizeof(procHeader) - 1;
   TR_S390MachineType ret_machine = TR_UNDEFINED_S390_MACHINE;  /* return value */

   ::FILE * fp = fopen("/proc/cpuinfo", "r");
   if (fp)
      {
      while (fgets(line, LINE_SIZE, fp) > 0)
         {
         int len = strlen(line);
         if (len > PROC_HEADER_SIZE && !memcmp(line, procHeader, PROC_HEADER_SIZE))
            {
            if (len == PROC_LINE_SIZE)
               {
               int i;
               int machine;
               sscanf(line, "%*s %*d%*c %*s %*c %*s %*s %*c %*s %*s %*c %d", &machine);

               // Scan list of unsupported machines - We do not initialize the JIT for such hardware.
               for (i = 0; i < sizeof(S390UnsupportedMachineTypes) / sizeof(int); ++i)
                  {
                  if (machine == S390UnsupportedMachineTypes[i])
                     {
                     TR_ASSERT(0,"Hardware is not supported.");
                     }
                  }

               // Scan list of supported machines.
               for (i = 0; i < sizeof(S390MachineTypes) / sizeof(int); ++i)
                  {
                  if (machine == S390MachineTypes[i])
                     {
                     ret_machine = (TR_S390MachineType)machine;
                     }
                  }
               }
            }
         }
      fclose(fp);
      }

   return ret_machine;
  }



//              .ooooo.       .ooo
//             d88'   `8.   .88'
// oooo    ooo Y88..  .8'  d88'
//  `88b..8P'   `88888b.  d888P"Ybo.
//    Y888'    .8'  ``88b Y88[   ]88
//  .o8"'88b   `8.   .88P `Y88   88P
// o88'   888o  `boood8'   `88bod8'

#if defined(TR_HOST_X86)
extern "C" bool jitTestOSForSSESupport(void);
#endif


//                    o8o
//                    `"'
// ooo. .oo.  .oo.   oooo   .oooo.o  .ooooo.
// `888P"Y88bP"Y88b  `888  d88(  "8 d88' `"Y8
//  888   888   888   888  `"Y88b.  888
//  888   888   888   888  o.  )88b 888   .o8
// o888o o888o o888o o888o 8""888P' `Y8bod8P'
//
//
//                                         .o8                 oooo
//                                        "888                 `888
//  .oooo.o oooo    ooo ooo. .oo.  .oo.    888oooo.   .ooooo.   888   .oooo.o
// d88(  "8  `88.  .8'  `888P"Y88bP"Y88b   d88' `88b d88' `88b  888  d88(  "8
// `"Y88b.    `88..8'    888   888   888   888   888 888   888  888  `"Y88b.
// o.  )88b    `888'     888   888   888   888   888 888   888  888  o.  )88b
// 8""888P'     .8'     o888o o888o o888o  `Y8bod8P' `Y8bod8P' o888o 8""888P'
//          .o..P'
//          `Y8P'


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


#define notImplemented(A) TR_ASSERT(0, "This function is not defined for FEBase %s", (A) )

// Brought this debug stuff from Compilation.cpp
//
// Limit on the size of the debug string
//
const int indebugLimit = 1023;


// The delimiter characters
//
static const char * delimiters = " ";
// The original value of the environment variable
//
static const char *TR_DEBUGValue = 0;

// The original string with embedded nulls between individual values
//
static char permDebugString[indebugLimit + 1];
static int  permDebugStrLen = 0;

struct DebugValue
   {
   DebugValue * next;
   DebugValue *prev;
   char       *value;
   };

static DebugValue *head = 0;



// Options stuff
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"


TR::OptionTable OMR::Options::_feOptions[] =
   {
   {"code=",              "C<nnn>\tsize of a single code cache, in KB",
    TR::Options::set32BitNumeric, offsetof(OMR::FrontEnd::JitConfig, options.codeCacheKB), 0, " %d (KB)"},
   {"exclude=",   "D{regexp}\tdo not compile methods matching regexp", TR::Options::limitOption, 1, 0, "P%s"},
   {"limit=",     "D{regexp}\tonly compile methods matching regexp", TR::Options::limitOption, 0, 0, "P%s"},
   {"limitfile=", "D<filename>\tfilter methods as defined in filename.  "
                  "Use limitfile=(filename,firstLine,lastLine) to limit lines considered from firstLine to lastLine", TR::Options::limitfileOption, 0, 0, "P%s"},
   {"maxInlinedCalls=",    "O<nnn>\tmaximum number of calls to be inlined",
        TR::Options::set32BitSignedNumeric, offsetof(TR::Options,_maxInlinedCalls), 0, "F%d"},
   {"verbose",    "L\twrite compiled method names to vlog file or stdout in limitfile format",
    TR::Options::setVerboseBits, offsetof(TR::JitConfig, options.verboseFlags), (1<<TR_VerboseCompileStart)|(1<<TR_VerboseCompileEnd), "F=1"},
   {"verbose=",   "L{regex}\tlist of verbose output to write to vlog or stdout",
    TR::Options::setVerboseBits, offsetof(TR::JitConfig, options.verboseFlags), 0, "F"},
   {"version",     "M\tprint out JIT version", TR::Options::versionOption, 0, 0, "F"},
   {"vlog=",      "L<filename>\twrite verbose output to filename",
    TR::Options::setString, offsetof(TR::JitConfig, options.vLogFileName), 0, "P%s"},
   {0}
   };


#include "control/Recompilation.hpp"

void TR_LinkageInfo::setHasBeenRecompiled()
   {
   }


void TR_LinkageInfo::setHasFailedRecompilation()
   {
   }



// S390 specific fucntion - FIXME: make this only be a problem when HOST is s390.  Also, use a better
// name for this
void setDllSlip(char*CodeStart,char*CodeEnd,char*dllName,  TR::Compilation *comp) { notImplemented("setDllSlip"); }

// X86 specific functions
extern "C" bool jitTestOSForSSESupport(void) { return false; } // TODO there are some signal handling peculiarities on x86-32 linux


// runtime assumptions
#ifdef J9_PROJECT_SPECIFIC
// FIXME:
#include "runtime/RuntimeAssumptions.hpp"
void TR_PatchNOPedGuardSite::compensate(TR_FrontEnd *fe, bool isSMP, uint8_t *location, uint8_t *destination) { notImplemented("TR_PatchNOPedGuardSite::compensate"); }
void TR_PersistentClassInfo::removeASubClass(TR_PersistentClassInfo *) { notImplemented("TR_PersistentClassInfo::removeASubClass"); }
bool isOrderedPair(uint8_t recordType) { notImplemented("isOrderedPair"); return false; }
void OMR::RuntimeAssumption::addToRAT(TR_PersistentMemory * persistentMemory, TR_RuntimeAssumptionKind kind, TR_FrontEnd *fe, OMR::RuntimeAssumption** sentinel) { notImplemented("addToRAT"); }
void OMR::RuntimeAssumption::dumpInfo(char *subclassName) { notImplemented("dumpInfo"); }
void TR_PatchJNICallSite::compensate(TR_FrontEnd*, bool, void *) { notImplemented("TR_PatchJNICallSite::compensate"); }
void TR_PreXRecompile::compensate(TR_FrontEnd*, bool, void *) { notImplemented("TR_PreXRecompile::compensate"); }
TR_PatchNOPedGuardSiteOnClassPreInitialize *TR_PatchNOPedGuardSiteOnClassPreInitialize::make(TR_FrontEnd *fe, TR_PersistentMemory *, char*, unsigned int, unsigned char*, unsigned char*, OMR::RuntimeAssumption**) { notImplemented("TR_PatchNOPedGuardSiteOnClassPreInitialize::allocate"); return 0; }
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
