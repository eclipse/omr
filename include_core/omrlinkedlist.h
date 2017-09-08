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

#ifndef omrlinkedlist_h
#define omrlinkedlist_h

/* Circular doubly linked list macros */

#define J9_LINKED_LIST_ADD_AFTER(after, element) \
	do { \
		(element)->linkPrevious = (after); \
		((element)->linkNext = (after)->linkNext)->linkPrevious = (element); \
		(after)->linkNext = (element); \
	} while(0)

#define J9_LINKED_LIST_ADD_BEFORE(root, before, element) \
	do { \
		(element)->linkNext = (before); \
		((element)->linkPrevious = (before)->linkPrevious)->linkNext = (element); \
		(before)->linkPrevious = (element); \
		if ((before) == (root)) { \
			(root) = (element); \
		} \
	} while(0)

#define J9_LINKED_LIST_ADD_FIRST(root, element) \
	do { \
		if ((root) == NULL) { \
			(root) = (element)->linkNext = (element)->linkPrevious = (element); \
		} else { \
			J9_LINKED_LIST_ADD_BEFORE(root, root, element); \
		} \
	} while(0)

#define J9_LINKED_LIST_ADD_LAST(root, element) \
	do { \
		if ((root) == NULL) { \
			(root) = (element)->linkNext = (element)->linkPrevious = (element); \
		} else { \
			(element)->linkNext = (root); \
			((element)->linkPrevious = (root)->linkPrevious)->linkNext = (element); \
			(root)->linkPrevious = (element); \
		} \
	} while(0)

#define J9_LINKED_LIST_REMOVE(root, element) \
	do { \
		if ((element) == (root)) { \
			if ((element)->linkNext == (element)) { \
				(root) = NULL; \
			} else { \
				(root) = (element)->linkNext; \
			} \
		} \
		(element)->linkPrevious->linkNext = (element)->linkNext; \
		(element)->linkNext->linkPrevious = (element)->linkPrevious; \
	} while(0)

#define J9_LINKED_LIST_REMOVE_FIRST(root, element) \
	do { \
		(element) = (root); \
		if ((element)->linkNext == (element)) { \
			(root) = NULL; \
		} else { \
			(root) = (element)->linkNext; \
			(element)->linkPrevious->linkNext = (element)->linkNext; \
			(element)->linkNext->linkPrevious = (element)->linkPrevious; \
		} \
	} while(0)

#define J9_LINKED_LIST_IS_EMPTY(root) \
	((root) == NULL)

#define J9_LINKED_LIST_START_DO(root) \
	(root)

#define J9_LINKED_LIST_NEXT_DO(root, element) \
	((root) == (element)->linkNext ? NULL : (element)->linkNext)

#define J9_LINKED_LIST_REVERSE_START_DO(root) \
	(J9_LINKED_LIST_IS_EMPTY(root) ? NULL : (root)->linkPrevious)

#define J9_LINKED_LIST_REVERSE_NEXT_DO(root, element) \
	((root) == (element) ? NULL : (element)->linkPrevious)

/* Linear doubly linked list macros */
#define J9_LINEAR_LINKED_LIST_IS_EMPTY(root) \
	((root) == NULL)

#define J9_LINEAR_LINKED_LIST_IS_TAIL(linkNext, linkPrevious, root, element) \
	((element)->linkNext == NULL)

#define J9_LINEAR_LINKED_LIST_START_DO(root) \
	(root)

#define J9_LINEAR_LINKED_LIST_NEXT_DO(linkNext, linkPrevious, root, element) \
	((element)->linkNext)

#define J9_LINEAR_LINKED_LIST_ADD(linkNext, linkPrevious, root, element) \
	do { \
		if ((root) == NULL) { \
			(root) = (element); \
			(root)->linkNext = NULL; \
			(root)->linkPrevious = NULL; \
		} else { \
			J9_LINEAR_LINKED_LIST_ADD_BEFORE(linkNext, linkPrevious, root, root, element); \
		} \
	} while(0)

#define J9_LINEAR_LINKED_LIST_ADD_BEFORE(linkNext, linkPrevious, root, before, element) \
	do { \
		(element)->linkNext = (before); \
		if ((before) == (root)) { \
			(element)->linkPrevious = NULL; \
		} else { \
			((element)->linkPrevious = (before)->linkPrevious)->linkNext = (element); \
		} \
		(before)->linkPrevious = (element); \
		if ((before) == (root)) { \
			(root) = (element); \
		} \
	} while(0)

#define J9_LINEAR_LINKED_LIST_ADD_AFTER(linkNext, linkPrevious, root, after, element) \
	do { \
		(element)->linkPrevious = (after); \
		if ((after)->linkNext == NULL) { \
			(element)->linkNext = NULL; \
		} else {\
			((element)->linkNext = (after)->linkNext)->linkPrevious = (element); \
		} \
		(after)->linkNext = (element); \
	} while(0)

#define J9_LINEAR_LINKED_LIST_REMOVE(linkNext, linkPrevious, root, element) \
	do { \
		if ((element) == (root)) { \
			(root) = (element)->linkNext; \
		} else { \
			(element)->linkPrevious->linkNext = (element)->linkNext; \
		} \
		if ((element)->linkNext != NULL) { \
			(element)->linkNext->linkPrevious = (element)->linkPrevious; \
		}\
	} while(0)

#endif /* omrlinkedlist_h */
