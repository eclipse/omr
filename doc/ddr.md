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

# DDR: The Direct Dump Reader

## Building DDR

On Linux, DDR depends on libdwarf and libelf.  For more instructions on
obtaining these packages, see INSTALL.

OMR must be configured to enable the DDR component. As of writing, this is not
the default. DDR will be built after the main files are built.

```sh
bash configure --enable-debug --enable-DDR CPPFLAGS=-I/usr/include/libdwarf
make
```

## Running the DDR tests and samples

How to run new ddrgen on the test samples:

1. Run the macros script from the top directory:
   ```sh
   bash ddr/tools/getmacros ddr/test
   ```
2. Run ddrgen with the files to scan and macro list as parameters
   ```sh
   ./ddrgen ddrgentest --macrolist ddr/test/macroList --blob blob.dat --superset superset.out
   ```
3. Print the generated blob
   ```sh
   ./blob_reader blob.dat > blob.dat.strings
   ```
4. Compare the output to the expected output. For linux x86 (Note that ubuntu has slightly different results):
   ```sh
   diff superset.out ddr/test/expected_output/xa64/superset.out
   diff blob.dat.strings ddr/test/expected_output/xa64/blob.dat.strings
   ```

## Using DDR for OMR
Make sure that OMR was configured to output debug information (Enabled by
default, or pass --enable-debug as above)

```sh
find -name "*.debuginfo" >filelist
make -f ddr_artifacts.mk TOP_SRCDIR=. DBG_FILE_LIST=filelist
```

## Using DDR for other projects
The project must be compiled with debug symbols.  You must populate a file with
a list of all files with debug information.  If you are building a single
executable, then that is all you have to list in that file.  If you build any
other artifacts, such as libraries, you will need to add them as well.

You need to call a special makefile, which will run ddrgen on your project.

```sh
make -f omr/ddr_artifacts.mk TOP_SRCDIR=. DBG_FILE_LIST=filelist
```
