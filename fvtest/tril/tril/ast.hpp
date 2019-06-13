/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef AST_HPP
#define AST_HPP

#include "ast.h"

#include "OMR/TypeTraits.hpp"

#include <assert.h>
#include <cstring>

/**
 * @brief The ASTValue struct represents a "value" in the Tril AST
 *
 * The struct acts as a variant capable of holding values of multiple types. It
 * abstracts away the details of how a value is stored and provides an interface
 * for retrieving a value using an appropriate type.
 *
 * An ASTValue can have one of three types:
 *
 * Type name     | Used for              | Compatible with (C++ types)
 * --------------|-----------------------|--------------------------------------
 * Integer       | integral values       | any type satisfying `OMR::IsIntegral`/`std::is_integral`
 * FloatingPoint | floating point values | any type satisfying `OMR::IsFloatingPoint`/`std::is_floating_point`
 * String        | character strings     | `const char*`
 *
 * Names for these types are defined in the ASTType enum.
 *
 * To aid usability, this struct provides the `get<T>()` template member function
 * that will return the stored value as type `T` if `T` is compatible with the
 * type of the stored value.
 *
 * For convenience Integer_t, FloatingPoint_t, and String_t are provided as
 * aliases for the underlying C++ types used to store values.
 */
struct ASTValue {
    public:

    // names for the different AST data types
    enum ASTType {Integer, FloatingPoint, String} ;

    // aliases for the underlying C++ types used to store the different AST types
    typedef uint64_t Integer_t;
    typedef double FloatingPoint_t;
    typedef const char* String_t;

    /**
     * STL compatible type traits for working with AST data types
     *
     * The `is_*_Compatible` type traits have their `::value` field set to true
     * if the specified type is compatible with the named type, false otherwise.
     *
     * Examples:
     *
     * is_Integer_Compatible<int>::value;          // true
     * is_Integer_Compatible<double>::value;       // false
     * is_FloatintPoint_Compatible<double>::value; // true
     * is_String_Compatible<const char*>::value;   // true
     */

    // constructors
    explicit ASTValue(Integer_t v) : _type(Integer), next(NULL) { _value.integer = v; }
    explicit ASTValue(FloatingPoint_t v) : _type(FloatingPoint), next(NULL) { _value.floatingPoint = v; }
    explicit ASTValue(String_t v) : _type(String), next(NULL) { _value.str = v; }

    /**
     * @brief Return the contained value as the specified type
     * @tparam T is the type the contained value should be returned as
     *
     * This function returns the contained value, casted to the specified type.
     * A type check is performed to ensure the specified type is compatible with
     * the type of the stored value.
     *
     * Examples:
     *
     * ASTValue a{4};
     * a.get<int>();   // returns 4 as an int
     * a.get<long>();  // returns 4 as a long
     * a.get<float>(); // causes an assertion failure
     *
     * ASTValue b{4.5};
     * b.get<float>();  // returns 4.5 as a float
     * b.get<double>(); // returns 4.5 as a double
     * b.get<int>();    // causes an assertion failure
     */

    template <typename T>
    typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE , T>::Type get() const {
        assert(Integer == _type);
        return static_cast<T>(_value.integer);
    }
    template <typename T>
    typename OMR::EnableIf<OMR::IsFloatingPoint<T>::VALUE, T>::Type get() const {
        assert(FloatingPoint == _type);
        return static_cast<T>(_value.floatingPoint);
    }
    template <typename T>
    typename OMR::EnableIf<OMR::IsSame<String_t, T>::VALUE, T>::Type get() const {
        assert(String == _type);
        return static_cast<T>(_value.str);
    }

    /**
     * @brief Return the contained value as its underlying integer type, if that is its type
     */
    Integer_t getInteger() const { assert(Integer == _type); return _value.integer; }

    /**
     * @brief Return the contained value as its underlying floating point type, if that is its type
     */
    FloatingPoint_t getFloatingPoint() const { assert(FloatingPoint == _type); return _value.floatingPoint; }

    /**
     * @brief Return the contained value as its underlying string type, if that is its type
     */
    String_t getString() const { assert(String == _type); return _value.str; }

    /**
     * @brief Checks whether the type of the contained value and the specified type are compatible
     * @tparam T is the type to be checked for compatibility
     *
     * Examples:
     *
     * ASTValue a{3};
     * a.isCompatibleWith<int>();    // returns true
     * a.isCompatibleWith<long>();   // returns true
     * a.isCompatibleWith<double>(); // returns false
     *
     * ASTValue b{4.5};
     * b.isCompatibleWith<float>();  // returns true
     * b.isCompatibleWith<double>(); // returns true
     * b.isCompatibleWith<int>();    // returns false
     */

    template <typename T>
    typename OMR::EnableIf<OMR::IsIntegral<T>::VALUE , bool>::Type isCompatibleWith() const {
        return Integer == _type;
    }
    template <typename T>
    typename OMR::EnableIf<OMR::IsFloatingPoint<T>::VALUE, bool>::Type isCompatibleWith() const {
        return FloatingPoint == _type;
    }
    template <typename T>
    typename OMR::EnableIf<OMR::IsSame<String_t, T>::VALUE, bool>::Type isCompatibleWith() const {
        return String == _type;
    }

    /**
     * @brief Returns the ASTType of the contained value
     * @return
     */
    ASTType getType() const { return _type; }

    private:

    // tag containing specifying the type of the contained value
    ASTType _type;

    // union holding the contained value
    union {
        Integer_t integer;
        FloatingPoint_t floatingPoint;
        String_t str;
    } _value;

    public:
    ASTValue* next;
};

/*
 * Overloaded operators for ASTValue
 */

inline bool operator == (const ASTValue& lhs, const ASTValue& rhs) {
   if (lhs.getType() == rhs.getType()) {
      switch (lhs.getType()) {
         case ASTValue::Integer: return lhs.getInteger() == rhs.getInteger();
         case ASTValue::FloatingPoint: return lhs.getFloatingPoint() == rhs.getFloatingPoint();
         case ASTValue::String: return std::strcmp(lhs.getString(), rhs.getString()) == 0;
      }
   }
   return false;
}

inline bool operator != (const ASTValue& lhs, const ASTValue& rhs) {
    return ! (lhs == rhs);
}

/**
 * @brief A struct representing arguments of nodes in the Tril AST
 */
struct ASTNodeArg {
    private:
    const char* _name;
    const ASTValue* _value;

    public:
    ASTNodeArg* next;

    ASTNodeArg(const char* name, ASTValue* value, ASTNodeArg* next = NULL)
        : _name(name), _value(value), next(next) {}

    const char* getName() const { return _name; }
    const ASTValue* getValue() const { return _value; }
};

// overloaded operators for ASTNodeArg

bool operator == (const ASTNodeArg& lhs, const ASTNodeArg& rhs);

inline bool operator != (const ASTNodeArg& lhs, const ASTNodeArg& rhs) {
    return ! (lhs == rhs);
}

/**
 * @brief A struct representing nodes in the Tril AST
 */
struct ASTNode {
    private:
    const char* _name;
    ASTNodeArg* _args;
    ASTNode* _children;

    public:
    ASTNode* next;

    ASTNode(const char* name, ASTNodeArg* args, ASTNode* children, ASTNode* next)
        : _name(name), _args(args), _children(children), next(next) {}

    const char* getName() const { return _name; }

    /**
     * @brief Returns pointer to linked list of arguments
     */
    const ASTNodeArg* getArgs() const { return _args; }

    /**
     * @brief Finds and returns an argument by its index
     * @param index is the index of the argument
     * @return the argument at the given index if found, NULL otherwise
     */
    const ASTNodeArg* getArgument(int index) const {
        auto arg = _args;

        while (arg != NULL && index > 0) {
            arg = arg->next;
            --index;
        }

        return arg;
    }

    /**
     * @brief Finds and returns an argument by name
     * @param name is the name for the argument
     * @return the argument with the given name if found, NULL otherwise
     */
    const ASTNodeArg* getArgByName(const char* name) const {
        auto arg = _args;
        while (arg) {
            if (arg->getName() != NULL && strcmp(name, arg->getName()) == 0) { // arg need not have a name
                return arg;
            }
            arg = arg->next;
        }
        return NULL;
    }

    /**
     * @brief Finds and returns a positional (nameless) argument by its position (index)
     * @param index is the index of the positional argument
     * @return the positional argument at the given index if found, NULL otherwise
     */
    const ASTNodeArg* getPositionalArg(int index) const {
        auto arg = _args;

        while (arg != NULL) {
            const auto name = arg->getName();
            if (name == NULL || name[0] == '\0') {
                if (index > 0) { --index; }
                else { break; }
            }
            arg = arg->next;
        }
        return arg;
    }

    /**
     * @brief Returns the number of arguments the node has
     */
    int argumentCount() const {
        auto arg = _args;
        auto i = 0;

        while (arg != NULL) {
            ++i;
            arg = arg->next;
        }

        return i;
    }

    const ASTNode* getChildren() const { return _children; }
    int getChildCount() const { return countNodes(_children); }
};

#endif // AST_HPP
