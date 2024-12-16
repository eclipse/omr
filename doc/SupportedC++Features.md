<!--
Copyright IBM Corp. and others 2018

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# Supported C++ Features

## Minimum Compiler Support

OS      | Architecture | Build Compiler | Version
--------|--------------|----------------|--------
Linux   | ARMv7        | g++            | [7.3](https://gcc.gnu.org/onlinedocs/7.3.0/)
Linux   | AArch64      | g++            | [7.3](https://gcc.gnu.org/onlinedocs/7.3.0/)
Linux   | x86/x86-64   | g++            | [7.3](https://gcc.gnu.org/onlinedocs/7.3.0/)
Linux   | s390x        | g++            | [7.3](https://gcc.gnu.org/onlinedocs/7.3.0/)
Linux   | ppc64le      | g++            | [7.3](https://gcc.gnu.org/onlinedocs/7.3.0/)
AIX     | ppc64        | XLC            | [13.1.3](https://www.ibm.com/docs/en/SSGH3R_13.1.3/com.ibm.compilers.aix.doc/compiler.pdf)
z/OS    | s390x        | XLC            | [v2r3](https://www.ibm.com/docs/en/SSLTBW_2.3.0/pdf/cbcux01_v2r3.pdf)
macOS   | x86-64       | XCode 12       | Documentation available [here](https://developer.apple.com/download/all) with an Apple Developer account
macOS   | AArch64      | XCode 13       | Documentation available [here](https://developer.apple.com/download/all) with an Apple Developer account
Windows | x86-64       | MSVC 19        | [Visual Studio 2017](https://learn.microsoft.com/en-us/cpp/cpp/?view=msvc-170)

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
* Rvalue references and move semantics: `template <typename T> T Fn(T&& t);`
* Static assertions: `static_assert`
* auto-typed variables `auto i = 1;`
* Trailing function return types: `auto f() -> int`
* Declared type of an expression: `decltype(expr)`
* Right angle brackets: `template <typename T = std::vector<int>>`
* Delegating constructors: `MyStruct() : MyStruct(NULL) {}`
* Extern templates
* Variadic macros: `#define m(p, ...)`, `__VA_ARGS__`
* `__func__` macro
* `long long`
* Generalized constant expressions: `constexpr`
* Defaulted and deleted functions
* Inline namespaces: `inline namespace inner {}`
* Extended friend declarations
* New character types: `char16_t`, `char32_t`
* Extended sizeof (sizeof nested structures)

## Unsupported C++11 Features

* Initializer lists: `std::vector<int> v = { 1, 2, 3 };`
* Type and template aliases: `using MyAlias = MyType;`
* Variadic templates: `template <class... Ts>`, `sizeof...`
* Range based for loops: `for (auto& x : container) { ... }`
* Non throwing exception specifier: `noexcept`
* Inheriting constructors
* Forward declarations for enums
* Extensible literals: `12_km`
* Thread-local storage
* Standard Layout Types: `is_standard_layout<T>::value`
* Unrestricted unions
* Unicode string literals
* Extended integral types: `<cstdint>` (use `<stdint.h>` instead)
* Raw string literals
* Universal character name literals
* ref qualifiers on member functions: `int my_member() &&;` (MSVC 2010)
* Lambda expressions and closures: `[](int i) -> int { return i + 1; }`
* Generalized attributes: `[[attribute]]`
* Null pointer constant: `nullptr`
* Alignment support: `alignas`, `alignof`
* Explicit conversion operators
###### Note: The XLC compilers are the culprits here; the other compilers specified above support these features.
