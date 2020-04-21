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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# DDR info generation

## Generating DDR debug information

### `ddrgen`
TODO

### `macro-tool`
TODO

### In-Source Annotations
C/C++ source annotations can be used to configure how DDR generates it's debug information.

Following are the different annotations provided by OMR.

`@ddr_namespace <format>`: Set the DDR namespace of all following C macros.

Possible values for `<format>`:
- `default`: Place macros in the default DDR namespace, named after the input source.
- `map_to_type=<type>`: Place macros in the DDR namespace `<type>`.

Setting the DDR namespace allows users to organize generated DDR info into logical categories. `@ddr_namespace` may be
called more than once. When DDR translates DDR macro info into Java sources, the Java class name is derived from the
DDR namespace.

`@ddr_options <option>`: Configure additional processing options.

Possible values for `<option>`:
- `valuesonly`: All *constant-like* macros (E.G. `#define x 1`) are added as a constant to the DDR namespace.
  *function-like* and empty macros are ignored. This is the default.
- `buildflagsonly`: All empty `#define`d and `#undef`ed symbols are added as constants to the DDR namespace. Macros are
  assigned a value of `1` if `#define`d, or `0` if `#undef`ed.
- `valuesandbuildflags`: This option combines the behavior of both `valuesonly` and `buildflagsonly`.

### Properties files

### Blacklisting Sources
## DDR & CMake

### New Terms
+ *DDR set* - the output of a single run of ddrgen (ie the blob and superset)

### Internals
For every target added to a ddr set a text file is generated which holds info which includes
+ source directory of the target
+ list of source files in the target
+ include path used when building the target
+ Pre processor defines which are set when building the target

For each ddr set a CMakeLists.txt file is generated which will process the target information text files, and do appropriate procesing to get the macro list needed for ddrgen.
In addition rules will be added to run cmake and run make on the generated output as part of the build
