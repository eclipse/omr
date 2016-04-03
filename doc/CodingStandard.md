```
(c) Copyright IBM Corp. 2016

 This program and the accompanying materials are made available
 under the terms of the Eclipse Public License v1.0 and
 Apache License v2.0 which accompanies this distribution.

     The Eclipse Public License is available at
     http://www.eclipse.org/legal/epl-v10.html

     The Apache License v2.0 is available at
     http://www.opensource.org/licenses/apache2.0.php

Contributors:
   Multiple authors (IBM Corp.) - initial implementation and documentation
```

# OMR C and C++ Coding Standards

## Statement of Principles

* All code is intended to be readable. Some guidelines may require a bit more work (typing) when writing code, but result in code which is easier to read.
* Don't write code to be clever. Use your brain power to design clever algorithms instead.
* Keep code simple. Use short "sentences", and clear, unambiguous "vocabulary", and well organized "paragraphs". Try to do only one thing on each line of code.
* Comments are used to explain the intent of the code, not how it works. Well-written code shouldn't require comments to explain how it works. Comments should explain why it is necessary.
* OMR is designed to be portable. Avoid using bleeding-edge language features that limit the portability of the code.

## Applying the Rules

The OMR coding standards have evolved over time, and just like the source code, we expect them to continue to improve. As a result, older source code may not perfectly reflect the most current version of the standards. Nonetheless, as a community our goal is to follow the standards as much possible. As older code is updated, we strive to move closer to the standards. Committers will enforce the rules during code reviews.

Exceptions have been and will continue to be made in extreme circumstances, such as a large contribution of pre-existing code where forcing adherence could have the unintentional side effect of introducing bugs due to manual code rewriting.

### New code

* All new functions and files should conform to the rules.

### Old code

* When rewriting or significantly modifying an existing function, it should be updated to conform to the current coding standards.
* When making small changes to a large function, please try to keep your code consistent with the existing coding standard used within that function.

### Common sense

* Rules were made to be broken. Use your common sense in the application of these standards.
* Disagreement with the coding standards does not count as common sense.

## Standards for C and C++ Code

### ISO/ANSI rules

* All C code is written to rules for C99 compilers.
* There is no need to cater for pre-ANSI (K&R) compilers. Nor can we rely on the presence of C++11 features.
* It is not necessary to cater for the ANSI unique first 6-characters naming rule.
* Functions that take no arguments are declared and defined using ```void``` in the parameter list.

Correct
```c
int
myFunction(void)
{
	statements;
}
```
Wrong
```c
int
myFunction()
{
	statements;
}
```

#### Rationale
* There are some classes of bugs that occur when mixing K&R and ANSI style.
* Allowing for non-ANSI compilers complicates the code. ANSI compilers are now pervasive .
* Prototyping catches some bugs; conversely lack of prototyping can cause some bugs.
* An empty parameter list produces warnings with some compilers, and removes type safety which catches some classes of bug.

### Brace style
* Brace style is OTBS (One True Brace Style) (also known as K&R Style, or the John Travolta style)

```c
if (predicate) {
	doThis();
} else {
	doTheOther();
}
```

* Function opening braces are placed in column 1.

Correct
```c
int
myFunction(void)
{
	statements;
}
```
Wrong
```c
int
myFunction(void) {
	statements;
}
```

* Always use matching open and closing braces. If #ifdefs are being used, ensure that brace counts still match.

Better
```c
if (
#ifdef FOO
	condition1
#else
	condition2
#endif
) {
	statements;
}
```
Correct
```c
#ifdef FOO
if (condition1)
#else
if (condition2)
#endif
{
	statements;
}
```
Wrong
```c
#ifdef FOO
if (condition1) {
#else
if (condition2) {
#endif
	statements;
}
```

* All if, for, while, do and other control statements must be written with braces.

Correct
```c
if (foo) {
	doSomething();
}
```
Wrong
```c
if (foo) doSomething();
```

#### Rationale

All brace styles have benefits and problems; the choice of one over any other is partly arbitrary. This is what we have chosen.

A common brace style allows the following:
* Consistent style leads to faster reading and writing of code.
* Matching braces make it easier to accurately identify blocks.
* Placing single statement condition or loop bodies on the same line as the control statement makes it impossible to set a breakpoint on the conditional statement.

### Indent code using TABS (not spaces)

* Use tabs for indentation
* Tab is typically set to 4 spaces

### Formatting switch statements
* Switch labels (```case``` or ```default```) are indented to the same level as the ```switch``` keyword.

Correct
```c
switch (foo) {
case 1:
	doCaseOne();
	break;
case 2:
	doCaseTwo();
	break;
default:
	doDefaultCase();
	break;
}
```
Wrong
```c
switch (foo) {
	case 1:
		doCaseOne();
		break;
	case 2:
		doCaseTwo();
		break;
	default:
		doDefaultCase();
		break;
}
```

* Switch statements should always include a ```default:``` label, if only to assert failure.

Correct
```c
switch (foo) {
case 1:
	doCaseOne();
	break;
case 2:
	doCaseTwo();
	break;
default:
	assertUnreachable();
}
```
Wrong
```c
switch (foo) {
case 1:
	doCaseOne();
	break;
case 2:
	doCaseTwo();
	break;
}
```

* Put break statements on their own lines (do only one thing on each line).
* Avoid fall-through if possible.
* When using fall-through, use a /* FALLTHROUGH */ label.
* An exception to the fall-through rule is permitted for labels with no code.

Better (no fall through)
```c
switch (foo) {
case 1:
	doCaseOne();
	break;
case 2:
	doCaseTwo();
	doCaseThree();
	break;
case 3:
	doCaseThree();
	break;
case 4:
case 5:
	doCaseFourOrFive();
	break;
default:
	assertUnreachable();
}
```
Correct
```c
switch (foo) {
case 1:
	doCaseOne();
	break;
case 2:
	doCaseTwo();
	/* FALLTHROUGH */
case 3:
	doCaseThree();
	break;
case 4:
case 5:
	doCaseFourOrFive();
	break;
default:
	assertUnreachable();
}
```
Wrong
```c
switch (foo) {
case 1:
	doCaseOne(); break;
case 2:
	doCaseTwo();
case 3:
	doCaseThree(); break;
case 4:
case 5:
	doCaseFourOrFive(); break;
}
```

#### Rationale
* Indenting case labels and the code under them leads to excessive horizontal spacing.
* Fall through is unusual and unexpected; avoid it or at least call it out explicitly.
* Tools like Lint recognize the FALLTHROUGH comment.
* ```default:``` labels with assertions help detect and diagnose bugs.

### Formatting conditions
* When comparing an lvalue and an rvalue for equality or inequality, always put the rvalue on the left.

Correct
```c
if (0 == value1) {
	...
} else if (0 != value2) {
	...
} else if ((value4 + 1) == value3) {
	...
}
```

Wrong
```c
if (value1 == 0) {
	...
} else if (value2 != 0) {
	...
} else if (value3 == (value4 + 1)) {
	...
}
```

* When comparing a function result to a constant, always put the constant on the left.

Correct
```c
if (0 == helperFunction(count, count + 1, dataBlock)) {
	...
}
```

Wrong
```c
if (helperFunction(count, count + 1, dataBlock) == 0) {
	...
}
```

* Try to avoid long conditions. Can the condition be rewritten to be simpler?
* When unavoidable, format long conditions with "&&" or "||" at the beginning of each line, and ending with ") {" on a line by itself.
* Use parentheses to avoid any confusion regarding order of operation.

Correct
```c
if ((value1 == option1)
	&& ((value2a == option2) || (value2b == option2))
	&& (value3 == option3)
) {
	...
}
```
Wrong
```c
if ((value1 == option1) &&
	((value2a == option2) ||  (value2b == option2)) &&
	(value3 == option3)) {
	...
}
```

* Never combine an assignment and a test, except in a while condition. In a while loop, always use an explicit test.

Correct
```c
myPointer = allocateStructure();
if (NULL != myPointer) {
	...
}
```
Wrong
```c
if (myPointer = allocateStructure()) {
	...
}
```

Correct
```c
while (NULL != (cursor = getNext(iterator))) {
	...
}
```
Wrong
```c
while (cursor = getNext(iterator)) {
	...
}
```


* When possible, try to avoid negative conditions. Positive conditions are easier to understand.

Correct
```c
if (NULL == myPointer) {
	doAnotherThing();
} else {
	doOneThing();
}
```

Wrong
```c
if (NULL != myPointer) {
	doOneThing();
} else {
	doAnotherThing();
}
```


#### Rationale
* Keeping rvalues on the left can prevent accidental assignment (= vs ==).
* Keeping small constants on the left improves readability.
* Long chains of conditional logic can be difficult to understand. Avoiding this pattern improves readability.
* Beginning each line with "||" or "&&" provides visual consistency as the connecting operators are all lined up.
* The ") {" sequence provides a clear visual indication that the condition has ended and the block has begun.
* C order of operation rules are complicated; avoid confusion and mistakes by using parentheses to make the order explicit.
* Combining assignment and tests violates the "Try to do only one thing on each line of code" principle.


### Parameter passing

* All objects and structures should be passed by address (i.e. *) rather than reference or value.
* Other parameters that require updating should also be passed by address.
* Non-object parameters that are not updated should be passed by value. C++ reference (i.e. &) arguments must not be used.
* Pointer parameters should be documented with [in], [out] or [in,out] to distinguish input from output parameters

Correct
```c
void
myFunction(MyObject *arg);
```
Wrong
```c
void
myFunction(MyObject& arg);
```

* As an exception to the rule, very simple C++ objects (e.g. base type wrappers) may be passed by value.

Correct
```c
void
myFunction(MyFlags arg);
```


#### Rationale
* Reference arguments can be problematic. For example, it's impossible to pass NULL for a reference object. They're just syntactic sugar to hide pointers, so we prefer to be explicit about how the argument is being passed.

### Macros
* Macros should be kept short and conservative. If a macro spans more than 2 or 3 lines, consider writing it as a subroutine, instead.
* Macros should be "contained" and "context-free". Do not rely on knowing the names of variables defined in the context in which the macro is used. If such knowledge is necessary, then use a setup macro to declare the variable (e.g. like ```OMRPORT_ACCESS_FROM_OMRPORT()```).

Correct
```c
#define FOO(v, x) ((v) = (x))
```
Wrong
```c
#define FOO(x) ((v) = (x))
```

* Do not rely on recursive evaluation of macros. Although some compilers support this, they are not in strict compliance with the ISO specification.
* Protect arguments in macros with parentheses.

Correct
```c
#define FOO(x) ((x) + 1)
```
Wrong
```c
#define FOO(x) x+1
```

* If a macro requires a scope, use ```do {} while (0)```, instead of ```{}```. This avoids several hard to diagnose errors.

Correct
```c
#define FOO(x) \
	do { \
		int32_t y = x; \
		... \
	} while (0)
```
Wrong
```c
#define FOO(x) \
	{ \
		int32_t y = x; \
		... \
	}
```

* For multi-line macros, always start the body of the macro on its own line.

Correct
```c
#define FOO(x) \
	do { \
		... \
	} while (0)
```
Wrong
```c
#define FOO(x) do { \
		... \
	} while (0)
```

* Avoid deep nesting of #ifdefs. Instead, make use of #elif.

Correct
```c
#if defined(FOO)
	return "foo";
#elif defined(BAR)
	return "bar";
#else
	return "other";
#endif
```

Wrong
```c
#ifdef FOO
	return "foo";
#else
#ifdef BAR
	return "bar";
#else
	return "other";
#endif
#endif
```

* Use function-style macros except for defining constants. Any macro which "does something" should look like a function.

Correct
```c
#define DOIT() doSomething()
#define MAX 23
```

Wrong
```c
#define DOIT doSomething()
#define MAX() 23
```

* Also see [Comment conditional compilation directives](#comment-conditional-compilation-directives-if-ifdef-ifndef-else-elif-endif)

#### Rationale

* Using ```do {} while(0)``` forces users of the macro to follow it with a semicolon, making it more similar to a function call.
* When the macro is followed by a semicolon, it forms a single (compound) statement, unlike a block followed by a semicolon, which is two statements.
* It's impossible to accidentally use the macro as a function body.

### Return

* Do not enclose the return value in parentheses.

Correct
```c
return 0;
```
Wrong
```c
return (0);
```

* Avoid multiple return points.

Correct
```c
int32_t result = 0;
if (NULL != input) {
	result = input->size;
}
return result;
```
Wrong
```c
if (NULL == input) {
	return 0;
} else {
	return input->size;
}
```

#### Rationale
* ```return``` is a keyword, not a function.
* ```return``` is an unstructured jump, like ```goto```. It can make the flow of code more difficult to understand.

### Header files

* Header files should always be wrapped in #ifdefs to prevent them from being included more than once.

Correct
```c
#if !defined(FILENAME_HPP_)
#define FILENAME_HPP_
. . .
#endif /* !defined(FILENAME_HPP_) */
```

* When including a header file, be sure to use the correct style of include directive. Use \<angle brackets\> for system header files, and "double quotes" for OMR header files.

Correct
```c
#include <string.h>
```
Wrong
```c
#include "string.h"
```

Correct
```c
#include "omr.h"
```
Wrong
```c
#include <omr.h>
```

* Organize header files into four groups:
  1. "matching header file" -- the header file which declares the module or class
  2. system header files -- string.h, math.h, etc.
  3. OMR C header files -- any .h file in OMR
  4. OMR C++ header files -- any .hpp file OMR
* Sort headers alphabetically within each group.
* Do not design ordering dependencies into header files.

Correct
```c
#include "ThisClass.hpp"

#include <float.h>
#include <string.h>

#include "omr.h"

#include "AnotherClass.hpp"
#include "HelperClass.hpp"
#include "OtherClass.hpp"
```

#### Rationale
* Properly gated header files prevent redefinition errors.
* Sorting headers alphabetically helps avoid accidental duplication of includes.

### const pointers

* In function arguments, temporary variables, and class data members, pointers to data that will only be read should be defined as pointers to ```const``` values.
* Mark scalar temporary variables ```const``` if they will only be read.

Correct
```c
void
doSomething(const SomeOptions *options)
{
	const uint32_t optionsCount = options->count;
	uint32_t optionIndex = 0;
	for (; optionIndex < optionsCount; optionIndex++) {
		...
```

#### Rationale
* ```const``` establishes an official contract between the caller and the callee. The compiler can validate that this contract is followed.
* Marking temporary variables ```const``` helps improve readability by explicitly distinguishing true variables from fixed values.
* Use of ```const``` may allow greater optimization.

### Variable declarations

* Never declare more than one variable on a line.
* Assign an initial value to each variable.

Correct
```c
int32_t x = 0;
int32_t *y = NULL;
```
Wrong
```c
int32_t x, *y;
```
  
* In C code, variables must be declared at the top of a scope. In C++ code, variables may be declared anywhere inside a scope. Although some C compilers may allow variables to be declared throughout a scope, some do not (e.g. MSVC).

Correct
```c
{
	int32_t x = 0;
	int32_t y = 0;
	 ...
	x += 1;
	y = x;
```
Wrong
```c
{
	int32_t x;
	 ...
	x += 1;
	int32_t y = x;
```

* Variables should always be declared in the innermost possible scope, EXCEPT in switch statements.
* Variables only used in a switch statement may be declared in the scope which encloses the switch.

Correct
```c
{
	if (i > 10) {
		int32_t y = 0;

		for (y = ...
	}
}
```
Wrong
```c
{
	int32_t y = 0;

	if (i > 10) {
		for (y = ...
	}
}
```

* When initializing a variable, always include spaces on both sides of the '=' assignment operator.

Correct
```c
int32_t x = 0;
```
Wrong
```c
int32_t x=0;
```

#### Rationale
* C89 compilers require all variables to be declared at the top of a scope.
* Declaring multiple variables on one line can cause confusion about types. Try to do only one thing on each line of code.
* Variables should be declared as close as possible to their use, making code easier to read and discouraging incorrect variable reuse.

### Organizing C++ classes
* Divide class declarations into three sections clearly indicated with a comment as illustrated:
  1. Data members
  2. Function members
  3. Friends
* Data and function sections include private/protected/public subsections, even if they are empty.
* Do not rely on default visibility.
* Include nested data types (structs, enums, etc.) in the data members section.
* Each data member must be accompanied by either a strategic or tactical comment that describes its purpose.
* Completely empty sections may be omitted (e.g. don't include a "Friends" comment if there are no friends).

Correct
```c
class MyClass : public MySuperClass
{
	/*
	 * Data members
	 */
private:
protected:
public:
	...

	/*
	 * Function members
	 */
private:
protected:
public:
	...

	/*
	 * Friends
	 */
	...
}
```

### Whitespace
* Include spaces on both sides of most operators.
* Include spaces between keywords and parenthesized lists or conditions.
* Include spaces between parenthesized lists or conditions and blocks.

Correct
```c
if ((count + 1) == limit) {
	...
}
```
Wrong
```c
if((count+1)==limit){
	...
}
```
  
* Do not include spaces between function names and parameter lists.

Correct
```c
helper(0, count);
```
Wrong
```c
helper (0, count);
```

* Do not include spaces between ```sizeof``` and its operand.
* Use blank lines where appropriate to divide code into "paragraphs".
* Always include blank lines between functions.

#### Rationale
* Judicious use of whitespace improves readability.

### Infinite loops
* Infinite loops generally require a ```break```, ```return``` or ```goto``` to terminate. Therefore infinite loops are discouraged.
* The preferred idiom for infinite loops is ```for (;;)```.
 
### Operators
* The increment and decrement operators (++ and --) are discouraged. Use += or -= instead.
 
Correct
```c
if (NULL != result) {
	unitsProcessed += 1;
	bytesProcessed += result->size;
}
```
Wrong
```c
if (NULL != result) {
	++unitsProcessed;
	bytesProcessed += result->size;
}
```

* EXCEPTION: ++ may be used in for loops, as this is a pervasive idiom.

Correct
```c
for (index = 0; index < size; index++) {
	...
}
```

* Use parentheses to specify order of operation, unless the order is completely unambiguous.
* If in doubt, use more parentheses.
 
Correct
```c
result = (size * count) + offset;
if (0 == (value & mask)) { ...	
if ((0 == a) && (0 != b)) { ... 
end = ((uint8_t *)start) + size;
```
Wrong
```c
result = size * count + offset;
if (0 == a && 0 != b) { ...
end = (uint8_t*)start + size;
```

### Enums
* Enum definitions exposed outside of a single file must include a dummy member with a large value to force the enum to be represented using at least 4 bytes. (If there is doubt about the visibility of the enum definition, include the dummy member.)
* The dummy member must be the last member of the enum. It must be named with the suffix EnsureWideEnum, and documented with the comment /* force 4-byte enum */, as shown in the following example.

Correct
```c
typedef enum Color {
	RED,
	GREEN,
	BLUE,
	Color_EnsureWideEnum = 0x1000000 /* force 4-byte enum */
} Color;
```

#### Rationale
* Compilers may choose to represent enum types using less than 4 bytes, but some legacy C code assumes that enum values can be converted to and from 4-byte integer types.
* Failing to follow this rule has caused bugs when different source files were compiled using different enum-size options.
* This technique is preferred over pragma because it is portable. Pragmas might not be portable across different compilers, and there is no guarantee that a particular compiler will have a pragma to control enum size.


### Function parameter list does not fit on one line

In a function call, when a function's parameter list does not fit on a single line:

* Start the parameter list on a new line. Insert a newline immediately after the opening bracket.
* Indent the parameter list by at least 1 tab. Where reasonable, the parameter list should be aligned with the opening bracket.
* Use the same indent for each line of parameters.

The function call shown below could arguably fit on one line. However, for the purposes of this example, the parameters have been separated into multiple lines.

Correct
```c
resultLength = convertString(
        srcFormat, destFormat,
        inBuffer, inBufferSize, result, bufferLength);
```

Correct
```c
resultLength = convertString(
        srcFormat,
        destFormat,
        inBuffer,
        inBufferSize,
        result,
        bufferLength);
```

Wrong
```c
resultLength = convertString(srcFormat, destFormat,
        inBuffer, inBufferSize, result, bufferLength);
```

#### Rationale
* The goal here is to make the code more readable so that it can be quickly scanned by the developer. This means the function name should stand out.
* Unless multiple lines of parameters are formatted with care, the function name becomes obscured, making the code more difficult to quickly process.
* Using one parameter per line is OK, however, the developer should be mindful of using too much vertical space - it's a judgment call.


## Documentation

### Comments
* Comments should explain why something is being done, or explain an algorithm. They shouldn't simply repeat what the code says.
* TODO comments may be used during development. They may be combined with assertions that indicate unimplemented functionality.
* Comments should use professional language; avoid slang, l33tspeek or anything you wouldn't want a customer to read.

### Formatting comments
* Comments should use  the	/* */ style, rather than //, even in C++ code.
* // comments must NEVER be used in C code.
* Comments that span more than one line should use the Java style as follows:

```c
/* This is a long comment
 * that spans many lines
 * and this is how it
 * should look.
 */
```
* The terminating \*/ of a multi-line comment should appear on a line by itself. The opening /\* may appear on a line by itself, or along with the first line of text.

Correct
```c
/* This is a long
 * comment that
 * spans four lines
 */
```
Correct
```c
/*
 * This is a long
 * comment that
 * spans five lines
 */
```
Wrong
```c
/* This long comment
   is missing *s at
   the start of each
   line
 */
```
Wrong
```c
/* This long comment
 * has the terminator
 * on the last line
 * of text */
```
Wrong
```c
/* This long */
/* comment is */
/* actually four */
/* comments */
```

* Comments should appear on a line by themselves, rather than beginning after a statement. Exceptions may be made for extremely short (i.e one to three word) comments.

Correct
```c
if (NULL == foo) {
	/* failure case */
	...
}
```
Discouraged
```c
if (NULL == foo) { /* fail */
	...
}
```
Wrong
```c
if (NULL == foo) { /* this is a long comment
				   * explaining something
				   * obscure
				   */
	. . .
}
```

* Do not use a comment to disable a section of code. Instead, use #if 0/#endif.

Correct
```c
#if 0
   printf("Returning %d\n", rc);
#endif
```
Wrong
```c
/* printf("Returning %d\n", rc);
*/
```

#### Rationale

* // comments are not permitted in C89.
* Avoiding them in C++ code is good practice as it makes it simpler to switch between C and C++.
* Starting each line of a comment with a * makes the comment stand out from actual code, especially when using an editor without syntax highlighting.
* Placing the closing */ on a line by itself makes it easy to see where the comment ends. It also makes it easier to see that the comment is terminated correctly.
* Using #if 0 distinguishes actual comments from non-functioning code. It also nests properly, permitting the elided code to contain comments.


### Comment function parameters clearly
* A function declaration is preceded by a strategic comment.
* Function comments go in header files for non-static functions. Do not duplicate the comment in the C file.
* The function design intent is described by the comments, such that the reader would have a good shot at reimplementing the function if the source were not available.
* Function comments use Doxygen format.
* Describe all parameters and return values.
* Always indicate units or quantity type where appropriate (e.g. bytes, megabytes, percent, ratio).
* The main point of commenting parameters is to document the responsibilities of the caller (e.g. that it has to free memory at some time).
* If a function uses or modifies global data, this is documented in the function comment.
* Keep comments simple and clear.

Correct
```c
/**
 * Determine the quantity of widgets required to make
 * the desired number of gadgets.
 * @param[in] gadgetFactory the gadget factory
 * @param[in] gadgetsRequired the number of gadgets required
 * @return kilograms of widgets required
 */
uint32_t
getWidgetsRequired(GadgetFactory *gadgetFactory, uint32_t gadgetsRequired)
{
	...
```

#### Rationale

* There is a lot of literature and evidence to suggest that commented code is more maintainable than uncommented code. Well chosen names for identifiers are thought by some programmers to eliminate the need for comments, but in practice this falls far short of adequate for effective maintenance.
* Comments that describe the design are far more durable and go out of date much less often than comments that describe the implementation detail. You often have to write less as well.

### Comment struct, class or union members clearly
* Each structure element should have a Doxygen comment explaining its use.
* Always indicate units or quantity type where appropriate (e.g. bytes, megabytes, percent, ratio).

Correct
```c
typedef struct WidgetData {
	uint32_t widgetSize; /**< The size of each widget, in bytes */
	float widgetRate; /**< The rate at which widgets are delivered, in widgets per millisecond */
} WidgetData;
```

### Comment conditional compilation directives (#if, #ifdef, #ifndef, #else, #elif, #endif)

* Each conditional compilation directive should document the condition that it closes.
* #endif should have a comment describing the matching #if, #ifdef or #ifndef, where practical.
* #elif and #else should have comments that echo the previous condition.

Correct
```c
#if defined(__cplusplus)
...
#endif /* defined(__cplusplus) */
```
Wrong
```c
#if defined (__cplusplus)
...
#endif
```

Correct
```c
#if defined(A) || defined(B)
...
#elif !defined(C) /* defined(A) || defined(B) */
...
#else /* !defined(C) */
...
#endif /* defined(A) || defined(B) */
```

Correct
```c
#if defined(A)
...
#else /* defined(A) */
...
#endif /* defined(A) */
```

Wrong
```c
#if defined(A)
...
#else /* !defined(A) */
...
#endif /* !defined(A) */
```

## Naming Conventions

### Header files

* Class header files have the same name as the class. Other header names are all lowercase.
* Prefixes (e.g. MM_) are to be omitted

### Modules

* Module names are all lowercase.

### Classes (C++ only)

* Class names start with an uppercase letter.
* Class names are camel case: e.g. WorkPackets
* All classes should have separate .cpp and .hpp files.
* The .cpp file may be omitted if it would be empty.

### Functions and methods

* Function names start with a lowercase letter.
* Function names are camel case: e.g. getWidgetFactory()
* Acronyms are all uppercase, unless they appear at the beginning, in which case they're all lowercase: e.g. restartGC(), gcHelperFunction()
* Method names do not use underscores.
* Function names may use underscores to indicate a "namespace" where such a convention has already been established: e.g. omrpool_startDo()
* Functions returning booleans are named after the truth value of the boolean, in such a way as to be totally unambiguous.

#### C++ example

Correct
```c
bool isValid(void);

if (isValid()) {
	doit();
}
```

Wrong
```c
bool checkValid(void);

if (checkValid()) {
	doit();
}
```

#### C example

Correct
```c
BOOLEAN isValid(void);

if (isValid()) {
	doit();
}
```

Wrong
```c
BOOLEAN checkValid(void);

if (checkValid()) {
	doit();
}
```
 
### Variables

* Variable names start with a lowercase letter.
* Variable names are camel case: e.g. heapSize
* Member variable names should start with an underscore: e.g. _heapSize (C++ only)
* Acronyms are all uppercase, unless they appear at the beginning, in which case they're all lowercase.
* Note that all identifiers starting with underscore followed by a capital letter are reserved for the compiler, and must not be used in OMR code. e.g. _S may not be used.
* Names are chosen to be readable but not too long (exceptions for loop indices) to convey the best information on the data or functionality.

### Macros

* Names of macros should be capitalized.

Correct
```c
#define INVARIANT_SNIPPET_LENGTH 18
```

#### Rationale

* Names influence the way you think about something. Well chosen names can mean the difference between instant understanding and confusion.
* It is helpful to have different syntactic elements named differently, for instant recognition when reading code and thus faster programming.

### Abbreviations

* Do not abbreviate words unless the abbreviation is widely used throughout the industry. e.g. use "resolve" instead of "reslv"
* This rule applies to ALL names, including structures, functions and temporary variables.

### Use typedef names to identify structures

* ```typedef``` is always used when declaring enumeration, structure and union in C.
* C++ doesn't require ```typedef``` for these elements.
* Always use the same name for the ```struct``` and the ```typedef```.
* Do not use ```typedef``` to define a pointer to a structure.
* Here is an example showing a structure correctly tagged, and the same structure with a missing tag:

Correct
```c
typedef struct MyStruct {
	char *string; /**<...
	uint32_t value; /**<...
} MyStruct;
```
Wrong
```c
typedef struct {
	char *string; /**<...
	uint32_t value; /**<...
} MyStruct, *MyStructp;
```
Wrong
```c
typedef struct MyStructTag {
	char *string; /**<...
	uint32_t value; /**<...
} MyStruct;
```

#### Rationale
* Typedefs allow for more concise code by avoiding the repetitive use of enum, struct or union prefixes.
* Pointer typedefs needlessly hide that a pointer is being used. A typedef'ed name can easily be turned into a pointer by the addition of a simple asterisk.


## OMR-Specific Rules

### Data Types

* Prefer using standard fixed-width data types like ```uint8_t```, ```int8_t```, ```uint32_t```, etc. Built-in types like ```int``` and ```long``` should be avoided. These may be different sizes on different platforms.
* Exceptions may be made when working with standard library or platform specific functions. In this case, use the appropriate C data types, like ```char``` or ```size_t``` or ```DWORD```.
* An exception is also permitted for pointers to C strings. These should be ```char *``` (or ```const char *```), rather than ```uint8_t *```.
* Use ```double``` for floating point data, unless there is a compelling reason to use ```float```.

#### Rationale
* C data types vary in size and sign from platform to platform. Using well-defined types avoids portability problems in the future.

### Library functions

* OMR code should not rely on any but the most rudimentary standard library functions being present.
* Generally you should restrict yourself to the functions declared in \<string.h\>, \<stdarg.h\> and \<math.h\>. \<ctype.h\> should be used with care, see below.
* sprintf() should not be used. Use the omrstr_printf() function from the port library, instead.
* printf() should not be used. Use the omrtty_printf() function from the port library, instead.
* stricmp() should not be used. It is not a standard function.
* strdup() should not be used. It is not a standard function.
* malloc(), calloc(), realloc(), alloca(), and free() should not be used. Use port library memory management routines, instead.
* tolower(), toupper(), strcasecmp(), and strncasecmp() should not be used as these provide locale- and platform-dependent results which may not be expected. Use j9_cmdla_tolower(), j9_ascii_tolower(), j9_cmdla_toupper(), j9_ascii_toupper(), j9_cmdla_stricmp(), and j9_cmdla_strnicmp() instead.
* This rule does not apply to platform-specific code (particularly in the port and thread libraries).
* When in doubt, look for other code in OMR source that makes use of the function you want to use.If you encounter more than one other use in common code, chances are that it is portable. If you encounter only one use, maybe we should replace that one use with some other call that is similar.

#### Rationale
* OMR is intended to be easily and quickly ported to new platforms. Help keep it this way by avoiding platform dependencies.

### Warnings and Errors

* OMR C and C++ code should compile with no warnings or errors.
* Don't just cover up a warning with a cast. Understand what the error means, and fix your code appropriately (using a cast if appropriate).
* Some common errors which might not be diagnosed on some platforms include:
  * assigning to an rvalue.<br>
    e.g. ```(uint32_t *)x = y;```
  * shifting by more that the number of bits in a type.<br>
    e.g. ```1 << 36 /* undefined if sizeof(int) <= 4 */```<br>
    use ```((uint64_t)1) << 36``` instead

#### Rationale
* A warning usually indicates that you're doing something bad, such as exploiting implementation-dependent compiler behaviour.
* A warning on one platform may be a fatal error on another.


## Best Practices

### Use simple control flow

* Try to use a simple, single control path for every function. Generally, you should try to have a single return point from the function.
* Break and continue should be avoided in looping constructs when they can be easily avoided.
* Gotos should be avoided, although they may be used for error cleanup in some cases.
* Complex control structures can often be avoided by the use of more and smaller functions.
* These guidelines should all be applied with a large dose of common sense. Multiple returns, break, continue, and goto may all be used when appropriate.

### Assertions
* Use assertions liberally.
* Validate function parameters using assertions.
* Test for common errors like NULL dereference and division by zero with assertions.
* Assertions are for impossible conditions; they do not replace error handling for legitimate failure conditions.

#### Rationale
* Assertions are generally very inexpensive.
* An assertion failure is much easier to debug than a crash or corrupt data.

### Cut and paste

* If you find yourself copying and pasting a large section of code, ask yourself if the code wouldn't be better off in a subroutine where everybody can use it.
* Cut and pasted code tends to quickly fall out of date, and contributes to code bloat.

### Local variable reuse

* Don't reuse a local variable for dissimilar tasks. e.g. using a variable named "temp" for three different things.
* This makes the code difficult to read and, contrary to popular belief, can actually result in larger, slower object code.

### Magic numbers

* Magic numbers are constants which appear in code with no explanation. Generally, literal numbers other than -1, 0, 1, 2 should be avoided. Use #define or enums to assign symbolic names to the constants, instead.

Best
```c
enum {
	TOO_TALL = -4,
	MAX_HEIGHT = 7
}

	if (height > MAX_HEIGHT) {
		return TOO_TALL;
	}
```

Correct
```c
#define TOO_TALL (-4)
#define MAX_HEIGHT 7

	if (height > MAX_HEIGHT) {
		return TOO_TALL;
	}
```

Wrong
```c
	if (height > 7) {
		return -4;
	}
```

### Code for debugging

* Write code with debugging in mind.
* Leave useful debugging code in (conditioned as appropriate using #ifdefs).

### Function pointers

* Often, structs include a function pointer member. When possible, name the function pointer with the same name as the function it will point to.

Correct
```c
typedef struct OMRInterface {
	FUNCPTR doSomething;
} OMRInterface;
```

Wrong
```c
typedef struct OMRInterface {
	FUNCPTR doSomethingPtr;
} OMRInterface;
```

#### Rationale
* This makes it easier to search for references to the function.

### Math
* Always consider overflow and underflow; use assertions to prove it can't happen.

Correct
```c
assert(newValue >= oldValue);
uint32_t delta = newValue - oldValue;
```

Wrong
```c
uint32_t delta = newValue - oldValue;
```

* Always consider division by zero; use assertions to prove it can't happen.

Correct
```c
assert(0 != oldValue);
double ratio = (double)newValue / (double)oldValue;
```

Wrong
```c
double ratio = (double)newValue / (double)oldValue;
```

* Use of floating point math is not discouraged.
* Avoid implicit up or down casts of scalar values; use an explicit cast instead, or change the data types to avoid the cast.

Best
```c
uint32_t count = getCount();
uint32_t size = getSize();
uint32_t averageSize = size / count;
```

Correct
```c
uint8_t count = getCount();
uint32_t size = getSize();
uint32_t averageSize = size / (uint32_t)count;
```

Wrong
```c
uint8_t count = getCount();
uint32_t size = getSize();
uint32_t averageSize = size / count;
```

