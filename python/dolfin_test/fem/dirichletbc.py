import types
from six import string_types
import hashlib

import dolfin_test.cpp as cpp
import dijitso
import ffc


def jit_generate(inside_code, module_name, signature, parameters):

    template_code = """
// Based on https://gcc.gnu.org/wiki/Visibility
#if defined _WIN32 || defined __CYGWIN__
    #ifdef __GNUC__
        #define DLL_EXPORT __attribute__ ((dllexport))
    #else
        #define DLL_EXPORT __declspec(dllexport)
    #endif
#else
    #define DLL_EXPORT __attribute__ ((visibility ("default")))
#endif

#include <dolfin/mesh/SubDomain.h>
#include <Eigen/Dense>

namespace dolfin
{{
  class {classname} : public SubDomain
  {{
     public:
       {members}

       {classname}()
          {{
            {constructor}
          }}

       // Return true for points inside the sub domain
       bool inside(const Eigen::Ref<Eigen::VectorXd>& x, bool on_boundary) const override
       {{
         return {inside};
       }}
  }};
}}

extern "C" DLL_EXPORT dolfin::SubDomain * create_{classname}()
{{
  return new dolfin::{classname};
}}

"""

    classname = signature
    code_c = template_code.format(inside=inside_code, classname=classname,
                                  members= "", constructor="")
    print(code_c)
    code_h = ""
    depends = []

    return code_h, code_c, depends


def compile_subdomain(inside_code):

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

    module_hash = hashlib.md5(inside_code.encode('utf-8')).hexdigest()
    module_name = "subdomain_" + module_hash
    module, signature = dijitso.jit(inside_code, module_name, params,
                                    generate=jit_generate)

    submodule = dijitso.extract_factory_function(module, "create_" + module_name)()
    print("JIT gives:", submodule, module, signature)

    sub_domain = cpp.mesh.make_dolfin_subdomain(submodule)
    return sub_domain


class CompiledSubDomain(cpp.mesh.SubDomain):
    def __new__(cls, inside_code):
        return compile_subdomain(inside_code)

    #def __init__(self, inside_code):
    #    pass


class DirichletBC(cpp.fem.DirichletBC):
    def __init__(self, *args, **kwargs):

        if len(args) != 3:
            raise(RuntimeError, "Not yet supported")

        if not isinstance(args[0], cpp.function.FunctionSpace):
            raise(RuntimeError, "First argument must be of type FunctionSpace")
        function_space = args[0]

        if not isinstance(args[1], cpp.function.GenericFunction):
            raise(RuntimeError, "Second argument must be of type GenericFunction")
        function = args[1]

        if isinstance(args[2], cpp.mesh.SubDomain):
            subdomain = args[2]
        elif isinstance(args[2], string_types):
            subdomain = CompiledSubDomain(args[2])
        else:
            raise(RuntimeError, "Third argument must be of type SubDomain or string")

        super().__init__(function_space, function, subdomain)
