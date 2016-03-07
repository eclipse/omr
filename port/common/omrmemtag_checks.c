/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "omrport.h"
#include "omrmemtag_checks.h"

/**
 * Checks that the memory tag is not corrupt.
 *
 * @param[in] 	tagAddress 	the in-process or out-of-proces address of the header/footer memory tag
 * @param[in]	eyeCatcher	the eyecatcher corresponding to the memory tag
 *
 * @return 0 if the sum check is successful, non-zero otherwise
 */
uint32_t
checkTagSumCheck(J9MemTag *tagAddress, uint32_t eyeCatcher)
{
	uint32_t sum = 0;
	uint32_t *slots;
	uintptr_t i;
	J9MemTag *tagStoragePointer;

	tagStoragePointer = tagAddress;

	if (tagStoragePointer->eyeCatcher != eyeCatcher) {
		return U_32_MAX;
	}
	slots = (uint32_t *) tagStoragePointer;
	/* Could be unrolled into chained xors with a OMR_ENV_DATA64 conditional on the extra 2 uint32_ts */
	for (i = 0; i < (sizeof(J9MemTag) / sizeof(uint32_t)); i++) {
		sum ^= *slots++;
	}
#ifdef OMR_ENV_DATA64
	sum ^= ((uint32_t)(((uintptr_t)tagAddress) >> 32)) ^ ((uint32_t)(((uintptr_t)tagAddress) & U_32_MAX));
#else
	sum ^= (uint32_t) tagAddress;
#endif
	return sum;
}

/**
 * Checks that the padding associated with the memory block has not been corrupted
 *
 * @param[in]	headerTagAddress	the address of the header memory tag for the memory block.
 *
 * @return	0 if no corruption was detected, otherwise non-zero.
 */
uint32_t
checkPadding(J9MemTag *headerTagAddress)
{
	uint8_t *padding;

	padding = omrmem_get_footer_padding(headerTagAddress);

	while ((((uintptr_t) padding) & (ROUNDING_GRANULARITY - 1)) != 0) {
		if (*padding == J9MEMTAG_PADDING_BYTE) {
			padding++;
		} else {
			return J9MEMTAG_TAG_CORRUPTION;
		}
	}
	return 0;
}

/* Given the address of the headerEyecatcher for the memory block, return the memory pointer that
 * was returned by omrmem_allocate_memory() when the block was allocated.
 */
void *
omrmem_get_memory_base(J9MemTag *headerEyeCatcherAddress)
{
	return (uint8_t *)headerEyeCatcherAddress + sizeof(J9MemTag);
}

/**
 * Given the address of the headerEyecatcher for the memory block, return the address of the footer padding.
 *
 * Note that there not be any padding, in which case this returns the same as @ref omrmem_get_footer_tag(),
 * the address of the footer tag.
 *
 */
void *
omrmem_get_footer_padding(J9MemTag *headerEyeCatcherAddress)
{
	J9MemTag *headerTagStoragePointer;
	uintptr_t cursor;
	void *padding;

	headerTagStoragePointer = headerEyeCatcherAddress;

	cursor = (uintptr_t)((uint8_t *)headerEyeCatcherAddress + sizeof(J9MemTag));
	padding = (uint8_t *)(cursor + headerTagStoragePointer->allocSize);

	return padding;
}

/**
 * Given the address of the headerEyecatcher for the memory block, return the address of the corresponding footer tag.
 */
J9MemTag *
omrmem_get_footer_tag(J9MemTag *headerEyeCatcherAddress)
{
	J9MemTag *headerTagStoragePointer;

	headerTagStoragePointer = headerEyeCatcherAddress;

	return (J9MemTag *)((uint8_t *)headerEyeCatcherAddress + ROUNDED_FOOTER_OFFSET(headerTagStoragePointer->allocSize));
}

/**
 * Given the address returned by @ref omrmem_allocate_memory(), return address of the header tag for the memory block
 *
 */
J9MemTag *
omrmem_get_header_tag(void *memoryPointer)
{
	return (J9MemTag *)((uint8_t *)memoryPointer - sizeof(J9MemTag));
}

