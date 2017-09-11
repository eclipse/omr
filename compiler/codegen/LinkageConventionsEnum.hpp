/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef TR_LINKAGECONVENTION_INCL
#define TR_LINKAGECONVENTION_INCL

// Linkage conventions
//
// A "Linkage Convention" is the abstraction of one group of similar calling conventions;
// actual interpretation is up to the Front End / Code Generator combination.
// For example, by default, TR_System is interpreted as:
//     X86-32 Windows and Linux: cdecl calling convention
//     X86-64 Windows:           Microsoft x64 calling convention
//     X86-64 Linux:             System V AMD64 ABI
//

enum TR_LinkageConventions
   {
   #include "codegen/LinkageConventions.enum"
   TR_NumLinkages,

   // Force size to be at least 32-bit, as it may be cast to/from integers
   TR_LinkageForceSize = 0x7fffffff
   };

#endif
