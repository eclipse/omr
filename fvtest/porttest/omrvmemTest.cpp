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


/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library virtual memory management.
 *
 * Exercise the API for port library virtual memory management operations.  These functions
 * can be found in the file @ref omrvmem.c
 *
 * @note port library virtual memory management operations are not optional in the port library table.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "testHelpers.hpp"
#include "omrport.h"
#include "omrmemcategories.h"

#if defined(AIXPPC)
#include <sys/vminfo.h>
#endif /* defined(AIXPPC) */

#define TWO_GIG_BAR 0x7FFFFFFF
#define ONE_MB (1*1024*1024)
#define FOUR_KB (4*1024)
#define TWO_GB	(0x80000000LL)
#define SIXTY_FOUR_GB (0x1000000000LL)

#define SIXTY_FOUR_KB (64*1024)
#define D2M (2*1024*1024)
#define D16M (16*1024*1024)
#define D512M (512*1024*1024)
#define D256M (256*1024*1024)
#define DEFAULT_NUM_ITERATIONS 50

#define MAX_ALLOC_SIZE 256 /**<@internal largest size to allocate */

#define allocNameSize 64 /**< @internal buffer size for name function */

#if defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(J9ZOS390) || defined(OSX)
#define ENABLE_RESERVE_MEMORY_EX_TESTS
#endif /* defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(J9ZOS390) || defined(OSX) */

#if defined(ENABLE_RESERVE_MEMORY_EX_TESTS)
static int omrvmem_testReserveMemoryEx_impl(struct OMRPortLibrary *, const char *, BOOLEAN, BOOLEAN, BOOLEAN, BOOLEAN, BOOLEAN);
static int omrvmem_testReserveMemoryEx_StandardAndQuickMode(struct OMRPortLibrary *, const char *, BOOLEAN, BOOLEAN, BOOLEAN, BOOLEAN);
static BOOLEAN memoryIsAvailable(struct OMRPortLibrary *, BOOLEAN);
#endif /* ENABLE_RESERVE_MEMORY_EX_TESTS */

#define CYCLES 10000
#define FREE_PROB 0.5

int myFunction1();
void myFunction2();

/**
 * @internal
 * Helper function for memory management verification.
 *
 * Given a pointer to an memory chunk verify it is
 * \arg a non NULL pointer
 * \arg correct size
 * \arg writeable
 * \arg aligned
 * \arg double aligned
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test requesting this functionality
 * @param[in] memPtr Pointer the memory under test
 * @param[in] byteAmount Size or memory under test in bytes
 * @param[in] allocName Calling function name to display in errors
 */
static void
verifyMemory(struct OMRPortLibrary *portLibrary, const char *testName, char *memPtr, uintptr_t byteAmount, const char *allocName)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char testCharA = 'A';
	const char testCharC = 'c';
	uintptr_t testSize;
	char stackMemory[MAX_ALLOC_SIZE];

	if (NULL == memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s returned NULL\n", allocName);
		return;
	}

	/* Don't trample memory */
	if (0 == byteAmount) {
		return;
	}

	/* Test write at start */
	memPtr[0] = testCharA;
	if (testCharA != memPtr[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test at 0\n", allocName);
	}

	/* Test write at middle */
	memPtr[byteAmount / 2] = testCharA;
	if (testCharA != memPtr[byteAmount / 2]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test at %u\n", allocName, byteAmount / 2);
	}

	/* Can only verify characters as large as the stack allocated size */
	if (MAX_ALLOC_SIZE < byteAmount) {
		testSize = MAX_ALLOC_SIZE;
	} else {
		testSize = byteAmount;
	}

	/* Fill the buffer start */
	memset(memPtr, testCharC, testSize);
	memset(stackMemory, testCharC, testSize);

	/* Check memory retained value */
	if (0 != memcmp(stackMemory, memPtr, testSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to retain value at start\n", allocName);
	}

	/* Fill the buffer end */
	memset(memPtr + byteAmount - testSize, testCharA, testSize);
	memset(stackMemory, testCharA, testSize);

	/* Check memory retained value */
	if (0 != memcmp(stackMemory, memPtr + byteAmount - testSize, testSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to retain value at end\n", allocName);
	}
}

/**
 * Verify port library memory management.
 *
 * Ensure the port library is properly setup to run vmem operations.
 */
TEST(PortVmemTest, vmem_verify_functiontable_slots)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_verify_functiontable_slots";

	reportTestEntry(OMRPORTLIB, testName);

	/* Verify that the memory management function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	if (NULL == OMRPORTLIB->vmem_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_startup is NULL\n");
	}

	/*  Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->vmem_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_shutdown is NULL\n");
	}

	/* omrmem_test2 */
	if (NULL == OMRPORTLIB->vmem_reserve_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_reserve_memory is NULL\n");
	}

	/* omrmem_test1 */
	if (NULL == OMRPORTLIB->vmem_commit_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_commit_memory is NULL\n");
	}

	/* omrmem_test4 */
	if (NULL == OMRPORTLIB->vmem_decommit_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_decommit_memory, is NULL\n");
	}

	if (NULL == OMRPORTLIB->vmem_free_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_free_memory, is NULL\n");
	}

	if (NULL == OMRPORTLIB->vmem_supported_page_sizes) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_supported_page_sizes, is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library memory management.
 *
 * Make sure there is at least one page size returned by omrvmem_get_supported_page_sizes
 */
TEST(PortVmemTest, vmem_test_verify_there_are_page_sizes)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_test_verify_there_are_page_sizes";
	uintptr_t *pageSizes;
	uintptr_t *pageFlags;
	int i = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* First get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto exit;
	}

	outputComment(OMRPORTLIB, "\t Supported page sizes:\n");
	for (i = 0 ; pageSizes[i] != 0 ; i++) {
		outputComment(OMRPORTLIB, "\t 0x%zx ", pageSizes[i]);
		outputComment(OMRPORTLIB, "\t 0x%zx \n", pageFlags[i]);
	}
	outputComment(OMRPORTLIB, "\n");

exit:
	reportTestExit(OMRPORTLIB, testName);
}

#if defined(J9ZOS390)
/**
 * Returns TRUE if the given page size and flags corresponds to new page sizes added in z/OS.
 * z/OS traditionally supports 4K and 1M fixed pages.
 * 1M pageable and 2G fixed, added recently, are considered new page sizes.
 */
BOOLEAN
isNewPageSize(uintptr_t pageSize, uintptr_t pageFlags)
{
	if ((FOUR_KB == pageSize) ||
		((ONE_MB == pageSize) && (OMRPORT_VMEM_PAGE_FLAG_FIXED == (OMRPORT_VMEM_PAGE_FLAG_FIXED & pageFlags)))
	) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Returns TRUE if the given page size and flags can be used to allocate memory below 2G bar, FALSE otherwise.
 * On z/OS, 1M fixed and 2G fixed pages can be used only to allocate above 2G bar.
 * Only 4K and 1M pageable pages can be used to allocate memory below the bar.
 */
isPageSizeSupportedBelowBar(uintptr_t pageSize, uintptr_t pageFlags)
{
	if ((FOUR_KB == pageSize) ||
		((ONE_MB == pageSize) && (OMRPORT_VMEM_PAGE_FLAG_PAGEABLE == (OMRPORT_VMEM_PAGE_FLAG_PAGEABLE & pageFlags)))
	) {
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif /* J9ZOS390 */

/**
 * Get all the page sizes and make sure we can allocate a memory chunk for each page size.
 *
 * Checks that each allocation manipulates the memory categories appropriately.
 */
TEST(PortVmemTest, vmem_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_test1";
	char *memPtr = NULL;
	uintptr_t *pageSizes;
#if defined(J9ZOS390)
	uintptr_t *pageFlags;
#endif /* J9ZOS390 */
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	char allocName[allocNameSize];
	int32_t rc;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* First get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
#if defined(J9ZOS390)
	pageFlags =
#endif /* J9ZOS390 */
		omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0 ; pageSizes[i] != 0 ; i++) {
		uintptr_t initialBlocks;
		uintptr_t initialBytes;

		/* Sample baseline category data */
		getPortLibraryMemoryCategoryData(OMRPORTLIB, &initialBlocks, &initialBytes);

		/* reserve and commit */
#if defined(J9ZOS390)
		/* On z/OS skip this test for newly added large pages as obsolete omrvmem_reserve_memory() does not support them */
		if (TRUE == isNewPageSize(pageSizes[i], pageFlags[i])) {
			continue;
		}
#endif /* J9ZOS390 */
		memPtr = (char *)omrvmem_reserve_memory(
						0, pageSizes[i], &vmemID,
						OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT,
						pageSizes[i], OMRMEM_CATEGORY_PORT_LIBRARY);

		/* did we get any memory? */
		if (memPtr == NULL) {

			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", pageSizes[i], pageSizes[i], lastErrorNumber, lastErrorMessage);

			if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				outputComment(OMRPORTLIB, "\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				outputComment(OMRPORTLIB, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		} else {
			uintptr_t finalBlocks;
			uintptr_t finalBytes;
			outputComment(OMRPORTLIB, "\treserved and committed 0x%zx bytes with page size 0x%zx at address 0x%zx\n", pageSizes[i], vmemID.pageSize, memPtr);

			getPortLibraryMemoryCategoryData(OMRPORTLIB, &finalBlocks, &finalBytes);

			if (finalBlocks <= initialBlocks) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category block as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
			}

			if (finalBytes < (initialBytes + pageSizes[i])) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category bytes as expected. Initial bytes=%zu, final bytes=%zu, page size=%zu.\n", finalBytes, initialBytes, pageSizes[i]);
			}
		}

		/* can we read and write to the memory? */
		omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", pageSizes[i]);
		verifyMemory(OMRPORTLIB, testName, memPtr, pageSizes[i], allocName);

		/* free the memory (reuse the vmemID) */
		rc = omrvmem_free_memory(memPtr, pageSizes[i], &vmemID);
		if (rc != 0) {
			outputErrorMessage(
				PORTTEST_ERROR_ARGS,
				"omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n",
				rc, pageSizes[i], memPtr);
			goto exit;
		}

		{
			uintptr_t finalBlocks;
			uintptr_t finalBytes;

			getPortLibraryMemoryCategoryData(OMRPORTLIB, &finalBlocks, &finalBytes);

			if (finalBlocks != initialBlocks) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category block as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
			}

			if (finalBytes != initialBytes) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category bytes as expected. Initial bytes=%zu, final bytes=%zu, page size=%zu.\n", initialBytes, finalBytes, pageSizes[i]);
			}
		}
	}

exit:

	reportTestExit(OMRPORTLIB, testName);
}

static void
setDisclaimVirtualMemory(struct OMRPortLibrary *portLibrary, BOOLEAN disclaim)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	if (disclaim) {
		omrtty_printf("\n *** Disclaiming virtual memory\n\n");
		omrport_control(OMRPORT_CTLDATA_VMEM_ADVISE_OS_ONFREE, 1);
	} else {
		omrtty_printf("\n *** Not disclaiming virtual memory\n\n");
		omrport_control(OMRPORT_CTLDATA_VMEM_ADVISE_OS_ONFREE, 0);
	}
}

/**
 * 	1. Reserve memory
 * 	2. Time running numIterations times:
 * 			commit memory
 * 			write to all of it
 * 			decommit all of it
 * 	3. Free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] byteAmount This value is the amount of memory in bytes to use for each commit/decommit within the loop
 * @param[in] numIterations The number of times to iterate through the loop
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
omrvmem_bench_write_and_decommit_memory(struct OMRPortLibrary *portLibrary, uintptr_t pageSize, uintptr_t byteAmount, BOOLEAN disclaim, uintptr_t numIterations)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrvmem_bench_write_and_decommit_memory";
	char *memPtr = NULL;
	unsigned int i = 0;
	struct J9PortVmemIdentifier vmemID;
	intptr_t rc = 0;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	J9PortVmemParams params;

	reportTestEntry(OMRPORTLIB, testName);
	setDisclaimVirtualMemory(OMRPORTLIB, disclaim);

	/* reserve the memory, but don't commit it yet */
	omrvmem_vmem_params_init(&params);
	params.byteAmount = byteAmount;
	params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE;
	params.pageSize = pageSize;
	params.category = OMRMEM_CATEGORY_PORT_LIBRARY;
	params.options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;
	memPtr = (char *)omrvmem_reserve_memory_ex(&vmemID, &params);

	/* check if we got memory */
	if (memPtr == NULL) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(
			PORTTEST_ERROR_ARGS, "unable to reserve 0x%zx bytes with page size 0x%zx.\n"
			"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
		if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
			omrtty_printf("\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
			omrtty_printf("\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
		}
		goto exit;
	} else {
		omrtty_printf("\treserved 0x%zx bytes with page size 0x%zx at 0x%zx\n", byteAmount, vmemID.pageSize, memPtr);
	}

	startTimeNanos = omrtime_nano_time();

	for (i = 0; numIterations != i; i++) {

		memPtr = (char *)omrvmem_commit_memory(memPtr, byteAmount, &vmemID);
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_commit_memory returned NULL when trying to commit 0x%zx bytes backed by 0x%zx-byte pages\n", byteAmount, pageSize);
			goto exit;
		}

		/* write to the memory */
		memset(memPtr, 'c', byteAmount);

		/* decommit the memory */
		rc = omrvmem_decommit_memory(memPtr, byteAmount, &vmemID);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
			goto exit;
		}
	}

	deltaMillis = (omrtime_nano_time() - startTimeNanos) / (1000 * 1000);
	omrtty_printf("%s pageSize 0x%zx, byteAmount 0x%zx, millis for %u iterations: [write_to_all/decommit] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, numIterations, deltaMillis);

	rc = omrvmem_free_memory(memPtr, byteAmount, &vmemID);

	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPtr);
		goto exit;
	}

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/**
 * 	1. Reserve memory
 * 	2. Commit memory
 * 	3. Time:
 * 		write to all of it
 * 		decommit a chunk of memory
 * 		write to the chunk
 * 	4. Free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] physicalMemorySize The size of physical memory on the machine (e.g. as read from /proc/meminfo)
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] disclaim When true, the OS is told it may disclaim the memory
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
omrvmem_bench_force_overcommit_then_decommit(struct OMRPortLibrary *portLibrary, uintptr_t physicalMemorySize, uintptr_t pageSize, BOOLEAN disclaim)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrvmem_bench_force_overcommit_then_decommit";
	char *memPtr = NULL;
	struct J9PortVmemIdentifier vmemID;
	intptr_t rc;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	J9PortVmemParams params;
	uintptr_t byteAmount = physicalMemorySize + D512M;

	reportTestEntry(OMRPORTLIB, testName);
	setDisclaimVirtualMemory(OMRPORTLIB, disclaim);
	if (byteAmount < D512M) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "byteAmount 0x%zx bytes must be greater than 0x%zx\n", byteAmount, D512M);
		goto exit;
	}

	/* reserve the memory */
	omrvmem_vmem_params_init(&params);
	params.byteAmount = byteAmount;
	params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
	params.pageSize = pageSize;
	params.category = OMRMEM_CATEGORY_PORT_LIBRARY;
	params.options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;
	memPtr = (char *)omrvmem_reserve_memory_ex(&vmemID, &params);

	/* check we get memory */
	if (memPtr == NULL) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
						   "\tlastErrorNumber=%d, lastErrorMessage=%s\n ", byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
		if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
			omrtty_printf("\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
			omrtty_printf("\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
		}
		goto exit;
	} else {
		omrtty_printf("reserved and committed 0x%zx bytes of pagesize 0x%zx\n", byteAmount, pageSize);
	}

	startTimeNanos = omrtime_nano_time();

	/* write the first chunk */
	memset(memPtr, 5, D512M);

	/* byteAmount-D512M should be at least the amount of real memory on the system */
	memset(memPtr + D512M, 5, byteAmount - D512M);

	/* in order to write to region being decommitted, the system would have to go to disk,
	 * as the previous memset would have forced the region to have been paged out
	 */
	rc = omrvmem_decommit_memory(memPtr, D512M, &vmemID);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
		goto exit;
	}

	/* write to the decommitted region */
	memset(memPtr, 5, D512M);

	deltaMillis = (omrtime_nano_time() - startTimeNanos) / (1000 * 1000);
	omrtty_printf("%s pageSize 0x%zx, byteAmount 0x%zx, millis: [force page, decommit pagedout region, write decommitted region] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, deltaMillis);

	rc = omrvmem_free_memory(memPtr, byteAmount, &vmemID);

	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPtr);
		goto exit;
	}

exit:
	return reportTestExit(OMRPORTLIB, testName);
}



/**
 * 	1. Time running numIterations times:
 * 			reserve memory
 * 			write to all of it
 * 			decommit all of it
 * 			free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] byteAmount This value is the amount of memory in bytes to use for each commit/decommit within the loop
 * @param[in] disclaim When true, the OS is told it may disclaim the memory
 * @param[in] numIterations The number of times to iterate through the loop
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
omrvmem_bench_reserve_write_decommit_and_free_memory(struct OMRPortLibrary *portLibrary, uintptr_t pageSize, uintptr_t byteAmount, BOOLEAN disclaim, uintptr_t numIterations)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrvmem_bench_reserve_write_decommit_and_free_memory";
	char *memPtr = NULL;
	unsigned int i = 0;
	struct J9PortVmemIdentifier vmemID;
	intptr_t rc;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	J9PortVmemParams params;

	reportTestEntry(OMRPORTLIB, testName);
	setDisclaimVirtualMemory(OMRPORTLIB, disclaim);

	omrvmem_vmem_params_init(&params);
	params.byteAmount = byteAmount;
	params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
	params.pageSize = pageSize;
	params.category = OMRMEM_CATEGORY_PORT_LIBRARY;
	params.options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;

	startTimeNanos = omrtime_nano_time();

	/*
	 * Bench numIterations times: Reserve/write to all of it/decommit/free/
	 */
	for (i = 0; numIterations != i; i++) {

		memPtr = (char *)omrvmem_reserve_memory_ex(&vmemID, &params);

		/* check we get memory */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(
				PORTTEST_ERROR_ARGS, "In iteration %d, unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
				"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", i, byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
			if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				omrtty_printf("\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				omrtty_printf("\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		}

		/* write to the memory */
		memset(memPtr, 'c', byteAmount);

		rc = omrvmem_decommit_memory(memPtr, byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
			goto exit;
		}

		rc = omrvmem_free_memory(memPtr, byteAmount, &vmemID);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPtr);
			goto exit;
		}
	}

	deltaMillis = (omrtime_nano_time() - startTimeNanos) / (1000 * 1000);

	omrtty_printf("%s pageSize 0x%zx, byteAmount 0x%zx, millis for %u iterations: [reserve/write_to_all/decommit/free] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, numIterations, deltaMillis);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}


/**
 * 	1. Time 1000 times:
 * 			reserve memory
 * 			write to all of it
 * 			decommit all of it
 *  2. Free the memory
 *
 * @param[in] portLibrary The port library under test
 * @param[in] pageSize The page size to use for this test, e.g. 4096
 * @param[in] byteAmount This value is the amount of memory in bytes to use for each commit/decommit within the loop
 * @param[in] disclaim When true, the OS is told it may disclaim the memory
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
omrvmem_exhaust_virtual_memory(struct OMRPortLibrary *portLibrary, uintptr_t pageSize, uintptr_t byteAmount, BOOLEAN disclaim)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrvmem_exhaust_virtual_memory";
	char *memPtr = NULL;
	int i = 0;
#define NUM_VMEM_IDS 1000
	struct J9PortVmemIdentifier vmemID[NUM_VMEM_IDS];
	J9PortVmemParams params[NUM_VMEM_IDS];
	char *memPointers[NUM_VMEM_IDS];
	intptr_t rc;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	I_64 startTimeNanos;
	I_64 deltaMillis;
	uintptr_t totalAlloc = 0;

	reportTestEntry(OMRPORTLIB, testName);
	setDisclaimVirtualMemory(OMRPORTLIB, disclaim);
	startTimeNanos = omrtime_nano_time();
	/*
	 * Bench 1000 times:
	 * Reserve/commit/write to all of it/decommit
	 */
	for (i = 0; i < NUM_VMEM_IDS; i++) {

		omrvmem_vmem_params_init(&params[i]);
		params[i].byteAmount = byteAmount;
		params[i].mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
		params[i].pageSize = pageSize;
		params[i].category = OMRMEM_CATEGORY_PORT_LIBRARY;
		params[i].options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;

		memPtr = memPointers[i] = (char *)omrvmem_reserve_memory_ex(&vmemID[i], &params[i]);

		/* check we get memory */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(
				PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
				"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", byteAmount, pageSize, lastErrorNumber, lastErrorMessage);
			if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				omrtty_printf("\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				omrtty_printf("\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		} else {
			omrtty_printf("\treserved 0x%zx bytes\n", byteAmount);
			totalAlloc = byteAmount + totalAlloc;
			omrtty_printf("\ttotal: 0x%zx bytes\n", totalAlloc);
		}

		/* write to the memory */
		memset(memPtr, 'c', byteAmount);

		/* decommit the memory */
		rc = omrvmem_decommit_memory(memPtr, byteAmount, &vmemID[i]);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n", rc, byteAmount, pageSize);
			goto exit;
		}

	}

	deltaMillis = (omrtime_nano_time() - startTimeNanos) / (1000 * 1000);

	for (i = 0; i < NUM_VMEM_IDS; i++) {
		rc = omrvmem_free_memory(memPointers[i], byteAmount, &vmemID[i]);

		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n", rc, pageSize, memPointers[i]);
			goto exit;
		}
	}

	omrtty_printf("%s pageSize 0x%zx, byteAmount 0x%zx, millis for %u iterations: [reserve/write_to_all/decommit] test: %lli\n", disclaim ? "disclaim" : "nodisclaim", pageSize, byteAmount, NUM_VMEM_IDS, deltaMillis);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}



/**
 * Verify that we don't get any errors decomitting memory
 *
 * Get all the page sizes and make sure we can allocate a memory chunk for each page size
 *
 * After reserving and committing the memory, decommit the memory before freeing it.
 */
TEST(PortVmemTest, vmem_decommit_memory_test)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_decommit_memory_test";
	char *memPtr = NULL;
	uintptr_t *pageSizes;
#if defined(J9ZOS390)
	uintptr_t *pageFlags;
#endif /* J9ZOS390 */
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	char allocName[allocNameSize];
	intptr_t rc;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* First get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
#if defined(J9ZOS390)
	pageFlags =
#endif /* J9ZOS390 */
		omrvmem_supported_page_flags();

	/* reserve, commit, decommit, and memory for each page size */
	for (i = 0 ; pageSizes[i] != 0 ; i++) {

		/* reserve and commit */
#if defined(J9ZOS390)
		/* On z/OS skip this test for newly added large pages as obsolete omrvmem_reserve_memory() does not support them */
		if (TRUE == isNewPageSize(pageSizes[i], pageFlags[i])) {
			continue;
		}
#endif /* J9ZOS390 */
		memPtr = (char *)omrvmem_reserve_memory(
						0, pageSizes[i], &vmemID,
						OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT,
						pageSizes[i], OMRMEM_CATEGORY_PORT_LIBRARY);

		/* did we get any memory? */
		if (memPtr == NULL) {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(
				PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
				"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", pageSizes[i], pageSizes[i], lastErrorNumber, lastErrorMessage);
			if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
				outputComment(OMRPORTLIB, "\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
				outputComment(OMRPORTLIB, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
			}
			goto exit;
		} else {
			outputComment(OMRPORTLIB, "\treserved and committed 0x%zx bytes with page size 0x%zx\n", pageSizes[i], vmemID.pageSize);
		}

		/* can we read and write to the memory? */
		omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", pageSizes[i]);
		verifyMemory(OMRPORTLIB, testName, memPtr, pageSizes[i], allocName);

		/* decommit the memory */
		rc = omrvmem_decommit_memory(memPtr, pageSizes[i], &vmemID);
		if (rc != 0) {
			outputErrorMessage(
				PORTTEST_ERROR_ARGS,
				"omrvmem_decommit_memory returned 0x%zx when trying to decommit 0x%zx bytes backed by 0x%zx-byte pages\n",
				rc, pageSizes[i], pageSizes[i]);
			goto exit;
		}

		/* free the memory (reuse the vmemID) */
		rc = omrvmem_free_memory(memPtr, pageSizes[i], &vmemID);
		if (rc != 0) {
			outputErrorMessage(
				PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n",
				rc, pageSizes[i], memPtr);
			goto exit;
		}
	}

exit:

	reportTestExit(OMRPORTLIB, testName);
}

#if defined(ENABLE_RESERVE_MEMORY_EX_TESTS)

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryEx)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, FALSE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryEx", FALSE, FALSE, FALSE, FALSE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryEx\n");
	}
}

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryExTopDown)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, FALSE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryExTopDown", TRUE, FALSE, FALSE, FALSE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryExTopDown\n");
	}
}

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryExStrictAdress)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, TRUE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryExStrictAddress", FALSE, TRUE, FALSE, FALSE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryExStrictAddress\n");
	}
}

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryExStrictPageSize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, FALSE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryExStrictPageSize", FALSE, FALSE, TRUE, FALSE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryExStrictPageSize\n");
	}
}





/**********/

#if defined(J9ZOS39064)
/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryEx_use2To32)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, FALSE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryEx_use2To32", FALSE, FALSE, FALSE, TRUE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryEx\n");
	}
}

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryExTopDown_use2To32)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, FALSE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryExTopDown_use2To32", TRUE, FALSE, FALSE, TRUE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryExTopDown\n");
	}
}

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryExStrictAddress_use2To32)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, TRUE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryExStrictAddress_use2To32", FALSE, TRUE, FALSE, TRUE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryExStrictAddress\n");
	}
}

/**
 * See @ref omrvmem_testReserveMemoryEx_impl
 */
TEST(PortVmemTest, vmem_testReserveMemoryExStrictPageSize_use2To32)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	if (memoryIsAvailable(OMRPORTLIB, FALSE)) {
		omrvmem_testReserveMemoryEx_StandardAndQuickMode(OMRPORTLIB, "omrvmem_testReserveMemoryExStrictPageSize_use2To32", FALSE, FALSE, TRUE, TRUE);
	} else {
		outputComment(OMRPORTLIB, "***Did not find enough memory available on system, not running test omrvmem_testReserveMemoryExStrictPageSize\n");
	}
}
#endif /* defined(J9ZOS39064) */



/**
 * Get all the page sizes and make sure we can allocate a memory chunk
 * within a certain range of addresses for each page size
 * using standard mode for most OS. For Linux, also test the quick mode
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test
 * @param[in] topDown Bit indicating whether or not to allocate topDown
 * @param[in] strictAddress Bit indicating whether or not to set the OMRPORT_VMEM_STRICT_ADDRESS flag
 * @param[in] strictPageSize Bit indicating whether or not to set the OMRPORT_VMEM_STRICT_PAGE_SIZE flag
 * @param[in] use2To32G Bit indicating whether or not to allocate memory in 2 GB to 32 GB range
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
omrvmem_testReserveMemoryEx_StandardAndQuickMode(struct OMRPortLibrary *portLibrary, const char *testName, BOOLEAN topDown, BOOLEAN strictAddress, BOOLEAN strictPageSize, BOOLEAN use2To32G)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	int rc = omrvmem_testReserveMemoryEx_impl(OMRPORTLIB, testName, topDown, FALSE, strictAddress, strictPageSize, use2To32G);
#if defined(LINUX) || defined(OSX)
	if (TEST_PASS == rc) {
		rc |= omrvmem_testReserveMemoryEx_impl(OMRPORTLIB, testName, topDown, TRUE, strictAddress, strictPageSize, use2To32G);
	}
#endif /* defined(LINUX) || defined(OSX) */
	return rc;
}


/**
 * Get all the page sizes and make sure we can allocate a memory chunk
 * within a certain range of addresses for each page size
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test
 * @param[in] topDown Bit indicating whether or not to allocate topDown
 * @param[in] quickSearch Bit indicating whether or not to use quick scan: OMRPORT_VMEM_ALLOC_QUICK
 * @param[in] strictAddress Bit indicating whether or not to set the OMRPORT_VMEM_STRICT_ADDRESS flag
 * @param[in] strictPageSize Bit indicating whether or not to set the OMRPORT_VMEM_STRICT_PAGE_SIZE flag
 * @param[in] use2To32G Bit indicating whether or not to allocate memory in 2 GB to 32 GB range
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int
omrvmem_testReserveMemoryEx_impl(struct OMRPortLibrary *portLibrary, const char *testName, BOOLEAN topDown, BOOLEAN quickSearch, BOOLEAN strictAddress, BOOLEAN strictPageSize, BOOLEAN use2To32G)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
#define NUM_SEGMENTS 3
	char *memPtr[NUM_SEGMENTS];
	uintptr_t *pageSizes;
	uintptr_t *pageFlags;
	int i = 0;
	int j = 0;
	struct J9PortVmemIdentifier vmemID[NUM_SEGMENTS];
	char allocName[allocNameSize];
	int32_t rc;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	J9PortVmemParams params[NUM_SEGMENTS];


	reportTestEntry(OMRPORTLIB, testName);

	/* Get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0 ; pageSizes[i] != 0 ; i++) {
		for (j = 0; j < NUM_SEGMENTS; j++) {
			uintptr_t initialBlocks;
			uintptr_t initialBytes;

			memPtr[j] = NULL;

			/* Sample baseline category data */
			getPortLibraryMemoryCategoryData(OMRPORTLIB, &initialBlocks, &initialBytes);

			omrvmem_vmem_params_init(&params[j]);
			if (TRUE == use2To32G) {
				params[j].startAddress = (void *)TWO_GB;
				params[j].endAddress = (void *)(SIXTY_FOUR_GB - 1);
			} else {
				params[j].startAddress = (void *)(pageSizes[i] * 2);
#if defined(LINUX) || defined(OSX)
				params[j].endAddress = (void *)(pageSizes[i] * 100);
#elif defined(WIN64)
				params[j].endAddress = (void *)0x7FFFFFFFFFF;
#elif defined(OMR_ENV_DATA64)
				params[j].endAddress = (void *)0xffffffffffffffff;
#else /* defined(OMR_ENV_DATA64) */
				params[j].endAddress = (void *)TWO_GIG_BAR;
#endif /* defined(OMR_ENV_DATA64) */
			}
			params[j].byteAmount = pageSizes[i];
			params[j].mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
			params[j].pageSize = pageSizes[i];
			params[j].pageFlags = pageFlags[i];
			params[j].category = OMRMEM_CATEGORY_PORT_LIBRARY;

			if (topDown) {
				params[j].options |= OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN;
			}

			if (quickSearch) {
				params[j].options |= OMRPORT_VMEM_ALLOC_QUICK;
			}

			if (strictAddress) {
				params[j].options |= OMRPORT_VMEM_STRICT_ADDRESS;
			}

			if (strictPageSize) {
				params[j].options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;
			}

			if (use2To32G) {
				params[j].options |= OMRPORT_VMEM_ZOS_USE2TO32G_AREA;
			}

			outputComment(OMRPORTLIB, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
						  "topDown: %s quickSearch: %s strictAddress: %s strictPageSize: %s use2To32G: %s\n", \
						  params[j].pageSize, params[j].startAddress, params[j].endAddress, \
						  (0 != (params[j].options & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
						  (0 != (params[j].options & OMRPORT_VMEM_ALLOC_QUICK)) ? "true" : "false",
						  (0 != (params[j].options & OMRPORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
						  (0 != (params[j].options & OMRPORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
						  (0 != (params[j].options & OMRPORT_VMEM_ZOS_USE2TO32G_AREA)) ? "true" : "false");

			/* reserve and commit */
			memPtr[j] = (char *)omrvmem_reserve_memory_ex(&vmemID[j], &params[j]);

			/* did we get any memory and is it in range if strict is set? */
			if (memPtr[j] == NULL) {
				if (strictPageSize) {
					outputComment(OMRPORTLIB, "\t! Unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n", pageSizes[i], pageSizes[i]);
				} else {
					lastErrorMessage = (char *)omrerror_last_error_message();
					lastErrorNumber = omrerror_last_error_number();
					outputErrorMessage(
						PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx.\n"
						"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", pageSizes[i], pageSizes[i], lastErrorNumber, lastErrorMessage);

					if (OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES == lastErrorNumber) {
						outputComment(OMRPORTLIB, "\tPortable error OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES...\n");
						outputComment(OMRPORTLIB, "\t\tREBOOT THE MACHINE to free up resources AND TRY THE TEST AGAIN\n");
					}
				}
			} else if (strictAddress && (((void *)memPtr[j] < params[j].startAddress) || ((void *)memPtr[j] > params[j].endAddress))) {
				/* if returned pointer is outside of range then fail */
				outputErrorMessage(
					PORTTEST_ERROR_ARGS, "Strict address flag set and returned pointer [0x%zx] is outside of range.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", memPtr[j], lastErrorNumber, lastErrorMessage);

			} else if (strictPageSize && (vmemID[j].pageSize != params[j].pageSize)) {
				/* fail if strict page size flag and returned memory does not have the requested page size */
				outputErrorMessage(
					PORTTEST_ERROR_ARGS, "Strict page size flag set and returned memory has a page size of [0x%zx] "
					"while a page size of [0x%zx] was requested.\n"
					"\tlastErrorNumber=%d, lastErrorMessage=%s\n ", vmemID[j].pageSize, params[j].pageSize, lastErrorNumber, lastErrorMessage);
#if defined(OMR_ENV_DATA64)
			} else if (use2To32G && strictAddress &&
					   (((uint64_t) memPtr[j] < TWO_GB) || ((uint64_t) memPtr[j] >= SIXTY_FOUR_GB))
			) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\t use2To32G flag is set and returned pointer [0x%zx] is outside of range.\n", memPtr[j]);
#endif /* OMR_ENV_DATA64 */
			} else {
				outputComment(
					OMRPORTLIB, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%zx at address 0x%zx\n", \
					pageSizes[i], vmemID[j].pageSize, vmemID[j].pageFlags, memPtr[j]);
			}

			/* perform memory checks */
			if (NULL != memPtr[j]) {
				uintptr_t finalBlocks;
				uintptr_t finalBytes;

				/* are the page sizes stored and reported correctly? */
				if (vmemID[j].pageSize != omrvmem_get_page_size(&(vmemID[j]))) {
					outputErrorMessage(
						PORTTEST_ERROR_ARGS, "vmemID[j].pageSize (0x%zx)!= omrvmem_get_page_size (0x%zx) .\n",
						vmemID[j].pageSize, omrvmem_get_page_size(vmemID));
				}

				/* are the page types stored and reported correctly? */
				if (vmemID[j].pageFlags != omrvmem_get_page_flags(&(vmemID[j]))) {
					outputErrorMessage(
						PORTTEST_ERROR_ARGS, "vmemID[j].pageFlags (0x%zx)!= omrvmem_get_page_flags (0x%zx) .\n",
						vmemID[j].pageFlags, omrvmem_get_page_flags(vmemID));
				}

				/* can we read and write to the memory? */
				omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", pageSizes[i]);
				verifyMemory(OMRPORTLIB, testName, memPtr[j], pageSizes[i], allocName);

				/* Have the memory categories been updated properly */
				getPortLibraryMemoryCategoryData(OMRPORTLIB, &finalBlocks, &finalBytes);

				if (finalBlocks <= initialBlocks) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category block as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
				}

				if (finalBytes < (initialBytes + pageSizes[i])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem reserve didn't increment category bytes as expected. Final bytes=%zu, initial bytes=%zu, page size=%zu.\n", finalBytes, initialBytes, pageSizes[i]);
				}
			}
		}

		/* free the memory */
		for (j = 0; j < NUM_SEGMENTS; j++) {
			if (NULL != memPtr[j]) {
				uintptr_t initialBlocks, initialBytes, finalBlocks, finalBytes;

				getPortLibraryMemoryCategoryData(OMRPORTLIB, &initialBlocks, &initialBytes);

				rc = omrvmem_free_memory(memPtr[j], pageSizes[i], &vmemID[j]);
				if (rc != 0) {
					outputErrorMessage(
						PORTTEST_ERROR_ARGS,
						"omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n",
						rc, pageSizes[i], memPtr[j]);
				}

				getPortLibraryMemoryCategoryData(OMRPORTLIB, &finalBlocks, &finalBytes);

				if (finalBlocks >= initialBlocks) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category blocks as expected. Final blocks=%zu, initial blocks=%zu, page size=%zu.\n", finalBlocks, initialBlocks, pageSizes[i]);
				}

				if (finalBytes > (initialBytes - pageSizes[i])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "vmem free didn't decrement category bytes as expected. Final bytes=%zu, initial bytes=%zu, page size=%zu.\n", finalBytes, initialBytes, pageSizes[i]);
				}
			}
		}

	}

#undef NUM_SEGMENTS

	return reportTestExit(OMRPORTLIB, testName);
}

/**
 * Tries to allocate and free same amount of memory that will be used by
 * reserve_memory_ex tests to see whether or not there is enough memory
 * on the system.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] strictAddress TRUE if OMRPORT_VMEM_STRICT_ADDRESS flag is to be used
 *
 * @return TRUE if memory is available, FALSE otherwise
 *
 */
static BOOLEAN
memoryIsAvailable(struct OMRPortLibrary *portLibrary, BOOLEAN strictAddress)
{
#define NUM_SEGMENTS 3
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	char *memPtr[NUM_SEGMENTS] = {(char *)NULL, (char *)NULL, (char *)NULL};
	uintptr_t *pageSizes;
	uintptr_t *pageFlags;
	int i = 0;
	int j = 0;
	struct J9PortVmemIdentifier vmemID[NUM_SEGMENTS];
	int32_t rc;
	J9PortVmemParams params[NUM_SEGMENTS];
	BOOLEAN isMemoryAvailable = TRUE;

	/* Get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0 ; pageSizes[i] != 0 ; i++) {
		for (j = 0; j < NUM_SEGMENTS; j++) {
			omrvmem_vmem_params_init(&params[j]);
			params[j].startAddress = (void *)(pageSizes[i] * 2);
#if defined(LINUX) || defined(OSX)
			params[j].endAddress = (void *)(pageSizes[i] * 100);
#elif defined(WIN64)
			params[j].endAddress = (void *) 0x7FFFFFFFFFF;
#endif /* defined(LINUX) || defined(OSX) */
			params[j].byteAmount = pageSizes[i];
			params[j].mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
			params[j].pageSize = pageSizes[i];
			params[j].pageFlags = pageFlags[i];

			if (strictAddress) {
				params[j].options |= OMRPORT_VMEM_STRICT_ADDRESS;
			}

			/* reserve and commit */
			memPtr[j] = (char *)omrvmem_reserve_memory_ex(&vmemID[j], &params[j]);

			/* did we get any memory */
			if (memPtr[j] == NULL) {
				outputComment(OMRPORTLIB, "\t**Could not find 0x%zx bytes available with page size 0x%zx\n", pageSizes[i], pageSizes[i]);
				isMemoryAvailable = FALSE;
				break;
			}
		}

		for (j = 0; j < NUM_SEGMENTS; j++) {
			/* free the memory (reuse the vmemID) */
			if (NULL != memPtr[j]) {
				rc = omrvmem_free_memory(memPtr[j], pageSizes[i], &vmemID[j]);
				if (rc != 0) {
					isMemoryAvailable = FALSE;
					outputComment(
						OMRPORTLIB, "\t**omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n",
						rc, pageSizes[i], memPtr);
				}
			}
		}

	}

#undef NUM_SEGMENTS

	return isMemoryAvailable;
}

#if defined(J9ZOS390)
/**
 * Test request for pages below the 2G bar on z/OS
 */
TEST(PortVmemTest, vmem_testReserveMemoryEx_zOSLargePageBelowBar)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	const char *testName = "omrvmem_testReserveMemoryEx_zOSLargePageBelowBar";
	int32_t i = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* Get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		char allocName[allocNameSize];
		void *memPtr = NULL;
		int32_t rc = 0;

		omrvmem_vmem_params_init(&params);

		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = 0;

		outputComment(OMRPORTLIB, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					  "topDown: %s strictAddress: %s strictPageSize: %s use2To32G: %s\n", \
					  params.pageSize, params.startAddress, params.endAddress, \
					  (0 != (params.options & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_ZOS_USE2TO32G_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = omrvmem_reserve_memory_ex(&vmemID, &params);

		/* did we get any memory? */
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx, page type 0x%zx.\n", \
							   params.byteAmount, pageSizes[i], pageFlags[i]);

			goto exit;

		} else {
			outputComment(OMRPORTLIB, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%x at address 0x%zx\n", \
						  params.byteAmount, vmemID.pageSize, vmemID.pageFlags, memPtr);
		}

		/* can we read and write to the memory? */
		omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", params.byteAmount);
		verifyMemory(OMRPORTLIB, testName, (char *)memPtr, params.byteAmount, allocName);

		/* free the memory */
		rc = omrvmem_free_memory(memPtr, params.byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned 0x%zx when trying to free 0x%zx bytes\n",
							   rc, params.byteAmount);
			goto exit;
		}
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test request for pages below the 2G bar with OMRPORT_VMEM_STRICT_ADDRESS set on z/OS
 */
TEST(PortVmemTest, vmem_testReserveMemoryExStrictAddress_zOSLargePageBelowBar)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	const char *testName = "omrvmem_testReserveMemoryExStrictAddress_zOSLargePageBelowBar";
	int32_t i = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* Get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		char allocName[allocNameSize];
		void *memPtr = NULL;
		int32_t rc = 0;

		if (TWO_GB <= pageSizes[i]) {
			/* On z/OS "below bar" range spans from 0-2GB.
			 * If the page size is >= 2G, then attempt to allocate memory with OMRPORT_VMEM_STRICT_ADDRESS flag is expected to fail.
			 */
			continue;
		}

		omrvmem_vmem_params_init(&params);

		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = OMRPORT_VMEM_STRICT_ADDRESS;

		outputComment(OMRPORTLIB, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					  "topDown: %s strictAddress: %s strictPageSize: %s use2To32G: %s\n", \
					  params.pageSize, params.startAddress, params.endAddress, \
					  (0 != (params.options & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_ZOS_USE2TO32G_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = omrvmem_reserve_memory_ex(&vmemID, &params);

		/* did we get any memory? */
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx, page type 0x%zx.\n", \
							   params.byteAmount, pageSizes[i], pageFlags[i]);
			goto exit;

		} else {
			outputComment(OMRPORTLIB, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%x at address 0x%zx\n", \
						  params.byteAmount, vmemID.pageSize, vmemID.pageFlags, memPtr);

			if ((uint64_t) memPtr >= TWO_GIG_BAR) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\t returned memory: %p not less than 2G\n", memPtr);
				/* Don't exit here, need to free the allocate memory */
			}
			/* When using page size that can allocate memory only above 2G bar,
			 * omrvmem_reserve_memory_ex should fail to reserve memory below 2G using strict address
			 * but should fall back to 4K page size.
			 */
			if (FALSE == isPageSizeSupportedBelowBar(pageSizes[i], pageFlags[i])) {
				if (FOUR_KB != vmemID.pageSize) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\t didn't expect to allocate memory below 2GB using page size 0x%zx and page flags 0x%x with strict address\n", \
									   vmemID.pageSize, vmemID.pageFlags);
					goto exit;
				}
			}
		}

		/* can we read and write to the memory? */
		omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", params.byteAmount);
		verifyMemory(OMRPORTLIB, testName, (char *)memPtr, params.byteAmount, allocName);

		/* free the memory */
		rc = omrvmem_free_memory(memPtr, params.byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(
				PORTTEST_ERROR_ARGS,
				"omrvmem_free_memory returned 0x%zx when trying to free 0x%zx bytes\n",
				rc, params.byteAmount);
			goto exit;
		}

	}

exit:
	reportTestExit(OMRPORTLIB, testName);

}

#if defined(OMR_ENV_DATA64)
/**
 * Test request for pages below the 2G bar with OMRPORT_VMEM_ZOS_USE2TO32G_AREA set on z/OS.
 * In this case, since STRICT_ADDRESS flag is not set, USE2TO32G flag is given priority over
 * the requested address range. Therefore, memory is expected to be allocated between 2G-32G.
 */
TEST(PortVmemTest, vmem_testReserveMemoryEx_use2To32_zOSLargePageBelowBar)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	const char *testName = "omrvmem_testReserveMemoryEx_use2To32_zOSLargePageBelowBar";
	int32_t i = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* Get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		char allocName[allocNameSize];
		void *memPtr = NULL;
		int32_t rc = 0;

		omrvmem_vmem_params_init(&params);

		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = OMRPORT_VMEM_ZOS_USE2TO32G_AREA;

		outputComment(OMRPORTLIB, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					  "topDown: %s strictAddress: %s strictPageSize: %s use2To32G: %s\n", \
					  params.pageSize, params.startAddress, params.endAddress, \
					  (0 != (params.options & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_ZOS_USE2TO32G_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = omrvmem_reserve_memory_ex(&vmemID, &params);

		/* did we get any memory? */
		if (NULL == memPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "unable to reserve and commit 0x%zx bytes with page size 0x%zx, page type 0x%zx.\n", \
							   params.byteAmount, pageSizes[i], pageFlags[i]);

			goto exit;

		} else {
			outputComment(OMRPORTLIB, "\t reserved and committed 0x%zx bytes with page size 0x%zx and page flag 0x%x at address 0x%zx\n", \
						  params.byteAmount, vmemID.pageSize, vmemID.pageFlags, memPtr);

			if (((uint64_t) memPtr < TWO_GB) ||
				((uint64_t) memPtr >= SIXTY_FOUR_GB)
			) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\t returned memory: %p is not in 2G-32G range\n", memPtr);
				goto exit;
			}
		}

		/* can we read and write to the memory? */
		omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", params.byteAmount);
		verifyMemory(OMRPORTLIB, testName, (char *)memPtr, params.byteAmount, allocName);

		/* free the memory */
		rc = omrvmem_free_memory(memPtr, params.byteAmount, &vmemID);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned 0x%zx when trying to free 0x%zx bytes\n",
							   rc, params.byteAmount);
			goto exit;
		}
	}

exit:
	reportTestExit(OMRPORTLIB, testName);

}

/**
 * Test request for pages below the 2G bar with OMRPORT_VMEM_STRICT_ADDRESS and OMRPORT_VMEM_ZOS_USE2TO32G_ARE set on z/OS
 */
TEST(PortVmemTest, vmem_testReserveMemoryExStrictAddress_use2To32_zOSLargePageBelowBar)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	const char *testName = "omrvmem_testReserveMemoryExStrictAddress_use2To32_zOSLargePageBelowBar";
	int32_t i = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* Get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0; pageSizes[i] != 0 ; i++) {
		struct J9PortVmemIdentifier vmemID;
		J9PortVmemParams params;
		void *memPtr = NULL;

		omrvmem_vmem_params_init(&params);

		/* request memory (at or) below the 2G bar */
		params.startAddress = 0;
		params.endAddress = (void *) TWO_GIG_BAR;
		params.byteAmount = pageSizes[i];
		params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];
		params.options = OMRPORT_VMEM_STRICT_ADDRESS | OMRPORT_VMEM_ZOS_USE2TO32G_AREA;

		outputComment(OMRPORTLIB, "\tPage Size: 0x%zx Range: [0x%zx,0x%zx] "\
					  "topDown: %s strictAddress: %s strictPageSize: %s use2To32G: %s\n", \
					  params.pageSize, params.startAddress, params.endAddress, \
					  (0 != (params.options & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_ADDRESS)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_STRICT_PAGE_SIZE)) ? "true" : "false",
					  (0 != (params.options & OMRPORT_VMEM_ZOS_USE2TO32G_AREA)) ? "true" : "false");

		/* reserve and commit */
		memPtr = omrvmem_reserve_memory_ex(&vmemID, &params);

		/* OMRPORT_VMEM_ZOS_USE2TO32G_AREA allocates memory in 2-32G but the requested address is below 2G with STRICT_ADDRESS set
		 * The two conditions are incompatible and should return NULL.
		 */
		if (memPtr != NULL) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Did not expect to allocate memory using page size 0x%zx, page type 0x%zx in < 2GB range with OMRPORT_VMEM_ZOS_USE2TO32G_AREA and OMRPORT_VMEM_STRICT_ADDRESS .\n", \
							   pageSizes[i], pageFlags[i]);

			omrvmem_free_memory(memPtr, params.byteAmount, &vmemID);

		} else {
			outputComment(OMRPORTLIB, "\t Expected to fail to allocate large pages in < 2GB range with OMRPORT_VMEM_ZOS_USE2TO32G_AREA and OMRPORT_VMEM_STRICT_ADDRESS set\n");
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* defined(OMR_ENV_DATA64) */
#endif /* defined(J9ZOS390) */

#endif /* ENABLE_RESERVE_MEMORY_EX_TESTS */

#if !defined(J9ZOS390)
/**
 * Verify port library memory management.
 *
 * Make sure there is at least one page size returned by omrvmem_get_supported_page_sizes
 */
TEST(PortVmemTest, vmem_test_commitOutsideOfReservedRange)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_test_commitOutsideOfReservedRange";
	char *memPtr = NULL;
	uintptr_t *pageSizes;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	J9PortVmemParams params;

	reportTestEntry(OMRPORTLIB, testName);

	pageSizes = omrvmem_supported_page_sizes();

	/* reserve and commit memory for each page size */
	for (i = 0 ; pageSizes[i] != 0 ; i++) {
		omrvmem_vmem_params_init(&params);
		params.byteAmount = pageSizes[i];
		params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE;
		params.options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;
		params.pageSize = pageSizes[i];

		/* reserve */
		memPtr = (char *)omrvmem_reserve_memory_ex(&vmemID, &params);

		if (NULL == memPtr) {
			outputComment(OMRPORTLIB, "! Could not find 0x%zx bytes available with page size 0x%zx\n", params.byteAmount, pageSizes[i]);
		} else {
			intptr_t rc;
			void *commitResult;

			/* attempt to commit 1 byte more than we reserved */
			commitResult = omrvmem_commit_memory(memPtr, params.byteAmount + 1, &vmemID);

			if (NULL != commitResult) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_commit_memory did not return an error when attempting to commit more memory"
								   "than was reserved. \n");
			} else {
				commitResult = omrvmem_commit_memory(memPtr, params.byteAmount, &vmemID);

				if (NULL == commitResult) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_commit_memory returned an error while attempting to commit reserved memory: %s\n", omrerror_last_error_message());
				} else {
					/* attempt to decommit 1 byte more than we reserved */
					rc = omrvmem_decommit_memory(memPtr, params.byteAmount + 1, &vmemID);

					if (0 == rc) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_decommit_memory did not return an error when attempting to decommit "
										   "more memory than was reserved. \n");
					} else {
						rc = omrvmem_decommit_memory(memPtr, params.byteAmount, &vmemID);

						if (rc != 0) {
							outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_decommit_memory returned an error when attempting to decommit "
											   "reserved memory, rc=%zd\n", rc);
						}
					}
				}
			}

			rc = omrvmem_free_memory(memPtr, params.byteAmount, &vmemID);
			if (rc != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned %d when trying to free 0x%zx bytes backed by 0x%zx-byte pages\n"
								   , rc, params.byteAmount, pageSizes[i]);
			}
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* !defined(J9ZOS390) */

#if defined(ENABLE_RESERVE_MEMORY_EX_TESTS)
/**
 * Verify port library memory management.
 *
 * Ensure that we can execute code within memory returned by
 * omrvmem_reserve_memory_ex with the J0PORT_VMEM_MEMORY_MODE_EXECUTE
 * mode flag specified.
 */
TEST(PortVmemTest, vmem_test_reserveExecutableMemory)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_test_reserveExecutableMemory";
	void *memPtr = NULL;
	uintptr_t *pageSizes;
	uintptr_t *pageFlags;
	int i = 0;
	struct J9PortVmemIdentifier vmemID;
	J9PortVmemParams params;

	reportTestEntry(OMRPORTLIB, testName);

	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* reserve and commit memory for each page size */
	for (i = 0 ; pageSizes[i] != 0 ; i++) {
		omrvmem_vmem_params_init(&params);
		params.byteAmount = pageSizes[i];
		params.mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE |
					   OMRPORT_VMEM_MEMORY_MODE_EXECUTE | OMRPORT_VMEM_MEMORY_MODE_COMMIT;
		params.options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;
		params.pageSize = pageSizes[i];
		params.pageFlags = pageFlags[i];

		/* reserve */
		memPtr = omrvmem_reserve_memory_ex(&vmemID, &params);

		if (NULL == memPtr) {
			outputComment(OMRPORTLIB, "! Could not find 0x%zx bytes available with page size 0x%zx and specified mode\n", params.byteAmount, pageSizes[i]);
		} else {
			intptr_t rc;
			void (*function)();

#if defined(AIXPPC)

			*((unsigned int *)memPtr) = (unsigned int)0x4e800020; /* blr instruction (equivalent to RET on x86)*/

			struct fDesc {
				uintptr_t func_addr;
				intptr_t toc;
				intptr_t envp;
			} myDesc;
			myDesc.func_addr = (uintptr_t)memPtr;

			function = (void (*)())(void *)&myDesc;
			outputComment(OMRPORTLIB, "About to call dynamically created function, pageSize=0x%zx...\n", params.pageSize);
			function();
			outputComment(OMRPORTLIB, "Dynamically created function returned successfully\n");

#elif defined(LINUX) || defined(WIN32) || defined(J9ZOS390) || defined(OSX)
			size_t length;

#if defined(J9ZOS39064)
			/* On 64-bit z/OS, only 4K and pageable 1M large pages that allocate memory below 2G bar
			 * can be used as executable pages. Other page sizes do not support executable pages.
			 */
			if (FALSE == isPageSizeSupportedBelowBar(pageSizes[i], pageFlags[i])) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Did not expect to allocate executable pages with large page size 0x%zx, page type 0x%zx\n", \
								   pageSizes[i], pageFlags[i]);
			} else {
#endif /* J9ZOS39064 */

				memset(memPtr, 0, params.pageSize);

				if ((uintptr_t)&myFunction2 > (uintptr_t)&myFunction1) {
					length = (size_t)((uintptr_t)&myFunction2 - (uintptr_t)&myFunction1);
				} else {
					length = (size_t)((uintptr_t)&myFunction1 - (uintptr_t)&myFunction2);
				}
				/* Occasionally, subtracing the addresses gives a weird result. Cap length to avoid corruption. */
				if (pageSizes[i] < length) {
					length = pageSizes[i];
				}

				outputComment(OMRPORTLIB, "function length = %d\n", length);

				memcpy(memPtr, (void *)&myFunction1, length);

				outputComment(OMRPORTLIB, "*memPtr: 0x%zx\n", *((unsigned int *)memPtr));

				function = (void (*)())memPtr;
				outputComment(OMRPORTLIB, "About to call dynamically created function, pageSize=0x%zx...\n", params.pageSize);
				function();
				outputComment(OMRPORTLIB, "Dynamically created function returned successfully\n");
#if defined(J9ZOS39064)
			}
#endif /* J9ZOS39064 */

#endif /* defined(AIXPPC) */

			rc = omrvmem_free_memory(memPtr, params.byteAmount, &vmemID);
			if (rc != 0) {
				outputErrorMessage(
					PORTTEST_ERROR_ARGS,
					"omrvmem_free_memory returned %i when trying to free 0x%zx bytes at 0x%zx\n",
					rc, params.byteAmount, memPtr);
			}
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* defined(ENABLE_RESERVE_MEMORY_EX_TESTS) */

#if defined(OMR_GC_VLHGC)
/**
 * Try to associate memory with each NUMA node at each page size.
 *
 * If NUMA is not available, this test does nothing.
 *
 * This test cannot test that the NUMA affinity is correctly set, just that the API succeeds.
 */
TEST(PortVmemTest, vmem_test_numa)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrvmem_test_numa";
	uintptr_t *pageSizes;
	uintptr_t *pageFlags;
	uintptr_t totalNumaNodeCount = 0;
	intptr_t detailReturnCode = 0;

	reportTestEntry(OMRPORTLIB, testName);

	/* First get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	/* Find out how many NUMA nodes are available */
	detailReturnCode = omrvmem_numa_get_node_details(NULL, &totalNumaNodeCount);

	if ((detailReturnCode != 0) || (0 == totalNumaNodeCount)) {
		outputComment(OMRPORTLIB, "\tNUMA not available\n");
	} else {
		uintptr_t i = 0;
		uintptr_t nodesWithMemoryCount = 0;
		uintptr_t originalNodeCount = totalNumaNodeCount;
		J9MemoryState policyForMemoryNode = J9NUMA_PREFERRED;
		uintptr_t nodeArraySize = sizeof(J9MemoryNodeDetail) * totalNumaNodeCount;
		J9MemoryNodeDetail *nodeArray = (J9MemoryNodeDetail *)omrmem_allocate_memory(nodeArraySize, OMRMEM_CATEGORY_PORT_LIBRARY);
		intptr_t rc = 0;

		if (nodeArray == NULL) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrvmem_free_memory returned NULL when trying to allocate J9MemoryNodeDetail array for NUMA test (FAIL)\n");
			goto exit;
		}
		memset(nodeArray, 0x0, nodeArraySize);
		rc = omrvmem_numa_get_node_details(nodeArray, &totalNumaNodeCount);
		if ((detailReturnCode != rc) || (totalNumaNodeCount != originalNodeCount)) {
			outputErrorMessage(
				PORTTEST_ERROR_ARGS,
				"omrvmem_numa_get_node_details returned different results on second call (return %zd versus %zd, count %zu versus %zu)\n",
				rc, detailReturnCode, totalNumaNodeCount, originalNodeCount);
			goto exit;
		}
		/* count the number of nodes with physical memory */
		{
			uintptr_t activeNodeIndex = 0;
			uintptr_t preferred = 0;
			uintptr_t allowed = 0;
			for (activeNodeIndex = 0; activeNodeIndex < totalNumaNodeCount; activeNodeIndex++) {
				J9MemoryState policy = nodeArray[activeNodeIndex].memoryPolicy;
				if (J9NUMA_PREFERRED == policy) {
					preferred += 1;
				} else if (J9NUMA_ALLOWED == policy) {
					allowed += 1;
				}
			}

			nodesWithMemoryCount = preferred;
			policyForMemoryNode = J9NUMA_PREFERRED;
			if (nodesWithMemoryCount == 0) {
				nodesWithMemoryCount = allowed;
				policyForMemoryNode = J9NUMA_ALLOWED;
			}
		}
		if (0 == nodesWithMemoryCount) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Found zero nodes with memory even after NUMA was reported as supported (FAIL)\n");
			goto exit;
		}
		/* reserve and commit memory for each page size */
		for (i = 0 ; pageSizes[i] != 0 ; i++) {
			struct J9PortVmemIdentifier vmemID;
			char *memPtr = NULL;
			uintptr_t pageSize = pageSizes[i];
			uintptr_t reserveSizeInBytes = pageSize * nodesWithMemoryCount;

			/* reserve and commit */
#if defined(J9ZOS390)
			/* On z/OS skip this test for newly added large pages as obsolete omrvmem_reserve_memory() does not support them */
			if (TRUE == isNewPageSize(pageSizes[i], pageFlags[i])) {
				continue;
			}
#endif /* J9ZOS390 */
			memPtr = (char *)omrvmem_reserve_memory(
						 NULL,
						 reserveSizeInBytes,
						 &vmemID,
						 OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE,
						 pageSize, OMRMEM_CATEGORY_PORT_LIBRARY);

			outputComment(OMRPORTLIB, "\treserved 0x%zx bytes with page size 0x%zx at address 0x%zx\n", reserveSizeInBytes, vmemID.pageSize, memPtr);

			/* did we get any memory? */
			if (memPtr != NULL) {
				char allocName[allocNameSize];
				uintptr_t nodeIndex = 0;
				char *numaBlock = memPtr;
				void *commitResult = NULL;

				for (nodeIndex = 0; nodeIndex < totalNumaNodeCount; nodeIndex++) {
					if (policyForMemoryNode == nodeArray[nodeIndex].memoryPolicy) {
						uintptr_t nodeNumber = nodeArray[nodeIndex].j9NodeNumber;
						rc = omrvmem_numa_set_affinity(nodeNumber, numaBlock, pageSize, &vmemID);
						if (rc != 0) {
							outputErrorMessage(
								PORTTEST_ERROR_ARGS,
								"omrvmem_numa_set_affinity returned 0x%zx when trying to set affinity for 0x%zx bytes at 0x%p to node %zu\n",
								rc, pageSize, numaBlock, nodeNumber);
							goto exit;
						}
						numaBlock += pageSize;
					}
				}

				/* can we commit the memory? */
				commitResult = omrvmem_commit_memory(memPtr, numaBlock - memPtr, &vmemID);
				if (commitResult != memPtr) {
					outputErrorMessage(
						PORTTEST_ERROR_ARGS,
						"omrvmem_commit_memory returned 0x%p when trying to commit 0x%zx bytes at 0x%p\n",
						commitResult, numaBlock - memPtr, memPtr);
					goto exit;
				}

				/* ideally we'd test that the memory actually has some NUMA characteristics, but that's difficult to prove */

				/* can we read and write to the memory? */
				omrstr_printf(allocName, allocNameSize, "omrvmem_reserve_memory(%d)", pageSize);
				verifyMemory(OMRPORTLIB, testName, memPtr, reserveSizeInBytes, allocName);

				/* free the memory (reuse the vmemID) */
				rc = omrvmem_free_memory(memPtr, reserveSizeInBytes, &vmemID);
				if (rc != 0) {
					outputErrorMessage(
						PORTTEST_ERROR_ARGS,
						"omrvmem_free_memory returned 0x%zx when trying to free 0x%zx bytes backed by 0x%zx-byte pages\n",
						rc, reserveSizeInBytes, pageSize);
					goto exit;
				}
			}
		}
	}

exit:

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* defined(OMR_GC_VLHGC) */

#define PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, pageSize, pageFlags) \
	outputComment(OMRPORTLIB, "Input > mode: %zu, requestedPageSize: 0x%zx, requestedPageFlags: 0x%zx\n", mode, pageSize, pageFlags); \
 
void
verifyFindValidPageSizeOutput(struct OMRPortLibrary *portLibrary,
						      const char *testName,
						      uintptr_t pageSizeExpected,
						      uintptr_t pageFlagsExpected,
						      uintptr_t isSizeSupportedExpected,
						      uintptr_t pageSizeReturned,
						      uintptr_t pageFlagsReturned,
						      uintptr_t isSizeSupportedReturned)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	uintptr_t result = FALSE;

	outputComment(OMRPORTLIB, "Expected > expectedPageSize: 0x%zx, expectedPageFlags: 0x%zx, isSizeSupported: 0x%zx\n\n",
				  pageSizeExpected, pageFlagsExpected, isSizeSupportedExpected);

	if ((pageSizeExpected == pageSizeReturned) && (pageFlagsExpected == pageFlagsReturned)) {
		if (isSizeSupportedExpected != isSizeSupportedReturned) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "isSizeSupported should be: 0x%zx but found: 0x%zx\n",
							   isSizeSupportedExpected, isSizeSupportedReturned);
		} else {
			result = TRUE;
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected omrvmem_find_valid_page_size() to return "
						   "pageSize: 0x%zx, pageFlags: 0x%zx, but found pageSize: 0x%zx, pageFlags: 0x%zx\n",
						   pageSizeExpected, pageFlagsExpected, pageSizeReturned, pageFlagsReturned);
	}

	if (TRUE == result) {
		outputComment(OMRPORTLIB, "PASSED\n");
	}
}

#if (defined(LINUX) && !defined(LINUXPPC)) || defined(WIN32) || defined(OSX)

static int
omrvmem_testFindValidPageSize_impl(struct OMRPortLibrary *portLibrary, const char *testName)
{
#define MODE_NOT_USED 0

	uintptr_t requestedPageSize = 0;
	uintptr_t requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSizeSupported = FALSE;

	uintptr_t expectedPageSize = 0;
	uintptr_t expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN expectedIsSizeSupported = FALSE;

	uintptr_t defaultLargePageSize = 0;
	uintptr_t defaultLargePageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	intptr_t caseIndex = 1;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	outputComment(OMRPORTLIB, "\n");
	reportTestEntry(OMRPORTLIB, testName);

	/* Linux and Windows support only one large page size
	 * which is also the default large page size at index 1 of PPG_vmem_pageSize.
	 * Moreover, since large page size supported by Linux and Windows is not fixed,
	 * we assume it to be 4M for testing purpose.
	 */
	omrvmem_default_large_page_size_ex(0, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		outputComment(OMRPORTLIB, "defaultLargePageSize: 0x%zx, defaultLargePageFlags: 0x%zx\n", defaultLargePageSize, defaultLargePageFlags);
	}

	/* Note that -Xlp<size>, -Xlp:objectheap and -Xlp:codecache are treated same, and
	 * callers always pass OMRPORT_VMEM_PAGE_FLAG_NOT_USED as the requestedPageFlags
	 * to omrvmem_find_valid_page_size().
	 * Only difference is in case of -Xlp<size>, omrvmem_find_valid_page_size() is not called
	 * if default large page size is not present.
	 */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4M\n", caseIndex++);

	requestedPageSize = 4 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K\n", caseIndex++);

	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not in 4K, 4M>\n", caseIndex++);

	/* Add any new cases before this line */
	outputComment(OMRPORTLIB, "%d tests completed\n", caseIndex);

	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	return reportTestExit(OMRPORTLIB, testName);
}

TEST(PortVmemTest, vmem_testFindValidPageSize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

#if defined(LINUX) || defined(OSX)
#define OMRPORT_VMEM_PAGESIZE_COUNT 5	/* This should be same as defined in port/unix_include/j9portpg.h */
#elif defined(WIN32)
#define OMRPORT_VMEM_PAGESIZE_COUNT 3	/* This should be same as defined in port/win32_include/j9portpg.h */
#endif

	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	uintptr_t old_vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT];
	uintptr_t old_vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT];
	const char *testName = "omrvmem_testFindValidPageSize(no default large page size)";
	const char *testName2 = "omrvmem_testFindValidPageSize(with 4M default large page size)";
	int rc = 0;

	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	/* Create copy of existing PPG_vmem_pageSize and PPG_vmem_pageFlags as they would be modified in this test */
	memcpy(old_vmem_pageSize, pageSizes, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memcpy(old_vmem_pageFlags, pageFlags, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags without any large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 0;
	pageFlags[1] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName);

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with 4M large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 4 * ONE_MB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName2);

	/* Restore PPG_vmem_pageSize and PPG_vmem_pageFlags to their actual values */
	memcpy(pageSizes, old_vmem_pageSize, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memcpy(pageFlags, old_vmem_pageFlags, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

_exit:
	EXPECT_TRUE(0 == rc) << "Test Failed!";
}

#elif defined(AIXPPC) || defined(LINUXPPC)

static int
omrvmem_testFindValidPageSize_impl(struct OMRPortLibrary *portLibrary, const char *testName)
{
#define MODE_NOT_USED 0

	uintptr_t requestedPageSize = 0;
	uintptr_t requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSizeSupported = FALSE;

	uintptr_t expectedPageSize = 0;
	uintptr_t expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN expectedIsSizeSupported = FALSE;

	uintptr_t defaultLargePageSize = 0;
	uintptr_t defaultLargePageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	uintptr_t mode = 0;
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;

#if defined(OMR_ENV_DATA64)
	BOOLEAN sixteenGBPageSize = FALSE;
#endif /* defined(OMR_ENV_DATA64) */
	BOOLEAN sixteenMBPageSize = FALSE;
	BOOLEAN sixtyFourKBPageSize = FALSE;

	void *address = NULL;
	uintptr_t dataSegmentPageSize = 0;

	intptr_t caseIndex = 1;
	intptr_t i = 0;
	intptr_t rc = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	outputComment(OMRPORTLIB, "\n");
	reportTestEntry(OMRPORTLIB, testName);

	/* First get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	for (i = 0 ; pageSizes[i] != 0 ; i++) {
#if defined(OMR_ENV_DATA64)
		if ((8 * TWO_GB == pageSizes[i]) &&
			(0 != (OMRPORT_VMEM_PAGE_FLAG_NOT_USED & pageFlags[i]))
		) {
			sixteenGBPageSize = TRUE;
		}
#endif /* defined(OMR_ENV_DATA64) */
		if ((16 * ONE_MB == pageSizes[i]) &&
			(0 != (OMRPORT_VMEM_PAGE_FLAG_NOT_USED & pageFlags[i]))
		) {
			sixteenMBPageSize = TRUE;
		}

		if ((16 * FOUR_KB == pageSizes[i]) &&
			(0 != (OMRPORT_VMEM_PAGE_FLAG_NOT_USED & pageFlags[i]))
		) {
			sixtyFourKBPageSize = TRUE;
		}
	}

#if defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "16G page size supported: %s\n", (sixteenGBPageSize) ? "yes" : "no");
#endif /* defined(OMR_ENV_DATA64) */
	outputComment(OMRPORTLIB, "16M page size supported: %s\n", (sixteenMBPageSize) ? "yes" : "no");
	outputComment(OMRPORTLIB, "64K page size supported: %s\n", (sixtyFourKBPageSize) ? "yes" : "no");

	/* On AIX default large page size should always be available.
	 * It is 16M, if available, 64K otherwise
	 */
	omrvmem_default_large_page_size_ex(0, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		outputComment(OMRPORTLIB, "defaultLargePageSize: 0x%zx, defaultLargePageFlags: 0x%zx\n", defaultLargePageSize, defaultLargePageFlags);
	}

	/* Note that -Xlp<size> and -Xlp:objectheap are treated same, and
	 * callers always pass OMRPORT_VMEM_PAGE_FLAG_NOT_USED as the requestedPageFlags
	 * to omrvmem_find_valid_page_size().
	 * Only difference is in case of -Xlp<size> omrvmem_find_valid_page_size() is not called
	 * if default large page size is not present.
	 */

#if defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=16G\n", caseIndex++);

	requestedPageSize = 8 * TWO_GB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == sixteenGBPageSize) {
		expectedPageSize = 8 * TWO_GB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* defined(OMR_ENV_DATA64) */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=16M\n", caseIndex++);

	requestedPageSize = 16 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == sixteenMBPageSize) {
		expectedPageSize = 16 * ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=64K\n", caseIndex++);

	requestedPageSize = 16 * FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != sixtyFourKBPageSize) {
		expectedPageSize = 16 * FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K\n", caseIndex++);

	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not in 4K, 64K, 16M, 16G>\n", caseIndex++);

	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(MODE_NOT_USED, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(MODE_NOT_USED, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);


	/* Test -Xlp:codecache options */

	/* First get the page size of the data segment. This is the page size used by the JIT code cache if 16M Code Pages are not being used */
#if defined(AIXPPC)
#define SAMPLE_BLOCK_SIZE 4
	/* Allocate a memory block using omrmem_allocate_memory, and use the address to get the page size of data segment */
	address = omrmem_allocate_memory(SAMPLE_BLOCK_SIZE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == address) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate block of memory to determine page size of data segment");
		goto _exit;
	}
	struct vm_page_info pageInfo;
	pageInfo.addr = (uint64_t) address;
	rc = vmgetinfo(&pageInfo, VM_PAGE_INFO, sizeof(struct vm_page_info));
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to get page size of data segment using vmgetinfo()");
		goto _exit;
	} else {
		dataSegmentPageSize = (uintptr_t) pageInfo.pagesize;
	}
	omrmem_free_memory(address);
#else
	dataSegmentPageSize = getpagesize();
#endif /* defined(AIXPPC) */

	outputComment(OMRPORTLIB, "Page size of data segment: 0x%zx\n", dataSegmentPageSize);

#if defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=16G\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * TWO_GB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (8 * TWO_GB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

#endif /* defined(OMR_ENV_DATA64) */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=16M\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

#if defined(OMR_ENV_DATA64)
	if (TRUE == sixteenMBPageSize) {
		expectedPageSize = 16 * ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else
#endif /* defined(OMR_ENV_DATA64) */
	{
		expectedPageSize = dataSegmentPageSize;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = (16 * ONE_MB == dataSegmentPageSize) ? TRUE : FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=16M with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	if (TRUE == sixteenMBPageSize) {
		expectedPageSize = 16 * ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = dataSegmentPageSize;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		expectedIsSizeSupported = (16 * ONE_MB == dataSegmentPageSize) ? TRUE : FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(OMR_ENV_DATA64) */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=64K\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (16 * FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=64K with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 16 * FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (16 * FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(OMR_ENV_DATA64) */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = (FOUR_KB == dataSegmentPageSize) ? TRUE : FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(OMR_ENV_DATA64) */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not in 16G, 16M, 64K, 4K>\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

#if !defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not in 16G, 16M, 64K, 4K> with TR_ppcCodeCacheConsolidationEnabled set\n", caseIndex++);
	/* No port library API to set the environment variable. Use setenv() */
	rc = setenv("TR_ppcCodeCacheConsolidationEnabled", "true", 1 /*overwrite any existing value */);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set environment variable TR_ppcCodeCacheConsolidationEnabled");
		goto _exit;
	}
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	/* unset the environment variable TR_ppcCodeCacheConsolidationEnabled */
	unsetenv("TR_ppcCodeCacheConsolidationEnabled");

	expectedPageSize = dataSegmentPageSize;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* !defined(OMR_ENV_DATA64) */

_exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrvmem_testFindValidPageSize(struct OMRPortLibrary *portLibrary)
{
#define OMRPORT_VMEM_PAGESIZE_COUNT 5	/* This should be same as defined in port/unix_include/j9portpg.h */

	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	uintptr_t old_vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT];
	uintptr_t old_vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT];
	const char *testName = "omrvmem_testFindValidPageSize (setup)";
	int rc = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	/* Create copy of existing PPG_vmem_pageSize and PPG_vmem_pageFlags as they would be modified in this test */
	memcpy(old_vmem_pageSize, pageSizes, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memcpy(old_vmem_pageFlags, pageFlags, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags without any large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 0;
	pageFlags[1] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, "omrvmem_testFindValidPageSize(no default large page size)");

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags only 64K large page size  */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 16 * FOUR_KB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, "omrvmem_testFindValidPageSize(64K large page size)");

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with 64K and 16M large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 16 * FOUR_KB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 16 * ONE_MB;
	pageFlags[2] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[3] = 0;
	pageFlags[3] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, "omrvmem_testFindValidPageSize(64K and 16M large page size)");

#if defined(OMR_ENV_DATA64)
	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with all possible large pages
	 * (16G, 16M, 64K)
	 */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[1] = 16 * FOUR_KB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[2] = 16 * ONE_MB;
	pageFlags[2] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[3] = 8 * TWO_GB;
	pageFlags[3] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	pageSizes[4] = 0;
	pageFlags[4] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, "omrvmem_testFindValidPageSize(all possible page sizes)");
#endif /* defined(OMR_ENV_DATA64) */

	/* Restore PPG_vmem_pageSize and PPG_vmem_pageFlags to their actual values */
	memcpy(pageSizes, old_vmem_pageSize, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memcpy(pageFlags, old_vmem_pageFlags, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

_exit:
	return rc;
}

#elif defined(J9ZOS390)

static int
omrvmem_testFindValidPageSize_impl(struct OMRPortLibrary *portLibrary, const char *testName)
{
	uintptr_t requestedPageSize = 0;
	uintptr_t requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSizeSupported = FALSE;

	uintptr_t expectedPageSize = 0;
	uintptr_t expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN expectedIsSizeSupported = FALSE;

	uintptr_t defaultLargePageSize = 0;
	uintptr_t defaultLargePageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	uintptr_t mode = 0;
	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;

#if	defined(OMR_ENV_DATA64)
	BOOLEAN twoGBFixed = FALSE;
	BOOLEAN oneMBFixed = FALSE;
#endif /* defined(OMR_ENV_DATA64) */
	BOOLEAN oneMBPageable = FALSE;

	intptr_t caseIndex = 1;
	intptr_t i = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	outputComment(OMRPORTLIB, "\n");
	reportTestEntry(OMRPORTLIB, testName);

	/* First get all the supported page sizes */
	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	for (i = 0; pageSizes[i] != 0; i++) {
#if	defined(OMR_ENV_DATA64)
		if ((TWO_GB == pageSizes[i]) &&
			(0 != (OMRPORT_VMEM_PAGE_FLAG_FIXED & pageFlags[i]))
		) {
			twoGBFixed = TRUE;
		}

		if ((ONE_MB == pageSizes[i]) &&
			(0 != (OMRPORT_VMEM_PAGE_FLAG_FIXED & pageFlags[i]))
		) {
			oneMBFixed = TRUE;
		}
#endif /* defined(OMR_ENV_DATA64) */
		if ((ONE_MB == pageSizes[i]) &&
			(0 != (OMRPORT_VMEM_PAGE_FLAG_PAGEABLE & pageFlags[i]))
		) {
			oneMBPageable = TRUE;
		}
	}

#if defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "2G nonpageable supported: %s\n", (twoGBFixed) ? "yes" : "no");
	outputComment(OMRPORTLIB, "1M nonpageable supported: %s\n", (oneMBFixed) ? "yes" : "no");
#endif /* defined(OMR_ENV_DATA64) */
	outputComment(OMRPORTLIB, "1M pageable supported: %s\n", (oneMBPageable) ? "yes" : "no");

	omrvmem_default_large_page_size_ex(0, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		outputComment(OMRPORTLIB, "defaultLargePageSize: 0x%zx, defaultLargePageFlags: 0x%zx\n", defaultLargePageSize, defaultLargePageFlags);
	}

	/* Test -Xlp:codecache options */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=1M,pageable\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=1M,nonpageable\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not 4K, 1M>,pageable\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=<not 4K, 1M>,nonpageable\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K,pageable\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:codecache:pagesize=4K,nonpageable\n", caseIndex++);
	mode = OMRPORT_VMEM_MEMORY_MODE_EXECUTE;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	/* Test -Xlp:objectheap options */

#if defined(OMR_ENV_DATA64)
	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=2G,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = TWO_GB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == twoGBFixed) {
		expectedPageSize = TWO_GB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
		expectedIsSizeSupported = TRUE;
	} else if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);
#endif /* defined(OMR_ENV_DATA64) */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=1M,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);
#if defined(OMR_ENV_DATA64)
	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = TRUE;
	} else
#endif /* defined(OMR_ENV_DATA64) */
	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not 4K, 1M, 2G>,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);
#if defined(OMR_ENV_DATA64)
	if (0 != defaultLargePageSize) {
		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;
	} else
#endif /* defined(OMR_ENV_DATA64) */
	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=1M,pageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = TRUE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=<not 4K, 1M>,pageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = 8 * ONE_MB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	if (TRUE == oneMBPageable) {
		expectedPageSize = ONE_MB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	} else {
		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = FALSE;
	}

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K,pageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = TRUE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp:objectheap:pagesize=4K,nonpageable\n", caseIndex++);
	mode = 0;
	requestedPageSize = FOUR_KB;
	requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
	PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

	omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

	expectedPageSize = FOUR_KB;
	expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
	expectedIsSizeSupported = FALSE;

	verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
								  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
								  requestedPageSize, requestedPageFlags, isSizeSupported);

#if defined(OMR_ENV_DATA64)

	/* Test -Xlp<size> options */

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp2G\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(OMRPORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = TWO_GB;
		requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		if (TRUE == twoGBFixed) {
			expectedPageSize = TWO_GB;
			expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_FIXED;
			expectedIsSizeSupported = TRUE;
		} else {
			expectedPageSize = defaultLargePageSize;
			expectedPageFlags = defaultLargePageFlags;
			expectedIsSizeSupported = FALSE;
		}

		verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
									  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									  requestedPageSize, requestedPageFlags, isSizeSupported);
	}

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp1M\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(OMRPORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = ONE_MB;
		requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = TRUE;

		verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
									  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									  requestedPageSize, requestedPageFlags, isSizeSupported);
	}

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp<not 4K, 1M, 2G>\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(OMRPORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = 8 * ONE_MB;
		requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		expectedPageSize = defaultLargePageSize;
		expectedPageFlags = defaultLargePageFlags;
		expectedIsSizeSupported = FALSE;

		verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
									  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									  requestedPageSize, requestedPageFlags, isSizeSupported);
	}

	outputComment(OMRPORTLIB, "\nCase %d: -Xlp4K\n", caseIndex++);
	if (0 == defaultLargePageSize) {
		outputComment(OMRPORTLIB, "Skip this test as the configuration does not support default large page size\n");
	} else {
		mode = 0;
		requestedPageSize = FOUR_KB;
		requestedPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		PRINT_FIND_VALID_PAGE_SIZE_INPUT(mode, requestedPageSize, requestedPageFlags);

		omrvmem_find_valid_page_size(mode, &requestedPageSize, &requestedPageFlags, &isSizeSupported);

		expectedPageSize = FOUR_KB;
		expectedPageFlags = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;
		expectedIsSizeSupported = TRUE;

		verifyFindValidPageSizeOutput(OMRPORTLIB, testName,
									  expectedPageSize, expectedPageFlags, expectedIsSizeSupported,
									  requestedPageSize, requestedPageFlags, isSizeSupported);
	}

#endif /* defined(OMR_ENV_DATA64) */

_exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrvmem_testFindValidPageSize(struct OMRPortLibrary *portLibrary)
{
#define OMRPORT_VMEM_PAGESIZE_COUNT 5	/* This should be same as defined in port/zos390/j9portpg.h */

	uintptr_t *pageSizes = NULL;
	uintptr_t *pageFlags = NULL;
	uintptr_t old_vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT];
	uintptr_t old_vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT];
	char *testName = "omrvmem_testFindValidPageSize (setup)";
	int rc = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	pageSizes = omrvmem_supported_page_sizes();
	pageFlags = omrvmem_supported_page_flags();

	if (pageSizes[0] == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "There aren't any supported page sizes on this platform \n");
		goto _exit;
	}

	/* Create copy of existing PPG_vmem_pageSize and PPG_vmem_pageFlags as they would be modified in this test */
	memcpy(old_vmem_pageSize, pageSizes, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memcpy(old_vmem_pageFlags, pageFlags, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

	testName = "omrvmem_testFindValidPageSize(no default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags without any large page size */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = 0;
	pageFlags[1] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName);


	testName = "omrvmem_testFindValidPageSize(pageable 1M pages)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with only Pageable 1MB large pages  */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName);

#if defined(OMR_ENV_DATA64)

	testName = "omrvmem_testFindValidPageSize(only default large page size)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with only default large page size (Non-pageable 1MB pages) */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[2] = 0;
	pageFlags[2] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName);


	testName = "omrvmem_testFindValidPageSize(both pageable and nonpageable 1M pages)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with both pageable and non-pageable 1M large pages */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[2] = ONE_MB;
	pageFlags[2] = OMRPORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[3] = 0;
	pageFlags[3] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName);


	testName = "omrvmem_testFindValidPageSize(all possible page sizes)";

	/* Create new PPG_vmem_pageSize and PPG_vmem_pageFlags with all possible large pages
	 * (non-pageable 2G, non-pageable 1M and pageable 1M)
	 */
	pageSizes[0] = FOUR_KB;
	pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[1] = ONE_MB;
	pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_PAGEABLE;

	pageSizes[2] = ONE_MB;
	pageFlags[2] = OMRPORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[3] = TWO_GB;
	pageFlags[3] = OMRPORT_VMEM_PAGE_FLAG_FIXED;

	pageSizes[4] = 0;
	pageFlags[4] = 0;

	rc |= omrvmem_testFindValidPageSize_impl(OMRPORTLIB, testName);

#endif /* defined(OMR_ENV_DATA64) */

	/* Restore PPG_vmem_pageSize and PPG_vmem_pageFlags to their actual values */
	memcpy(pageSizes, old_vmem_pageSize, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memcpy(pageFlags, old_vmem_pageFlags, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

_exit:
	return rc;
}

#endif /* defined(J9ZOS390) */


uintptr_t
getSizeFromString(struct OMRPortLibrary *portLibrary, char *sizeString)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	if (0 == strcmp(sizeString, "4K")) {
		return FOUR_KB;
	} else if (0 == strcmp(sizeString, "64K")) {
		return SIXTY_FOUR_KB;
	} else if (0 == strcmp(sizeString, "16M")) {
		return D16M;
	} else if (0 == strcmp(sizeString, "2M")) {
		return D2M;
	} else if (0 == strcmp(sizeString, "256M")) {
		return D256M;
	} else if (0 == strcmp(sizeString, "512M")) {
		return D512M;
	} else if (0 == strcmp(sizeString, "2G")) {
		return TWO_GB;
#if defined (OMR_ENV_DATA64)
	} else if (0 == strcmp(sizeString, "4G")) {
		return J9CONST_U64(2) * TWO_GB;
	} else if (0 == strcmp(sizeString, "10G")) {
		return J9CONST_U64(5) * TWO_GB;
	} else if (0 == strcmp(sizeString, "16G")) {
		return J9CONST_U64(8) * TWO_GB;
	} else if (0 == strcmp(sizeString, "32G")) {
		return J9CONST_U64(16) * TWO_GB;
#endif
	} else {
		outputComment(OMRPORTLIB, "\n\nWarning: invalid option (%s)\n\n", sizeString);
	}
	return 0;
}

/**
 * Run with varied arguments based on values from /proc/meminfo, this function tests effects of disclaiming memory
 * on performance and prints out performance results
 *
 * @ref omrvmem.c
 *
 * @param[in] portLibrary The port library under test
 * @param[in] disclaimPerfTestArg This arg holds the page size to disclaim and the byte amount
 * @param[in] shouldDisclaim  If true, disclaim virtual memory, otherwise do not disclaim.
 *
 * @return 0 on success, -1 on failure
 */
int32_t
omrvmem_disclaimPerfTests(struct OMRPortLibrary *portLibrary, char *disclaimPerfTestArg, BOOLEAN shouldDisclaim)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	int rc = 0;
	uintptr_t byteAmount = 0;
	uintptr_t memTotal = 0;
	uintptr_t pageSize = 0;
	char *pchar;
	uintptr_t numIterations = DEFAULT_NUM_ITERATIONS;

	/* parse the disclaimPerfTestArg to get the pageSize, byteAmount and disclaim values
	 * disclaimPerfTestArg is the string after the ':' from -disclaimPerfTest:<pageSize>,<byteAmount>,<totalMemory>[,<numIterations>]
	 * while shouldDisclaim is set from the +/- prefix, numIterations is optional (default is 50 iterations)
	 * Example options: -disclaimPerfTest:4K,256M,4G
	 *              	+disclaimPerfTest:4K,256M,4G,100
	 */
	pchar = strtok(disclaimPerfTestArg, ",");
	pageSize = getSizeFromString(OMRPORTLIB, pchar);

	pchar = strtok(NULL, ",");
	byteAmount = getSizeFromString(OMRPORTLIB, pchar);

	pchar = strtok(NULL, ",");
	memTotal = getSizeFromString(OMRPORTLIB, pchar);

	pchar = strtok(NULL, ",");
	if (NULL != pchar) {
		numIterations = atoi(pchar);
	}

	if ((0 == pageSize) || (0 == byteAmount) || (0 == memTotal)) {
		return -1;
	} else if (0 == numIterations) {
		outputComment(OMRPORTLIB, "\n\nWarning: invalid option numIterations = (%s)\n\n", pchar);
		return -1;
	}

	/* Display unit under test */
	HEADING(OMRPORTLIB, "Disclaim performance tests");

	/* verify sanity of port library for these tests.  If this fails do not continue */
	/* OMROTOD: This function was changed into a test, fix it so it can be called here.
	 *rc = omrvmem_verify_functiontable_slots(portLibrary);
	 */

	if (TEST_PASS == rc) {
		outputComment(OMRPORTLIB, "\npageSize: 0x%zx, byteAmount 0x%zx, memTotal 0x%zx, numIterations %i, disclaim %i\n", pageSize, byteAmount, memTotal, numIterations, shouldDisclaim);
		rc |= omrvmem_bench_write_and_decommit_memory(OMRPORTLIB, pageSize, byteAmount, shouldDisclaim, numIterations);
		rc |= omrvmem_bench_reserve_write_decommit_and_free_memory(OMRPORTLIB, pageSize, byteAmount, shouldDisclaim, numIterations);
#if defined (OMR_ENV_DATA64)
		rc |= omrvmem_exhaust_virtual_memory(OMRPORTLIB, pageSize, byteAmount, shouldDisclaim);
#endif
		rc |= omrvmem_bench_force_overcommit_then_decommit(OMRPORTLIB, memTotal, pageSize, shouldDisclaim);

		/* Output results */
		outputComment(OMRPORTLIB, "\nDisclaim performance test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	}
	return TEST_PASS == rc ? 0 : -1;
}

/**
 * Helper for omrvmem_testOverlappingSegments. Initializes the J9PortVmemParams struct.
 */
J9PortVmemParams *
getParams(struct OMRPortLibrary *portLibrary, J9PortVmemParams *vmemParams)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	uintptr_t *pageSizes = omrvmem_supported_page_sizes();
	omrvmem_vmem_params_init(vmemParams);
	vmemParams->byteAmount = pageSizes[0];
	vmemParams->pageSize = pageSizes[0];
	vmemParams->mode |= OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_VIRTUAL;
	vmemParams->options |= OMRPORT_VMEM_STRICT_PAGE_SIZE;
	vmemParams->category = OMRMEM_CATEGORY_PORT_LIBRARY;

	return vmemParams;
}

/**
 * Helper for omrvmem_testOverlappingSegments. Checks whether two memory segments overlap.
 */
BOOLEAN
checkForOverlap(J9PortVmemIdentifier *a, J9PortVmemIdentifier *b)
{
	BOOLEAN A_before_B = ((uintptr_t)a->address + a->size) <= (uintptr_t)b->address;
	BOOLEAN B_before_A = ((uintptr_t)b->address + b->size) <= (uintptr_t)a->address;

	if (A_before_B || B_before_A) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/**
 * Allocates CYCLES virtual memory segments and checks for overlapping segments.
 *
 * @ref omrvmem.c
 */
TEST(PortVmemTest, vmem_testOverlappingSegments)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	int32_t result = 0;
	J9PortVmemParams *vmemParams;
	struct J9PortVmemIdentifier *vmemID;
	int *keepCycles;
	int freed = 0;
	int i = 0;
	int j = 0;
	const char *testName = "omrvmem__testOverlappingSegments";

	vmemParams = (J9PortVmemParams *)omrmem_allocate_memory(sizeof(J9PortVmemParams) * CYCLES, OMRMEM_CATEGORY_PORT_LIBRARY);
	vmemID = (J9PortVmemIdentifier *)omrmem_allocate_memory(sizeof(J9PortVmemIdentifier) * CYCLES, OMRMEM_CATEGORY_PORT_LIBRARY);
	keepCycles = (int *)omrmem_allocate_memory(sizeof(int) * CYCLES, OMRMEM_CATEGORY_PORT_LIBRARY);

	reportTestEntry(OMRPORTLIB, testName);
	memset(keepCycles, 0, CYCLES);
	srand(10);

	outputComment(OMRPORTLIB, "Cycles: %d\n\n", CYCLES);
	outputComment(OMRPORTLIB, "Segment           Start             End               Size              Keep cycle\n");

	for (i = 0; i < CYCLES; i++) {
		char *memPtr = NULL;
		getParams(OMRPORTLIB, &vmemParams[i]);
		memPtr = (char *)omrvmem_reserve_memory_ex(&vmemID[i], &vmemParams[i]);

		if (NULL == memPtr) {
			outputComment(OMRPORTLIB, "Failed to get memory. Error: %s.\n", strerror(errno));
			outputComment(OMRPORTLIB, "Ignoring memory allocation failure(%d of %d loops finished).\n", i, CYCLES);
			goto exit;
		}
		/* Determine how long to keep the segment */
		keepCycles[i] = (rand() % (((int)((double)CYCLES / FREE_PROB)) - i)) + i + 1;
		outputComment(OMRPORTLIB, "%.16d  %.16X  %.16X  %.16X  %d\n", i, (uintptr_t)vmemID[i].address, (uintptr_t)vmemID[i].address + vmemID[i].size, vmemID[i].size, keepCycles[i]);

		/* Check for overlapping segments */
		for (j = 0; j < i; j++) {
			if (keepCycles[j] >= i) { /* If not already freed */
				if (checkForOverlap(&vmemID[i], &vmemID[j])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "OVERLAP\n");
					outputComment(OMRPORTLIB, "%.16d  %.16X  %.16X  %.16X  %d\n", j, (uintptr_t)vmemID[j].address, (uintptr_t)vmemID[j].address + vmemID[j].size, vmemID[j].size, keepCycles[j]);
					keepCycles[i] = i - 1; /* Pretend this segment is already freed to avoid double freeing overlapping segments */
					result = -1;
					goto exit;
				}
			}
		}

		/* Free segment if marked for freeing at this cycle*/
		for (j = 0; j < i; j++) {
			if (keepCycles[j] == i) {
				int32_t rc = omrvmem_free_memory(vmemID[j].address, vmemParams[j].byteAmount, &vmemID[j]);
				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Problem freeing segment...");
					outputComment(OMRPORTLIB, "%.16d  %.16X  %.16X  %.16X  %d\n", j, (uintptr_t)vmemID[j].address, (uintptr_t)vmemID[j].address + vmemID[j].size, vmemID[j].size, keepCycles[j]);
					result = -1;
					goto exit;
				} else {
					outputComment(OMRPORTLIB, "%.16d  Freed\n", j);
					freed++;
				}
			}
		}
	}
exit:
	outputComment(OMRPORTLIB, "\n=========================\n");
	outputComment(OMRPORTLIB, "%d cycles completed\n", i);
	outputComment(OMRPORTLIB, "%d segments freed\n\n", freed);

	/* Free remaining segments */
	freed = 0;
	for (j = 0; j < CYCLES; j++) {
		if (keepCycles[j] >= i) {
			int32_t rc = omrvmem_free_memory(vmemID[j].address, vmemParams[j].byteAmount, &vmemID[j]);
			if (0 == rc) {
				freed++;
			}
		}
	}
	outputComment(OMRPORTLIB, "%d remaining segments freed\n\n", freed);

	omrmem_free_memory(vmemParams);
	omrmem_free_memory(vmemID);
	omrmem_free_memory(keepCycles);
	reportTestExit(OMRPORTLIB, testName);
	EXPECT_TRUE(0 == result) << "Test Failed!";
}

/**
 * Queries process virtual, physical, and private memory sizes.
 *
 * @ref omrvmem.c
 */
TEST(PortVmemTest, vmem_testGetProcessMemorySize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	int32_t result = -1;
	uint64_t size = 0;
	const char *testName = "vmem_testGetProcessMemorySize";

	reportTestEntry(OMRPORTLIB, testName);
#if defined(J9ZOS390)
	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_PRIVATE, &size);
	EXPECT_TRUE(result < 0) << "OMRPORT_VMEM_PROCESS_PRIVATE did not fail";
	EXPECT_TRUE(0 == size) << "value updated when query invalid";

	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_PHYSICAL, &size);
	EXPECT_TRUE(result < 0) << "OMRPORT_VMEM_PROCESS_PHYSICAL did not fail";
	EXPECT_TRUE(0 == size) << "value updated when query invalid";

	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_VIRTUAL, &size);
	EXPECT_TRUE(result < 0) << "OMRPORT_VMEM_PROCESS_VIRTUAL did not fail";
	EXPECT_TRUE(0 == size) << "value updated when query invalid";
#else
#if !defined(OSX)
	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_PRIVATE, &size);
	EXPECT_TRUE(0 == result) << "OMRPORT_VMEM_PROCESS_PRIVATE failed";
	EXPECT_TRUE(size > 0) << "OMRPORT_VMEM_PROCESS_PRIVATE returned 0";
	outputComment(OMRPORTLIB, "OMRPORT_VMEM_PROCESS_PRIVATE = %lu.\n", size);
#endif /* !defined(OSX) */

#if defined(LINUX) || defined(WIN32) || defined(OSX)
	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_PHYSICAL, &size);
	EXPECT_TRUE(0 == result) << "OMRPORT_VMEM_PROCESS_PHYSICAL failed";
	EXPECT_TRUE(size > 0) << "OMRPORT_VMEM_PROCESS_PHYSICAL returned 0";
	outputComment(OMRPORTLIB, "OMRPORT_VMEM_PROCESS_PHYSICAL = %lu.\n", size);

	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_VIRTUAL, &size);
	EXPECT_TRUE(0 == result) << "OMRPORT_VMEM_PROCESS_VIRTUAL failed";
	EXPECT_TRUE(size > 0) << "OMRPORT_VMEM_PROCESS_VIRTUAL returned 0";
	outputComment(OMRPORTLIB, "OMRPORT_VMEM_PROCESS_VIRTUAL = %lu.\n", size);
#elif defined(AIXPPC)
	size = 0;
	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_PHYSICAL, &size);
	EXPECT_TRUE(result < 0) << "OMRPORT_VMEM_PROCESS_PHYSICAL did not fail";
	EXPECT_TRUE(0 == size) << "value updated when query invalid";

	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_VIRTUAL, &size);
	EXPECT_TRUE(result < 0) << "OMRPORT_VMEM_PROCESS_VIRTUAL did not fail";
	EXPECT_TRUE(0 == size) << "value updated when query invalid";
#endif /* aix*/
#endif /* zos */
	size = 0;
	result = omrvmem_get_process_memory_size(OMRPORT_VMEM_PROCESS_EnsureWideEnum, &size);
	EXPECT_TRUE(result < 0) << "Invalid query not detected";
	EXPECT_TRUE(0 == size) << "value updated when query invalid";
}

/* This function is used by omrvmem_test_reserveExecutableMemory */
int
myFunction1()
{
	return 1;
}

/* This function is used by omrvmem_test_reserveExecutableMemory to tell
 * where myFunction1() ends. It is never called but its body must be
 * different from myFunction1() so that it is not optimized away by the
 * C compiler. */
void
myFunction2()
{
	int i = 0;
	while (++i < 10);
	return;
}
