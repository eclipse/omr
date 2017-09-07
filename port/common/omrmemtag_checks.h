/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
