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

#include <cstdint>
#include "gtest/gtest.h"
#include "format.hpp"

#define ASSERT_ERROR(expr) ASSERT_THROW({ expr; }, std::runtime_error)

TEST(FormatTest, FormatString) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), ">>%s<<", "nejaky retezec");
   ASSERT_STREQ(buffer, ">>nejaky retezec<<");
}

TEST(FormatTest, FormatStringBad) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%s<<", (int)123));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%s<<", (unsigned)123));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%s<<", 1.3));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%s<<", &buffer));
}



TEST(FormatTest, FormatInt) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (int8_t)42);
   ASSERT_STREQ(buffer, ">>42<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (int16_t)-42);
   ASSERT_STREQ(buffer, ">>-42<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (int32_t)42);
   ASSERT_STREQ(buffer, ">>42<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (int64_t)-42);
   ASSERT_STREQ(buffer, ">>-42<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (uint8_t)255);
   ASSERT_STREQ(buffer, ">>-1<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (uint16_t)0xFFFE);
   ASSERT_STREQ(buffer, ">>-2<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (uint32_t)0xFFFFFFFD);
   ASSERT_STREQ(buffer, ">>-3<<");

   Tril::format(buffer, sizeof(buffer), ">>%d<<", (uint64_t)0xFFFFFFFFFFFFFFFC);
   ASSERT_STREQ(buffer, ">>-4<<");
}

TEST(FormatTest, FormatIntBad) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%d<<", "nejaky retezec"));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%d<<", 1.3));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%d<<", &buffer));
}

TEST(FormatTest, FormatUInt) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), ">>%u<<", (uint8_t)42);
   ASSERT_STREQ(buffer, ">>42<<");

   Tril::format(buffer, sizeof(buffer), ">>%u<<", (uint16_t)42);
   ASSERT_STREQ(buffer, ">>42<<");

   Tril::format(buffer, sizeof(buffer), ">>%u<<", (uint32_t)42);
   ASSERT_STREQ(buffer, ">>42<<");

   Tril::format(buffer, sizeof(buffer), ">>%u<<", (uint64_t)42);
   ASSERT_STREQ(buffer, ">>42<<");

   Tril::format(buffer, sizeof(buffer), ">>%u<<", (uint32_t)2147483646);
   ASSERT_STREQ(buffer, ">>2147483646<<");


}

TEST(FormatTest, FormatUIntBad) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%u<<", "nejaky retezec"));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%u<<", (int)123));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%u<<", 1.3));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%u<<", &buffer));
}

TEST(FormatTest, FormatFloatingPoint) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), ">>%f<<", (float)1.2);
   ASSERT_STREQ(buffer, ">>1.200000<<");

   Tril::format(buffer, sizeof(buffer), ">>%f<<", (double)1.2);
   ASSERT_STREQ(buffer, ">>1.200000<<");

}

TEST(FormatTest, FormatFloatingPointBad) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%f<<", "nejaky retezec"));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%f<<", (int)123));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%f<<", (unsigned)123));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%f<<", &buffer));
}

TEST(FormatTest, FormatPtr) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), ">>%X<<", (void*)0xCAFE);
   ASSERT_STREQ(buffer, ">>CAFE<<");
}

TEST(FormatTest, FormatPtrBad) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%X<<", (int)123));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%X<<", 1.3));
}

TEST(FormatTest, FormatBad) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), ">>%b<<", (int)123));
}

TEST(FormatTest, Percent) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), "%d%%", (int8_t)100);
   ASSERT_STREQ(buffer, "100%");

   Tril::format(buffer, sizeof(buffer), "%% (percent)");
   ASSERT_STREQ(buffer, "% (percent)");
}

TEST(FormatTest, Multiple) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), "None");
   ASSERT_STREQ(buffer, "None");

   Tril::format(buffer, sizeof(buffer), "%d, %d", 1, 2);
   ASSERT_STREQ(buffer, "1, 2");

   Tril::format(buffer, sizeof(buffer), "%d, %d, %d", 1, 2, 3);
   ASSERT_STREQ(buffer, "1, 2, 3");
}

TEST(FormatTest, NotEnough) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "%d"));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "%d, %d", 1));
}

TEST(FormatTest, TooMany) {
   char buffer[1024] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "None", 1));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "%d, %d", 1, 2, 3));
}

TEST(FormatTest, BufferTooSmall) {
   char buffer[2] = { '\0' };

   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "None"));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "%d", 12));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "%d", 1234));
   ASSERT_ERROR(Tril::format(buffer, sizeof(buffer), "%d%d", 123, 456));
}

TEST(FormatTest, IgnorePrefixes) {
   char buffer[1024] = { '\0' };

   Tril::format(buffer, sizeof(buffer), "%ld", 1);
   ASSERT_STREQ(buffer, "1");

   Tril::format(buffer, sizeof(buffer), "%lld", 1);
   ASSERT_STREQ(buffer, "1");

   Tril::format(buffer, sizeof(buffer), "%hd", 1);
   ASSERT_STREQ(buffer, "1");

   Tril::format(buffer, sizeof(buffer), "%hhd", 1);
   ASSERT_STREQ(buffer, "1");

   Tril::format(buffer, sizeof(buffer), "%jX", (char*)1);
   ASSERT_STREQ(buffer, "1");
}
