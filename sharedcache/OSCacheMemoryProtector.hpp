/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#if !defined(OSCACHE_REGION_MEMORY_PROTECTOR_HPP_INCLUDED)
#define OSCACHE_REGION_MEMORY_PROTECTOR_HPP_INCLUDED

class OSCacheContiguousRegion;

class OSCacheMemoryProtector
{
public:
    /** 
     * @brief Set memory protections for OSCacheRegions
     * @return 0 if the operations has been successful, -1 if an error has occured
     */
    virtual IDATA omr_setPermissions(OSCacheContiguousRegion&) = 0;
};

#endif /* !defined(OSCACHE_REGION_MEMORY_PROTECTOR_HPP_INCLUDED) */
