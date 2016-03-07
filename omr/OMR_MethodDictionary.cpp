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
#include "OMR_MethodDictionary.hpp"

#include <stdint.h>
#include <string.h>

#include "ut_omrti.h"

omr_error_t
omr_ras_initMethodDictionary(OMR_VM *vm, size_t numProperties, const char * const *propertyNames)
{
	omr_error_t rc = OMR_ERROR_NONE;
	if (NULL == vm->_methodDictionary) {
		OMRPORT_ACCESS_FROM_OMRVM(vm);
		OMR_MethodDictionary *methodDictionary =
			(OMR_MethodDictionary *)omrmem_allocate_memory(sizeof(*methodDictionary), OMRMEM_CATEGORY_OMRTI);
		if (NULL != methodDictionary) {
			rc = methodDictionary->init(vm, numProperties, propertyNames);
			if (OMR_ERROR_NONE == rc) {
				vm->_methodDictionary = methodDictionary;
			} else {
				omrmem_free_memory(methodDictionary);
			}
		} else {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
	}
	return rc;
}

void
omr_ras_cleanupMethodDictionary(OMR_VM *vm)
{
	if (NULL != vm->_methodDictionary) {
		OMRPORT_ACCESS_FROM_OMRVM(vm);
		((OMR_MethodDictionary *)vm->_methodDictionary)->cleanup();
		omrmem_free_memory(vm->_methodDictionary);
		vm->_methodDictionary = NULL;
	}
}

omr_error_t
omr_ras_insertMethodDictionary(OMR_VM *vm, OMR_MethodDictionaryEntry *entry)
{
	omr_error_t rc = OMR_ERROR_NONE;
	if (NULL != vm->_methodDictionary) {
		rc = ((OMR_MethodDictionary *)vm->_methodDictionary)->insert(entry);
	}
	return rc;
}

omr_error_t
OMR_MethodDictionary::init(OMR_VM *vm, size_t numProperties, const char * const *propertyNames)
{
	_lock = NULL;
	_hashTable = NULL;
	_currentBytes = 0;
	_maxBytes = 0;
	_maxEntries = 0;
	_vm = vm;
	_numProperties = numProperties;
	_propertyNames = propertyNames;
	_sizeofEntry = (uint32_t)(sizeof(OMR_MethodDictionaryEntry) + (_numProperties * sizeof(char *)));

	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;
	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		OMRPORT_ACCESS_FROM_OMRVM(vm);
		_hashTable = hashTableNew(
			OMRPORTLIB, OMR_GET_CALLSITE(), 0, _sizeofEntry, 0, 0, OMRMEM_CATEGORY_OMRTI,
			entryHash, entryEquals, NULL, NULL);
		if (NULL != _hashTable) {
			if (0 != omrthread_monitor_init_with_name(&_lock, 0, "omrVM->_methodDictionary")) {
				rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
			}
		} else {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}

		if (OMR_ERROR_NONE != rc) {
			cleanup();
		}
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}
	return rc;
}

void
OMR_MethodDictionary::cleanup()
{
	if (NULL != _vm) {
		omrthread_t self = NULL;
		if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
			Trc_OMRPROF_methodDictionaryHighWaterMark(_maxBytes, _maxEntries, _sizeofEntry,
				_maxBytes - (_maxEntries * _sizeofEntry));
			if (NULL != _hashTable) {
				hashTableForEachDo(_hashTable, OMR_MethodDictionary::cleanupEntryStrings, this);
				hashTableFree(_hashTable);
				_hashTable = NULL;
			}
			if (NULL != _lock) {
				omrthread_monitor_destroy(_lock);
				_lock = NULL;
			}
			_vm = NULL;
			omrthread_detach(self);
		}
	}
}
void
OMR_MethodDictionary::getProperties(size_t *numProperties, const char *const **propertyNames, size_t *sizeofSampledMethodDesc) const
{
	*numProperties = _numProperties;
	*propertyNames = _propertyNames;
	*sizeofSampledMethodDesc = sizeof(OMR_SampledMethodDescription) + sizeof(const char *) * _numProperties;
}

omr_error_t
OMR_MethodDictionary::insert(OMR_MethodDictionaryEntry *entry)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;
	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		if (0 == omrthread_monitor_enter_using_threadId(_lock, self)) {
			OMR_MethodDictionaryEntry *newEntry =
				(OMR_MethodDictionaryEntry *)hashTableAdd(_hashTable, entry);
			if (NULL == newEntry) {
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			} else {
				bool reusedEntry = false;

				if (!entryValueEquals(newEntry, entry)) {
					/* There was an existing hash table entry with the same key.
					 * Overwrite its contents.
					 */
					traceInsertEntryReplace(newEntry);
					cleanupEntryStrings(newEntry, this);
					reusedEntry = true;
				}

				/* copy the entry strings */
				rc = dupEntryStrings(newEntry, entry);
				if (OMR_ERROR_NONE == rc) {
					traceInsertEntrySuccess(newEntry);
					if (!reusedEntry) {
						_currentBytes += _sizeofEntry;
					}
					_currentBytes += countEntryNameBytesNeeded(newEntry);
					if (_currentBytes > _maxBytes) {
						_maxBytes = _currentBytes;
						_maxEntries = hashTableGetCount(_hashTable);
					}
				}
			}
			omrthread_monitor_exit_using_threadId(_lock, self);
		} else {
			rc = OMR_ERROR_INTERNAL;
		}
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}
	if (OMR_ERROR_NONE != rc) {
		traceInsertEntryFailed(rc, entry);
	}
	return rc;
}

omr_error_t
OMR_MethodDictionary::getEntries(OMR_VMThread *vmThread, void **methodArray, size_t methodArrayCount,
	OMR_SampledMethodDescription *methodDescriptions, char *nameBuffer, size_t nameBytes,
	size_t *firstRetryMethod, size_t *nameBytesRemaining)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (0 == omrthread_monitor_enter(_lock)) {
		OMR_SampledMethodDescription *currentMthDesc = methodDescriptions;
		size_t firstRetryMethodLocal = 0;
		size_t nameBytesAvailable = nameBytes;
		size_t nameBytesRemainingLocal = 0;
		char *nameBufferPos = nameBuffer;

		/*
		 * Collect method descriptions until we encounter a description that can't fit into
		 * the available nameBuffer. At this point, flip to RETRY mode: Stop collecting names,
		 * and only count the remaining space required.
		 */

		for (size_t i = 0; i < methodArrayCount; i++) {
			OMR_MethodDictionaryEntry searchEntryHdr; /* NOTE This is only an entry header, and can't hold any propertyValues */
			OMR_MethodDictionaryEntry *entry = NULL;

			searchEntryHdr.key = methodArray[i];
			entry = (OMR_MethodDictionaryEntry *)hashTableFind(_hashTable, &searchEntryHdr);
			if (NULL == entry) {
				currentMthDesc->reasonCode = OMR_ERROR_NOT_AVAILABLE;
			} else {
				size_t nameBytesNeeded = countEntryNameBytesNeeded(entry);
				if (OMR_ERROR_RETRY == rc) {
					/* In RETRY mode. Only count the remaining space needed. */
					currentMthDesc->reasonCode = OMR_ERROR_RETRY;
					nameBytesRemainingLocal += nameBytesNeeded;

				} else if (nameBytesNeeded <= nameBytesAvailable) {
					/* The description fits in nameBuffer. */
					currentMthDesc->reasonCode = OMR_ERROR_NONE;
					copyEntryNameBytes(entry, currentMthDesc, nameBufferPos);
					nameBufferPos += nameBytesNeeded;
					nameBytesAvailable -= nameBytesNeeded;

					/* To minimize the size of the method dictionary,
					 * delete entries that are successfully retrieved.
					 */
					_currentBytes -= nameBytesNeeded;
					cleanupEntryStrings(entry, this);
					if (0 != hashTableRemove(_hashTable, entry)) {
						rc = OMR_ERROR_INTERNAL;
						currentMthDesc->reasonCode = OMR_ERROR_INTERNAL;
						if (NULL != firstRetryMethod) {
							*firstRetryMethod = i;
						}
						break;
					}
					_currentBytes -= _sizeofEntry;

				} else {
					/* Just ran out of space in nameBuffer. Flip to RETRY mode. */
					rc = OMR_ERROR_RETRY;
					firstRetryMethodLocal = i;
					currentMthDesc->reasonCode = OMR_ERROR_RETRY;
					nameBytesRemainingLocal += nameBytesNeeded;
				}
			}
			currentMthDesc = (OMR_SampledMethodDescription *)((uintptr_t)currentMthDesc + sizeof(OMR_SampledMethodDescription) + sizeof(const char *) * _numProperties);
		}
		if (OMR_ERROR_RETRY == rc) {
			if (NULL != firstRetryMethod) {
				*firstRetryMethod = firstRetryMethodLocal;
			}
			if (NULL != nameBytesRemaining) {
				*nameBytesRemaining = nameBytesRemainingLocal;
			}
		}
		omrthread_monitor_exit(_lock);
	} else {
		rc = OMR_ERROR_INTERNAL;
	}
	return rc;
}

size_t
OMR_MethodDictionary::countEntryNameBytesNeeded(OMR_MethodDictionaryEntry *entry) const
{
	size_t nameBytesNeeded = 0;
	for (size_t i = 0; i < _numProperties; ++i) {
		if (NULL != entry->propertyValues[i]) {
			nameBytesNeeded += strlen(entry->propertyValues[i]) + 1;
		}
	}
	return nameBytesNeeded;
}

void
OMR_MethodDictionary::copyEntryNameBytes(OMR_MethodDictionaryEntry *entry, OMR_SampledMethodDescription *desc, char *nameBufferPos) const
{
	for (size_t i = 0; i < _numProperties; ++i) {
		if (NULL == entry->propertyValues[i]) {
			desc->propertyValues[i] = NULL;
		} else {
			size_t len = strlen(entry->propertyValues[i]) + 1;
			memcpy((void *)nameBufferPos, entry->propertyValues[i], len);
			desc->propertyValues[i] = nameBufferPos;
			nameBufferPos += len;
		}
	}
}

/**
 * Calculate the hash value of a dictionary entry.
 *
 * Don't access any fields from the flexible array member of OMR_MethodDictionaryEntry.
 * They might not be allocated for the input entry.
 */
uintptr_t
OMR_MethodDictionary::entryHash(void *entry, void *userData)
{
	return (uintptr_t)((OMR_MethodDictionaryEntry *)entry)->key;
}

/**
 * Calculate entry equality.
 *
 * Don't access any fields from the flexible array member of OMR_MethodDictionaryEntry.
 * They might not be allocated for the input entries.
 *
 * @see OMR_MethodDictionary::entryValueEquals()
 */
uintptr_t
OMR_MethodDictionary::entryEquals(void *leftEntry, void *rightEntry, void *userData)
{
	OMR_MethodDictionaryEntry *lhs = (OMR_MethodDictionaryEntry *)leftEntry;
	OMR_MethodDictionaryEntry *rhs = (OMR_MethodDictionaryEntry *)rightEntry;
	return (lhs->key == rhs->key);
}

uintptr_t
OMR_MethodDictionary::cleanupEntryStrings(void *entry, void *userData)
{
	OMR_MethodDictionary *md = (OMR_MethodDictionary *)userData;
	OMR_MethodDictionaryEntry *mdEntry = (OMR_MethodDictionaryEntry *)entry;
	OMRPORT_ACCESS_FROM_OMRVM(md->_vm);
	for (size_t i = 0; i < md->_numProperties; ++i) {
		omrmem_free_memory((void *)mdEntry->propertyValues[i]);
		mdEntry->propertyValues[i] = NULL;
	}
	return FALSE; /* don't remove the entry */
}

omr_error_t
OMR_MethodDictionary::dupEntryStrings(OMR_MethodDictionaryEntry *dest, const OMR_MethodDictionaryEntry *src)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(_vm);
	for (size_t i = 0; i < _numProperties; i++) {
		if (NULL != src->propertyValues[i]) {
			size_t len = strlen(src->propertyValues[i]) + 1;
			void *copy = omrmem_allocate_memory(len, OMRMEM_CATEGORY_OMRTI);
			if (NULL != copy) {
				memcpy(copy, src->propertyValues[i], len);
				dest->propertyValues[i] = (const char *)copy;
			} else {
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
				dest->propertyValues[i] = NULL;

				/* Release previously allocated strings */
				for (size_t j = 0; j < i; j++) {
					omrmem_free_memory((void *)dest->propertyValues[j]);
					dest->propertyValues[j] = NULL;
				}
				break;
			}
		}
	}
	return rc;
}

/**
 * Calculate whether two entries have the same key and property pointers.
 *
 * @see OMR_MethodDictionary::entryEquals()
 */
bool
OMR_MethodDictionary::entryValueEquals(const OMR_MethodDictionaryEntry *e1, const OMR_MethodDictionaryEntry *e2)
{
	bool rc = (e1->key == e2->key);
	if (rc) {
		for (size_t i = 0; i < _numProperties; i++) {
			if (e1->propertyValues[i] != e2->propertyValues[i]) {
				rc = false;
				break;
			}
		}
	}
	return rc;
}

void
OMR_MethodDictionary::formatEntryProperties(const OMR_MethodDictionaryEntry *entry, char **str) const
{
	if (_numProperties == 0) {
		*str = NULL;
	} else {
		OMRPORT_ACCESS_FROM_OMRVM(_vm);
		size_t buflen = _numProperties; /* space between each property + nul-terminator, might include some extra if we have nulls */
		for (size_t i = 0; i < _numProperties; ++i) {
			if (NULL != entry->propertyValues[i]) {
				buflen += strlen(entry->propertyValues[i]);
			}
		}

		char *buf = (char *)omrmem_allocate_memory(buflen, OMRMEM_CATEGORY_OMRTI);
		uintptr_t bufUsed = 0;
		if (NULL != buf) {
			bufUsed += omrstr_printf(buf, (uintptr_t)buflen, "%s", entry->propertyValues[0]);
			for (size_t i = 1; i < _numProperties; ++i) {
				if (NULL != entry->propertyValues[i]) {
					bufUsed += omrstr_printf(buf + bufUsed, (uintptr_t)buflen - bufUsed, " %s", entry->propertyValues[i]);
				}
			}
		}
		*str = buf;
	}
}

void
OMR_MethodDictionary::traceInsertEntrySuccess(const OMR_MethodDictionaryEntry *newEntry) const
{
	if (TrcEnabled_Trc_OMRPROF_insertMethodDictionary_success) {
		OMRPORT_ACCESS_FROM_OMRVM(_vm);
		char *propertyStr = NULL;
		formatEntryProperties(newEntry, &propertyStr);
		if (NULL == propertyStr) {
			Trc_OMRPROF_insertMethodDictionary_success(newEntry->key, "");
		} else {
			Trc_OMRPROF_insertMethodDictionary_success(newEntry->key, propertyStr);
			omrmem_free_memory(propertyStr);
		}
	}
}

void
OMR_MethodDictionary::traceInsertEntryFailed(omr_error_t rc, const OMR_MethodDictionaryEntry *newEntry) const
{
	if (TrcEnabled_Trc_OMRPROF_insertMethodDictionary_failed) {
		OMRPORT_ACCESS_FROM_OMRVM(_vm);
		char *propertyStr = NULL;
		formatEntryProperties(newEntry, &propertyStr);
		if (NULL == propertyStr) {
			Trc_OMRPROF_insertMethodDictionary_failed(rc, newEntry->key, "");
		} else {
			Trc_OMRPROF_insertMethodDictionary_failed(rc, newEntry->key, propertyStr);
			omrmem_free_memory(propertyStr);
		}
	}
}

void
OMR_MethodDictionary::traceInsertEntryReplace(const OMR_MethodDictionaryEntry *newEntry) const
{
	if (TrcEnabled_Trc_OMRPROF_insertMethodDictionary_replaceExistingEntry) {
		OMRPORT_ACCESS_FROM_OMRVM(_vm);
		char *propertyStr = NULL;
		formatEntryProperties(newEntry, &propertyStr);
		if (NULL == propertyStr) {
			Trc_OMRPROF_insertMethodDictionary_replaceExistingEntry(newEntry->key, "");
		} else {
			Trc_OMRPROF_insertMethodDictionary_replaceExistingEntry(newEntry->key, propertyStr);
			omrmem_free_memory(propertyStr);
		}
	}
}

void
OMR_MethodDictionary::print()
{
	OMRPORT_ACCESS_FROM_OMRVM(_vm);
	omrtty_printf("OMR Method Dictionary\n");
	omrtty_printf("=====================\n");
	omrtty_printf("%016s %032s %032s %032s %10s\n", "key", "methodName", "className", "fileName", "lineNumber");
	hashTableForEachDo(_hashTable, OMR_MethodDictionary::printEntry, this);
}

uintptr_t
OMR_MethodDictionary::printEntry(void *entry, void *userData)
{
	OMR_MethodDictionary *md = (OMR_MethodDictionary *)userData;
	OMR_MethodDictionaryEntry *mdEntry = (OMR_MethodDictionaryEntry *)entry;
	OMRPORT_ACCESS_FROM_OMRVM(md->_vm);

	omrtty_printf("%016p", mdEntry->key);
	for (size_t i = 0; i < md->_numProperties; ++i) {
		if (mdEntry->propertyValues[i]) {
			omrtty_printf(" %s", mdEntry->propertyValues[i]);
		}
	}
	omrtty_printf("\n");
	return FALSE; /* don't remove the entry */
}
