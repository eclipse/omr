/*******************************************************************************
 *  Copyright (c) 2018, 2018 IBM and others
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

#if !defined(OMR_INTRUSIVELIST_HPP_)
#define OMR_INTRUSIVELIST_HPP_

#include <omrcfg.h>
#include <stddef.h>

namespace OMR
{

template <typename T>
class IntrusiveListNode;

template <typename T, typename Accessor>
class IntrusiveList;

template <typename T, typename Accessor>
class IntrusiveListIterator;

template <typename T, typename Accessor>
class IntrusiveListConstIterator;

/// Stores the next and previous pointers for elements in an IntrusiveList. Each element in an intrusive list must
/// contain an IntrusiveListNode<T>. The Node is obtained via an accessor class. See
template <typename T>
class IntrusiveListNode
{
public:
	IntrusiveListNode() : prev(NULL), next(NULL) {}

	IntrusiveListNode(T* p, T* n) : prev(p), next(n) {}

	/// assign previous, next to node.
	void assign(T* p, T* n)
	{
		prev = p;
		next = n;
	}

	// deactivate the node, and clear the next/prev pointers.
	void clear()
	{
		prev = NULL;
		next = NULL;
	}

	T* prev;
	T* next;
};

/// The IntrusiveListNodeAccessor is the default accessor used to obtain an InstrusiveListNode from a list element. This
/// template can be specialized to set the default accessor for a type.
template <typename T>
struct IntrusiveListNodeAccessor
{
	typedef IntrusiveListNode<T> Node;

	/// Obtain the IntrusiveListNode from an element. By default, calls element.node().
	static Node& node(T& element) { return element.node(); }

	/// Obtain a constant node from an element.
	static const Node& node(const T& element) { return element.node(); }
};

/// Simple bidirectional iterator for the elements in an intrusive list.
template <typename T, typename Accessor = IntrusiveListNodeAccessor<T> >
class IntrusiveListIterator
{
public:
	IntrusiveListIterator() : _current(NULL) {}

	explicit IntrusiveListIterator(T* root) : _current(root) {}

	IntrusiveListIterator(const IntrusiveListIterator<T, Accessor>& rhs)
	        : _current(rhs.current())
	{}

	T& operator*() const { return *_current; }

	T* operator->() const { return _current; }

	IntrusiveListIterator<T, Accessor>& operator++()
	{
		_current = Accessor::node(*_current).next;
		return *this;
	}

	IntrusiveListIterator<T, Accessor> operator++(int)
	{
		IntrusiveListIterator<T, Accessor> copy = *this;
		++this;
		return copy;
	}

	IntrusiveListIterator<T, Accessor>& operator--()
	{
		_current = Accessor::node(*_current).prev;
		return *this;
	}

	IntrusiveListIterator<T, Accessor> operator--(int)
	{
		IntrusiveListIterator<T, Accessor> copy = *this;
		_current = Accessor::node(*_current).prev;
		return copy;
	}

	bool operator==(const IntrusiveListIterator<T, Accessor>& rhs) const
	{
		return _current == rhs._current;
	}

	bool operator!=(const IntrusiveListIterator<T, Accessor>& rhs) const
	{
		return _current != rhs._current;
	}

	IntrusiveListIterator<T, Accessor>& operator=(const IntrusiveListIterator<T, Accessor>& rhs)
	{
		_current = rhs._current;
		return *this;
	}

	T* current() const { return _current; }

private:
	T* _current;
};

/// Simple bidirectional iterator for the elements in a constant intrusive list.
template <typename T, typename Accessor = IntrusiveListNodeAccessor<T> >
class IntrusiveListConstIterator
{
public:
	IntrusiveListConstIterator() : _current(NULL) {}

	explicit IntrusiveListConstIterator(T* root) : _current(root) {}

	IntrusiveListConstIterator(const IntrusiveListConstIterator<T, Accessor>& rhs)
	        : _current(rhs.current())
	{}

	IntrusiveListConstIterator(const IntrusiveListIterator<T, Accessor>& rhs)
	        : _current(rhs.current())
	{}

	const T& operator*() const { return *_current; }

	const T* operator->() const { return _current; }

	IntrusiveListConstIterator<T, Accessor>& operator++()
	{
		_current = Accessor::node(*_current).next;
		return *this;
	}

	IntrusiveListConstIterator<T, Accessor> operator++(int)
	{
		IntrusiveListConstIterator<T, Accessor> copy = *this;
		++this;
		return copy;
	}

	IntrusiveListConstIterator<T, Accessor>& operator--()
	{
		_current = Accessor::node(*_current).prev;
		return *this;
	}

	IntrusiveListConstIterator<T, Accessor> operator--(int)
	{
		IntrusiveListConstIterator<T, Accessor> copy = *this;
		_current = Accessor::node(*_current).prev;
		return copy;
	}

	bool operator==(const IntrusiveListIterator<T, Accessor>& rhs) const
	{
		return _current == rhs.current();
	}

	bool operator!=(const IntrusiveListIterator<T, Accessor>& rhs) const
	{
		return _current != rhs.current();
	}

	bool operator==(const IntrusiveListConstIterator<T, Accessor>& rhs) const
	{
		return _current == rhs._current;
	}

	bool operator!=(const IntrusiveListConstIterator<T, Accessor>& rhs) const
	{
		return _current != rhs._current;
	}

	IntrusiveListConstIterator<T, Accessor>&
	operator=(const IntrusiveListConstIterator<T, Accessor>& rhs)
	{
		_current = rhs._current;
		return *this;
	}

	const T* current() const { return _current; }

private:
	const T* _current;
};

template <typename T, typename A>
bool
operator==(const IntrusiveListIterator<T, A>& lhs, const IntrusiveListConstIterator<T, A>& rhs)
{
	return rhs.current() == lhs.current();
}

template <typename T, typename A>
bool
operator!=(const IntrusiveListIterator<T, A>& lhs, const IntrusiveListConstIterator<T, A>& rhs)
{
	return rhs.current() != lhs.current();
}

/// A doubly linked linear list, where the list node is embedded in the element.
///
/// To use an intrusive list, the element type T must store an IntrusiveListNode<T>. The default accessor will use T's
/// node() member-function to access the list node.
///
/// The Intrusive list provides two mechanisms for overriding the default node accessor:
///  1. Specialize the IntrusiveListNodeAccessor<T> template.
///  2. Override the Accessor template parameter in the list.
///
template <typename T, typename Accessor = IntrusiveListNodeAccessor<T> >
class IntrusiveList
{
public:
	typedef IntrusiveListNode<T> Node;

	typedef IntrusiveListIterator<T, Accessor> Iterator;

	typedef IntrusiveListConstIterator<T, Accessor> ConstIterator;

	IntrusiveList() : _root(NULL) {}

	/// Add element to the head of the list. Constant time.
	void add(T* element)
	{
		Accessor::node(*element).assign(NULL, _root);
		if (_root) {
			Accessor::node(*_root).prev = element;
		}
		_root = element;
	}

	/// Remove element from the list. Removing an element invalidates any iterators. Constant time.
	void remove(T* element)
	{
		Node& node = Accessor::node(*element);
		if (element == _root) {
			if (node.next != NULL) {
				Accessor::node(*node.next).prev = NULL;
				_root = node.next;
			} else {
				_root = NULL;
			}
		} else {
			Accessor::node(*node.prev).next = node.next;
			if (node.next != NULL) {
				Accessor::node(*node.next).prev = node.prev;
			}
		}
		node.clear();
	}

	Iterator begin() const { return Iterator(_root); }

	Iterator end() const { return Iterator(NULL); }

	ConstIterator cbegin() const { return ConstIterator(_root); }

	ConstIterator cend() const { return ConstIterator(NULL); }

	bool empty() const { return _root == NULL; }

private:
	T* _root;
};

} // namespace OMR

#endif // OMR_INTRUSIVELIST_HPP_
