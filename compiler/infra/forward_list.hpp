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

#ifndef FORWARD_LIST_HPP
#define FORWARD_LIST_HPP

#pragma once

#if 0
#include <memory>
#include <forward_list>

namespace TR {

/**
 * @class TR::forward_list
 *
 * @brief An extension of std::forward_list providing a linear-time size method.
 *
 * std::forward_list is a container for which the objective is to provide performance equal manually maintained
 * singly-linked list. A result of this  is that std::forward_list does not provide a size method, as it cannot
 * provide one that operates in O(1) time. However, there are a number of places in our code base that depend on
 * being able to request the size of a container, where the container is one targetted for re-implementation as
 * a forward_list. To reconcile these requirements, we provide an extension to std::forward_list that provides a
 * size() operation that runs in time linear to the number of objects stored in the container.
 *
 */

template < typename T, typename Alloc = std::allocator<T> >
class forward_list : public std::forward_list<T, Alloc>
   {
public:
   using typename std::forward_list<T, Alloc>::allocator_type;
   using typename std::forward_list<T, Alloc>::size_type;
   using typename std::forward_list<T, Alloc>::iterator;
   using typename std::forward_list<T, Alloc>::const_iterator;
   forward_list(const allocator_type &allocator) : std::forward_list<T, Alloc>(allocator) {}
   size_type size() const;
   };

}

template < typename T, typename Alloc >
auto TR::forward_list< T, Alloc >::size() const -> size_type
   {
   size_type size(0);
   for (auto it = this->begin(); it != this->end(); ++it)
      {
      ++size;
      }
   return size;
   }

#else
#include <list>
#include <iterator>

namespace TR {

namespace forward_list_private {

struct Link
   {
   explicit Link(Link *next) : _next(next) {}
   Link *_next;
   };

template <typename T>
struct ListElement : public Link
   {
   typedef T value_type;
   ListElement(Link *tail, const value_type &value) : Link(tail), _value(value) {}
   value_type _value;
   };

}

/**
 * @class TR::forward_list
 *
 * @brief A partial re-implementation of std::forward_list for C++03 supplemented by a linear-time size method.
 *
 * std::forward_list is a container introduced in the 2011 revision of the C++ standard for which
 * the objective is to provide the performance of a singly-linked list. Because the continer is singly-linked,
 * the interfaces common to other containers, where manipulations are performed on the half-closed interval
 * [begin, end), are problematic. To provide a work-around, std::forward_list provides similar operations that
 * operate on the open interval (before_begin, end), and provide [operation]_after variants that perform the
 * requested operation on the position immediately following the provided iterator.
 *
 * A further result of the strict requirement of being equally performant to a manually maintained singly-linked
 * list, is that std::forward_list does not provide a size method, as it cannot provide one that operates in O(1)
 * time. However, there are a number of places in our code base that depend on being able to request the size of
 * a container, where the container is one targetted for re-implementation as a forward_list. To reconcile these
 * requirements, we provide an extension to std::forward_list that provides a size() operation that runs in time
 * linear to the number of objects stored in the container.
 *
 */

template < typename T, typename Alloc >
class forward_list
   {
public:
   typedef Alloc allocator_type;
   typedef typename allocator_type::value_type value_type;
   typedef typename allocator_type::reference reference;
   typedef typename allocator_type::const_reference const_reference;
   typedef typename allocator_type::pointer pointer;
   typedef typename allocator_type::const_pointer const_pointer;
   typedef typename allocator_type::difference_type difference_type;
   typedef typename allocator_type::size_type size_type;

private:
   typedef forward_list_private::Link Link;

public:

   class iterator;
   class const_iterator
      {
   public:
      typedef forward_list container_type;
      typedef typename container_type::value_type value_type;
      typedef typename container_type::const_reference reference;
      typedef typename container_type::const_reference const_reference;
      typedef typename container_type::const_pointer pointer;
      typedef typename container_type::const_pointer const_pointer;
      typedef typename container_type::difference_type difference_type;
      typedef typename std::forward_iterator_tag iterator_category;

      const_iterator();
      const_iterator(const const_iterator &other);
      const_iterator &operator =(const const_iterator &other);
      const_iterator &operator ++();
      const_iterator operator ++(int);
      const_reference operator *() throw();
      const_pointer operator ->() throw();
      friend bool operator ==(const const_iterator &left, const const_iterator &right) { return left._element == right._element; }
      friend bool operator !=(const const_iterator &left, const const_iterator &right) { return !(left == right); }

   private:
      friend class forward_list;
      friend class container_type::iterator;

      static Link * degenerate();

      const_iterator(const Link *element);

      const Link * _element;
      };

   class iterator
      {
   public:
      typedef forward_list container_type;
      typedef typename container_type::value_type value_type;
      typedef typename container_type::reference reference;
      typedef typename container_type::const_reference const_reference;
      typedef typename container_type::pointer pointer;
      typedef typename container_type::const_pointer const_pointer;
      typedef typename container_type::difference_type difference_type;
      typedef typename std::forward_iterator_tag iterator_category;

      iterator();
      iterator(const iterator &other);
      iterator &operator =(const iterator &other);
      iterator &operator ++();
      iterator operator ++(int);
      reference operator *() throw();
      pointer operator ->() throw();
      friend bool operator ==(const iterator &left, const iterator &right) { return left._element == right._element; }
      friend bool operator !=(const iterator &left, const iterator &right) { return !(left == right); }
      operator const_iterator() { return const_iterator(_element); }

   private:
      friend class forward_list;

      static Link *degenerate();

      iterator(Link *element);
      Link *_element;
      };

   explicit forward_list(const allocator_type &allocator = allocator_type());
//   explicit forward_list(size_type size, const allocator_type &allocator = allocator_type());
//   forward_list(size_type size, const value_type& initialValue, const allocator_type &allocator = allocator_type());
   template < typename InputIterator > forward_list(InputIterator first, InputIterator last, const allocator_type &allocator = allocator_type());
   forward_list(const forward_list &prototype);
   forward_list(const forward_list &prototype, const allocator_type &allocator);
   ~forward_list();
   forward_list &operator =(const forward_list &prototype);

//   template < typename InputIterator > void assign(InputIterator first, InputIterator last);
//   void assign(size_type n, const value_type &value);

   allocator_type get_allocator() const throw();

   iterator before_begin();
   const_iterator before_begin() const;
   inline const_iterator cbefore_begin() const;

   iterator begin();
   const_iterator begin() const;
   inline const_iterator cbegin() const;

   iterator end();
   const_iterator end() const;
   inline const_iterator cend() const;

   bool empty() const throw();
   size_type size() const throw();
   size_type max_size() const throw();

   reference front();
   const_reference front() const;

   void push_front(const value_type &value);
   void pop_front();

   iterator insert_after(iterator position, const value_type& value);
//   iterator insert_after(const_iterator position, size_type count, const value_type& value);
   template < typename InputIterator > iterator insert_after(iterator position, InputIterator first, InputIterator last);

   iterator erase_after(iterator position);
//   iterator erase_after(const_iterator position, iterator last);

//   void resize(size_type newSize);
//   void resize(size_type newSize, const value_type &value);
   void clear() throw();

//   void splice_after(const_iterator position, forward_list &donor);
//   void splice_after(const_iterator position, forward_list &donor, const_iterator breakPoint);
//   void splice_after(const_iterator position, forward_list &donor, const_iterator nearBound, const_iterator farBound);

   void remove(const value_type &value);

//   void unique();

//   void merge(forward_list &donor);

private:

   typedef forward_list_private::ListElement<T> ListElement;
   typedef typename allocator_type::template rebind<ListElement>::other ListElementAllocator;

   Link _head;
   ListElementAllocator _allocator;
   };

}


template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::forward_list(const allocator_type &allocator) :
   _head(NULL),
   _allocator(allocator)
   {
   }

//template < typename T, typename Alloc >
//TR::forward_list< T, Alloc >::forward_list(size_type size, const allocator_type &allocator) :
//   _head(NULL),
//   _allocator(allocator)
//   {
//   }

//template < typename T, typename Alloc >
//TR::forward_list< T, Alloc >::forward_list(size_type size, const value_type &initialValue, const allocator_type &allocator) :
//   _head(NULL),
//   _allocator(allocator)
//   {
//   }

//template < typename T, typename Alloc >
//template < typename InputIterator >
//TR::forward_list< T, Alloc >::forward_list(InputIterator first, InputIterator last, const allocator_type &allocator) :
//   _head(NULL),
//   _allocator(allocator)
//   {
//   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::forward_list(const forward_list &prototype) :
   _head(NULL),
   _allocator(prototype._allocator)
   {
   insert_after(before_begin(), prototype.begin(), prototype.end());
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::forward_list(const forward_list &prototype, const allocator_type &allocator):
   _head(NULL),
   _allocator(allocator)
   {
   insert_after(before_begin(), prototype.begin(), prototype.end());
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::~forward_list()
   {
   clear();
   }

//template < typename T, typename Alloc >
//TR::forward_list< T, Alloc > &
//TR::forward_list< T, Alloc >::operator =(const forward_list &other)
//   {
//   }

//template < typename T, typename Alloc >
//template < typename InputIterator >
//void
//TR::forward_list< T, Alloc >::assign(InputIterator first, InputIterator last)
//   {
//   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::assign(size_type n, const value_type &value)
//   {
//   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::allocator_type
TR::forward_list< T, Alloc >::get_allocator() const throw()
   {
   return _allocator;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::before_begin()
   {
   return iterator(&_head);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::before_begin() const
   {
   return const_iterator(&_head);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::cbefore_begin() const
   {
   return const_cast<const forward_list &>(*this).before_begin();
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::begin()
   {
   return iterator(_head._next);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::begin() const
   {
   return const_iterator(_head._next);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::cbegin() const
   {
   return const_cast<const forward_list &>(*this).begin();
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::end()
   {
   return iterator(NULL);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::end() const
   {
   return const_iterator(NULL);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::cend() const
   {
   return const_cast<const forward_list &>(*this).end();
   }

template < typename T, typename Alloc >
bool
TR::forward_list< T, Alloc >::empty() const throw()
   {
   return _head._next == NULL;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::size_type
TR::forward_list< T, Alloc >::size() const throw()
   {
   size_type size(0);
   for (auto it = this->begin(); it != this->end(); ++it)
      {
      ++size;
      }
   return size;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::size_type
TR::forward_list< T, Alloc >::max_size() const throw()
   {
   return _allocator.max_size();
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::reference
TR::forward_list< T, Alloc >::front()
   {
   return static_cast<ListElement &>(*_head._next)._value;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_reference
TR::forward_list< T, Alloc >::front() const
   {
   return static_cast<const ListElement &>(*_head._next)._value;
   }

template < typename T, typename Alloc >
void
TR::forward_list< T, Alloc >::push_front(const value_type &value)
   {
   ListElement *newElement = _allocator.allocate(1);
   _allocator.construct(newElement, ListElement(_head._next, value));
   _head._next = newElement;
   }

template < typename T, typename Alloc >
void
TR::forward_list< T, Alloc >::pop_front()
   {
   Link *deadElement(_head._next);
   _head._next = deadElement->_next;
   _allocator.destroy(static_cast<ListElement *>(deadElement));
   _allocator.deallocate(static_cast<ListElement *>(deadElement), 1);
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::insert_after(iterator position, const value_type &value)
   {
   ListElement *newElement = _allocator.allocate(1);
   _allocator.construct(newElement, ListElement(position._element->_next, value));
   position.element->_next = newElement;
   }

//template < typename T, typename Alloc >
//typename TR::forward_list< T, Alloc >::iterator
//TR::forward_list< T, Alloc >::insert_after(const_iterator position, size_type count, const value_type &value)
//   {
//   }

template < typename T, typename Alloc >
template < typename InputIterator >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::insert_after(iterator position, InputIterator first, InputIterator last)
   {
   Link * const farBound = position._element->_next;

   try
      {
      Link * prev = position._element;
      for (auto currentPosition = first; currentPosition != last; ++currentPosition)
         {
         ListElement *newElement = _allocator.allocate(1);
         try
            {
            _allocator.construct(newElement, ListElement(farBound, *currentPosition));
            }
         catch (...)
            {
            _allocator.deallocate(newElement, 1);
            throw;
            }
         prev->_next = newElement;
         prev = newElement;
         }
      return iterator(prev);
      }
   catch (...)
      {
      while (position._element->_next != farBound)
         {
         erase_after(position);
         }
      throw;
      }
   return position;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::erase_after(iterator position)
   {
   Link *deadElement(position._element->_next);
   position._element->_next = deadElement->_next;
   _allocator.destroy(static_cast<ListElement *>(deadElement));
   _allocator.deallocate(static_cast<ListElement *>(deadElement), 1);
   return iterator(position._element->_next);
   }

//template < typename T, typename Alloc >
//typename TR::forward_list< T, Alloc >::iterator
//TR::forward_list< T, Alloc >::erase_after(const_iterator position, iterator last)
//   {
//   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::resize(size_type newSize)
//   {
//   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::resize(size_type newSize, const value_type &value)
//   {
//   }

template < typename T, typename Alloc >
void
TR::forward_list< T, Alloc >::clear() throw()
   {
   while (_head._next != NULL)
      {
      ListElement *deadElement = static_cast<ListElement *>(_head._next);
      _head._next = _head._next->_next;
      _allocator.destroy(deadElement);
      _allocator.deallocate(deadElement, 1);
      }
   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::splice_after(const_iterator position, forward_list &donor)
//   {
//   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::splice_after(const_iterator position, forward_list &donor, const_iterator breakPoint)
//   {
//   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::splice_after(const_iterator position, forward_list &donor, const_iterator nearBound, const_iterator farBound)
//   {
//   }

template < typename T, typename Alloc >
void
TR::forward_list< T, Alloc >::remove(const value_type &value)
   {
   for (auto preceding = before_begin(), current = begin(); current != end(); ++preceding, ++current)
      {
      while (*current == value)
         {
         current = erase_after(preceding);
         if (current == end())
            {
            return;
            }
         }
      }
   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::unique()
//   {
//   }

//template < typename T, typename Alloc >
//void
//TR::forward_list< T, Alloc >::merge(forward_list &donor)
//   {
//   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::Link *
TR::forward_list< T, Alloc >::const_iterator::degenerate()
   {
   return reinterpret_cast<Link *>(static_cast<uintptr_t>(-1));
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::const_iterator::const_iterator() :
   _element(degenerate())
   {
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::const_iterator::const_iterator(const const_iterator &other) :
   _element(other._element)
   {
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::const_iterator::const_iterator(const Link *element) :
   _element(element)
   {
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator &
TR::forward_list< T, Alloc >::const_iterator::operator =(const const_iterator &other)
   {
   _element = other._element;
   return *this;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator &
TR::forward_list< T, Alloc >::const_iterator::operator ++()
   {
   _element = _element->_next;
   return *this;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_iterator
TR::forward_list< T, Alloc >::const_iterator::operator ++(int)
   {
   const_iterator current(*this);
   _element = _element->_next;
   return current;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_reference
TR::forward_list< T, Alloc >::const_iterator::operator *() throw()
   {
   return static_cast<const ListElement &>(*_element)._value;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::const_pointer
TR::forward_list< T, Alloc >::const_iterator::operator ->() throw()
   {
   return &static_cast<const ListElement &>(*_element)._value;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::Link *
TR::forward_list< T, Alloc >::iterator::degenerate()
   {
   return reinterpret_cast<Link *>(static_cast<uintptr_t>(-1));
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::iterator::iterator() :
   _element(degenerate())
   {
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::iterator::iterator(const iterator &other) :
   _element(other._element)
   {
   }

template < typename T, typename Alloc >
TR::forward_list< T, Alloc >::iterator::iterator(Link *element) :
   _element(element)
   {
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator &
TR::forward_list< T, Alloc >::iterator::operator =(const iterator &other)
   {
   _element = other._element;
   return *this;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator &
TR::forward_list< T, Alloc >::iterator::operator ++()
   {
   _element = _element->_next;
   return *this;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::iterator
TR::forward_list< T, Alloc >::iterator::operator ++(int)
   {
   iterator current(*this);
   _element = _element->_next;
   return current;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::reference
TR::forward_list< T, Alloc >::iterator::operator *() throw()
   {
   return static_cast<ListElement &>(*_element)._value;
   }

template < typename T, typename Alloc >
typename TR::forward_list< T, Alloc >::pointer
TR::forward_list< T, Alloc >::iterator::operator ->() throw()
   {
   return &static_cast<ListElement &>(*_element)._value;
   }

#endif
#endif // FORWARD_LIST_HPP
