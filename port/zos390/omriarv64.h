/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#if !defined(OMRIARV64_H_)
#define OMRIARV64_H_

/**
 * On ZOS64 memory allocation above 2G bar can be:
 *	- general: no limitation for OS for memory range
 * 	- 2_TO_32: an allocation in range between 2 and 32 GB, available if 2_TO_32 or 2_TO_64 is supported by OS
 *  - 2_TO_64: an allocation in range between 2 and 64 GB, available if 2_TO_64 is supported by OS
 * !! Do not modify these values without correspondent changes in getUserExtendedPrivateAreaMemoryType.s
 */
typedef enum UserExtendedPrivateAreaType {
	ZOS64_VMEM_ABOVE_BAR_GENERAL = 0,
	ZOS64_VMEM_2_TO_32G = 1,
	ZOS64_VMEM_2_TO_64G = 2
} UserExtendedPrivateAreaType;

#endif /* !defined(OMRIARV64_H_) */
