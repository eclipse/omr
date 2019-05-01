#! /usr/bin/env python

###############################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
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

import os
import sys
import json
import unittest

import genutils

class GenUtilsTests(unittest.TestCase):
    """Tests for free functions in genutils."""

    def test_list_str_prepend_1(self):
        msg = genutils.list_str_prepend("quux", "foo, bar")
        self.assertEqual("quux, foo, bar", msg)

    def test_list_str_prepend_2(self):
        msg = genutils.list_str_prepend("foo", "")
        self.assertEqual("foo", msg)

    def test_list_str_prepend_3(self):
        msg = genutils.list_str_prepend("", "")
        self.assertEqual("", msg)

# The following tests cover the methods of the genutils classes.
#
# In the setup of each testcase, the `test/test_sample.json` API
# description is loaded so it can be used as test input for all
# the tests. Parts of the API description that are relevant to
# to the tests are cached in member variables. Instances of the
# class(es) under test corresponding to the API description
# are also constructed and stored in member variables.

class APITypeTests(unittest.TestCase):
    """Tests for methods in genutils.APIType."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.raw_api = json.load(f)
            self.api = genutils.APIDescription(self.raw_api)
            self.raw_class_1 = self.raw_api["classes"][0]
            self.class_1 = genutils.APIClass(self.raw_class_1, self.api)

    def test_name_1(self):
        self.assertEqual("pointer", genutils.APIType("pointer", self.api).name())

    def test_name_2(self):
        self.assertEqual(self.class_1.name(), self.class_1.as_type().name())

    def test_is_builtin_1(self):
        self.assertTrue(genutils.APIType("none", self.api).is_builtin())

    def test_is_builtin_2(self):
        self.assertTrue(genutils.APIType("ppointer", self.api).is_builtin())

    def test_is_builtin_3(self):
        self.assertFalse(self.class_1.as_type().is_builtin())

    def test_is_class_1(self):
        self.assertTrue(genutils.APIType(self.class_1.name(), self.api).is_class())

    def test_is_class_2(self):
        self.assertFalse(genutils.APIType("boolean", self.api).is_class())

    def test_as_class(self):
        self.assertEqual(self.class_1, genutils.APIType(self.class_1.name(), self.api).as_class())

    def test_is_none_1(self):
        self.assertTrue(genutils.APIType("none", self.api).is_none())

    def test_is_none_2(self):
        self.assertFalse(genutils.APIType("integer", self.api).is_none())

class APIFieldTests(unittest.TestCase):
    """Tests for methods in genutils.APIField."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.raw_api = json.load(f)
            self.api = genutils.APIDescription(self.raw_api)
            self.raw_class_1 = self.raw_api["classes"][0]
            self.class_1 = genutils.APIClass(self.raw_class_1, self.api)
            self.raw_field_1 = self.raw_api["fields"][0]
            self.field_1 = genutils.APIField(self.raw_field_1, self.api)
            self.raw_field_2 = self.raw_class_1["fields"][0]
            self.field_2 = genutils.APIField(self.raw_field_2, self.api)

    def test_name_1(self):
        self.assertEqual("Project_field_1", self.field_1.name())

    def test_name_2(self):
        self.assertEqual("class_1_field_1", self.field_2.name())

    def test_type_1(self):
        self.assertEqual(genutils.APIType("int32", self.api), self.field_1.type())

    def test_type_2(self):
        self.assertEqual(genutils.APIType("float", self.api), self.field_2.type())

class APIServiceTests(unittest.TestCase):
    """Tests for methods in genutils.APIService."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.raw_api = json.load(f)
            self.api = genutils.APIDescription(self.raw_api)
            self.raw_class_1 = self.raw_api["classes"][0]
            self.class_1 = genutils.APIClass(self.raw_class_1, self.api)
            self.raw_service_1 = self.raw_class_1["services"][0]
            self.service_1 = genutils.APIService(self.raw_service_1, self.class_1, self.api)
            self.raw_service_2 = self.raw_class_1["services"][1]
            self.service_2 = genutils.APIService(self.raw_service_2, self.class_1, self.api)
            self.raw_service_3 = self.raw_api["services"][0]
            self.service_3 = genutils.APIService(self.raw_service_3, None, self.api)

    def test_name_1(self):
        self.assertEqual("class_1_service_1", self.service_1.name())

    def test_name_2(self):
        self.assertEqual("class_1_service_2", self.service_2.name())

    def test_suffix_1(self):
        self.assertEqual("", self.service_1.suffix())

    def test_suffix_2(self):
        self.assertEqual("overload", self.service_2.suffix())

    def test_overload_name_1(self):
        self.assertEqual("class_1_service_1", self.service_1.overload_name())

    def test_overload_name_2(self):
        self.assertEqual("Project_service_1overload", self.service_3.overload_name())

    def test_sets_allocators_1(self):
        self.assertTrue(self.service_1.sets_allocators())

    def test_sets_allocators_2(self):
        self.assertFalse(self.service_2.sets_allocators())

    def test_is_static(self):
        self.assertFalse(self.service_1.is_static())

    def test_is_impl_default(self):
        self.assertFalse(self.service_2.is_impl_default())

    def test_visibility_1(self):
        self.assertEqual("public", self.service_1.visibility())

    def test_visibility_2(self):
        self.assertEqual("protected", self.service_2.visibility())

    def test_return_type_1(self):
        self.assertEqual(genutils.APIType("constString", self.api), self.service_1.return_type())

    def test_return_type_2(self):
        self.assertEqual(genutils.APIType("none", self.api), self.service_3.return_type())

    def test_parameters_1(self):
        self.assertListEqual([], self.service_2.parameters())

    def test_parameters_2(self):
        expect = [genutils.APIService.APIParameter(p, self.service_3) for p in self.raw_service_3["parms"]]
        self.assertListEqual(expect, self.service_3.parameters())

    def test_is_vararg_1(self):
        self.assertFalse(self.service_1.is_vararg())

    def test_is_vararg_2(self):
        self.assertTrue(self.service_3.is_vararg())

    def test_owning_class_1(self):
        self.assertEqual(self.class_1, self.service_1.owning_class())

    def test_owning_class_2(self):
        self.assertEqual(self.class_1, self.service_2.owning_class())

    def test_owning_class_3(self):
        self.assertEqual(None, self.service_3.owning_class())

class APIParameterTests(unittest.TestCase):
    """Tests for methods in genutils.APIParameter."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.raw_api = json.load(f)
            self.api = genutils.APIDescription(self.raw_api)
            self.raw_service_1 = self.raw_api["services"][0]
            self.service_1 = genutils.APIService(self.raw_service_1, None, self.api)
            self.raw_parm_1 = self.raw_service_1["parms"][0]
            self.parm_1 = genutils.APIService.APIParameter(self.raw_parm_1, self.service_1)
            self.raw_parm_2 = self.raw_service_1["parms"][1]
            self.parm_2 = genutils.APIService.APIParameter(self.raw_parm_2, self.service_1)
            self.raw_parm_3 = self.raw_service_1["parms"][2]
            self.parm_3 = genutils.APIService.APIParameter(self.raw_parm_3, self.service_1)

    def test_name_1(self):
        self.assertEqual("Project_service_1_parm_1", self.parm_1.name())

    def test_name_2(self):
        self.assertEqual("Project_service_1_parm_2", self.parm_2.name())

    def test_name_3(self):
        self.assertEqual("Project_service_1_parm_3", self.parm_3.name())

    def test_type_1(self):
        self.assertEqual(genutils.APIType("int16", self.api), self.parm_1.type())

    def test_type_2(self):
        self.assertEqual(genutils.APIType("pointer", self.api), self.parm_2.type())

    def test_type_3(self):
        self.assertEqual(genutils.APIType("double", self.api), self.parm_3.type())

    def test_is_in_out_1(self):
        self.assertFalse(self.parm_1.is_in_out())

    def test_is_in_out_2(self):
        self.assertTrue(self.parm_2.is_in_out())

    def test_is_in_out_3(self):
        self.assertFalse(self.parm_3.is_in_out())

    def test_is_array_1(self):
        self.assertFalse(self.parm_1.is_array())

    def test_is_array_2(self):
        self.assertFalse(self.parm_2.is_array())

    def test_is_array_3(self):
        self.assertTrue(self.parm_3.is_array())

    def test_array_len_1(self):
        self.assertEqual("Project_service_1_parm_1", self.parm_3.array_len())

    @unittest.skipIf(sys.version_info < (3, 2),
                    "assertRaisesRegex as a context manager requires Python 3.2 support")
    def test_array_len_2(self):
        with self.assertRaisesRegex(AssertionError, "array_len\(\) can only be called on descriptions of array parameters"):
            self.parm_2.array_len()

    def test_can_be_vararg_1(self):
        self.assertFalse(self.parm_1.can_be_vararg())

    def test_can_be_vararg_2(self):
        self.assertFalse(self.parm_2.can_be_vararg())

    def test_can_be_vararg_3(self):
        self.assertTrue(self.parm_3.can_be_vararg())

class APIConstructorTests(unittest.TestCase):
    """Tests for methods in genutils.APIConstructor."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.raw_api = json.load(f)
            self.api = genutils.APIDescription(self.raw_api)
            self.raw_class_1 = self.raw_api["classes"][0]
            self.class_1 = genutils.APIClass(self.raw_class_1, self.api)
            self.raw_ctor_1 = self.raw_class_1["constructors"][0]
            self.ctor_1 = genutils.APIConstructor(self.raw_ctor_1, self.class_1, self.api)

    def test_name_1(self):
        self.assertEqual(self.class_1.name(), self.ctor_1.name())

    def test_return_type(self):
        with self.assertRaises(NotImplementedError):
            self.ctor_1.return_type()

    def test_owning_class_1(self):
        self.assertEqual(self.class_1, self.ctor_1.owning_class())

    def test_owning_class_2(self):
        self.assertEqual(self.class_1, self.ctor_1.owning_class())

class APIClassTests(unittest.TestCase):
    """Tests for methods in genutils.APIClass."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.raw_api = json.load(f)
            self.api = genutils.APIDescription(self.raw_api)
            self.raw_class_1 = self.raw_api["classes"][0]
            self.class_1 = genutils.APIClass(self.raw_class_1, self.api)
            self.raw_class_2 = self.raw_api["classes"][1]
            self.class_2 = genutils.APIClass(self.raw_class_2, self.api)
            self.raw_inner = self.raw_class_1["types"][0]
            self.inner = genutils.APIClass(self.raw_inner, self.api)

    def test_name_1(self):
        self.assertEqual("class_1", self.class_1.name())

    def test_name_2(self):
        self.assertEqual("class_2", self.class_2.name())

    def test_short_name_1(self):
        self.assertEqual("c1", self.class_1.short_name())

    def test_short_name_2(self):
        self.assertEqual("c2", self.class_2.short_name())

    def test_has_parent_1(self):
        self.assertFalse(self.class_1.has_parent())

    def test_has_parent_2(self):
        self.assertTrue(self.class_2.has_parent())

    def test_parent_1(self):
        self.assertEqual(self.class_1, self.class_2.parent())

    @unittest.skipIf(sys.version_info < (3, 2),
                    "assertRaisesRegex as a context manager requires Python 3.2 support")
    def test_parent_2(self):
        with self.assertRaisesRegex(AssertionError, "class 'class_1' does not extend any class"):
            self.class_1.parent()

    def test_inner_classes_1(self):
        inners = self.class_1.inner_classes()
        self.assertEqual(1, len(inners))
        self.assertEqual(self.inner, inners[0])

    def test_inner_classes_2(self):
        self.assertListEqual([], self.class_2.inner_classes())

    def test_services_1(self):
        expect = [genutils.APIService(s, self.class_1, self.api) for s in self.raw_class_1["services"]]
        self.assertListEqual(expect, self.class_1.services())

    def test_services_2(self):
        self.assertListEqual([], self.api.classes()[1].services())

    def test_constructors_1(self):
        expect = [genutils.APIConstructor(c, self.class_1, self.api) for c in self.raw_class_1["constructors"]]
        self.assertListEqual(expect, self.class_1.constructors())

    def test_constructors_2(self):
        expect = [genutils.APIConstructor(c, self.class_2, self.api) for c in self.raw_class_2["constructors"]]
        self.assertListEqual(expect, self.class_2.constructors())

    def test_callbacks_1(self):
        expect = [genutils.APICallback(c, self.class_1, self.api) for c in self.raw_class_1["callbacks"]]
        self.assertListEqual(expect, self.class_1.callbacks())

    def test_callbacks_2(self):
        self.assertListEqual([], self.class_2.callbacks())

    def test_fields_1(self):
        expect = [genutils.APIField(c, self.api) for c in self.raw_class_1["fields"]]
        self.assertListEqual(expect, self.class_1.fields())

    def test_fields_2(self):
        self.assertListEqual([], self.class_2.fields())

    def test_containing_classes_1(self):
        self.assertListEqual(["class_1"], self.inner.containing_classes())

    def test_containing_classes_2(self):
        self.assertListEqual([], self.class_2.containing_classes())

    def test_base_1(self):
        self.assertEqual(self.class_1, self.class_1.base())

    def test_base_2(self):
        self.assertEqual(self.class_1, self.class_2.base())

    def test_as_type_1(self):
        self.assertIsInstance(self.class_1.as_type(), genutils.APIType)

    def test_as_type_2(self):
        self.assertEqual("class_2", self.class_2.as_type().name())

    def test_as_type_3(self):
        self.assertEqual(self.class_1, self.class_1.as_type().as_class(), "failed round-trip")

class APIDescriptionTest(unittest.TestCase):
    """Tests for methods in genutils.APIDescription."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.api = genutils.APIDescription.load_json_file(f)

    def test_project(self):
        self.assertEqual("Project", self.api.project())

    def test_namespaces(self):
        self.assertListEqual(["NS1", "NS2"], self.api.namespaces())

    def test_classes(self):
        for c in self.api.classes():
            self.assertIsInstance(c,  genutils.APIClass)

    def test_services(self):
        for s in self.api.services():
            self.assertIsInstance(s, genutils.APIService)

    def test_get_class_names(self):
        self.assertListEqual(["class_1", "class_2"], self.api.get_class_names())

    def test_get_class_by_name_1(self):
        c = self.api.get_class_by_name("class_1")
        self.assertIsInstance(c, genutils.APIClass)
        self.assertEqual("class_1", c.name())

    @unittest.skipIf(sys.version_info < (3, 2),
                    "assertRaisesRegex as a context manager requires Python 3.2 support")
    def test_get_class_by_name_2(self):
        with self.assertRaisesRegex(AssertionError, "'foo' is not a class in the Project API"):
            self.api.get_class_by_name("foo")

    def test_is_class_1(self):
        self.assertTrue(self.api.is_class("class_1"), "Failed to detect top-level class as one")

    def test_is_class_2(self):
        self.assertTrue(self.api.is_class("class_1_inner_class_1"), "Failed to detect inner class")

    def test_is_class_3(self):
        self.assertFalse(self.api.is_class("bar"), "Erroneously recognized class")

    def test_containing_classes_of_1(self):
        self.assertListEqual(["class_1"], self.api.containing_classes_of("class_1_inner_class_1"))

    def test_containing_classes_of_2(self):
        self.assertListEqual([], self.api.containing_classes_of("class_1"))

    @unittest.skipIf(sys.version_info < (3, 2),
                    "assertRaisesRegex as a context manager requires Python 3.2 support")
    def test_containing_classes_of_3(self):
        with self.assertRaisesRegex(AssertionError, "'quux' is not a class in the Project API"):
            self.api.containing_classes_of("quux")

    def test_base_of_1(self):
        self.assertEqual("class_1", self.api.base_of("class_2"))

    def test_base_of_2(self):
        self.assertEqual("class_1", self.api.base_of("class_1"))

    @unittest.skipIf(sys.version_info < (3, 2),
                    "assertRaisesRegex as a context manager requires Python 3.2 support")
    def test_base_of_3(self):
        with self.assertRaisesRegex(AssertionError, "'foo' is not a class in the Project API"):
            self.api.base_of("foo")
