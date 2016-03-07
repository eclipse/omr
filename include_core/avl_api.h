/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef avl_api_h
#define avl_api_h

/**
* @file avl_api.h
* @brief Public API for the AVL module.
*
* This file contains public function prototypes and
* type definitions for the AVL module.
*
*/

#include "omrport.h"
#include "omravldefines.h"
#include "omravl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- avlsup.c ---------------- */

/**
* @brief
* @param *tree
* @param *nodeToDelete
* @return J9AVLTreeNode *
*/
J9AVLTreeNode *
avl_delete(J9AVLTree *tree, J9AVLTreeNode *nodeToDelete);


/**
* @brief
* @param *tree
* @param *nodeToInsert
* @return J9AVLTreeNode *
*/
J9AVLTreeNode *
avl_insert(J9AVLTree *tree, J9AVLTreeNode *nodeToInsert);


/**
* @brief
* @param *tree
* @param searchValue
* @return J9AVLTreeNode *
*/
J9AVLTreeNode *
avl_search(J9AVLTree *tree, uintptr_t searchValue);


#ifdef __cplusplus
}
#endif

#endif /* avl_api_h */
