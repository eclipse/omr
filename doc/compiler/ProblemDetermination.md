<!--
Copyright (c) 2016, 2017 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath 
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

#  Introduction

The Testarossa compiler technology is shipped as part of a number of compiler
products, such as IBM J9. It consumes a description of a program (in bytecode,
or through the direct generation of intermediate language) into native code,
thereby improving the performance of the application. During the compilation,
the Testarossa based compilers performs analyses and optimizes the program.

As with any complex software system, occasionally the compiler might
malfunction. Typical symptoms include:

  * incorrect results or exceptions are produced by a correct but unusual,
    or complex program;
  * the VM crashes, resulting in a core file and/or crash report. 

This document discusses troubleshooting methods with which you could try to
determine whether the compiler is faulty, and which part of the compiler caused
a problem.

#  Invocation

The most common usecase of Testarossa is in JIT compilation where it is
typically invoked by the VM automatically; the run-time behaviour of the JIT
can only be controlled via options passed through VM command line or, in some
cases, environment variables.

Generally speaking, the JIT can operate in two different modes:

  1. recompilation
  2. fixed optimization level

In many VMs, by default, the JIT is enabled in recompilation mode, i.e. more
frequently executed methods are compiled more often, with increasingly
aggressive optimizations. This mode tends to maximize the performance gain from
JIT compilation while minimizing its cost.

Alternatively, the JIT can operate at a fixed optimization level, which forces
it to compile all methods (or only certain methods selected by the user) with
the same aggresiveness. As you will see in this document, this mode is
extremely useful for diagnosing JIT problems.
  
    
# Isolating a Failure

# Completely Disabling the Compiler 

The first step in diagnosing a failure is to determine whether the problem is
in fact related to the compiler. When Testarossa is being used as a JIT compiler, 
there is almost certainly an interpreter to fall back to, which also means 
a bug could be in the interpreter, not the compiler. 

Running the program with the compiler disabled leads to one of two outcomes:

  * The failure remains. The problem is, therefore, not related to the compiler. In
    some cases, the program might start failing in a different manner;
    nevertheless, the problem is not in the compiler. You should consult other
    documentation on VM problem determination,
  * The failure disappears. The problem is most likely, although _not
    definitely_, in the compiler.

# Reducing Optimization Levels 

If the failure of your program appears to come from a problem within the
compiler, you can try to narrow down the problem further by reducing the amount
of optimizations performed by the compiler.

The compile roptimizes methods at various optimization levels; that is, different
selections of optimizations are applied to different methods, based on their
call counts. Methods that are called more frequently are optimized at higher
levels. By changing compiler parameters, you can control the optimization level at
which methods are optimized, and determine whether the optimizer is at fault
and, if it is, which optimization is problematic.

If it is supported in your VM, the first compiler parameter to try is `count=0`,
which sets the compilation  threshold to zero and effectively causes the
program to be run in purely compiled mode, and tends to simplify the remainder
of the problem determination process. If the failure disappears when `count=0`
is used, try other count values such as 1 and 10. 

In systems with concurrency, Changing the compiler threshold could affect the order in
which compilations and VM state changes occur. If the failure does not
occur when any compiler threshold is set, omit the `count=` parameter.

Another useful compiler parameter is `disableInlining`. With this parameter set, the
compiler is prohibited from generating larger and more complex code that is the side
effect of inlining, in an attempt to perform aggressive optimizations. If the
failure disappears when `disableInlining` is used, omit the parameter.

Now try decreasing the compiler optimization levels. The various optimization
levels are, depending on the implementation, typically 

  1. `scorching`
  2. `veryHot`
  3. `hot`
  4. `warm`
  5. `cold`
  6. `noOpt`

from the most aggressive to the least expensive.

Run the program with the `optlevel=` parameter:

    
    count=0,disableInlining,optLevel=scorching
    

Try each of the optimization levels in turn, and record your observations,
until the `noOpt` level is reached, or until the failure cannot be reproduced.
The last optimization level that still triggers the failure is the level that
contains a potentially faulty optimization. If the program still fails at
`noOpt`, then the problem is most likely in the code generation phase (versus
the optimization phase) of the compiler.

# Locating the Failing Method 

When you have arrived at the lowest optimization level at which the compiler must
compile methods to trigger the failure, you can try to find out which part of
the program, when compiled, causes the failure. You can then instruct the
compiler, when running as a JIT, to limit the workaround to a specific method,
class, or package, allowing the compiler to compile the rest of the program as it
normally would. If the failure occurs with `optLevel=noOpt`, you can also
instruct the compiler to not compile the method or methods that are causing the
failure (thus avoiding it).

To locate the method that is causing the failure, follow these steps:

1. Run the program with the compiler options `verbose` and
   `vlog=filename`. With these options, the JIT reports its progress, as it
   compiles methods, in a verbose log file, also called a **limit file**. A
   typical limit file contains lines that correspond to compiled methods, like: 
    
    
        + (hot) java/lang/Math.max(II)I @ 0x10C11DA4-0x10C11DDD <... snipped ...> 
       

   Lines that do not start with the plus sign are ignored by the JIT in the
   steps below, so you can edit them out of the file.

2. Run the program again with the compiler option `limitFile=(filename,m,n)`,
   where `filename` is the path to the limit file, and `m` and `n` are line
   numbers indicating respectively the first and the last methods in the limit
   file that should be compiled. This parameter causes the JIT to compile only the
   methods listed on lines `m` to `n` in the limit file. Methods not listed in
   the limit file and methods listed on lines outside the range are not compiled.
   Repeat this step with a smaller range if the program still fails.
   
   The recommended number of lines to select from the limit file in each
   repetition is half of the previous selection, such that this step consists of
   an efficient binary search for the failing method.

3. If the program no longer fails, one or more of the methods that you have
   removed in the last iteration must have been the cause of the failure. Invert
   the selection (select methods not selected in the previous iteration), and
   repeat the previous step to see if the program starts to the fail again.

4. Repeat the last two steps, as many times as necessary, to find the minimum
   number of methods that must be compiled to trigger the failure. Often, you can
   reduce the file to a single line.

When you have located the failing method, you can restrict compiler options to
the failing method, so that the remainder of the application can run normally.
For example, if the method `test.rb:61:pass` causes the program to
fail when compiled with `optLevel=hot`, you can run the program with:

    
    
    {test.rb:61:pass}(optLevel=warm,count=0)


which tells the compiler to compile only the troublesome method at the `warm`
optimization level, but recompile all other methods normally. Note, in most shells
the limit syntax uses special characters, and needs to be quoted. 

If a method fails when it is compiled at `noOpt`, you can exclude it from
compilation altogether, using the `exclude=method` parameter:
    
    exclude={test.rb:61:pass}

# Identifying the failing Optimization.

Most transformations that can be elided are guarded in the source code by a special
check called `performTransformation` that can be controlled by the option 
`lastOptIndex=N`. This tells the compiler to perform `N` optional transformations, 
then proceed to only do required operations. In this sense, `lastOptIndex=-1` is an
even stronger property than `optLevel=noOpt`. 

By binary searching `lastOptIndex=` you can find the optimization that failed.  
    

# Identifying Compilation Failures

If the VM crashes, and you can see that the crash has occurred in the compiler 
module, it means that either the compier has failed during an attempt to compile a
method, or one of the JIT run-time routines, which are also contained in the
JIT module, has failed. Usually the VM prints this information on standard
error just before it terminates; the information is also recorded in the corefile
if one is produced. 

To see if the compiler is crashing in the middle of a compilation, use the
`verbose` option with the following additional settings:
    
    verbose={compileStart|compileEnd}
    

These verbose settings report when the compiler starts to compile a method, and
when it ends. If the compiler fails on a particular method (that is, it starts
compiling, but crashes before it can end), use the `exclude=` parameter to
prevent the compiler from compiling the method.

# Options 

## Option Sets

It is possible to apply a set of compile options ("option sets") only to a
method, or group of methods. Furthermore, it is possible to apply different
option sets to different (groups of) methods. An option set can be defined on
the command line by enclosing the desired set of parameters in parentheses,
and preceding it with a method filter expression, e.g.

    
    
    {HelloWorld.main([Ljava/lang/String;)V}(count=0,optlevel=hot)
    

The option example below makes the compiler compile all methods at the warm
optimization level, and trace the compilation of all methods that start 
with `Frobnicate` and `Brobnicate`  in two different log files.

    
    
    optlevel=warm,count=0,{Frobnicate*}(log=frob.log,traceFull),{Brobnicate*}(log=brob.log,traceFull)
    

Option sets can also be defined in a limit file, if one is used, by adding a
non-zero integer immediately after the initial plus sign on the line with the
selected method(s).

    
    
    +1 (hot) HelloWorld.main([Ljava/lang/String;)V @ 0x10C11DA4-0x10C11DDD
    

The number is the option set number, which can be used on the command line in
lieu of a method filter expression, e.g.

    
    
    1(count=0,optlevel=hot)
    

Note that some parameters are always applied globally and are illegal within
option sets. The compiler will fail to start up if an option set contains such
parameters.

Here are some options, though, not all are connected up to all compiler technologies. 

## Code Generation Parameters

| Option                                     | Description                                     |
| ------------------------------------------ | ----------------------------------------------- |
| bcLimit=<em>nnn</em>                       | bytecode size limit                             |
| code=<em>nnn</em>                          | code cache size, in KB                          |
| codetotal=<em>nnn</em>                     | total code memory limit, in KB                  |
| noExceptions                               | fail compilation for methods with exceptions    |
| noregmap                                   | generate GC maps without register maps          |

## Optimization Parameters

| Option                                           | Description                                                                                                                                                                                |
| ------------------------------------------------ | ------------------------------------------------------------------------------- |
| acceptHugeMethods                                | allow processing of really large methods                                        |
| count=<em>nnn</em>                               | number of invocations before compiling methods without loops                    |
| disableAndSimplification                         | disable and simplification                                                      |
| disableBasicBlockExtension                       | disable basic block extension                                                   |
| disableBasicBlockSlicing                         | disable basic block slicing                                                     |
| disableBlockSplitter                             | disable block splitter                                                          |
| disableBlockVersioner                            | disable block versioner                                                         |
| disableCallGraphInlining                         | disable Interpreter Profiling based inlining and code size estimation           |
| disableCatchBlockRemoval                         | disable catch block removal                                                     |
| disableCFGSimplification                         | disable Control Flow Graph simplification                                       |
| disableColdBlockMarker                           | disable detection of cold blocks                                                |
| disableColdBlockOutlining                        | disable outlining of cold blocks                                                |
| disableCompactLocals                             | disable compact locals                                                          |
| disableCriticalEdgeSplitting                     | disable critical edge splitting                                                 |
| disableDeadTreeElimination                       | disable dead tree elimination                                                   |
| disableGlobalDSE                                 | disable global dead store elimination                                           |
| disableGlobalRegisterCandidates                  | disable global register candidates                                              |
| disableGlobalVP                                  | disable global value propagation                                                |
| disableGLU                                       | disable general loop unroller                                                   |
| disableGRA                                       | disable IL based global register allocator                                      |
| disableInlining                                  | disable IL inlining                                                             |
| disableInnerPreexistence                         | disable inner preexistence                                                      |
| disableInternalPointers                          | disable internal pointer creation                                               |
| disableIsolatedSE                                | disable isolated store elimination                                              |
| disableLiveRegisterAnalysis                      | disable live register analysis                                                  |
| disableLocalCSE                                  | disable local common subexpression elimination                                  |
| disableLocalDSE                                  | disable local dead store elimination                                            |
| disableLocalLiveVariablesForGC                   | disable local live variables for GC                                             |
| disableLocalReordering                           | disable local reordering                                                        |
| disableLocalVP                                   | disable local value propagation                                                 |
| disableLongDispStackSlot                         | disable use of stack slot for handling long displacements on z                  |
| disableLoopCanonicalization                      | disable loop canonicalization                                                   |
| disableLoopInversion                             | disable loop inversion                                                          |
| disableLoopReduction                             | disable loop reduction                                                          |
| disableLoopReplicator                            | disable loop replicator                                                         |
| disableLoopStrider                               | disable loop strider                                                            |
| disableLoopUnroller                              | disable loop unroller                                                           |
| disableLoopVersioner                             | disable loop versioner                                                          |
| disableMergeStackMaps                            | disable stack map merging                                                       |
| disableMoreOpts                                  | apply noOpt optimization level and disable codegen optimizations                |
| disableNewBlockOrdering                          | disable new block ordering, instead use basic block extension                   |
| disableNewBVA                                    | disable structure based bit vector analysis                                     |
| disableNonvirtualInlining                        | disable inlining of non virtual methods                                         |
| disableOpts={<em>regex</em>}                     | list of optimizations to disable                                                |
| disableOptTransformations={<em>regex</em>}       | list of optimizer transformations to disable                                    |
| disablePRE                                       | disable partial redudndancy elimination                                         |
| disableRematerialization                         | disable rematerialization                                                       |
| disableReorderArrayIndexExpr                     | disable reordering of index expressions                                         |
| disableRXusage                                   | disable increased usage of RX instructions                                      |
| disableSequenceSimplification                    | disable arithmetic sequence simplification                                      |
| disableSequentialStoreSimplification             | disable sequential store simplification phase                                   |
| disableTraceRegDeps                              | disable printing of register dependancies for each instruction in trace file    |
| disableTreeCleansing                             | disable tree cleansing                                                          |
| disableTreeSimplification                        | disable tree simplification                                                     |
| disableTrex                                      | disable Trex Instructions on z/Architecture.                                    |
| disableValueProfiling                            | disable value profiling                                                         |
| disableVerification                              | disable verification of internal data structures between passes                 |
| disableVirtualGuardNOPing                        | disable virtual guard NOPing                                                    |
| disableVirtualGuardTailSplitter                  | disable virtual guard tail splitter                                             |
| disableVirtualInlining                           | disable inlining of virtual methods                                             |
| disableVMThreadGRA                               | disable reuse of the vmThread's real register as a global register              |
| disableYieldVMAccess                             | disable yielding of VM access when GC is waiting                                |
| disableZ6                                        | disable z6 Instructions on z.                                                   |
| disableZArchitecture                             | disable zArchitecture Instructions on z.                                        |
| dontInline={<em>regex</em>}                      | list of methods to not inline                                                   |
| enable390FreeVMThreadReg                         | enable use of vm thread reg as assignable reg on z.                             |
| enable390GlobalAccessRegs                        | enable use of access regs for global allocation on z.                           |
| enableBasicBlockHoisting                         | enable basic block hoisting                                                     |
| enableLocalLiveRangeReduction                    | enable local live range reduction                                               |
| enableLoopyMethodForcedCompilations              | enable compiling loopy methods with noOpt,count=0                               |
| enableRangeSplittingGRA                          | enable GRA splitting of live ranges to reduce register pressure                 |
| enableReorderArrayIndexExpr                      | reorder array index expressions to encourage hoisting                           |
| enableUpgradingAllColdCompilations               | try to upgrade to warm all cold compilations                                    |
| enableVpic                                       | enable PIC for resolved virtual calls                                           |
| firstOptIndex=<em>nnn</em>                       | index of the first optimization to perform                                      |
| firstOptTransformationIndex=<em>nnn</em>         | index of the first optimization transformation to perform                       |
| ignoreIEEE                                       | allow non-IEEE compliant optimizations                                          |
| insertDebuggingCounters=<em>nnn</em>             | Insert instrumentation for debugging counters                                   |
| lastOptIndex=<em>nnn</em>                        | index of the last optimization to perform                                       |
| lastOptTransformationIndex=<em>nnn</em>          | index of the last optimization transformation to perform                        |
| numRestrictedGPRs=<em>nnn</em>                   | number of restricted GPRS (0-5).  Currently z only                              |
| onlyInline={<em>regex</em>}                      | list of methods that can be inlined                                             |
| optLevel=cold                                    | compile all methods at cold level                                               |
| optLevel=hot                                     | compile all methods at hot level                                                |
| optLevel=noOpt                                   | compile all methods at noOpt level                                              |
| optLevel=scorching                               | compile all methods at scorching level                                          |
| optLevel=veryHot                                 | compile all methods at veryHot level                                            |
| optLevel=warm                                    | compile all methods at warm level                                               |
| paranoidOptCheck                                 | check the trees and cfgs after every optimization phase                         |
| suffixLogs                                       | add the date/time/pid suffix to the file name of the logs                       |
| suffixLogsFormat=                                | add the suffix in specified format to the file name of the logs                 |
| tlhPrefetchSize=<em>nnn</em>                     | allocation prefetch size for X86 allocation prefetch                            |

## Logging and Trace Parameters
| Option                                                                        | Description                                                                                            |
| ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| debugCounters=                                                                | Activate dynamic Debug counters                                                                        |
| firstVlogLine=<em>nnn</em>                                                    | first vlog line to be written                                                                          |
| lastVlogLine=<em>nnn</em>                                                     | last vlog line to be written                                                                           |
| log=<em>filename</em>                                                         | write log output to <em>filename</em>                                                                  |
| optDetails                                                                    | log all optimizer transformations                                                                      |
| stats                                                                         | dump statistics at end of run                                                                          |
| traceAliases                                                                  | trace alias set generation                                                                             |
| traceAllocationSplitter                                                       | trace allocation splitter                                                                              |
| traceAndSimplification                                                        | trace and simplification                                                                               |
| traceBasicBlockExtension                                                      | trace basic block extension                                                                            |
| traceBasicBlockHoisting                                                       | trace basic block hoisting                                                                             |
| traceBasicBlockSlicing                                                        | trace basic block slicing                                                                              |
| traceBBVA                                                                     | trace backward bit vector analysis                                                                     |
| traceBC                                                                       | dump bytecodes                                                                                         |
| traceBin                                                                      | dump binary instructions                                                                               |
| traceBlockFrequencyGeneration                                                 | trace block frequency generation                                                                       |
| traceBlockSplitter                                                            | trace block splitter                                                                                   |
| traceBVA                                                                      | trace bit vector analysis                                                                              |
| traceCatchBlockRemoval                                                        | trace catch block removal                                                                              |
| traceCFGSimplification                                                        | trace Control Flow Graph simplification                                                                |
| traceCG                                                                       | dump output of code generation passes                                                                  |
| traceCGStatistics                                                             | report code generation statistics per method                                                           |
| traceColdBlockMarker                                                          | trace detection of cold blocks                                                                         |
| traceColdBlockOutlining                                                       | trace outlining of cold blocks                                                                         |
| traceCompactLocals                                                            | trace compact locals                                                                                   |
| traceCompactNullChecks                                                        | trace compact null checks                                                                              |
| traceCriticalEdgeSplitting                                                    | trace critical edge splitting                                                                          |
| traceDeadTreeElimination                                                      | trace dead tree elimination                                                                            |
| traceEscapeAnalysis                                                           | trace escape analysis                                                                                  |
| traceExplicitNewInitialization                                                | trace explicit new initialization                                                                      |
| traceFieldPrivatization                                                       | trace field privatization                                                                              |
| traceForCodeMining={<em>regex</em>}                                           | add instruction annotations for code mining                                                            |
| traceFull                                                                     | turn on all trace options                                                                              |
| traceGlobalDSE                                                                | trace global dead store elimination                                                                    |
| traceGlobalLiveVariablesForGC                                                 | trace global live variables for GC                                                                     |
| traceGlobalRegisterCandidates                                                 | trace global register candidates                                                                       |
| traceGlobalVP                                                                 | trace global value propagation                                                                         |
| traceGLU                                                                      | trace general loop unroller                                                                            |
| traceGRA                                                                      | trace IL based global register allocator                                                               |
| traceInductionVariableAnalysis                                                | trace Induction Variable Analysis                                                                      |
| traceInlining                                                                 | trace IL inlining                                                                                      |
| traceInnerPreexistence                                                        | trace inner preexistence                                                                               |
| traceInvariantArgumentPreexistence                                            | trace invariable argument preexistence                                                                 |
| traceIsolatedSE                                                               | trace isolated store elimination                                                                       |
| traceLiveness                                                                 | trace liveness analysis                                                                                |
| traceLocalCSE                                                                 | trace local common subexpression elimination                                                           |
| traceLocalDSE                                                                 | trace local dead store elimination                                                                     |
| traceLocalLiveVariablesForGC                                                  | trace local live variables for GC                                                                      |
| traceLocalReordering                                                          | trace local reordering                                                                                 |
| traceLocalVP                                                                  | trace local value propagation                                                                          |
| traceLoopCanonicalization                                                     | trace loop canonicalization                                                                            |
| traceLoopInversion                                                            | trace loop inversion                                                                                   |
| traceLoopReduction                                                            | trace loop reduction                                                                                   |
| traceLoopReplicator                                                           | trace loop replicator                                                                                  |
| traceLoopStrider                                                              | trace loop strider                                                                                     |
| traceLoopUnroller                                                             | trace loop unroller                                                                                    |
| traceMethodMetaData                                                           | dump method meta data                                                                                  |
| traceMethodSpecializer                                                        | trace method specializer                                                                               |
| traceMixedModeDisassembly                                                     | dump generated assembly with bytecodes                                                                 |
| traceNodeFlags                                                                | trace setting/resetting of node flags                                                                  |
| traceOpts={<em>regex</em>}                                                    | list of optimizations to trace                                                                         |
| traceOptTrees                                                                 | dump trees after each optimization                                                                     |
| tracePostBinaryEncoding                                                       | dump instructions (code cache addresses, real registers) after binary encoding                         |
| tracePostInstructionSelection                                                 | dump instructions (virtual registers) after instruction selection                                      |
| tracePostRegisterAssignment                                                   | dump instructions (real registers) after register assignment                                           |
| tracePostScheduling                                                           | dump instructions (real registers) after instruction scheduling                                        |
| tracePRE                                                                      | trace partial redudndancy elimination                                                                  |
| tracePreInstructionSelection                                                  | dump trees prior to instruction selection                                                              |
| traceProfileGenerator                                                         | trace profile generator                                                                                |
| traceRA=                                                                      | trace register allocations                                                                             |
| traceRedundantAsyncCheckRemoval                                               | trace redundant async check removal                                                                    |
| traceRedundantGotoElimination                                                 | trace redundant goto elimination                                                                       |
| traceRedundantMonitorElimination                                              | trace redundant monitor elimination                                                                    |
| traceRegisterPressureDetails                                                  | include extra register pressure annotations in register pressure simulation and tree evaluation traces |
| traceRematerialization                                                        | trace rematerialization                                                                                |
| traceReorderArrayIndexExpr                                                    | trace reorder array index expressions                                                                  |
| traceSequenceSimplification                                                   | trace arithmetic sequence simplification                                                               |
| traceSplitMethod                                                              | trace method splitting                                                                                 |
| traceStoreSinking                                                             | trace store sinking                                                                                    |
| traceStringPeepholes                                                          | trace string peepholes                                                                                 |
| traceSwitchAnalyzer                                                           | trace switch analyzer                                                                                  |
| traceTreeCleansing                                                            | trace tree cleansing                                                                                   |
| traceTrees                                                                    | dump trees after each compilation phase                                                                |
| traceTreeSimplification                                                       | trace tree simplification                                                                              |
| traceUseDefs                                                                  | trace use def info                                                                                     |
| traceValueNumbers                                                             | trace value number info                                                                                |
| traceVirtualGuardTailSplitter                                                 | trace virtual guard tail splitter                                                                      |
| verbose                                                                       | write compiled method names to vlog file or stdout in limitfile format                                 |
| verbose={<em>regex</em>}                                                      | list of verbose output to write to vlog or stdout                                                      |
| version                                                                       | display the jit build version                                                                          |
| vlog=<em>filename</em>                                                        | write verbose output to <em>filename</em>                                                              |

## Debugging Parameters

| Option                                     | Description                                                                                                                                                                                                        |
| ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| breakAfterCompile                          | raise trap when method compilation ends                                                                                                                                                                            |
| breakBeforeCompile                         | raise trap when method compilation begins                                                                                                                                                                          |
| breakOnBBStart                             | raise trap on BBStarts of method                                                                                                                                                                                   |
| breakOnCompile                             | deprecated; equivalent to breakBeforeCompile                                                                                                                                                                       |
| breakOnCreate={<em>regex</em>}             | raise trap when creating an item whose name (given by <em>enumerate</em>) matches <em>regex</em>                                                                                                                   |
| breakOnEntry                               | insert entry breakpoint instruction in generated code                                                                                                                                                              |
| breakOnLoad                                | break after the options have been processed                                                                                                                                                                        |
| breakOnOpts={<em>regex</em>}               | raise trap when performing opts with matching <em>regex</em>                                                                                                                                                       |
| compile                                    | Compile these methods immediately.  Primarily for use with Compiler.command                                                                                                                                        |
| debugBeforeCompile                         | invoke the debugger when method compilation begins                                                                                                                                                                 |
| debugOnCompile                             | deprecated; equivalent to debugBeforeCompile                                                                                                                                                                       |
| debugOnEntry                               | invoke the debugger at the entry of a method                                                                                                                                                                       |
| enumerateAddresses=                        | select kinds of addresses to be replaced by unique identifiers in trace file                                                                                                                                       |
| exclude=<em>xxx</em>                       | do not compile methods beginning with <em>xxx</em>                                                                                                                                                                 |
| limit=<em>xxx</em>                         | only compile methods beginning with <em>xxx</em>                                                                                                                                                                   |
| limitfile=<em>filename</em>                | filter method compilation as defined in <em>filename</em>.  Use <tt>limitfile=(<em>filename</em>,<em>firstLine</em>,<em>lastLine</em>)</tt> to limit lines considered from <em>firstLine</em> to <em>lastLine</em> |
| maskAddresses                              | remove addresses from trace file                                                                                                                                                                                   |
| noRecompile                                | do not recompile even when counts allow it                                                                                                                                                                         |
| stopOnFailure                              | stop compilation if exceed memory threshold                                                                                                                                                                        |
| tlhPrefetch                                | enable software prefetch on allocation for X86                                                                                                                                                                     |
| tossCode                                   | throw code and data away after compiling                                                                                                                                                                           |

## Other Parameters

| Option                                     | Description                                                                                                     |
| ------------------------------------------ | --------------------------------------------------------------------------------------------------------------- |
| softFailOnAssume                           | fail the compilation quietly and use the interpreter if an assume fails                                         |
| timing                                     | time individual phases and optimizations                                                                        |
| timingCummulative                          | time cummulative phases (ILgen,Optimizer,codegen)                                                               |
| traceMarkingOfHotFields                    | trace marking of Hot Fields                                                                                     |

