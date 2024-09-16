# implementation.py -- Implementation class and FAUST source parser
#
# Roger B. Dannenberg
# Sep 2024

class Implementation:
    def __init__(self, before_lines, param_names, param_interp,
                 beyond_process, after_lines):
        self.before_lines = before_lines  # lines before "process("
        self.param_names = param_names  # list of parameter names
        self.param_interp = param_interp  # list of True (interpolated) and False
        self.beyond_process = beyond_process  # rest of line after "process(...)"
        self.after_lines = after_lines  # list of lines after line with "process"

    def __str__(self):
        return f"Implementation({self.before_lines}, " + \
               f"{self.param_names}, {self.param_interp}, " + \
               f"{self.beyond_process}, {self.after_lines})"

    def __repr__(self):
        return self.__str__()


def prepare_implementation(ugen_src):
    """
    extract the implementation, returning an Implementation with:
    - lines before the line with "process("
    - a list of parameter names from process definition
    - the remainder of the line with "process", beginning with "="
    - the lines after the line with "process("
    """
    params = None
    interpolated = []
    for i, line in enumerate(ugen_src):
        loc = line.find("declare interpolated")
        if loc >= 0:
            loc = line.find('"', loc)  # first quote
            if loc < 0:
                print("Error: could not find string after declare interpolated")
                return True
            loc2 = line.find('"', loc + 1)
            interpolated = line[loc + 1 : loc2].replace(",", " ").split()
        elif line.find("process") >= 0:
            paramline = i
            params = line
            break
    if params == None:
        print("Error: Could not find process in Faust code")
        exit(-1)

    loc = params.find("process")
    loc = params.find("(", loc + 7)
    if loc < 0:
        print('Error: Expected ( after "process"')
        exit(-1)
    loc2 = params.find(")", loc)
    if loc2 < 0:
        print('Error: Expected ) after "process("')
        exit(-1)
    loc3 = params.find("=", loc2)
    if loc3 < 0:
        print('Error: Expected = after "process(...)"')
        exit(-1)
    beyond_process = params[loc3 : ]
    param_names = params[loc + 1 : loc2].replace(",", " ").split()
    param_interp = [p in interpolated for p in param_names]
    return Implementation(ugen_src[0 : paramline], param_names, param_interp,
                          beyond_process, ugen_src[paramline + 1 : ])

