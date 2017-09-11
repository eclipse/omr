/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if defined(AIXPPC) || defined(J9ZOS390)
#define __IBMCPP_TR1__ 1
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

#include <stdio.h>

#include "ddr/blobgen/genBlob.hpp"
#include "ddr/blobgen/java/genBinaryBlob.hpp"
#include "ddr/blobgen/java/genSuperset.hpp"

DDR_RC
genBlob(struct OMRPortLibrary *portLibrary, Symbol_IR *const ir, const char *supersetFile, const char *blobFile, bool printEmptyTypes)
{
	JavaSupersetGenerator supersetGenerator(printEmptyTypes);
	DDR_RC rc = supersetGenerator.printSuperset(portLibrary, ir, supersetFile);

	if (DDR_RC_OK == rc) {
		printf("Superset written to file: %s\n", supersetFile);

		JavaBlobGenerator blobGenerator(printEmptyTypes);
		rc = blobGenerator.genBinaryBlob(portLibrary, ir, blobFile);
	}

	if (DDR_RC_OK == rc) {
		printf("Blob written to file: %s\n", blobFile);
	}

	return rc;
}
