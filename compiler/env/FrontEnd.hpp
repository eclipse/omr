/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef FRONTEND_INCL
#define FRONTEND_INCL

//  __   ___  __   __   ___  __       ___  ___  __
// |  \ |__  |__) |__) |__  /  `  /\   |  |__  |  \
// |__/ |___ |    |  \ |___ \__, /~~\  |  |___ |__/
//

// The FrontEnd interface is a legacy interface to a language frontend.
// It is deprecated and new functions should not be added to this interface.
//
// If you need to ask a question of a language runtime the `ClassEnv`
// structure in the `env` directory is the preferred means of doing so.
// Add your query to the appropriate category, or create a new category if
// it is warranted.

#include "infra/List.hpp"
#include "infra/Random.hpp"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/CodeGenPhase.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/Snippet.hpp"
#include "compile/CompilationTypes.hpp"
#include "env/Processors.hpp"
#include "env/ProcessorInfo.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "optimizer/OptimizationStrategies.hpp"
#include "optimizer/Optimizations.hpp"
#include "runtime/Runtime.hpp"
#include "env/VerboseLog.hpp"

class TR_Debug;
class TR_FrontEnd;
class TR_Memory;
class TR_ResolvedMethod;
namespace OMR { struct MethodMetaDataPOD; }
namespace TR { class CodeCache; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class MethodSymbol; }
namespace TR { class Node; }
namespace TR { class Options; }
namespace TR { class PersistentInfo; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
struct TR_InlinedCallSite;
template <typename ListKind> class List;

char * feGetEnv(const char *);

namespace TR
   {
   static bool isJ9()
      {
#if defined(NONJAVA) || defined(JITTEST)
      return false;
#else
      return true;
#endif
      }
   }

// va_copy for re-using a va_list
//
#if defined(_MSC_VER)             // MSVC doesn't define va_copy
#if (_MSC_VER < 1800)
   #define va_copy(d,s) ((d)=(s))
   #define va_copy_end(x)
#else /* (_MSC_VER < 1800) */
   #define va_copy_end(x) va_end(x)
#endif /* (_MSC_VER < 1800) */
#elif defined(J9ZOS390) && !defined(__C99)  // zOS defines it only if __C99 is defined
   #define va_copy(d,s) (memcpy((d),(s),sizeof(va_list)))
   #define va_copy_end(x)
#else                                       // other platforms support va_copy properly
   #define va_copy_end(x) va_end(x)
#endif


/*
 * Codegen-specific data structure to pass data into Frontend-specific binary
 * encoding functions.
 */
struct TR_BinaryEncodingData
   {
   };

class TR_Uncopyable
   {
   public:
   TR_Uncopyable() {}
   ~TR_Uncopyable() {}
   private:
   TR_Uncopyable(const TR_Uncopyable &);            // = delete;
   TR_Uncopyable& operator=(const TR_Uncopyable &); // = delete;
   };


class TR_FrontEnd : public TR_Uncopyable
   {
public:
   TR_FrontEnd() {}

   // --------------------------------------------------------------------------
   // Method
   // --------------------------------------------------------------------------

   virtual TR_ResolvedMethod * createResolvedMethod(TR_Memory *, TR_OpaqueMethodBlock *, TR_ResolvedMethod * = 0, TR_OpaqueClassBlock * = 0);
   virtual OMR::MethodMetaDataPOD *createMethodMetaData(TR::Compilation *comp) { return NULL; }

   virtual TR_OpaqueMethodBlock * getMethodFromName(char * className, char *methodName, char *signature);
   virtual uint32_t offsetOfIsOverriddenBit();

   // Needs VMThread

   // Check if method entry and exit tracing is enabled
   virtual bool isMethodTracingEnabled(TR_OpaqueMethodBlock *method);

   virtual const char * sampleSignature(TR_OpaqueMethodBlock * aMethod, char * bug = 0, int32_t bufLen = 0,TR_Memory *memory = NULL);

   virtual int32_t getLineNumberForMethodAndByteCodeIndex(TR_OpaqueMethodBlock *, int32_t);

   virtual bool canDevirtualizeDispatch()     { return true; }

   // --------------------------------------------------------------------------
   // Codegen
   // --------------------------------------------------------------------------

   virtual uint8_t * allocateRelocationData(TR::Compilation *, uint32_t numBytes);

   // --------------------------------------------------------------------------
   // Optimizer
   // --------------------------------------------------------------------------

   // Inliner
   virtual bool isInlineableNativeMethod(TR::Compilation * comp, TR::ResolvedMethodSymbol * methodSymbol){ return false;}

   virtual TR_OpaqueMethodBlock *getInlinedCallSiteMethod(TR_InlinedCallSite *ics);

   // --------------------------------------------------------------------------
   // Class hierarchy
   // --------------------------------------------------------------------------

   virtual bool classHasBeenExtended(TR_OpaqueClassBlock *);
   virtual bool classHasBeenReplaced(TR_OpaqueClassBlock *);
   virtual TR_OpaqueClassBlock * getSuperClass(TR_OpaqueClassBlock * classPointer);
   virtual TR_YesNoMaybe isInstanceOf(TR_OpaqueClassBlock *instanceClass, TR_OpaqueClassBlock * castClass, bool instanceIsFixed, bool castIsFixed = true, bool callSiteVettedForAOT=false);
   virtual bool isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *);

   // --------------------------------------------------------------------------
   // Persistence
   // --------------------------------------------------------------------------

   virtual TR::PersistentInfo * getPersistentInfo() = 0;

   // --------------------------------------------------------------------------
   // J9 Object Model
   // --------------------------------------------------------------------------

   virtual int32_t getArraySpineShift(int32_t);
   virtual int32_t getArrayletMask(int32_t);
   virtual int32_t getArrayletLeafIndex(int32_t, int32_t);
   virtual uintptr_t getOffsetOfContiguousArraySizeField();
   virtual uintptr_t getOffsetOfDiscontiguousArraySizeField();
   virtual uintptr_t getObjectHeaderSizeInBytes();
   virtual uintptr_t getOffsetOfIndexableSizeField();

   // --------------------------------------------------------------------------
   // J9 Classes / VM?
   // --------------------------------------------------------------------------

   virtual TR::DataType dataTypeForLoadOrStore(TR::DataType dt) { return dt; }

   virtual TR_OpaqueClassBlock * getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer);
   virtual TR_OpaqueClassBlock * getClassFromMethodBlock(TR_OpaqueMethodBlock *mb);
   virtual int32_t getNewArrayTypeFromClass(TR_OpaqueClassBlock *clazz);

   // VM+Shared
   virtual TR_OpaqueClassBlock * getArrayClassFromComponentClass(TR_OpaqueClassBlock * componentClass);
   virtual TR_OpaqueClassBlock * getClassFromNewArrayType(int32_t arrayType);
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool callSiteVettedForAOT=false);
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool callSiteVettedForAOT=false);
   virtual TR_OpaqueClassBlock * getClassOfMethod(TR_OpaqueMethodBlock *method);
   virtual TR_OpaqueClassBlock * getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass);
   virtual TR_OpaqueClassBlock * getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass);

   // Null-terminated.  bufferSize >= 1+getStringUTF8Length(objectPointer).  Returns buffer just for convenience.
   virtual char *getStringUTF8(uintptr_t objectPointer, char *buffer, intptr_t bufferSize);
   virtual intptr_t getStringUTF8Length(uintptr_t objectPointer);

   // --------------------------------------------------------------------------
   // Code cache
   // --------------------------------------------------------------------------

   virtual void reserveTrampolineIfNecessary(TR::Compilation *, TR::SymbolReference *symRef, bool inBinaryEncoding);
   virtual intptr_t methodTrampolineLookup(TR::Compilation *, TR::SymbolReference *symRef, void * callSite);

   // --------------------------------------------------------------------------
   // Stay in FrontEnd
   // --------------------------------------------------------------------------

   virtual TR_Debug * createDebug(TR::Compilation * comp = NULL);

   // --------------------------------------------------------------------------

   virtual void acquireLogMonitor(); // used for multiple compilation threads
   virtual void releaseLogMonitor();
   virtual char *getFormattedName(char *, int32_t, char *, char *, bool);
   virtual void printVerboseLogHeader(TR::Options *cmdLineOptions) {}

   };

#endif
