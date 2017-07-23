# -*- coding: utf-8 -*-
from __future__ import print_function

__all__ = ["CompiledExpression", "UserExpression"]

# Python imports
import types
from six import add_metaclass
from six import string_types
from six.moves import xrange as range
from functools import reduce
import weakref
import hashlib

import dijitso

# Import UFL and SWIG-generated extension module (DOLFIN C++)
import ufl
from ufl import product
from ufl.utils.indexflattening import flatten_multiindex, shape_to_strides
import dolfin.cpp as cpp
import numpy

#from dolfin import warning, error

class _InterfaceExpression(cpp.function.Expression):
    def __init__(self, user_expression, *args, **kwargs):
        self.user_expression = user_expression

        cpp.function.Expression.__init__(self, *args, **kwargs)

        def eval(self, values, x):
            self.user_expression.eval(values, x)
        def eval_cell(self, values, x, cell):
            self.puser_expression.eval(values, x, cell)

        # Attach eval functions of they exists in the user Expression
        # class
        if hasattr(user_expression, 'eval'):
            self.eval = types.MethodType(eval, self)
        if hasattr(user_expression, 'eval_cell'):
            self.eval_cell = types.MethodType(eval_cell, self)


class UserExpression(ufl.Coefficient):
    def __init__(self, function_space=None, element=None, degree=None):
        """Create an Expression."""

        #if element is None and degree is None:
        #    raise RuntimeError('UserExpression must specific a FiniteElement or a dgeree')

        #if element is None:
        #    # Create UFL element
        #    element = ufl.FiniteElement(family, mesh.ufl_cell(), degree,
        #                                form_degree=None)

        ufl.Coefficient.__init__(self, function_space)
        #cpp.function.Expression.__init__(self, 1)
        #cpp.function.Expression.__init__(self, self.ufl_shape)

        self._cpp_object = _InterfaceExpression(self)

        value_shape = tuple(self.value_dimension(i)
                            for i in range(self.value_rank()))

        #element = _auto_select_element_from_shape(value_shape, degree, cell)
        # Check if scalar, vector or tensor valued
        family = "Lagrange"
        degree = 2
        cell = None
        if len(value_shape) == 0:
            element = ufl.FiniteElement(family, cell, degree)
        elif len(value_shape) == 1:
            element = ufl.VectorElement(family, cell, degree, dim=value_shape[0])
        else:
            element = ufl.TensorElement(family, cell, degree, shape=value_shape)

        # Initialize UFL base class
        ufl_function_space = ufl.FunctionSpace(None, element)
        ufl.Coefficient.__init__(self, ufl_function_space, count=self.id())

    def value_rank(self):
        return self._cpp_object.value_rank()

    def value_dimension(self, i):
        return self._cpp_object.value_dimension(i)

    def id(self):
        return self._cpp_object.id()

    def cpp_object(self):
        """Return the underling cpp.Expression object"""
        return self._cpp_object


def jit_generate(class_data, module_name, signature, parameters):

    template_code = """

#include <dolfin/function/Expression.h>
#include <Eigen/Dense>

namespace dolfin
{{
  class {classname} : public Expression
  {{
     public:
       {members}

       {classname}()
          {{
            {constructor}
          }}

       void eval(Eigen::Ref<Eigen::VectorXd> values, const Eigen::Ref<Eigen::VectorXd> x) const override
       {{
{statement}
       }}

       void set_property(std::string name, double value)
       {{
{set_props}
       }}

       double get_property(std::string name) const
       {{
{get_props}
       }}

  }};
}}

extern "C" __attribute__ ((visibility ("default"))) dolfin::Expression * create_{classname}()
{{
  return new dolfin::{classname};
}}

"""
    _get_props = """          if (name == "{name}") return {name};"""
    _set_props = """          if (name == "{name}") {{ {name} = value; return; }}"""

    statements = class_data["statements"]
    statement = ""
    for i, val in enumerate(statements):
        statement += "          values[" + str(i) + "] = " + val + ";\n"

    constructor = ""
    members = ""
    set_props = ""
    get_props = ""

    # Add code for setting and getting property values
    properties = class_data["properties"]
    for k in properties:
        value = properties[k]
        members += "double " + k + ";\n"
        set_props += _set_props.format(name=k)
        get_props += _get_props.format(name=k)

    # Set the value_shape
    if len(statements) > 1:
        constructor += "_value_shape.push_back(" + str(len(statements)) + ");"

    classname = signature
    code_c = template_code.format(statement=statement, classname=classname,
                                  members=members, constructor=constructor,
                                  set_props=set_props, get_props=get_props)
    code_h = ""
    depends = []

    return code_h, code_c, depends


def compile_expression(statements, properties):
    """Compile a user C(++) string to a Python object"""

    import pkgconfig
    if not pkgconfig.exists('dolfin'):
        raise RuntimeError("Could not find DOLFIN pkg-config file. Please make sure appropriate paths are set.")

    # Get pkg-config data
    d = pkgconfig.parse('dolfin')

    # Set compiler/build options
    params = dijitso.params.default_params()
    params['build']['include_dirs'] = d["include_dirs"]
    params['build']['libs'] = d["libraries"]
    params['build']['lib_dirs'] = d["library_dirs"]

    if isinstance(statements, string_types):
        statements = tuple((statements,))

    if not isinstance(statements, tuple):
        raise RuntimeError("Expression must be a string, or a tuple of strings")

    class_data = {'statements': statements, 'properties': properties}

    module_hash = hashlib.md5("".join(statements).encode('utf-8')).hexdigest()
    module_name = "dolfin_expression_" + module_hash
    module, signature = dijitso.jit(class_data, module_name, params,
                                    generate=jit_generate)

    submodule = dijitso.extract_factory_function(module, "create_" + module_name)()

    expression = cpp.function.make_dolfin_expression(submodule)

    # Set properties to initial values
    for k in properties:
        expression.set_property(k, properties[k])

    return expression

class CompiledExpression(ufl.Coefficient):
    def __init__(self, statements, **kwargs):

        degree = kwargs.pop("degree", None)
        element = kwargs.pop("element", None)

        properties = kwargs
        for k in properties:
            if not isinstance(k, string_types):
                raise KeyError("Invalid key")
            if not isinstance(properties[k], float):
                raise ValueError("Invalid value")

        self._cpp_object = compile_expression(statements, properties)

        if element is None:
            if (degree == 0):
                element = ufl.FiniteElement("DG", None, 0)
            else:
                element = ufl.FiniteElement("Lagrange", None, degree)

        ufl_function_space = ufl.FunctionSpace(None, element)
        ufl.Coefficient.__init__(self, ufl_function_space, count=self.id())

    def __getattr__(self, name):
        "Pass attribites through to (JIT compiled) Expression object"
        if hasattr(self._cpp_object, name):
            return self._cpp_object.get_property(name)
        else:
            raise(AttributeError)

    def __setattr__(self, name, value):
        if name.startswith("_"):
            super().__setattr__(name, value)
        else:
            self._cpp_object.set_property(name, value)

    def __call__(self, x):
        return self._cpp_object(x)

    def id(self):
        return self._cpp_object.id()

    def cpp_object(self):
        return self._cpp_object
