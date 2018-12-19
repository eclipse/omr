/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "Jitbuilder.h"


#define TOSTR(x) #x
#define LINETOSTR(x) TOSTR(x)

int main(int argc, char *argv[]) {
    puts("Step 1: initialize JIT");
    bool initialized = JBinitializeJit();
    if (!initialized) {
        fprintf(stderr, "FAIL: could not initialized JIT\n");
        exit(-1);
    }

    puts("Step 2: define type dictionary");
    TypeDictionary *types = TDInit();
    puts("Step 3: compile method builder");
    MethodBuilder *method = MBInit(types, (VirtualMachineState*) NULL, buildIL);

    MBDefineLine(method, LINETOSTR(__LINE__));
    MBDefineFile(method, __FILE__);
    MBDefineParameter(method, "value", types->Int32);
    MBDefineReturnType(method, types->Int32);

    void *entry = 0;
    int32_t rc = JBcompileMethodBuilder(method, &entry);
    if (rc != 0) {
        fprintf(stderr, "FAIL: compilation error %d\n", rc);
        exit(-2);
    }

    puts("Step 4: invoke compiled code and print results\n");
    typedef int32_t (SimpleMethodFunction)(int32_t);
    SimpleMethodFunction *increment = (SimpleMethodFunction *) entry;

    int32_t v;
    v = 0;
    printf("increment(%d) == %d\n", v, increment(v));

    v = 1;
    printf("increment(%d) == %d\n", v, increment(v));

    v = 10;
    printf("increment(%d) == %d\n", v, increment(v));

    v = 15;
    printf("increment(%d) == %d\n", v, increment(v));

    puts("Step 5: shutdown JIT");
    MBDestroy(method);
    TDDestroy(types);
    JBshutdownJit();
}

bool buildIL(MethodBuilder* mb) {
    puts("SimpleMethod::buildIL() running!");
    ILBuilder* ilBuilder = mb->super;
    IBReturn(ilBuilder,
              IBAdd(ilBuilder,
                     IBLoad(ilBuilder,"value"),
                     IBConstInt32(ilBuilder, 1))
    );
    return true;
}
