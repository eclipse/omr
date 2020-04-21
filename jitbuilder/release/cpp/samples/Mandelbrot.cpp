/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "Mandelbrot.hpp"


MandelbrotMethod::MandelbrotMethod(OMR::JitBuilder::TypeDictionary *types)
   : OMR::JitBuilder::MethodBuilder(types)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   pDouble = types->PointerTo(Double);
   pInt8 = types->PointerTo(Int8);

   DefineName("mandelbrot");
   DefineParameter     ("N",      Int32);
   DefineArrayParameter("buffer", pInt8);
   DefineArrayParameter("cr0",    pDouble);
   DefineReturnType(NoType);

   DefineLocal("line", pInt8);
   DefineLocal("cr0_x", pDouble);
   DefineLocal("cr", pDouble);
   DefineLocal("ci", pDouble);

   DefineLocal("limit", Double);
   DefineLocal("limit_sq", Double);
   DefineLocal("ci0", Double);
   DefineLocal("cr_k", Double);
   DefineLocal("cr_k_sq", Double);
   DefineLocal("ci_k", Double);
   DefineLocal("ci_k_sq", Double);

   DefineLocal("width", Int32);
   DefineLocal("height", Int32);
   DefineLocal("max_x", Int32);
   DefineLocal("max_iterations", Int32);
   DefineLocal("x", Int32);
   DefineLocal("y", Int32);
   DefineLocal("i", Int32);
   DefineLocal("k", Int32);
   DefineLocal("xk", Int32);
   DefineLocal("bits", Int32);
   DefineLocal("bit_k", Int32);
   }

//
bool
MandelbrotMethod::buildIL()
   {
   // marking all locals as defined allows remaining locals to be temps
   // which enables further optimization opportunities particularly for
   //    floating point types
   AllLocalsHaveBeenDefined();

   Store("cr",
      CreateLocalArray(8, Double));
   Store("ci",
      CreateLocalArray(8, Double));

   Store("width", Load("N"));
   Store("height", Load("N"));
   Store("max_x",
      Div(
         Add(
            Load("width"),
            ConstInt32(7)),
         ConstInt32(8)));
   Store("max_iterations", ConstInt32(50));
   Store("limit", ConstDouble(2.0));
   Store("limit_sq",
      Mul(
         Load("limit"),
         Load("limit")));

   IlBuilder *x1Loop=NULL;
   ForLoopUp("x", &x1Loop,
      ConstInt32(0),
      Load("max_x"),
      ConstInt32(1));

   IlBuilder *k1Loop=NULL;
   x1Loop->ForLoopUp("k", &k1Loop,
   x1Loop->          ConstInt32(0),
   x1Loop->          ConstInt32(8),
   x1Loop->          ConstInt32(1));

   k1Loop->Store("xk",
   k1Loop->   Add(
   k1Loop->      Mul(
   k1Loop->         ConstInt32(8),
   k1Loop->         Load("x")),
   k1Loop->      Load("k")));

   k1Loop->StoreAt(
   k1Loop->   IndexAt(pDouble,
   k1Loop->      Load("cr0"),
   k1Loop->      Load("xk")),
   k1Loop->   Sub(
   k1Loop->      Div(
   k1Loop->         Mul(
   k1Loop->            ConstDouble(2.0),
   k1Loop->            ConvertTo(Double,
   k1Loop->               Load("xk"))),
   k1Loop->         ConvertTo(Double,
   k1Loop->            Load("width"))),
   k1Loop->      ConstDouble(1.5)));

   IlBuilder *y2Loop=NULL;
   ForLoopUp("y", &y2Loop,
      ConstInt32(0),
      Load("height"),
      ConstInt32(1));

   y2Loop->Store("line",
   y2Loop->   IndexAt(pInt8,
   y2Loop->      Load("buffer"),
   y2Loop->      Mul(
   y2Loop->         Load("y"),
   y2Loop->         Load("max_x"))));

   y2Loop->Store("ci0",
   y2Loop->   Sub(
   y2Loop->      Div(
   y2Loop->         Mul(
   y2Loop->            ConstDouble(2.0),
   y2Loop->            ConvertTo(Double,
   y2Loop->               Load("y"))),
   y2Loop->         ConvertTo(Double,
   y2Loop->            Load("height"))),
   y2Loop->      ConstDouble(1.0)));

   IlBuilder *x2Loop=NULL;
   y2Loop->ForLoopUp("x", &x2Loop,
   y2Loop->   ConstInt32(0),
   y2Loop->   Load("max_x"),
   y2Loop->   ConstInt32(1));

   x2Loop->Store("cr0_x",
   x2Loop->   IndexAt(pDouble,
   x2Loop->      Load("cr0"),
   x2Loop->      Mul(
   x2Loop->         ConstInt32(8),
   x2Loop->         Load("x"))));

   IlBuilder *k2Loop=NULL;
   x2Loop->ForLoopUp("k", &k2Loop,
   x2Loop->   ConstInt32(0),
   x2Loop->   ConstInt32(8),
   x2Loop->   ConstInt32(1));

   k2Loop->StoreAt(
   k2Loop->   IndexAt(pDouble,
   k2Loop->      Load("cr"),
   k2Loop->      Load("k")),
   k2Loop->   LoadAt(pDouble,
   k2Loop->      IndexAt(pDouble,
   k2Loop->         Load("cr0_x"),
   k2Loop->         Load("k"))));

   k2Loop->StoreAt(
   k2Loop->   IndexAt(pDouble,
   k2Loop->      Load("ci"),
   k2Loop->      Load("k")),
   k2Loop->   Load("ci0"));

   x2Loop->Store("bits",
   x2Loop->   ConstInt32(0xff));

   IlBuilder *i2Loop=NULL, *i2Break=NULL;
   x2Loop->ForLoopWithBreak(true, "i", &i2Loop, &i2Break,
   x2Loop->   ConstInt32(0),
   x2Loop->   Load("max_iterations"),
   x2Loop->   ConstInt32(1));

   i2Loop->IfCmpEqualZero(&i2Break,
   i2Loop->   Load("bits"));

   i2Loop->Store("bit_k",
   i2Loop->   ConstInt32(0x80));

   IlBuilder *k3Loop=NULL;
   i2Loop->ForLoopUp("k", &k3Loop,
   i2Loop->   ConstInt32(0),
   i2Loop->   ConstInt32(8),
   i2Loop->   ConstInt32(1));

   IlBuilder *bit_k_set=NULL;
   k3Loop->IfThen(&bit_k_set,
   k3Loop->   And(
   k3Loop->      Load("bits"),
   k3Loop->      Load("bit_k")));

   bit_k_set->Store("cr_k",
   bit_k_set->   LoadAt(pDouble,
   bit_k_set->      IndexAt(pDouble,
   bit_k_set->         Load("cr"),
   bit_k_set->         Load("k"))));

   bit_k_set->Store("ci_k",
   bit_k_set->   LoadAt(pDouble,
   bit_k_set->      IndexAt(pDouble,
   bit_k_set->         Load("ci"),
   bit_k_set->         Load("k"))));

   bit_k_set->Store("cr_k_sq",
   bit_k_set->   Mul(
   bit_k_set->      Load("cr_k"),
   bit_k_set->      Load("cr_k")));

   bit_k_set->Store("ci_k_sq",
   bit_k_set->   Mul(
   bit_k_set->      Load("ci_k"),
   bit_k_set->      Load("ci_k")));

   bit_k_set->StoreAt(
   bit_k_set->   IndexAt(pDouble,
   bit_k_set->      Load("cr"),
   bit_k_set->      Load("k")),
   bit_k_set->   Add(
   bit_k_set->      Sub(
   bit_k_set->         Load("cr_k_sq"),
   bit_k_set->         Load("ci_k_sq")),
   bit_k_set->      LoadAt(pDouble,
   bit_k_set->         IndexAt(pDouble,
   bit_k_set->            Load("cr0_x"),
   bit_k_set->            Load("k")))));

   bit_k_set->StoreAt(
   bit_k_set->   IndexAt(pDouble,
   bit_k_set->      Load("ci"),
   bit_k_set->      Load("k")),
   bit_k_set->   Add(
   bit_k_set->      Mul(
   bit_k_set->         Mul(
   bit_k_set->            ConstDouble(2.0),
   bit_k_set->            Load("cr_k")),
   bit_k_set->         Load("ci_k")),
   bit_k_set->      Load("ci0")));

   IlBuilder *clear_bit_k=NULL;
   bit_k_set->IfThen(&clear_bit_k,
   bit_k_set->   GreaterThan(
   bit_k_set->      Add(
   bit_k_set->         Load("cr_k_sq"),
   bit_k_set->         Load("ci_k_sq")),
   bit_k_set->      Load("limit_sq")));

   clear_bit_k->Store("bits",
   clear_bit_k->   Xor(
   clear_bit_k->      Load("bits"),
   clear_bit_k->      Load("bit_k")));

   k3Loop->Store("bit_k",
   k3Loop->   UnsignedShiftR(
   k3Loop->      Load("bit_k"),
   k3Loop->      ConstInt32(1)));

   x2Loop->StoreAt(
   x2Loop->   IndexAt(pInt8,
   x2Loop->      Load("line"),
   x2Loop->      Load("x")),
   x2Loop->   ConvertTo(Int8,
   x2Loop->      Load("bits")));

   Return();

   return true;
   }

#define max(a,b) ((a)>(b)?(a):(b))

int
main(int argc, char *argv[])
   {
   if (argc < 2)
      {
      fprintf(stderr, "Usage: mandelbrot <N> [output file name in PPM format]\n");
      exit(-1);
      }
   const int N = max(0, (argc > 1) ? atoi(argv[1]) : 0);

   printf("Step 1: initialize JIT\n");
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }

   printf("Step 2: define relevant types\n");
   OMR::JitBuilder::TypeDictionary types;

   printf("Step 3: compile method builder\n");
   MandelbrotMethod mandelbrotMethod(&types);
   void *entry = 0;
   int32_t rc = compileMethodBuilder(&mandelbrotMethod, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: run mandelbrot compiled code\n");
   MandelbrotFunctionType *mandelbrot = (MandelbrotFunctionType *)entry;
   const int height = N;
   const int max_x = N;
   const int size = height * max_x * sizeof(uint8_t);
   uint8_t *buffer = (uint8_t *) malloc(size);
   double *cr0 = (double *) malloc(8 * max_x * sizeof(double));

   mandelbrot(N, buffer, cr0);

   printf("Step 5: output result buffer\n");
   FILE *out = (argc == 3) ? fopen(argv[2], "wb") : stdout;

   // The file we generate here is in the PPM image format which has a simple header describing how to interpret the
   // buffer. This string needs to be encoded as ASCII in the file generated, however on platforms such as z/OS whose
   // native encoding is not ASCII this is problematic.
   //
   // TODO (#4901): Use the port library to output the header bytes for PPM image file format
   fprintf(out, "P4\n%u %u\n", max_x, height);
   fwrite(buffer, size, 1, out);

   if (out != stdout)
      {
      fclose(out);
      }

   printf ("Step 6: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
