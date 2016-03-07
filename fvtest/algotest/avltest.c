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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "omrport.h"
#include "avl_api.h"


/* we need isspace on ce platforms */
#if !defined(isspace)
#define isspace(x) ( ((x)==' ')||((x)=='\t')||((x)=='\r')||((x)=='\n') )
#endif

#define AVL_TEST_MAX_LINELEN 4096

typedef struct J9AVLTestNode {
	J9WSRP leftChild;
	J9WSRP rightChild;
	int datum;
} J9AVLTestNode;

static BOOLEAN avlTestInsert(OMRPortLibrary *portlib, J9AVLTree *tree, uintptr_t val);
static intptr_t testInsertionComparator(J9AVLTree *tree, J9AVLTestNode *insertNode, J9AVLTestNode *walkNode);
static uintptr_t get_node_string(OMRPortLibrary *portlib, J9AVLTree *tree, J9AVLTestNode *walk, char *buffer, uintptr_t bufSize);
static void avlReset(J9AVLTree *tree);
static uintptr_t get_datum_string(OMRPortLibrary *portlib, J9AVLTestNode *walk, char *buffer, uintptr_t bufSize);
static void avl_get_string(OMRPortLibrary *portlib, J9AVLTree *tree, char *buffer, uintptr_t bufSize);
static intptr_t testSearchComparator(J9AVLTree *tree, intptr_t search, J9AVLTestNode *walkNode);
static void freeAVLTree(OMRPortLibrary *portlib, J9AVLTreeNode *currentNode);


int32_t verifyAVLTree(OMRPortLibrary *portLib, char *testListFile, uintptr_t *passCount, uintptr_t *failCount)
{
	J9AVLTree tree;
	char buf[AVL_TEST_MAX_LINELEN];
	char buffer[1024];
	intptr_t handle;
	int32_t line;
	char *result;
	char *title, *titleEnd;
	char *success, *successEnd;
	char *actionStr, *actionStrEnd;
	int32_t action;
	int remove;
	int value;

	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	handle = omrfile_open(testListFile, EsOpenRead, 0);

	if (handle == -1) {
		omrtty_printf("AVL TEST FAILED: Couldn't open '%s'\n", testListFile);
		return -1;
	}

	line = 0;
	while (NULL != (result = omrfile_read_text(handle, buf, AVL_TEST_MAX_LINELEN))) {
		line++;

		while (isspace(*result)) {
			result++;
		}
		/* ignore comments */
		if ((*result == '#') || (*result == 0)) {
			continue;
		}

		avlReset(&tree);

		title = result;
		titleEnd = strchr(result, ',');
		if (titleEnd == NULL) {
			goto malformed_input;
		}

		success = titleEnd + 1;
		while (isspace(*success)) {
			success++;
		}
		successEnd = strchr(success, ',');
		if (successEnd == NULL) {
			goto malformed_input;
		}

		actionStr = successEnd + 1;
		/* walking backwards is safe -- we will stop on the first , */
		while (isspace(successEnd[-1])) {
			successEnd--;
		}

		omrtty_printf("AVLTest: %.*s: ", titleEnd - title, title);

		for (;;) {
			/* advance past the whitespace, so we can accurately determine failure */
			while (isspace(*actionStr)) {
				actionStr++;
			}

			/* walk past the number */
			actionStrEnd = actionStr;
			if (*actionStrEnd == '-') {
				remove = 1;
				actionStrEnd++;
			} else {
				remove = 0;
			}

			action = 0;

			/* a big wad of ugly ebcidic safe code */
			for (;;) {
				switch (*actionStrEnd) {
				case '0':
					value = 0;
					break;
				case '1':
					value = 1;
					break;
				case '2':
					value = 2;
					break;
				case '3':
					value = 3;
					break;
				case '4':
					value = 4;
					break;
				case '5':
					value = 5;
					break;
				case '6':
					value = 6;
					break;
				case '7':
					value = 7;
					break;
				case '8':
					value = 8;
					break;
				case '9':
					value = 9;
					break;
				default:
					value = -1;
					break;
				}
				if (value < 0) {
					break;
				}
				action = action * 10 + value;
				actionStrEnd++;
			}

			if (action == 0) {
				/* -0 -garbage or garbage */
				if ((remove == 1) || (actionStrEnd == actionStr)) {
					goto malformed_input;
				}
				while (isspace(*actionStrEnd)) {
					actionStrEnd++;
				}
				if (*actionStrEnd != 0) {
					goto malformed_input;
				}

				avl_get_string(portLib, &tree, buffer, 1024);

				if ((!strncmp(buffer, success, successEnd - success)) && (buffer[successEnd - success] == 0)) {
					omrtty_printf(" PASSED\n");
					(*passCount)++;
				} else {
					omrtty_printf(" FAILED\n");
					omrtty_printf("\tExpected Result: %.*s\n", successEnd - success, success);
					omrtty_printf("\tResult: %s\n\n", buffer);
					(*failCount)++;
				}
				break;
			}

			actionStr = actionStrEnd;
			while (isspace(*actionStr)) {
				actionStr++;
			}
			if (*actionStr != ',') {
				goto malformed_input;
			}
			actionStr++;

			if (remove == 1) {
				J9AVLTreeNode *n = avl_search(&tree, action);

				if (n) {
					avl_delete(&tree, n);
					omrmem_free_memory(n);
				}
			} else {
				if (avlTestInsert(portLib, &tree, action) == FALSE) {
					goto fail;
				}
			}
		}
		freeAVLTree(portLib, tree.rootNode);
	}

	avlReset(&tree);
	omrfile_close(handle);
	return 0;
fail:
	freeAVLTree(portLib, tree.rootNode);
	omrfile_close(handle);
	return -1;
malformed_input:
	avlReset(&tree);
	omrtty_printf("AVL TEST FAILED: Invalid format on line %i:\n", line);
	omrtty_printf("%s\n", result);
	omrfile_close(handle);
	return -1;
}


static uintptr_t
get_datum_string(OMRPortLibrary *portlib, J9AVLTestNode *walk, char *buffer, uintptr_t bufSize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portlib);
	uintptr_t out = omrstr_printf(buffer, bufSize, "%i", walk->datum);
	if (out >= bufSize) {
		return bufSize;
	}
	return out;
}


static void avlReset(J9AVLTree *tree)
{
	memset(tree, 0, sizeof(J9AVLTree));
	tree->insertionComparator = (intptr_t ( *)(struct J9AVLTree *, J9AVLTreeNode *, J9AVLTreeNode *))testInsertionComparator;
	tree->searchComparator = (intptr_t ( *)(struct J9AVLTree *, uintptr_t, J9AVLTreeNode *))testSearchComparator;
}

static void freeAVLTree(OMRPortLibrary *portlib, J9AVLTreeNode *currentNode)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portlib);
	if (currentNode != NULL) {
		J9AVLTreeNode *leftChild = J9AVLTREENODE_LEFTCHILD(currentNode);
		J9AVLTreeNode *rightChild = J9AVLTREENODE_RIGHTCHILD(currentNode);

		freeAVLTree(portlib, leftChild);
		freeAVLTree(portlib, rightChild);
		omrmem_free_memory(currentNode);
	}
}


static BOOLEAN avlTestInsert(OMRPortLibrary *portlib, J9AVLTree *tree, uintptr_t val)
{
	J9AVLTestNode *node = NULL;
	J9AVLTestNode *t1;
	J9AVLTestNode *t2;

	OMRPORT_ACCESS_FROM_OMRPORT(portlib);

	node = omrmem_allocate_memory(sizeof(J9AVLTestNode), OMRMEM_CATEGORY_VM);
	if (!node) {
		omrtty_printf("out of memory");
		return FALSE;
	}

	memset(node, 0, sizeof(J9AVLTestNode));
	node->datum = (int)val;

	t1 = (J9AVLTestNode *)avl_search(tree, val);

	t2 = (J9AVLTestNode *)avl_insert(tree, (J9AVLTreeNode *)node);

	/* Node exist in AVLTree */
	if (t1 != NULL) {
		if (t2 == NULL) {
			omrtty_printf("insert failed\n");
			omrmem_free_memory(node);
			return FALSE;
		} else if (t2 == t1) {
			omrmem_free_memory(node);
			return TRUE;
		} else if (t2 != t1) {
			omrtty_printf("insert added, when it should have found\n");
			/* Might not be a good idea to free the memory since it is in AVLTree now */
			return FALSE;
		}
	} else {
		if (t2 == NULL) {
			omrtty_printf("insert failed\n");
			omrmem_free_memory(node);
			return FALSE;
		} else if (t2 != node) {
			omrtty_printf("insert found, when it should have added\n");
			omrmem_free_memory(node);
		}
	}
	return TRUE;
}


static intptr_t testSearchComparator(J9AVLTree *tree, intptr_t search, J9AVLTestNode *walkNode)
{
	return search - walkNode->datum;
}

static intptr_t testInsertionComparator(J9AVLTree *tree, J9AVLTestNode *insertNode, J9AVLTestNode *walkNode)
{
	return insertNode->datum - walkNode->datum;
}

static void avl_get_string(OMRPortLibrary *portlib, J9AVLTree *tree, char *buffer, uintptr_t bufSize)
{
	uintptr_t len = get_node_string(portlib, tree, (J9AVLTestNode *)tree->rootNode, buffer, bufSize);
	if (len >= bufSize) {
		buffer[bufSize - 1] = 0;
	} else {
		buffer[len] = 0;
	}
}

static uintptr_t get_node_string(OMRPortLibrary *portlib, J9AVLTree *tree, J9AVLTestNode *walk, char *buffer, uintptr_t bufSize)
{
	uintptr_t len;

	if (bufSize < 3) {
		return 0;
	}

	if (walk == 0) {
		buffer[0] = '@';
		return 1;
	}

	buffer[0] = '{';
	if (AVL_GETBALANCE(walk) == AVL_BALANCED) {
		buffer[1] = '=';
	} else if (AVL_GETBALANCE(walk) == AVL_LEFTHEAVY) {
		buffer[1] = 'L';
	} else {
		buffer[1] = 'R';
	}

	len = 2;
	len += get_datum_string(portlib, walk, &buffer[len], bufSize - len);
	len += get_node_string(portlib, tree, (J9AVLTestNode *)AVL_SRP_GETNODE(walk->leftChild), &buffer[len], bufSize - len);
	len += get_node_string(portlib, tree, (J9AVLTestNode *)AVL_SRP_GETNODE(walk->rightChild), &buffer[len], bufSize - len);

	if ((bufSize - len) > 1) {
		buffer[len++] = '}';
	}
	return len;
}





