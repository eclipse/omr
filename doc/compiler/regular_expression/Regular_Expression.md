<!--
Copyright (c) 2016, 2017 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

# Regular expression

A **regular expression** or **regex** is a concise way to describe matching or searching criteria for text strings. For more on regular expressions in general, see the [Wikipedia article](https://en.wikipedia.org/wiki/Regular_expression).

The Eclipse OMR compiler `-Xjit` option allows a simplified form of regular expressions as arguments to some options. The regular expressions are not the same as those used by Perl or grep but behave more like shell globs.

## Syntax

Regular expressions in `-Xjit` options are delimited by curly braces, and the braces are not considered part of the regular expression itself. Most characters in a regular expression match only themselves. For example,

```
-Xjit:limit={foo.method()V}
```

The regular expression here is `foo.method()V`. This option specifies that only the method whose signature exactly matches this string be compiled. The method names to search for are the ones after they have been mangled. The mangled method names are determined by whatever the internal mangling is for the compiler implementation. For example, in C++, a method `bar` in the class `Foo` that accepts an integer and retruns an integer will be mangled to something like `_ZN3Foo3barEi`. In Java, it would be `Foo.bar(I)I`. The mangling can be determined by running the following option:

```
-Xjit:verbose
```

Some characters in a regular expression have special meanings:

Character   |  Meaning
-----       |  -----
?           |  Matches a single character.
\*          |  Matches a sequence of characters.
\[\]        |  Matches a single character of those specified. (See below)
\|          |  Separates alternatives. (See below)
,           |  Alternative syntax for the \| operator.

The following table gives some examples, and the effect they will have when matched against method signatures. These examples are java-specific since they rely on the mangled signature representation in java.

Regular Expression               |  Matches
------------------               |  -------
`{java/lang/String.indexOf(*}`   |  All overloads of `String.indexOf()`
`{*(I)*}`                        |  All methods that accept a single parameter of type `int`
`{*.[^a]*}`                      |  All methods whose names do not begin with "a"
`{java/lang/[A-M]*}`             |  All methods of all classes in the java.lang package whose names start with the letters A through M
`{*.a*|*)V}`                     |  All methods that begin with "a" or return `void`
`{*)[[]*}`                       |  All methods that return any kind of array

Note that the use of brackets for character sets interferes with their use within method signatures as array type specifiers. The last example above shows how the left bracket character can be escaped by enclosing it in brackets.

If the `-Xjit` options are double-quoted, bash will perform substitution on `*` and `?`. An easy way to check that the options look the way they are intended to (after bash is done with them) is to run with `-Xjit:verbose`.
