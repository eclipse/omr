/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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
 ******************************************************************************/

#ifndef TR_TYPED_ALLOCATOR_HPP
#define TR_TYPED_ALLOCATOR_HPP

#pragma once

#undef min
#undef max

#include <cstddef>  // for ptrdiff_t, size_t
#include <limits>   // for numeric_limits
#include <new>      // for operator new

namespace TR
{

template <typename T, class Alloc>
class typed_allocator;

template <class Alloc>
class typed_allocator<void, Alloc>
   {

public:
   typedef Alloc untyped_allocator;

   typedef typename std::size_t size_type;
   typedef typename std::ptrdiff_t difference_type;

   typedef void value_type;

   typedef void * pointer;
   typedef void * const const_pointer;

   template <typename U> struct rebind { typedef typed_allocator<U, untyped_allocator> other; };

   template <typename T, typename U>
   friend bool operator == (const typed_allocator<T, untyped_allocator>& left, const typed_allocator<U, untyped_allocator>& right)
      {
      return left._backingAllocator == right._backingAllocator;
      }

   template <typename T, typename U>
   friend bool operator != (const typed_allocator<T, untyped_allocator>& left, const typed_allocator<U, untyped_allocator>& right)
      {
      return !(left == right);
      }

   untyped_allocator backingAllocator() { return _backingAllocator; }
protected:
   explicit typed_allocator( untyped_allocator backingAllocator ) throw() : _backingAllocator(backingAllocator) {}

   pointer allocate(size_type n, const_pointer hint)
      {
      return _backingAllocator.allocate(n);
      }

   value_type deallocate(pointer p, size_type n)
      {
      _backingAllocator.deallocate(p, n);
      }

private:
   untyped_allocator _backingAllocator;

   };

template <typename T, class Alloc>
class typed_allocator : public typed_allocator<void, Alloc>
   {
   public:

   typedef Alloc untyped_allocator;

   typedef T value_type;
   typedef std::size_t size_type;
   typedef std::ptrdiff_t difference_type;

   typedef T * pointer;
   typedef const T * const_pointer;

   typedef T & reference;
   typedef const T & const_reference;

   static pointer address(reference r) { return &r; }
   static const_pointer address(const_reference r) { return &r; }

   explicit typed_allocator(untyped_allocator backingAllocator) throw() :
      typed_allocator<void, untyped_allocator>(backingAllocator)
      {
      }

   template <typename U> typed_allocator(const typed_allocator<U, untyped_allocator> &other) :
      typed_allocator<void, untyped_allocator>(other)
      {
      }

   pointer allocate(size_type n, typename typed_allocator<void, untyped_allocator>::const_pointer hint = 0)
      {
      return static_cast<pointer>( typed_allocator<void, untyped_allocator>::allocate(n * sizeof(value_type), hint ) );
      }

   typename typed_allocator<void, untyped_allocator>::value_type deallocate(pointer p, size_type n) throw()
      {
      typed_allocator<void, untyped_allocator>::deallocate(p, n * sizeof(value_type));
      }

   static void construct(pointer p, const_reference prototype) { new(p) value_type(prototype); }
   static void destroy(pointer p) { p->~value_type(); }

   static size_type max_size() throw()
      {
      return std::numeric_limits<size_type>::max() / sizeof(value_type);
      }

   private:
   typed_allocator() throw(); /* delete */
   typed_allocator &operator =(const typed_allocator &); /* delete */
   };

}

#endif // TR_TYPED_ALLOCATOR_HPP
