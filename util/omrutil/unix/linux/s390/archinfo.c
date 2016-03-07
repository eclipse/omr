/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatability */
#if defined(LINUX)
#define _GNU_SOURCE
#endif /* defined(__GNUC__) */

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
#if (defined (LINUX) && defined(S390))
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
#endif
	return machine;
}
