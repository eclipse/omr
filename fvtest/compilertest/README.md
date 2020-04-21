<!--
Copyright (c) 2016, 2020 IBM Corp. and others

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

Test Compiler
=============

This file contains the Eclipse OMR 'Test compiler', a framework for building
language independent compiler tests for the Testarossa compiler technology.

Building
--------
Before building the Test compiler you must make sure to configure the project
by following the 'Basic configuration and compile' step as outlined in the
top-level `<omr_root_folder>/README.md` document.

To build the Test compiler, typing `make` in this directory
(`<omr_root_folder>/fvtest/compilertest`) should suffice. A binary called
`testjit` will be produced in the top-level `omr` directory.

BUILD_CONFIG defaults to prod.

In the case where `guess-platform.sh` is incorrect, you can set the `PLATFORM`
environment variables to one of the supported ones, listed in
`<omr_root_folder>/fvtest/compilertest/build/platforms/host.mk`.


Running the Test Compiler
-------------------------

The Test compiler has been written using [Google Test][gtest], and as such,
obeys the Google Test command line arguments, that can be seen by running
`testjit --help`.

A very useful subset include:

* `--gtest_list_tests` Lists the tests.
* `--gtest_filter=` Filters what runs based on a simple regex.
* `--gtest_also_run_disabled_tests` Also run disabled test group.

Options can be passed to the Testarossa compiler technology using the
`TR_Options` environment variable.

    $ TR_Options='log=/tmp/testcomplog,tracefull' testjit


Contributing to Test Compiler
-----------------------------

The Test compiler uses IlInjectors to directly generate simple IL,
which is designed to target particular code generator evaluators.
In this way, the Test compiler can exercise compiler functionality
independent from a particular language runtime. An evaluator is
invoked for a particular IL opcode or set of IL opcodes,
so Test compiler test is also called OpCodes test.

A full list of OMR IL OpCodes can be found in `<omr_root_folder>/compiler/il/OMRILOpCodesEnum.hpp`.
Testarossa compiler supported evaluators are platform dependent. Supported evaluators
on each platform can be found in `<omr_root_folder>/compiler/<platform>/codegen/TreeEvaluatorTable.hpp`

Some evaluators in Testarossa compiler fail the tests and give unexpected behaviors.
They are put into disbaled groups in test source codes. To enable and run those disabled
tests, append `--gtest_also_run_disabled_tests` after `testjit` when running the test.

To contribute to Test compiler, you can continue adding supported evaluator tests,
or try to fix unexpected evaluator behaviors and put its disabled tests back into
its corresponding test group.

Each OpCode test need to be paired with an ILInjector. ILInjectors should
be put in `<omr_root_folder>/fvtest/compilertest/ilgen/`. Examples can be
found in *`BinaryOpIlInjector`*, *`SelectOpIlInjector`* and *`UnaryOpIlInjector`*.

Coding style in Test compiler is mostly using Whitesmiths format and Camel case.  
Some guidelines for naming convention:

* Commonly used variables should be defined in static const in capital letter.
* Macro `signatureChar` are defined in `<omr_root_folder>/fvtest/compilertest/tests/OpCodesTest.hpp`.
  If a new `signatureChar` is needed, it needs to follow the strucutre 
  '`<output_type> <signed_or_unsigned>SignatureChar<input_operand_type_prefix>_<output_operand_type_prefix>_testMethodType`' 
  (e.g. `typedef int8_t (signatureCharB_B_testMethodType)(int8_t);`)
* Opcode functions need to be defined as the same of its corresponding IL opcode (e.g. `_iAdd`). 
  Also, Opcode functions should be defined by `signatureChar` and need to begin with underscore '`_`'
  to distinguish from its corresponding IL opcode. 
* Opcodes tests are grouped into methods. Method name needs to reflect the arithmetics or logics
  of opcodes it contains (e.g. `compileIntegerArithmeticTestMethods()` and 
  `invokeIntegerArithmeticTests()`).


[gtest]: https://github.com/google/googletest/
