#! /usr/bin/env python

###############################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

"""
A collection of common utilities for generating JitBuilder client APIs
and language bindings.

The primary utilities provided in this module are wrapper classes for
the data structure produced from a JSON API description. Instead of
using lists and dictionaires directly, generators should use these
classes to answer questions about an API. The classes provide a more
convenient and safe interface for accessing an API description. In
addition to providing direct information about an API "element"
description, a class can also provide information about how the
element is related to other API elements.

In addition to these wrapper classes, a few helper functions
are also included for use in generator implementations.
"""

import sys
import json
from functools import reduce

# API description wrappers

class APIType:
    """
    A wrapper for a datatype uses in an API description.

    This wrapper, unlike the others, does not wrap an API component
    description but rather the use of a datatype within a description.
    The intention is to provide a defined interface for answering
    questions about types that are used in the API.
    """

    def __init__(self, type_name, api):
        self.type_name = type_name
        self.api = api
        self.builtin_types = [ "none"
                             , "boolean"
                             , "integer"
                             , "int8"
                             , "int16"
                             , "int32"
                             , "int64"
                             , "uint32"
                             , "float"
                             , "double"
                             , "pointer"
                             , "ppointer"
                             , "unsignedInteger"
                             , "constString"
                             , "string"
                             ]

    def __eq__(self, other):
        return self.type_name == other.type_name and self.api == other.api and self.builtin_types == other.builtin_types

    def __ne__(self, other):
        return not (self == other)

    def name(self):
        return self.type_name

    def is_builtin(self):
        return self.type_name in self.builtin_types

    def is_class(self):
        return self.api.is_class(self.type_name)

    def as_class(self):
        assert self.is_class(), "cannot retrieve class description for non-class type `{}`".format(self.type_name)
        return self.api.get_class_by_name(self.type_name)

    def is_none(self):
        return "none" == self.type_name

class APIField:
    """A wrapper for a field API description."""

    def __init__(self, description, api):
        self.description = description
        self.api = api # not used but included for consistency

    def __eq__(self, other):
        return self.description == other.description and self.api == other.api

    def __ne__(self, other):
        return not (self == other)

    def name(self):
        """Returns the name of the field."""
        return self.description["name"]

    def type(self):
        """Returns the type of the field."""
        return APIType(self.description["type"], self.api)

class APIService:
    """A wrapper for a service API description."""

    class APIParameter:
        """A wrapper for a service parameter API description."""

        def __init__(self, description, service):
            self.description = description
            self.service = service

        def __eq__(self, other):
            return self.description == other.description and self.service == other.service

        def __ne__(self, other):
            return not (self == other)

        def name(self):
            """Returns the name of the parameter."""
            return self.description["name"]

        def type(self):
            """Returns the type of the parameter."""
            return APIType(self.description["type"], self.service.api)

        def is_in_out(self):
            """Returns whether the parameter is in-out."""
            return "attributes" in self.description and "in_out" in self.description["attributes"]

        def is_array(self):
            """Returns whether the parameter is an array."""
            return "attributes" in self.description and "array" in self.description["attributes"]

        def array_len(self):
            """
            Returns the name of the service parameter that
            specifies the length of this array parameter.

            This method can only be called on descriptions
            of array parameters.
            """
            assert self.is_array(), "array_len() can only be called on descriptions of array parameters"
            assert "array-len" in self.description, "'array-len' field missing in array parameter description"
            return self.description["array-len"]

        def can_be_vararg(self):
            """Returns whether the parameter may be implemented as a vararg."""
            return "attributes" in self.description and "can_be_vararg" in self.description["attributes"]

    def __init__(self, description, owner, api):
        self.description = description
        self.owner = owner
        self.api = api

    def __eq__(self, other):
        return self.description == other.description and self.owner == other.owner and self.api == other.api

    def __ne__(self, other):
        return not (self == other)

    def name(self):
        """Returns the base-name of the API service."""
        return self.description["name"]

    def suffix(self):
        """Returns the overload suffix of the API service."""
        return self.description["overloadsuffix"]

    def overload_name(self):
        """Returns the name of the API service as an overload."""
        return self.name() + self.suffix()

    def __flags(self):
        """Returns the list of flags set for this service."""
        return self.description["flags"]

    def sets_allocators(self):
        """Returns whether the service sets class allocators."""
        return "sets-allocators" in self.__flags()

    def is_static(self):
        """Returns true if this service is static."""
        return "static" in self.__flags()

    def is_impl_default(self):
        """Returns true if this service has the 'impl-default' flag set."""
        return "impl-default" in self.__flags()

    def visibility(self):
        """
        Returns the visibility of the service as a string.

        By default, a service is always public, since this is
        the common case in most APIs.
        """
        return "protected" if "protected" in self.__flags() else "public"

    def return_type(self):
        """Returns the name of the type returned by this service."""
        return APIType(self.description["return"], self.api)

    def parameters(self):
        """Returns a list of the services parameters."""
        return [self.APIParameter(p, self) for p in self.description["parms"]]

    def is_vararg(self):
        """
        Checks if the given API service description can be
        implemented as a vararg.

        A service is assumed to be implementable as a vararg
        if one of its parameters contains the attribute
        `can_be_vararg`.
        """
        vararg_attrs = [p.can_be_vararg() for p in self.parameters()]
        return reduce(lambda l,r: l or r, vararg_attrs, False)

    def owning_class(self):
        """Returns the description of the API class this constructor belongs to."""
        return self.owner

class APICallback(APIService):
    """
    A wrapper for a callback API description.

    Callbacks are currently described the same way as services,
    so all the functionality can be simply shared.
    """

    def __init__(self, description, owner, api):
        APIService.__init__(self, description, owner, api)

class APIConstructor(APIService):
    """
    A wrapper for a constructor API description.

    Class constructors are currently described the same as services,
    so most of the functionality can be simply shared. However, because
    constructors do not have names or return types, which just correspond
    to the owning class, these properties are appropriately override.
    To accommodate these requirements, the description of the owning
    class must also be provided.
    """

    def __init__(self, description, owner, api):
        """
        Constructs an instance of APIConstructor, wrapping a given
        raw description object.
        """
        APIService.__init__(self, description, owner, api)

    def name(self):
        """The name of a constructor is just the name of the owning class."""
        return self.owner.name()

    def return_type(self):
        """
        Constructors do not have a "return type" so this method
        raises a NotImplementedError exception.
        """
        raise NotImplementedError()

class APIClass:
    """A wrapper for a class API description."""

    def __init__(self, description, api):
        self.description = description
        self.api = api

    def __eq__(self, other):
        return self.description == other.description and self.api == other.api

    def __ne__(self, other):
        return not (self == other)

    def name(self):
        """Returns the (base) name of the API class."""
        return self.description["name"]

    def short_name(self):
        """Returns the short-name of the API class. """
        return self.description["short-name"]

    def has_parent(self):
        """Returns true if this class extends another class."""
        return "extends" in self.description

    def parent(self):
        """
        Returns the name of the parent class if it has one,
        an empty string otherwise.
        """
        assert self.has_parent(), "class '{}' does not extend any class".format(self.name())
        return self.api.get_class_by_name(self.description["extends"])

    def inner_classes(self):
        """Returns a list of inner classes descriptions."""
        return [APIClass(c, self.api) for c in self.description["types"]]

    def services(self):
        """Returns a list of descriptions of all contained services."""
        return [APIService(s, self, self.api) for s in self.description["services"]]

    def constructors(self):
        """Returns a list of the class constructor descriptions."""
        return [APIConstructor(c, self, self.api) for c in self.description["constructors"]]

    def callbacks(self):
        """Returns a list of descriptions of all class callbacks."""
        return [APICallback(s, self, self.api) for s in self.description["callbacks"]]

    def fields(self):
        """Returns a list of descriptions of all class fields."""
        return [APIField(f, self.api) for f in self.description["fields"]]

    def containing_classes(self):
        """Returns a list of classes containing the current class, from inner- to outer-most."""
        return self.api.containing_classes_of(self.name())

    def base(self):
        """
        Returns the base class of the current class. If the class does not
        extend another class, the current class is returned.
        """
        return self.api.get_class_by_name(self.api.base_of(self.name())) if self.has_parent() else self

    def as_type(self):
        """Returns an instance of APIType corresponding to the described class."""
        return APIType(self.name(), self.api)

class APIDescription:
    """A class abstract the details of how an API description is stored"""

    @staticmethod
    def load_json_string(desc):
        """Load an API description from a JSON string."""
        return APIDescription(json.loads(desc))

    @staticmethod
    def load_json_file(desc):
        """Load an API description from a JSON file."""
        return APIDescription(json.load(desc))

    def __init__(self, description):
        self.description = description

        # table mapping class names to class descriptions
        self.class_table = {}
        self.__init_class_table(self.class_table, self.classes())

        # table of classes and their contained classes
        self.containing_table = {}
        self.__init_containing_table(self.containing_table, self.classes())

        # table of base-classes
        self.inheritance_table = {}
        self.__init_inheritance_table(self.inheritance_table, self.classes())

    def __init_class_table(self, table, cs):
        """Generates a dictionary from class names class descriptions."""
        for c in cs:
            table[c.name()] = c
            self.__init_class_table(table, c.inner_classes())

    def __init_containing_table(self, table, cs, outer_classes=[]):
        """
        Generates a dictionary from class names to complete class names
        that include the names of containing classes from a list of
        client API class descriptions.
        """
        for c in cs:
            self.__init_containing_table(table, c.inner_classes(), outer_classes + [c.name()])
            table[c.name()] = outer_classes

    def __init_inheritance_table(self, table, cs):
        """
        Generates a dictionary from class names to base-class names
        from a list of API class descriptions.
        """
        for c in cs:
            self.__init_inheritance_table(table, c.inner_classes())
            if c.has_parent(): table[c.name()] = c.parent()

    def __eq__(self, other):
        return self.description == other.description

    def __ne__(self, other):
        return not (self == other)

    def project(self):
        """Returns the name of the project the API is for."""
        return self.description["project"]

    def namespaces(self):
        """Returns the namespace that the API is in."""
        return self.description["namespace"]

    def classes(self):
        """Returns a list of all the top-level classes defined in the API."""
        return [APIClass(c, self) for c in self.description["classes"]]

    def services(self):
        """Returns a list of all the top-level services defined in the API."""
        return [APIService(s, None, self) for s in self.description["services"]]

    def get_class_names(self):
        """Retruns a list of the names of all the top-level classes defined in the API."""
        return [c.name() for c in self.classes()]

    def get_class_by_name(self, c):
        """Returns the description of a class from its name."""
        assert self.is_class(c), "'{}' is not a class in the {} API".format(c, self.project())
        return self.class_table[c]

    def is_class(self, c):
        """Returns true if the given string is the name of an API class."""
        return c in self.class_table

    def containing_classes_of(self, c):
        """
        Returns a list of the classes containing the specified class,
        from outmost to innermost.
        """
        assert self.is_class(c), "'{}' is not a class in the {} API".format(c, self.project())
        return self.containing_table[c]

    def base_of(self, c):
        """
        Returns the name of the base-class of a given class name `c`.
        If `c` does not extend any class, then `c` itself is returned.
        """
        assert self.is_class(c), "'{}' is not a class in the {} API".format(c, self.project())
        return self.base_of(self.inheritance_table[c].name()) if c in self.inheritance_table else c

class PrettyPrinter:
    """A class to help with pretty printing indented text"""
    def __init__(self, out, indentstr="    "):
        """
        Construct a pretty printer
        out - stream formatted text will be written to
        indentstr - string to be used for each level of indentation
        """
        self.indent_level = 0
        self.is_start_of_line = True
        self.out = out
        self.indentstr = indentstr

    def _write(self, line, append_nl):
        prefix_str = ""
        if self.is_start_of_line and line != "":
            self._put_indent()
            self.is_start_of_line = False
        self.out.write(line)
        if append_nl:
            self.out.write("\n")
            self.is_start_of_line = True

    def _put_indent(self):
        self.out.write(self.indentstr * self.indent_level)

    def write(self, str):
        """
        Writes a string to output, indenting if required.
        Embeded newlines are detected and embedded if required
        """
        lines = str.split('\n')

        for line in lines[:-1]:
            self._write(line, True)

        self._write(lines[-1], False)

    def writeln(self, str):
        """Convinience function. Equivilent to write(str+"\n")"""
        self.write("{}\n".format(str))

    def indent(self):
        """Increase the indent level"""
        self.indent_level += 1

    def outdent(self):
        """Decrease the indent level"""
        self.indent_level -= 1
        if self.indent_level < 0:
            self.indent_level = 0

# Useful helper functions

def list_str_prepend(pre, list_str):
    """
    Helper function that preprends an item to a stringified
    comma separated list of items.
    """
    return pre + ("" if list_str == "" else ", " + list_str)

def callback_setter_name(callback_desc):
    """
    Produces the name of the function used to set a client callback
    on an implementation object.
    """
    return "setClientCallback_" + callback_desc.name()

def get_impl_service_name(service):
    """
    Produces the name of the JitBuilder implementation of a
    "stand-alone" service (not an API class member).
    """
    return "internal_" + service.name()