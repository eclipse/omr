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
 *******************************************************************************/

#include "omrTest.h"
#include "j9hypervisor.h"
#include "omrTestHelpers.h"
#include "portTestHelpers.hpp"

TEST(PortHypervisorTest, DISABLED_HypervisorPresent)
{
	PORT_ACCESS_FROM_PORT(portTestEnv->getPortLibrary());

	intptr_t rc = 0;
	BOOLEAN isVirtual = FALSE;

	if (NULL != portTestEnv->hypervisor) {
		/* -hypervisor *was* specified */
		isVirtual = TRUE;
	}

	rc = j9hypervisor_hypervisor_present();
	if (!isVirtual) {
		/* Test for Non virtualized Environment */
		switch (rc) {
		case J9HYPERVISOR_NOT_PRESENT: {
			/* Pass case, since -hypervisor was NOT specified; neither was it detected. */
			outputComment(PORTLIB,
						  "\t\tTest Passed: No Hypervisor present and j9hypervisor_hypervisor_present()"
						  "returned false\n");

			break;
		}
		case J9PORT_ERROR_HYPERVISOR_UNSUPPORTED: {
			/* Pass case; give benefit of doubt since hypervisor detection is NOT supported. */
			outputComment(PORTLIB, "\t\tThis platform does not support Hypervisor detection\n");
			break;
		}
		case J9HYPERVISOR_PRESENT: {
			J9HypervisorVendorDetails vendDetails;
			/* Failure scenario, since we ARE running on hypervisor, and yet missing the -hypervisor
			 * specification on the command line to test.
			 */
			rc = j9hypervisor_get_hypervisor_info(&vendDetails);
			if (0 == rc) {
				OMRTEST_EXPECT_FAILURE("Expecting bare metal machine but is running on hypervisor: \n" << vendDetails.hypervisorName <<
									   "[Info]: Please verify the machine capabilities.\n");
			} else {
				OMRTEST_EXPECT_FAILURE("Expecting bare metal machine but is running on hypervisor: <unknown>\n"
									   "[Info]: Failed to obtain hypervisor information:" << rc);
			}
			break;
		}
		default: { /* Capture any possible error codes reported (other than hypdetect unsupported). */
			OMRTEST_EXPECT_FAILURE("j9hypervisor_hypervisor_present() returned: " << rc << " when 0 (J9HYPERVISOR_NOT_PRESENT) was expected\n");
		}
		};
	} else {
		/* Test for Virtualized Environment */
		switch (rc) {
		case J9HYPERVISOR_PRESENT: {
			/* Pass case, since we detected a hypervisor as also found -hypervisor specified. */
			outputComment(PORTLIB, "\t\tTest Passed: Expected a hypervisor and"
						  " j9hypervisor_hypervisor_present() returned true\n");
			break;
		}
		case J9PORT_ERROR_HYPERVISOR_UNSUPPORTED: {
			if (portTestEnv->negative) {
				/* If this is a negative test case then we expect hypervisor detection to return
				 * error code: J9PORT_ERROR_HYPERVISOR_UNSUPPORTED, when -hypervisor is specified.
				 */
				outputComment(PORTLIB, "\t\tTest (negative case) passed: Expected an "
							  "unsupported hypervisor.\n\tj9hypervisor_hypervisor_present() returned "
							  "-856 (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED).\n");
			} else {
				/* Failed test if this is /not/ a negative test, since it looks like it indeed
				 * detected an unsupported hypervisor (other the ones passed for negative tests).
				 */
				OMRTEST_EXPECT_FAILURE("j9hypervisor_hypervisor_present() "
									   "returned: -856 (J9PORT_ERROR_HYPERVISOR_UNSUPPORTED).\n");
			}
			break;
		}
		case J9HYPERVISOR_NOT_PRESENT: {
			OMRTEST_EXPECT_FAILURE("Expecting virtualized environment whereas no hypervisor detected\n");
			break;
		}
		default: {
			OMRTEST_EXPECT_FAILURE("j9hypervisor_hypervisor_present() returned: " << rc << " when 1 (true) was expected\n");
		}
		};
	}
}
