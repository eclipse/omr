/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#include <stdint.h>
#include <iostream>

// C/C++ coding constructs that ddrgen should be able to process and represent in supersets and blobs.

// 1. enums
// - anonymous enum
// NOTE this produces a compiler warning.
enum { Aa, Bb, Cc } instanceOfAnonEnum = Aa;

// - named enum
enum E { E1 = 2, E2, E3 };
enum E instanceOfE = E2;

// 2. structs
struct A {
	uint16_t *x;
	uint16_t y;
};
struct A instanceOfA = {0, 1};

struct SOA {
	uint8_t a[10];
	uint16_t b[10][10];
};
struct SOA instanceOfSOA = {{0}, {0}};

struct IPv4 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t ihl:4;
	uint8_t version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t version:4;
	uint8_t ihl:4;
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t ecn:2;
	uint8_t dscp:6;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int ecn:2;
	unsigned int dscp:6;
#endif
	uint16_t tot_len;
	uint16_t id;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t fragOff:13;
	uint16_t flags:3;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint16_t fragOff:3;
	uint16_t flags:13;
#endif
	uint8_t ttl;
	uint8_t protocol;
	uint16_t check;
	uint32_t saddr;
	uint32_t daddr;
};
struct IPv4 instanceOfIPv4 = {0};

struct C {
	uint32_t x;
	uint32_t y;
};
struct C instanceOfC = {0, 0};

struct D {
	struct C xy;
	uint32_t z;
};
struct D instanceOfD = {{0, 0}, 0};

struct UDPPacket {
	struct IPv4 ipHeader;
	uint16_t srcPort;
	uint16_t destPort;
	uint16_t length;
	uint16_t checksum;
};
struct UDPPacket instanceOfUDPPacket;

struct F {
	uint16_t x:4;
	uint16_t y:8;
	uint16_t z:4;
};
struct F instanceOfF = {0};

struct G {
	uint16_t x :10;
	uint16_t y :1;
	uint16_t z :7;
};
struct G instanceOfG = {0};

struct H {
	uint16_t a:6;
	uint16_t b:5;
	uint16_t c:5;
	uint16_t d;
};
struct H instanceOfH = {0};

struct Empty {};
struct Empty instanceOfEmpty;

// 3. classes
class Box
{
private:
	int64_t length;
	int64_t width;
	int64_t height;
public:
	int64_t getLength() { return length; }
	int64_t getWidth() { return width; }
	int64_t getHeight() { return height; }
	void setLength(int64_t lengthIn) { length = lengthIn; }
	void setWidth(int64_t widthIn) { width = widthIn; }
	void setHeight(int64_t heightIn) { height = heightIn; }
	uint64_t getVolume() { return (length * width * height); }
};

class RGBColour
{
public:
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

// 4. unions
union Uniondata1 {
	uint16_t i8;
	uint32_t f16;
	uint8_t  arrI8[1];
};
union Uniondata1 instanceOfUniondata1;

union Uniondata2 {
	uint16_t i16;
	uint32_t f32;
	uint8_t arrI8[3];
};
union Uniondata2 instanceOfUniondata2;

union Uniondata3 {
	uint16_t i16;
	uint32_t f32;
	uint8_t arrI8[4];
};
union Uniondata3 instanceOfUniondata3;

union Uniondata4 {
	uint16_t i16;
	uint32_t f32;
	uint8_t arrI8[5];
};
union Uniondata4 instanceOfUniondata4;

union Unionjob {
	char name[32];
	float salary;
	int worker_no;
};
union Unionjob instanceOfUnionjob;

// 5. Types defined in a namespace
namespace MyNamespace
{
	// - enum
	enum THINGS { THING1, THING2, THING3 };

	// - struct
	struct Q {
		uint16_t d;
		uint16_t e[10][10];
	};

	// - class
	class RGBColourV2
	{
	private:
		uint8_t red;
		uint8_t green;
		uint8_t blue;
	public:
		int64_t getRed() { return red; }
		int64_t getGreen() { return green; }
		int64_t getBlue() { return blue; }
		void setRed(int64_t redIn) { red = (uint8_t)redIn; }
		void setGreen(int64_t greenIn) { green = (uint8_t)greenIn; }
		void setBlue(int64_t blueIn) { blue = (uint8_t)blueIn; }
	};

	// - typedef
	typedef unsigned int UDATA;

	// - union
	union Uniondata5 {
		uint16_t i16;
		uint32_t f32;
		uint8_t arrI8[3];
	};

	namespace InnerNamespace
	{
		struct TypeInInnerNamespace
		{
			uint16_t h;
		};
		TypeInInnerNamespace instanceOfTypeInInnerNamespace;
	}
}
MyNamespace::Q instanceOfQ;
MyNamespace::RGBColourV2 instanceOfRGBColourV2;
MyNamespace::UDATA instanceOfUDATA;
MyNamespace::Uniondata5 instanceOfUniondata5;
MyNamespace::THINGS instanceOfThings;

/* This is a forward declaration of a class. */
class ForwardDeclaredClass;

/* This class has a field who's type is a pointer of the forward declared class. */
class HasAForwardDeclaredClass
{
	ForwardDeclaredClass *fieldOfForwardDeclaredClass;
};

HasAForwardDeclaredClass instanceOfHasAForwardDeclaredClass;

extern void sample2();
extern void sample3();
extern "C" void sample4(void);

int
main(int argc, char *argv[])
{
	std::cout << "\ncpp_samples: main: Init" << std::endl;

	A a = { NULL, 8 };

	std::cout << "\ncpp_samples: main: a.x: " << a.x << std::endl;
	std::cout << "cpp_samples: main: a.y: " << a.y << std::endl;

	SOA soa = { { 2, 4, 8, 1, 3 }, {{ 1, 4, 7, 9, 0, 4 }, { 8, 2, 9, 1, 6, 3 }} };

	std::cout << "\ncpp_samples: main: soa.a[0]: " << soa.a[0] << std::endl;
	std::cout << "cpp_samples: main: soa.a[1]: " << soa.a[1] << std::endl;
	std::cout << "cpp_samples: main: soa.a[2]: " << soa.a[2] << std::endl;
	std::cout << "cpp_samples: main: soa.a[3]: " << soa.a[3] << std::endl;
	std::cout << "cpp_samples: main: soa.a[4]: " << soa.a[4] << std::endl;
	std::cout << "cpp_samples: main: soa.b[0][2]: " << soa.b[0][2] << std::endl;
	std::cout << "cpp_samples: main: soa.b[1][1]: " << soa.b[1][1] << std::endl;
	std::cout << "cpp_samples: main: soa.b[0][3]: " << soa.b[0][3] << std::endl;
	std::cout << "cpp_samples: main: soa.b[1][5]: " << soa.b[1][5] << std::endl;
	std::cout << "cpp_samples: main: soa.b[0][3]: " << soa.b[0][3] << std::endl;

	C c = { 78, 126 };

	std::cout << "\ncpp_samples: main: c.x: " << c.x << std::endl;
	std::cout << "cpp_samples: main: c.y: " << c.y << std::endl;

	Box box1;
	Box box2;

	box1.setLength(43);
	box1.setWidth(83);
	box1.setHeight(91);

	box2.setLength(78);
	box2.setWidth(92);
	box2.setHeight(19);

	uint64_t volume = 0;

	volume = box1.getVolume();
	std::cout << "Volume: box1: " << volume << std::endl;

	volume = box2.getVolume();
	std::cout << "Volume: box2: " << volume << std::endl;


	RGBColour rgb1;
	RGBColour rgb2;

	rgb1.red = 83;
	rgb1.green = 10;
	rgb1.blue = 39;

	rgb2.red = 43;
	rgb2.green = 13;
	rgb2.blue = 71;

	std::cout << "Colour: box1: rgb1: Red: " << (short)rgb1.red << " Green: " << (short)rgb1.green << " Blue: " << (short)rgb1.blue << std::endl;

	std::cout << "Colour: box2: rgb2: Red: " << (short)rgb2.red << " Green: " << (short)rgb2.green << " Blue: " << (short)rgb2.blue << std::endl;

	sample2();

	sample3();

	sample4();

	return 0;
}
