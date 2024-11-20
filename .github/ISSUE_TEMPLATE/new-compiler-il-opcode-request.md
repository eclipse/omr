---
name: New Compiler IL Opcode Request
about: Complete this template to propose a new compiler IL opcode.

---

# New Compiler IL Opcode Request

Complete the following sections to propose a new IL opcode.  Your proposal will be
discussed and reviewed with your participation during a [Compiler Architecture Meeting](https://github.com/eclipse/omr/issues/2316).
Please provide as much detail as possible to assist with the evaluation.

The title of your issue should follow the format:
```
New IL Opcode Request: <name of IL opcode>
```

In the template below, you may delete the text that is provided in this template as
you replace it with the information requested.

---

## IL Opcode Name

*Provide the name of a single IL opcode, or list each opcode in a family of IL opcodes.
An opcode family is considered to be one where each opcode has identical structure and
operational semantics and differs only by its data type or the data types of its children.*

## Motivation

*Describe the use case for this IL opcode.  Explain why a new IL opcode is required.*

*Please describe alternatives to introducing a new IL opcode and why a new IL opcode is 
the best solution.*

## Homogeneity

*Does this IL opcode have the same basic structure and operation as other IL opcodes, or
does it behave differently in any significant ways?  Please elaborate.*

## Structure

#### Data Type

* *OMR data type of the IL opcode*

#### Children

* *Number of children of this IL opcode*

*For each child:*
* *Describe the purpose, semantics, assumptions, and any requirements of the child*
* *OMR data type of the child*

#### Symbol Reference

* *Do Nodes with this IL opcode require a SymbolReference?*

* *If a new kind of SymbolReference is required, describe its purpose and semantics.*

* *Describe the aliasing semantics in the context of this IL opcode.*

#### IL Opcode Properties

* *Provide the set of IL opcode properties applicable to this IL opcode.*

* *Describe the semantics of any new IL opcode properties required for this IL opcode.*

#### Node flags

* *Describe the Node flags that will be set on Nodes with this IL opcode.*

#### Control Flow

* *If the new IL opcode introduces control flow explain the semantics.*
* *Do Nodes with this IL opcode end a basic block?*

## Operation

*Provide a detailed description of the operation of this IL opcode.  Use pseudo-code if necessary.*

#### Result Value

*How many values does the evaluation of this IL opcode produce?  A value is a result artifact
that can be consumed by another Node.  If there is more than one, please explain the purpose
of each and which children produce the results.*

#### Side Effects

*Describe any side effects the evaluation of this IL opcode may have.  For example: memory
updates, hardware/software exceptions, callbacks into the VM, allocation of memory, etc.*
 
#### Anchoring

*Describe the anchoring semantics of Nodes with this IL opcode.  For example, is it a 
tree top, does it behave like a call, is it free floating, etc.?*

## Scope

* *How will this IL opcode be handled by the optimizer?  Is there a reasonable enough chance of
optimization occurring to warrant introducing a new IL opcode rather than using an intrinsic?*
* *Will this IL opcode be useful on all code generator backends in Eclipse OMR?  If not, please
explain.*
* *Does this IL opcode map to a machine instruction or require specialized hardware to support?*

## Implementation Dependencies

*Describe any implementation dependencies this IL opcode incurs.  For example, does it require:*

* *Special support to be implemented in one or more optimizations before it can be used?*
* *All backends to implement it simultaneously?*
* *Special backend support (e.g., an out-of-line path to handle an exceptional case)?*
* *Implementation in all downstream projects consuming OMR?*
* *Codegen Node lowering, and if so, to what trees?*

## Performance

*Describe the performance benefits, if any, of this new IL opcode.*

## Testing Considerations

*Describe how this proposed IL opcode will be tested in a project-independent manner that can
be automated in Eclipse OMR CI builds.  If Tril unit tests cannot be provided, please explain
and propose an alternative.*

## IL Validation

*Will any changes to the IL Validator be required to validate trees using this IL opcode and
its properties or characteristics?  If so, please elaborate.*
