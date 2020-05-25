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

#include <assert.h>
#include <cstring>
#include <stdio.h>

typedef void (MandelbrotFunction) (int32_t, int32_t, int32_t*);

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
    Tril::DefaultCompiler mandelbrotCompiler(trees);

    int32_t rc = mandelbrotCompiler.compile();
    if (rc != 0) {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
    }

    auto mandelbrot = mandelbrotCompiler.getEntryPoint<MandelbrotFunction*>();

    const auto size = 80;                   // number of rows/columns in the output table
    const auto iterations = 1000;           // number of iterations to be performed
    int32_t table[size][size];              // the output table

    std::memset(table, 0, sizeof(table));

    mandelbrot(iterations, size, &table[0][0]);

    // iterate over each cell in the table and print a corresponding character
    for (auto y = 0; y < size; ++y) {
        for (auto x = 0; x < size; ++x) {

#if defined(FULL_COLOUR)
            // if the current cell is *outside* the Mandelbrot set, print a '#',
            // other wise print ' ' (blank space)
            auto c = table[y][x] < iterations ? '#' : ' ';

            // map the modulus of the cell's value to a terminal color
            int colors[] = {1, 1, 5, 4, 6, 2, 3, 3, 3, 3};
            auto color = colors[table[y][x] % 10];

            // print the selected character in the calculated color
            // using ANSI escape codes
            printf(" \e[0;3%dm%c\e[0m ", color, c);
#elif defined(SIMPLE_COLOUR)
            // if the current cell is *inside* the Mandelbrot set, print a '#',
            // other wise print ' ' (blank space)
            auto c = table[y][x] >= 1000 ? '#' : ' ';

            // map the cell's x-coordinate to a color
            int colors[] = {0, 1, 3, 2, 6, 4, 5, 5, 5};
            auto color = colors[x / 10];

            // print the selected character in the calculated color
            // using ANSI escape codes
            printf(" \e[0;3%dm%c\e[0m ", color , c);
#else
            // if the current cell is *inside* the Mandelbrot set, print a '#',
            // other wise print ' ' (blank space)
            auto c = table[y][x] >= 1000 ? '#' : ' ';

            // print the selected character
            printf(" %c ", c );
#endif

        }
        printf("\n");
    }

   shutdownJit();
   return 0;
}
