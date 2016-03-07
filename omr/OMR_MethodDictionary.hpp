/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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
#if !defined(OMR_METHODDICTIONARY_HPP_INCLUDED)
#define OMR_METHODDICTIONARY_HPP_INCLUDED

#include "omr.h"
#include "omragent.h"
#include "omrprofiler.h"
#include "hashtable_api.h"
#include "thread_api.h"

class OMR_MethodDictionary
{
/*
 * Data members
 */
public:
protected:
private:
	omrthread_monitor_t _lock;
	J9HashTable *_hashTable;
	size_t _currentBytes; /* approx current byte size of the hash table */
	size_t _maxBytes; /* highest # of bytes */
	uint32_t _maxEntries; /* # of entries in the hash table when _maxBytes was achieved */
	OMR_VM *_vm;
	size_t _numProperties;
	const char * const *_propertyNames;
	uint32_t _sizeofEntry; /* size of a method dictionary entry in bytes, not including string data */

/*
 * Function members
 */
public:
	omr_error_t init(OMR_VM *vm, size_t numProperties, const char * const *propertyNames);
	void cleanup();
	omr_error_t insert(OMR_MethodDictionaryEntry *entry);
	omr_error_t getEntries(OMR_VMThread *vmThread, void **methodArray, size_t methodArrayCount,
		OMR_SampledMethodDescription *methodDescriptions, char *nameBuffer, size_t nameBytes,
		size_t *firstRetryMethod, size_t *nameBytesRemaining);
	void getProperties(size_t *numProperties, const char *const **propertyNames, size_t *sizeofSampledMethodDesc) const;
	void print();

protected:

private:
	bool entryValueEquals(const OMR_MethodDictionaryEntry *e1, const OMR_MethodDictionaryEntry *e2);
	omr_error_t dupEntryStrings(OMR_MethodDictionaryEntry *dest, const OMR_MethodDictionaryEntry *src);
	static uintptr_t cleanupEntryStrings(void *entry, void *userData);

	size_t countEntryNameBytesNeeded(OMR_MethodDictionaryEntry *entry) const;
	void copyEntryNameBytes(OMR_MethodDictionaryEntry *entry, OMR_SampledMethodDescription *desc, char *nameBufferPos) const;

	static uintptr_t entryHash(void *entry, void *userData);
	static uintptr_t entryEquals(void *leftEntry, void *rightEntry, void *userData);

	void formatEntryProperties(const OMR_MethodDictionaryEntry *entry, char **str) const;
	void traceInsertEntrySuccess(const OMR_MethodDictionaryEntry *newEntry) const;
	void traceInsertEntryFailed(omr_error_t rc, const OMR_MethodDictionaryEntry *newEntry) const;
	void traceInsertEntryReplace(const OMR_MethodDictionaryEntry *newEntry) const;
	static uintptr_t printEntry(void *entry, void *userData);
};

#endif /* defined(OMR_METHODDICTIONARY_HPP_INCLUDED) */
