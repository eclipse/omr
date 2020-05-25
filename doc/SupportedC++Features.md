<!--
Copyright (c) 2018, 2019 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Supported C++ Features

## Minimum Compiler Support

OS      | Architecture | Build Compiler | Version
--------|--------------|----------------|--------
Linux   | x86          | g++            | [4.4.7](https://gcc.gnu.org/gcc-4.4/cxx0x_status.html)
Linux   | s390x        | g++            | [4.4.7](https://gcc.gnu.org/gcc-4.4/cxx0x_status.html)
Linux   | ppc64le      | XLC            | [12.1](https://www.ibm.com/developerworks/community/blogs/5894415f-be62-4bc0-81c5-3956e82276f3/entry/xlc_compiler_s_c_11_support50?lang=en)
Linux   | ppc64le      | g++            | [4.4.7](https://gcc.gnu.org/gcc-4.4/cxx0x_status.html)
AIX     | ppc64        | XLC            | [12.1](https://www.ibm.com/developerworks/community/blogs/5894415f-be62-4bc0-81c5-3956e82276f3/entry/xlc_compiler_s_c_11_support50?lang=en)
z/OS    | s390x        | XLC            | [v2r2](http://www-01.ibm.com/support/docview.wss?uid=swg27036892)
Windows | x86-64       | MSVC           | [2010 (version 10)](https://docs.microsoft.com/en-us/previous-versions/hh567368(v=vs.140))

* Note: moving to gcc 7.3
* Note: moving to msvc 2017

### External Resources

* [CppReference compiler support table](https://en.cppreference.com/w/cpp/compiler_support)

## Using the C++ Standard Library

- The C++ standard library is not fully implemented in XLC.
- The GC disallows linking against the C++ standard library. Header-only utilities are allowed, but likely unavailable.
- The compiler makes heavy use of the C++ standard library, and statically links the stdlib whenever possible.
- The C standard library is used everywhere. Where possible, prefer to use OMR's port and thread libraries.

## Exceptions in OMR

- The C++ standard library requires exceptions in order to function reasonably.
- Don't use RAII types or std containers when exceptions are disabled.
- The Compiler and JitBuilder have exceptions *enabled*.
- All other components, including port, thread, and GC, have exceptions *disabled*.
- MSVC does not allow exception specifiers in code (eg `throw()`, `noexcept`, when exceptions are disabled.

## Supported C++11 features

OMR is written in a pre-standardization dialect of C++11.
The supported language and library features are set by the minimum compiler versions we support.

* Strongly-typed/scoped enums: `enum class`
* Rvalue references and move semantics: `template <typename T> T Fn(T&& t);` (V2.0--MSVC 2010)
* Static assertions: `static_assert`
* auto-typed variables `auto i = 1;` (V1.0--MSVC 2010)
* Trailing function return types: `auto f() -> int`
* Declared type of an expression: `decltype(expr)` (V1.0--MSVC 2010)
* Right angle brackets: `template <typename T = std::vector<int>>`
* Delegating constructors: `MyStruct() : MyStruct(NULL) {}`
* Extern templates
* Variadic macros: `#define m(p, ...)`, `__VA_ARGS__`
* `__func__` macro
* `long long`

## Unsupported C++11 Features

* SFINAE (MSVC 2010)
* Generalized constant expressions: `constexpr` (MSVC 2010)
* Lambda expressions and closures: `[](int i) -> int { return i + 1; }` (GCC 4.4.7)
* Initializer lists: `std::vector<int> v = { 1, 2, 3 };` (MSVC 2010)
* Type and template aliases: `using MyAlias = MyType;` (MSVC 2010)
* Variadic templates: `template <class... Ts>`, `sizeof...` (MSVC 2010)
* Defaulted and deleted functions (MSVC 2010, XLC 12.1)
* Range based for loops: `for (auto& x : container) { ... }` (GCC 4.4.7, MSVC 2010)
* Non throwing exception specifier: `noexcept` (GCC 4.4.7, MSVC 2010)
* Generalized attributes: `[[attribute]]` (GCC 4.4.7)
* Null pointer constant: `nullptr` (GCC 4.4.7)
* Alignment support: `alignas`, `alignof` (GCC 4.4.7)
* Inline namespaces: `inline namespace inner {}` (MSVC 2010)
* Inheriting constructors (MSVC 2010)
* Forward declarations for enums (MSVC 2010)
* Explicit conversion operators (GCC 4.4.7)
* Extensible literals: `12_km` (MSVC 2010)
* Thread-local storage
* Standard Layout Types: `is_standard_layout<T>::value`
* Extended friend declarations
* Unrestricted unions (MSVC 2010)
* Unicode string literals
* Extended integral types: `<cstdint>` (use `<stdint.h>` instead) (XLC)
* Raw string literals
* Universal character name literals
* New character types: `char16_t`, `char32_t` (MSVC 2010)
* Extended sizeof (sizeof nested structures) (XLC 12.1)
* ref qualifiers on member functions: `int my_member() &&;` (MSVC 2010)
