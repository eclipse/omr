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
A module for generating the JitBuilder C++ client API.

By convention, functions in this module that start with "generate_"
or "get_" return generated code as a string. Functions that start with
"write_" take as first argument a writer-like object whose `write()`
method is called to write the generated code. Passing a file handle
as this object will therefore write the generated code to a the
corresponding file. The documentation for several "writer_" services
also describes how the generated code works.
"""

import os
import datetime
import json
import shutil
import argparse
from genutils import *

class CppGenerator:

    def __init__(self, api, headerdir, extras):
        self.api = api

        # Mapping between API type descriptions and C++ data types
        self.builtin_type_map = { "none": "void"
                                , "boolean": "bool"
                                , "integer": "size_t"
                                , "int8": "int8_t"
                                , "int16": "int16_t"
                                , "int32": "int32_t"
                                , "int64": "int64_t"
                                , "uint32": "uint32_t"
                                , "float": "float"
                                , "double": "double"
                                , "pointer": "void *"
                                , "ppointer": "void **"
                                , "unsignedInteger": "size_t"
                                , "constString": "const char *"
                                , "string": "char *"
                                }

        # List of files to be included in the client API implementation.
        self.impl_include_files = self.gen_api_impl_includes(api.classes(), headerdir)

        # Classes that have extra APIs, not covered in the master description
        self.classes_with_extras = extras

        self.allocator_setter_name = "setAllocators"

    # Generic utilities ##################################################

    def get_copyright_header(self):
        return """\
/*******************************************************************************
 * Copyright (c) {0}, {0} IBM Corp. and others
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
        """.format(datetime.datetime.now().year)

    def get_common_system_includes(self):
        return ["stdint.h", "stddef.h"]

    def gen_api_impl_includes(self, classes_desc, api_headers_dir):
        """
        Generates a list of files to be included in the client
        API implementation.

        Two headers are included for each top-level (non-nested) class
        in the API description: one is the implementation header in `ilgen/`,
        the other is the header produced by this generator. In addition,
        a `Macros.hpp` is included as it contains some utilities used
        in the generated code.
        """
        files = [os.path.join("ilgen", c.name() + ".hpp") for c in classes_desc]
        files += [os.path.join(api_headers_dir, "Macros.hpp")]
        files += [os.path.join(api_headers_dir, c.name() + ".hpp") for c in classes_desc]
        return files

    def generate_include(self, path):
        """Returns an #include directive for a given path."""
        return '#include "{}"\n'.format(path)

    def get_class_name(self, c):
        """
        Returns the name of a given class in the client API implementation.

        If the class is a nested class, then the name is prefixed with name
        of all containing classes, separated by the scope resolution operator.
        """
        return "::".join(c.containing_classes() + [c.name()])

    def get_client_class_name(self, c):
        """
        Returns the name of a given class in the client API implementation,
        prefixed with the name of all contianing classes.
        """
        return self.get_class_name(c)

    def get_impl_class_name(self, c):
        """
        Returns the name of a given class in the JitBuilder implementation,
        prefixed with the name of all containing classes.
        """
        return "TR::{}".format(self.get_class_name(c))

    def get_client_type(self, t, namespace=""):
        """
        Returns the C++ type to be used in the client API implementation
        for a given type name, prefixing with a given namespace if needed.
        """
        return "{ns}{t} *".format(ns=namespace,t=self.get_client_class_name(t.as_class())) if t.is_class() else self.builtin_type_map[t.name()]

    def get_impl_type(self, c):
        """
        Returns the C++ type to be used in the JitBuilder implementation
        for a given type name, prefixing with a given namespace if needed.
        """
        return "{} *".format(self.get_impl_class_name(c.as_class())) if c.is_class() else self.builtin_type_map[c.name()]

    def generate_static_cast(self, t, v):
        """Generate a static cast of the value `v` to type `t`."""
        return "static_cast<{}>({})".format(t,v)

    def to_impl_cast(self, c, v):
        """
        Constructs a cast of the value `v` to the type of the
        implementation class `c`.
        """
        b = c.base()
        v = v if b.name() == c.name() else self.generate_static_cast(self.get_impl_type(b.as_type()), v)
        return self.generate_static_cast(self.get_impl_type(c.as_type()),v)

    def to_opaque_cast(self, v, from_c):
        """
        Constructs a cast of the value `v` from the type of the
        implementation class `from_c` to an opaque pointer type.
        """
        b = from_c.base()
        v = v if b.name() == from_c.name() else self.generate_static_cast(self.get_impl_type(b.as_type()), v)
        return self.generate_static_cast("void *", v)

    def to_client_cast(self, c, v):
        """
        Constructs a cast of the value `v` to the type of the
        client API class `c`.
        """
        return self.generate_static_cast(self.get_client_type(c.as_type()), v)

    def grab_impl(self, v, t):
        """
        Constructs an expression that grabs the implementation object
        from a client API object `v` and with type name `t`.
        """
        return self.to_impl_cast(t.as_class(), "{v} != NULL ? {v}->_impl : NULL".format(v=v)) if t.is_class() else v

    def generate_parm(self, parm_desc, namespace="", is_client=True):
        """
        Produces a parameter declaration from a given parameter description.
        The parameter declaration is usable in a function declaration.
        """
        fmt = "{t}* {n}" if parm_desc.is_in_out() or parm_desc.is_array() else "{t} {n}"
        t = parm_desc.type()
        t = self.get_client_type(t, namespace) if is_client else self.get_impl_type(t)
        return fmt.format(t=t,n=parm_desc.name())

    def generate_parm_list(self, parms_desc, namespace="", is_client=True):
        """
        Produces a stringified comma seperated list of parameter
        declarations from a list of parameter descriptions. The list
        is usable as the parameter listing of a function declaration.
        """
        return ", ".join([ self.generate_parm(p, namespace=namespace, is_client=is_client) for p in parms_desc ])

    def generate_vararg_parm_list(self, parms_desc):
        """
        Produces a stringified comma separated list of parameter
        declarations for a vararg function.
        """
        return ", ".join(["..." if p.can_be_vararg() else self.generate_parm(p) for p in parms_desc])

    def generate_arg(self, parm_desc):
        """
        Produces an argument variable from a parameter description.
        The produced value is usable as the "variable" for accessing
        the specified parameter.
        """
        n = parm_desc.name()
        t = parm_desc.type()
        return (n + "Arg") if parm_desc.is_in_out() or parm_desc.is_array() else self.grab_impl(n,t)

    def generate_arg_list(self, parms_desc):
        """
        Produces a stringified comma separated list of argument
        variables, given a list of parameter descriptions. The
        produced list can be used to forward the arguments from
        a caller to a callee function.
        """
        return ", ".join([ self.generate_arg(a) for a in parms_desc ])

    def callback_thunk_name(self, class_desc, callback_desc):
        """
        Produces the name of a callback thunk.

        Thunks are used for setting callbacks to the C++ client API.
        A thunk simply calls the corresponding client API service on
        the given client object.
        """
        return "{cname}Callback_{callback}".format(cname=class_desc.name(), callback=callback_desc.name())

    def impl_getter_name(self, class_desc):
        """
        Produces the name of the callback that, given a client object,
        returns the corresponding JitBuilder implementation of the object.
        """
        return "getImpl_{}".format(class_desc.name())

    def get_allocator_name(self, class_desc):
        """Produces the name of an API client object allocator."""
        return "allocate" + self.get_class_name(class_desc).replace("::", "")

    # header utilities ###################################################

    def generate_field_decl(self, field, with_visibility = True):
        """
        Produces the declaration of a client API field from
        its description, specifying its visibility as required.
        """
        t = self.get_client_type(field.type())
        n = field.name()
        v = "public: " if with_visibility else ""
        return "{visibility}{type} {name};\n".format(visibility=v, type=t, name=n)

    def generate_class_service_decl(self, service,is_callback=False):
        """
        Produces the declaration for a client API class service
        from its description.
        """
        vis = service.visibility() + ": "
        static = "static " if service.is_static() else ""
        qual = ("virtual " if is_callback else "") + static
        ret = self.get_client_type(service.return_type())
        name = service.name()
        parms = self.generate_parm_list(service.parameters())

        decl = "{visibility}{qualifier}{rtype} {name}({parms});\n".format(visibility=vis, qualifier=qual, rtype=ret, name=name, parms=parms)
        if service.is_vararg():
            parms = self.generate_vararg_parm_list(service.parameters())
            decl = decl + "{visibility}{qualifier}{rtype} {name}({parms});\n".format(visibility=vis,qualifier=qual,rtype=ret,name=name,parms=parms)

        return decl

    def generate_ctor_decl(self, ctor_desc, class_name):
        """
        Produces the declaration of a client API class constructor
        from its description and the name of its class.
        """
        v = ctor_desc.visibility() + ": "
        parms = self.generate_parm_list(ctor_desc.parameters())
        decls = "{visibility}{name}({parms});\n".format(visibility=v, name=class_name, parms=parms)
        return decls

    def generate_dtor_decl(self, class_desc):
        """
        Produces the declaration of client API class destructor
        from the class's name.
        """
        return "public: ~{cname}();\n".format(cname=class_desc.name())

    def generate_allocator_decl(self, class_desc):
        """Produces the declaration of a client API object allocator."""
        return 'extern "C" void * {alloc}(void * impl);\n'.format(alloc=self.get_allocator_name(class_desc))

    def write_allocator_decl(self, writer, class_desc):
        """
        Write the allocator declarations for a given client API
        class and its contained classes.
        """
        for c in class_desc.inner_classes():
            self.write_allocator_decl(writer, c)
        writer.write(self.generate_allocator_decl(class_desc))

    def write_class_def(self, writer, class_desc):
        """Write the definition of a client API class from its description."""

        name = class_desc.name()
        has_extras = name in self.classes_with_extras

        inherit = ": public {parent}".format(parent=self.get_class_name(class_desc.parent())) if class_desc.has_parent() else ""
        writer.write("class {name}{inherit} {{\n".format(name=name,inherit=inherit))
        writer.indent()

        # write nested classes
        writer.write("public:\n")
        for c in class_desc.inner_classes():
            self.write_class_def(writer, c)

        # write fields
        for field in class_desc.fields():
            decl = self.generate_field_decl(field)
            writer.write(decl)

        # write needed impl field
        if not class_desc.has_parent():
            writer.write("public: void* _impl;\n")

        for ctor in class_desc.constructors():
            decls = self.generate_ctor_decl(ctor, name)
            writer.write(decls)

        # write impl constructor
        writer.write("public: explicit {name}(void * impl);\n".format(name=name))

        # write impl init service delcaration
        writer.write("protected: void initializeFromImpl(void * impl);\n")

        dtor_decl = self.generate_dtor_decl(class_desc)
        writer.write(dtor_decl)

        for callback in class_desc.callbacks():
            decl = self.generate_class_service_decl(callback, is_callback=True)
            writer.write(decl)

        for service in class_desc.services():
            decl = self.generate_class_service_decl(service)
            writer.write(decl)

        if has_extras:
            writer.write(self.generate_include('{}ExtrasInsideClass.hpp'.format(class_desc.name())))

        writer.outdent()
        writer.write('};\n')

    # source utilities ###################################################

    def write_ctor_impl(self, writer, ctor_desc):
        """
        Write the definition of a client API class constructor from
        its description and its class description.

        Client API constructors first construct an instance of the
        corresponding JitBuilder class by invoking the corresponding
        construct and saving a pointer to the object in a private
        field. Constructors then set themselves (the current
        object) as client object of the implementation object and
        call the common initialization function. An abstract example:

        ```
        Constructor(...) {
            arg_set
            auto impl = ::new TR::Constructor(...);
            arg_return
            impl->setClient(this);
            initializeFromImpl(impl);
        }
        ```
        """
        class_desc = ctor_desc.owning_class()
        parms = self.generate_parm_list(ctor_desc.parameters())
        name = ctor_desc.name()
        full_name = self.get_class_name(class_desc)
        inherit = ": {parent}((void *)NULL)".format(parent=self.get_class_name(class_desc.parent())) if class_desc.has_parent() else ""

        writer.write("{cname}::{name}({parms}){inherit} {{\n".format(cname=full_name, name=name, parms=parms, inherit=inherit))
        writer.indent()
        for parm in ctor_desc.parameters():
            self.write_arg_setup(writer, parm)
        args = self.generate_arg_list(ctor_desc.parameters())
        writer.write("auto * impl = ::new {cname}({args});\n".format(cname=self.get_impl_class_name(class_desc), args=args))
        for parm in ctor_desc.parameters():
            self.write_arg_return(writer, parm)
        writer.write("{impl_cast}->setClient(this);\n".format(impl_cast=self.to_impl_cast(class_desc,"impl")))
        writer.write("initializeFromImpl({});\n".format(self.to_opaque_cast("impl",class_desc)))
        writer.outdent()
        writer.write("}\n")

    def write_impl_ctor_impl(self, writer, class_desc):
        """
        Writes the implementation of the special client API class
        constructor that takes a pointer to an implementation object.

        The special constructor only has one parameter: an opaque pointer
        to an implementation object. It simply sets itself as the client
        object corresponding to the passed-in implementation object and
        calls the common initialization function. This is the constructor
        that constructors of derived client API classes should invoke.
        """
        name = class_desc.name()
        full_name = self.get_class_name(class_desc)
        inherit = ": {parent}((void *)NULL)".format(parent=self.get_class_name(class_desc.parent())) if class_desc.has_parent() else ""

        writer.write("{cname}::{name}(void * impl){inherit} {{\n".format(cname=full_name, name=name, inherit=inherit))
        writer.indent()
        writer.write("if (impl != NULL) {\n")
        writer.indent()
        writer.write("{impl_cast}->setClient(this);\n".format(impl_cast=self.to_impl_cast(class_desc,"impl")));

        impl = "impl" if not class_desc.has_parent() else self.to_opaque_cast("static_cast<{}>(impl)".format(self.get_impl_type(class_desc.as_type())),class_desc)
        writer.write("initializeFromImpl({});\n".format(impl))

        writer.outdent()
        writer.write("}\n")
        writer.outdent()
        writer.write("}\n")

    def write_impl_initializer(self, writer, class_desc):
        """
        Writes the implementation of the client API class common
        initialization function from the description of a class.

        The common initialization function is called by all client
        API class constructors to initialize the class's fields,
        including the private pointer to the corresponding
        implementation object. It is also used to set any callbacks
        to the client on the implementation object.
        """
        full_name = self.get_class_name(class_desc)
        impl_cast = self.to_impl_cast(class_desc,"_impl")

        writer.write("void {cname}::initializeFromImpl(void * impl) {{\n".format(cname=full_name))
        writer.indent()

        if class_desc.has_parent():
            writer.write("{parent}::initializeFromImpl(impl);\n".format(parent=self.get_class_name(class_desc.parent())))
        else:
            writer.write("_impl = impl;\n")

        for field in class_desc.fields():
            fmt = "GET_CLIENT_OBJECT(clientObj_{fname}, {ftype}, {impl_cast}->{fname});\n"
            writer.write(fmt.format(fname=field.name(), ftype=field.type().name(), impl_cast=impl_cast))
            writer.write("{fname} = clientObj_{fname};\n".format(fname=field.name()))

        for callback in class_desc.callbacks():
            fmt = "{impl_cast}->{registrar}(reinterpret_cast<void*>(&{thunk}));\n"
            registrar = callback_setter_name(callback)
            thunk = self.callback_thunk_name(class_desc, callback)
            writer.write(fmt.format(impl_cast=impl_cast,registrar=registrar,thunk=thunk))

        # write setting of the impl getter
        impl_getter = self.impl_getter_name(class_desc)
        writer.write("{impl_cast}->setGetImpl(&{impl_getter});\n".format(impl_cast=impl_cast,impl_getter=impl_getter))

        writer.outdent()
        writer.write("}\n")

    def write_arg_setup(self, writer, parm):
        """
        Writes the setup needed in the implementation of a client
        API service to forward arguments to the corresponding
        JitBuilder implementation function.

        When an argument is an array or is and in-out parameter
        (passed by reference), it must be converted to the equivalent
        construct for the implementation objects. That is, an array
        of client objects must be converted into an array of
        implementation objects and a reference to a client object
        must converted into a reference to an implementation object.

        Since these act as in-out parameters and can visibly altered,
        the user arguments must be reconstructed at the end of a call.
        The `write_arg_return()` function generates this code.
        """
        if parm.is_in_out():
            assert parm.type().is_class()
            t = self.get_class_name(parm.type().as_class())
            writer.write("ARG_SETUP({t}, {n}Impl, {n}Arg, {n});\n".format(t=t, n=parm.name()))
        elif parm.is_array():
            assert parm.type().is_class()
            t = self.get_class_name(parm.type().as_class())
            writer.write("ARRAY_ARG_SETUP({t}, {s}, {n}Arg, {n});\n".format(t=t, n=parm.name(), s=parm.array_len()))

    def write_arg_return(self, writer, parm):
        """
        Writes the argument reconstruction for in-out parameters
        and array parameters.

        Since calling the implementation function can alter the
        arguments passed, the client arguments must be reconstructed
        from the implementation objects. After the implementation
        function is called, the implementation arguments must be
        transformed back into client objects, which must then be
        assigned to the client arguments. Effectively, the
        generated code undoes what the code generated by
        `write_arg_setup()` does.
        """
        if parm.is_in_out():
            assert parm.type().is_class()
            t = self.get_class_name(parm.type().as_class())
            writer.write("ARG_RETURN({t}, {n}Impl, {n});\n".format(t=t, n=parm.name()))
        elif parm.is_array():
            assert parm.type().is_class()
            t = self.get_class_name(parm.type().as_class())
            writer.write("ARRAY_ARG_RETURN({t}, {s}, {n}Arg, {n});\n".format(t=t, n=parm.name(), s=parm.array_len()))

    def write_class_service_impl(self, writer, desc, class_desc):
        """
        Writes the implementation of a client API class service.

        Services simply forward their arguments to the corresponding
        functions on the implementation object and forward returned
        values. Special setup for the arguments that require it is
        done before the implementation function  is called and the
        corresponding reconstruction is done after.
        """
        rtype = self.get_client_type(desc.return_type())
        name = desc.name()
        parms = self.generate_parm_list(desc.parameters())
        class_name = self.get_class_name(class_desc)
        writer.write("{rtype} {cname}::{name}({parms}) {{\n".format(rtype=rtype, cname=class_name, name=name, parms=parms))
        writer.indent()

        if desc.is_impl_default():
            writer.write("return 0;\n")
        else:
            for parm in desc.parameters():
                self.write_arg_setup(writer, parm)

            args = self.generate_arg_list(desc.parameters())
            impl_call = "{impl_cast}->{sname}({args})".format(impl_cast=self.to_impl_cast(class_desc,"_impl"),sname=name,args=args)
            if "none" == desc.return_type().name():
                writer.write(impl_call + ";\n")
                for parm in desc.parameters():
                    self.write_arg_return(writer, parm)
            elif desc.return_type().is_class():
                writer.write("{rtype} implRet = {call};\n".format(rtype=self.get_impl_type(desc.return_type()), call=impl_call))
                for parm in desc.parameters():
                    self.write_arg_return(writer, parm)
                writer.write("GET_CLIENT_OBJECT(clientObj, {t}, implRet);\n".format(t=desc.return_type().name()))
                writer.write("return clientObj;\n")
            else:
                writer.write("auto ret = " + impl_call + ";\n")
                for parm in desc.parameters():
                    self.write_arg_return(writer, parm)
                writer.write("return ret;\n")

        writer.outdent()
        writer.write("}\n")

        if desc.is_vararg():
            writer.write("\n")
            self.write_vararg_service_impl(writer, desc, class_name)

    def write_vararg_service_impl(self, writer, desc, class_name):
        """
        Writes the implementation of a client API class
        vararg service.

        Vararg functions are expected to have equivalent functions
        that take an array instead of the vararg. As such,
        their implementation simply re-packages the vararg into
        an array and calls the array version of the function.
        """
        rtype = self.get_client_type(desc.return_type())
        name = desc.name()
        vararg = desc.parameters()[-1]
        vararg_type = self.get_client_type(vararg.type())

        parms = self.generate_vararg_parm_list(desc.parameters())
        writer.write("{rtype} {cname}::{name}({parms}) {{\n".format(rtype=rtype,cname=class_name,name=name,parms=parms))
        writer.indent()

        args = ", ".join([p.name() for p in desc.parameters()])
        writer.write("{t}* {arg} = new {t}[{num}];\n".format(t=vararg_type,arg=vararg.name(),num=vararg.array_len()))
        writer.write("va_list vararg;\n")
        writer.write("va_start(vararg, {num});\n".format(num=vararg.array_len()))
        writer.write("for (int i = 0; i < {num}; ++i) {{ {arg}[i] = va_arg(vararg, {t}); }}\n".format(num=vararg.array_len(),arg=vararg.name(),t=vararg_type))
        writer.write("va_end(vararg);\n")
        get_ret = "" if "none" == desc.return_type().name() else "{rtype} ret = ".format(rtype=rtype)
        writer.write("{get_ret}{name}({args});\n".format(get_ret=get_ret,name=name,args=args))
        writer.write("delete[] {arg};\n".format(arg=vararg.name()))
        if "none" != desc.return_type().name():
            writer.write("return ret;\n")
        writer.outdent()
        writer.write("}\n")

    def generate_callback_parm_list(self, parm_descs):
        """
        Generates the parameter list declaration for a client
        API callback from a list of parameter descriptions.

        Callbacks have a special signature as they are functions
        that take as first parameter an opaque pointer to a
        client object. Any other parameters accepting an object
        type also use opaque pointers.

        The generated list is usable as the parameter declaraion
        of the callback thunk.
        """
        parms = [self.generate_parm(p) if p.type().is_builtin() else "void * {}".format(p.name()) for p in parm_descs]
        return list_str_prepend("void * clientObj", ", ".join(parms))

    def generate_callback_arg_list(self, parm_descs):
        """
        Generates a list of the arguments of a client API callback
        from a list of parameter descriptions.

        The generated list is usable in the callback thunk to
        forwarding the arguments to function implementing the
        callback body.
        """
        cast_fmt = "static_cast<{t}>({n})"
        args= [self.generate_arg(p) if p.type().is_builtin() else cast_fmt.format(t=self.get_client_type(p.type()),n=p.name()) for p in parm_descs]
        return ", ".join(args)

    def write_callback_thunk(self, writer, class_desc, callback_desc):
        """
        Writes the implementation of a callback thunk.

        A callback is implemented as a thunk that forwards the call
        to a member function on the object that the the callback is
        called on (the first argument of the callback).
        An abstract example:

        ```
            callback_thunk(obj, ...) {
                return obj->callback(...);
            }
        ```
        """
        rtype = self.get_client_type(callback_desc.return_type())
        thunk = self.callback_thunk_name(class_desc, callback_desc)
        ctype = self.get_client_type(class_desc.as_type())
        callback = callback_desc.name()
        args = self.generate_callback_arg_list(callback_desc.parameters())
        parms = self.generate_callback_parm_list(callback_desc.parameters())

        writer.write('extern "C" {rtype} {thunk}({parms}) {{\n'.format(rtype=rtype,thunk=thunk,parms=parms))
        writer.indent()
        writer.write("{ctype} client = {clientObj};\n".format(ctype=ctype,clientObj=self.to_client_cast(class_desc,"clientObj")))
        writer.write("return client->{callback}({args});\n".format(callback=callback,args=args))
        writer.outdent()
        writer.write("}\n")

    def write_impl_getter_impl(self, writer, class_desc):
        """
        Writes the implementation of the callback used to get
        the implementation object from a client API object.

        The generated code simply casts the client object from
        an opaque pointer to the type for the object and then
        returns the field pointing to the implementation as
        an opaque pointer.
        """
        getter = self.impl_getter_name(class_desc)
        client_cast = self.to_client_cast(class_desc, "client")
        impl_cast = self.to_impl_cast(class_desc,"{client_cast}->_impl".format(client_cast=client_cast))
        writer.write('extern "C" void * {getter}(void * client) {{\n'.format(getter=getter))
        writer.indent()
        writer.write("return {impl_cast};\n".format(impl_cast=impl_cast))
        writer.outdent()
        writer.write("}\n")

    def write_allocator_impl(self, writer, class_desc):
        """
        Writes the implementation of the client API object allocator.

        By default, the allocator simply uses the global operator `new`
        to allocated client objects and returns it as an opaque pointer.
        """
        allocator = self.get_allocator_name(class_desc)
        name = self.get_class_name(class_desc)
        writer.write('extern "C" void * {alloc}(void * impl) {{\n'.format(alloc=allocator))
        writer.indent()
        writer.write("return new {name}(impl);\n".format(name=name))
        writer.outdent()
        writer.write("}\n")

    def write_class_impl(self, writer, class_desc):
        """Write the implementation of a client API class."""

        name = class_desc.name()
        full_name = self.get_class_name(class_desc)

        # write source for inner classes first
        for c in class_desc.inner_classes():
            self.write_class_impl(writer, c)

        # write callback thunk definitions
        for callback in class_desc.callbacks():
            self.write_callback_thunk(writer, class_desc, callback)
            writer.write("\n")

        # write impl getter defintion
        self.write_impl_getter_impl(writer, class_desc)
        writer.write("\n")

        # write constructor definitions
        for ctor in class_desc.constructors():
            self.write_ctor_impl(writer, ctor)
        writer.write("\n")

        self.write_impl_ctor_impl(writer, class_desc)
        writer.write("\n")

        # write class initializer (called from all constructors)
        self.write_impl_initializer(writer, class_desc)
        writer.write("\n")

        # write destructor definition
        writer.write("{cname}::~{name}() {{}}\n".format(cname=full_name,name=name))
        writer.write("\n")

        # write service definitions
        for s in class_desc.services():
            self.write_class_service_impl(writer, s, class_desc)
            writer.write("\n")

        # write service definitions
        for s in class_desc.callbacks():
            self.write_class_service_impl(writer, s, class_desc)
            writer.write("\n")

        self.write_allocator_impl(writer, class_desc)
        writer.write("\n")

    # common utilities ##############################################

    def generate_allocator_setting(self, class_desc):
        """
        Given a class description, generates a list of calls that set
        the client API object allocators for the class itself and any
        nested classes.

        The generated code allows the implementation to allocated objects
        by invoking these allocators as callbacks.
        """
        registrations = []
        for c in class_desc.inner_classes():
            registrations += self.generate_allocator_setting(c)
        registrations += "{iname}::setClientAllocator(OMR::JitBuilder::{alloc});\n".format(iname=self.get_impl_class_name(class_desc),cname=self.get_class_name(class_desc),alloc=self.get_allocator_name(class_desc))
        return registrations

    def generate_service_decl(self, service, namespace=""):
        """Generates a client API service declaraion from its description"""
        ret = self.get_client_type(service.return_type())
        name = service.name()
        parms = self.generate_parm_list(service.parameters(), namespace)

        decl = "{rtype} {name}({parms});\n".format(rtype=ret, name=name, parms=parms)
        if service.is_vararg():
            parms = self.generate_vararg_parm_list(service.parameters())
            decl = decl + "{rtype} {name}({parms});\n".format(rtype=ret,name=name,parms=parms)

        return decl

    def write_common_decl(self, writer, api_desc):
        """
        Writes the declarations of all client API (non-class) services
        from an API description.
        """
        writer.write(self.get_copyright_header())
        writer.write("\n")
        namespaces = api_desc.namespaces()

        writer.write("#ifndef {}_INCL\n".format(api_desc.project()))
        writer.write("#define {}_INCL\n\n".format(api_desc.project()))

        # write some needed includes and defines
        for header in self.get_common_system_includes():
            writer.write(self.generate_include(header))
        writer.write("\n")

        writer.write("#define TOSTR(x) #x\n")
        writer.write("#define LINETOSTR(x) TOSTR(x)\n\n")

        # include headers for each defined class
        for c in api_desc.get_class_names():
            writer.write(self.generate_include(c + ".hpp"))
        writer.write("\n")

        # write declarations for all services
        ns = "::".join(namespaces) + "::"
        for service in api_desc.services():
            decl = self.generate_service_decl(service, namespace=ns)
            writer.write(decl)
        writer.write("\n")

        writer.write("#endif // {}_INCL\n".format(api_desc.project()))

    def generate_impl_service_import(self, service_desc):
        """
        Generates an import for the implementation function
        corresponding to a client API service.
        """
        rt = self.get_impl_type(service_desc.return_type())
        n = get_impl_service_name(service_desc)
        ps=self.generate_parm_list(service_desc.parameters(), is_client=False)
        return "extern {rt} {n}({ps});\n".format(rt=rt, n=n, ps=ps)

    def write_allocators_setter(self, writer, api_desc):
        """
        Writes the implementation of a function that sets the
        allocator functions for all client API classes.

        The generated function will be called by any service
        that has the `sets-allocators` flag.
        """
        writer.write("static void {}() {{\n".format(self.allocator_setter_name))
        writer.indent()
        for c in api_desc.classes():
            writer.write("".join(self.generate_allocator_setting(c)))
        writer.outdent()
        writer.write("}\n")

    def write_service_impl(self, writer, desc, namespace=""):
        """
        Writes the implementation of client API (non-class) service.

        The same approach is used to implement non-class services
        as is used for class services.
        """
        rtype = self.get_client_type(desc.return_type())
        name = desc.name()
        parms = self.generate_parm_list(desc.parameters(), namespace)
        writer.write("{rtype} {name}({parms}) {{\n".format(rtype=rtype, name=name, parms=parms))
        writer.indent()

        if desc.sets_allocators():
            writer.write("{}();\n".format(self.allocator_setter_name))

        for parm in desc.parameters():
            self.write_arg_setup(writer, parm)

        args = self.generate_arg_list(desc.parameters())
        impl_call = "{sname}({args})".format(sname=get_impl_service_name(desc),args=args)
        if "none" == desc.return_type().name():
            writer.write(impl_call + ";\n")
            for parm in desc.parameters():
                self.write_arg_return(writer, parm)
        elif desc.return_type().is_class():
            writer.write("{rtype} implRet = {call};\n".format(rtype=self.get_impl_type(desc.return_type()), call=impl_call))
            for parm in desc.parameters():
                self.write_arg_return(writer, parm)
            writer.write("GET_CLIENT_OBJECT(clientObj, {t}, implRet);\n".format(t=desc.return_type().name()))
            writer.write("return clientObj;\n")
        else:
            writer.write("auto ret = " + impl_call + ";\n")
            for parm in desc.parameters():
                self.write_arg_return(writer, parm)
            writer.write("return ret;\n")
        writer.outdent()
        writer.write("}\n")

        if desc.is_vararg():
            writer.write("\n")
            self.write_vararg_service_impl(writer, desc, class_name)

    def write_common_impl(self, writer, api_desc):
        """Writes the implementation of all client API (non-class) services."""

        writer.write(self.get_copyright_header())
        writer.write("\n")

        for h in self.impl_include_files:
            writer.write(self.generate_include(h))
        writer.write("\n")

        for service in api_desc.services():
            writer.write(self.generate_impl_service_import(service))
        writer.write("\n")

        self.write_allocators_setter(writer, api_desc)
        writer.write("\n")

        ns = "::".join(api_desc.namespaces()) + "::"
        for service in api_desc.services():
            self.write_service_impl(writer, service, ns)
            writer.write("\n")

    def write_class_header(self, writer, class_desc, namespaces, class_names):
        """Writes the header for a client API class from the class description."""

        has_extras = class_desc.name() in self.classes_with_extras

        writer.write(self.get_copyright_header())
        writer.write("\n")

        writer.write("#ifndef {}_INCL\n".format(class_desc.name()))
        writer.write("#define {}_INCL\n\n".format(class_desc.name()))

        for header in self.get_common_system_includes():
            writer.write(self.generate_include(header))

        if class_desc.has_parent():
            writer.write(self.generate_include("{}.hpp".format(class_desc.parent().name())))

        if has_extras:
            writer.write(self.generate_include('{}ExtrasOutsideClass.hpp'.format(class_desc.name())))
        writer.write("\n")

        # open each nested namespace
        for n in namespaces:
            writer.write("namespace {} {{\n".format(n))
        writer.write("\n")

        writer.write("// forward declarations for all API classes\n")
        for c in class_names:
            writer.write("class {};\n".format(c))
        writer.write("\n")

        self.write_class_def(writer, class_desc)
        writer.write("\n")

        self.write_allocator_decl(writer, class_desc)
        writer.write("\n")

        # close each opened namespace
        for n in reversed(namespaces):
            writer.write("}} // {}\n".format(n))
        writer.write("\n")

        writer.write("#endif // {}_INCL\n".format(class_desc.name()))

    def write_class_source(self, writer, class_desc, namespaces, class_names):
        """
        Writes the implementation (source) for a client API class
        from the class description.
        """
        writer.write(self.get_copyright_header())
        writer.write("\n")

        # don't bother checking what headers are needed, include everything
        for h in self.impl_include_files:
            writer.write(self.generate_include(h))
        writer.write("\n")

        # open each nested namespace
        for n in namespaces:
            writer.write("namespace {} {{\n".format(n))
        writer.write("\n")

        self.write_class_impl(writer, class_desc)

        # close each openned namespace
        for n in reversed(namespaces):
            writer.write("}} // {}\n".format(n))

    def write_class(self, header_dir, source_dir, class_desc, namespaces, class_names):
        """Generates and writes a client API class from its description."""

        cname = class_desc.name()
        header_path = os.path.join(header_dir, cname + ".hpp")
        source_path = os.path.join(source_dir, cname + ".cpp")
        with open(header_path, "w") as writer:
            self.write_class_header(PrettyPrinter(writer), class_desc, namespaces, class_names)
        with open(source_path, "w") as writer:
            self.write_class_source(PrettyPrinter(writer), class_desc, namespaces, class_names)

# main generator #####################################################

if __name__ == "__main__":
    default_dest = os.path.join(os.getcwd(), "client")
    parser = argparse.ArgumentParser()
    parser.add_argument("--sourcedir", type=str, default=default_dest,
                        help="destination directory for the generated source files")
    parser.add_argument("--headerdir", type=str, default=default_dest,
                        help="destination directory for the generated header files")
    parser.add_argument("description", help="path to the API description file")
    args = parser.parse_args()

    with open(args.description) as api_src:
        api_description = APIDescription.load_json_file(api_src)

    generator = CppGenerator(api_description, args.headerdir, ["TypeDictionary"])

    namespaces = api_description.namespaces()
    class_names = api_description.get_class_names()
    # impl_include_files = gen_api_impl_includes(api_description.classes(), args.headerdir)

    for class_desc in api_description.classes():
        generator.write_class(args.headerdir, args.sourcedir, class_desc, namespaces, class_names)
    with open(os.path.join(args.headerdir, "JitBuilder.hpp"), "w") as writer:
        generator.write_common_decl(PrettyPrinter(writer), api_description)
    with open(os.path.join(args.sourcedir, "JitBuilder.cpp"), "w") as writer:
        generator.write_common_impl(PrettyPrinter(writer), api_description)

    extras_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "extras", "cpp")
    names = os.listdir(extras_dir)
    for name in names:
        if name.endswith(".hpp"):
            shutil.copy(os.path.join(extras_dir,name), os.path.join(args.headerdir,name))
