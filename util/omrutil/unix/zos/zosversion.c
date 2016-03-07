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
