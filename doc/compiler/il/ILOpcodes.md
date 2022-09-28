<!--
Copyright (c) 2022, 2022 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Notes on IL Opcodes

## PassThrough

`PassThrough` is a dummy node that represents its single child. It has no type. It is assumed
its child has a type.

`PassThrough` was originally conceived as a placeholder in a `NULLCHK` tree.  The `NULLCHK` would
consult its first grandchild (skipping over its first child) as part of its analysis, but if the
first child had been removed for some reason, the shape of the trees needs to be kept as the `NULLCHK`
evaluation code expects to evaluate its grandchild. Hence a `PassThrough` was introduced.

`PassThrough` has since been used in similar situations to hold the place of a removed node (e.g., a
`PassThrough` that anchors a `glRegDeps`). `PassThrough` with a register is needed as it is part of
the syntax for [GlRegDeps](GlRegDeps.md).

In the below example, if `NULLCHK` is eliminated, `NULLCHK` is replaced with `treetop`.

```
NULLCHK
  PassThrough
    aload value
===>
treetop
  PassThrough
    aload value
```

In the above example, `treetop` takes the value of the child and ignores it. `PassThrough`
takes the value of the child and produces the same value. The `aload` in the above tree may or
may not result in codegen emitting a move to register instruction.

For example, a tree like this could add `1` to the result of `aload`. It could turn out later that
the result of the addition is no longer needed, and then the result of the `aload` might also not be
needed. Since "explicitly ignore this value" and "copy this value with no modifications" are never
meaningful on their own, the only significance is what the `aload` loads here (rather than earlier
or later).
