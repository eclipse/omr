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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "env/TRMemory.hpp"
#include "ras/CallStackIterator.hpp"
#include "compile/Compilation.hpp"


void TR_CallStackIterator::printStackBacktrace(TR::Compilation *comp)
   {
   while (!isDone())
      {
      if (comp)
         traceMsg(comp, "%s+0x%lx\n", getProcedureName(), getOffsetInProcedure());
      else
         fprintf(stderr, "%s+0x%lx\n", getProcedureName(), getOffsetInProcedure());
      getNext();
      }
   }

#if defined(AIXPPC)
#include <demangle.h>
#include <sys/debug.h>

void * getCurrPC();
#pragma mc_func getCurrPC \
   { \
   "48000005"  /* bl .+4   */ \
   "7C6802A6"  /* mfspr r3,lr */ \
   }

void * getCurrTos();
#pragma mc_func getCurrTos \
   { \
   "38610000"  /* mr r3, r1   */ \
   }

void TR_PPCCallStackIterator::_set_tb_table()
   {
   _tb_table = NULL;
   uintptr_t x = 0;
   unsigned int *pc = (unsigned int *) _pc;
   while (*(++pc) != 0 && (++x < 10000000));
   if (*pc != 0)
      {
      return;
      }
   else
      {
      ++pc;
      _tb_table = (struct tbtable *)pc;
      if (_tb_table->tb.has_tboff)
    {
    char *x = (char *)(&_tb_table->tb_ext);
    if (_tb_table->tb.fixedparms + _tb_table->tb.floatparms != 0)
       {
       x += sizeof(_tb_table->tb_ext.parminfo);
       }
    if (((unsigned int *) (uintptr_t) _tb_table - *((unsigned int *) x)) > pc)
       {
       _tb_table = NULL;
       return;
       }
    else
       {
       _offset_in_proc = ((uintptr_t) pc) - ((uintptr_t) _tb_table - *((unsigned int *) x));
       }
    }
      else
    {
    _offset_in_proc = 0;
    }
      return;
      }
   }

TR_PPCCallStackIterator::TR_PPCCallStackIterator() : TR_CallStackIterator(), _num_next(0)
   {
   _pc = getCurrPC();
   _tos = getCurrTos();
   _set_tb_table();
   _done = (_tb_table == NULL);
   getNext();     // Skip this constructor
   getNext();     // Skip TR_Debug::getCallStackIterator()
   }

bool TR_PPCCallStackIterator::getNext()
   {
   if (++_num_next > 200) _done = true;      // Chicken out
   if (!_done)
      {
      // We're going to assume that the LR is always saved
      struct stack_mark_t
    {
    struct stack_mark_t *  back_chain;
    uintptr_t     saved_cr;
    uintptr_t     saved_lr;
    } *stack_mark = (struct stack_mark_t *) _tos;
      _tos = (void *) stack_mark->back_chain;
      _pc = (void *) stack_mark->back_chain->saved_lr;
      _set_tb_table();
      _done = (_tb_table == NULL);
      if (_tos == NULL) _done = true;
      }
   return !_done;
   }

const char *TR_PPCCallStackIterator::getProcedureName()
   {
   if (_done)
      {
      return NULL;
      }
   else
      {
      if (_tb_table->tb.name_present)
    {
    char *x = (char *)(&_tb_table->tb_ext);
    if (_tb_table->tb.fixedparms + _tb_table->tb.floatparms != 0)
       {
       x += sizeof(_tb_table->tb_ext.parminfo);
       }
    if (_tb_table->tb.has_tboff)
       {
       x += sizeof(_tb_table->tb_ext.tb_offset);
       }
    if (_tb_table->tb.int_hndl)
       {
       x += sizeof(_tb_table->tb_ext.hand_mask);
       }
    if (_tb_table->tb.has_ctl)
       {
       int y = *((int *) x);
       x += sizeof(_tb_table->tb_ext.ctl_info) + (y * sizeof(_tb_table->tb_ext.ctl_info_disp[0]));
       }
    short name_len = *((short *) x);
    x += sizeof(_tb_table->tb_ext.name_len);
    char *z = (char*) TR::globalAllocator().allocate(name_len+4);
    strncpy(z, x, name_len);
    z[name_len] = '\0';
    char *ptoc;
    Name *n = Demangle(z, ptoc); // Probably does a new...which is bad!
    return n == NULL ? z : n->Text();
    }
      else
    {
    return NULL;
    }
      }
   }

#elif defined(LINUX)
#include <execinfo.h>
#include <cxxabi.h>

#if defined(MUSL)
char ** backtrace_symbols(void *const *trace, int size)
{
	return NULL;
}

int backtrace(void **trace, int size)
{
	return 0;
}
#endif

void TR_LinuxCallStackIterator::printSymbol(int32_t frame, char *sig, TR::Compilation *comp)
   {
   char lib[256];
   char func[256];
   intptr_t offset;
   intptr_t address;

   int rc = sscanf(sig, "%255[^(](%255[^+]+%llx) [%llx]", lib, func, &offset, &address);
   if (rc == 4)
      {
      char *funcToPrint = func;
      size_t length = 256;
      char *buffer = (char*) malloc(length);
      int32_t status = -1;
      char *demangled = abi::__cxa_demangle(func, buffer, &length, &status);
      if (status == 0) funcToPrint = demangled;
      if (comp)
         traceMsg(comp, "#%d: function %s+%#x [%#p]\n",
              frame,
              funcToPrint,
              offset,
              address);
      else
         fprintf(stderr, "#%d: function %s+%#x [%#p]\n",
                 frame,
                 funcToPrint,
                 offset,
                 address);
      if (demangled) free(demangled);
      }
   else
      {
      if (comp)
         traceMsg(comp, "#%d: %s\n", frame, sig);
      else
         fprintf(stderr, "#%d: %s\n", frame, sig);
      }
   }

void TR_LinuxCallStackIterator::printStackBacktrace(TR::Compilation *comp)
   {
   const uint32_t MAX_TRACE_SIZE = 30;
   const uint32_t SKIP_FRAMES = 0;
   void *trace[MAX_TRACE_SIZE];

   auto size = backtrace(trace, MAX_TRACE_SIZE);
   char **symbols = backtrace_symbols(trace, size);
   for (uint32_t i = SKIP_FRAMES; i < size; ++i)
      {
      char *signature = symbols[i];
      printSymbol(i, signature, comp);
      }
   free(symbols);
   }

#elif defined(J9ZOS390)

#include <unistd.h>
#include <ceeedcct.h>

extern "builtin" void *__gdsa();

TR_MvsCallStackIterator::TR_MvsCallStackIterator ()
      : TR_CallStackIterator()
   {
   _parms.__tf_dsa_addr = (void*)__gdsa();
   _parms.__tf_caa_addr = (void*)__gtca();
   _parms.__tf_call_instruction = 0;
   _parms.__tf_pu_addr = 0;
   _parms.__tf_entry_addr = 0;
   _parms.__tf_cib_addr = 0;
   _parms.__tf_member_id = -1;
   _parms.__tf_is_main = 0;
   _parms.__tf_pu_name.__tf_buff = 0;
   _parms.__tf_pu_name.__tf_bufflen = 0;
   _parms.__tf_entry_name.__tf_buff = _proc_name;
   _parms.__tf_entry_name.__tf_bufflen = MAX_NAME_LENGTH;
   _parms.__tf_statement_id.__tf_buff = 0;
   _parms.__tf_statement_id.__tf_bufflen = 0;
   _parms.__tf_caller_dsa_addr = 0;
   _parms.__tf_caller_call_instruction = 0;

#ifndef TR_HOST_64BIT
   // Not present in 64-bit mode:
   _parms.__tf_dsa_format = -1;
   _parms.__tf_caller_dsa_format = 0;
#endif

   _offset = 0;

   _done = !callTraceback();
   }

bool TR_MvsCallStackIterator::callTraceback()
   {
   if (_parms.__tf_dsa_addr == 0) return false;

#ifdef TR_HOST_64BIT
   // 64-bit LE traceback function
   __le_traceback(__TRACEBACK_FIELDS, &_parms, &_fc);
#else
   // 31-bit LE traceback function
   CEEKTBCK (&_parms.__tf_dsa_addr,                   // in
             &_parms.__tf_dsa_format,                 // in out
             &_parms.__tf_caa_addr,                   // in
             &_parms.__tf_member_id,                  //    out
              _parms.__tf_pu_name.__tf_buff,          //    out
             &_parms.__tf_pu_name.__tf_bufflen,       // in out
             &_parms.__tf_pu_addr,                    //    out
             &_parms.__tf_call_instruction,           // in out
              _parms.__tf_entry_name.__tf_buff,       //    out
             &_parms.__tf_entry_name.__tf_bufflen,    // in out
             &_parms.__tf_entry_addr,                 //    out
             &_parms.__tf_caller_call_instruction,    //    out
             &_parms.__tf_caller_dsa_addr,            //    out
             &_parms.__tf_caller_dsa_format,          //    out
              _parms.__tf_statement_id.__tf_buff,     //    out
             &_parms.__tf_statement_id.__tf_bufflen,  // in out
             &_parms.__tf_cib_addr,                   //    out
             &_parms.__tf_is_main,                    //    out
             &_fc);                                   //    out
#endif
   // Null-terminate the string.
   _proc_name[_parms.__tf_entry_name.__tf_bufflen] = 0;

#if defined(J9ZOS390) && defined(J9_PROJECT_SPECIFIC)
   // On Java zOS, we need to convert the ebcdic string to ascii
   __e2a_l( _proc_name, (int32_t) _parms.__tf_entry_name.__tf_bufflen);
#endif

   _offset = (char*)_parms.__tf_call_instruction -
             (char*)_parms.__tf_entry_addr;
   return true;
   }

bool TR_MvsCallStackIterator::getNext ()
   {
   if (isDone()) return false;

   _parms.__tf_dsa_addr = _parms.__tf_caller_dsa_addr;
   _parms.__tf_caller_dsa_addr = 0;
#ifndef TR_HOST_64BIT
   _parms.__tf_dsa_format = _parms.__tf_caller_dsa_format;
#endif
   _parms.__tf_call_instruction = _parms.__tf_caller_call_instruction;
   // Reset the buffer size back to max.
   _parms.__tf_entry_name.__tf_bufflen = MAX_NAME_LENGTH;

   _done = !callTraceback();
   return true;
   }

#elif defined(OMR_OS_WINDOWS) && defined(TR_HOST_X86) && defined(TR_HOST_32BIT)

#include "ras/CallStack.hpp"
#include <windows.h>
#include <Winbase.h>

uintptr_t TR_WinCallStackIterator::getOffsetInProcedure()
   {
   return (_done ? 0 : _offset_in_proc[_num_next]);
   }

const char * TR_WinCallStackIterator::getProcedureName()
   {
   return (_done ? NULL : _function_names[_num_next]);
   }

unsigned long TR_WinCallStackIterator::getEip()
   {
   unsigned long Eip;
   // Clang format gets upset at the missing ; below, so disable.
   // clang-format off
   __asm mov eax, [ebp + 4]         // Get the return address out of the current stack frame
   __asm mov [Eip], eax
   return Eip;
   // clang-format on
   }

TR_WinCallStackIterator::TR_WinCallStackIterator() :
      TR_CallStackIterator(),
      _num_next(0),
      _trace_size(0),
      _done(true)
   {
   // Load dbghelp.dll
   HINSTANCE hLib=LoadLibrary("dbghelp.dll");
   if(hLib==NULL)
      {
      fprintf (stderr, "Unable to print trace stack. Failed to load dbghelp.dll");
      return;
      }

   // Initialize function pointers.
   SymGetOptionsType SymGetOptions = (SymGetOptionsType) GetProcAddress((HMODULE)hLib, "SymGetOptions");
   SymSetOptionsType SymSetOptions = (SymSetOptionsType) GetProcAddress((HMODULE)hLib, "SymSetOptions");
   SymInitializeType SymInitialize = (SymInitializeType) GetProcAddress((HMODULE)hLib, "SymInitialize");
   StackWalk64Type   StackWalk64   = (StackWalk64Type)   GetProcAddress((HMODULE)hLib, "StackWalk64");
   SymFromAddrType   SymFromAddr   = (SymFromAddrType)   GetProcAddress((HMODULE)hLib, "SymFromAddr");
   SymCleanupType    SymCleanup    = (SymCleanupType)    GetProcAddress((HMODULE)hLib, "SymCleanup");

   PFUNCTION_TABLE_ACCESS_ROUTINE64 SymFunctionTableAccess64 =(PFUNCTION_TABLE_ACCESS_ROUTINE64) GetProcAddress((HMODULE)hLib, "SymFunctionTableAccess64");
   PGET_MODULE_BASE_ROUTINE64 SymGetModuleBase64 = (PGET_MODULE_BASE_ROUTINE64) GetProcAddress((HMODULE)hLib, "SymGetModuleBase64");

   // Ensure all the function pointers are loaded correctly.
   char * loadFailed = NULL;
   if (SymGetOptions == NULL)
      loadFailed = "SymGetOptions";
   if (SymSetOptions == NULL)
      loadFailed = "SymSetOptions";
   if (SymInitialize == NULL)
      loadFailed = "SymInitialize";
   if (StackWalk64 == NULL)
      loadFailed = "StackWalk64";
   if (SymFromAddr == NULL)
      loadFailed = "SymFromAddr";
   if (SymFunctionTableAccess64 == NULL)
      loadFailed = "SymFunctionTableAccess64";
   if (SymGetModuleBase64 == NULL)
      loadFailed = "SymGetModuleBase64";

   if (loadFailed)
      {
      fprintf (stderr, "Unable to print trace stack. GetProcAddress failed for %s\n", loadFailed);
      return;
      }

   HANDLE       hProcess   = GetCurrentProcess() ;
   HANDLE       hThread    = GetCurrentThread() ;

   memset( _function_names, NULL, MAX_STACK_SIZE * sizeof(char **) );
   memset( _offset_in_proc, 0, MAX_STACK_SIZE * sizeof(unsigned int) );

   // Set options
   DWORD Options = SymGetOptions();
   #define SYMOPT_DEBUG                    0x80000000
   Options |= SYMOPT_DEBUG;
   SymSetOptions( Options );

   // Define search path.  This is needed to find the .pdb file that contains
   // the symbol information.
   const int32_t MAX_SEARCHPATH = 1024;
   char *userSearchPath;
   char userSearchPathBuffer[MAX_SEARCHPATH];
   DWORD searchPathLength = GetModuleFileName(NULL, userSearchPathBuffer, MAX_SEARCHPATH);
   if (searchPathLength == 0 || searchPathLength >= MAX_SEARCHPATH)
      {
      fprintf(stderr, "unable to find module path for current executing - symbol names might be incorrect\n");
      userSearchPath = NULL;
      }
   else
      userSearchPath = userSearchPathBuffer;

   if (!(* SymInitialize)(hProcess, userSearchPath, TRUE))
      {
      // SymInitialize failed
      fprintf(stderr, "Unable to retrieve symbol informaton. SymInitialize returned error : %d\n", GetLastError());
      }

   //Initialize StackFrame
   STACKFRAME64 StackFrame;
   memset( &StackFrame, 0, sizeof(StackFrame) );
   unsigned long pc = getEip();      // Retrieve eip
   unsigned long basePtr;            // Retrieve base pointer
   __asm mov [basePtr], ebp

   StackFrame.AddrPC.Offset      = pc;
   StackFrame.AddrPC.Mode        = AddrModeFlat;
   StackFrame.AddrFrame.Offset   = basePtr;
   StackFrame.AddrFrame.Mode     = AddrModeFlat;

   while (_trace_size < MAX_STACK_SIZE)
      {
      if ( !StackWalk64(
                  IMAGE_FILE_MACHINE_I386,   // Machine architecture type (x86)
                  hProcess,      // Process handle
                  hThread,       // Thread handle
                  &StackFrame,   // Stack frame
                  NULL,          // Thread context (not needed for x86)
                  0,             // Read memory function - not used
                  SymFunctionTableAccess64,  // Function table access function (FPO access on x86)
                  SymGetModuleBase64,  // Function that can determine module base for the given address
                  0 ) )          // Address translation function - not used
         {
         break;
         }

      DWORD64  dwDisplacement;
      // Allocate buffer to retrieve symbol info
      PSYMBOL_INFO pSymbol = (PSYMBOL_INFO) TR::globalAllocator("CallStackIterator").allocate (
            (sizeof(SYMBOL_INFO) +
             MAX_SYM_NAME*sizeof(TCHAR) +
             sizeof(ULONG64) - 1) /
            sizeof(ULONG64));

      pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      pSymbol->MaxNameLen = MAX_SYM_NAME;

      if (SymFromAddr(hProcess, StackFrame.AddrPC.Offset, &dwDisplacement, pSymbol))
         {
         _function_names[_trace_size] = (char *) TR::globalAllocator("CallStackIterator").allocate (strlen(pSymbol->Name) * sizeof (char) + 1);
         strcpy(_function_names[_trace_size], pSymbol->Name);
         }

      _offset_in_proc[_trace_size] = dwDisplacement;

      _trace_size ++;
      }
   SymCleanup(hProcess);
   FreeLibrary((HMODULE)hLib);

   _done = (_num_next >= _trace_size);
   getNext();     // Skip this constructor
   getNext();     // Skip TR_Debug::getWinCallStackIterator()
   }

bool TR_WinCallStackIterator::getNext()
   {
   _done = (++ _num_next >= _trace_size);
   return !_done;
   }

#elif !(defined(OMR_OS_WINDOWS) && defined(TR_HOST_X86) && defined(TR_HOST_32BIT))

#endif /* defined(AIXPPC) */
