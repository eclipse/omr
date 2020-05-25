/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
#include <time.h>

typedef double FP_t;

__attribute__((noinline))
void mandelbrot(int x_pixels, int y_pixels, int* out_table, int max_iterations)
   {
   FP_t x_scale_factor = 3.5 / (FP_t) x_pixels;
   FP_t y_scale_factor = 2.0 / (FP_t) y_pixels;

   for (int py = 0; py < y_pixels; ++py)
      {
      for (int px = 0; px < x_pixels; ++px)
         {
         FP_t x0 = px * x_scale_factor - 2.5;
         FP_t y0 = py * y_scale_factor - 1.0;

         FP_t x = 0.0;
         FP_t y = 0.0;
         int iteration = 0;
         while (x*x + y*y <= 2*2)
            {
            if (iteration == max_iterations) break;
            FP_t x_temp = x*x - y*y + x0;
            y = 2*x*y + y0;
            x = x_temp;
            iteration += 1;
            }

         out_table[py*x_pixels + px] = iteration;
         }
      }
   }

void print_image(int x_pixels, int y_pixels, int* table)
   {
   for (int py = 0; py < y_pixels; ++py)
      {
      for (int px = 0; px < x_pixels; ++px)
         {
         if (table[py*x_pixels + px] > 150)
            {
            printf("*");
            }
         else
            {
            printf(" ");
            }
         }
      printf("\n");
      }
   }

void tdiff(struct timespec * start, struct timespec * end, struct timespec * diff)
   {
   if ((end->tv_nsec-start->tv_nsec)<0)
      {
      diff->tv_sec = end->tv_sec-start->tv_sec-1;
      diff->tv_nsec = 1000000000+end->tv_nsec-start->tv_nsec;
      }
      else
      {
      diff->tv_sec = end->tv_sec-start->tv_sec;
      diff->tv_nsec = end->tv_nsec-start->tv_nsec;
      }
   }

int main(void)
   {
   int small_table [3][4];
   mandelbrot(4, 3, (int*)small_table, 10);
   print_image(4, 3, (int*)small_table);

   int table[34][80];
   clockid_t clockid = CLOCK_MONOTONIC;

   struct timespec start_time, end_time;
   clock_gettime(clockid, &start_time);
   mandelbrot(80, 34, (int*)table, 300000);
   clock_gettime(clockid, &end_time);

   struct timespec time_diff;
   tdiff(&start_time, &end_time, &time_diff);
   FP_t time_taken = (double)time_diff.tv_sec + time_diff.tv_nsec*1.0e-9;
   printf("Calculated Mandelbrot set in %lf (s)\n", time_taken);

   print_image(80, 34, (int*)table);

   return 0;
   }
