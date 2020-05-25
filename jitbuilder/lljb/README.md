<!--
Copyright (c) 2020, 2020 IBM Corp. and others

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

# LLJB

LLVM + JitBuilder = LLJB

# About

The LLJB project enables interoperability between LLVM and JitBuilder. LLJB
takes LLVM Intermediate Representation (IR) files and uses JitBuilder API to
construct equivalent OMR Compiler IL. The OMR Compiler then performs
optimizations and generates native code for every function in the module.

# Additional Requirements

You need the following to build lljb:

* clang 8.0
* llvm 8.0

### On macOS - using Homebrew

```
brew install llvm@8
```

### On Ubuntu
```
apt install clang-8 llvm-8 llvm-8-dev
```

## Enabling building LLJB

You must use the CMake build system to build LLJB. The following are
the options that must be enabled when configuring CMake build:

```
OMR_LLJB
OMR_JITBUILDER
OMR_COMPILER
```

## Tests

LLJB tests are located in `fvtest/lljbtest`.
