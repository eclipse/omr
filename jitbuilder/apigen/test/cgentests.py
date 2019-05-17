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

import unittest

import genutils
import cgen

class CGeneratorTest(unittest.TestCase):
    """Tests for the CGenerator class."""

    def setUp(self):
        with open("test/test_sample.json") as f:
            self.api = genutils.APIDescription.load_json_file(f)
        self.generator = cgen.CGenerator(self.api, "")

    def test_get_client_class_name_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertEqual("class_1class_1_inner_class_1", self.generator.get_client_class_name(class_desc))

    def test_get_client_type_1(self):
        type_desc = genutils.APIType("class_1", self.api)
        self.assertEqual("struct class_1 *", self.generator.get_client_type(type_desc))

    def test_get_client_type_2(self):
        type_desc = genutils.APIType("class_1_inner_class_1", self.api)
        self.assertEqual("struct class_1class_1_inner_class_1 *", self.generator.get_client_type(type_desc, "NO_NAMESPACE"))

    def test_to_client_cast_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.to_client_cast(class_desc, "foo"),
                                "static_cast<struct class_1class_1_inner_class_1\s*\*>\(\s*foo\s*\)")

    def test_get_ctor_name_1(self):
        ctor_desc = self.api.get_class_by_name("class_1_inner_class_1").constructors()[0]
        self.assertEqual("Newclass_1_inner_class_1", self.generator.get_ctor_name(ctor_desc))

    def test_get_ctor_name_2(self):
        ctor_desc = self.api.get_class_by_name("class_1").constructors()[0]
        self.assertEqual("Newclass_1ctor_overload_1", self.generator.get_ctor_name(ctor_desc))

    def test_get_dtor_name_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertEqual("Deleteclass_1class_1_inner_class_1", self.generator.get_dtor_name(class_desc))

    def test_get_dtor_name_2(self):
        class_desc = self.api.get_class_by_name("class_2")
        self.assertEqual("Deleteclass_2", self.generator.get_dtor_name(class_desc))

    def test_generate_parm_callback_1(self):
        callback = self.api.get_class_by_name("class_1").callbacks()[0]
        self.assertRegexpMatches(self.generator.generate_parm_callback(callback),
                                "bool\s*\(\s*\*\s*class_1_callback_1_\s*\)\(\s*bool\s*\)")

    def test_generate_ctor_parms_1(self):
        ctor = self.api.get_class_by_name("class_1").constructors()[0]
        self.assertRegexpMatches(self.generator.generate_ctor_parms(ctor),
                                "bool\s*\(\s*\*\s*class_1_callback_1_\s*\)\(\s*bool\s*\)")

    def test_generate_field_decl_1(self):
        field = self.api.get_class_by_name("class_1").fields()[0]
        self.assertRegexpMatches(self.generator.generate_field_decl(field, True),
                                "float\s+class_1_field_1\s*;")

    def test_generate_field_decl_2(self):
        field = self.api.get_class_by_name("class_1").fields()[1]
        self.assertRegexpMatches(self.generator.generate_field_decl(field, False),
                                "double\s+class_1_field_2\s*;")

    def test_generate_class_service_decl_1(self):
        service = self.api.get_class_by_name("class_1").services()[0]
        self.assertRegexpMatches(self.generator.generate_class_service_decl(service),
                                "const\s*char\s*\*\s*c1_class_1_service_1\(\s*struct\s+class_1\s*\*\s*self,\s*const\s*char\s*\*\s*class_1_service_1_parm\s*\);")

    def test_generate_class_service_decl_2(self):
        service = self.api.get_class_by_name("class_1").services()[1]
        self.assertRegexpMatches(self.generator.generate_class_service_decl(service),
                                "void\s*c1_class_1_service_2overload\(\s*struct\s+class_1\s*\*\s*self\s*\);")

    def test_generate_ctor_decl_1(self):
        ctor = self.api.get_class_by_name("class_2").constructors()[0]
        self.assertRegexpMatches(self.generator.generate_ctor_decl(ctor),
                                "class_2\s*\*\s*Newclass_2\(\)\s*;")

    def test_generate_ctor_decl_2(self):
        ctor = self.api.get_class_by_name("class_1_inner_class_1").constructors()[0]
        self.assertRegexpMatches(self.generator.generate_ctor_decl(ctor),
                                "class_1\s*\*\s*Newclass_1_inner_class_1\(\)\s*;")

    def test_generate_ctor_decl_3(self):
        ctor = self.api.get_class_by_name("class_1").constructors()[0]
        self.assertRegexpMatches(self.generator.generate_ctor_decl(ctor),
                                "class_1\s*\*\s*Newclass_1ctor_overload_1\(\s*bool\s*\(\s*\*\s*class_1_callback_1_\s*\)\(\s*bool\s*\)\s*\)\s*;")

    def test_generate_allocator_decl_1(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.generate_allocator_decl(class_desc),
                                "extern\s*void\s*\*\s*allocateclass_1class_1_inner_class_1\(void\s*\*\s*impl\);")
    
    def test_get_impl_getter_name_1(self):
        class_desc = self.api.get_class_by_name("class_1")
        self.assertEqual(self.generator.impl_getter_name(class_desc),
                        "getImpl_class_1")

    def test_impl_getter_name_2(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertEqual(self.generator.impl_getter_name(class_desc),
                        "getImpl_class_1class_1_inner_class_1")

    def test_generate_buildil_callback_decl_1(self):
        class_desc = self.api.get_class_by_name("class_1")
        self.assertRegexpMatches(self.generator.generate_buildil_callback_decl(class_desc), "void\s+c1_setBuildIlCallback\(\s*struct\s+class_1\s*\*\s+self,\s*bool\(\*\s*buildIl_Callback\s*\)\(struct\s+IlBuilder\s*\*\)\)\s*;")

    def test_generate_buildil_callback_decl_2(self):
        class_desc = self.api.get_class_by_name("class_1_inner_class_1")
        self.assertRegexpMatches(self.generator.generate_buildil_callback_decl(class_desc), "void\s+c1ic1_setBuildIlCallback\(\s*struct\s+class_1_inner_class_1\s*\*\s+self,\s*bool\(\*\s*buildIl_Callback\s*\)\(struct\s+IlBuilder\s*\*\)\)\s*;")