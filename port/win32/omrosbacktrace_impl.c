/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Stack backtracing support
 */

#define UDATA UDATA_win32_
#include <windows.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <DbgHelp.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#include "omrport.h"
#include "omrsignal.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrutil.h"
#include "omrintrospect_common.h"
#include "omrintrospect.h"


static wchar_t *
buildPDBPath(struct OMRPortLibrary *portLibrary, HANDLE process)
{
	HMODULE hModules[128];
	wchar_t modulePath[EsMaxPath];
	uintptr_t pathLength = 0;
	DWORD nModules;
	DWORD cbNeeded;
	uintptr_t i;
	DWORD maxPathLength = sizeof(PPG_pdbData.searchPath) - 1;

	if (EnumProcessModules(process, hModules, sizeof(hModules), &cbNeeded) == TRUE) {
		/* iterate over the modules and extract their paths. */
		if (sizeof(hModules) > cbNeeded) {
			nModules = cbNeeded / sizeof(HMODULE);
		} else {
			nModules = sizeof(hModules) / sizeof(HMODULE);
		}

		/* check to see if the module count has changed since last use */
		if (PPG_pdbData.numberOfModules == nModules) {
			return PPG_pdbData.searchPath;
		}

		for (i = 0; i < nModules; i++) {
			if (GetModuleFileNameW(hModules[i], modulePath, sizeof(modulePath)) != 0) {
				wchar_t *cursor;
				uintptr_t j;
				uintptr_t moduleNameLen;

				/* remove basename of the module from the end of the path. */
				cursor = wcsrchr(modulePath, L'\\');

				*cursor = '\0';
				moduleNameLen = wcslen(modulePath);

				/* force all paths to lower case */
				for (j = 0; j < moduleNameLen; j++) {
					modulePath[j] = tolower(modulePath[j]);
				}

				if (NULL == PPG_pdbData.searchPath) {
					/* the constructed pdb path is empty, so just cat the first entry onto the null string */
					if (moduleNameLen < maxPathLength) {
						wcscat(PPG_pdbData.searchPath, modulePath);
						pathLength = moduleNameLen;
					} else {
						/* cannot fit any more data, so abandon. */
						break;
					}
				} else {
					/* the constructed pdb path has entries, so search it to prevent adding the same entry again */
					if (wcsstr(PPG_pdbData.searchPath, modulePath) == NULL) {
						/* path not found, so add it */
						if (moduleNameLen + pathLength + 1 < maxPathLength) {
							wcscat(PPG_pdbData.searchPath, L";");
							wcscat(PPG_pdbData.searchPath, modulePath);
							pathLength += moduleNameLen + 1;
						} else {
							/* cannot fit any more data, so abandon. */
							break;
						}
					}
				}
			}
		}

		PPG_pdbData.numberOfModules = nModules;
	}
	return PPG_pdbData.searchPath;
}

/* This function loads the table of debug library function pointers, if not already loaded. Once loaded,
 * we keep the DLL and function table for the life of the JVM. PPG_dbgHlpLibraryFunctions->hDbgHelpLib
 * indicates whether we have already loaded the library and function pointers.
 *
 * Note - this is called single threaded, e.g. under PPG_osBacktrace_Mutex
 *
 * @param portLibrary - a pointer to an initialized port library
 * @return - 0 on success, -1 on failure
 */
int32_t
load_dbg_functions(struct OMRPortLibrary *portLibrary)
{
	API_VERSION globalDbgHelpVersionInfo = {0};

	if (NULL == PPG_dbgHlpLibraryFunctions) {
		/* port library startup failed to allocate the debug function table */
		return -1;
	}

	if (NULL == PPG_dbgHlpLibraryFunctions->hDbgHelpLib) {
		/* see if the debug help library is already loaded, if not try to load it */
		PPG_dbgHlpLibraryFunctions->hDbgHelpLib = (HMODULE) j9getdbghelp_getDLL();
		if (NULL == PPG_dbgHlpLibraryFunctions->hDbgHelpLib) {
			PPG_dbgHlpLibraryFunctions->hDbgHelpLib = (HINSTANCE) j9getdbghelp_loadDLL();
		}

		if (NULL == PPG_dbgHlpLibraryFunctions->hDbgHelpLib) {
			/* no debug help DLL available */
			return -1;
		}

		/* found and loaded the debug help library, so now locate the entry points we need */
		PPG_dbgHlpLibraryFunctions->ImagehlpApiVersion =
			(IMAGEHLPAPIVERSION) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "ImagehlpApiVersion");

		/* check which debug help library was loaded by this process */
		globalDbgHelpVersionInfo = *(PPG_dbgHlpLibraryFunctions->ImagehlpApiVersion());

		/* ensure that we can load the minimum required version of the dbghelp API */
		if (globalDbgHelpVersionInfo.MajorVersion >= 4) {
			PPG_dbgHlpLibraryFunctions->SymGetOptions = (SYMGETOPTIONS) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymGetOptions");
			PPG_dbgHlpLibraryFunctions->SymSetOptions = (SYMSETOPTIONS) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymSetOptions");
			PPG_dbgHlpLibraryFunctions->SymInitializeW = (SYMINITIALIZEW) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymInitializeW");
			PPG_dbgHlpLibraryFunctions->SymCleanup = (SYMCLEANUP) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymCleanup");
			PPG_dbgHlpLibraryFunctions->SymGetModuleBase64 = (SYMGETMODULEBASE64) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymGetModuleBase64");
			PPG_dbgHlpLibraryFunctions->SymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymFunctionTableAccess64");
			PPG_dbgHlpLibraryFunctions->StackWalk64 = (STACKWALK64) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "StackWalk64");
			PPG_dbgHlpLibraryFunctions->SymFromAddr = (SYMFROMADDR) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymFromAddr");
			PPG_dbgHlpLibraryFunctions->SymGetLineFromAddr64 = (SYMGETLINEFROMADDR64) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymGetLineFromAddr64");
			PPG_dbgHlpLibraryFunctions->SymGetModuleInfo64 = (SYMGETMODULEINFO64) GetProcAddress(PPG_dbgHlpLibraryFunctions->hDbgHelpLib, "SymGetModuleInfo64");
		} else {
			/* wrong version of the library */
			PPG_dbgHlpLibraryFunctions->hDbgHelpLib = NULL;
			return -1;
		}
	}

	return 0;
}

/* This function sets symbol options and initializes the debug PDB path and symbol handler, if not already
 * initialised. We reset the symbol options and free loaded symbols on completion of the native stack walk
 * for javacores. PPG_pdbData.numberOfModules indicates whether we have currently initialized symbols.
 *
 * Note - this is called single threaded, e.g. under PPG_osBacktrace_Mutex
 *
 * @param portLibrary - a pointer to an initialized port library
 * @return - 0 on success, -1 on failure
 */
int32_t
load_dbg_symbols(struct OMRPortLibrary *portLibrary)
{
	HANDLE process;

	if (0 == PPG_pdbData.numberOfModules) {
		/* save the current symbol options */
		PPG_pdbData.symOptions = PPG_dbgHlpLibraryFunctions->SymGetOptions();
		/* this gives a cut down version of the module list (much faster) */
		PPG_dbgHlpLibraryFunctions->SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_CASE_INSENSITIVE | SYMOPT_LOAD_ANYTHING);

		/* initialize the PDB search path */
		buildPDBPath(portLibrary, GetCurrentProcess());

		/* reload the symbol data */
		process = GetCurrentProcess();
		PPG_dbgHlpLibraryFunctions->SymCleanup(process);

		/* initialize the symbol handler */
		if (FALSE == PPG_dbgHlpLibraryFunctions->SymInitializeW(process, PPG_pdbData.searchPath, TRUE)) {
			free_dbg_symbols(portLibrary);
			return -1;
		}
	}
	return 0;
}


/* This function frees loaded debug symbols and resets debug symbol options. PPG_pdbData.numberOfModules
 * set to 0 indicates that we have unloaded the debug symbols and reset the options.
 *
 * Note - this function is called single threaded, e.g. under PPG_osBacktrace_Mutex
 *
 * @param portLibrary - a pointer to an initialized port library
 * @returns - none
 */
void
free_dbg_symbols(struct OMRPortLibrary *portLibrary)
{
	if (NULL != PPG_dbgHlpLibraryFunctions) {
		if (NULL != PPG_dbgHlpLibraryFunctions->hDbgHelpLib) {
			PPG_dbgHlpLibraryFunctions->SymCleanup(GetCurrentProcess());
			PPG_dbgHlpLibraryFunctions->SymSetOptions(PPG_pdbData.symOptions);
			PPG_pdbData.numberOfModules = 0;
		}
	}
}


/* This function constructs a backtrace from a CPU context. Generally there are only one or two
 * values in the context that are actually used to construct the stack but these vary by platform
 * so arn't detailed here. If no heap is specified then this function will use malloc to allocate
 * the memory necessary for the stack frame structures which must be freed by the caller.
 *
 * @param portLbirary a pointer to an initialized port library
 * @param threadInfo the thread structure we want  to attach the backtrace to. Must not be NULL.
 * @param heap a heap from which to allocate any necessary memory. If NULL malloc is used instead.
 * @param signalInfo a platform signal context. If not null the context held in threadInfo is replaced
 *  	  with the signal context before the backtrace is generated.
 *
 * @return the number of frames in the backtrace.
 */
uintptr_t
omrintrospect_backtrace_thread_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, void *signalInfo)
{
	BOOLEAN rc = FALSE;
	uintptr_t frameNumber = 0;
	HANDLE process = NULL;
	HANDLE thread = NULL;
	CONTEXT threadContext = {0};
	STACKFRAME64 stackFrame = {0};
	J9PlatformStackFrame **nextFrame;
	struct J9Win32SignalInfo *sigInfo = (struct J9Win32SignalInfo *)signalInfo;

	if (threadInfo == NULL || (threadInfo->context == NULL && sigInfo == NULL)) {
		return 0;
	}

	/* if it's a stack overflow then we need to supress generation of a backtrace or we'll overflow the stack buffer */
	if (sigInfo != NULL && sigInfo->ExceptionRecord != NULL && sigInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
		return 0;
	}

	if (NULL == PPG_osBacktrace_Mutex) {
		/* Create an anonymous mutex with this thread as the owner */
		PPG_osBacktrace_Mutex = CreateMutex(NULL, TRUE, NULL);
		if (NULL == PPG_osBacktrace_Mutex) {
			/* Couldn't create or obtain a mutex */
			return 0;
		}
	} else {
		/* mutex already exists, so go and acquire it */
		WaitForSingleObject(PPG_osBacktrace_Mutex, INFINITE);
	}

	/* Get the current process and thread handles */
	process = GetCurrentProcess();
	/* if this is being used directly without a target thread the get the ID */
	if (threadInfo->thread_id == 0) {
		threadInfo->thread_id = GetCurrentThreadId();
	}

	thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, (DWORD)threadInfo->thread_id);
	if (thread == NULL) {
		/* failed to get a handle to the specified thread */
		ReleaseMutex(PPG_osBacktrace_Mutex);
		return 0;
	}

	if (sigInfo != NULL) {
		memcpy(&threadContext, sigInfo->ContextRecord, sizeof(CONTEXT));
	} else {
		memcpy(&threadContext, threadInfo->context, sizeof(CONTEXT));
	}

	/* check that we have loaded the dbg help library functions and symbols */
	if (0 == load_dbg_functions(portLibrary) && 0 == load_dbg_symbols(portLibrary)) {
		/* initialize the stackframe */
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrStack.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Mode = AddrModeFlat;

#ifdef WIN64
		stackFrame.AddrPC.Offset = threadContext.Rip;
		stackFrame.AddrStack.Offset = threadContext.Rsp;
		stackFrame.AddrFrame.Offset = threadContext.Rbp;
#else
		stackFrame.AddrPC.Offset = threadContext.Eip;
		stackFrame.AddrStack.Offset = threadContext.Esp;
		stackFrame.AddrFrame.Offset = threadContext.Ebp;
#endif

		rc = TRUE;
		nextFrame = &threadInfo->callstack;

		/* now walk the stack and maintain a count of the frames */
		for (frameNumber = 0; rc != FALSE; frameNumber++) {
			rc = PPG_dbgHlpLibraryFunctions->StackWalk64(
#ifdef WIN64
					 IMAGE_FILE_MACHINE_AMD64,
#else
					 IMAGE_FILE_MACHINE_I386,
#endif
					 process,
					 thread,
					 &stackFrame,
					 &threadContext, /* thread context not needed for x86, however is required for AMD64 */
					 NULL,
					 PPG_dbgHlpLibraryFunctions->SymFunctionTableAccess64,
					 PPG_dbgHlpLibraryFunctions->SymGetModuleBase64,
					 NULL);

			if (rc == 0) {
				break;
			}

			if (heap == NULL) {
				*nextFrame = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9PlatformStackFrame), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			} else {
				*nextFrame = portLibrary->heap_allocate(portLibrary, heap, sizeof(J9PlatformStackFrame));
			}

			if (*nextFrame == NULL) {
				if (!threadInfo->error) {
					threadInfo->error = ALLOCATION_FAILURE;
				}

				break;
			}

			(*nextFrame)->parent_frame = NULL;
			(*nextFrame)->symbol = NULL;

			/*
			 * this truncates from DWORD64 to uintptr_t on Win32 systems. However win32 is exclusively little endian so this
			 * works as expected
			 */
			(*nextFrame)->instruction_pointer = (uintptr_t)stackFrame.AddrPC.Offset;
			(*nextFrame)->stack_pointer = (uintptr_t)stackFrame.AddrStack.Offset;
			(*nextFrame)->base_pointer = (uintptr_t)stackFrame.AddrFrame.Offset;

			if (stackFrame.AddrReturn.Offset == 0) {
				/* sanity check so we don't walk past a broken frame */
				break;
			}
			nextFrame = &(*nextFrame)->parent_frame;
		}
	}

	CloseHandle(thread);
	ReleaseMutex(PPG_osBacktrace_Mutex);

	return frameNumber;
}



/* This function takes a thread structure already populated with a backtrace by omrintrospect_backtrace_thread
 * and looks up the symbols for the frames. The format of the string generated is:
 * 		symbol_name (statement_id instruction_pointer [module+offset])
 * If it isn't possible to determine any of the items in the string then they are omitted. If no heap is specified
 * then this function will use malloc to allocate the memory necessary for the symbols which must be freed by the caller.
 *
 * @param portLbirary a pointer to an initialized port library
 * @param threadInfo a thread structure populated with a backtrace
 * @param heap a heap from which to allocate any necessary memory. If NULL malloc is used instead.
 *
 * @return the number of frames for which a symbol was constructed.
 */
uintptr_t
omrintrospect_backtrace_symbols_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap)
{
	J9PlatformStackFrame *frame;
	unsigned int i = 0;
	IMAGEHLP_LINE64 line_info;
	IMAGEHLP_MODULE64 module_info;
	HANDLE process = NULL;

	/* CAUTION: symbol_info is mapped to the start of buffer, this is because the Name field in */
	/* the SYMBOL_INFO structure is a 1 char array positioned at the end of the structure */
	/* The APIs that use this structure rely on there being extra memory available at the end */
	/* of the structure */
	BYTE buffer[256];
	PSYMBOL_INFO symbol_info = (PSYMBOL_INFO)buffer;

	if (NULL == PPG_osBacktrace_Mutex) {
		/* Create an anonymous mutex with this thread as the owner */
		PPG_osBacktrace_Mutex = CreateMutex(NULL, TRUE, NULL);
		if (NULL == PPG_osBacktrace_Mutex) {
			/* Couldn't create or obtain a mutex */
			return 0;
		}
	} else {
		/* mutex already exists, so go and acquire it */
		WaitForSingleObject(PPG_osBacktrace_Mutex, INFINITE);
	}

	symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol_info->MaxNameLen = sizeof(buffer) - sizeof(SYMBOL_INFO) + 1;

	/* Get the current process and thread handles */
	process = GetCurrentProcess();

	/* check that we have loaded the dbg help library functions and symbols */
	if (0 == load_dbg_functions(portLibrary) && 0 == load_dbg_symbols(portLibrary)) {
		/* work through the address_array and look up the symbol for each address */
		for (i = 0, frame = threadInfo->callstack; frame != NULL; frame = frame->parent_frame, i++) {
			BOOLEAN rc = FALSE;
			DWORD64 symbol_offset;
			DWORD disp;
			size_t length;
			char *module_name = "";
			char *symbol_name = "";
			char *file_name = "";
			DWORD64 module_offset = 0;
			char output_buf[512];
			char *cursor = output_buf;

			line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

			rc = PPG_dbgHlpLibraryFunctions->SymGetModuleInfo64(process, frame->instruction_pointer, &module_info);
			if (rc == FALSE) {
				module_name = "";
			} else {
				module_name = module_info.ModuleName;
				module_offset = frame->instruction_pointer - module_info.BaseOfImage;
			}

			rc = PPG_dbgHlpLibraryFunctions->SymFromAddr(process, (uint64_t)frame->instruction_pointer, &symbol_offset, symbol_info);
			if (rc == FALSE) {
				symbol_name = "";
			} else {
				symbol_name = symbol_info->Name;
			}

			rc = PPG_dbgHlpLibraryFunctions->SymGetLineFromAddr64(process, (DWORD64)frame->instruction_pointer, &disp, &line_info);
			if (rc == FALSE) {
				file_name = "";
			} else {
				/* strip off the full path if possible */
				file_name = strrchr(line_info.FileName, '\\');
				if (file_name == NULL) {
					file_name = line_info.FileName;
				} else {
					/* skip the backslash */
					file_name++;
				}
			}

			/* symbol_name+offset (id, instruction_pointer [module+offset]) */
			if (symbol_name[0] != '\0') {
				cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "%s", symbol_name);
				cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "+0x%x ", symbol_offset);
			}
			*(cursor++) = '(';
			if (file_name[0] != '\0') {
				cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "%s:%i, ", file_name, line_info.LineNumber);
			}
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "0x%p", frame->instruction_pointer);
			if (module_name[0] != '\0') {
				cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), " [%s+0x%x]", module_name, module_offset);
			}
			*(cursor++) = ')';
			*cursor = 0;

			length = (cursor - output_buf) + 1;
			if (heap == NULL) {
				frame->symbol = portLibrary->mem_allocate_memory(portLibrary, length, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			} else {
				frame->symbol = portLibrary->heap_allocate(portLibrary, heap, length);
			}

			if (frame->symbol != NULL) {
				strncpy(frame->symbol, output_buf, length);
			} else {
				frame->symbol = NULL;
				if (!threadInfo->error) {
					threadInfo->error = ALLOCATION_FAILURE;
				}
				i--;
			}
		}
	}

	ReleaseMutex(PPG_osBacktrace_Mutex);

	return i;
}
