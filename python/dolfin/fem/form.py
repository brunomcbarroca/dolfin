import dolfin.cpp as cpp
import ufl
import ffc

class Form(cpp.fem.Form):
    def __init__(self, form, **kwargs):

        # Check form argument
        if not isinstance(form, ufl.Form):
            raise RuntimeError("Expected a ufl.Form.")

        sd = form.subdomain_data()
        self.subdomains, = list(sd.values())  # Assuming single domain
        domain, = list(sd.keys())  # Assuming single domain
        mesh = domain.ufl_cargo()

        # Having a mesh in the form is a requirement
        if mesh is None:
            raise RuntimeError("Expecting to find a Mesh in the form.")

        form_compiler_parameters = kwargs.pop("form_compiler_parameters", None)
        ufc_form = ffc.jit(form, form_compiler_parameters)
        ufc_form = cpp.fem.make_ufc_form(ufc_form[0])

        function_spaces = [func.function_space() for func in form.arguments()]

        cpp.fem.Form.__init__(self, ufc_form, function_spaces)

        original_coefficients = form.coefficients()
        self.coefficients = []
        for i in range(self.num_coefficients()):
            j = self.original_coefficient_position(i)
            self.coefficients.append(original_coefficients[j].cpp_object())

        # Type checking coefficients
        if not all(isinstance(c, (cpp.function.GenericFunction, cpp.function.MultiMeshFunction))
                   for c in self.coefficients):
            # Developer note:
            # The form accepts a MultiMeshFunction but does not set the
            # correct coefficients. This is done in assemble_multimesh
            # at the moment
            coefficient_error = "Error while extracting coefficients. "
            raise TypeError(coefficient_error +
                            "Either provide a dict of cpp.function.GenericFunctions, " +
                            "or use Function to define your form.")

        for i in range(self.num_coefficients()):
            if isinstance(self.coefficients[i], cpp.function.GenericFunction):
                self.set_coefficient(i, self.coefficients[i])


        # Attach mesh (because function spaces and coefficients may be
        # empty lists)
        if not function_spaces:
            self.set_mesh(mesh)

        # Attach subdomains to C++ Form if we have them
        subdomains = self.subdomains.get("cell")
        if subdomains is not None:
            self.set_cell_domains(subdomains)
        subdomains = self.subdomains.get("exterior_facet")
        if subdomains is not None:
            self.set_exterior_facet_domains(subdomains)
        subdomains = self.subdomains.get("interior_facet")
        if subdomains is not None:
            self.set_interior_facet_domains(subdomains)
        subdomains = self.subdomains.get("vertex")
        if subdomains is not None:
            self.set_vertex_domains(subdomains)