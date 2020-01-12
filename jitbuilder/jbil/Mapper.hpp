/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef MAPPER_INCL
#define MAPPER_INCL


namespace OMR
{

namespace JitBuilder
{

template <typename T>
class Mapper
   {
   private:

   struct Element
      {
      Element(T *t, std::string name="", size_t offset=0)
         : _item(t)
         , _name(name)
         , _offset(offset)
         , _next(NULL)
         { }

      T *_item;
      std::string _name;
      size_t _offset;
      Element *_next;
      };

   public:
   Mapper() : _head(NULL), _tail(NULL), _current(NULL), _size(0) { }
   Mapper(T *t, std::string name="", size_t offset=0)
      : _head(NULL)
      , _tail(NULL)
      , _current(NULL)
      , _size(0)
      {
      add(t, name, offset);
      }

   size_t size()  { return _size; }
   void start()   { _current = _head; }
   void add(T *t, std::string name="", size_t offset=0)
      {
      Element *elem = new Element(t, name, offset);
      if (_head == NULL)
         {
         _head = elem;
         _current = _head;
         }
      if (_tail)
         _tail->_next = elem;
      _tail = elem;
      elem->_next = _head;
      _size++;
      }
   void clear()
      {
      if (_head == NULL) return;
      while (_head != _tail)
         {
         Element *next = _head->_next;
         delete _head;
         _head = next;
         }
      delete _head;
      _head = _tail = _current = NULL;
      _size = 0;
      }

   // add() adds a new element to the mapper
   // next() returns the next value from _current
   // start() should be called to ensure next() starts from the first element
   // clear() can be used to empty out the mapper

   // The list always wraps around so next() will always return an element once something
   // has been added to the Mapper.
   // Expected common scenarios:
   //   1) _items has several item; TypeReplacer calls next() as many times as size()
   //   2) _items has one item and TypeReplacer will call next() many times to reuse that item
   //          with different items returned by another Mapper (like "scalar" expansion)
   T *next()
      {
      if (!_current) return NULL;
      T *item = _current->_item;
      _current = _current->_next;
      return item;
      }
   T *current()
      {
      return _current->_item;
      }
   std::string name() // must be called *before* next() to get correct name
      {
      if (!_current) return std::string("");
      return _current->_name;
      }
   size_t offset() // must be called *before* next() to geet correct offset
      {
      if (!_current) return 0;
      return _current->_offset;
      }

   private:
   Element *_head;
   Element *_tail;
   Element *_current;
   size_t   _size;
   };

class Builder;
typedef Mapper<Builder> BuilderMapper;

class LiteralValue;
typedef Mapper<LiteralValue> LiteralMapper;

class Symbol;
typedef Mapper<Symbol> SymbolMapper;

class Type;
typedef Mapper<Type> TypeMapper;

class Value;
typedef Mapper<Value> ValueMapper;


} // namespace JitBuilder

} // namespace OMR

#endif // defined(MAPPER_INCL)
