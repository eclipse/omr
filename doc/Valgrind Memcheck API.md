<!--
Copyright (c) 2017, 2017 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at http://eclipse.org/legal/epl-2.0
or the Apache License, Version 2.0 which accompanies this distribution
and is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following Secondary
Licenses when the conditions for such availability set forth in the
Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
version 2 with the GNU Classpath Exception [1] and GNU General Public
License, version 2 with the OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

# Valgrind Memchek API

## About Memcheck
Memcheck is a memory error detector. It can be used to detect detect memory leaks and other memory access errors in objects used in languages built with OMR.

## Need for using API
Valgrind monitors system calls for creation and deletion of objects and also memory operations using its own synthetic CPU to detect memory access errors. 

OMR's garbage collector has Valgrind integration to allow Valgrind to know when objects are allocated the free'd on the heap. For most programs, Valgrind will work properly out of the box, but it requires help to understand custom heaps. Valgrind will see the entire heap as a single object, and will not recognize the individual objects allocated inside of the heap. This prevents valgrind from showing errors when memory is accessed outside of any allocated object, or even when multiple objects are allocated at the same memory address inside the heap.

## API requests used
We have used following valgrind API requests:

1.  `VALGRIND_MAKE_MEM_NOACCESS(lowAddress, size)` - This marks address ranges as completely inaccessible. This is mainly used when a new range added to the heap in `MM_HeapVirtualMemory::heapAddRange`.

2. `VALGRIND_MAKE_MEM_DEFINED(lowAddress, size)` - This marks range as accessable, mainly used when we need to access some area of heap that is unallocated (NOACCESS) and then mark it back with `VALGRIND_MAKE_MEM_NOACCESS`.

3. `VALGRIND_MAKE_MEM_UNDEFINED(lowAddress, size)` - This marks address ranges as accessible but containing undefined data. This request is used in `MM_HeapVirtualMemory::heapRemoveRange` to let Valgrind know that the memory is no longer in use for the heap.

4. `VALGRIND_CREATE_MEMPOOL(pool, rzB, is_zeroed)` - pool provides the anchor address for our memory pool. We have set it to  `handle->getMemoryBase(),` in `MM_MemoryManager::newInstance` and saved it to `valgrindMempoolAddr` in `MM_ExtensionsBase` to identify the mempool whenever we add or remove objects. We also set is_zeroed to 1 to tell valgrind that object is defined just after allocation request is used.

5. `VALGRIND_MEMPOOL_ALLOC(pool, addr, size)` - This request is used to inform valgrind that object of `size` has been allocated at `addr` inside `pool`.  This also marks the object as `DEFINED`. The request is added to `allocateAndInitializeObject(OMR_VMThread *omrVMThread)`.

6. `VALGRIND_MEMPOOL_FREE(pool, addr)` - This request is used to inform valgrind that object at `addr` has been freed from the `pool`.  This also marks the object as `NOACCESS`. The request is to be used in `MM_SweepPoolManagerAddressOrderedListBase::addFreeMemory(env, *sweepChunk, *address, size)`

7. `VALGRIND_DESTROY_MEMPOOL(pool)` - This request destroys valgrind `pool` along with objects allocated on it. This request is used in `MM_MemoryManager::destroyVirtualMemory`. So to avoid any area getting marked as noaccess fafter a GC cycle is complete, it is nececcary that all objects inside it to be freed before (or inside) `MM_HeapVirtualMemory::heapRemoveRange` that marks the area as undefined at end.

8. `VALGRIND_PRINTF(format, ...)` and `VALGRIND_PRINTF_BACKTRACE(format, ...)` - These requests print messages on valgrind terminal. We use them to  print request logs with stack trace. They are placed to debug internal integration of API. To enable them turn on `VALGRIND_REQUEST_LOGS` in MemcheckWrapper.

## Different Ways of freeing objects in Valgrind.

API request `VALGRIND_MEMPOOL_FREE(pool, addr)` requires us to have base address of object that we are attempting to free. GC keeps a record of the heap areas that are available or free. The GC is informed of the live objects during the marking phase of the GC in `MM_MarkingDelegate::scanRoots`, where all live objects are noted. The GC combines all unused memory into a "free list" which is used for future allocations. Proper Valgrind integration requires `VALGRIND_MEMPOOL_FREE` to be called on each object freed during a garbage collection, which is not something that the GC tracks

There are 3 workarounds for this issue.

1. Using `MM_MarkingDelegate::masterCleanupAfterGC`. Target language has record of objects in a object table, and this method is called after GC cycle has been completed with a record of marked objects. While it is freeing the dead objects from object table we are also requesting valgrind to free them. **Disadvantage:** free requests should have been made inside the GC (where we also request allocations) and not in target language.
 
 2. Using a list to track allocated objects. This workaround allows us to free objects from GC directly. This is how we are freeing them currently. **Disadvantage** we haven't needed such a list in current Implementation, it is a overhead to maintain it later and increases complexity of code. Another disadvantage is we cannot access this list everywhere in GC (where `MM_ExtensionsBase` is absent)

 3. Adding new valgrind API: Valgrind already has the list of allocated objects, so it should be possible for it to find the objects in a range and also free them. A patch has been made for this and it is in their mailing lists for review. https://sourceforge.net/p/valgrind/mailman/message/35996446/ . This does not require to have a track of allocated objects. This should be best solution, but until it is merged in their code, it will be difficult to use.

## Debugging and extending API usage in GC

All api requests are made using a wrapper file `MemcheckWrapper.cpp`. Refer `MemcheckWrapper.hpp`for more documentation. Enable `VALGRIND_REQUEST_LOGS` to get verbose logs/

## Running Valgrind

1. **Preparing Makefiles** - To compile with valgrind API enabled, use build option  `--enable-OMR_VALGRIND_MEMCHECK` while generating makefiles. Valgrind documentation also recommends turning off optimizations. Their docs say:
    > If you are planning to use Memcheck: On rare occasions, compiler optimisations (at -O2 and above, and sometimes -O1) have been observed to generate code which fools Memcheck into wrongly reporting uninitialised value errors.

    So for given example, makefiles are generated by: 
    
        make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue EXTRA_CONFIGURE_ARGS='--disable-optimized --enable-OMR_VALGRIND_MEMCHECK'

2. **Running ExampleVM** - run `valgrind --trace-children=yes --leak-check=full ./omrgcexample`. It has infinite allocations, so you need to terminate in manually (`Ctrl + C`) and see the leaks.

3. **Running GC Tests** - run ` valgrind --trace-children=yes --leak-check=full ./omrgctest`.

## Examining problems

1. **Dumping logs** - Valgrind can generate a lot of errors, it is convinient to dump them with `valgrind --trace-children=yes --leak-check=full ./omrgctest  > valGcDump 2>&1`.

2. **Error Count** - last line of output indicates errors. Example:  
    > ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

3. **Invalid Read / Write errors** - these are errors that are very helpful. They are detailed and may also contain location of previous objects they are occurring, Example:
       
        Invalid read of size 8
        Address 0x6d71d18 is 1,104 bytes inside a block of size 1,448 free'd  
        <StackTrace>
    This means that a read attempt of size 8 byes has been made at adddress `0x6d71d18` which is not allocated to any object.
    
    Further it says that the address is 1104 bytes inside a block of size 1448. This means a block was previously allocated there, starting from `0x6D72168` (0x6d71d18 - 1104 (decimal)) to `0x6D72710` (0x6D72168 + 1448 (decimal)).

    *Example:* `cat valGcDump | grep "invalid" -i -C50 --color=always | less -R`

4. **Increasing logs** - along with Valgrind logs, GC logs can also be enabled using `-logLevel=verbose` option. This enables logs such as:

        Allocate object name: objM_11_203(0000000006826AB8[0x148])
	    add child objM_11_203(0000000006826AB8) to parent objM_10_101(0000000006773800) slot 0000000006773810[6826ab8].

    *Example:* `valgrind --trace-children=yes --leak-check=full ./omrgctest -logLevel=verbose > valGcDump 2>&1`.

## Using GDB

Valgrind can be hooked with gdb. Not all gdb commands are supported (example run and step commands don't work while continue does).

1. Start the program with valgrind and vgdb options, `--vgdb=yes` and `--vgdb-error=0` . The later option specifier the number of errors after which gdb will become active. Specifying 0 means that gdb will become active at startup and breakpoints can be inserted here.

2. Start GDB with arguments same as the program, example for `omrgcexample` we have 2 terminals with commands

        $ valgrind --trace-children=yes --vgdb=yes --vgdb-error=0 --leak-check=full ./omrgctest -logLevel=verbose
        $ gdb --args ./omrgcexample -logLevel=verbose

3. Hook GDB with valgrind. Terminal running valgrind will give the gdb command we need.
    
        target remote | /usr/local/lib/valgrind/../../bin/vgdb --pid=18203
    pid is optional as long as only one valgrind process is running.

4. Setup breakpoints and continue. valgrind will stop and return to gdb whenever a error is encountered or a breakpoint.

5. `monitor leak_check` run this to get valgrind leak summary of program at the moment.

6. `monitor v.kill` this command will kill valgrind vgdb process.
