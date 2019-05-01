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

#ifndef CALL_STACK_ITERATOR_INCL
#define CALL_STACK_ITERATOR_INCL

#include <stddef.h>
#include <stdint.h>

namespace TR { class Compilation; }

class TR_CallStackIterator
   {
public:
   TR_CallStackIterator() {}
   virtual void printStackBacktrace(TR::Compilation *comp);

protected:

   virtual bool getNext() { return false; }
   virtual const char *getProcedureName() { return NULL; }
   virtual uintptr_t getOffsetInProcedure() {return 0; }
   virtual bool isDone() { return true; }
   };

#if defined(AIXPPC)
struct tbtable;

class TR_PPCCallStackIterator : public TR_CallStackIterator
   {
public:
   TR_PPCCallStackIterator();
   virtual bool getNext();
   virtual const char *getProcedureName();
   virtual uintptr_t getOffsetInProcedure() { return _offset_in_proc; }
   virtual bool isDone() { return _done; }
private:
   void        _set_tb_table();
   bool        _done;
   void *      _pc;
   uintptr_t      _offset_in_proc;
   void *      _tos;
   struct tbtable *  _tb_table;
   unsigned int      _num_next;
   friend class TR_CallStackIterator;
   };

typedef TR_PPCCallStackIterator TR_CallStackIteratorImpl;

#elif defined(LINUX)

class TR_LinuxCallStackIterator : public TR_CallStackIterator
   {
public:
   TR_LinuxCallStackIterator() {}
   virtual void printStackBacktrace(TR::Compilation *comp);
private:
   void printSymbol(int32_t frame, char *sig, TR::Compilation *comp);
   friend class TR_CallStackIterator;
   };

typedef TR_LinuxCallStackIterator TR_CallStackIteratorImpl;

#elif defined(J9ZOS390)

#ifdef TR_HOST_64BIT
   #include <__le_api.h>

   void __le_traceback(int cmd, void* cmd_parms, _FEEDBACK *fc);
   typedef __tf_parms_t parms_t;

#else
   #include <leawi.h>

   // This function is documented under the name CEETBCK.
   extern "C" void CEEKTBCK (void **dsaptr,
                             int  *dsa_format,
                             void **caaptr,
                             int  *member_id,
                             char *program_unit_name,
                             int  *program_unit_name_length,
                             void **program_unit_address,
                             int  *call_instruction_address,
                             char *entry_name,
                             int  *entry_name_length,
                             void **entry_address,
                             int  *caller_call_instruction_address,
                             void **caller_dsaptr,
                             int  *caller_dsa_format,
                             char *statement_id,
                             int  *statement_id_length,
                             void **cibptr,
                             int  *caller_main,
                             _FEEDBACK *fc);

   // Emulate 64-bit versions of __tf_string_t and __tf_parms_t:
   struct  tf_string_t
      {
      int          __tf_bufflen;
      char        *__tf_buff;
      };

   struct parms_t
      {
      void        *__tf_dsa_addr;
      void        *__tf_caa_addr;
      int          __tf_call_instruction;
      void        *__tf_pu_addr;
      void        *__tf_entry_addr;
      void        *__tf_cib_addr;
      int          __tf_member_id;
      int          __tf_is_main;
      tf_string_t  __tf_pu_name;
      tf_string_t  __tf_entry_name;
      tf_string_t  __tf_statement_id;
      void        *__tf_caller_dsa_addr;
      int          __tf_caller_call_instruction;

      // not present in 64-bit
      int          __tf_caller_dsa_format;
      int          __tf_dsa_format;
      };
#endif

class TR_MvsCallStackIterator : public TR_CallStackIterator
   {
public:
   TR_MvsCallStackIterator();
   virtual bool        getNext();
   virtual const char *getProcedureName()     { return _proc_name; }
   virtual uintptr_t   getOffsetInProcedure() { return _offset; }
   virtual bool        isDone() { return _done; }

private:
   bool        callTraceback();

   static const int MAX_NAME_LENGTH = 1024;
   char        _proc_name[MAX_NAME_LENGTH+1];
   uintptr_t   _offset;
   parms_t     _parms;
   _FEEDBACK   _fc;
   bool        _done;

   friend class TR_CallStackIterator;
   };

typedef TR_MvsCallStackIterator TR_CallStackIteratorImpl;

#elif defined(OMR_OS_WINDOWS) && defined(TR_HOST_X86) && defined(TR_HOST_32BIT)

class TR_WinCallStackIterator : public TR_CallStackIterator
   {
   // Class to retrieve call stack during assume failure.  Requires dbghelp.dll.
public:
   TR_WinCallStackIterator();
   bool getNext();
   const char *getProcedureName();
   unsigned int getOffsetInProcedure();
   bool isDone() { return _done; }

   private:
   unsigned long getEip();
   static const size_t MAX_STACK_SIZE = 30;
   char *      _function_names[MAX_STACK_SIZE];
   bool        _done;
   uintptr_t   _offset_in_proc[MAX_STACK_SIZE];
   uint32_t    _num_next;
   uint32_t    _trace_size;
   };

typedef TR_WinCallStackIterator TR_CallStackIteratorImpl;

#elif !(defined(OMR_OS_WINDOWS) && defined(TR_HOST_X86) && defined(TR_HOST_32BIT))

typedef TR_CallStackIterator TR_CallStackIteratorImpl;

#endif /* defined(AIXPPC) */

#endif
