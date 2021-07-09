/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#ifndef FORMAT_HPP
#define FORMAT_HPP

#include <cstdio>
#include <type_traits>
#include <stdexcept>

#include "omrformatconsts.h"

namespace Tril
{

template <typename In, typename Out>
inline Out reinterpret_as(const In value)
   {
   static_assert(sizeof(In) == sizeof(Out), "Both types must be of the same size");
   union {
         In in;
         Out out;
      } u;
      u.in = value;
   return u.out;
   }


template <typename U>
inline typename std::make_signed<U>::type reinterpret_as_signed(const U value)
   {
   typedef typename std::make_signed<U>::type S;
   return reinterpret_as<U, S>(value);
   }

template <typename T>
inline int64_t reinterpret_as_signed_int64(const T value)
   {
   throw std::runtime_error("Invalid parameter type");
   }

template <>
inline int64_t reinterpret_as_signed_int64<int8_t>(const int8_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<int16_t>(const int16_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<int32_t>(const int32_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<int64_t>(const int64_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<uint8_t>(const uint8_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<uint16_t>(const uint16_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<uint32_t>(const uint32_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

template <>
inline int64_t reinterpret_as_signed_int64<uint64_t>(const uint64_t value)
   {
   return static_cast<int64_t>(reinterpret_as_signed(value));
   }

inline void format(char* dst, size_t size, const char *fmt)
   {
   auto rem = size;
   for ( ; *fmt != '\0'; fmt++ )
      {
      if (*fmt == '%' && *++fmt != '%')
         {
         throw std::runtime_error("found conversion specifier but not enough value(s) given");
         }
      else
         {
         *dst = *fmt;
         dst++;
         rem--;
         }

      if (rem == 0)
         {
         throw std::runtime_error("buffer too small");
         }

      }
   *dst = '\0';
   }

template <typename V, typename... Rest>
inline void format(char* dst, size_t size, const char *fmt, V value, Rest... rest)
   {
   for ( ; *fmt != '\0'; fmt++ )
      {
      if (*fmt == '%')
         {
         fmt++;
         if (*fmt == '%')
            {
            *dst = '%';
            dst++;
            size--;
            }
         else
            {
            /*
             * Ignore l, ll, h, hh and j prefixes
             */
            if (*fmt == 'l')
               {
               fmt++;
               if (*fmt == 'l')
                  fmt++;
               }
            else if (*fmt == 'h')
               {
               fmt++;
               if (*fmt == 'h')
                  fmt++;
               }
            else if (*fmt == 'j')
               fmt++;

            size_t consumed = 0;
            switch (*fmt) {
            case 's':
               {
               if (!(std::is_same<const char *, V>::value || std::is_same<char *, V>::value))
                  throw std::runtime_error("%s conversion specified but given value is not of a string type");

               consumed = std::snprintf(dst, size, "%s", value);
               }
               break;

            case 'd':
               {
               if (! std::is_integral<V>::value)
                  throw std::runtime_error("%d conversion specified but given value is not of an integral type");

               /*
                * TODO: If an unsigned type is to be formated using %d, interpret the bits as
                * signed. This is to handle tests for unsigned ops that load data (typed in
                * test param tuple as unsigned) using "(iconst %d)" which takes signed value.
                * Hence the as_signed().
                *
                * This is questionable: (i) is it at all good idea to allow %d with unsigned type?
                * (ii) is all the machinery behind reinterpret_as_signed_int64() legit and/or necessary?
                */
               consumed = std::snprintf(dst, size, "%" OMR_PRId64, reinterpret_as_signed_int64(value));
               }
               break;

            case 'u':
               {
               if (!std::is_integral<V>::value)
                  throw std::runtime_error("%u conversion specified but given value is not of an integral type");
               if (!std::is_unsigned<V>::value)
                  throw std::runtime_error("%u conversion specified but given value is not of an unsigned type");

               consumed = std::snprintf(dst, size, "%" OMR_PRIu64, (uint64_t)value);
               }
               break;

            case 'X':
            case 'p':
               {
               if (!std::is_pointer<V>::value)
                  throw std::runtime_error("%X / %p conversion specified but given value is not of a pointer type");

               consumed = std::snprintf(dst, size, "%" OMR_PRIXPTR, (uintptr_t)value);
               }
               break;

            case 'f':
               {
               if (!std::is_floating_point<V>::value)
                  throw std::runtime_error("%f conversion specified but given value is not of a floating-point type");

               // FIXME: Handle NaN values on zOS and on Windows(see issue #5183 and #5324)
               consumed = std::snprintf(dst, size, "%f" , value);
               }
               break;

            default:
               throw std::runtime_error("unknown conversion specifier (only %%, %s, %d, %u, %X / %p and %f are supported, with optional j, l or ll prefix)");
            }

            if (consumed > size - 1)
               {
               throw std::runtime_error("buffer too small");
               }
            dst += consumed;
            size -= consumed;
            fmt++;
            format(dst, size, fmt, rest...);
            return;
            }
         }
      else
         {
         *dst = *fmt;
         dst++;
         size--;
         if (size < 0)
            {
            throw std::runtime_error("buffer too small");
            }
         }
      }
   throw std::runtime_error("no conversion specifier found but value(s) given");
   }
} // namespace Tril

#endif /* FORMAT_HPP */
