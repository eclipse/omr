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

#ifndef omrmemtag_checks_h
#define omrmemtag_checks_h

#define ROUNDING_GRANULARITY	8
#define ROUNDED_FOOTER_OFFSET(number)	(((number) + (ROUNDING_GRANULARITY - 1) + sizeof(J9MemTag)) & ~(uintptr_t)(ROUNDING_GRANULARITY - 1))
#define ROUNDED_BYTE_AMOUNT(number)		(ROUNDED_FOOTER_OFFSET(number) + sizeof(J9MemTag))

uint32_t checkPadding(J9MemTag *tagAddress);
uint32_t checkTagSumCheck(J9MemTag *tagAddress, uint32_t eyeCatcher);
void *omrmem_get_memory_base(J9MemTag *headerEyecatcherAddress);
void *omrmem_get_footer_padding(J9MemTag *headerEyecatcherAddress);
J9MemTag *omrmem_get_footer_tag(J9MemTag *headerEyecatcherAddress);
J9MemTag *omrmem_get_header_tag(void *memoryPointer);

#endif /* omrmemtag_checks_h */
