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

"""
A module for generating the JitBuilder C client API.
"""

import os
import shutil
import argparse
import cppgen
import genutils

class CGenerator(cppgen.CppGenerator):

    def __init__(self, api, headerdir):
        cppgen.CppGenerator.__init__(self, api, headerdir, [])

    def get_client_class_name(self, c):
        """
        Returns the name of a given class in the client API implementation,
        prefixed with the name of all contianing classes.
        """
        return "".join(c.containing_classes() + [c.name()])

    def get_client_type(self, t, ns=""):
        """
        Returns the C type to be used in the client API implementation
        for a given type name, prefixing with a given namespace if needed.

        Because C does not support the notion of namespaces, the namespace
        argument to this function is forced to be an empty string.
        """
        return cppgen.CppGenerator.get_client_type(self, t, "")

    def get_service_call_name(self, service, owner=None):
        """
        Produces the full name of a service usable as a call.
        """
        owner = owner if owner is not None else service.owning_class()
        prefix = "{}_".format(owner.short_name()) if owner is not None else ""
        return "{prefix}{name}".format(prefix=prefix, name=service.name())

    def get_service_decl_name(self, service, owner=None):
        """
        Produces the full name of a service for use in a function declarations.
        """
        # in C, we can use the same name as in a function call
        return self.get_service_call_name(service, owner)

    def get_service_impl_name(self, service, owner=None):
        """
        Produces the full name of a service for use in a function definition.
        """
        # in C, we can use the same name as in a function call
        return self.get_service_call_name(service, owner)

    def get_ctor_name(self, ctor_desc):
        """
        Produces the name of a client API class constructor from the class's desctipion.
        """
        return "New{}".format(ctor_desc.overload_name())

    def get_impl_ctor_name(self, class_desc):
        return "New{}WithImpl".format(class_desc.name())

    def get_dtor_name(self, class_desc):
        """
        Produces the name of a client API class destructor from the class's desctipion.
        """
        return "Delete{}".format(class_desc.name())

    def get_self_parm_for_class(self, class_desc):
        return "{} self".format(self.get_client_type(class_desc.as_type()))

    def get_self_parm(self, service):
        return self.get_self_parm_for_class(service.owning_class())

    def get_self_arg(self, service):
        return "self"

    def generate_service_parms(self, service):
        """
        Produces a stringified, comma separated list of parameter
        declarations for a service.
        """
        return genutils.list_str_prepend(self.get_self_parm(service), self.generate_parm_list(service.parameters()))

    def generate_service_vararg_parms(self, service):
        """
        Produces a stringified, comma separated list of parameter
        declarations for a service.
        """
        return genutils.list_str_prepend(self.get_self_parm(service), self.generate_vararg_parm_list(service.parameters()))

    def generate_parm_callback(self, callback_desc):
        """
        Produces the declaration of a function parameter for accepting a
        callback as a function pointer.
        """
        rtype = self.get_client_type(callback_desc.return_type())
        name = callback_desc.overload_name()
        arg_types = ", ".join([self.get_client_type(p.type()) for p in callback_desc.parameters()])
        return "{rtype} (*{name}_)({arg_types})".format(rtype=rtype, name=name, arg_types=arg_types)

    def generate_arg_callback(self, callback_desc):
        """
        Produces an expression representing a callback function pointer passed in as an argument.
        """
        return "{}_".format(callback_desc.overload_name())

    def generate_ctor_parms(self, ctor_desc):
        parms = [self.generate_parm(p) for p in ctor_desc.parameters()]
        callback_parms = [self.generate_parm_callback(c) for c in ctor_desc.owning_class().callbacks()]
        return ", ".join(parms + callback_parms)

    def generate_get_client_field(self, name):
        """
        Produces a string that "gets" a field with name 'name' from a client class.
        """
        return "self->{}".format(name)

    def get_class_inititializer(self, class_desc):
        """
        Produces a description of a service to initialize an instance of a client API class.
        """
        init_service = {
            "name":"initializeFromImpl",
            "overloadsuffix": "",
            "flags": "",
            "return": "none",
            "parms": [ {"name":"impl","type":"pointer"} ]
        }
        return genutils.APIService(init_service, class_desc, class_desc.api)

    # header utilities ###################################################

    def generate_field_decl(self, field, with_visibility = False):
        """
        Produces the declaration of a client API field from
        its description, specifying its visibility as required.
        """
        t = self.get_client_type(field.type())
        n = field.name()
        return "{type} {name};\n".format(type=t, name=n)

    def generate_service_args(self, service):
        """
        Produces a stringified, comma separated list of parameter
        declarations for a service.
        """
        return genutils.list_str_prepend(self.get_self_parm(service), self.generate_parm_list(service.parameters()))

    def generate_class_service_decl(self, service,is_callback=False):
        """
        Produces the declaration for a client API class service
        from its description.
        """
        vis = service.visibility() + ": "
        ret = self.get_client_type(service.return_type())
        class_prefix = service.owning_class().short_name()
        name = self.get_service_decl_name(service)
        parms = self.generate_service_parms(service)

        decl = "{rtype} {name}({parms});\n".format(rtype=ret, name=name, parms=parms)
        if service.is_vararg():
            parms = self.generate_service_vararg_parms(service)
            decl = decl + "{rtype} {name}_v({parms});\n".format(rtype=ret, name=name, parms=parms)

        return decl

    def generate_ctor_decl(self, ctor_desc):
        """
        Produces the declaration of a client API class constructor
        from its description and the name of its class.
        """
        rtype = self.get_client_type(ctor_desc.owning_class().as_type())
        name = self.get_ctor_name(ctor_desc)
        parms = self.generate_ctor_parms(ctor_desc)
        return "{rtype} {name}({parms});\n".format(rtype=rtype, name=name, parms=parms)

    def generate_impl_ctor_decl(self, class_desc):
        """
        Produces the declaration of the client API class constructor
        that takes an impl object as argument.
        """
        ctype = self.get_client_type(class_desc.as_type())
        name = self.get_impl_ctor_name(class_desc)
        return "{ctype} {name}(void * impl);\n".format(ctype=ctype, name=name)

    def generate_dtor_decl(self, class_desc):
        """
        Produces the declaration of client API class destructor
        from the class's name.
        """
        return "void {name}({parm});\n".format(name=self.get_dtor_name(class_desc), parm=self.get_self_parm_for_class(class_desc))

    def generate_allocator_decl(self, class_desc):
        """Produces the declaration of a client API object allocator."""
        return 'extern void * {alloc}(void * impl);\n'.format(alloc=self.get_allocator_name(class_desc))

    def write_class_def(self, writer, class_desc):
        """Write the definition of a client API class from its description."""

        name = class_desc.name()
        has_extras = name in self.classes_with_extras

        writer.write("struct {name} {{\n".format(name=name))
        if class_desc.has_parent():
            # "inheritance" is represented using a data member called `super` that
            # points to the instance of the "parent" class
            writer.write("{} super;\n".format(self.get_client_type(class_desc.parent().as_type())))

        # write fields
        for field in class_desc.fields():
            decl = self.generate_field_decl(field)
            writer.write(decl)

        # write needed impl field
        if not class_desc.has_parent():
            writer.write("void * _impl;\n")

        if has_extras:
            writer.write(self.generate_include('{}ExtrasInsideClass.hpp'.format(class_desc.name())))

        writer.write("};\n")

        for ctor in class_desc.constructors():
            decls = self.generate_ctor_decl(ctor)
            writer.write(decls)

        # write impl constructor
        writer.write(self.generate_impl_ctor_decl(class_desc))

        # write impl init service delcaration
        init_service = self.get_class_inititializer(class_desc)
        writer.write(self.generate_class_service_decl(init_service))

        dtor_decl = self.generate_dtor_decl(class_desc)
        writer.write(dtor_decl)

        for callback in class_desc.callbacks():
            decl = self.generate_class_service_decl(callback, is_callback=True)
            writer.write(decl)

        service_names = [s.name() for s in class_desc.services()]
        for service in class_desc.services():
            if service.suffix() == "" or service.overload_name() not in service_names:
                # if a service already exists with the same overload name, don't declare the current one
                decl = self.generate_class_service_decl(service)
                writer.write(decl)

        # write nested classes
        for c in class_desc.inner_classes():
            self.write_class_def(writer, c)

    def write_class_header(self, writer, class_desc, namespaces, class_names):
        """Writes the header for a client API class from the class description."""

        has_extras = class_desc.name() in self.classes_with_extras

        writer.write(self.get_copyright_header())
        writer.write("\n")

        writer.write("#ifndef {}_INCL\n".format(class_desc.name()))
        writer.write("#define {}_INCL\n".format(class_desc.name()))
        writer.write("#ifdef __cplushplus")
        writer.write('extern "C" {\n')
        writer.write("#endif // __cplushplus")

        if class_desc.has_parent():
            writer.write(self.generate_include("{}.hpp".format(class_desc.parent().name())))

        if has_extras:
            writer.write(self.generate_include('{}ExtrasOutsideClass.hpp'.format(class_desc.name())))
        writer.write("\n")

        writer.write("// forward declarations for all API classes\n")
        for c in class_names:
            writer.write("struct {};\n".format(c))
        writer.write("\n")

        self.write_class_def(writer, class_desc)
        writer.write("\n")

        self.write_allocator_decl(writer, class_desc)
        writer.write("\n")

        writer.write("#ifdef __cplushplus\n")
        writer.write('} // extern "C" \n')
        writer.write("#endif // __cplushplus\n")
        writer.write("#endif // {}_INCL\n".format(class_desc.name()))

    # source utilities ###################################################

    def write_ctor_impl(self, writer, ctor_desc):
        """
        Write the definition of a client API class constructor from
        its description and its class description.
        """
        class_desc = ctor_desc.owning_class()
        rtype = self.get_client_type(ctor_desc.owning_class().as_type())
        name = self.get_ctor_name(ctor_desc)
        parms = self.generate_ctor_parms(ctor_desc)
        writer.write("{rtype} {name}({parms}) {{\n".format(rtype=rtype, name=name, parms=parms))

        # allocate client object
        writer.write("auto * self = new {};\n".format(class_desc.name()))

        # allocate implementation object
        for parm in ctor_desc.parameters():
            self.write_arg_setup(writer, parm)
        args = self.generate_arg_list(ctor_desc.parameters())
        writer.write("auto * impl = new {cname}({args});\n".format(cname=self.get_impl_class_name(class_desc), args=args))
        for parm in ctor_desc.parameters():
            self.write_arg_return(writer, parm)
        writer.write("{impl_cast}->setClient(self);\n".format(impl_cast=self.to_impl_cast(class_desc,"impl")))

        impl_cast = self.to_opaque_cast("impl",class_desc)
        if class_desc.has_parent():
            # allocate parent
            writer.write("{} = {}({});\n".format(self.generate_get_client_field("super"), self.get_impl_ctor_name(class_desc.parent()), impl_cast))

        init_service = self.get_class_inititializer(class_desc)
        writer.write("{}(self, {});\n".format(self.get_service_call_name(init_service), impl_cast))

        # set callbacks
        for callback in class_desc.callbacks():
            fmt = "impl->{registrar}({callback});\n"
            registrar = genutils.callback_setter_name(callback)
            callback = self.generate_arg_callback(callback)
            writer.write(fmt.format(registrar=registrar, callback=callback))


        writer.write("return self;\n")
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
        name = self.get_impl_ctor_name(class_desc)
        rtype = self.get_client_type(class_desc.as_type())

        writer.write("{rtype} {name}(void * impl) {{\n".format(rtype=rtype, name=name))

        # allocate client object
        writer.write("auto * self = new {};\n".format(class_desc.name()))

        # set client object on impl if needed
        #
        # Note that it's important for this to happen *before* a parent is allocated.
        # Otherwise the parent will erroneously overwrite the client object.
        impl_cast = self.to_impl_cast(class_desc,"impl")
        writer.write("if ({}->client() == NULL) {{\n".format(impl_cast))
        writer.write("{impl_cast}->setClient(self);\n".format(impl_cast=impl_cast))
        writer.write("}\n")

        if class_desc.has_parent():
            # allocate parent
            writer.write("{} = {}(impl);\n".format(self.generate_get_client_field("super"), self.get_impl_ctor_name(class_desc.parent())))

        # call initializer
        init_service = self.get_class_inititializer(class_desc)
        writer.write("{}(self, {});\n".format(self.get_service_call_name(init_service), self.to_opaque_cast("impl",class_desc)))

        # return new client object
        writer.write("return self;\n")

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

        init_service = self.get_class_inititializer(class_desc)
        writer.write("void {name}({parms}) {{\n".format(name=self.get_service_impl_name(init_service), parms=self.generate_service_parms(init_service)))

        full_name = self.get_class_name(class_desc)
        impl_cast = self.to_impl_cast(class_desc,"impl")

        if class_desc.has_parent():
            name = self.get_service_call_name(self.get_class_inititializer(class_desc.parent()))
            parms = genutils.list_str_prepend(self.generate_get_client_field("super"), "impl")
            writer.write("{name}({parms});\n".format(name=name, parms=parms))
        else:
            writer.write("{} = impl;\n".format(self.generate_get_client_field("_impl")))

        for field in class_desc.fields():
            fmt = "GET_CLIENT_OBJECT(clientObj_{fname}, {ftype}, {impl_cast}->{fname});\n"
            writer.write(fmt.format(fname=field.name(), ftype=field.type().name(), impl_cast=impl_cast))
            writer.write("{field} = clientObj_{fname};\n".format(field=self.generate_get_client_field(field.name()), fname=field.name()))

        # write setting of the impl getter
        impl_getter = self.impl_getter_name(class_desc)
        writer.write("{impl_cast}->setGetImpl(&{impl_getter});\n".format(impl_cast=impl_cast,impl_getter=impl_getter))

        writer.write("}\n")

    def write_dtor_impl(self, writer, class_desc):
        writer.write("void {}({}) {{\n".format(self.get_dtor_name(class_desc), self.get_self_parm_for_class(class_desc)))

        # delete parent object if present
        if class_desc.has_parent():
            writer.write("{}({});\n".format(self.get_dtor_name(class_desc.parent()), self.generate_get_client_field("super")))

        writer.write("{}(self);\n".format(self.get_dtor_name(class_desc)))

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
        writer.write("return {}(impl);\n".format(self.get_impl_ctor_name(class_desc)))
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
        self.write_dtor_impl(writer, class_desc)
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

        self.write_class_impl(writer, class_desc)

    # common utilities ###################################################

    def write_class(self, header_dir, source_dir, class_desc, namespaces, class_names):
        """Generates and writes a client API class from its description."""

        cname = class_desc.name()
        header_path = os.path.join(header_dir, cname + ".h")
        source_path = os.path.join(source_dir, cname + ".cpp")
        with open(header_path, "w") as writer:
            self.write_class_header(writer, class_desc, namespaces, class_names)
        with open(source_path, "w") as writer:
            self.write_class_source(writer, class_desc, namespaces, class_names)

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
        api_description = genutils.APIDescription.load_json_file(api_src)

    generator = CGenerator(api_description, args.headerdir)

    namespaces = api_description.namespaces()
    class_names = api_description.get_class_names()

    for class_desc in api_description.classes():
        generator.write_class(args.headerdir, args.sourcedir, class_desc, namespaces, class_names)
    with open(os.path.join(args.headerdir, "JitBuilder.h"), "w") as writer:
        generator.write_common_decl(writer, api_description)
    with open(os.path.join(args.sourcedir, "JitBuilder.c"), "w") as writer:
        generator.write_common_impl(writer, api_description)

    extras_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "extras", "cpp")
    names = os.listdir(extras_dir)
    for name in names:
        if name.endswith(".hpp"):
            shutil.copy(os.path.join(extras_dir,name), os.path.join(args.headerdir,name))
