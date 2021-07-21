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

#ifndef ITERATOR_INCL
#define ITERATOR_INCL

#include <cstdarg>
#include <vector>

namespace OMR
{

namespace JitBuilder
{

class Builder;
class Case;
class LiteralValue;
class Symbol;
class Type;
class Value;

template <class T>
class Iterator
   {
   public:
   Iterator<T>() // used to create an "end" iterator, _index must be -1 to match end of iteration
      : _index(-1)
      { }
   Iterator<T>(const Iterator<T> & other)
      : _index(other._index)
      {
      _items = other._items;
      }
   Iterator<T>(T * one)
      : _index(0)
      , _items(1)
      {
      _items[0] = one;
      }

   Iterator<T>(T * one, T * two)
      : _index(1)
      , _items(2)
      {
      _items[0] = one;
      _items[1] = two;
      }
   Iterator<T>(T * one, T * two, T * three)
      : _index(2)
      , _items(3)
      {
      _items[0] = one;
      _items[1] = two;
      _items[2] = three;
      }
   Iterator<T>(int numArgs, ...)
      : _index(numArgs-1)
      , _items(numArgs)
      {
      va_list(args);
      va_start(args, numArgs);
      for (int a=0;a < numArgs;a++)
         _items[a] = va_arg(args, T *);
      va_end(args);
      }
   Iterator<T>(T **array, int arraySize)
      : _index(arraySize-1)
      {
      _items.assign(array, array+arraySize);
      }
   Iterator<T>(std::vector<T *> v)
      : _index(v.size()-1)
      , _items(v)
      {
      }
   void prepend(Iterator<T> toPrepend)
      {
      std::vector<T *> v = toPrepend._items;
      _items.reserve(_items.size() + v.size());
      _items.insert(_items.begin(), v.begin(), v.end());
      _index = _items.size() - 1;
      }
   T * operator*()
      {
      return _items[_items.size()-1-_index];
      }

   T * operator++(int)
      {
      if (_index >= 0)
         {
         T * elem = _items[_items.size()-1-_index];
         _index--;
         return elem;
         }
      // at end, _index == -1
      return NULL;
      }
   bool operator!=(const Iterator<T> & other)
      {
      return !(*this == other);
      }
   bool operator==(const Iterator<T> & other)
      {
      return _index == other._index;
      }
   protected:
   std::vector<T *> _items;
   int              _index;
   };

typedef Iterator<Builder> BuilderIterator;
typedef Iterator<Case> CaseIterator; //std::vector<Case *>::iterator CaseIterator;
typedef Iterator<LiteralValue> LiteralIterator;
typedef Iterator<Symbol> SymbolIterator;
typedef Iterator<Type> TypeIterator;
typedef Iterator<Value> ValueIterator;

} // namespace JitBuilder

} // namespace OMR

#endif // defined(ITERATOR_INCL)
