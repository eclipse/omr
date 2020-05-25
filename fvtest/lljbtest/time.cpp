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

#include <ctime>
#include <cstdio>
#include <unistd.h>

void current_time(struct timespec *time)
   {
   clock_gettime(CLOCK_MONOTONIC, time);
   }

void time_diff(struct timespec * start, struct timespec * end, struct timespec * diff)
   {
   if ((end->tv_nsec - start->tv_nsec) < 0)
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

int main()
   {
   struct timespec startTime;
   struct timespec endTime;
   struct timespec diff;
   current_time(&startTime);
   usleep(2000*1000);
   current_time(&endTime);

   time_diff(&startTime, &endTime, &diff);
   double time = (double) diff.tv_sec + diff.tv_nsec*1.0e-9;

   printf("current time %lf (s)\n", time);
   return (int) time;
   }
