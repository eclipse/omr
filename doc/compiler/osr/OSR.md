<!--
Copyright (c) 2018, 2018 IBM Corp. and others

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


# OSR transition

OSR transition is the process of transfering the execution from jitted code to the interpreter. OSR transition involves leaving the jitted code, reconstructing the runtime VM state and resuming the execution where the jitted code left off in the interpreter.

# OSR point
An OSR point is a place in the jitted code where OSR transition is supported.

At an OSR point, the VM state can be correctly reconstructed and is consistent with the VM state needed by the interpreter to resume the execution at a given bytecode index.

To achieve that, bookkeeping is required at all possible OSR points during ILGen. Due to the cost and complexity of bookkeeping, OSR transition is limited to some well-defined points, which are calls, asyncchecks and monitor enters, etc.

Being part of the program, OSR points like calls, asyncchecks and monitor enters can be removed or optimized and no longer be identified as OSR points. However, OSR transition may still be supported at that position.

# potentialOSRPointHelper
potentialOSRPointHelper is a call that serves as a marker of places where OSR transition is supported. The target bytecode index to be executed in the interpreter can be obtained by adding an offset (an information stored in the call node, the size of OSR point's bytecode in post-execution OSR) to the bytecode index of the helper call node.

The helper is not a must for OSR, but it can help locating the OSR points that are available at ILGen but disappear due to later transformations. It works as a bookkeeping tool, to track places that are OSR possible.

# osrFearPointHelper
This helper marks a place that has been optimized with runtime assumptions and requires protection of OSR mechnism. This means an OSR point is required between the invalidation point and the fear point.

Similar to potentialOSRPointHelper, this helper is not a must for optimizations requiring OSR mechnism. It works as a tool to help locating the actual fear (being part of the program that may be removed or optimized to something else).
