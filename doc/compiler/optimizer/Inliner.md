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

## Overview

Inliner in OMR is both
* An *optimization* that could be run multiple times as a part an optimization strategy defined in [OMROptimizer.cpp](https://github.com/eclipse/omr/blob/master/compiler/optimizer/OMROptimizer.cpp)
* An *API* other optimizations (e.g. [GlobalValuePropagation](https://github.com/eclipse/omr/blob/master/compiler/optimizer/ValuePropagation.cpp), EscapeAnalysis) could use in order to inline calls in IL. 
* A host of smaller optimizations that are either *convenient* to run as the part of inlining or *beneficial* to the inlining process itself. These optimizations can
  * Reduce the number of temporaries used to pass arguments from a *caller* to a *callee*
  * Propagate *type* information along edges of a call graph. The type information can in turn be used in many different ways including **devirtualization** (resolving a polymorphic call to a particular implementation) or field chain folding 
		
## Higher-Level Structure

In order to serve as a standalone optimization, Inliner includes a class called `TR_TrivialInliner` which extends `TR::Optimization` and implements `perform` and `create` methods, so Optimizer knows how to instantiate and invoke Inliner. `TR_TrivialInliner` is a very lightweight class whose sole job is to create an instance of `TR_DumbInliner` and call its `performInlining`. You might want to think of `TR_DumbInliner` as a *"real"* Inliner and of `TR_TrivialInliner` as an adapter class that needs to conform to the contract defined by Optimizer infrastructure.    

This is a class hierarchy of the adapter class

* **TR::Optimization**
	* **TR_TrivialInliner**

This is a class hierarchy of the *real* Inliner 

* **TR_InlinerBase**
	* **TR_DumbInliner**
		* **TR_InlineCall**
	* **TR_MultipleTargetInliner** 
	
	
* **TR_InlinerBase** is a parent class containing common functionality used by all of its children (e.g. TR_DumbInliner, TR_InlineCall, etc)
* **TR_DumbInliner** is a simple greedy Inliner. Given a starting budget **initialSize**, **TR_DumbInliner** walks the IL trees and inlines calls in the order they appear in IL until it squandres its entire budget. A callee receives the budget adjusted by the sizes of every caller in a call chain from the topmost caller to that particular callee. 
* **TR_InlineCall** is an API other optimizations could use to inline a particular call in IL. **TR_InlineCall** uses very similar heuristics to **TR_DumbInliner**. The budget is typically smaller than **TR_DumbInliner** 

### **TR_CallSite**s & **TR_CallTarget**s

There are two more very important classes in Inliner:

* **TR_CallSite**
* **TR_CallTarget**
	
**TR_CallSite** includes all the information relevant to a particular call encountered in IL. For example, 
  * `callNode` -- an original callNode containing a call 
  * `callNodeTreeTop`, -- an TR::TreeTop containing the `callNode`
  * `initialCalleeMethod`, -- an initial callee method. 
  
are all properties of **TR_CallSite**
  
Note, `initialCalleeMethod` will not necessarily be the method Inliner chooses to inline. This is especially true for *polymorphic* calls where there is more than one choice due to multiple receivers and/or receiver types. Inliner represents this possible set of choices by **TR_CallTarget**s 
  
**TR_CallTarget** directly corresponds to one of the possible implementations of a particular **TR_CallSite**. As such, it typically includes the exact `TR_ResolvedMethod` and `TR_ResolvedMethodSymbol`. It also includes a `TR_VirtualGuardSelection` object that describes testing conditions Inliner needs to generate to make sure that it is valid ("validitity" here could mean different things depending on a type of **TR_CallSite**) to execute the inlined body for the implementation it has chosen to inline. 

### Inlining Guards 


Guards are a very *important* tool available to Inliner, so let's take a look at an example where Inliner needs to use a inline guard.

Given the following class hierarchy in Java:

```java

class A {
	int method1() { return 0; }
}

class B extends A {
	@override
	int method1() { return 1; }
	
	static int callMethod1 (A a) {
		return a.method1();
	}
	
}

```    	

Now let's assume that callMethod1 receives objects of type **A** in 90% of all calls to `callMethod1` and the rest could be objects of type **B** or of any other type that might extend **A**. If Inliner wants to inline a virtual call `a.method1();` it needs to consult a Profiler in this case to get a list of receiver types recorded (seen) at this particular callsite. The Profiler provides Inliner with this same statistics `{ type.A : 0.9, type.B : 0.1 }`. Based on the statistics Inliner creates two **TR_CallTargets** -- one for `A.method1` and another one for `B.method1` -- to represent the possible set of the choices. Each **TR_CallTarget** points to its own **TR_VirtualGuardSelection** (briefly mentioned above). **TR_VirtualGuardSelection** contains two very important fields: **_type** and **_kind** that Inliner uses to generate the testing conditions. In the example given, `_kind` would be set to **TR_ProfiledGuard** and `_type` would be equal to **TR_VftTest**. In other words, this means that Inliner needs to generate a *class* test before the inlined body should it decide to inline either `A.method1` or `B.method1`. The class test is one of the two profiled tests: **TR_VftTest** and **TR_MethodTest**, available in OMR. 
Now Inliner decides to inline `A.method1` (`A`'s implementation of `method1`). It generates the IL similar to the pseudo IL  below.

```cpp
BB_Start <block_1>

NULLCHK
	aloadi <vft> 
		aload <parm0>

ifacmpne --> <block_3>
	==> aloadi 
	aconst A.vft
BB_Start <block_4>
	istore <tmp1>
		iconst 0

BB_Start <block_2>
	ireturn <tmp1>

BB_Start <block_3>

istore <tmp1>
	icalli <A.a>
goto <block2>
```

Below is a graphical representation of the same process. 

`<block 1>` is transformed 
 

```
   +--------------+ 
   |              | 
   | BB_Start <1> | 
   |              | 
   |IL before call| 
   |              | 
   |    ...       | 
   |              | 
   |  method1()   | 
   |              | 
   |    ...       | 
   |              |
   |IL after call |
   +--------------+
```   
 
into the following 


```
      +---------------+
      |               |
      |BB_Start <1>   |
      |============== |
      |IL before call |
      |               |
      |    ...        |
      |               |
      | store tmp1    |
      | store tmp2    |
      |               |
      | ifacmpne <3>  +---+
      |    ==> vft    |   |
      |    aconst <A> |   |
      |               |   |
      +---------------+   |
             v            |  
      +---------------+   |  
      |               |   |  
      |BB_Start <2>   |   |  
      |============== |   |  
      |store tmp3     |   |  
      |   aload arg1  |   |  
      |store tmp4     |   |  
      |   aload arg2  |   |  
      |               |   |  
      |  method1 IL   |   |  
      +---------------+   |  
             v            |  
     +----------------+   |  
     |                |   |  
  +> | BB_Start <4>   |   |  
  |  | ============== |   |
  |  | aload tmp1     |   |
  |  | aload tmp2     |   |
  |  |                |   |
  |  | IL after call  |   |
  |  |                |   |
  |  +----------------+   |
  |                       |
  |         ...           |
  |                       |
  |     Rest of IL        |
  |                       |
  |         ...           |
  |                       |
  |  +----------------+   |
  |  |BB_Start <3>    | <-+
  |  |=============   |
  |  |calli method1   |
  |  |   aload <>ft>  |
  |  |   aload vthis> |
  |  |                |
  |  |store <result>  |
  |  |   ==> calli    |
  |  |                |
  +--+goto  <4>       |
     |                |
     +----------------+
```


* In `<block 1>` Inliner first needs to load the vft pointer (virtual function table) out of an object. This pointer is used in OMR to uniquely identify a particular class.  
* Next, `ifacmpne` compares the runtime type of an object against `A.vft` since this is the class containing the inlined implementation (`A.method1`)
	* If the test fails, the execution transfers to `<block 3>` and we make a virtual call. This virtual call is resolved by runtime depending on the current type of `a`. The result of the call is stored into `<tmp1>`. Think of `<tmp1>` as a temporary variable created by Inliner so it could be use it across block boundaries. Remember, OMR does *not* allow us to use commoned expressions across blocks. 
	* If the test succeeds, we execute the body of `A` which also stores `iconst 0` into `<tmp1>`. 
* Lastly, we return the value stored in `<tmp1>` which either comes from `<block2>` or `<block3>` 

Note, `<block 4>` is typically called a *merge* block due to the fact that the control flow merges back from either inlined callee's body or from the block containing virtual call (`<block 3>`)

## Main phases of Inlining

```
performInlining --> inlineCallTargets --> inlineCallTarget --> inlineCallTarget2 ------------ ... 
                            ^                                                       |
							|_________[until there's  budget left]__________________|

```


### inlineCallTargets


`inlineCallTargets` identifies a list of potential callees by walking the IL trees of a passed-in caller symbol and inlines some according to the set policy and budget. 

It performs these 3 very important duties:

* Adjusting the inliner budget which will be propagated down via `TR_CallStack` to the inlined callees
* Deciding whether a basic block containing a call is *cold* in which case the call is typically not inlined unless the current inlining policy `inlineMethodEvenForColdBlocks` says otherwise.
* Calling `analyzeCallSite` to proceed with the current call 

In turn, `analyzeCallSite` relies on the following methods to do its job.  

* **analyzeCallSite**
	* **TR_CallSite::create**
	* **getSymbolAndFindInlineTargets**
		* **TR_CallSite::findCallSiteTarget**
		* **applyPolicyToTargets**
	* inlineCallTarget

`analyzeCallSite` calls `TR_CallSite::create` to create a TR_CallSite. `TR_CallSite::create` is a static constructor method that analyzes the *type* of the call and returns a `TR_CallSite` instance of a corresponding type. [CallInfo.hpp](https://github.com/eclipse/omr/blob/master/compiler/optimizer/CallInfo.hpp) specifies the list of available `TR_CallSite` types (e.g. `TR_DirectCallSite`, `TR_IndirectCallSite`, etc). The reason behind this type proliferation is to decouple the logic of *finding* and *creating* (packaged in `findCallSiteTarget`) a call target from Inliner code and from different `TR_CallSite` types. In principle, if two types share common functionality they should extend the same parent type. 

Next, `analyzeCallSite` passes a freshly minted `TR_CallSite` to `getSymbolAndFindInlineTargets`. `getSymbolAndFindInlineTargets` calls ` callsite->findCallSiteTarget(callStack, this);` to let the logic specific to the type of this particular `TR_CallSite` to build a list of possible `TR_CallTarget`s. `getSymbolAndFindInlineTargets`also calls `applyPolicyToTargets` to filter out the list of calltargets depending on an inlining policy. This *policing* occurs both in `applyPolicyToTargets` and also in `getSymbolAndFindInlineTargets`. The policies using `TR_ResolvedMethod` are typically placed in `applyPolicyToTargets` whereas the policies using `TR_ResolvedMethodSymbol` usually go into `getSymbolAndFindInlineTargets`. This is more of a historical idiosyncrasy . It stems from the fact that **TR_MultipleTargetInliner** (not available just yet) uses original bytecodes (as opposed to IL used by `TR_DumbInliner`) to build a call graph. As a result, `TR_ResolvedMethodSymbol`s are not available at that particular stage and `applyPolicyToTargets` is used to apply the inlining policy. 

Lastly, if all is well, the calltarget is passed on to `inlineCallTarget`. `inlineCallTarget`'s name is a bit of a misnomer. What it actually does is to apply more heuristics (mostly IL-based) to make a final decision on whether the target will be inlined or not. If `inlineCallTarget` decides to inline the target it calls `inlineCallTarget2`
		
### inlineCallTarget2

`inlineCallTarget2` contains physical inlining mechanics. In other words, this is a method that does *actual* inlining of the target. *Inlining* is a complex process; there are a number of challenges involved (to be discussed next), so `inlineCallTarget2` makes use of numerous helper functions. These are probably the most important ones.    
 
* **tryToGenerateILForMethod**
* **inlineCallTargets**
* **TR_TransformInlinedFunction::transform**
* Stitching callee's IL into caller
* Merging CFGs

First, `inlineCallTarget2` calls `tryToInlineTrivialMethod`. `tryToInlineTrivialMethod` is a part of Inliner imbued with *some* knowledge of a target language. `tryToInlineTrivialMethod` is able to recognize certain *standard-library* methods of the target language and generate the IL for those avoiding the full inlining process of `inlineCallTarget2`. If `tryToInlineTrivialMethod` is successful, `inlineCallTarget2` returns immediately.

Next, `inlineCallTarget2` makes a call to `tryToGenerateILForMethod` to generate callee's IL. Typically, the call is successful and inlining proceeds. 	

Now that `inlineCallTarget2` got callee's IL it can *recursively* call `inlineCallTargets` to scout and inline possible calls in the callee. 

Then, `inlineCallTarget2` initializes `TR_ParameterToArgumentMapper`. `TR_ParameterToArgumentMapper` as its name suggests needs to map *actual* arguments to formal *parameters*. `TR_ParameterToArgumentMapper` *can* directly reuse some of caller's arguments. Currently, the logic in `TR_ParameterToArgumentMapper` will reuse an incoming argument if 
* it is a const, 
* is is *not* modified by callee,
* the argument is *not* used anywhere else, 
* the argument is a *this* object and it is used *only* twice:
	* to fetch a vft out of *this* object
	* to be passed as *this* argument to the call. 
	
Otherwise, `TR_ParameterToArgumentMapper` either creates a fresh temporary to hold the argument value or reuses one of the available temps (see [Temp Sharing](#temp-sharing)) 


`TR_TransformInlinedFunction` object is created and callee's IL transformed in the following ways: 
 * replaces the uses of callee's parameters with the temps or args assigned by `TR_ParameterToArgumentMapper` via `transformNode`
 * transforms callee's return node by 
  * creating a new temporary and storing the return value into the temp
  * replacing the result of the call (the use of the call node) in caller with the return value 
  * ignoring the return value if the result of the call is not used. 


After that, `inlineCallTarget2` embeds callee's IL into caller's. The IL includes all the stores required to move arguments into temporaries. Also, Inliner adds CFG callee's CFG (control flow graph) nodes to the caller's CFG.

Next, if `TR_VirtualGuardSelection` of the calltarget dictates that an inlining guard is required for this call, Inliner invokes `addGuardForVirtual`. `addGuardForVirtual` adds a virtual snippet block and generates a configuration described in [Inline Guards](#inlining-guards). `addGuardForVirtual` uses `createVirtualGuard` to generate IL for the guard test (e.g. `ifacmpne`, `ificmpne`, etc). In general, OMR Inliner currently supports only single-condition guards. However, certain guard types (**HCR Guards**) can be combined (merged) and stand for more complicated tests than expressed in IL.  

We already know that commoned expressions (e.g. nodes with reference counts greater than 1) *can not* span multiple extended basic blocks. A quick refresher, an **extended basic block** (or **superblock**) is a sequence of IL which has an exactly one entry point and *can* have multiple exits. The entry point needs to be the first treetop in the sequence. Unfortunately, Inliner often breaks this rule; it might need to generate an inlining guard or callee itself might introduce extra control flow (e.g. `ifs`, `loops`, etc). If the block containing calls needs to be split into multiple, Inliner needs to identify all the commoned expressions that will end up on the opposite side of the split and fix those. Consider a canonical example where Inliner needs to generate a virtual guard snippet. We saw, in [Inline Guards](#inlining-guards), that the block containing a call (e.g. `method1`) was split at the treetop containing the call node exactly. Now let's imagine that there was a load of some variable right before the call and the very same load node of that variable is used (referenced) again right after the call. The use of the load node after the call (in the merge block) is now invalid and needs to be fixed. 

This is precisely the job of `TR_HandleInjectedBasicBlock::findAndReplaceReferences`. `findAndReplaceReferences` calls `collectNodesWithMultipleReferences` to walk the beginning of the extended basic block containing the call up to the last basic block containg the call and collect nodes *whose reference count is greater than 1*. Every time it process such a node it also decrements its `_referencesToBeFound`. If `_referencesToBeFound` of the node comes down to exactly 0 it means that there are no outstanding uses of the node beyond the basic block containing the call and the node does *not* need to be fixed. At this point, the node is removed from `_multiplyReferencedNodes`

If `_multiplyReferencedNodes` does contain the nodes spanning multiple blocks, `findAndReplaceReferences` 
 * calls `createTemps` to store the nodes on `_multiplyReferencedNodes` into temporaries
 * calls `replaceNodesReferencedFromAbove` to replace the invalid uses in `nextBBEndInCaller` (replaceBlock1) block and `virtualCallSnippet` (replaceBlock2)
 
If `virtualCallSnippet` exists (e.g. not equal to `nullptr`) findAndReplaceReferences repeats the entire process on `virtualCallSnippet` to make sure that no commoned nodes created for `virtualCallSnippet` are used in the merge block. 

Then, Inliner marks callee's local variables and temproraries created for passing arguments for the caller to the callee as available for *reuse* (Please see [Temp Sharing](#temp-sharing) for more details.)

The very last thing Inliner needs to do is to update caller's flags depending on the inlined callee. `updateCallersFlags` takes care of this task.  

## Additional Topics

### Rematerialization 

@TODO
 

### Type Information Propagation 

@TODO

### Temp Sharing 

@TODO

 