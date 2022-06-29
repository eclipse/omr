/*******************************************************************************
 * Copyright (c) 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "JitBuilder.h"

#define max(a, b) ((a)>(b)?(a):(b))
#define TOSTR(x) #x
#define LINETOSTR(x) TOSTR(x)

typedef void (MandelbrotFunctionType)(int32_t, uint8_t* buffer, double* cr0);

struct IlType* pDouble;
struct IlType* pInt8;

bool buildIL(struct IlBuilder* ib)
{
    // N.B. Currently, all memory allocated in this method is being leaked
    IB_Store(ib, "cr", IB_CreateLocalArray(ib, 8, ib->Double));
    IB_Store(ib, "ci", IB_CreateLocalArray(ib, 8, ib->Double));

    IB_Store(ib, "width", IB_Load(ib, "N"));
    IB_Store(ib, "height", IB_Load(ib, "N"));
    IB_Store(ib, "max_x", IB_Div(ib,
            IB_Add(ib,
                    IB_Load(ib, "width"), IB_ConstInt32(ib, 7)), IB_ConstInt32(ib, 8)));

    IB_Store(ib, "max_iterations", IB_ConstInt32(ib, 50));
    IB_Store(ib, "limit", IB_ConstDouble(ib, 2.0));
    IB_Store(ib, "limit_sq", IB_Mul(ib, IB_Load(ib, "limit"), IB_Load(ib, "limit")));

    struct IlBuilder* x1Loop = NULL;
    IB_ForLoopUp(ib, "x", &x1Loop,
            IB_ConstInt32(ib, 0), IB_Load(ib, "max_x"), IB_ConstInt32(ib, 1));

    struct IlBuilder* k1Loop = NULL;
    IB_ForLoopUp(x1Loop, "k", &k1Loop,
            IB_ConstInt32(x1Loop, 0),
            IB_ConstInt32(x1Loop, 8),
            IB_ConstInt32(x1Loop, 1));

    IB_Store(k1Loop, "xk",
            IB_Add(k1Loop, IB_Mul(k1Loop, IB_ConstInt32(k1Loop, 8),
                    IB_Load(k1Loop, "x")), IB_Load(k1Loop, "k")));

    IB_StoreAt(k1Loop, IB_IndexAt(k1Loop, pDouble,
            IB_Load(k1Loop, "cr0"), IB_Load(k1Loop, "xk")),
            IB_Sub(k1Loop, IB_Div(k1Loop, IB_Mul(k1Loop, IB_ConstDouble(k1Loop, 2.0),
                    IB_ConvertTo(k1Loop, k1Loop->Double, IB_Load(k1Loop, "xk"))),
                    IB_ConvertTo(k1Loop, k1Loop->Double, IB_Load(k1Loop, "width"))),
                    IB_ConstDouble(k1Loop, 1.5)));

    struct IlBuilder* y2Loop = NULL;
    IB_ForLoopUp(ib, "y", &y2Loop, IB_ConstInt32(ib, 0), IB_Load(ib, "height"), IB_ConstInt32(ib, 1));

    IB_Store(y2Loop, "line", IB_IndexAt(y2Loop, pInt8, IB_Load(y2Loop, "buffer"),
            IB_Mul(y2Loop, IB_Load(y2Loop, "y"), IB_Load(y2Loop, "max_x"))));

    IB_Store(y2Loop, "ci0", IB_Sub(y2Loop,
            IB_Div(y2Loop,
                    IB_Mul(y2Loop,
                            IB_ConstDouble(y2Loop, 2.0),
                            IB_ConvertTo(y2Loop, y2Loop->Double, IB_Load(y2Loop, "y"))),
                    IB_ConvertTo(y2Loop, y2Loop->Double, IB_Load(y2Loop, "height"))),
            IB_ConstDouble(y2Loop, 1.0)));

    struct IlBuilder* x2Loop = NULL;
    IB_ForLoopUp(y2Loop, "x", &x2Loop, IB_ConstInt32(y2Loop, 0), IB_Load(y2Loop, "max_x"), IB_ConstInt32(y2Loop, 1));

    IB_Store(x2Loop, "cr0_x", IB_IndexAt(x2Loop, pDouble, IB_Load(x2Loop, "cr0"),
            IB_Mul(x2Loop, IB_ConstInt32(x2Loop, 8), IB_Load(x2Loop, "x"))));

    struct IlBuilder* k2Loop = NULL;
    IB_ForLoopUp(x2Loop, "k", &k2Loop, IB_ConstInt32(x2Loop, 0), IB_ConstInt32(x2Loop, 8), IB_ConstInt32(x2Loop, 1));

    IB_StoreAt(k2Loop, IB_IndexAt(k2Loop, pDouble, IB_Load(k2Loop, "cr"), IB_Load(k2Loop, "k")),
            IB_LoadAt(k2Loop, pDouble, IB_IndexAt(k2Loop, pDouble, IB_Load(k2Loop, "cr0_x"), IB_Load(k2Loop, "k"))));

    IB_StoreAt(k2Loop, IB_IndexAt(k2Loop, pDouble, IB_Load(k2Loop, "ci"), IB_Load(k2Loop, "k")),
            IB_Load(k2Loop, "ci0"));

    IB_Store(x2Loop, "bits", IB_ConstInt32(x2Loop, 0xff));

    struct IlBuilder* i2Loop = NULL, * i2Break = NULL;
    IB_ForLoopWithBreak(x2Loop, true, "i", &i2Loop, &i2Break, IB_ConstInt32(x2Loop, 0),
            IB_Load(x2Loop, "max_iterations"), IB_ConstInt32(x2Loop, 1));

    IB_IfCmpEqualZero(i2Loop, i2Break, IB_Load(i2Loop, "bits"));

    IB_Store(i2Loop, "bit_k", IB_ConstInt32(i2Loop, 0x80));

    struct IlBuilder* k3Loop = NULL;
    IB_ForLoopUp(i2Loop, "k", &k3Loop, IB_ConstInt32(i2Loop, 0), IB_ConstInt32(i2Loop, 8), IB_ConstInt32(i2Loop, 1));

    struct IlBuilder* bit_k_set = NULL;
    IB_IfThen(k3Loop, &bit_k_set,
            IB_And(k3Loop, IB_Load(k3Loop, "bits"),
                    IB_Load(k3Loop, "bit_k")));

    IB_Store(bit_k_set, "cr_k", IB_LoadAt(bit_k_set, pDouble,
            IB_IndexAt(bit_k_set, pDouble, IB_Load(bit_k_set, "cr"),
                    IB_Load(bit_k_set, "k"))));

    IB_Store(bit_k_set, "ci_k", IB_LoadAt(bit_k_set, pDouble,
            IB_IndexAt(bit_k_set, pDouble, IB_Load(bit_k_set, "ci"),
                    IB_Load(bit_k_set, "k"))));

    IB_Store(bit_k_set, "cr_k_sq", IB_Mul(bit_k_set, IB_Load(bit_k_set, "cr_k"), IB_Load(bit_k_set, "cr_k")));

    IB_Store(bit_k_set, "ci_k_sq", IB_Mul(bit_k_set, IB_Load(bit_k_set, "ci_k"), IB_Load(bit_k_set, "ci_k")));

    IB_StoreAt(bit_k_set, IB_IndexAt(bit_k_set, pDouble, IB_Load(bit_k_set, "cr"), IB_Load(bit_k_set, "k")),
            IB_Add(bit_k_set, IB_Sub(bit_k_set, IB_Load(bit_k_set, "cr_k_sq"), IB_Load(bit_k_set, "ci_k_sq")),
                    IB_LoadAt(bit_k_set, pDouble,
                            IB_IndexAt(bit_k_set, pDouble, IB_Load(bit_k_set, "cr0_x"), IB_Load(bit_k_set, "k")))));

    IB_StoreAt(bit_k_set, IB_IndexAt(bit_k_set, pDouble, IB_Load(bit_k_set, "ci"), IB_Load(bit_k_set, "k")),
            IB_Add(bit_k_set,
                    IB_Mul(bit_k_set, IB_Mul(bit_k_set, IB_ConstDouble(bit_k_set, 2.0), IB_Load(bit_k_set, "cr_k")),
                            IB_Load(bit_k_set, "ci_k")), IB_Load(bit_k_set, "ci0")));

    struct IlBuilder* clear_bit_k = NULL;
    IB_IfThen(bit_k_set, &clear_bit_k,
            IB_GreaterThan(bit_k_set, IB_Add(bit_k_set, IB_Load(bit_k_set, "cr_k_sq"), IB_Load(bit_k_set, "ci_k_sq")),
                    IB_Load(bit_k_set, "limit_sq")));

    IB_Store(clear_bit_k, "bits", IB_Xor(clear_bit_k, IB_Load(clear_bit_k, "bits"), IB_Load(clear_bit_k, "bit_k")));

    IB_Store(k3Loop, "bit_k", IB_UnsignedShiftR(k3Loop, IB_Load(k3Loop, "bit_k"), IB_ConstInt32(k3Loop, 1)));

    IB_StoreAt(x2Loop, IB_IndexAt(x2Loop, pInt8, IB_Load(x2Loop, "line"), IB_Load(x2Loop, "x")),
            IB_ConvertTo(x2Loop, x2Loop->Int8, IB_Load(x2Loop, "bits")));

    IB_Return(ib);

    return true;
}

int main(int argc, char* argv[])
{
    if (argc<2) {
        fprintf(stderr, "Usage: mandrelbrot <N> [output file name]\n");
        exit(-1);
    }

    const int N = max(0, (argc>1) ? atoi(argv[1]) : 0);

    printf("Step 1: initialize JIT\n");
    bool initialized = initializeJit();
    if (!initialized) {
        fprintf(stderr, "FAIL: could not initialize JIT\n");
        exit(-1);
    }

    printf("Step 2: define relevant types\n");
    struct TypeDictionary* types = NewTypeDictionary();

    printf("Step 3: compile method builder\n");
    struct MethodBuilder* mandelbrotMethod = NewMethodBuilder(types, NULL);

    MB_DefineLineString(mandelbrotMethod, LINETOSTR(__LINE__));
    MB_DefineFile(mandelbrotMethod, __FILE__);

    pDouble = TD_PointerTo(types, types->Double);
    pInt8 = TD_PointerTo(types, types->Int8);

    MB_DefineName(mandelbrotMethod, "mandelbrot");
    MB_DefineParameter(mandelbrotMethod, "N", types->Int32);
    MB_DefineArrayParameter(mandelbrotMethod, "buffer", pInt8);
    MB_DefineArrayParameter(mandelbrotMethod, "cr0", pDouble);
    MB_DefineReturnType(mandelbrotMethod, types->NoType);

    MB_DefineLocal(mandelbrotMethod, "line", pInt8);
    MB_DefineLocal(mandelbrotMethod, "cr0_x", pDouble);
    MB_DefineLocal(mandelbrotMethod, "cr", pDouble);
    MB_DefineLocal(mandelbrotMethod, "ci", pDouble);

    MB_DefineLocal(mandelbrotMethod, "limit", types->Double);
    MB_DefineLocal(mandelbrotMethod, "limit_sq", types->Double);
    MB_DefineLocal(mandelbrotMethod, "ci0", types->Double);
    MB_DefineLocal(mandelbrotMethod, "cr_k", types->Double);
    MB_DefineLocal(mandelbrotMethod, "cr_k_sq", types->Double);
    MB_DefineLocal(mandelbrotMethod, "ci_k", types->Double);
    MB_DefineLocal(mandelbrotMethod, "ci_k_sq", types->Double);

    MB_DefineLocal(mandelbrotMethod, "width", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "height", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "max_x", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "max_iterations", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "x", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "y", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "i", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "k", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "xk", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "bits", types->Int32);
    MB_DefineLocal(mandelbrotMethod, "bit_k", types->Int32);

    // marking all locals as defined allows remaining locals to be temps
    // which enables further optimization opportunities particularly for
    // floating point types
    MB_AllLocalsHaveBeenDefined(mandelbrotMethod);

    MB_setBuildIlCallback(mandelbrotMethod, &buildIL);

    void* entry = 0;
    int32_t rc = compileMethodBuilder(mandelbrotMethod, &entry);

    if (rc!=0) {
        fprintf(stderr, "FAIL: compilation error %d\n", rc);
        exit(-2);
    }

    printf("Step 4: run mandelbrot compiled code\n");
    MandelbrotFunctionType* mandelbrot = (MandelbrotFunctionType*) entry;
    const int height = N;
    const int max_x = N;
    const int size = height*max_x*sizeof(uint8_t);
    uint8_t* buffer = (uint8_t*) malloc(size);
    double* cr0 = (double*) malloc(8*max_x*sizeof(double));

    mandelbrot(N, buffer, cr0);

    printf("Step 5: output result buffer\n");
    FILE* out = (argc==3) ? fopen(argv[2], "wb") : stdout;
    fprintf(out, "P4\n%u %u\n", max_x, height);
    fwrite(buffer, size, 1, out);

    if (out!=stdout) {
        fclose(out);
    }

    printf("Step 6: shutdown JIT\n");
    DeleteIlType(pInt8);
    DeleteIlType(pDouble);
    DeleteMethodBuilder(mandelbrotMethod);
    DeleteTypeDictionary(types);
    shutdownJit();

    printf("PASS\n");
}
