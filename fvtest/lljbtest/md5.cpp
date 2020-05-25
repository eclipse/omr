/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * https://www.ietf.org/rfc/rfc1321.txt
 */

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <time.h>

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

void MD5MemCpy (unsigned char * output, unsigned char * input, uint32_t len)
   {
   for (int i = 0; i < len; i++)
      output[i] = input[i];
   }

void MD5MemSet (unsigned char * output, int value, uint32_t len)
   {
   for (int i = 0; i < len; i++)
      ((char *)output)[i] = (char)value;
   }

uint32_t FFTransform(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
   {
   uint32_t result = a + ((b & c) | (~b & d)) + x + ac;
   result = (result << s) | (result >> (32 - s));
   result += result + b;
   return result;
   }

uint32_t GGTransform(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac){
  uint32_t result = a + ((b & d) | (c & (~d))) + x + ac;
  result = (result << s) | (result >> (32 - s));
  result += result + b;
  return result;
}

uint32_t HHTransform(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
   {
   uint32_t result = a + (b ^ c ^ d) + x + ac;
   result = (result << s) | (result >> (32 - s));
   result += result + b;
   return result;
   }

uint32_t IITransform(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
   {
   uint32_t result = a + (c ^ (b | (~d))) + x + ac;
   result = (result << s) | (result >> (32 - s));
   result += result + b;
   return result;
   }

void MD5Init(uint32_t  * state, uint32_t  * count)
   {
   state[0] = 0x67452301;
   state[1] = 0xefcdab89;
   state[2] = 0x98badcfe;
   state[3] = 0x10325476;
   count[0] = 0;
   count[1] = 0;
   }


/* Encodes input (unsigned ) into output (unsigned char). Assumes len is
  a multiple of 4.
 */
void Encode (unsigned char *output, uint32_t  *input, uint32_t len)
   {
   for (int i = 0, j = 0; j < len; i++, j += 4)
      {
      output[j] = (unsigned char)(input[i] & 0xff);
      output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
      output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
      output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
      }
   }

/* Decodes input (unsigned char) into output (unsigned ). Assumes len is
  a multiple of 4.
 */
void Decode (uint32_t  *output, unsigned char *input, uint32_t len)
   {
   uint32_t i, j, a = 8, b = 16, c = 24;

   for (i = 0, j = 0; j < len; i++, j += 4)
      {
      output[i] = input[j] | input[j+1] << 8 | input[j+2] << 16 | input[j+3] << 24;
      }
   }

/* MD5 basic transformation. Transforms state based on block.
 */
void MD5Transform (uint32_t * state, unsigned char block[64])
   {
   uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

   Decode (x, block, 64);

   /* Round 1 */
   a = FFTransform(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
   d = FFTransform (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
   c = FFTransform (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
   b = FFTransform (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
   a = FFTransform (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
   d = FFTransform (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
   c = FFTransform (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
   b = FFTransform (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
   a = FFTransform (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
   d = FFTransform (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
   c = FFTransform (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
   b = FFTransform (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
   a = FFTransform (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
   d = FFTransform (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
   c = FFTransform (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
   b = FFTransform (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

   /* Round 2 */
   a = GGTransform (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
   d = GGTransform (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
   c = GGTransform (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
   b = GGTransform (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
   a = GGTransform (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
   d = GGTransform (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
   c = GGTransform (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
   b = GGTransform (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
   a = GGTransform (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
   d = GGTransform (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
   c = GGTransform (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
   b = GGTransform (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
   a = GGTransform (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
   d = GGTransform (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
   c = GGTransform (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
   b = GGTransform (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

   /* Round 3 */
   a = HHTransform (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
   d = HHTransform (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
   c = HHTransform (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
   b = HHTransform (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
   a = HHTransform (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
   d = HHTransform (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
   c = HHTransform (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
   b = HHTransform (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
   a = HHTransform (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
   d = HHTransform (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
   c = HHTransform (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
   b = HHTransform (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
   a = HHTransform (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
   d = HHTransform (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
   c = HHTransform (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
   b = HHTransform (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

   /* Round 4 */
   a = IITransform (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
   d = IITransform (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
   c = IITransform (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
   b = IITransform (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
   a = IITransform (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
   d = IITransform (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
   c = IITransform (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
   b = IITransform (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
   a = IITransform (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
   d = IITransform (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
   c = IITransform (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
   b = IITransform (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
   a = IITransform (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
   d = IITransform (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
   c = IITransform (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
   b = IITransform (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

   state[0] += a;
   state[1] += b;
   state[2] += c;
   state[3] += d;

   /* Zeroize sensitive information.

   */
   MD5MemSet ((unsigned char *)x, 0, sizeof (x));
   }

/* MD5 block update operation. Continues an MD5 message-digest
  operation, processing another message block, and updating the
  context.
 */
void MD5Update (unsigned  * state, unsigned  * count, unsigned char * buffer, unsigned char * input, unsigned int inputLen)
   {
   unsigned int i, index, partLen;

   /* Compute number of bytes mod 64 */
   index = (unsigned int)((count[0] >> 3) & 0x3F);

   /* Update number of bits */
   if ((count[0] += ((unsigned )inputLen << 3)) < ((unsigned )inputLen << 3)) count[1]++;

   count[1] += ((unsigned )inputLen >> 29);

   partLen = 64 - index;

   /* Transform as many times as possible. */
   if (inputLen >= partLen)
      {
      MD5MemCpy((unsigned char *)&buffer[index], (unsigned char *)input, partLen);
      MD5Transform (state, buffer);

      for (i = partLen; i + 63 < inputLen; i += 64)
         MD5Transform (state, &input[i]);

      index = 0;
      }
   else i = 0;

   /* Buffer remaining input */
   MD5MemCpy ((unsigned char *)&buffer[index], (unsigned char *)&input[i], inputLen-i);
   }

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
  the message digest and zeroizing the context.
 */
void MD5Final (unsigned char * digest,unsigned  * state, unsigned  * count, unsigned char * buffer)
   {
   unsigned char PADDING[64];
   PADDING[0] = 0x80;
   for (uint32_t i = 1; i < 64; i++)
      {
      PADDING[i] = 0;
      }
   unsigned char bits[8];
   uint32_t index, padLen;

   /* Save number of bits */
   Encode((unsigned char *) bits, count, 8);

   /* Pad out to 56 mod 64.*/
   index = (count[0] >> 3) & 0x3f;
   if (index < 56) padLen = 56 - index;
   else padLen = 120 - index;
   MD5Update (state, count, buffer, PADDING, padLen);
   /* Append length (before padding) */
   MD5Update (state, count, buffer, bits, 8);

   /* Store state in digest */
   Encode (digest, state, 16);

   }

void timeDiff(struct timespec * start, struct timespec * end, struct timespec * diff)
   {
   if ((end->tv_nsec-start->tv_nsec)<0)
      {
      diff->tv_sec = end->tv_sec-start->tv_sec-1;
      diff->tv_nsec = 1000000000+end->tv_nsec-start->tv_nsec;
      }
      else
      {
      diff->tv_sec = end->tv_sec-start->tv_sec;
      diff->tv_nsec = end->tv_nsec-start->tv_nsec;
      }
   }

void printDigest(unsigned char * digest, uint32_t len)
   {
   unsigned char c;
   uint32_t i;
   for (i = 0; i < len; i++)
      {
      c = digest[i];
      printf("%x\n",c);
      }
   }


int main()
   {
   char * message = (char *) "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";

   uint32_t  state[4];
   unsigned  count[2];
   unsigned char buffer[20000];
   strcpy((char *) buffer, message);
   unsigned char digest[16];
   uint32_t len = strlen(message);

   clockid_t clockid = CLOCK_MONOTONIC;
   struct timespec start_time, end_time, time_diff;
   clock_gettime(clockid, &start_time);

   MD5Init((uint32_t  *) state, (uint32_t  *) count);
   MD5Update((uint32_t  *) state, (uint32_t  *) count, (unsigned char *) buffer, (unsigned char *) message, len);
   MD5Final((unsigned char *)digest, (uint32_t  *) state, (uint32_t  *) count, (unsigned char *) buffer);
   clock_gettime(clockid, &end_time);
   timeDiff(&start_time, &end_time, &time_diff);
   double time_taken = (double)time_diff.tv_sec + time_diff.tv_nsec*1.0e-9;
   printf("Calculated MD5 hash in %lf (s)\n", time_taken);

   printDigest((unsigned char *) digest, 16);
   return 0;
   }
