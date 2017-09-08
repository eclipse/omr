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

#ifndef omravldefines_h
#define omravldefines_h

/*
 * @ddr_namespace: map_to_type=J9AVLConsts
 */

/* DO NOT DIRECTLY INCLUDE THIS FILE! */
/* Include avl_api.h instead */

#define AVL_BALANCEMASK 0x3
#define AVL_BALANCED 0
#define AVL_LEFTHEAVY 1
#define AVL_RIGHTHEAVY 2

#define AVL_GETNODE(x) ((J9AVLTreeNode *)((uintptr_t)(x) & (~(uintptr_t)AVL_BALANCEMASK)))
#define AVL_SETNODE(x, node) ((x) = (J9AVLTreeNode *)(((uintptr_t)(x) & AVL_BALANCEMASK) | (uintptr_t)(node)))

#define AVL_GETBALANCE(x) ((uintptr_t)((x)->leftChild) & (AVL_BALANCEMASK))
#define AVL_SETBALANCE(x, bal) (((x)->leftChild) = (J9WSRP)(((uintptr_t)((x)->leftChild) & (~(uintptr_t)AVL_BALANCEMASK)) | (bal)))

#define AVL_SRP_SETNODE(field, value) (field) = (J9WSRP)(((uintptr_t)field & AVL_BALANCEMASK) | ((value) ? (uintptr_t)(((uint8_t *)(value)) - (uint8_t *)&(field)) : 0))
#define AVL_NNSRP_SETNODE(field, value) (field) = (J9WSRP)(((uintptr_t)field & AVL_BALANCEMASK) | (uintptr_t)(((uint8_t *)(value)) - (uint8_t *)&(field)))

#define AVL_SRP_SET_TO_NULL(field) (field) = (J9WSRP)(((uintptr_t)field & AVL_BALANCEMASK))

#define AVL_SRP_PTR_SETNODE(ptr, value) AVL_SRP_SETNODE(*((J9WSRP *) (ptr)), (value))
#define AVL_NNSRP_PTR_SETNODE(ptr, value) AVL_NNSRP_SETNODE(*((J9WSRP *) (ptr)), (value))

#define AVL_SRP_GETNODE(node) ((J9AVLTreeNode *)(AVL_GETNODE(node) ? ((((uint8_t *) &(node)) + (J9WSRP)(AVL_GETNODE(node)))) : NULL))
#define AVL_NNSRP_GETNODE(node) ((J9AVLTreeNode *) (((uint8_t *) &(node)) + (J9WSRP)(AVL_GETNODE(node))))

#define J9AVLTREE_ACTION_INSERT  1
#define J9AVLTREE_ACTION_INSERT_EXISTS  2
#define J9AVLTREE_ACTION_REMOVE  3
#define J9AVLTREE_ACTION_REMOVE_NOT_IN_TREE  4
#define J9AVLTREE_ACTION_SINGLE_ROTATE  5
#define J9AVLTREE_ACTION_DOUBLE_ROTATE  6
#define J9AVLTREE_ACTION_REPLACE_REMOVED_PARENT  7

#define J9AVLTREE_IS_SHARED_TREE  1
#define J9AVLTREE_SHARED_TREE_INITIALIZED  2
#define J9AVLTREE_DISABLE_SHARED_TREE_UPDATES  4
#define J9AVLTREE_TEST_INTERNAVL  8
#define J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS  16

#endif /* omravldefines_h */
