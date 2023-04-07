# faust code generator - expands to b-rate and a-rate types
#
# Roger B. Dannenberg
# Jan 2022

"""
creates .dsp files for faust from simplified .dsp files, creating
various combinations of b-rate and a-rate parameters and output.
"""

import sys

def main():
    # find files
    if len(sys.argv) != 2:
        print("Usage: python3 u2f.py classname[.ugen]")
        exit(-1)
    source = sys.argv[1]  # can pass "sine.ugen" or just "sine"
    if source.find(".") < 0:
        source += ".ugen"
    print("**** Translating", source)
    with open(source, "r") as srcf:
        src = srcf.readlines()
    signature_set = []
    impl = None
    for i, line in enumerate(src):
        line = line.strip()
        if len(line) == 0:
            continue
        if line[0] == "#":
            continue
        if line == "FAUST":
            impl = src[i + 1 : ]
            break
        # we found a signature line
        loc = line.find("(")
        name = line[0 : loc]
        loc = loc + 1
        params = []
        line = line[loc : ].replace(" ", "").replace("\t", "")
        pend = line.find(")")
        params = line[0 : pend].split(",")
        if line[pend : pend + 2] != "):":
            print("Error in line", i, "Expected ): after parameters.")
            return
        outputtype = line[pend + 2 : ]
        signature_set.append((name, params, outputtype))
    if impl == None:
        print("Error: did not find FAUST to begin implementation.")
        return
    classnames = []
    for s in signature_set:
        expand_signature(s[0], s[1], s[2], "", impl)
        # gather up all classnames, e.g. ["sine", "sineb"] for later:
        if s[0] not in classnames:
            classnames.append(s[0])
    classnamelc = source[ : -5]
    shellfilename = "generate_" + classnamelc + ".sh"
    print("**** Generating", shellfilename)
    with open(shellfilename, "w") as genf:
        print("#!/bin/sh", file=genf)
        for cn in classnames:
            print("py ../preproc/f2a.py", cn, file=genf)
        for cn in classnames:
            print("py ../../o2/preproc/o2idc.py", cn + ".cpp", file=genf)


def expand_signature(name, params, outputtype, types, impl):
    """
    recursively generate all permutations of parameters and write .dsp file
    indices is a string of indices into parameters. E.g., it could be 
    "a" or "aba" if there are 3 parameters. If len(types) is 
    len(params), we have a full specification, so generate. Otherwise,
    recursively expand by passing indices + ab, where ab is each value
    of the corresponding element in params
    
    """
    if len(types) < len(params):
        for typ in params[len(types)]:
            expand_signature(name, params, outputtype, types + typ, impl)
    else:  # base case: we have a specific signature to generate
        # first, get parameter names
        params = None
        for i, line in enumerate(impl):
            if line.find("process") >= 0:
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
        params = params[loc + 1 : loc2].replace(",", " ").split()

        outfile = name + "_" + types + "_" + outputtype + ".dsp"
        with open(outfile, "w") as outf:
            print('declare name "' + name + '";', file=outf)
            for line in impl[0 : paramline]:
                print(line, file=outf, end="")
            # insert control for each b-rate parameter
            plist = ""
            for i, typ in enumerate(types):
                if typ == 'a':
                    plist = plist + ", " + params[i]
                else:
                    print(params[i], '= nentry("' + params[i] + \
                          '", 0, 0, 1, 0.1);', file=outf)
            if 'b' in types:  # blank line after nentry and before process
                print("", file=outf)
            processline = "process"
            if plist != "":
                processline = processline + "(" + plist[2 : ] + ")"
            processline = processline + " " + beyond_process
            print(processline, file=outf, end="")
            for line in impl[paramline + 1 : ]:
                print(line, file=outf, end="")
            
main()
