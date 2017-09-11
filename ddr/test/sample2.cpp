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

// 6. Nested type definitions (Definitions that are scoped inside another struct/class)
class MyClass
{
private:
	// - class
	class MyNestedClass
	{
	public:
		int d;
	};
	MyNestedClass instanceOfMyNestedClass;

	int64_t data;

	// - enum
	enum { ENUM1, ENUM2, ENUM3 } anonymousEnum;

	enum InnerEnum
	{
		ENUM_A,
		ENUM_B,
		ENUM_C
	};
	InnerEnum innerEnum;

	enum
	{
		ENUM_A2,
		ENUM_B2,
		ENUM_C2
	};

	// - struct
	struct Z {
		uint64_t g;
		uint16_t h[20][5];
	} Z;
	struct Z instanceOfZ;

public:
	// - typedef
	typedef unsigned short USDATA;
	uint64_t getData() { return (data + Z.g * (USDATA)Z.h[7][2] + ENUM_A2); }
};
MyClass instanceOfMyClass;


// 7. typedef
// - typedef'ed primitive
typedef int IDATA;
IDATA instanceOfIDATA;

// - typedef'ed struct

// - typedef where the name doesn't match the struct tag
typedef struct S1 {
	IDATA a;
} T1;
T1 instanceOfT1 = {0};

// - typedef where the name matches the struct tag
typedef struct S2 {
	IDATA b;
	S1 b2;
} S2;
S2 instanceOfS2 = {0, {0}};

// - typedef there there is no struct tag
typedef struct {
	S2 c;
} T2;
T2 instanceOfT2 = {{0, {0}}};

/* Type without struct name at beginning used as a field. */
typedef S2 * T3 ;
typedef struct {
	T2 d;
	S2 e;
	T3 f;
} T4;
T4 instanceOfT4 = {{{0, {0}}}, {0, {0}}, NULL};

// - typedef'ed enum
typedef enum {
	something1,
	something2,
	something3,
	something4,
	something5,
	somethingN
} MyEnum;
MyEnum instanceOfEnum;

// - typedef'ed union
typedef union {
	uint64_t ipv6;
	struct {
		uint32_t hi_ipv6;
		uint32_t lo_ipv6;
	} s;
} Ip;
Ip instanceOfIp;

// 8. Fields of classes or structs
// - unqualified fields
// - public/private/protected fields
// - fields qualified as volatile or const

class BoxV2
{
protected:
	volatile int64_t hashCode;
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
	void generateHashCode() { hashCode = (length + width + height); }
	int64_t getHashCode() { return hashCode; }
};

class Foo
{
private:
	static int staticTest;
	uintptr_t data;
	const int data2;
public:
	Foo(int x) : data(x), data2(4) {}
};

class Bar : public Foo
{
public:
	Bar(int x) : Foo(x) {}
};

typedef struct HasVoidStarField {
	void *voidStar;
} HasVoidStarField;
HasVoidStarField instanceOfHasVoidStarField;

typedef union UnionFieldType {
	int32_t i;
	int16_t s;
} UnionFieldType;

typedef struct HasUnionField {
	union UnionFieldType unionField1;
	UnionFieldType unionField2;
} HasUnionField;
HasUnionField instanceOfHasUnionField;

// 9. Simple numeric macros

#define AA 1
#define BB 2

// 10. Arithmetic macros

#define A_PLUS_B (A + B)
#define PI 3.1415926535897932385
#define PI_PLUS_ONE (PI + 1)
#define INCREMENT(x) x++
#define DECREMENT(x) x--
#define MULT(x, y) (x) * (y)
#define ADD_FIVE(a) ((a) + 5)
#define SWAP(a, b)  do { a ^= b; b ^= a; a ^= b; } while ( 0 )
#define MAX(a, b) ((a) < (b) ? (b) : (a))

void
sample2()
{
	BoxV2 box3;
	BoxV2 box4;

	box3.setLength(47);
	box3.setWidth(98);
	box3.setHeight(72);

	box4.setLength(23);
	box4.setWidth(89);
	box4.setHeight(43);

	uint64_t volume = 0;

	volume = box3.getVolume();
	std::cout << "Volume: box3: " << volume << std::endl;

	volume = box4.getVolume();
	std::cout << "Volume: box4: " << volume << std::endl;

	box3.generateHashCode();
	int64_t hashCode = box3.getHashCode();
	std::cout << "Hash Code: box3: " << hashCode << std::endl;

	box4.generateHashCode();
	hashCode = box4.getHashCode();
	std::cout << "Hash Code: box4: " << hashCode << std::endl;

	Bar bar1(5);

	double x = PI;

	std::cout << "x (PI): " << x << std::endl;

	DECREMENT(x);

	std::cout << "x (decrement): " << x << std::endl;

	x = PI_PLUS_ONE * 5;

	std::cout << "x (Pi + 1 * 5): " << x << std::endl;

	INCREMENT(x);

	std::cout << "x (increment): " << x << std::endl;

	int y = MULT(3 + 2, 4 + 2);

	std::cout << "y (mult (3 + 2, 4 + 2)): " << y << std::endl;

	int z = ADD_FIVE(3) * 3;

	std::cout << "z (add_five(3) * 3): " << z << std::endl;

	int w = 5;

	std::cout << "max(w++y z++): " << std::endl;
	y = 10;
	z = MAX(w++, y++);

	std::cout << "w: " << w << std::endl;
	std::cout << "y: " << y << std::endl;
	std::cout << "z: " << z << std::endl;
	
	std::cout << instanceOfMyClass.getData() << std::endl;
}
