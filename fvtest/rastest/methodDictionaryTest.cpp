/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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

#include "omr.h"
#include "omrprofiler.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "omrvm.h"

#include "rasTestHelpers.hpp"

static const size_t propertyCount = 2;
static const char *propertyNames[propertyCount] = { "Year", "Month" };

static const char *sstrstr(const char *haystack, const char *needle, size_t length);

/* Data structure that overlays OMR_SampledMethodDescription */
typedef struct Test_MethodDesc {
	omr_error_t reasonCode;
	const char *propertyValues[propertyCount];
} Test_MethodDesc;

class RASMethodDictionaryTest: public ::testing::TestWithParam<const char *>
{
protected:
	virtual void
	SetUp()
	{
		OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, rasTestEnv->getPortLibrary()));
		OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "methodDictionaryTest"));
		ti = omr_agent_getTI();
	}

	virtual void
	TearDown()
	{
		OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

		/* Now clear up the VM we started for this test case. */
		OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
	}

	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread;
	const OMR_TI *ti;
};

TEST_F(RASMethodDictionaryTest, Test0)
{
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initMethodDictionary(&testVM.omrVM, propertyCount, propertyNames));

	OMRPORT_ACCESS_FROM_OMRPORT(testVM.portLibrary);
	OMR_MethodDictionaryEntry *newEntry = (OMR_MethodDictionaryEntry *)omrmem_allocate_memory(
			sizeof(OMR_MethodDictionaryEntry) + sizeof(const char *) * propertyCount,
			OMRMEM_CATEGORY_OMRTI);
	ASSERT_NE((void *)NULL, newEntry);

	newEntry->key = (void *)0;
	newEntry->propertyValues[0] = "2000";
	newEntry->propertyValues[1] = "Dec";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	newEntry->key = (void *)1;
	newEntry->propertyValues[0] = "2001";
	newEntry->propertyValues[1] = "Jan";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	newEntry->key = (void *)2;
	newEntry->propertyValues[0] = "2002";
	newEntry->propertyValues[1] = "Feb";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	/* replace an existing entry */
	newEntry->key = (void *)1;
	newEntry->propertyValues[0] = "2003";
	newEntry->propertyValues[1] = "Mar";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	/* entry with no strings */
	newEntry->key = (void *)3;
	newEntry->propertyValues[0] = NULL;
	newEntry->propertyValues[1] = NULL;
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	/* done inserting into method dictionary */
	omrmem_free_memory((void *)newEntry);
	newEntry = NULL;

	size_t getPropertyCount = 0;
	const char *const *getPropertyNames = NULL;
	size_t getSizeof = 0;
	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodProperties(vmthread, &getPropertyCount, &getPropertyNames, &getSizeof));
	ASSERT_EQ(propertyCount, getPropertyCount);
	for (size_t i = 0; i < propertyCount; ++i) {
		ASSERT_STREQ(propertyNames[i], getPropertyNames[i]);
	}
	ASSERT_EQ(sizeof(Test_MethodDesc), getSizeof);
#if 0
	getPropertyNames[1] = "hello"; /* should not compile */
#endif

	/* ============================================================
	 * Start testing OMR_TI functions that are available to agents
	 * ============================================================
	 */

	const size_t numMethods = 5;
	Test_MethodDesc desc[numMethods];
	void *methodArray[numMethods] = { (void *)0, (void *)2, (void *)3, (void *)4, (void *)1 };
	size_t firstRetryMethod = 0;
	size_t nameBytesRemaining = 0;

	/* NULL name buffer. All available methods should be marked RETRY. */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_RETRY, ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, NULL, 0, &firstRetryMethod, &nameBytesRemaining));
	ASSERT_EQ((size_t)0, firstRetryMethod);
	ASSERT_LT((size_t)0, nameBytesRemaining);
	ASSERT_EQ(OMR_ERROR_RETRY, desc[0].reasonCode);
	ASSERT_EQ(OMR_ERROR_RETRY, desc[1].reasonCode);
	ASSERT_EQ(OMR_ERROR_RETRY, desc[2].reasonCode);
	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[3].reasonCode);
	ASSERT_EQ(OMR_ERROR_RETRY, desc[4].reasonCode);

	/* All available methods should be successfully retrieved. */
	char nameBuffer[50];
	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), &firstRetryMethod, &nameBytesRemaining));
	ASSERT_EQ(OMR_ERROR_NONE, desc[0].reasonCode);
	ASSERT_STREQ("2000", desc[0].propertyValues[0]);
	ASSERT_STREQ("Dec", desc[0].propertyValues[1]);

	ASSERT_EQ(OMR_ERROR_NONE, desc[1].reasonCode);
	ASSERT_STREQ("2002", desc[1].propertyValues[0]);
	ASSERT_STREQ("Feb", desc[1].propertyValues[1]);

	ASSERT_EQ(OMR_ERROR_NONE, desc[2].reasonCode);
	ASSERT_EQ((const char *)NULL, desc[2].propertyValues[0]);
	ASSERT_EQ((const char *)NULL, desc[2].propertyValues[1]);

	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[3].reasonCode);

	ASSERT_EQ(OMR_ERROR_NONE, desc[4].reasonCode);
	ASSERT_STREQ("2003", desc[4].propertyValues[0]);
	ASSERT_STREQ("Mar", desc[4].propertyValues[1]);

	/* On re-query, all methods already retrieved are no longer available */
	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), &firstRetryMethod, &nameBytesRemaining));
	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[0].reasonCode);
	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[1].reasonCode);
	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[2].reasonCode);
	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[3].reasonCode);
	ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[4].reasonCode);

	/* ============================================================
	 * End testing OMR_TI functions that are available to agents
	 * ============================================================
	 */

	omr_ras_cleanupMethodDictionary(&testVM.omrVM);

}

TEST_F(RASMethodDictionaryTest, TestNegativeCasesForGetMethodProperties)
{
	size_t getPropertyCount = 0;
	const char *const *getPropertyNames = NULL;
	size_t getSizeof = 0;

	/* call GetMethodProperties before MethodDictionary is enabled */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_NOT_AVAILABLE, ti->GetMethodProperties(vmthread, &getPropertyCount, &getPropertyNames, &getSizeof));

	/* init method dictionary */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initMethodDictionary(&testVM.omrVM, propertyCount, propertyNames));

	/* NULL getPropertyCount */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, ti->GetMethodProperties(vmthread, NULL, &getPropertyNames, &getSizeof));

	/* NULL getPropertyNames */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, ti->GetMethodProperties(vmthread, &getPropertyCount, NULL, &getSizeof));

	/* NULL getSizeof */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, ti->GetMethodProperties(vmthread, &getPropertyCount, &getPropertyNames, NULL));

	omr_ras_cleanupMethodDictionary(&testVM.omrVM);
}

TEST_F(RASMethodDictionaryTest, TestNegativeCasesForGetMethodDescriptions)
{
	size_t getPropertyCount = 0;
	const char *const *getPropertyNames = NULL;
	size_t getSizeof = 0;
	const size_t numMethods = 2;
	Test_MethodDesc desc[numMethods];
	void *methodArray[numMethods] = { (void *)0, (void *)1 };
	size_t firstRetryMethod = 0;
	size_t nameBytesRemaining = 0;
	char nameBuffer[27];
	char origBuffer[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	strcpy(nameBuffer, origBuffer);

	/* call GetMethodDescriptions before MethodDictionary is enabled */
	OMRTEST_ASSERT_ERROR(
		OMR_ERROR_NOT_AVAILABLE,
		ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), &firstRetryMethod, &nameBytesRemaining));

	/* init method dictionary */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initMethodDictionary(&testVM.omrVM, propertyCount, propertyNames));

	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodProperties(vmthread, &getPropertyCount, &getPropertyNames, &getSizeof));

	/* Test before insert any method dictionary */

	/*
	 * OMR_ERROR_NONE is expected as there is no method dictionary inserted
	 * nameBuffer should not be changed
	 */
	OMRTEST_ASSERT_ERROR_NONE(
		ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), &firstRetryMethod, &nameBytesRemaining));
	ASSERT_STREQ(origBuffer, nameBuffer);
	ASSERT_EQ((size_t)0, firstRetryMethod);
	ASSERT_EQ((size_t)0, nameBytesRemaining);
	for (size_t i = 0; i < numMethods; i++) {
		ASSERT_EQ(OMR_ERROR_NOT_AVAILABLE, desc[i].reasonCode);
	}

	OMRPORT_ACCESS_FROM_OMRPORT(testVM.portLibrary);
	OMR_MethodDictionaryEntry *newEntry = (OMR_MethodDictionaryEntry *)omrmem_allocate_memory(sizeof(OMR_MethodDictionaryEntry) + sizeof(const char *) * propertyCount, OMRMEM_CATEGORY_OMRTI);
	ASSERT_NE((void *)NULL, newEntry);

	newEntry->key = (void *)0;
	newEntry->propertyValues[0] = "2014";
	newEntry->propertyValues[1] = "Aug";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	newEntry->key = (void *)1;
	newEntry->propertyValues[0] = "2001";
	newEntry->propertyValues[1] = "Jan";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	/* done inserting into method dictionary */
	omrmem_free_memory((void *)newEntry);
	newEntry = NULL;

	/* Test after insert any method dictionary */

	/*
	 * zero nameBytes
	 */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_RETRY, ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, 0, &firstRetryMethod, &nameBytesRemaining));
	ASSERT_STREQ(origBuffer, nameBuffer);
	ASSERT_EQ((size_t)0, firstRetryMethod);
	ASSERT_EQ((size_t)18, nameBytesRemaining);
	for (size_t i = 0; i < numMethods; i++) {
		ASSERT_EQ(OMR_ERROR_RETRY, desc[i].reasonCode);
	}

	/* NULL nameBuffer and non-zero nameBytes */
	OMRTEST_ASSERT_ERROR(
		OMR_ERROR_ILLEGAL_ARGUMENT,
		ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, NULL, sizeof(nameBuffer), &firstRetryMethod, &nameBytesRemaining));

	/* zero methodArrayCount */
	OMRTEST_ASSERT_ERROR_NONE(
		ti->GetMethodDescriptions(vmthread, methodArray, (size_t)0, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), &firstRetryMethod, NULL));
	ASSERT_STREQ(origBuffer, nameBuffer);

	/* NULL methodArray */
	OMRTEST_ASSERT_ERROR(
		OMR_ERROR_ILLEGAL_ARGUMENT,
		ti->GetMethodDescriptions(vmthread, NULL, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), NULL, NULL));

	/* NULL methodDescriptions */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, ti->GetMethodDescriptions(vmthread, methodArray, numMethods, NULL, nameBuffer, sizeof(nameBuffer), &firstRetryMethod, &nameBytesRemaining));

	omr_ras_cleanupMethodDictionary(&testVM.omrVM);
}

TEST_F(RASMethodDictionaryTest, TestUndersizedNameBytes)
{
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initMethodDictionary(&testVM.omrVM, propertyCount, propertyNames));

	OMRPORT_ACCESS_FROM_OMRPORT(testVM.portLibrary);
	OMR_MethodDictionaryEntry *newEntry = (OMR_MethodDictionaryEntry *)omrmem_allocate_memory(sizeof(OMR_MethodDictionaryEntry) + sizeof(const char *) * propertyCount, OMRMEM_CATEGORY_OMRTI);
	ASSERT_NE((void *)NULL, newEntry);

	newEntry->key = (void *)0;
	newEntry->propertyValues[0] = "2000";
	newEntry->propertyValues[1] = "Dec";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	newEntry->key = (void *)1;
	newEntry->propertyValues[0] = "2014";
	newEntry->propertyValues[1] = "Aug";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	/* done inserting into method dictionary */
	omrmem_free_memory((void *)newEntry);
	newEntry = NULL;

	size_t getPropertyCount = 0;
	const char *const *getPropertyNames = NULL;
	size_t getSizeof = 0;
	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodProperties(vmthread, &getPropertyCount, &getPropertyNames, &getSizeof));
	ASSERT_EQ(propertyCount, getPropertyCount);
	for (size_t i = 0; i < propertyCount; ++i) {
		ASSERT_STREQ(propertyNames[i], getPropertyNames[i]);
	}
	ASSERT_EQ(sizeof(Test_MethodDesc), getSizeof);

	const size_t numMethods = 3;
	Test_MethodDesc desc[numMethods];
	void *methodArray[numMethods] = { (void *)1, (void *)0, (void *)2 };
	size_t firstRetryMethod = 0;
	size_t nameBytesRemaining = 0;
	char nameBuffer[27];
	char origBuffer[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	strcpy(nameBuffer, origBuffer);

	/* undersized nameBytes */
	size_t undersize = 10;
	strcpy(nameBuffer, origBuffer);
	OMRTEST_ASSERT_ERROR(
		OMR_ERROR_RETRY,
		ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, nameBuffer, undersize, &firstRetryMethod, &nameBytesRemaining));
	ASSERT_STRNE(origBuffer, nameBuffer);
	ASSERT_TRUE(sstrstr(nameBuffer, "2014", sizeof(nameBuffer)) != NULL);
	ASSERT_TRUE(sstrstr(nameBuffer, "Aug", sizeof(nameBuffer)) != NULL);
	ASSERT_TRUE(sstrstr(nameBuffer, "JKLMNOPQRSTUVWXYZ", sizeof(nameBuffer)) != NULL);
	ASSERT_EQ(size_t(1), firstRetryMethod);
	ASSERT_EQ(size_t(9), nameBytesRemaining);
	ASSERT_EQ(OMR_ERROR_NONE, desc[0].reasonCode);
	ASSERT_STREQ("2014", desc[0].propertyValues[0]);
	ASSERT_STREQ("Aug", desc[0].propertyValues[1]);
	ASSERT_EQ(OMR_ERROR_RETRY, desc[1].reasonCode);

	/* re-query using the nameBytesRemaining returned from above */
	char newNameBuffer[9];
	size_t newFirstRetryMethod = 0;
	size_t newNameBytesRemaining = 0;
	OMRTEST_ASSERT_ERROR_NONE(
		ti->GetMethodDescriptions(vmthread, methodArray, numMethods, (OMR_SampledMethodDescription *)desc, newNameBuffer, sizeof(newNameBuffer), &newFirstRetryMethod, &newNameBytesRemaining));
	ASSERT_TRUE(sstrstr(newNameBuffer, "2000", sizeof(nameBuffer)) != NULL);
	ASSERT_TRUE(sstrstr(newNameBuffer, "Dec", sizeof(nameBuffer)) != NULL);
	ASSERT_EQ(size_t(0), newFirstRetryMethod);
	ASSERT_EQ(size_t(0), newNameBytesRemaining);
	ASSERT_NE(OMR_ERROR_NONE, desc[0].reasonCode);
	ASSERT_EQ(OMR_ERROR_NONE, desc[1].reasonCode);
	ASSERT_STREQ("2000", desc[1].propertyValues[0]);
	ASSERT_STREQ("Dec", desc[1].propertyValues[1]);

	omr_ras_cleanupMethodDictionary(&testVM.omrVM);
}

TEST_F(RASMethodDictionaryTest, TestUndersizedMethodArrayCount)
{
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initMethodDictionary(&testVM.omrVM, propertyCount, propertyNames));

	OMRPORT_ACCESS_FROM_OMRPORT(testVM.portLibrary);
	OMR_MethodDictionaryEntry *newEntry = (OMR_MethodDictionaryEntry *)omrmem_allocate_memory(sizeof(OMR_MethodDictionaryEntry) + sizeof(const char *) * propertyCount, OMRMEM_CATEGORY_OMRTI);
	ASSERT_NE((void *)NULL, newEntry);

	newEntry->key = (void *)0;
	newEntry->propertyValues[0] = "2000";
	newEntry->propertyValues[1] = "Dec";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	newEntry->key = (void *)1;
	newEntry->propertyValues[0] = "2001";
	newEntry->propertyValues[1] = "Jan";
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_insertMethodDictionary(&testVM.omrVM, newEntry));

	/* done inserting into method dictionary */
	omrmem_free_memory((void *)newEntry);
	newEntry = NULL;

	size_t getPropertyCount = 0;
	const char *const *getPropertyNames = NULL;
	size_t getSizeof = 0;
	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodProperties(vmthread, &getPropertyCount, &getPropertyNames, &getSizeof));
	ASSERT_EQ(propertyCount, getPropertyCount);
	for (size_t i = 0; i < propertyCount; ++i) {
		ASSERT_STREQ(propertyNames[i], getPropertyNames[i]);
	}
	ASSERT_EQ(sizeof(Test_MethodDesc), getSizeof);

	const size_t numMethods = 2;
	Test_MethodDesc desc[numMethods];
	void *methodArray[numMethods] = { (void *)1, (void *)0 };
	char nameBuffer[27];
	char origBuffer[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	strcpy(nameBuffer, origBuffer);

	/* undersized methodArrayCount */
	OMRTEST_ASSERT_ERROR_NONE(ti->GetMethodDescriptions(vmthread, methodArray, numMethods - 1, (OMR_SampledMethodDescription *)desc, nameBuffer, sizeof(nameBuffer), NULL, NULL));
	ASSERT_STRNE(origBuffer, nameBuffer);
	ASSERT_TRUE(sstrstr(nameBuffer, "2001", sizeof(nameBuffer)) != NULL);
	ASSERT_TRUE(sstrstr(nameBuffer, "Jan", sizeof(nameBuffer)) != NULL);
	ASSERT_EQ(OMR_ERROR_NONE, desc[0].reasonCode);
	ASSERT_STREQ("2001", desc[0].propertyValues[0]);
	ASSERT_STREQ("Jan", desc[0].propertyValues[1]);

	omr_ras_cleanupMethodDictionary(&testVM.omrVM);
}

/*
 * Checks if needle is in the haystack
 * Return a pointer of the first occurrence of needle in the haystack, or NULL if haystack does not contain the needle.
 */
const char *
sstrstr(const char *haystack, const char *needle, size_t length)
{
	size_t needleLength = strlen(needle);
	size_t i;

	for (i = 0; i < length; i++) {
		if (i + needleLength > length) {
			return NULL;
		}
		if (strncmp(&haystack[i], needle, needleLength) == 0) {
			return &haystack[i];
		}
	}
	return NULL;
}
