/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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



/*
 * $RCSfile: si_numcpusTest.c,v $
 * $Revision: 1.7 $
 * $Date: 2012-08-28 17:00:47 $
 */
#include "testHelpers.hpp"

/**
 * Get number of physical CPUs. Validate number > 0.
 */
TEST(PortSysinfoTest, sysinfo_numcpus_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_numcpus_test0";
	uintptr_t numberPhysicalCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_PHYSICAL);

	outputComment(OMRPORTLIB, "\n");
	outputComment(OMRPORTLIB, "Get number of physical CPUs.\n");
	outputComment(OMRPORTLIB, "	Expected: >0\n");
	outputComment(OMRPORTLIB, "	Result:   %d\n", numberPhysicalCPUs);

	if (0 == numberPhysicalCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tomrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_PHYSICAL) returned invalid value 0.\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Validates that number of bound CPUs returned is the same as current affinity.
 */
TEST(PortSysinfoTest, sysinfo_numcpus_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_numcpus_test1";
	uintptr_t numberPhysicalCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);
	uintptr_t numberBoundCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_BOUND);

	intptr_t boundCPUs = 0;
	char *boundTestArg = NULL;
	for (int i = 1; i < portTestEnv->_argc; i += 1) {
		if (startsWith(portTestEnv->_argv[i], BOUNDCPUS)) {
			boundCPUs = -1;
			boundTestArg = &portTestEnv->_argv[i][strlen(BOUNDCPUS)];
			boundCPUs = atoi(boundTestArg);
			if (-1 == boundCPUs) {
				outputComment(OMRPORTLIB, "Invalid (non-numeric) format for %s value (%s).\n", BOUNDCPUS, boundTestArg);
				boundCPUs = 0;
			}
		}
	}

	if (0 == boundCPUs) {
		uintptr_t numberPhysicalCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);
		uintptr_t numberBoundCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_BOUND);
		if (numberBoundCPUs < numberPhysicalCPUs) {
			outputComment(OMRPORTLIB, "The test was configured to run without CPU restrictions (e.g. taskset), but CPU restrictions were detected.\n");
			outputComment(OMRPORTLIB, "The test will continue with CPU restrictions configuration.\n\n");
			boundCPUs = numberBoundCPUs;
		}
	}
	uintptr_t boundTest = boundCPUs;

	outputComment(OMRPORTLIB, "\n");
	outputComment(OMRPORTLIB, "Get number of bound CPUs.\n");
	if (0 == boundTest) {
		outputComment(OMRPORTLIB, "	Expected: %d\n", numberPhysicalCPUs);
		outputComment(OMRPORTLIB, "	Result:   %d\n", numberBoundCPUs);
	} else {
		outputComment(OMRPORTLIB, "	Expected: %d\n", boundTest);
		outputComment(OMRPORTLIB, "	Result:   %d\n", numberBoundCPUs);
	}

	if ((0 == boundTest) && (numberPhysicalCPUs == numberBoundCPUs)) {
		/* Not bound, bound should be equal to physical */
	} else if ((0 != boundTest) && (numberBoundCPUs == boundTest)) {
		/* Bound. Confirm that binding is correct */
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\nomrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_BOUND) returned unexpected value.\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Get number of online CPUs. Validate number > 0.
 */
TEST(PortSysinfoTest, sysinfo_numcpus_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_numcpus_test2";
	uintptr_t numberOnlineCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);

	outputComment(OMRPORTLIB, "\n");
	outputComment(OMRPORTLIB, "Get number of online CPUs.\n");
	outputComment(OMRPORTLIB, "	Expected: >0\n");
	outputComment(OMRPORTLIB, "	Result:   %d\n", numberOnlineCPUs);

	if (0 == numberOnlineCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tomrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE) returned invalid value 0.\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Get number of online and physical CPUs. Validate online <= physical.
 */
TEST(PortSysinfoTest, sysinfo_numcpus_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_numcpus_test3";
	uintptr_t numberOnlineCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);
	uintptr_t numberPhysicalCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_PHYSICAL);

	outputComment(OMRPORTLIB, "\nGet number of online and physical CPUs. Validate online <= physical.\n");

	if (numberOnlineCPUs > numberPhysicalCPUs) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tomrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE) returned value greater than omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_PHYSICAL).\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Call Port Library function omrsysinfo_get_number_CPUs_by_type with invalid type. Validate that return value is 0.
 */
TEST(PortSysinfoTest, sysinfo_numcpus_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_numcpus_test4";
	uintptr_t result = omrsysinfo_get_number_CPUs_by_type(500);

	outputComment(OMRPORTLIB, "\n");
	outputComment(OMRPORTLIB, "Call omrsysinfo_get_number_CPUs_by_type with invalid type.\n");
	outputComment(OMRPORTLIB, "	Expected: 0\n");
	outputComment(OMRPORTLIB, "	Result:   %d\n", result);

	if (result > 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tomrsysinfo_get_number_CPUs_by_type returned non-zero value when given invalid type.\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Measure run time for getting number of CPUs 10000 times.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] type The type of CPU number to query (physical, bound, entitled, target).
 *
 * @return TEST_PASSED on success, TEST_FAILED on failure
 */
int omrsysinfo_numcpus_runTime(OMRPortLibrary *portLibrary, uintptr_t type)
{
#define J9SYSINFO_NUMCPUS_RUNTIME_LOOPS 1000
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrsysinfo_numcpus_runTime";
	int i = 0;
	const char *testType = "";
	I_64 difference = 0;
	I_64 after = 0;
	I_64 before = omrtime_nano_time();

	for (i = 0; i < J9SYSINFO_NUMCPUS_RUNTIME_LOOPS; i++) {
		omrsysinfo_get_number_CPUs_by_type(type);
	}

	after = omrtime_nano_time();
	difference = after - before;

	switch (type) {
	case OMRPORT_CPU_PHYSICAL:
		testType = "physical";
		break;
	case OMRPORT_CPU_ONLINE:
		testType = "online";
		break;
	case OMRPORT_CPU_BOUND:
		testType = "bound";
		break;
	case OMRPORT_CPU_ENTITLED:
		testType = "entitled";
		break;
	case OMRPORT_CPU_TARGET:
		testType = "target";
		break;
	default:
		testType = "";
		break;
	}
	outputComment(OMRPORTLIB, "\n");
	outputComment(OMRPORTLIB, "Get number of %s CPUs %d times: %d ns\n",
				  testType,
				  J9SYSINFO_NUMCPUS_RUNTIME_LOOPS,
				  difference);

	return reportTestExit(OMRPORTLIB, testName);
}

TEST(PortSysinfoTest, sysinfo_numcpus_runTime)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	EXPECT_TRUE(TEST_PASS == omrsysinfo_numcpus_runTime(OMRPORTLIB, OMRPORT_CPU_PHYSICAL)) << "Test failed.";
	EXPECT_TRUE(TEST_PASS == omrsysinfo_numcpus_runTime(OMRPORTLIB, OMRPORT_CPU_ONLINE)) << "Test failed.";
	EXPECT_TRUE(TEST_PASS == omrsysinfo_numcpus_runTime(OMRPORTLIB, OMRPORT_CPU_BOUND)) << "Test failed.";
}

#if !defined(J9ZOS390)
/**
 * @internal
 * Internal function: Counts up the number of processors that are online as per the
 * records delivered by the port library routine omrsysinfo_get_processor_info().
 *
 * @param[in] procInfo Pointer to J9ProcessorInfos filled in with processor info records.
 *
 * @return Number (count) of online processors.
 */
static uint32_t
onlineProcessorCount(const struct J9ProcessorInfos *procInfo)
{
	register int32_t cntr = 0;
	register uint32_t n_onln = 0;

	for (cntr = 1; cntr < procInfo->totalProcessorCount + 1; cntr++) {
		if (OMRPORT_PROCINFO_PROC_ONLINE == procInfo->procInfoArray[cntr].online) {
			n_onln++;
		}
	}
	return n_onln;
}

/**
 * Test for omrsysinfo_get_number_online_CPUs() port library API. We obtain the online
 * processor count using other (indirect) method - calling the other port library API
 * omrsysinfo_get_processor_info() and cross-check against this.
 */
TEST(PortSysinfoTest, sysinfo_testOnlineProcessorCount1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_testOnlineProcessorCount1";
	intptr_t rc = 0;
	J9ProcessorInfos procInfo = {0};

	outputComment(OMRPORTLIB, "\n");
	reportTestEntry(OMRPORTLIB, testName);

	/* Call omrsysinfo_get_processor_info() to retrieve a set of processor records from
	 * which we may then ascertain the number of processors online. This will help us
	 * cross-check against the API currently under test.
	 */
	rc = omrsysinfo_get_processor_info(&procInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_processor_info() failed.\n");

		/* Should not try freeing memory unless it was actually allocated! */
		if (OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED != rc) {
			omrsysinfo_destroy_processor_info(&procInfo);
		}
		reportTestExit(OMRPORTLIB, testName);
		return;

	} else {
		/* Call the port library API omrsysinfo_get_number_online_CPUs() to check that the online
		 * processor count received is valid (that is, it does not fail) and that this indeed
		 * matches the online processor count as per the processor usage retrieval API.
		 */
		uintptr_t n_cpus_online = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);
		if (0 == n_cpus_online) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE) failed.\n");
			goto _cleanup;
		}

		if ((n_cpus_online > 0) &&
			(onlineProcessorCount(&procInfo) == n_cpus_online)) {
			outputComment(OMRPORTLIB, "Number of online processors: %d\n",  n_cpus_online);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid online processor count found.\n");
		}
	}

_cleanup:
	omrsysinfo_destroy_processor_info(&procInfo);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test for omrsysinfo_get_number_total_CPUs() port library API. Validate the number of
 * available (configured) logical CPUs by cross-checking with what is obtained from
 * invoking the other port library API omrsysinfo_get_processor_info().
 */
TEST(PortSysinfoTest, sysinfo_testtotalProcessorCount)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsysinfo_testTotalProcessorCount";
	intptr_t rc = 0;
	J9ProcessorInfos procInfo = {0};

	outputComment(OMRPORTLIB, "\n");
	reportTestEntry(OMRPORTLIB, testName);

	/* Call omrsysinfo_get_processor_info() to retrieve a set of processor records from
	 * which we may then ascertain the total number of processors configured. We then
	 * cross-check this against what the API currently under test returns.
	 */
	rc = omrsysinfo_get_processor_info(&procInfo);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_processor_info() failed.\n");

		/* Should not try freeing memory unless it was actually allocated! */
		if (OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED != rc) {
			omrsysinfo_destroy_processor_info(&procInfo);
		}
		reportTestExit(OMRPORTLIB, testName);
		return;
	} else {
		/* Ensure first that the API doesn't fail. If not, check that we obtained the correct total
		 * processor count by checking against what omrsysinfo_get_processor_info() returned.
		 */
		uintptr_t n_cpus_physical = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_PHYSICAL);
		if (0 == n_cpus_physical) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_PHYSICAL) failed.\n");
			goto _cleanup;
		}

		if ((procInfo.totalProcessorCount > 0) &&
			((uintptr_t)procInfo.totalProcessorCount == n_cpus_physical)) {
			outputComment(OMRPORTLIB, "Total number of processors: %d\n",  n_cpus_physical);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Invalid processor count retrieved.\n");
		}
	}

_cleanup:
	omrsysinfo_destroy_processor_info(&procInfo);
	reportTestExit(OMRPORTLIB, testName);
}
#endif /* !defined(J9ZOS390) */
