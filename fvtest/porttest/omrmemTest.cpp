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
 * $RCSfile: omrmemTest.c,v $
 * $Revision: 1.55 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library memory management.
 *
 * Exercise the API for port library memory management operations.  These functions
 * can be found in the file @ref omrmem.c
 *
 * @note port library memory management operations are not optional in the port library table.
 *
 */
#include <stdlib.h>
#include <string.h>
#include "omrmemcategories.h"

#include "testHelpers.hpp"
#include "omrport.h"

extern PortTestEnvironment *portTestEnv;

#define MAX_ALLOC_SIZE 256 /**<@internal largest size to allocate */

#define allocNameSize 64 /**< @internal buffer size for name function */

#if defined(J9ZOS390)
#define MEM32_LIMIT 0X7FFFFFFF
#else
#define MEM32_LIMIT 0xFFFFFFFF
#endif

#if defined(OMR_ENV_DATA64)
/* this macro corresponds to the one defined in omrmem32helpers */
#define HEAP_SIZE_BYTES 8*1024*1024
#endif

#define COMPLETE_LARGE_REGION 1
#define COMPLETE_NEAR_REGION 2
#define COMPLETE_SUBALLOC_BLOCK 4
#define COMPLETE_ALL_SIZES (COMPLETE_LARGE_REGION | COMPLETE_NEAR_REGION | COMPLETE_SUBALLOC_BLOCK)

#if !(defined(OSX) && defined(OMR_ENV_DATA64))
static void freeMemPointers(struct OMRPortLibrary *portLibrary, void **memPtrs, uintptr_t length);
static void shuffleArray(struct OMRPortLibrary *portLibrary, uintptr_t *sizes, uintptr_t length);
#endif /* !(defined(OSX) && defined(OMR_ENV_DATA64)) */

/**
 * @internal
 * @typdef
 */
typedef void *(*J9MEM_ALLOCATE_MEMORY_FUNC)(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite);

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

	/* TODO alignment */
/*
	if (0 != ((uintptr_t)memPtr % sizeof(void*))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_allocate_memory(%d) pointer mis aligned\n", byteAmount);
	}
*/

	/* TODO double array alignment */

	/* Don't trample memory */
	if (0 == byteAmount) {
		return;
	}

	/* Test write */
	memPtr[0] = testCharA;
	if (testCharA != memPtr[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test 0\n", allocName);
	}

	/* Fill the buffer */
	memset(memPtr, testCharC, byteAmount);
	memset(stackMemory, testCharC, MAX_ALLOC_SIZE);

	/* Can only verify characters as large as the stack allocated size */
	if (MAX_ALLOC_SIZE < byteAmount) {
		testSize = MAX_ALLOC_SIZE;
	} else {
		testSize = byteAmount;
	}

	/* Check memory retained value */
	memPtr[testSize - 1] = testCharC;
	if (0 != memcmp(stackMemory, memPtr, testSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to retain value\n", allocName);
	}

	/* NUL terminate, check length */
	memPtr[byteAmount - 1] = '\0';
	if (strlen(memPtr) != byteAmount - 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed write test\n", allocName);
	}
}

/**
 * Verify port library memory management.
 *
 * Ensure the port library is properly setup to run string operations.
 */
TEST(PortMemTest, mem_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test0";

	reportTestEntry(OMRPORTLIB, testName);

	/* Verify that the memory management function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	if (NULL == OMRPORTLIB->mem_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_startup is NULL\n");
	}

	/*  Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->mem_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_shutdown is NULL\n");
	}

	/* omrmem_test2 */
	if (NULL == OMRPORTLIB->mem_allocate_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_allocate_memory is NULL\n");
	}

	/* omrmem_test1 */
	if (NULL == OMRPORTLIB->mem_free_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_free_memory is NULL\n");
	}

	/* omrmem_test4 */
	if (NULL == OMRPORTLIB->mem_reallocate_memory) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_reallocate_memory, is NULL\n");
	}

	if (NULL == OMRPORTLIB->mem_allocate_memory32) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_allocate_memory32, is NULL\n");
	}

	if (NULL == OMRPORTLIB->mem_free_memory32) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_free_memory32, is NULL\n");
	}

	if (NULL == OMRPORTLIB->mem_walk_categories) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mem_walk_categories, is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library memory management.
 *
 * Ensure the port library memory management function
 * @ref omrmem.c::omrmem_free_memory "omrmem_free_memory()"
 * handles NULL pointers as well as valid pointers values.
 */
TEST(PortMemTest, mem_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test1";
	char *memPtr;

	reportTestEntry(OMRPORTLIB, testName);

	/* First verify that a NULL pointer can be freed, as specified by API docs. This ensures that one can free any pointer returned from
	 * allocation routines without checking for NULL first.
	 */
	memPtr = NULL;
	omrmem_free_memory(memPtr);
	if (NULL != memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_free_memory(NULL) altered pointer\n");
		goto exit; /* no point in continuing */
	}

	/* One more test of free with a non NULL pointer */
	memPtr = (char *)omrmem_allocate_memory(32, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_allocate_memory(32) returned NULL\n");
		goto exit;
	}
	omrmem_free_memory(memPtr);

	/* What can we really check? */

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library memory management.
 *
 * Allocate memory of various sizes, including a size of 0 using the
 * port library function
 * @ref omrmem.c::omrmem_allocate_memory "omrmem_allocate_memory()".
 */
TEST(PortMemTest, mem_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test2";

	char *memPtr;
	char allocName[allocNameSize];
	uint32_t byteAmount;

	reportTestEntry(OMRPORTLIB, testName);

	/* allocate some memory of various sizes */
	byteAmount = 0;
	omrstr_printf(allocName, allocNameSize, "omrmem_allocate_memory(%d)", byteAmount);
	memPtr = (char *)omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = 1;
	omrstr_printf(allocName, allocNameSize, "omrmem_allocate_memory(%d)", byteAmount);
	memPtr = (char *)omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 8;
	omrstr_printf(allocName, allocNameSize, "omrmem_allocate_memory(%d)", byteAmount);
	memPtr = (char *)omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 4;
	omrstr_printf(allocName, allocNameSize, "omrmem_allocate_memory(%d)", byteAmount);
	memPtr = (char *)omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 2;
	omrstr_printf(allocName, allocNameSize, "omrmem_allocate_memory(%d)", byteAmount);
	memPtr = (char *)omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE;
	omrstr_printf(allocName, allocNameSize, "omrmem_allocate_memory(%d)", byteAmount);
	memPtr = (char *)omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	reportTestExit(OMRPORTLIB, testName);
}

/* removed test 3 due to DESIGN 355 (no longer separate allocate function for callsite) */

/**
 * Verify port library memory management.
 *
 * Allocate memory of various sizes, including a size of 0.  Free this memory
 * then re-allocate via the port library function
 * @ref omrmem.c::omrmem_reallocate_memory "omrmem_reallocate_memory()".
 */
TEST(PortMemTest, mem_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test4";

	/* Not visible outside the port library, so can't verify.  Should this be visible
	 * outside the portlibrary?
	 */
#if 0
	void *memPtr;
	void *saveMemPtr;
	char allocName[allocNameSize];
	int32_t rc;
	uint32_t byteAmount;

	reportTestEntry(OMRPORTLIB, testName);

	/* allocate some memory and then free it */
	byteAmount = MAX_ALLOC_SIZE;
	memPtr = omrmem_allocate_memory(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == memPtr) {
		goto exit;
	}
	saveMemPtr = memPtr;
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	/* Now re-allocate memory of various sizes */
	/* TODO is memPtr supposed to be re-used? */
	byteAmount = 0;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_reallocate_memory(%d)", byteAmount);
	memPtr = omrmem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = 1;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_reallocate_memory(%d)", byteAmount);
	memPtr = omrmem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 8;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_reallocate_memory(%d)", byteAmount);
	memPtr = omrmem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 4;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_reallocate_memory(%d)", byteAmount);
	memPtr = omrmem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 2;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_reallocate_memory(%d)", byteAmount);
	memPtr = omrmem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_reallocate_memory(%d)", byteAmount);
	memPtr = omrmem_reallocate_memory(saveMemPtr, byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_free_memory(memPtr);
	memPtr = NULL;
#endif

	reportTestEntry(OMRPORTLIB, testName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library memory management.
 *
 * Ensure the port library memory management function
 * @ref omrmem.c::omrmem_deallocate_portLibrary "omrmem_deallocate_memory()"
 * handles NULL pointers as well as valid pointers values.
 *
 * @note This function does not need to be implemented in terms of
 * @ref omrmem.c::omrmem_free_memory "omrmem_free_memory()", but it may be on some
 * platforms.
 */
TEST(PortMemTest, mem_test5)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test5";

	/* Not visible outside the port library, so can't verify.  Should this be visible
	 * outside the portlibrary?
	 */
#if 0
	OMRPortLibrary *memPtr;

	reportTestEntry(OMRPORTLIB, testName);

	/* First verify that a NULL pointer can be freed, then can free any pointer returned from
	 * allocation routines without checking for NULL
	 */
	memPtr = NULL;
	omrmem_deallocate_portLibrary(memPtr);
	if (NULL != memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_deallocate_portLibrary(NULL) altered pointer\n");
		goto exit; /* no point in continuing */
	}

	/* One more test of free with a non NULL pointer */
	memPtr = omrmem_allocate_portLibrary(32);
	if (NULL == memPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_allocate_library(32) returned NULL\n");
		goto exit;
	}
	omrmem_deallocate_portLibrary(memPtr);

	/* What can we really check? */
#endif

	reportTestEntry(OMRPORTLIB, testName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library memory management.
 *
 * Self allocating port libraries call a function not available via the port library
 * table.  Need to verify that these functions have been correctly implemented.  These
 * functios are called by the port library management routines in @ref j9port.c "j9port.c".
 * The API for these functions clearly state they must be implemented.
 *
 * Verify @ref omrmem.c::omrmem_allocate_portLibrary "omrmem_allocate_portLibrary()" allocates
 * memory correctly.
 *
 * @note This function does not need to be implemented in terms of
 * @ref omrmem.c::omrmem_allocate_memory "omrmem_allocate_memory()", but it may be on some
 * platforms.
 */
TEST(PortMemTest, mem_test6)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test6";

	/* Not visible outside the port library, so can't verify.  Should this be visible
	 * outside the portlibrary?
	 */
#if 0
	char *memPtr;
	char allocName[allocNameSize];
	int32_t rc;
	uint32_t byteAmount;

	reportTestEntry(OMRPORTLIB, testName);

	/* allocate some memory of various sizes */
	byteAmount = 0;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_allocate_portLibrary(%d)", byteAmount);
	memPtr = omrmem_allocate_portLibrary(byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = 1;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_allocate_portLibrary(%d)", byteAmount);
	memPtr = omrmem_allocate_portLibrary(byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 8;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_allocate_portLibrary(%d)", byteAmount);
	memPtr = omrmem_allocate_portLibrary(byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 4;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_allocate_portLibrary(%d)", byteAmount);
	memPtr = omrmem_allocate_portLibrary(byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE / 2;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_allocate_portLibrary(%d)", byteAmount);
	memPtr = omrmem_allocate_portLibrary(byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_deallocate_portLibrary(memPtr);
	memPtr = NULL;

	byteAmount = MAX_ALLOC_SIZE;
	rc = omrstr_printf(allocName, allocNameSize, "omrmem_allocate_portLibrary(%d)", byteAmount);
	memPtr = omrmem_allocate_portLibrary(byteAmount);
	verifyMemory(OMRPORTLIB, testName, memPtr, byteAmount, allocName);
	omrmem_deallocate_portLibrary(memPtr);
	memPtr = NULL;
#endif

	reportTestEntry(OMRPORTLIB, testName);
	reportTestExit(OMRPORTLIB, testName);
}

/* 64bit OSX does not allow memory below 4GB to be mapped. */
#if !(defined(OSX) && defined(OMR_ENV_DATA64))

/* helper that shuffle contents of an array using a random number generator */
static void
shuffleArray(struct OMRPortLibrary *portLibrary, uintptr_t *array, uintptr_t length)
{
	uintptr_t i;

	for (i = 0; i < length; i++) {
		intptr_t randNum = (intptr_t)(rand() % length);
		uintptr_t temp = array[i];

		array[i] = array[randNum];
		array[randNum] = temp;
	}
}

/**
 * Verify port library memory management.
 *
 * Ensure the function @ref omrmem.c::omrmem_allocate_memory32 "omrmem_allocate_memory()"
 * returns a memory location that is below the 4 gig boundary, and as well that the entire region
 * is below the 4 gig boundary.
 *
 * We allocate a variety of sizes comparable to the heap size used in mem_allocate_memory32 implementation (see HEAP_SIZE_BYTES in omrmem32helpers.c):
 * 1. size requests that are much smaller than the heap size are satisfied with omrheap suballocation.
 * 2. size requests that are slightly smaller than the heap size will not use omrheap suallocation (because they cannot be satisfied by
 *    suballocating a omrheap due to heap management overhead), but rather given their own vmem allocated chunk.
 * 3. size requests that are greater or equal to the heap size also will not use omrheap suallocation, but rather given their own vmem allocated chunk.
 *
 * @note This function does not need to be implemented in terms of
 * @ref omrmem.c::omrmem_free_memory "omrmem_free_memory()", but it may be on some
 * platforms.
 */
TEST(PortMemTest, mem_test7_allocate32)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	void *pointer;
#if defined(OMR_ENV_DATA64)
	uintptr_t finalAllocSize;
#endif
	char allocName[allocNameSize];
	const char *testName = "omrmem_test7_allocate32";
	int randomSeed = 0;
	char rand[99] = "";
	int i;

	/* Use a more exhausive size array for 64 bit platforms to exercise the omrheap API.
	 * Retain the old sizes for regular 32 bit VMs, including ME platforms with very limited amount of physical memory.
	 */
#if defined(OMR_ENV_DATA64)
	uintptr_t allocBlockSizes[] = {0, 1, 512, 4096, 1024 * 1024,
								   HEAP_SIZE_BYTES / 8, HEAP_SIZE_BYTES / 4, HEAP_SIZE_BYTES / 3, HEAP_SIZE_BYTES / 2,
								   HEAP_SIZE_BYTES - 6, HEAP_SIZE_BYTES - 10,
								   HEAP_SIZE_BYTES, HEAP_SIZE_BYTES + 6
								  };
#else
	uintptr_t allocBlockSizes[] = {0, 1, 512, 4096, 4096 * 1024};
#endif
	uintptr_t allocBlockSizesLength = sizeof(allocBlockSizes) / sizeof(allocBlockSizes[0]);
	void *allocBlockReturnPtrs[sizeof(allocBlockSizes) / sizeof(allocBlockSizes[0])];
	uintptr_t allocBlockCursor = 0;


	reportTestEntry(OMRPORTLIB, testName);

	for (i = 1; i < portTestEnv->_argc; i += 1) {
		if (startsWith(portTestEnv->_argv[i], (char *)"-srand:")) {
			strcpy(rand, &portTestEnv->_argv[i][7]);
			randomSeed = atoi(rand);
		}
	}
	if (0 == randomSeed) {
		randomSeed = (int)omrtime_current_time_millis();
	}
	outputComment(OMRPORTLIB, "Random seed value: %d. Add -srand:[seed] to the command line to reproduce this test manually.\n", randomSeed);
	srand(randomSeed);

	shuffleArray(OMRPORTLIB, allocBlockSizes, allocBlockSizesLength);

	for (; allocBlockCursor < allocBlockSizesLength; allocBlockCursor++) {
		uintptr_t byteAmount = allocBlockSizes[allocBlockCursor];
		void **returnPtrLocation = &allocBlockReturnPtrs[allocBlockCursor];

		omrstr_printf(allocName, allocNameSize, "\nomrmem_allocate_memory32(%d)", byteAmount);
		pointer = omrmem_allocate_memory32(byteAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
		verifyMemory(OMRPORTLIB, testName, (char *)pointer, byteAmount, allocName);

#if defined(OMR_ENV_DATA64)
		if (((uintptr_t)pointer + byteAmount) > MEM32_LIMIT) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Entire memory block allocated by %s is not below 32-bit limit\n", allocName);
		}
#endif
		*returnPtrLocation = pointer;
	}

	/* all return pointers are verified, now free them */
	freeMemPointers(OMRPORTLIB, allocBlockReturnPtrs, allocBlockSizesLength);

#if defined(OMR_ENV_DATA64)
	/* now we should have at least one entirely free J9Heap, try suballocate a large portion of the it.
	 * This should not incur any vmem allocation.
	 */
	finalAllocSize = HEAP_SIZE_BYTES - 60;
	omrstr_printf(allocName, allocNameSize, "\nomrmem_allocate_memory32(%d)", finalAllocSize);
	pointer = omrmem_allocate_memory32(finalAllocSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	verifyMemory(OMRPORTLIB, testName, (char *)pointer, finalAllocSize, allocName);
#endif

	/* should not result in a crash */
	omrmem_free_memory32(NULL);

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* !(defined(OSX) && defined(OMR_ENV_DATA64)) */


/* Dummy categories for omrmem_test8_categories */

#define DUMMY_CATEGORY_ONE 0
#define DUMMY_CATEGORY_TWO 1
#define DUMMY_CATEGORY_THREE 2

static uint32_t childrenOfDummyCategoryOne[] = {DUMMY_CATEGORY_TWO, DUMMY_CATEGORY_THREE, OMRMEM_CATEGORY_PORT_LIBRARY, OMRMEM_CATEGORY_UNKNOWN};

static OMRMemCategory dummyCategoryOne = {"Dummy One", DUMMY_CATEGORY_ONE, 0, 0, 0, 0, 4, childrenOfDummyCategoryOne};

static OMRMemCategory dummyCategoryTwo = {"Dummy Two", DUMMY_CATEGORY_TWO, 0, 0, 0, 0, 0, NULL};

static OMRMemCategory dummyCategoryThree = {"Dummy Three", DUMMY_CATEGORY_THREE, 0, 0, 0, 0, 0, NULL};

static OMRMemCategory *categoryList[3] = {&dummyCategoryOne, &dummyCategoryTwo, &dummyCategoryThree};

static OMRMemCategorySet dummyCategorySet = {3, categoryList};

/* Structure used to summarise the state of the categories */
struct CategoriesState {
	BOOLEAN dummyCategoryOneWalked;
	BOOLEAN dummyCategoryTwoWalked;
	BOOLEAN dummyCategoryThreeWalked;
	BOOLEAN portLibraryWalked;
	BOOLEAN unknownWalked;
#if defined(OMR_ENV_DATA64)
	BOOLEAN unused32bitSlabWalked;
#endif


	BOOLEAN otherError;

	uintptr_t dummyCategoryOneBytes;
	uintptr_t dummyCategoryOneBlocks;
	uintptr_t dummyCategoryTwoBytes;
	uintptr_t dummyCategoryTwoBlocks;
	uintptr_t dummyCategoryThreeBytes;
	uintptr_t dummyCategoryThreeBlocks;
	uintptr_t portLibraryBytes;
	uintptr_t portLibraryBlocks;
	uintptr_t unknownBytes;
	uintptr_t unknownBlocks;
#if defined(OMR_ENV_DATA64)
	uintptr_t unused32bitSlabBytes;
	uintptr_t unused32bitSlabBlocks;
#endif
};

static uintptr_t
categoryWalkFunction(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *walkState)
{
	OMRPORT_ACCESS_FROM_OMRPORT((OMRPortLibrary *)walkState->userData1);
	struct CategoriesState *state = (struct CategoriesState *)walkState->userData2;
	const char *testName = "omrmem_test8_categories - categoryWalkFunction";

	switch (categoryCode) {
	case DUMMY_CATEGORY_ONE:
		if (! isRoot) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_ONE should be a root, and isn't\n");
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (categoryName != dummyCategoryOne.name) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_ONE has the wrong name: %p=%s instead of %p=%s\n", categoryName, categoryName, dummyCategoryOne.name, dummyCategoryOne.name);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		state->dummyCategoryOneWalked = TRUE;
		state->dummyCategoryOneBytes = liveBytes;
		state->dummyCategoryOneBlocks = liveAllocations;

		break;

	case DUMMY_CATEGORY_TWO:
		if (isRoot) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_TWO shouldn't be a root, and is\n");
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (DUMMY_CATEGORY_ONE != parentCategoryCode) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_TWO's parent should be DUMMY_CATEGORY_ONE, not %u\n", parentCategoryCode);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (categoryName != dummyCategoryTwo.name) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_TWO has the wrong name: %p=%s instead of %p=%s\n", categoryName, categoryName, dummyCategoryTwo.name, dummyCategoryTwo.name);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		state->dummyCategoryTwoWalked = TRUE;
		state->dummyCategoryTwoBytes = liveBytes;
		state->dummyCategoryTwoBlocks = liveAllocations;
		break;

	case DUMMY_CATEGORY_THREE:
		if (isRoot) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_THREE shouldn't be a root, and is\n");
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (DUMMY_CATEGORY_ONE != parentCategoryCode) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_THREE's parent should be DUMMY_CATEGORY_ONE, not %u\n", parentCategoryCode);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		if (categoryName != dummyCategoryThree.name) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "DUMMY_CATEGORY_THREE has the wrong name: %p=%s instead of %p=%s\n", categoryName, categoryName, dummyCategoryThree.name, dummyCategoryThree.name);
			state->otherError = TRUE;
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}

		state->dummyCategoryThreeWalked = TRUE;
		state->dummyCategoryThreeBytes = liveBytes;
		state->dummyCategoryThreeBlocks = liveAllocations;
		break;

	case OMRMEM_CATEGORY_PORT_LIBRARY:
		/* Root status changes depending on whether we're using the default category set or not. */
		state->portLibraryWalked = TRUE;
		state->portLibraryBytes = liveBytes;
		state->portLibraryBlocks = liveAllocations;

		break;

	case OMRMEM_CATEGORY_UNKNOWN:
		/* Root status changes depending on whether we're using the default category set or not. */
		state->unknownWalked = TRUE;
		state->unknownBytes = liveBytes;
		state->unknownBlocks = liveAllocations;
		break;

#if defined(OMR_ENV_DATA64)
	case OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS:
		state->unused32bitSlabWalked = TRUE;
		state->unused32bitSlabBytes = liveBytes;
		state->unused32bitSlabBlocks = liveAllocations;
		break;
#endif

	default:
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected categoryCode: %u\n", categoryCode);
		state->otherError = TRUE;
		return J9MEM_CATEGORIES_STOP_ITERATING;
	}

	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Utility method that calls omrmem_walk_categories and populates the CategoriesState structure
 */
static void
getCategoriesState(struct OMRPortLibrary *portLibrary, struct CategoriesState *state)
{
	OMRMemCategoryWalkState walkState;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	memset(state, 0, sizeof(struct CategoriesState));
	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.userData1 = portLibrary;
	walkState.userData2 = state;
	walkState.walkFunction = &categoryWalkFunction;

	omrmem_walk_categories(&walkState);
}

/**
 * Verifies that memory categories work with the omrmem_memory_allocate, reallocate and allocate32 APIs
 *
 * We test:
 *
 * - That unknown categories are mapped to unknown
 * - That each API increments and decrements the appropriate counter
 * - That the special OMRMEM_CATEGORY_PORT_LIBRARY works
 *
 * The tests for vmem, mmap and shmem test those APIs also manipulate the counters properly
 */
TEST(PortMemTest, mem_test8_categories)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	struct CategoriesState categoriesState;
	const char *testName = "omrmem_test8_categories";

	reportTestEntry(OMRPORTLIB, testName);

	/* Reset the categories */
	omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, 0);

	/* Check that walking the categories state only walks port library & unknown categories */
	getCategoriesState(OMRPORTLIB, &categoriesState);

	if (categoriesState.dummyCategoryOneWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "dummyCategoryOne unexpectedly walked\n");
		goto end;
	}
	if (categoriesState.dummyCategoryTwoWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "dummyCategoryTwo unexpectedly walked\n");
		goto end;
	}
	if (categoriesState.dummyCategoryThreeWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "dummyCategoryThree unexpectedly walked\n");
		goto end;
	}
	if (!categoriesState.portLibraryWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary unexpectedly not walked\n");
		goto end;
	}
	if (!categoriesState.unknownWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unknown unexpectedly not walked\n");
		goto end;
	}
#if defined(OMR_ENV_DATA64)
	if (!categoriesState.unused32bitSlabWalked) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unused32bitSlab unexpectedly not walked\n");
		goto end;
	}
#endif
	if (categoriesState.otherError) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
		goto end;
	}

	/* Try allocating under the port library category and check the block and byte counters are incremented */
	{
		uintptr_t initialBlocks;
		uintptr_t initialBytes;
		uintptr_t finalBlocks;
		uintptr_t finalBytes;
		uintptr_t expectedBlocks;
		uintptr_t expectedBytes;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocks = categoriesState.portLibraryBlocks;
		initialBytes = categoriesState.portLibraryBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory(32, OMRMEM_CATEGORY_PORT_LIBRARY);

		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 32;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.portLibraryBlocks;
		finalBytes = categoriesState.portLibraryBytes;

		omrmem_free_memory(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.portLibraryBlocks;
		finalBytes = categoriesState.portLibraryBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* Try allocating with a user category code - having not registered any categories. Check it maps to unknown */
	{
		uintptr_t initialBlocks;
		uintptr_t initialBytes;
		uintptr_t finalBlocks;
		uintptr_t finalBytes;
		uintptr_t expectedBlocks;
		uintptr_t expectedBytes;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocks = categoriesState.unknownBlocks;
		initialBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;

		omrmem_free_memory(ptr);
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* Register our dummy categories */
	omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, (uintptr_t) &dummyCategorySet);

	/* Try allocating to one of our user category codes - check the arithmetic is done correctly */
	{
		uintptr_t initialBlocks;
		uintptr_t initialBytes;
		uintptr_t finalBlocks;
		uintptr_t finalBytes;
		uintptr_t expectedBlocks;
		uintptr_t expectedBytes;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocks = categoriesState.dummyCategoryOneBlocks;
		initialBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;

		omrmem_free_memory(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}
		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

/* 64bit OSX does not allow memory below 4GB to be mapped. */
#if !(defined(OSX) && defined(OMR_ENV_DATA64))
	/* Try allocating with allocate32 */
	{
		uintptr_t initialBlocks;
		uintptr_t initialBytes;
		uintptr_t finalBlocks;
		uintptr_t finalBytes;
		uintptr_t expectedBlocks;
		uintptr_t expectedBytes;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocks = categoriesState.dummyCategoryOneBlocks;
		initialBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory32(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;

		omrmem_free_memory32(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}
		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

	/* On 64 bit, check that allocate32 manipulates the "unused 32 bit slab" category */
	/* n.b. we're reliant on previous tests having initialized the 32bithelpers. */
#if defined(OMR_ENV_DATA64)
	{
		uintptr_t initialBlocks;
		uintptr_t initialBytes;
		uintptr_t finalBlocks;
		uintptr_t finalBytes;
		uintptr_t expectedBlocks;
		uintptr_t expectedBytes;
		uintptr_t initialUnused32BitSlabBytes;
		uintptr_t initialUnused32BitSlabBlocks;
		uintptr_t minimumExpectedUnused32BitSlabBytes;
		uintptr_t finalUnused32BitSlabBytes;
		uintptr_t finalUnused32BitSlabBlocks;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocks = categoriesState.dummyCategoryOneBlocks;
		initialBytes = categoriesState.dummyCategoryOneBytes;
		initialUnused32BitSlabBytes = categoriesState.unused32bitSlabBytes;
		initialUnused32BitSlabBlocks = categoriesState.unused32bitSlabBlocks;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory32(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;
		/* Should have decremented by at least 64 bytes */
		minimumExpectedUnused32BitSlabBytes = initialUnused32BitSlabBytes - 64;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		finalUnused32BitSlabBytes = categoriesState.unused32bitSlabBytes;
		finalUnused32BitSlabBlocks = categoriesState.unused32bitSlabBlocks;

		omrmem_free_memory32(ptr);

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}
		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}
		/* Should have the same number of unused slab blocks */
		if (finalUnused32BitSlabBlocks != initialUnused32BitSlabBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Number of unused 32bit slab blocks should have stayed the same. Was %zu, now %zu\n", initialUnused32BitSlabBlocks, finalUnused32BitSlabBlocks);
			goto end;
		}

		if (finalUnused32BitSlabBytes > minimumExpectedUnused32BitSlabBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected value for unused 32 bit slab bytes. Expected %zu, was %zu. Initial value was %zu.\n", minimumExpectedUnused32BitSlabBytes, finalUnused32BitSlabBytes, initialUnused32BitSlabBytes);
			goto end;
		}

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.dummyCategoryOneBlocks;
		finalBytes = categoriesState.dummyCategoryOneBytes;
		finalUnused32BitSlabBytes = categoriesState.unused32bitSlabBytes;
		finalUnused32BitSlabBlocks = categoriesState.unused32bitSlabBlocks;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
		if (finalUnused32BitSlabBlocks != initialUnused32BitSlabBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of unused 32 bit slab blocks after free. Expected %zu, now %zu\n", initialUnused32BitSlabBlocks, finalUnused32BitSlabBlocks);
			goto end;
		}
		if (finalUnused32BitSlabBytes != initialUnused32BitSlabBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of unused 32 bit slab bytes after free. Expected %zu, now %zu\n", initialUnused32BitSlabBytes, finalUnused32BitSlabBytes);
			goto end;
		}
	}
#endif
#endif /* !(defined(OSX) && defined(OMR_ENV_DATA64)) */

	/* Try allocating and reallocating */
	{
		uintptr_t initialBlocksCat1;
		uintptr_t initialBytesCat1;
		uintptr_t initialBlocksCat2;
		uintptr_t initialBytesCat2;
		uintptr_t finalBlocksCat1;
		uintptr_t finalBytesCat1;
		uintptr_t finalBlocksCat2;
		uintptr_t finalBytesCat2;
		uintptr_t expectedBlocks1;
		uintptr_t expectedBytes1;
		uintptr_t expectedBlocks2;
		uintptr_t expectedBytes2;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		initialBytesCat1 = categoriesState.dummyCategoryOneBytes;
		initialBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		initialBytesCat2 = categoriesState.dummyCategoryTwoBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory(64, DUMMY_CATEGORY_ONE);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks1 = initialBlocksCat1 + 1;
		expectedBytes1 = initialBytesCat1 + 64;
		expectedBlocks2 = initialBlocksCat2;
		expectedBytes2 = initialBlocksCat2;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		finalBytesCat1 = categoriesState.dummyCategoryOneBytes;
		finalBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		finalBytesCat2 = categoriesState.dummyCategoryTwoBytes;

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
		}
		if (finalBlocksCat1 != expectedBlocks1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks1, finalBlocksCat1);
		}
		if (finalBytesCat1 < expectedBytes1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes1, finalBytesCat1);
		}
		if (finalBlocksCat2 != expectedBlocks2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks2, finalBlocksCat2);
		}
		if (finalBytesCat2 < expectedBytes2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes2, finalBytesCat2);
		}

		/* reallocate under a different category */

		ptr = omrmem_reallocate_memory(ptr, 128, DUMMY_CATEGORY_TWO);

		expectedBlocks1 = initialBlocksCat1;
		expectedBytes1 = initialBytesCat1;
		expectedBlocks2 = initialBlocksCat2 + 1;
		expectedBytes2 = initialBlocksCat2 + 128;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		finalBytesCat1 = categoriesState.dummyCategoryOneBytes;
		finalBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		finalBytesCat2 = categoriesState.dummyCategoryTwoBytes;

		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
		}
		if (finalBlocksCat1 != expectedBlocks1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks1, finalBlocksCat1);
		}
		if (finalBytesCat1 < expectedBytes1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes1, finalBytesCat1);
		}
		if (finalBlocksCat2 != expectedBlocks2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks2, finalBlocksCat2);
		}
		if (finalBytesCat2 < expectedBytes2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes2, finalBytesCat2);
		}

		omrmem_free_memory(ptr);

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocksCat1 = categoriesState.dummyCategoryOneBlocks;
		finalBytesCat1 = categoriesState.dummyCategoryOneBytes;
		finalBlocksCat2 = categoriesState.dummyCategoryTwoBlocks;
		finalBytesCat2 = categoriesState.dummyCategoryTwoBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocksCat1 != initialBlocksCat1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocksCat1, finalBlocksCat1);
			goto end;
		}
		if (finalBytesCat1 != initialBytesCat1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytesCat1, finalBytesCat1);
			goto end;
		}
		if (finalBlocksCat2 != initialBlocksCat2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocksCat2, finalBlocksCat2);
			goto end;
		}
		if (finalBytesCat2 != initialBytesCat2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytesCat2, finalBytesCat2);
			goto end;
		}
	}



	/* Try allocating to an unknown category code - check it maps to unknown properly */
	{
		uintptr_t initialBlocks;
		uintptr_t initialBytes;
		uintptr_t finalBlocks;
		uintptr_t finalBytes;
		uintptr_t expectedBlocks;
		uintptr_t expectedBytes;
		void *ptr;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		initialBlocks = categoriesState.unknownBlocks;
		initialBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		ptr = omrmem_allocate_memory(64, DUMMY_CATEGORY_THREE + 1);
		if (NULL == ptr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected native OOM\n");
			goto end;
		}

		expectedBlocks = initialBlocks + 1;
		expectedBytes = initialBytes + 64;

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;
		if (!categoriesState.unknownWalked) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Category walk didn't cover unknown category.\n");
			goto end;
		}
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		omrmem_free_memory(ptr);

		if (finalBlocks != expectedBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after allocate. Expected %zd, got %zd.\n", expectedBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes < expectedBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after allocate. Expected %zd, got %zd.\n", expectedBytes, finalBytes);
			goto end;
		}

		getCategoriesState(OMRPORTLIB, &categoriesState);
		finalBlocks = categoriesState.unknownBlocks;
		finalBytes = categoriesState.unknownBytes;
		if (categoriesState.otherError) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Some other error hit while walking categories (see messages above).\n");
			goto end;
		}

		if (finalBlocks != initialBlocks) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of blocks after free. Expected %zd, got %zd.\n", initialBlocks, finalBlocks);
			goto end;
		}
		if (finalBytes != initialBytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of bytes after free. Expected %zd, got %zd.\n", initialBytes, finalBytes);
			goto end;
		}
	}

end:
	/* Reset categories to NULL */
	omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, 0);

	reportTestExit(OMRPORTLIB, testName);
}

static uint32_t categoriesWalked;

/**
 * Category walk function that always returns STOP_ITERATING.
 *
 * Used with omrmem_test9_category_walk
 */
static uintptr_t
categoryWalkFunction2(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *walkState)
{
	categoriesWalked++;

	return J9MEM_CATEGORIES_STOP_ITERATING;
}

/*
 * Tests that the memory category walk can be stopped by returning J9MEM_CATEGORIES_STOP_ITERATING
 */
TEST(PortMemTest, mem_test9_category_walk)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmem_test9_category_walk";
	OMRMemCategoryWalkState walkState;

	reportTestEntry(OMRPORTLIB, testName);

	/* Register our dummy categories */
	omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, (uintptr_t) &dummyCategorySet);

	categoriesWalked = 0;

	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.walkFunction = &categoryWalkFunction2;

	omrmem_walk_categories(&walkState);

	if (categoriesWalked != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected number of categories walk. Expected 1, got %u\n", categoriesWalked);
	}

	/* Reset categories to NULL */
	omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, 0);

	reportTestExit(OMRPORTLIB, testName);
}

#if !(defined(OSX) && defined(OMR_ENV_DATA64))
/* attempt to free all mem pointers stored in memPtrs array with length */
static void
freeMemPointers(struct OMRPortLibrary *portLibrary, void **memPtrs, uintptr_t length)
{
	void *mem32Ptr = NULL;
	uintptr_t i;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	for (i = 0; i < length; i++) {
		mem32Ptr = memPtrs[i];
		omrmem_free_memory32(mem32Ptr);
	}
}
#endif /* !(defined(OSX) && defined(OMR_ENV_DATA64)) */
