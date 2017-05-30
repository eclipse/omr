## Overview

**TrivialDeadBlockRemover** is a *fast* and *small* optimization for folding **trivial** branches in IL and removing resulting **unreachable** blocks.

A branch is considered to be **trivial** if 
* the branch operands are equal AND 
 * branch operands' data type  is *not* a floating point expression AND 
 * the branch comparison operator is either `==`, `!=`, `<=`, `>=`
* the branch operands are integer constants (either `signed` or `unsigned`)

A block is considered to be **unreachable**, if there is no path to the block starting from the entry block (`<block_0>`)

**TrivialDeadBlockRemover** was introduced to encourage optimizer developers to write optimizations that focus on *one and only one particular* task and do it well (also known as **Separation of Concerns** principle). Namely, if a new optimization needs to fold an existing branch and remove resulting unreachable blocks, it should convert the existing branch into a trivial one and schedule a pass of **TrivialDeadBlockRemover**. Removing blocks especially while iterating over a Control Flow Graph structure is a complicated and error-prone process.  

### @TODO

foldIf
* evaluateTakeBranch
* changeConditionalToUnconditional
 * removeOrconvertIfToGoto
 * removeEdge
* _gotoSimplifier

