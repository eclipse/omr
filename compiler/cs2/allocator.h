/*******************************************************************************
 * Copyright (c) 1996, 2021 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/***************************************************************************/
/*                                                                         */
/*  File name:  allocator.h                                                */
/*  Purpose:    Definition of the CS2 base allocator                       */
/*                                                                         */
/***************************************************************************/

#ifndef CS2_ALLOCATOR_H
#define CS2_ALLOCATOR_H

#include <stdio.h>
#include <memory.h>
#include "cs2/cs2.h"
#include "env/TypedAllocator.hpp"

namespace CS2 {

  // Basic CS2 allocator class
  // CS2 allocators are per-instance, and should not have any non-static
  // data. Only exception is that they may have a reference to an actual
  // memory pool. Copying an allocator only copies that reference and both
  // objects will share the same memory pool
  class malloc_allocator {
  public:
    void *allocate(size_t size) {
      return malloc(size);
    }
    void deallocate(void *pointer, size_t size) {
      free(pointer);
    }
    void *reallocate(size_t newsize, void *pointer, size_t size) {
      return realloc(pointer, newsize);
    }

  };

  template <size_t segmentsize = 65536, uint32_t segmentcount= 10, class base_allocator = ::CS2::malloc_allocator>
  class heap_allocator : private base_allocator {
  private:

    class Segment {
      Segment *next, *prev;
      void *freelist;
      uint32_t alloc;
      uint32_t freed;

    public:
      Segment(Segment *n) : next(n), prev(NULL), freelist(NULL), alloc(0), freed(0) {
        if (n) n->prev=this;
      }

      void *allocate(uint32_t index) {
        if (freelist) {
          void *ret = (void *)freelist;
          freelist = *(void **)ret;
          freed-=1;

          CS2Assert(holds_address(ret),
                    ("Found pointer outside of segment: %p", ret));
          return ret;
        }

        if (!is_full(index)) {
          void *ret = (char *)this + sizeof(Segment) + element_size(index)*alloc;
          alloc+=1;
          return ret;
        }
        return NULL;
      }
      void deallocate(void *pointer) {
        *(void **)pointer = freelist;
        freelist=pointer;
        freed+=1;
      }
      bool holds_address(void *pointer) {
        return (this <= pointer && pointer < ((char *)this)+segmentsize);
      }
      bool is_full(uint32_t index ) {
        return alloc == element_count(index);
      }
      bool is_empty() {
        return alloc == freed;
      }

      static uint32_t segment_index(size_t size) {
        if (size<=sizeof(void *)) return 1;
        if (size<= element_size(segmentcount-1)) {

          uint32_t halfsegment=segmentcount/2;

          if (size<=element_size(halfsegment)) {
            uint32_t i=2;
            for (; i<halfsegment; i++) {
              if (size<=element_size(i)) return i;
            }
            return i;
          } else {
            uint32_t i=halfsegment+1;
            for (; i<segmentcount-1; i++) {
              if (size<=element_size(i)) return i;
            }
            return i;
          }
        }
        return 0;
      }
      static size_t element_size(uint32_t index) {
        // Ensure allocation granule can hold a pointer for free list
        return size_t(sizeof(void *))<<(index-1);
      }
      static size_t element_count(uint32_t index) {
        return (segmentsize-sizeof(Segment)) / element_size(index);
      }
      Segment *next_segment() {
        return next;
      }

      size_t freelist_size(uint32_t index) {
        return freed*element_size(index);
      }

      uint32_t get_alloc(uint32_t index) { return alloc*element_size(index); }
      uint32_t get_free(uint32_t index)  { return (element_count(index)-alloc)*element_size(index);}

      Segment *move_to_head(Segment *head) {
        if (prev!=NULL) {              // already at the head?
          prev->next = next;           // unlink myself
          if (next) next->prev = prev;

          next=head;                // link at the head
          if (head)head->prev=this;
          prev=NULL;
        }
        return this;
      }
      Segment *unlink(Segment *head) {
        if (prev!=NULL) {              // already at the head?
          prev->next = next;           // unlink myself
          if (next) next->prev = prev;
          return head;
        } else {
          if (next) next->prev = NULL;
          return next;
        }
      }

      void *operator new (size_t, void *ptr) {
        return ptr;
      }
    };

    Segment *segments[segmentcount];
  public:
    heap_allocator(const base_allocator &a=base_allocator()) : base_allocator(a) {
      for (uint32_t i=0; i<segmentcount; i++)
        segments[i]=NULL;
    }
    heap_allocator(const heap_allocator &c) : base_allocator(c) {
      for (uint32_t i=0; i<segmentcount; i++)
        segments[i]=NULL;
    }

    ~heap_allocator() {
      for (uint32_t i=0; i<segmentcount; i++) {
        Segment *s = segments[i];
        while (s) {
          Segment *next = s->next_segment();
          base_allocator::deallocate(s, segmentsize);
          s = next;
        }
        segments[i]=NULL;
      }
    }

    Segment *new_segment(Segment *next) {
      void *ret = base_allocator::allocate(segmentsize);
      return new (ret) Segment(next);
    }

    void *allocate(size_t size) {
      uint32_t ix = Segment::segment_index(size);
      if (ix==0) {
        return base_allocator::allocate(size);
      }

      for (Segment *s = segments[ix]; s; s=s->next_segment()) {
        void *ret = s->allocate(ix);
        if (ret) {
          if (s!=segments[ix])
            segments[ix]= s->move_to_head(segments[ix]);
          return ret;
        }
      }
      segments[ix] = new_segment(segments[ix]);
      return segments[ix]->allocate(ix);
    }
    void deallocate(void *pointer, size_t size) {
      uint32_t ix = Segment::segment_index(size);
      if (ix==0) {
        return base_allocator::deallocate(pointer, size);
      }

      for (Segment *s = segments[ix]; s; s=s->next_segment()) {
        if (s->holds_address(pointer)) {
          s->deallocate(pointer);
          if (s->is_empty()) {
            segments[ix] = s->unlink(segments[ix]);
            base_allocator::deallocate(s, segmentsize);
          } else if (s!=segments[ix])
            segments[ix]= s->move_to_head(segments[ix]);
          return;
        }
      }
      CS2Assert(false, ("Could not find pointer to delete: %p", pointer));
    }
    void *reallocate(size_t newsize, void *pointer, size_t size) {
      uint32_t ix = Segment::segment_index(size);
      uint32_t nix = Segment::segment_index(newsize);

      if (ix==nix)  {
        if (ix==0) {
          return base_allocator::reallocate(newsize, pointer, size);
        }
        return pointer;
      }

      void * npointer = allocate(newsize);
      memcpy(npointer, pointer, newsize<size?newsize:size);
      deallocate(pointer, size);

      return npointer;
    }
  };

  template <class base_allocator>
  class shared_allocator {
    base_allocator &base;
  public:
    shared_allocator(base_allocator &b = base_allocator::instance()) : base(b) {}

    void *allocate(size_t size) {
        return base.allocate(size);
    }

    void deallocate(void *pointer, size_t size) {
      return base.deallocate(pointer, size);
    }

    void *reallocate(size_t newsize, void *pointer, size_t size) {
      return base.reallocate(newsize, pointer, size);
    }

    shared_allocator & operator = (const shared_allocator & a2 ) {
      // no need to copy the allocator being shared
      return *this;
    }

    // Enable automatic conversion into a form compatible with C++ standard library containers
    template <typename T>
    operator TR::typed_allocator<T, shared_allocator>() { return TR::typed_allocator<T, shared_allocator>(*this); }

    friend bool operator ==(const shared_allocator &left, const shared_allocator &right) { return &left.base == &right.base; }

    friend bool operator !=(const shared_allocator &left, const shared_allocator &right) { return !(operator ==(left, right)); }
  };

  template <size_t segmentsize = 65536, class base_allocator = ::CS2::malloc_allocator>
  class arena_allocator : private base_allocator {

    struct Segment {
      struct Segment *next;
      size_t size;
    };

  public:
    arena_allocator(base_allocator b = base_allocator() ) : base_allocator(b), segment(NULL), allocated(0) {}
    ~arena_allocator() {
      Segment *s = segment;

      while (s) {
        Segment *next = s->next;
        base_allocator::deallocate(s, s->size);
        s = next;
      }
    }

    static size_t arena_size() { return segmentsize - sizeof(Segment);}

    void *allocate(size_t size) {
      if (size % sizeof(size_t)) size = (size/sizeof(size_t)+1)*sizeof(size_t);

      void *ret;
      if (segment && size>=arena_size()) {
        Segment *new_segment = (Segment *)base_allocator::allocate(sizeof(Segment)+size);
        new_segment->size = sizeof(Segment)+size;
        new_segment->next = segment->next;
        segment->next=new_segment;

        ret = (void *)((char *)new_segment + sizeof(Segment));
      } else if (segment==NULL || allocated+size>arena_size()) {

        Segment *new_segment = (Segment *)base_allocator::allocate(segmentsize);
        new_segment->size = segmentsize;
        new_segment->next = segment;

        segment = new_segment;
        ret = (void*) ((char *)new_segment + sizeof(Segment));
        allocated = size;
      } else {
        ret = (void*) ((char *)segment + sizeof(Segment) + allocated);
        allocated+=size;
      }
      return ret;
    }

    void deallocate(void *pointer, size_t size) {
      // no deallocation
    }

    void *reallocate(size_t newsize, void *pointer, size_t size) {
      if (newsize<=size) return pointer;
      void *ret = allocate(newsize);
      memcpy(ret, pointer, size);
      return ret;
    }

    arena_allocator & operator = (const arena_allocator & a2 ) {
      // no need to copy the allocator being shared
      return *this;
    }

  private:
    Segment *segment;
    size_t allocated;
  };

}

#endif // CS2_ALLOCATOR_H
