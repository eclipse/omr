/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "default_compiler.hpp"
#include "Jit.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>

typedef int32_t (IncOrDecFunction)(int32_t*);

int main(int argc, char const * const * const argv) {
    assert(argc == 2);

   bool initialized = initializeJit();
   if (!initialized) {
        fprintf(stderr, "FAIL: could not initialize JIT\n");
        exit(-1);
    }

    // parse the input Tril file
    FILE* inputFile = fopen(argv[1], "r");
    assert(inputFile != NULL);
    ASTNode* trees = parseFile(inputFile);
    fclose(inputFile);

    printf("parsed trees:\n");
    printTrees(stdout, trees, 0);

    // assume that the file contians a single method and compile it
    Tril::DefaultCompiler incordecCompiler(trees);

    int32_t result = incordecCompiler.compile();
    if (result != 0) {
       printf("Failed to compile\n");
       exit(-2);
    }

    auto incordec = incordecCompiler.getEntryPoint<IncOrDecFunction*>();

    int32_t value = 1;
    printf("%d -> %d\n", value, incordec(&value));
    value = 2;
    printf("%d -> %d\n", value, incordec(&value));
    value = -1;
    printf("%d -> %d\n", value, incordec(&value));
    value = -2;
    printf("%d -> %d\n", value, incordec(&value));

    shutdownJit();
    return 0;
}
