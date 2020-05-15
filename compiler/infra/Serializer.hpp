/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <type_traits>
#include "infra/Assert.hpp"

/**
 * @class TR_Serializer
 * @brief Provides facility to write and read trivially copyable data types to and from a memory buffer.
 * When reading or writing, it maintains a pointer to the next location in the memory buffer where the data
 * has to be read from or written to.
 * It also has methods to compute the size of buffer required for writing data type.
 * This class can be helpful when there is a requirement to serialize and deserialize objects by writing
 * some specific fields of the class, or when the class is not trivially copyable.
 */
class TR_Serializer
{
   public:

   TR_Serializer() : _buffer(0), _size(0), _bufferEnd(0), _current(0) {}

   /**
    * @brief Constructor that sets the memory buffer and the size of buffer used by the serializer
    * @param [in] buffer pointer to memory buffer
    * @param [in] size size of buffer in bytes
    */
   TR_Serializer(uint8_t *buffer, uint32_t size) : _buffer(buffer), _size(size), _bufferEnd(buffer + size), _current(buffer) {}

   /**
    * @brief adds the size of the parameter to the buffer size
    * @tparam T type of the parameter
    * @param [in] t size of this parameter is added to the buffer size
    * @return void
    */
   template<typename T>
   void addSize(T t)
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot determine size of non-trivially copyable types");
      _size += sizeof(T);
      }

   /**
    * @brief adds the size of one or more parameters to the buffer size
    * @tparam T type of the first parameter
    * @tparam Args template parameter pack
    * @param [in] t size of this parameter is added to the buffer size
    * @param [in] args function parameter pack
    * @return void
    */
   template<typename T, typename... Args>
   void addSize(T t, Args... args)
      {
      addSize(t);
      addSize(args...);
      }

   /**
    * @brief adds the size of the array to the buffer size
    * @tparam T type of the array element
    * @param [in] t pointer to array
    * @param [in] count number of elements in the array
    * @return void
    */
   template<typename T>
   void addArraySize(T* t, uint32_t count)
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot serialize non-trivially copyable types");
      _size += (sizeof(T) * count);
      }

   /**
    * @brief writes a data to the memory buffer
    * @tparam T type of the parameter
    * @param [in] t data to be written to the memory buffer
    * @return void
    * @note increments the internal pointer to the memory buffer by the amount of bytes written
    */
   template<typename T>
   void write(T& t)
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot serialize non-trivially copyable types");
      TR_ASSERT_FATAL((_current + sizeof(T)) <= _bufferEnd, "Attempting to write beyond the buffer size, _current=%p, size=%zu, _bufferEnd=%p\n", _current, sizeof(T), _bufferEnd);
      memcpy(_current, &t, sizeof(T));
      _current += sizeof(T);
      }

   /**
    * @brief writes one or more data to the memory buffer
    * @tparam T type of the parameter
    * @tparam Args template parameter pack
    * @param [in] t data to be written to the memory buffer
    * @param [in] args function parameter pack
    * @return void
    * @note increments the internal pointer to the memory buffer by the amount of bytes written
    */
   template<typename T, typename... Args>
   void write(T& t, Args... args)
      {
      write(t);
      write(args...);
      }

   /**
    * @brief writes an array to the memory buffer
    * @tparam T type of the array element
    * @param [in] t pointer to the array to be written
    * @param [in] count number of elements in the array
    * @return void
    * @note increments the internal pointer to the memory buffer by the amount of bytes written
    */
   template<typename T>
   void writeArray(T* t, uint32_t count)
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot serialize non-trivially copyable types");
      uint32_t size = sizeof(T) * count;
      TR_ASSERT_FATAL((_current + size) <= _bufferEnd, "Attempting to write beyond the buffer size, _current=%p, size=%zu, _bufferEnd=%p\n", _current, size, _bufferEnd);
      memcpy(_current, t, size);
      _current += size;
      }

   /**
    * @brief reads data from the memory buffer
    * @tparam T type of the data to be read
    * @return returns a reference of type T obtained by typecasting current pointer
    *         in memory buffer to pointer to T
    * @note increments the internal pointer to the memory buffer by the amount of bytes read
    */
   template<typename T>
   T & read()
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot read non-trivially copyable types");
      TR_ASSERT_FATAL((_current + sizeof(T)) <= _bufferEnd, "Attempting to read beyond the buffer size, _current=%p, size=%zu, _bufferEnd=%p\n", _current, sizeof(T), _bufferEnd);
      uint8_t *ptr = _current;
      _current += sizeof(T);
      return *(reinterpret_cast<T*>(ptr));
      }

   /**
    * @brief reads data from the memory buffer
    * @tparam T type of the data to be read
    * @param [out] t reference of type T in which data is read
    * @return void
    * @note increments the internal pointer to the memory buffer by the amount of bytes read
    */
   template<typename T>
   void read(T& t)
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot read non-trivially copyable types");
      TR_ASSERT_FATAL((_current + sizeof(T)) <= _bufferEnd, "Attempting to read beyond the buffer size, _current=%p, size=%zu, _bufferEnd=%p\n", _current, sizeof(T), _bufferEnd);
      memcpy(&t, _current, sizeof(T));
      _current += sizeof(T);
      }

   /**
    * @brief reads one or more data from the memory buffer
    * @tparam T type of the data to be read
    * @tparam Args template parameter pack
    * @param [out] t reference of type T in which data is read
    * @param [out] args function parameter pack
    * @return void
    * @note increments the internal pointer to the memory buffer by the amount of bytes read
    */
   template<typename T, typename...Args>
   void read(T& t, Args...args)
      {
      read(t);
      read(args...);
      }

   /**
    * @brief reads array from the memory buffer
    * @tparam T type of the array element
    * @param [out] t pointer to the memory where the array is read to
    * @param [in] count number of elements in the array to be read
    * @return void
    * @note increments the internal pointer to the memory buffer by the amount of bytes read
    */
   template<typename T>
   void readArray(T* t, uint32_t count)
      {
      static_assert(std::is_trivially_copyable<T>::value, "Cannot read non-trivially copyable types");
      uint32_t size = (sizeof(T) * count);
      TR_ASSERT_FATAL((_current + size) <= _bufferEnd, "Attempting to read beyond the buffer size, _current=%p, size=%zu, _bufferEnd=%p\n", _current, size, _bufferEnd);
      memcpy(t, _current, size);
      _current += size;
      }

   /**
    * @brief reads data at the current location to check if it contains a non-NULL pointer
    * @return bool true if the data at current location is non-NULL, false otherwise.
    * @note this method increments the internal pointer to the memory buffer by size of pointer
    */
   bool nextDataNotNullPointer()
      {
      uintptr_t ptr = *(uintptr_t *)_current;
      TR_ASSERT_FATAL((_current + sizeof(ptr) <= _bufferEnd), "Attempting to read beyond the buffer size, _current=%p, size=%zu, _bufferEnd=%p\n", _current, sizeof(ptr), _bufferEnd);
      _current += sizeof(ptr);
      if (ptr)
         {
         return true;
	 }
      else
         {
         return false;
         }
      }

   /**
    * @brief computes the size of a non-trivially copyable type by calling its getSerializedSize() method
    * @tparam T type of the parameter
    * @param t reference of the type whose serialized size is to be calculated
    * @return void
    */
   template<typename T>
   void accumulateSize(T& t)
      {
      t.getSerializedSize(*this);
      }

   /**
    * @brief Set the memory buffer and size to be used by the serializer for reading/writing
    * @param [in] buffer the memory buffer
    * @param [in] size size of the memory buffer in bytes
    * @return void
    */
   void setMemoryBuffer(uint8_t *buffer, uint32_t size)
      {
      _buffer = buffer;
      _size = size;
      _bufferEnd = buffer + _size;
      _current = buffer;
      }

   /**
    * @brief returns the size of the memory buffer
    * @return uint32_t size of the memory buffer
    */
   uint32_t getSize() { return _size; }

   /**
    * @brief clears the internal fields preparing this object for next (de-)serialization operation
    * @return void
    */
   void reset()
      {
      _buffer = NULL;
      _current = NULL;
      _bufferEnd = NULL;
      _size = 0;
      }

   private:

   uint8_t *_buffer;
   uint32_t _size;
   uint8_t *_bufferEnd;
   uint8_t *_current;
};

#endif
