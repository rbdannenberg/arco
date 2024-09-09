# parameter representation
#
# Roger B. Dannenberg
# Sep 2024

# This module provides a class Param that represents a parameter
# Param is used for both Arco meta-data and Faust "process" parameters.
#
# Class Signature is a unit generator name, a list of Params, and a 
# Param for the output.

class Param:
    def __init__(self, param, source=None):
        """param is a string that describes a parameter. Syntax is
        <name>:[<count>]<ab>, where <name> is a parameter name (string),
        <count> is digits that specify the number of channels, and
        <ab> is one of 'a', 'b', or 'ab' that give possible signal rates.
        If source is provided, the new Param object will inherit the name
        and channel count and fixedness from source, and the initial
        <name>: can be omitted. This is to simplify the derivation of
        a specialization of 'ab' parameters to either 'a' or 'b',
        retaining the channel count.
        """
        print("starting Param(", param, source, ")")
        # parse out name (up to ":"), count (digits), and type (remainder):
        loc = param.find(":")
        if loc < 0:
            name = ""  # no name
        else:
            name = param[0 : loc]
            param = param[loc + 1 : ]
        count = ''
        while len(param) > 0 and param[0].isdigit():
            count += param[0]
            param = param[1 : ]

        if source:
            self.name = name if name else source.name
            self.chans = int(count) if count else source.chans
            self.fixed = source.fixed
        else:
            self.name = name
            self.chans = int(count) if len(count) > 0 else 1
            self.fixed = (count != '')

        self.abtype = param
        self.compute_fulltype()
        print("ending Param(", self.fulltype, ") prints as", self)


    def compute_fulltype(self):
        """recompute the full type string from the name, channel count,
        and abtype
        """
        self.fulltype = self.name + ":" + \
                (str(self.chans) if self.fixed else "") + self.abtype

        
    def __str__(self):
        return f"Param({repr(self.fulltype)})"


    def __repr__(self):
        return self.__str__()


    def fix(self):
        """make the parameter count fixed"""
        if not self.fixed:  # channel count was not specified but
            # has a default value of 1
            assert(self.chans == 1)
            self.fixed = True
            self.compute_fulltype()



class Signature:
    def __init__(self, name, params, output):
        self.name = name
        self.params = params  # a list of Param objects
        self.output = output  # a Param object


    def __str__(self):
        return f"Signature({self.name}, {self.params}, {self.output})"


    def __repr__(self):
        return self.__str__()
    

    def check_signature(self, param_names):
        """Check this signature for channel count consistency with
        param_names, which is a list of implementation parameter names from
        the FAUST "process" definition. If output is fixed, then all
        parameters must be fixed, and len(param_names) must match the number
        implied by this signature.
        """
        print("check_signature: ", self, "param_names", param_names)
        # check for consistent fixed properties. All Params should match output:
        for p in self.params:
            if p.fixed != self.output.fixed:
                print("Error: inconsistent fixed properties in params or output:")
                print("    params for", self.name, "are", self.params)
                print("    output for", self.naem, "is", self.output)
                break
        # check for consistent count
        count = sum([p.chans for p in self.params])
        if count != len(param_names):
            print("Error: signature for", self.name, "(", self.params, ") implies",
                  count, "process parameters, but process has", len(param_names),
                  "parameters.")



def get_signatures(src):
    """Find all arco type signatures.

    src is a list of strings. Each string is a line of text from the 
        .ugen file.

    A signature looks like "sine(freq: a, amp: b): a" where a 
    and b mean "audio" or "block" (rate), and "ab" is also allowed. The 
    signature can also be "sine(freq: 2a, amp: b): 2a" where digits
    indicate a fixed number of channels.

    Return a list of Signatures, one for each signature found in src.
    """
    signature_set = []
    for i, line in enumerate(src):
        line = line.strip()
        if len(line) == 0:
            continue
        if line[0] == "#":
            continue
        if line == "FAUST":
            break

        # we found a signature line
        signature_set.append(parse_signature(line, i + 1))  # first line = 1
    return signature_set



def parse_signature(line, line_no):
    """Parse an arco signature line into a Signature object."""
    line = line.strip()
    loc = line.find("(")
    name = line[0 : loc].strip()
    loc = loc + 1
    param_list = []
    line = line[loc : ].replace(" ", "").replace("\t", "")
    pend = line.find(")")
    if pend < 0:
        print("Error in line", line_no, "Expected ) after parameters.")
        return
    param_list = []
    for p in line[0 : pend].split(","):
       if p.find(":") < 0:
              print("Error in line", line_no, "Expected <name>: in parameter.")
              return
       param_list.append(Param(p))
    if line[pend : pend + 2] != "):":
        print("Error in line", line_no, "Expected ): after parameters.")
        return
    output_type = Param(line[pend + 1 : ])  # uses the ":" to indicate no name
    update_params(param_list, output_type)
    return Signature(name, param_list, output_type)



def update_params(params, output_param):
    """makes all parameters and output be fixed channel if any have
    fixed channel counts
    """
    # find out if any parameter has a fixed channel count
    isfixed = any(param.fixed for param in params)
    isfixed = isfixed or output_param.fixed
    # if yes, then all parameters have fixed channel count
    if isfixed:
        for param in params:
            param.fix()
        output_param.fix()



"""
def parse_arco_signature(line):
    ""Find quoted string of arco parameters in line and construct a list
    of Params. A signature looks like "sine(freq: a, amp: b): a" where a 
    and b mean "audio" or "block" (rate), and "ab" is also allowed. The 
    signature can also be "sine(freq: 2a, amp: b): 2a" where digits
    indicate a fixed number of channels.
    ""
    loc = line.find('"')
    loc2 = line.find('"', loc + 1)
    if loc < 0:
        print('Error: Expected " after arco_signature')
        exit(-1)
    if loc2 < 0:
        print('Error: Expected quoted string after arco_signature')
        exit(-1)
    arco_params = line[loc + 1 : loc2].split(',')
    for i, parm in enumerate(arco_params):
        loc = parm.find("(")
        if loc < 0:
            arco_params[i] = (parm, None)
        else:
            loc2 = parm.find(")")
            if loc2 < 0:
                print('Error: Expected ) after ( in arco_signature')
                arco_params[i] = (parm[0 : loc], None)
            else:
                arco_params[i] = (parm[0 : loc], int(parm[loc + 1 : loc2]))
    return arco_params
"""



