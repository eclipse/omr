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
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"

struct OMR_VMThread;
namespace TR { class ClassEnv; }
namespace TR { class Compilation; }
namespace TR { class SymbolReference; }
namespace TR { class TypeLayout; }
namespace TR { class Region; }
class TR_ResolvedMethod;
class TR_Memory;
class TR_PersistentClassInfo;
template <typename ListKind> class List;

namespace OMR
{

class OMR_EXTENSIBLE ClassEnv
   {
public:

   TR::ClassEnv *self();

   // Are classes allocated on the object heap?
   //
   bool classesOnHeap() { return false; }

   // Can class objects be collected by a garbage collector?
   //
   bool classObjectsMayBeCollected() { return true; }

   // Depth of a class from the base class in a hierarchy.
   //
   uintptr_t classDepthOf(TR_OpaqueClassBlock *clazzPointer) { return 0; }

   // Is specified class a string class?
   //
   bool isStringClass(TR_OpaqueClassBlock *clazz) { return false; }

   // Is specified object a string class?
   //
   bool isStringClass(uintptr_t objectPointer) { return false; }

   // Class of specified object address
   //
   TR_OpaqueClassBlock *classOfObject(OMR_VMThread *vmThread, uintptr_t objectPointer) { return NULL; }
   TR_OpaqueClassBlock *classOfObject(TR::Compilation *comp, uintptr_t objectPointer) { return NULL; }

   bool isAbstractClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer) { return false; }
   bool isInterfaceClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer) { return false; }
   bool isConcreteClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer) { return true; }
   bool isPrimitiveClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return false; }
   bool isAnonymousClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return false; }
   bool isValueTypeClass(TR_OpaqueClassBlock *) { return false; }

   /** \brief
    *    Returns the size of the flattened array element
    *
    *    When array elements are flattened, the array element is inlined into the array. For example,
    *       Class Point {
    *          int x;
    *          int y;
    *       }
    *    Point pointArray[];
    *
    *    If pointArray is not flattened, the size of pointArray[i] is the size of the reference pointer size.
    *    If pointArray is flattened, the size of pointArray[i] is the total size of x and y and plus padding if there is any.
    *
    *  \param comp
    *    The compilation object
    *
    *  \param arrayClass
    *    The array class that is to be checked
    *
    *  \return
    *    Size of the flattened array element
    */
   int32_t flattenedArrayElementSize(TR::Compilation *comp, TR_OpaqueClassBlock *arrayClass) { return 0; }

   /**
    * \brief
    *    Checks whether instances of the specified class can be trivially initialized by
    *    "zeroing" their fields
    *
    * \param clazz
    *    The class that is to be checked
    *
    * \return
    *    `true` if instances of the specified class can be initialized by zeroing their fields;
    *    `false` otherwise (if some special initialization is required for some fields)
    */
   bool isZeroInitializable(TR_OpaqueClassBlock *clazz) { return true; }
   bool isPrimitiveArray(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   TR::DataTypes primitiveArrayComponentType(TR::Compilation *comp, TR_OpaqueClassBlock *) { return TR::NoType; }
   bool isReferenceArray(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool isClassArray(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool isClassFinal(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool hasFinalizer(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer) { return false; }
   bool isClassInitialized(TR::Compilation *comp, TR_OpaqueClassBlock *) { return false; }
   bool isClassVisible(TR::Compilation *comp, TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass) { return false; }
   bool hasFinalFieldsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer) { return false; }
   bool sameClassLoaders(TR::Compilation *comp, TR_OpaqueClassBlock *, TR_OpaqueClassBlock *) { return false; }
   bool isString(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return false; }
   bool isString(TR::Compilation *comp, uintptr_t objectPointer) { return false; }
   bool jitStaticsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2) { return false; }
   bool jitFieldsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2, int32_t isStatic) { return false; }

   uintptr_t getArrayElementWidthInBytes(TR::Compilation *comp, TR_OpaqueClassBlock* arrayClass);

   uintptr_t persistentClassPointerFromClassPointer(TR::Compilation *comp, TR_OpaqueClassBlock *clazz) { return 0; }
   TR_OpaqueClassBlock *objectClass(TR::Compilation *comp, uintptr_t objectPointer) { return NULL; }
   TR_OpaqueClassBlock *classFromJavaLangClass(TR::Compilation *comp, uintptr_t objectPointer) { return NULL; }

   // 0 <= index < getStringLength
   uint16_t getStringCharacter(TR::Compilation *comp, uintptr_t objectPointer, int32_t index) { return 0; }
   bool getStringFieldByName(TR::Compilation *, TR::SymbolReference *stringRef, TR::SymbolReference *fieldRef, void* &pResult) { return false; }

   int32_t vTableSlot(TR::Compilation *comp, TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *) { return 0; }
   int32_t flagValueForPrimitiveTypeCheck(TR::Compilation *comp) { return 0; }
   int32_t flagValueForArrayCheck(TR::Compilation *comp) { return 0; }
   int32_t flagValueForFinalizerCheck(TR::Compilation *comp) { return 0; }

   const char *classNameChars(TR::Compilation *, TR::SymbolReference *symRef, int32_t &length);
   const char *classNameChars(TR::Compilation *, TR_OpaqueClassBlock *clazz, int32_t &length) { return NULL; }

   char *classSignature_DEPRECATED(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, int32_t & length, TR_Memory *) { return NULL; }
   char *classSignature(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, TR_Memory *) { return NULL; }

   /**
    * \brief
    *    Constructs a class signature char string based on the class name
    *
    * \param[in] name
    *    The class name
    *
    * \param[in,out] len
    *    The input is the length of the class name. Returns the length of the signature
    *
    * \param[in] comp
    *    The compilation object
    *
    * \param[in] allocKind
    *    The type of the memory allocation
    *
    * \param[in] clazz
    *    The class that the class name belongs to
    *
    * \return
    *    A class signature char string
    */
   char *classNameToSignature(const char *name, int32_t &len, TR::Compilation *comp, TR_AllocationKind allocKind = stackAlloc, TR_OpaqueClassBlock *clazz = NULL);

   /**
    * Get the virtual function table entry at a specific offset from the class
    *
    * @param clazz The RAM class pointer to read from
    * @param offset An offset into the virtual function table (VFT) of clazz
    * @return The method at the given offset, or (depending on offset) its
    * entry point, or 0 if offset is out of bounds or the result is otherwise
    * unavailable.
    */
   intptr_t getVFTEntry(TR::Compilation *comp, TR_OpaqueClassBlock* clazz, int32_t offset);

   bool classUnloadAssumptionNeedsRelocation(TR::Compilation *comp);

   /** \brief
    *	    Populates a TypeLayout object.
    *
    *  \param region
    *     The region used to allocate TypeLayout.
    *
    *  \param opaqueClazz
    *     Class of the type whose layout needs to be populated.
    *
    *  \return
    *     Returns a NULL pointer.
    */
   const TR::TypeLayout* enumerateFields(TR::Region& region, TR_OpaqueClassBlock * clazz, TR::Compilation *comp) { return NULL; }

   /**
    * @brief Determine if a list of classes contains less than two concrete classes.
    * A class is considered concrete if it is not an interface or an abstract class
    *
    * @param subClasses List of subclasses to be checked.
    *
    * @return Returns 'true' if the given list of classes contains less than
    * 2 concrete classes and false otherwise.
    */
   bool containsZeroOrOneConcreteClass(TR::Compilation *comp, List<TR_PersistentClassInfo>* subClasses);
   };

}

#endif
