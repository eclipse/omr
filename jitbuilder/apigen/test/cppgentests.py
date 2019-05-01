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

import re
import unittest

import genutils
import cppgen

class CppGeneratorTest(unittest.TestCase):
    """Tests for CppGenerator class"""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.api = genutils.APIDescription.load_json_file(f)
        self.generator = cppgen.CppGenerator(self.api, "", [])

    def test_generate_include_1(self):
        self.assertRegexpMatches(self.generator.generate_include("foo.hpp"), '#include\s+"foo.hpp"')

    def test_generate_include_2(self):
        self.assertRegexpMatches(self.generator.generate_include("bar"), '#include\s+"bar"')

    def test_get_client_class_name_1(self):
        class_desc = self.api.classes()[0].inner_classes()[0]
        self.assertEqual("class_1::class_1_inner_class_1", self.generator.get_client_class_name(class_desc))

    def test_get_impl_class_name_1(self):
        class_desc = self.api.classes()[0].inner_classes()[0]
        self.assertEqual("TR::class_1::class_1_inner_class_1", self.generator.get_impl_class_name(class_desc))

    def test_get_client_type_1(self):
        type_desc = genutils.APIType("none", self.api)
        self.assertEqual("void", self.generator.get_client_type(type_desc))

    def test_get_client_type_2(self):
        type_desc = genutils.APIType("ppointer", self.api)
        self.assertEqual("void **", self.generator.get_client_type(type_desc))

    def test_get_client_type_3(self):
        type_desc = genutils.APIType("class_1", self.api)
        self.assertEqual("class_1 *", self.generator.get_client_type(type_desc))

    def test_get_client_type_4(self):
        type_desc = genutils.APIType("class_1_inner_class_1", self.api)
        namespaces = "::".join(self.api.namespaces()) + "::"
        self.assertEqual("NS1::NS2::class_1::class_1_inner_class_1 *", self.generator.get_client_type(type_desc, namespaces))

    def test_get_client_type_5(self):
        type_desc = genutils.APIType("foo", self.api)
        self.assertRaises(KeyError, self.generator.get_client_type, type_desc)

    def test_get_impl_type_1(self):
        type_desc = genutils.APIType("constString", self.api)
        self.assertEqual("const char *", self.generator.get_impl_type(type_desc))

    def test_get_impl_type_2(self):
        type_desc = genutils.APIType("int32", self.api)
        self.assertEqual("int32_t", self.generator.get_impl_type(type_desc))

    def test_get_impl_type_3(self):
        type_desc = genutils.APIType("class_2", self.api)
        self.assertEqual("TR::class_2 *", self.generator.get_impl_type(type_desc))

    def test_get_impl_type_4(self):
        type_desc = genutils.APIType("class_1_inner_class_1", self.api)
        self.assertEqual("TR::class_1::class_1_inner_class_1 *", self.generator.get_impl_type(type_desc))

    def test_get_impl_type_5(self):
        type_desc = genutils.APIType("foo", self.api)
        self.assertRaises(KeyError, self.generator.get_impl_type, type_desc)

    def test_generate_static_cast_1(self):
        self.assertRegexpMatches(self.generator.generate_static_cast("void *", "foo"),
                                "static_cast<\s*void \*\s*>\(\s*foo\s*\)")

    def test_generate_static_cast_2(self):
        self.assertRegexpMatches(self.generator.generate_static_cast("bar", "quux"),
                                "static_cast<\s*bar\s*>\(\s*quux\s*\)")

    def test_generate_static_cast_3(self):
        self.assertRegexpMatches(self.generator.generate_static_cast(" ", " "),
                                "static_cast<\s*>\(\s*\)")

    def test_to_impl_cast_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.to_impl_cast(class_desc, "foo"),
                                "static_cast<TR::class_1::class_1_inner_class_1\s*\*>\(\s*foo\s*\)")

    def test_to_impl_cast_2(self):
        class_desc = self.api.get_class_by_name("class_2")
        self.assertRegexpMatches(self.generator.to_impl_cast(class_desc, "bar"),
                                "static_cast<TR::class_2\s*\*>\(\s*static_cast<TR::class_1\s*\*>\(\s*bar\s*\)\s*\)")

    @unittest.skip("Instead of rasing an assert, to_impl_case raises an AttributeError: 'APIType' object has no attribute 'base', which is no good.")
    def test_to_impl_cast_3(self):
        type_desc = self.api.get_class_by_name("class_1").as_type()
        self.assertRaises(AssertionError, self.generator.to_impl_cast(type_desc, "bar"))

    def test_to_opaque_cast_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.to_opaque_cast("foo", class_desc),
                                "static_cast<void\s*\*>\(\s*foo\s*\)")

    def test_to_opaque_cast_2(self):
        class_desc = self.api.get_class_by_name("class_2")
        self.assertRegexpMatches(self.generator.to_opaque_cast("bar", class_desc),
                                "static_cast<void\s*\*>\(\s*static_cast<TR::class_1\s*\*>\(\s*bar\s*\)\s*\)")

    @unittest.skip("Instead of rasing an assert, to_impl_case raises an AttributeError: 'APIType' object has no attribute 'base', which is no good.")
    def test_to_opaque_cast_3(self):
        type_desc = self.api.get_class_by_name("class_1").as_type()
        self.assertRaises(AssertionError, self.generator.to_opaque_cast("quux", type_desc))

    def test_to_client_cast_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.to_client_cast(class_desc, "foo"),
                                "static_cast<class_1::class_1_inner_class_1\s*\*>\(\s*foo\s*\)")

    def test_to_client_cast_2(self):
        class_desc = self.api.get_class_by_name("class_2")
        self.assertRegexpMatches(self.generator.to_client_cast(class_desc, "bar"),
                                "static_cast<class_2\s*\*>\(\s*bar\s*\)")

    @unittest.skip("Instead of rasing an assert, to_impl_case raises an AttributeError: 'APIType' object has no attribute 'base', which is no good.")
    def test_to_client_cast_3(self):
        type_desc = self.api.get_class_by_name("class_1").as_type()
        self.assertRaises(AssertionError, self.generator.to_opaque_cast("quux", type_desc))

    def test_grab_impl_1(self):
        type_desc = genutils.APIType("int32", self.api)
        self.assertRegexpMatches(self.generator.grab_impl("foo", type_desc), "foo")

    def test_grab_impl_2(self):
        type_desc = self.api.get_class_by_name("class_1_inner_class_1").as_type()
        self.assertRegexpMatches(self.generator.grab_impl("bar", type_desc),
                                "static_cast<TR::class_1::class_1_inner_class_1\s*\*>\(\s*bar\s*!=\s*NULL\s*\?\s*bar->_impl\s*:\s*NULL\s*\)")

    def test_generate_parm_1(self):
        parm_desc = self.api.services()[0].parameters()[0]
        self.assertRegexpMatches(self.generator.generate_parm(parm_desc),
                                "int16_t\s*Project_service_1_parm_1")

    def test_generate_parm_2(self):
        parm_desc = self.api.services()[0].parameters()[1]
        self.assertRegexpMatches(self.generator.generate_parm(parm_desc),
                                "void\s*\*\s*\*\s*Project_service_1_parm_2")

    def test_generate_parm_3(self):
        parm_desc = self.api.services()[0].parameters()[2]
        self.assertRegexpMatches(self.generator.generate_parm(parm_desc),
                                "double\s*\*\s*Project_service_1_parm_3")

    def test_generate_arg_1(self):
        parm_desc = self.api.services()[0].parameters()[0]
        self.assertEqual(self.generator.generate_arg(parm_desc), "Project_service_1_parm_1")

    def test_generate_arg_2(self):
        parm_desc = self.api.services()[0].parameters()[1]
        self.assertEqual(self.generator.generate_arg(parm_desc), "Project_service_1_parm_2Arg")

    def test_generate_arg_3(self):
        parm_desc = self.api.services()[0].parameters()[2]
        self.assertEqual(self.generator.generate_arg(parm_desc), "Project_service_1_parm_3Arg")

    def test_callback_thunk_name_1(self):
        class_desc = self.api.get_class_by_name("class_1")
        callback = class_desc.callbacks()[0]
        self.assertEqual(self.generator.callback_thunk_name(class_desc, callback),
                        "class_1Callback_class_1_callback_1")

    def test_impl_getter_name_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertEqual(self.generator.impl_getter_name(class_desc),
                        "getImpl_class_1_inner_class_1")

    def test_get_allocator_name_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertEqual(self.generator.get_allocator_name(class_desc),
                        "allocateclass_1class_1_inner_class_1")

    def test_generate_field_decl_1(self):
        field = self.api.get_class_by_name("class_1").fields()[0]
        self.assertRegexpMatches(self.generator.generate_field_decl(field),
                                "public:\s*float\s+class_1_field_1\s*;")

    def test_generate_field_decl_2(self):
        field = self.api.get_class_by_name("class_1").fields()[1]
        self.assertRegexpMatches(self.generator.generate_field_decl(field, False),
                                "double\s+class_1_field_2\s*;")

    def test_generate_class_service_decl_1(self):
        service = self.api.get_class_by_name("class_1").services()[0]
        self.assertRegexpMatches(self.generator.generate_class_service_decl(service),
                                "public:\s*const\s*char\s*\*\s*class_1_service_1\(\s*const\s*char\s*\*\s*class_1_service_1_parm\s*\);")

    def test_generate_class_service_decl_2(self):
        service = self.api.get_class_by_name("class_1").services()[1]
        self.assertRegexpMatches(self.generator.generate_class_service_decl(service),
                                "protected:\s*void\s*class_1_service_2\(\s*\);")

    def test_generate_ctor_decl_1(self):
        ctor = self.api.get_class_by_name("class_2").constructors()[0]
        self.assertRegexpMatches(self.generator.generate_ctor_decl(ctor, "class_2"),
                                "public:\s*class_2\(\)\s*;")

    def test_generate_ctor_decl_2(self):
        ctor = self.api.get_class_by_name("class_1_inner_class_1").constructors()[0]
        self.assertRegexpMatches(self.generator.generate_ctor_decl(ctor, "class_1_inner_class_1"),
                                "public:\s*class_1_inner_class_1\(\)\s*;")

    def test_generate_dtor_decl_1(self):
        class_desc = self.api.get_class_by_name("class_2")
        self.assertRegexpMatches(self.generator.generate_dtor_decl(class_desc),
                                "public:\s*~class_2\(\)\s*;")

    def test_generate_dtor_decl_2(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.generate_dtor_decl(class_desc),
                                "public:\s*~class_1_inner_class_1\(\)\s*;")

    def test_generate_allocator_decl_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.generate_allocator_decl(class_desc),
                                'extern\s*"C"\s*void\s*\*\s*allocateclass_1class_1_inner_class_1\(void\s*\*\s*impl\);')
