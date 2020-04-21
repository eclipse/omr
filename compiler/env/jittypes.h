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

#ifndef JITTYPES_INCL
#define JITTYPES_INCL

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define IS_8BIT_SIGNED(x)    ((x) == (  int8_t)(x))
#define IS_8BIT_UNSIGNED(x)  ((x) == ( uint8_t)(x))
#define IS_16BIT_SIGNED(x)   ((x) == ( int16_t)(x))
#define IS_16BIT_UNSIGNED(x) ((x) == (uint16_t)(x))
#define IS_32BIT_SIGNED(x)   ((x) == ( int32_t)(x))
#define IS_32BIT_UNSIGNED(x) ((x) == (uint32_t)(x))

#define MAX_UINTPTRJ (~(uintptr_t)0)
#define MAX_UINTPTR (~(uintptr_t)0)

#ifdef __cplusplus
namespace TR { class Compilation; }

/// This data type exists to refer class data which TR knows exists,
/// but doesn't know a lot about. Intended to avoid using `void *`
///
/// You can't create any instances of them.
class TR_OpaqueClassBlock
   {
private:
   TR_OpaqueClassBlock() { }
   };

/// This data type exists to refer to method data which TR knows exists,
/// but doesn't know a lot about. Intended to avoid using `void *`
///
/// You can't create any instances of them.
class TR_OpaqueMethodBlock
   {
private:
   TR_OpaqueMethodBlock() { }
   };

#else
struct TR_Compilation;
typedef struct TR_structOpaqueClassBlock  *TR_OpaqueClassBlock;
typedef struct TR_structOpaqueMethodBlock *TR_OpaqueMethodBlock;
#endif

typedef struct TR_ByteCodeInfo
   {
#ifdef __cplusplus
   bool doNotProfile(){return this->_doNotProfile;}
   void setDoNotProfile(bool b){this->_doNotProfile = b;}
   bool isSameReceiver(){return this->_isSameReceiver;}
   void setIsSameReceiver(bool b){this->_isSameReceiver = b;}
   static const int32_t invalidByteCodeIndex =  -1;
   static const int16_t invalidCallerIndex = -1;

   void setInvalidByteCodeIndex()
      {
      setByteCodeIndex(invalidByteCodeIndex);
      }
   void setZeroByteCodeIndex()
      {
      setByteCodeIndex(0);
      }
   bool isInvalidByteCodeIndex()
      {
      return getByteCodeIndex() == invalidByteCodeIndex;
      }
   void setInvalidCallerIndex()
      {
      setCallerIndex(invalidCallerIndex);
      }
   bool isInvalidCallerIndex()
      {
      return getCallerIndex() == invalidCallerIndex;
      }

   void setByteCodeIndex(int32_t i)
      {
      _byteCodeIndex = i;
      }
   int32_t getByteCodeIndex() const {return _byteCodeIndex;}

   void setCallerIndex(int16_t i){_callerIndex = i;}
   int16_t getCallerIndex() const {return _callerIndex;}

   enum
      {
      maxCallerIndex = 4095,
      };

   private:

#endif

   uint32_t _doNotProfile   : 1;
   uint32_t _isSameReceiver : 1;  /* check whether callee has the same receiver as the caller */
   int32_t  _callerIndex    : 13; /* index into TR_InlinedCallSite array.  -1 implies outermost function */
   int32_t _byteCodeIndex   : 17; /* should not be less than jitConfig->bcSizeLimit, currently is 64k (16 bits) */

   } TR_ByteCodeInfo;

typedef struct TR_InlinedCallSite
   {
   TR_OpaqueMethodBlock   * _methodInfo;
   #define _vmMethodInfo _methodInfo
   struct TR_ByteCodeInfo   _byteCodeInfo;
   } TR_InlinedCallSite;

/* Useful between initializing a data structure and making it visible to other threads */
#if !defined(TR_HOST_POWER)
   #if defined(TR_HOST_ARM)
      //dmb
      #define FLUSH_MEMORY(smp) if ( smp ) __asm__(".word 0xf57ff05f");
   #elif defined(TR_HOST_ARM64)
      #define FLUSH_MEMORY(smp) if ( smp ) __asm__("dmb sy");
   #else
      #define FLUSH_MEMORY(smp)
   #endif
#else
   #if defined(__IBMC__) || defined(__IBMCPP__)
      #if defined(__cplusplus)
      #include <builtins.h>
      #endif /* __cplusplus */
      #define FLUSH_MEMORY(smp) if( smp ) __lwsync();
   #elif defined(LINUX)
      #define FLUSH_MEMORY(smp) if( smp ) __asm__("lwsync");
   #endif
#endif

#endif
