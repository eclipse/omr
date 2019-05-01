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

#ifndef OMR_CLASSENV_INCL
#define OMR_CLASSENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CLASSENV_CONNECTOR
#define OMR_CLASSENV_CONNECTOR
namespace OMR { class ClassEnv; }
namespace OMR { typedef OMR::ClassEnv ClassEnvConnector; }
#endif

#include <stdint.h>
#include "infra/Annotations.hpp"
#include "env/jittypes.h"

struct OMR_VMThread;
namespace TR { class Compilation; }
namespace TR { class SymbolReference; }
class TR_ResolvedMethod;
class TR_Memory;

namespace OMR
{

class OMR_EXTENSIBLE ClassEnv
   {
public:

   // Are classes allocated on the object heap?
   //
   bool classesOnHeap() { return false; }

   // Can class objects be collected by a garbage collector?
   //
   bool classObjectsMayBeCollected() { return true; }

   // Depth of a class from the base class in a hierarchy.
   //
   uintptrj_t classDepthOf(TR_OpaqueClassBlock *clazzPointer) { return 0; }

   // Is specified class a string class?
   //
   bool isStringClass(TR_OpaqueClassBlock *clazz) { return false; }

   // Is specified object a string class?
   //
   bool isStringClass(uintptrj_t objectPointer) { return false; }

   // Class of specified object address
   //
   TR_OpaqueClassBlock *classOfObject(OMR_VMThread *vmThread, uintptrj_t objectPointer) { return NULL; }
   TR_OpaqueClassBlock *classOfObject(TR::Compilation *comp, uintptrj_t objectPointer) { return NULL; }

   bool isAbstractClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer) { return false; }
   bool isInterfaceClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer) { return false; }
   bool isPrimitiveClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return false; }
   bool isAnonymousClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return false; }
   bool isPrimitiveArray(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool isReferenceArray(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool isClassArray(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool isClassFinal(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool hasFinalizer(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer) { return false; }
   bool isClassInitialized(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool hasFinalFieldsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer) { return false; }
   bool sameClassLoaders(TR::Compilation *comp, TR_OpaqueClassBlock *, TR_OpaqueClassBlock *) { return false; }
   bool isString(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return false; }
   bool isString(TR::Compilation *comp, uintptrj_t objectPointer) { return false; }
   bool jitStaticsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2) { return false; }
   bool jitFieldsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2, int32_t isStatic) { return false; }

   uintptrj_t getArrayElementWidthInBytes(TR::Compilation *comp, TR_OpaqueClassBlock* arrayClass);

   uintptrj_t persistentClassPointerFromClassPointer(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return 0; }
   TR_OpaqueClassBlock *objectClass(TR::Compilation *comp, uintptrj_t objectPointer) { return NULL; }
   TR_OpaqueClassBlock *classFromJavaLangClass(TR::Compilation *comp, uintptrj_t objectPointer) { return NULL; }

   // 0 <= index < getStringLength
   uint16_t getStringCharacter(TR::Compilation *comp, uintptrj_t objectPointer, int32_t index) { return 0; }
   bool getStringFieldByName(TR::Compilation *, TR::SymbolReference *stringRef, TR::SymbolReference *fieldRef, void* &pResult) { return false; }

   int32_t vTableSlot(TR::Compilation *comp, TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *) { return 0; }
   int32_t flagValueForPrimitiveTypeCheck(TR::Compilation *comp) { return 0; }
   int32_t flagValueForArrayCheck(TR::Compilation *comp) { return 0; }
   int32_t flagValueForFinalizerCheck(TR::Compilation *comp) { return 0; }

   char *classNameChars(TR::Compilation *, TR::SymbolReference *symRef, int32_t & length);
   char *classNameChars(TR::Compilation *, TR_OpaqueClassBlock * clazz, int32_t & length) { return NULL; }

   char *classSignature_DEPRECATED(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, int32_t & length, TR_Memory *) { return NULL; }
   char *classSignature(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, TR_Memory *) { return NULL; }

   /**
    * Get the virtual function table entry at a specific offset from the class
    *
    * @param clazz The RAM class pointer to read from
    * @param offset An offset into the virtual function table (VFT) of clazz
    * @return The entry point of the method at the given offset
    */
   intptrj_t getVFTEntry(TR::Compilation *comp, TR_OpaqueClassBlock* clazz, int32_t offset);

   bool classUnloadAssumptionNeedsRelocation(TR::Compilation *comp);

   };

}

#endif
