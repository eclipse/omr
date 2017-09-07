/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/** 
 * Declares utility functions to be used with STL containers.
 */

#ifndef TR_STL_HPP
#define TR_STL_HPP

/**
 * @brief Compares two strings based on ASCII values of their characters.
 *        This function is intended to be used as a string comparator for
 *        STL containers.
 * @param s1 pointer to the first operand string
 * @param s2 pointer to the second operand string
 * @returns true if the string pointed to by s1 is 'less than' the one pointed to by s2
 */
bool str_comparator(const char *s1, const char *s2);

#endif
