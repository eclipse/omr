/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatability */
#if defined(LINUX) && !defined(OMRZTPF)
#define _GNU_SOURCE
#endif /* defined(LINUX) && !defined(OMRZTPF) */
#if defined(OMRZTPF)
#include <tpf/c_cinfc.h>   /* for access to cinfc */
#include <tpf/c_pi1dt.h>   /* for access to machine type. */
#endif /* defined(OMRZTPF) */

#include <string.h>
#include <stdio.h>

#include "omrutil.h"

/*
 * The following are the 2 versions of the /proc/cpuinfo file for Linux/390.
 *
 * When Gallileo (TREX) comes out, we will need to update this routine to support
 * TREX and not end up generating default 9672 code!
 * vendor_id       : IBM/S390
 * # processors    : 2
 * bogomips per cpu: 838.86
 * processor 0: version = FF,  identification = 100003,  machine = 2064
 * processor 1: version = FF,  identification = 200003,  machine = 2064
 *
 * vs.
 *
 * vendor_id       : IBM/S390
 * # processors    : 3
 * bogomips per cpu: 493.15
 * processor 0: version = FF,  identification = 100003,  machine = 9672
 * processor 1: version = FF,  identification = 200003,  machine = 9672
 * processor 2: version = FF,  identification = 300003,  machine = 9672
 */

const int S390UnsupportedMachineTypes[] = {
	G5, MULTIPRISE7000
};

/* Fetch the current machine type from /proc/cpuinfo on zLinux platform
 * The return value is the machine type (int) as defined in omrutil.h
 * A value of -1 is returned upon error or when an unsupported machine
 * platform is detected
 */
int32_t
get390zLinuxMachineType(void)
{
	int machine = -1;
#if (defined (LINUX) && defined(S390) && !defined(OMRZTPF))
	int i;
	char line[80];
	const int LINE_SIZE = sizeof(line) - 1;
	const char procHeader[] = "processor ";
	const int PROC_LINE_SIZE = 69;
	const int PROC_HEADER_SIZE = sizeof(procHeader) - 1;



	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (fp) {
		while (fgets(line, LINE_SIZE, fp) > 0) {
			int len = (int)strlen(line);

			if (len > PROC_HEADER_SIZE && !memcmp(line, procHeader, PROC_HEADER_SIZE)) {
				if (len == PROC_LINE_SIZE) {
					/* eg. processor 0: version = FF,  identification = 100003,  machine = 9672 */
					sscanf(line,
						   "%*s %*d%*c %*s %*c %*s %*s %*c %*s %*s %*c %d",
						   &machine);
				}
			}
		}
		fclose(fp);
	}

	/* Scan list of unsupported machines - We do not initialize the JIT for such hardware. */
	for (i = 0; i < sizeof(S390UnsupportedMachineTypes) / sizeof(int); ++i) {
		if (machine == S390UnsupportedMachineTypes[i]) {
			machine = -1; /* unsupported platform */
		}
	}
#elif defined(OMRZTPF)
        struct pi1dt *pid;
        /* machine hardware name */
        pid = (struct pi1dt *)cinfc_fast(CINFC_CMMPID);
        machine = (int)(pid->pi1pids.pi1mslr.pi1mod);
#endif
	return machine;
}
