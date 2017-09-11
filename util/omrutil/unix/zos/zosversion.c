/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available 
 *  under the terms of the Eclipse Public License 2.0 which 
 * accompanies this distribution and is available at 
 * https://www.eclipse.org/legal/epl-2.0/ or the 
 * Apache License, Version 2.0 which accompanies this distribution 
 *  and is available at https://www.apache.org/licenses/LICENSE-2.0.
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
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include "omrcomp.h"

/**
 * Function to determine if the zos version is at least a given
 * release and version.  The implementation is based on uname(),
 * NOT on __osname() as the __osname() release numbers are not
 * guarenteed to increase.
 *
 * For release and version numbers, see
 * 	http://publib.boulder.ibm.com/infocenter/zos/v1r10/index.jsp?topic=/com.ibm.zos.r10.bpxbd00/osnm.htm
 *
 * Operating System 	Sysname 	Release 	Version
 * z/OS V1.10			OS/390		20.00		03
 * z/OS 1.9				OS/390		19.00		03
 * z/OS 1.8 or z/OS.e 1.8 	OS/390		18.00		03
 * z/OS 1.7 or z/OS.e 1.7 	OS/390 		17.00 		03
 * z/OS 1.6 or z/OS.e 1.6 	OS/390 		16.00 		03
 */
BOOLEAN
zos_version_at_least(double min_release, double min_version)
{
	struct utsname sysinfo;
	int rc;

	rc = uname(&sysinfo);
	if (rc >= 0) {
		double release;
		double version;

		release = strtod(sysinfo.release, NULL);
		version = strtod(sysinfo.version, NULL);

		if (min_release < release) {
			return TRUE;
		} else if (min_release == release) {
			if (min_version <= version) {
				return TRUE;
			}
		}
	}
	return FALSE;
}
