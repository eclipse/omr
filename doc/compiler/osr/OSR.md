<!--
Copyright IBM Corp. and others 2023

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# Introduction

On Stack Replacement (OSR) refers to a process of transitioning method execution from compiled to interpreted code while the compiled method is still in progress.
In particular, it can handle compiled code that has been optimized.
During this process, the JIT communicates to the VM the correct values of all locals and the operands on the operand stack for
each interpreter stack frame as if the code had been interpreted before the transition.
It also informs the VM about the current instruction address.
The VM then uses all this information to reconstruct all required interpreter stack frames and resume execution
in the interpreter at the appropriate point.

# High Level Sequence of Events

OSR is typically triggered by some VM event that applies to a compiled method that is currently on the stack.
For example, such an event could be an attempt to set a breakpoint in a method on the stack.
In addition, if assumptions that the compiler made while compiling a method are invalidated, OSR might eventually be triggered for the method as well. In this case, the VM event is an attempt to do something that would violate an assumption (like redefine a method).


A VM event can be detected at certain points during execution of compiled code. These points are called **OSR yield points**. The JIT and the VM agree on the set of OSR yield points.

Usually, we would like the VM event to be discovered relatively soon after it took place. [Garbage Collection (GC) points](https://github.com/eclipse/omr/blob/81b79405da6c7c960e611a8b2b12fd5861543330/compiler/il/OMRNode.cpp#L2298) meet such criteria.
Therefore, depending on the event, OSR yield points usually include all or a subset of GC points.
The reason why we would like to limit the set of OSR yield points, if possible, is the cost of the bookkeeping associated with each such point as well as the restrictions that they impose on the optimizer.

The actual OSR happens at **OSR Induction points**. It can happen at the time of the yield or later, as described in the following sections.

Therefore, at a high level, the sequence of events can be described as follows:

       VM event => OSR yield => OSR induction => OSR transition


# Voluntary and Involuntary OSR

The difference between voluntary vs. involuntary OSR is whether it is induced by the JIT or the VM. In the case of involuntary OSR, the transition is induced by the VM by forcibly transferring control from the VM code to an OSR handler in the compiled code. Such possible transfer of control is modelled as exception control flow in the IL because it behaves similarly. Therefore, every OSR yield point is also an OSR induction point.

Under voluntary OSR, the JIT controls when to trigger an OSR transition by inserting OSR inducing code into the IL.

Each compiled method is compiled in either of the two modes: all possible OSR transitions from that method are voluntary or all OSR transitions are involuntary.
OMR provides infrastructure for involuntary OSR. Downstream projects can implement voluntary OSR.

# Inducible and Uniducible OSR Yield Points

During voluntary OSR, the JIT can induce OSR after practically every OSR yield point and those points are called **inducible OSR yield points**. However, there are some OSR yield points after which the JIT cannot induce OSR. Those are referred to as **uninducible OSR yield points**. A typical example of an uninducible OSR yield point is when there is [a thunk archetype](https://github.com/eclipse-openj9/openj9/doc/compiler/methodHandles/MethodHandles.md) present on the inline stack.


# Pre- and Post-Execution OSR

In the Pre-execution OSR mode, OSR happens before the side-effects of executing the OSR yield take place. For example, if a monitor enter was identified as an OSR yield point, the VM induces OSR before entering the monitor. Then, we reconstruct the state such that the VM's interpreter can begin execution of the monitor enter.

Under post-execution OSR, the transition occurs after the OSR yield point has been evaluated in the compiled code.

Post-execution OSR can only be voluntary. Pre-execution OSR can be voluntary or involuntary. Both post-execution and pre-execution OSR can exist in the same method.


# Information Needed by the VM During OSR

During OSR, the VM needs the following information from the JIT for each interpreter frame that needs to be restored:

  - values of all interpreter local variable and operand stack slots
  - slot sharing info (if necessary)

When a method is compiled, values of the interpreter locals and slots on the operand stack are stored in the symbols created by the JIT. Locals are stored into parameter and automatic symbols. Operands are stored into pending push temporaries. In the simplest case, there is a one-to-one correspondence between a JIT symbol and a slot and it's known at compile time which symbol needs to be stored into which slot. This information is specific to the method being compiled and to each method inlined into it. The number of symbols copied can be reduced based on the location of the OSR induction point, but it's not necessary for correctness. If such optimization is done, the information becomes specific to each method, as well as the location of the OSR induction point that started the transition.

Sometimes, a slot is shared by one or more JIT symbols. For example, it happens when one slot holds values of different types at different points in the method's bytecode. Then, the JIT would create two symbols of different types but with the same slot number. In such situations, we need to create slot sharing info for each interpreter frame that needs to be restored and has slot sharing. The slot sharing info specifies for each shared slot which symbol corresponds to it at that point. This information will depend not only on the method, but also on the location of the OSR induction point that triggered the transition.

As an example, let's consider compiled method A that calls method B.

If OSR is induced inside method A, values of all A's locals and operands at the OSR induction point need to be conveyed to the VM, along with the slot sharing info at that point.

If OSR happens within B and B is not inlined into A, two scenarios are possible:

  - the OSR will be handled by B (B will be interpreted) and the control will be returned into the compiled method A. This can happen if some assumption is invalidated in B and only B needs to be interpreted. In this case, the call to B does not have to be treated as an OSR induction point in A.

  - We need to reconstruct the whole stack, and after OSR handling mechanism reconstructs B's frame, the control returns to A, which in turn proceeds reconstructing its own frame and so on. In this case, the call to B is treated as an OSR induction point in A.


If OSR happens within B and it's inined into A, we need to know:

  - values of all local and stack operand slots for A
  - values of all local and stack operand slots for B
  - slot sharing info for B at the OSR induction point in B
  - slot sharing info for A at the point where A calls B. If there is another instance of B inlined into A, we would need yet another set of slot sharing info for A.

Therefore, we can say that the full set of all the necessary info is specific to the offset of the OSR induction point within the compiled body of the method being compiled. This point will determine (1) the frames that need to be restored, (2) how the slots need to be populated for each of those frames.


# OSR Points

OSR points are points in the program where the information above is collected, therefore, allowing the transition to happen in those points. OSR points are originally created just before or after OSR yield points during GenIL. While the OSR yield points can be optimized away later (such as asynccheck, monenter, etc.) OSR transition can still happen at those points.

Notice that OSR points are not explicitly present in the IL or the generated code. They are just bookkeeping points that are used for making decisions during compile time as well as generating metadata to be used at runtime. Some OSR points are used purely for analysis and some will eventually become OSR induction points (in the case of voluntary OSR).


# Liveness Analysis

In order to create slot sharing info, liveness analysis is performed. Liveness info is collected at OSR points. In the example above, the OSR analysis points will be:

   - the OSR induction point in B
   - all points just before B is inlined into A.

Notice that, if B is inlined into A, B's parameters should be excluded from the live pending push temporaries since they are already popped from the stack at the time the OSR within B takes place.

In addition to creating slot sharing info, liveness analysis can also determine if some symbol is not used on a certain path and therefore does not need to be copied into its slot.

**Q**: does the optimization above only apply to the symbols that share a slot? Can we determine that some symbol is not needed on some path even if it does not share a slot?


# Data Structures and Helpers

There are two types of data structures: ones that only exist at compile time and metadata that is used at run time.

## Compile time

```TR_OSRMethodData```: slot sharing info for each OSR point within one inlined site index. OSR point is identified by its bytecode index.

```TR_OSRPoint```: associates ```TR_OSRMethodData``` with a bytecode index.

**Q**: seems like a circular dependence between TR_OSRPoint and TR_OSRMethodData: the latter describes mutiple points but then is associated with one point?

Array of ```TR_OSRMethodData```: has an entry for each inlined site index in the method being compiled. Eventually, it is used to populate ```Instr2SharedSlotMetaData```.

## Run time

```OSRBuffer```: the buffer that is populated by compiled code with locals and operands for all active frames. It is used by the VM to reconstruct the frames. ```OSRBuffer``` is created by the VM. Since the VM is aware of the number of auto slots, parameter slots,
and the maximum number of pending push temporary slots, it can always ensure the buffer is large enough to hold all autos,
parameters and pending push temporaries for all interpreter frames the VM needs to recreate at a particular OSR point.

```OSRScratchBuffer```: the buffer is populated by compiled code with symbols that share slots. It is used by ```prepareForOSR``` together with ```Instr2SharedSlotMetaData``` to copy the right symbols into ```OSRBuffer```.

```Instr2SharedSlotMetaData```: each entry contains an instruction offset of an OSR induction point within the compiled method. It is followed by an array of tuples. Each tuple contains inined site index and indicates from which offset in ```OSRScratchBuffer``` to which offset in ```OSRBuffer``` the symbol needs to be copied by the corresponding ```prepareForOsr```. Therefore, given an offset of an OSR induction point, this metadata describes which JIT symbols need to be copied into ```OSRBuffer``` for each frame.

```prepareForOsr```: a helper call that is inserted in the IL with the purpose of keeping all necessary symbol live at a transition point. It also "logically" copies JIT symbols into ```OSRBuffer```. Although, during trees lowering, symbols are explicitly copied in the IL, except for the ones that share slots. Then, the helper is only used for copying symbols that require slot sharing info. ```prepareForOsr``` takes its inlined site index as a parameter.


# IL Generation

During IL generation, we examine each bytecode to see if it is an OSR yield point. If the current bytecode is an OSR yield point,
we first store all operands that are currently on the stack into pending push temporaries. An **OSR catch block** for the current inlined site index is created if it does not exist yet. An exception edge is then added between the current block and the OSR catch block.

Each OSR catch block has a single successor that is called an **OSR code block**. A ```prepareForOSR``` call node is added into that OSR code block. The node takes loads of all autos, parameters, and pending push temporaries used in the current method (but not methods inlined into it) as its arguments so that they will remain live at OSR points. It also takes its inlined site index as an argument.
Creating the exception blocks and edges prevents splitting of the block where the inspected bytecode resides,
hence allowing local optimizations to work on the same block with the additional restrictions imposed by these OSR points as they can now cause OSR exceptions.

In our example, for an OSR yield point in B that is inlined into A, we create two OSR catch blocks and their corresponding OSR code blocks:

1) one OSR catch block and a following OSR code block which stores all A's autos, parameters, and pending push temporaries and then returns control back to the VM;

2) one OSR catch block and a following OSR code block which stores all B's autos, parameters, and pending push temporaries and then goes to the OSR code block for A.


The reason for having both OSR catch block (intially empty) and the following OSR code block is that when we want to call A's ```prepareForOSR``` after we called B's we don't want to have a regular edge to A's catch block in addition to the exception edge since having both regular and exception edge into the same block is not supported in the IL.


During lowering, in an OSR code block, we prepend a sequence of stores of autos, parameters, and pending push temporaries that don't share any slots directly into ```OSRBuffer```. The ones that are shared are stored into ```OSRScratchBuffer```. The call to ```prepareForOSR``` only remains if slot sharing is present at least on one path leading to it.


Here is a CFG example when method B is inlined into method A twice:

```
                    --------------------------
                    |       entry            |---> OSR catch block 1
                    --------------------------
                             |
                    --------------------------
                    |   monenter in B        |---> OSR catch block 2
                    --------------------------
                             |
                    --------------------------
                    |   monenter in A        |---> OSR catch block 1
                    --------------------------
                             |
                    --------------------------
                    |  monenter in another B |---> OSR catch block 3
                    --------------------------
                             |
                    --------------------------
                    | another monenter in A  |---> OSR catch block 1
                    --------------------------
                             |
                    --------------------------    ---------------------    ---------------------
                    |       exit             |    | OSR catch block 2 |    | OSR catch block 1 |
                    --------------------------    ---------------------    ---------------------
                                                          |                        |
                                                  ---------------------    ---------------------
                                                  | OSR code block    |___>| OSR code block    |___>  call to VM
                                                  |prepareForOSR      |    |prepareForOSR      |
                                                  |   iconst 0        |    |   iconst -1       |
                                                  ---------------------    ---------------------
                                                                                   ^
                                                  ---------------------            |
                                                  | OSR catch block 3 |            |
                                                  ---------------------            |
                                                          |                        |
                                                  ---------------------            |
                                                  | OSR code block    |____________|
                                                  |prepareForOSR      |
                                                  |   iconst 1        |
                                                  ---------------------

```


# Execution

In the case of involuntary OSR, any OSR yield point can trigger an OSR.
If OSR is requested, it is treated as an exception and control flow is transferred to the corresponding OSR catch block.
The corresponding OSR code block copies all the necessary parameters, autos, and pending push temporaries into ```OSRBuffer```. Symbols that share slots are copied into ```OSRScratchBuffer```.
If shared slots are present, ```prepareForOSR``` decides which symbols have to be copied to which slot based on ```Instr2SharedSlotMetaData```, the inlined site index passed to it, and the address of the OSR induction point that is known to the VM at the point of exception.

At the end of each OSR code block, we either return control back to the VM by calling the runtime OSR helper if the current method is not inlined, or we go to the OSR code block of the caller of the inlined method. This way, ```prepareForOSR``` is called for each frame that needs to be restored and finds all the necessery info inside ```Instr2SharedSlotMetaData```.

Generally, execution is transitioned from optimized code to interpreted code once we are in OSR mode.
However, there is an exception to this rule. If both the VM and the JIT agree that we can resume executing optimized code after reconstructing interpreter stack frames,
we will not transition to the interpreter. A possible debugging scenario when this can happen is when a user pauses application execution, inspects all stack variables,
and then continues execution without any other debugging actions.

**TODO:** document this scenario in more detail


# Supporting Classes and Methods

```bool OMR::Compilation::isPotentialOSRPoint()``` : returns true iff given tree top is an OSR yield point

```bool OMR::Compilation::isPotentialOSRPointWithSupport()``` : returns true iff given tree top is an inducible OSR yield point

```OSRData```: contains compile time and runtime OSR data

```osrLiveRangeAnalysis```: calculates autos and pending push temporaries liveness info at the OSR points. If pending push temporaries liveness was calculated during ILGen it is reused by this analysis.

```OSRDefAnalysis```: Used by ```osrLiveRangeAnalysis```.