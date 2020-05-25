/*******************************************************************************
 *  Copyright (c) 2018, 2020 IBM and others
 *
 *  This program and the accompanying materials are made available under
 *  the terms of the Eclipse Public License 2.0 which accompanies this
 *  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 *  or the Apache License, Version 2.0 which accompanies this distribution and
 *  is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 *  This Source Code may also be made available under the following
 *  Secondary Licenses when the conditions for such availability set
 *  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 *  General Public License, version 2 with the GNU Classpath
 *  Exception [1] and GNU General Public License, version 2 with the
 *  OpenJDK Assembly Exception [2].
 *
 *  [1] https://www.gnu.org/software/classpath/license.html
 *  [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 *  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <OMR/IntrusiveList.hpp>
#include <gtest/gtest.h>
#include <ostream>

namespace OMR
{

struct LinkedInt;

typedef IntrusiveListNode<LinkedInt> LinkedIntListNode;

/// Forms a linked list of integers.
struct LinkedInt
{
	int value;
	LinkedIntListNode node;
};

template <>
struct IntrusiveListNodeAccessor<LinkedInt>
{
	static LinkedIntListNode& node(LinkedInt &i) { return i.node; }

	static const LinkedIntListNode& node(const LinkedInt &i)
	{
		return i.node;
	}
};

typedef IntrusiveList<LinkedInt>  LinkedIntList;

typedef LinkedIntList::Iterator LinkedIntListIterator;

typedef LinkedIntList::ConstIterator ConstLinkedIntListIterator;


// Custom gtest printers
template<typename T, typename A>
void PrintTo(const IntrusiveListConstIterator<T,A> &it, ::std::ostream *os){
	*os << it.current();
}

template<typename T, typename A>
void PrintTo(const IntrusiveListIterator<T,A> &it, ::std::ostream *os){
	*os << it.current();
}

TEST(TestIntrusiveList, Empty)
{
	LinkedIntList list;
	EXPECT_TRUE(list.empty());
	EXPECT_EQ(list.begin(), list.end());
	EXPECT_EQ(list.cbegin(), list.cend());
	for (LinkedIntListIterator iter = list.begin(); iter != list.end(); ++iter) {
		FAIL();
	}
}

TEST(TestIntrusiveList, ConvertIterToConstIter)
{
	LinkedIntList list;
	LinkedIntListIterator a = list.begin();
	ConstLinkedIntListIterator b = a;
	EXPECT_EQ(a, b);
	EXPECT_EQ(b, a);
}

TEST(TestIntrusiveList, AddOne)
{
	LinkedIntList list;
	LinkedInt a = {1};
	list.add(&a);
	EXPECT_FALSE(list.empty());
}

TEST(TestIntrusiveList, IterateOne)
{
	LinkedIntList list;

	LinkedInt a = {1};
	list.add(&a);

	LinkedIntList::Iterator iter = list.begin();
	EXPECT_NE(iter, list.end());
	EXPECT_EQ(iter->value, a.value);

	++iter;
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, IterateTwo)
{
	LinkedIntList list;

	LinkedInt a = {1};
	list.add(&a);

	LinkedInt b = {2};
	list.add(&b);

	LinkedIntList::Iterator iter = list.begin();
	EXPECT_EQ(iter->value, 2);

	++iter;
	EXPECT_EQ(iter->value, 1);

	++iter;
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, AddThenRemoveOne)
{
	LinkedIntList list;
	EXPECT_TRUE(list.empty());

	LinkedInt a = {1};
	list.add(&a);
	EXPECT_FALSE(list.empty());

	list.remove(&a);
	EXPECT_TRUE(list.empty());

	LinkedIntList::Iterator iter = list.begin();
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, AddTwoThenRemoveFirst)
{
	LinkedIntList list;
	LinkedInt a = {1};
	LinkedInt b = {2};

	list.add(&a);
	list.add(&b);
	list.remove(&a);

	LinkedIntList::Iterator iter = list.begin();
	EXPECT_NE(iter, list.end());
	EXPECT_EQ(iter->value, 2);
	++iter;
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, AddTwoThenRemoveSecond)
{
	LinkedIntList list;
	LinkedInt a = {1};
	LinkedInt b = {2};

	list.add(&a);
	list.add(&b);
	list.remove(&b);

	LinkedIntList::Iterator iter = list.begin();
	EXPECT_NE(iter, list.end());
	EXPECT_EQ(iter->value, 1);
	++iter;
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, AddTwoThenRemoveBoth)
{
	LinkedIntList list;
	LinkedInt a = {1};
	LinkedInt b = {2};

	list.add(&a);
	list.add(&b);
	list.remove(&a);
	list.remove(&b);

	EXPECT_TRUE(list.empty());
	EXPECT_EQ(list.begin(), list.end());
	for (LinkedIntListIterator iter = list.begin(); iter != list.end(); ++iter) {
		FAIL();
	}
}

TEST(TestIntrusiveList, AddTwoThenRemoveBothInReverse)
{
	LinkedIntList list;
	LinkedInt a = {1};
	LinkedInt b = {2};

	list.add(&a);
	list.add(&b);
	list.remove(&b);
	list.remove(&a);

	EXPECT_TRUE(list.empty());
	EXPECT_EQ(list.begin(), list.end());
	for (LinkedIntListIterator iter = list.begin(); iter != list.end(); ++iter) {
		FAIL();
	}
}

} // namespace OMR
