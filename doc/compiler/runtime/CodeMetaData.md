!--
Copyright (c) 2016, 2017 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

# Code Metadata and Code Metadata Manager in Testarossa

## Introduction

This document explains and documents the concepts and implementation of runtime
code metadata and code metadata manager in Testarossa.

## Code Metadata

Code metadata contains runtime metadata information about a compilation unit. 
Metadata may include start address of allocated code memory and its size, 
start PC address of compiled code (when it is invoked from an interpreter 
or invoked from another piece of compiled code), end PC address of compiled code, 
compilation hotness level, list of runtime assumptions made during the 
compilation and any other information the compiler may find useful during 
runtime.

An instance of code metadata will be created for each compilation unit after
it is compiled. The code metadata instance will exist for the duration of the 
compilation unit's runtime until it is unloaded or invalidated. Storage and 
look up of code metadata instances are carried out by the code metadata manager.

In Testarossa, an instance of code metadata is uniquely identified by its start 
and end PC addresses. Two instances of code metadata with PC address ranges that 
overlap are not allowed.

## Code Metadata Manager

Code metadata manager is a singleton class created in the runtime to manage 
code metadata instances. The manager class constructor allocates an AVL tree 
in persistent memory to facilitate storage and look up of code metadata instances 
created by the compiler. Each tree node in the AVL tree is a hash table representing 
a piece of code cache segment. A node is created and added to the AVL tree when a 
new code cache segment is allocated. For J9, code cache segment size is configured 
to be 2MB. Entries in each hash table correspond to equal chunks of the associated 
code cache. For J9, each chunk is 512 bytes(?). A hash table entry points to a 
metadata pointer or a list of metadata pointers whose compiled code occupies a part 
of the chunk or the entire chunk of the code cache.

For efficient insertion, deletion, and look-up of a particular metadata pointer, 
please note that to add or remove nodes from the AVL tree, we will need exclusive 
VM access. However, to look up a metadata given a PC address in a hash table, we 
only need VM access; to remove a metadata from a hash table, we will need 
exclusive VM access.

When a metadata pointer is added with the start and end PC addresses of its 
compiled code, we need to do the following: 

1. Find out which code cache the PC address range belongs to, i.e. which node in 
the AVL tree contains the PC address range;
2. Calculate which chunks of the code cache correspond to the PC address 
range, i.e. what entries in the hash table will we update;
3. Add the metadata pointer to each hash table entry found in step #2. If 
the hash table entry has one metadata pointer, we will create a list and 
prepend the list with the new one and the existing metadata pointer (with its
low bit set) will terminate the list. If the entry is already a list of meta 
data pointers, we will either add the new metadata to the list if there is 
enough space or allocate free space to copy the list and add the new metadata.



