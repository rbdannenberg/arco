# faust to Arco translation
#
# Roger B. Dannenberg
# Jan 2022

""" 
Assumes you have Faust source files, e.g., sine_aa_a.dsp, If there are
no parameters, append an underscore anyway, e.g. "whitenoise__a.dsp"
has zero inputs and one output.

Pass the lower-cased class name on the command line, e.g. "py f2a.py
sine".

Underscores are not allowed in the main unit generator name.
Everything after the first "_" is called the *signature* and is not
part of the class name.

The program will search for sine_*.dsp with different combinations of
input rates, e.g. sine_ba_a.dsp would take a block-rate (or constant)
and an audio-rate and produce an audio-rate. You can add as many
combinations as you want, but due to the exponential growth of
combinations, e.g. aaaa, aaab, aaba, aabb, abaa, ..., it is allowed
for the list to be incomplete. At runtime, when a Ugen is
instantiated, either all inputs are b-rate or c-rate and the output is
b-rate, or Arco will upsample or downsample signal inputs if a perfect
match is not found.

When there is not a perfect match, e.g. input is abb_a but the
implementation provides only aba_a and aab_a, the unit generator must
select a "best match", which is done eagerly from left to right. In
this case, a??_a matches aba_a and aab_a, so both are possible. ?b?_a
matches aba_a but not aab_a, so we eliminate aab_a. Finally ??b_a does
not match the remaining implemenation aba_a, so the b-rate parameter
is upsampled to a-rate.

If the input were aaa_a, we would have chosen aab_a because of the
match to the second input (?a?_a). Therefore, the third parameter
would be downsampled to b-rate.

A parameter may be for initialization only, e.g. the initial phase
of an oscillator. These parameters are marked with "c", and if a
parameter is marked "c" for one .dsp file, it should be "c" in all.

Unit generators are either a-rate or b-rate, but not both. To
indicate the fixed output type, append "_a" or "_b" to the file name,
e.g. "sine_aa_a.dsp" or "lfo_bb_b.dsp".  In the case of b-rate-only
output, there should only be one .dsp file, e.g. "lfo_bb_b.dsp" or
"lfo_bc_b.dsp".  To get a choice of output rates, you must make
two Ugen classes, normally using a "b" suffix: Sine and Sineb.
The output rate is a class property.

If the output is "a", there should be one file with no "b" in the 
signature, and there may be other files where one or more "a" inputs
are changed to "b".

If the output is "b", there should be only one .dsp file for the
class, and it should have no "a" inputs.

Summary of input types:
---------------------------------------------------
Signature................ a          b           c
Rate required by input... a,b,or c   a,b, or c   c
Update method generated.. yes        yes         no
---------------------------------------------------
Note that only one type has a "c-rate" output, and that is a Const
unit generator, which looks like it has a b-rate output.  One thing
special about these is that Ugens that accept b or c-rate inputs have
"set_x" methods for setting input x. A precondition to "set_x" is that
intput x comes from a Const Ugen. The action is to write a new float
value into the Const Ugen. This allows you to treat inputs as setable
scalar values.  It also allows you to forget the Arco ID of the const
input and, instead, treat the input value as belonging to the unit
generator.

The main .dsp file is used to generate multiple .dsp files which
are each translated using faust to .fh files, e.g.:
    faust -light -cn Sine sine_aa_a.dsp -o sine_aa_a.fh
Block rate output is specified by a file matching "*b.dsp" and
this also is used to create .dsp files translated using Faust's
"one sample" switch "-os" as in:
    faust -light -cn Sine -os sine_bb_b.dsp -o sine_bb_b.fh
Then, the multiple .fh files are translated and merged to form
a .cpp and a .h file. (Typically a single .dsp file is used to
create 4 files, e.g., mult.h, mult.cpp, multb.h and multb.cpp.

See arco/doc/building.md for more details on specification.

Faust parameter interpolation is either a first-order filter or a
sample-rate algorithm that allocates a 64K-sample delay line just
to look back one block-length, but the whole point of interpolation
is to keep everything in registers in the inner loop. f2a.py will
insert fast, compact linear interpolation if you declare a b-rate
parameter as interpolated using:
    declare interpolated "amp";
or in general:
    declare interpolated "p1, p2, p3"; // commas are optional

Interpolation is tricky. Consider the mult ugen. A naive solution to
make the 2nd input interpolated would start with compute() from
mult_ab_a.fh, discover that fSlow1 is the internal representation for
the second input, x2. Then we create an interpolated version of x2 we
call x2_prev, add a line to update x2_prev in the inner loop, and
substitute x2_prev for fSlow1 in the inner loop.

But this fails because Faust believes x2 is constant during compute(),
so it can safely lift computations with x2 out of the loop. In
mult_bb_a.fh, Faust will compute the product x1 * x2 before the inner
loop. Do we interpolate x1 and x2 and move the product into the loop?
Or interpolate the product instead? Since b-rate computations in
filters often invoke trig functions and are therefore slow (and good
to move out of the inner loop), we don't generally want to interpolate
immediately. So Arco's policy will be to interpolate those b-rate
signals that are used in the inner loop and derived from inputs
declared to be interpolated (including the inputs themselves when used
directly in the inner loop).

To implement this, we need to find all b-rate inner loop signals
derived from inputs. Apparently, these are all named fSlowN, where N
is a number (0, 1, 2, ...). Every fSlowN variable is declared and
assigned once, so we can check if it is derived from an "interpolated"
input just by pattern matching within the right hand side of the
fSlowN initialization expression. (Of course, we need to search for
uses of other fSlowN variables that are derived from interpolated
inputs as well.)

Then, having identified all fSlowN variables to be interpolated, we
can add fSlowN_prev to the state, create a local fSlowN_fast to hold
the interpolated value, and fSlowN_incr to store the increment. The
loop will begin with statements of the form:
    fSlowN_prev += fSlowN_incr;
for each variable to upsample by interpolation, and then in the 
inner loop computation we just substitute fSlowN_prev for fSlowN.
"""

# TODO: 
#       make initialization constants only update at initialization

import os
import sys
import glob
import re
from pathlib import Path
from implementation import prepare_implementation
from params import get_signatures

def generate_arco_cpp(classname, impl, signature, rate, initializer_code, outf):
    """
    write the .cpp file to outf
    rate is ? (currently not used)
    """

    global fsrc
    print("/*", classname.lower(),  "-- unit generator for arco", file=outf)
    print(" *\n * generated by f2a.py\n */\n", file=outf)
    print('#include "arcougen.h"', file=outf)
    cnlc = classname.lower()
    print('#include "' + cnlc + '.h"\n', file=outf)
    print(f'const char *{classname}_name = "{classname}";\n', file=outf)
    print("generate_arco_cpp: class", classname, "signature:", signature)
    inputs = ""
    for param in signature.params:
        if param.abtype == 'c':
            inputs += ", float " + param.name
        else:
            inputs += ", int32 " + param.name
    print(f"/* O2SM INTERFACE: /arco/{cnlc}/new int32 id", file=outf, end="")
    if not signature.output.fixed:
        print(", int32 chans", file=outf, end="")
    print(f"{inputs};\n */", file=outf)
    print(f"void arco_{cnlc}_new(O2SM_HANDLER_ARGS)\n{{", file=outf)
    print("    // begin unpack message (machine-generated):", file=outf)
    # variable declarations and initialization are generated by o2idc.py
    print("    // end unpack message\n", file=outf)
    parameters = ""
    for param in signature.params:
        if param.abtype != 'c':
            print(f'    ANY_UGEN_FROM_ID({param.name}_ugen, {param.name},',
                  f'"arco_{cnlc}_new");', file=outf)
            parameters += f", {param.name}_ugen"
        else:
            parameters += f", {param.name}"
    chans = "" if signature.output.fixed else ", chans"
    print(f"\n    new {classname}(id{chans}{parameters});", file=outf)
    print("}\n\n", file=outf)
    
    for param in signature.params:
        if param.abtype == 'c':
            continue  # no repl_ or set_ methods for constant parameters
        print(f"/* O2SM INTERFACE: /arco/{cnlc}/repl_{param.name}",
              f"int32 id, int32 {param.name}_id;\n */", file=outf)
        print(f'static void arco_{cnlc}_repl_{param.name}' + \
              "(O2SM_HANDLER_ARGS)\n{", file=outf)
        print("    // begin unpack message (machine-generated):", file=outf)
        # variable declarations and initialization are generated by o2idc.py
        print("    // end unpack message\n", file=outf)
        
        print(f"    UGEN_FROM_ID({classname}, {cnlc}, id,",
              f'"arco_{cnlc}_repl_{param.name}");', file=outf)
        print(f"    ANY_UGEN_FROM_ID({param.name}, {param.name}_id,",
              f'"arco_{cnlc}_repl_{param.name}");', file=outf)
        print(f"    {cnlc}->repl_{param.name}({param.name});\n}}\n\n",
              file=outf)

        print(f"/* O2SM INTERFACE: /arco/{cnlc}/set_{param.name}",
              "int32 id, int32 chan, float val;\n */", file=outf)
        print(f"static void arco_{cnlc}_set_{param.name}",
              "(O2SM_HANDLER_ARGS)\n{", file=outf)
        print("    // begin unpack message (machine-generated):", file=outf)
        print("    // end unpack message\n", file=outf)
        print(f"    UGEN_FROM_ID({classname}, {cnlc}, id,",
              f'"arco_{cnlc}_set_{param.name}");', file=outf)
        print(f"    {cnlc}->set_{param.name}(chan, val);\n}}\n\n", file=outf)

    print(f"static void {cnlc}_init()\n{{", file=outf)
    print("    // O2SM INTERFACE INITIALIZATION: (machine generated)",
          file=outf)
    print(f'    o2sm_method_new("/arco/{cnlc}/new',
          f'"ii{"i" * len(impl.param_names)}", arco_{cnlc}_new, NULL,'
          "true, true);", file=outf)
    for param in signature.params:
        if param.abtype == 'c':
            continue
        print(f'    o2sm_method_new("/arco/{cnlc}/repl_{param.name}",',
              f'"ii", arco_{cnlc}_repl_{param.name}, NULL, true, true);',
              file=outf)
        print(f'    o2sm_method_new("/arco/{cnlc}/set_{param.name}",',
              f'"if", arco_{cnlc}set_{param.name}, NULL, true, true);',
              file=outf)
    print("    // END INTERFACE INITIALIZATION", file=outf)

    print(initializer_code, file=outf)
    print("}\n", file=outf)

    print(f"Initializer {cnlc}_init_obj({cnlc}_init);", file=outf)

    return


def find_matching_brace(fsrc, loc):
    """
    Find the first unmatched close brace after loc. (fsrc[loc] is ignored)
    This function does not consider brace characters can be quoted.
    """
    loc += 1
    loc_open = fsrc.find("{", loc)
    loc_close = fsrc.find("}", loc)
    if loc_open >= 0 and loc_open < loc_close:  # contains nested {...}
        # recursion: search for matching close brace after loc_open,
        # then search for a close brace
        loc2 = find_matching_brace(fsrc, loc_open)
        if loc2 < 0: return -1
        return find_matching_brace(fsrc, loc2)
    elif loc_close >= 0:
        return loc_close
    else:
        return -1


def extract_method(classname, methodname, fimpl, alternates = None):
    """
    Extract the method from classname
    Algorithm: 
       find classname,
       find methodname, 
       find first "{" after methodname
       find its match; 
       find end of line;
       find beginning of line before methodname;
       extract the method.        
    Return: method as string or None if error occurs
    """
    loc = fimpl.find("class " + classname)
    if loc < 0:
        print("Error: extract_method could not find class", classname)
        return None
    loc = fimpl.find(" " + methodname + "(", loc)
    # In Faust version 2.75, "control" was changed to "frame" when -os
    # (= one sample) is used (this is the case for Arco block-rate
    # output ugens, but otherwise output is still compatible with this
    # f2a.py translator, so now we just search for "frame" first but
    # if we do not find it, we look for "control". This is generalized
    # so we can look for a list of alternates for any method.
    if loc < 0 and alternates:
        for alternate in alternates:
            loc = fimpl.find(" " + alternate + "(", loc)
            if loc >= 0:
                break
    if loc < 0:
        print('Note: could not find "' + methodname + '"')
        return None
    loc2 = fimpl.find("{", loc)
    if loc2 < 0:
        print("Error: could not find body of classInit")
        return None
    loc2 = find_matching_brace(fimpl, loc2)
    if loc2 < 0: return None
    # capture to end of line (move loc2 just after newline)
    while loc2 < len(fimpl) and fimpl[loc2 - 1] != '\n':
        loc2 += 1
    # capture from beginning of line:
    while loc > 0 and fimpl[loc] != '\n':
        loc -= 1
    return fimpl[loc + 1 : loc2]


def generate_initializer_code(fimpl, classname, rate):
    """find classInit in Faust code and use it to create class_init for Arco
    returns True if there is an error, otherwise returns initializer code
    (a string).
    """

    init_code = ""
    rate_name = "AR" if rate == "a" else "BR"

    # find classInit method
    class_init = extract_method(classname, "classInit", fimpl)
    if class_init != None:
        # print("---------classInit method:\n" + class_init + "-------")
        loc = class_init.find("(")
        if loc < 0:
            print('Error: could not find "static void classInit("')
            return True
        loc2 = class_init.find(")", loc)
        if loc2 < 0:
            print("Error: could not find ')' after \"static void classInit(\"")
            return True

        # remove int sample_rate parameter:
        class_init = class_init[0 : loc + 1] + class_init[loc2 : ]

        # replace sample_rate with AR or BR
        class_init = class_init.replace("sample_rate", rate_name)

        # remove first line (method declaration) and last line (close brace):
        class_init = class_init.split("\n")
        print("class_init", class_init)
        class_init[0] = "\n    // class initialization code from faust:"
        # reduce indentation by 4 spaces (except for first line):
        for i in range(len(class_init)):
            if class_init[i].find("    ") == 0:  # only remove spaces
                class_init[i] = class_init[i][4 : ]
        init_code = "\n".join(class_init[0 : -2])
        print("initializer code", init_code)

    # repeat these steps + local var decls for staticInit method
    static_init = extract_method(classname, "staticInit", fimpl)
    if static_init != None:
        # print("---------staticInit method:\n" +static_init + "-------")
        loc = static_init.find("(")
        if loc < 0:
            print('Error: could not find "void staticInit("')
            return True
        loc2 = static_init.find(")", loc)
        if loc2 < 0:
            print("Error: could not find ')' after \"static void classInit(\"")
            return True

        # remove int sample_rate parameter:
        static_init = static_init[0 : loc + 1] + static_init[loc2 : ]

        # replace sample_rate with AR or BR
        static_init = static_init.replace("sample_rate", rate_name)

        # remove first line (method declaration) and last line (close brace):
        static_init = static_init.split("\n")
        print("static_init", static_init)
        static_init = static_init[1 : -2]
        # reduce indentation by 4 spaces (except for first line):
        for i in range(len(static_init)):
            if static_init[i].find("    ") == 0:  # only remove spaces
                static_init[i] = static_init[i][4 : ]

        # Local Variables: This seems likely to fail in the future on
        # different cases, but for now, we observe in sineb_bb_b.fh
        # that the staticInit uses member variables as temporaries.
        # These are iVec0[] and iRec0[]. We need to declare them here
        # as locals since we are not defining a method and we want to
        # run this initialization before any instances even exist.  We
        # look for every assignment to a variable that starts with
        # "iVec", "iRec", "fVec" or "fRec" and assume that the size of
        # the array is determined by the largest integer subscript,
        # e.g. there is a reference to iRec[1] but not iRec[2], so we
        # must declare "float iRec[2];."  We ignore iRec[l1_re0]
        # because the subscript is not an integer constant. Since
        # these variables all occur to the left of assignments, we can
        # restrict our search to the first token after space on each
        # line.

        local_prefixes = ["iVec", "iRec", "fVec", "fRec"]
        local_vars = {}
        for line in static_init:
            # get the first token; there must also be "=" after token
            token = line.split(maxsplit=1)          # so test len > 1:
            if len(token) < 2:
                continue  # nothing like token = expr found
            token = token[0]
            has_prefix = None
            for prefix in local_prefixes:
                if token.find(prefix) == 0:  # found a "Vec"
                    has_prefix = prefix
                    break
            if not has_prefix:
                continue  # no iVec or iRec found
            brk_open = token.find("[")
            if brk_open == -1:
                continue  # no open bracket
            brk_close = token.find("]")
            if brk_close < brk_open:
                continue  # no close bracket
            local_var = token[0 : brk_open]
            print("## local_var", local_var, "token", token,
                  "brk_open", brk_open)
            index = token[brk_open + 1 : brk_close]
            if not index.isdigit():
                continue  # not a constant index
            index = int(index)
            sz = local_vars.get(local_var, 0)
            if index >= sz:
                local_vars[local_var] = index + 1  # dim of array
        # now, we've computed local_vars and dimensions, so declare them:
        decls = ""
        for local_var in local_vars.keys():
            var_type = "float" if local_var[0] == "f" else "int"
            decls += f"    {var_type}" + \
                             f" {local_var}[{str(local_vars[local_var])}];\n"

        head = "\n    // \"static\" initialization code from faust:\n"

        init_code = head + init_code + decls + "\n".join(static_init)
        print("initializer code", init_code)

    # replace classInit:
    #class_init = class_init.replace("classInit", "class_init")
    #print("---------revised classInit method:\n" + class_init + "-------")

    # write to .h file:
    #print(class_init, file=outf)

    # make initializer to run class_init when Ugens are intialized:
    #initializer_code = "Initializer " + classname.lower() + "_init_obj(" + \
    #                   classname + "::class_init);\n"
    return init_code


def line_start(s, loc):
    """Find location of beginning of line containing loc"""
    while loc > 0 and s[loc - 1] != "\n":
        loc -= 1;
    return loc


def line_next(s, loc):
    """Find location of beginning of line after loc"""
    while (loc < len(s) - 1 and s[loc - 1] != "\n") or \
          (loc == 0 and len(s) > 0):
        loc += 1;
    return loc


def skip_to_non_space(s, loc):
    """advance loc to next non-space in s"""
    while loc < len(s) and s[loc].isspace():
        loc += 1
    return loc


def skip_to_space(s, loc):
    """advance loc to next space in s"""
    while loc < len(s) and not s[loc].isspace():
        loc += 1
    return loc

    
def skip_token(s, loc):
    """skip to non-space, skip to space, skip to non_space"""
    return skip_to_non_space(s, skip_to_space(s, skip_to_non_space(s, loc)))


def get_token(s, loc):
    """return the space-delineated token starting at loc"""
    loc = skip_to_non_space(s, loc)
    endloc = skip_to_space(s, loc)
    return s[loc : endloc]


def string_insert(s, insert, loc):
    """return s with insert inserted at loc"""
    print("string_insert loc", loc)
    return s[0 : loc] + insert + s[loc : ]


def has_interpolated_variable(expr, varlist):
    """Test if a variable in varlist is used in expr"""
    print("has_interpolated_variable: expr", expr, "varlist", varlist)
    for var in varlist:
        if expr.find(var) >= 0:
            print("    returns true")
            return True
    print("    returns false")
    return False


def insert_interpolation(body, varlist):
    """Transform body to interpolate var, return None on error"""
    slow_vars = []
    print("insert_interpolation of variables", varlist)
    # first, find all fSlowN variables derived from interpolated vars in varlist
    # search up to the first for statement which should be the inner loop
    loc = 0
    while get_token(body, loc) != "for":
        loc = skip_token(body, loc)  # skip over "float"
        var = get_token(body, loc)  # get fSlowN (maybe)
        if len(var) > 5 and var[0 : 5] == "fSlow":  # found it
            # now see if fSlowN depends on an interpolated variable
            # get the expression that defines fSlowN:
            loc = skip_token(body, skip_token(body, loc))  # skip fSlowN and "="
            endloc = line_next(body, loc)
            expr = body[loc : endloc]
            if has_interpolated_variable(expr, varlist):
                slow_vars.append(var)
                # anything derived from var is indirectly derived from an 
                # interpolated variable:
                varlist.append(var)
        else:
            endloc = line_next(body, loc)
        loc = endloc  # move on to the next line

    # now we have the slow variables to be interpolated in inner loop
    # and we know the inner loop begins at loc
    for var in slow_vars:
        incr_decls = f"        Sample {var}_incr = " + \
                f"({var} - state->{var}_prev) * BL_RECIP;\n" + \
                f"        Sample {var}_fast = state->{var}_prev;\n" + \
                f"        state->{var}_prev = {var};\n"
        insert_loc = line_next(body, loc)
        body = string_insert(body, incr_decls, insert_loc)
        loc = insert_loc + len(incr_decls);

    # now, loc is at the beginning of the line with the inner loop
    loc = skip_token(body, loc)  # skip beyond line break so next stmt works...
    loc = line_next(body, loc)  # skip to line after "for..."
    # now, loc is at the beginning of the inner loop body
    for var in slow_vars:
        incr_stmt = f"            {var}_fast += {var}_incr;\n"
        body = string_insert(body, incr_stmt, loc)
        loc += len(incr_stmt);
    # replace every slow var in the inner loop with fSlowN_fast:
    body_begin = body[0 : loc]
    body_end = body[loc : ]
    for var in slow_vars:
        body_end = replace_symbols(body_end, var, var + "_fast")
    return (body_begin + body_end, slow_vars)


def find_b_rate_parameter_faust_names(classname, impl, src):
    """find b-rate and c-rate parameter faust names from
    buildUserInterface method. For each parameter found, add an extra
    element to the corresponding parameter in params_info so the element
    will look like [parameter_name, interpolated?, faust_name]
    """
    print("find_b_rate_parameter_faust_names called", classname)
    print("SOURCE---------------\n", src, "\n----------------")
    bui = extract_method(classname, "buildUserInterface", src)
    if bui == None: return True
    bui = bui.split("\n")
    printed_something = False
    for line in bui:
        loc = line.find("addNumEntry(")
        if loc >= 0:  # ... "amp", &fEntry0, ...
            args = line[loc + len("addNumEntry(") + 1 : ].split(",")
            print("line =", line, "args =", args)
            printed_something = True
            arco_name = args[0][0 : -1]  # remove trailing quote
            faust_name = args[1].strip()[1 : ]  # remove leading &
            i = impl.param_names.index(arco_name)
            if i >= 0:
                impl.param_faust_names[i] = faust_name
    if printed_something:  # print a separator for readability
        print("------\n")


def generate_channel_method(fhfile, classname, signature,
                            instvars, impl, src, rate):
    """Generate and return one channel method. Return True on error
    The method is returned in 5 parts:
        the declaration
        the body
        the close brackets "}  }"
    so that it can be used as is, or it can be used as inline
    statements in real_run for b-rate unit generators that have only
    one channel method and don't need indirection of (this->*run_channel)(...)

    fhfile is [filename, signature (ab_b), output (classname)]
    impl is an Implementation object
    rate is the output rate of the Arco ugen
    """
    print("---------------------------generate_channel_method", classname, signature, 
          instvars, "\nrate:", rate)
    param_types = fhfile[1][ : -2]  # e.g. ab_a -> ab
    print("------compute method for " + fhfile[2])

    # full copy to avoid side affecting the original impl
    # params_info = [p.copy() for p in params_info]

    # initially, set param_faust_names as if there are no b-rate parameters
    impl.param_faust_names = [None] * len(impl.param_names)
    if 'b' in param_types or 'c' in param_types:
        # update impl.param_faust_names
        find_b_rate_parameter_faust_names(classname, impl, src)

    print("After find_b_rate_parameter_faust_names, impl.param_faust_names:", impl.param_faust_names)
    compute = extract_method(classname, "compute", src)
    print(compute + "-----")
    if compute == None: return True
    compute = compute.replace("\t", "    ")
    # print("params_info", repr(params_info))

    ## The Declaration is stored in rslt[0]:
    rslt = ["    void chan" + fhfile[1] + "(" + classname + "_state *state) {"]
 
    # remove the first line and the last 2 lines of compute
    compute = "\n".join(compute.split("\n")[1 : -2])  

    # replace outputs[0], outputs[1], etc. with out_samps, out_samps + BL, ...
    # or if this is 'b'-rate, replace outputs[ with out_samps[
    if rate == 'a':
        for i in range(signature.output.chans):
            if i == 0:  # we don't need this special case or 0 ...
                compute = compute.replace("outputs[0]", "out_samps")
            elif i == 1:  # ... or this one either, but output is prettier
                compute = compute.replace("outputs[1]", "out_samps + BL")
            else:
                compute = compute.replace(f"outputs[{str(i)}]", 
                                          f"out_samps + BL * {str(i)}")
    else:  # change outputs[0] = ... to *out_samps = ...
        # modify compute by pre-pending body of control() method after
        # fixing up references to fControl[] here and in compute
        control = extract_method(classname, "frame", src, 
                                 alternates=["control"])
        print(control + "-----")
        if control == None:
            control = "    ignore me {\n    }\n"  # will be removed
        control = control.replace("\t", "    ")

        # strip off the method declaration to get body
        loc = control.find("{") + 1
        if loc == 0:
            return True
        # probably we're at the end of a line, so delete the whole line:
        while control[loc] == " ":  # skip space before newline
            loc += 1
        if control[loc] == "\n":  # skip newline
            loc += 1
        control = control[loc : ]

        # strip off trailing } closing the method:
        loc = control.rfind("}")
        if loc == -1:
            return True
        # probably } is alone on a line: delete the whole line
        while control[loc - 1] == " ":
            loc = loc - 1
        control = control[0 : loc]

        compute = control + compute  # prepend control stmts to compute

        compute = compute.replace("outputs[", "out_samps[")

        i = 0
        while compute.find(f"fControl[{str(i)}]") >= 0:
            print(f"found fControl[{str(i)}]")
            compute = compute.replace(f"fControl[{str(i)}]", f"tmp_{str(i)}")
            i += 1
        # add declarations for tmp_0, tmp_1, ...
        for j in range(i):
            compute = compute.replace(f"    tmp_{str(j)} = ",
                                      f"    FAUSTFLOAT tmp_{str(j)} = ")
        # there are references to fEntry0, etc., but these are replaced later

    # change count to BL
    compute = compute.replace("count", "BL")

    # change each inputs[n] to <param>_samps + BL * N where <param> is an
    # Arco parameter name and N is the channel number. To make things easier,
    # we will expand Arco parameters into strings like "in_samps + in_stride"
    # to correspond to the FAUST parameters (even though we will only use the
    # expressions for a-rate parameters).
    # Build the expressions:
    expressions = []
    i = 0
  
    for param in signature.params:
        if param.abtype == 'c':
            expressions.append(param.name)
        else:
            expressions.append(f"{param.name}_samps" + \
                               ("[0]" if impl.param_faust_names[i] else ""))
        i += 1
        # note that if this is not fixed (channels) and we allow any number
        # of channels, each with it's own state, then param.chans will be 1
        # and the expression will  simply be <param>_samps, which is updated
        # in the real_run per-channel loop, so it's always correct.
        chans = param.chans 
        if chans > 1:
            if impl.param_faust_names[i]:
                expr = f"{param.name}_samps[{param.name}_stride]"
            else:
                expr = f"{param.name}_samps + {param.name}_stride"
            expressions.append(expr)
            i += 1
        for i in range(2, chans):
            if impl.param_faust_names[i]:
                expr = f"{param.name}_samps[{param.name}_stride * {str(i)}]"
            else:
                expr = f"{param.name}_samps + {param.name}_stride * {str(i)}"
            expressions.append(expr)
            i += 1

    # careful: n is the number of the a-rate input, so if inputs are ba,
    #   n will be 0 for the 2nd parameter
    param_names = [p.name for p in signature.params]
    i = 0 # index into FAUST implementation (.fh) variable names
    print("Before replace loop", impl.param_names, "impl.param_faust_names", impl.param_faust_names)
    for j, name in enumerate(impl.param_names):
        if impl.param_faust_names[j]:
            faust_name = impl.param_faust_names[j]  # b-rate fEntry0, etc.
            arco_name = expressions[j]
        else:
            faust_name = f"inputs[{str(i)}]"  # a-rate inputs[0], ...
            arco_name = expressions[j]
            i += 1  # increment only for each a-rate parameter
        print("####replace", impl.param_faust_names[j], faust_name, arco_name)  # substitute address of block
        compute = compute.replace(faust_name, arco_name)
        # also replace fControl[i] with arco_name
        if faust_name.find("fEntry") == 0:
            faust_control_var = f"fControl[{faust_name[6 : ]}]"
            print("replace", faust_control_var, arco_name)
            compute = compute.replace(faust_control_var, arco_name)

    # change each instvar to state->instvar
    for iv in instvars:
        if not iv.isconst:
            compute = replace_symbols(compute, iv.name, "state->" + iv.name)

    # add code for interpolated parameters
    print("add code for interpolation") # , params_info is", params_info)
    varlist = []
    for i, name in enumerate(impl.param_names):
        if impl.param_interp[i] and fhfile[1][1 + i] != 'a':  # interpolated flag and b-rate
            varlist.append(name)

    if len(varlist) > 0:
        (compute, slow_vars) = insert_interpolation(compute, varlist)
        if compute == None: return True
    else:
        slow_vars = []

    print(f"** final compute method for {fhfile[2]}\n{compute}\n-----")
    rslt.append(compute)
    rslt.append("    }\n")
    print(f"generate_channel_method {classname} returns", rslt, slow_vars)
    return (rslt, slow_vars)


def generate_channel_methods(fhfiles, classname, signature,
                             instvars, impl, rate):
    """
    create channel methods, which are specific inner loops that compute one
    channel of output assuming a specific combination of a-rate and b-rate
    inputs. We have to generate a channel method for each combination.

    fhfiles is list of fhfile of the form
        [filename, signature (ab_b), output (classname)]
    impl is an Implementation object
    
    Return True on error or string containing all the methods on success
    """
    print("******* generate_channel_methods fhfiles", fhfiles)
    methods_src = []
    all_slow_vars = []
    for fhfile in fhfiles:
        with open(fhfile[2], "r") as inf:
            print("        ******* generate_channel_methods fhfile", fhfile)
            cm, slow_vars = generate_channel_method(fhfile, classname, 
                                signature, instvars, impl, inf.read(), rate)
            if cm == True:
                return True
            # accumulate the union of all slow_vars (interpolated variables)
            # that we will declare and initialize in channel state structs
            for slow_var in slow_vars:
                if slow_var not in all_slow_vars:
                    all_slow_vars.append(slow_var)
            methods_src += cm  # append arrays of strings
    return ("\n".join(methods_src) + "\n", all_slow_vars)
 

class Vardecl:
    def __init__(self, type, isconst):
        self.type = type
        self.isconst = isconst
        self.name = None
        self.arrayspec = ""

    def __str__(self):
        return f"<Vardecl: {self.type} {self.name} {self.arrayspec} " + \
               f"{str(self.isconst)}>"

    def __repr__(self):
        return str(self)


def find_class_declaration(classname, fimpl):
    """Return the location of the class declaration"""
    loc = fimpl.find(f"class {classname} ")
    if loc < 0:
        loc = fimpl.find("class {classname}:")
        if loc < 0:
            print("Error: find_private_variables could not find class",
                  classname)
            return None
    return loc


def find_private_variables(fimpl, classname):
    """
    Find the private variables in the declaration of classname.
    remove fsampleRate
    Return: list of Vardecl with type, isconst, name, arrayspec, e.g. 
              [<Vardecl "float" False "fRec1" "2">, 
               <Vardecl "float" True "fConst1" "">, ...]
            or return None on error.
    """
    loc = find_class_declaration(classname, fimpl)
    if loc == None: return None
    loc = fimpl.find("private:", loc)
    if loc < 0:
        print('Error: find_private_variables could not find "private:"')
        return None
    loc2 = fimpl.find("public:", loc)
    if loc < 0:
        print('Error: find_private_variables could not find "public:"',
              'after "private:"')
        return None
    privates = fimpl[loc + len("private:") : loc2].strip().split("\n")
    rslt = []
    for p in privates:
        if p.find("fSampleRate") < 0:
            decl = p.strip().split()  # [type, var;]
            # initialize with type and isconst
            instvar = Vardecl(decl[0], 
                              p.find("fConst") >= 0 or p.find("iConst") >= 0)
            bracket = decl[1].find("[")
            if bracket < 0:
                instvar.name = decl[1][0 : -1]  # remove semicolon
            else:
                instvar.name = decl[1][0 : bracket]  # remove semicolon
                instvar.arrayspec = decl[1][bracket + 1 : -2]
            rslt.append(instvar)
    print("private variables:", repr(rslt))
    return rslt


def generate_state_struct(classname):
    """
    Generate the state structure, which contains Ugen state for one channel.
    The state structure is replicated for each channel in the constructor.

    returns a tuple: (list of instance variables, code as a string) 
    """

    global fimpl
    state_struct = "    struct " + classname + "_state {\n"
    # find private variables which include Faust instance variables
    privates = find_private_variables(fimpl, classname)

    # declare instance variables from Faust here:
    for p in privates:
        if not p.isconst: # and not p.name in b_rate_variables:
            arrayspec = f"[{p.arrayspec}];" if p.arrayspec != "" \
                        else ";"
            state_struct += f"        {p.type} {p.name}{arrayspec}\n"

    # also declare a _prev variable for every interpolated parameter:
    state_struct2 = f"    }};\n    Vec<{classname}_state> states;\n"
    state_struct2 += f"    void ({classname}::*run_channel)(" + \
                                     f"{classname}_state *state);\n\n"
    return (privates, state_struct, state_struct2)


def replace_symbols(src, sym, replacement):
    """Replace all symbols in src that match sym with replacement.
    Similar to src.replace(sym, replacement), but for example,
    "fRec1" will not match or replace "fRec10".
    """
    # Escape the symbol to handle any special characters
    escaped_sym = re.escape(sym)
    # Use word boundaries to match the exact symbol
    pattern = r'\b' + escaped_sym + r'\b'
    return re.sub(pattern, replacement, src)


def generate_state_init(classname, instvars, slow_vars):
    """
    output loop body that initializes states[i], returns True if error

    impl is an Implementation object
    """
    global fimpl
    clear_meth = extract_method(classname, "instanceClear", fimpl)
    if clear_meth == None: return True
    # indent for state loop
    clear_meth = ["    " + line for line in clear_meth.split("\n")]
    # remove the first line and last line:
    clear_meth = "\n".join(clear_meth[1 : -2])
    # note: clear_meth is missing newline at end
    # replace variable v with states[i].v
    for var in instvars:
        if not var.isconst:
            clear_meth = replace_symbols(clear_meth, var.name, 
                                         f"states[i].{var.name}")
    print("** body of clear meth\n" + clear_meth + "\n------")
    
    state_init = "    void initialize_channel_states() {\n"
    state_init += "        for (int i = 0; i < chans; i++) {\n"
    state_init += clear_meth + "\n"
    # initialize _prev variables whether we use them or not:
    for p in slow_vars:
        state_init += f"            states[i].{p}_prev = 0.0f;\n"
    state_init += "        }\n    }\n\n"
    print(f"** state_init\n{state_init}\n------")
    return state_init


def initialize_constants(classname, instvars, rate):
    """
    compute initializers for constants, returns True if error
    """
    global fimpl
    const_meth = extract_method(classname, "instanceConstants", fimpl)
    if const_meth == None: 
        return True
    # remove first line and last line to get the body
    const_meth = const_meth.split("\n")[1 : -2]
    # remove the line that sets fSampleRate
    foundit = None
    for i, line in enumerate(const_meth):
        if line.find("fSampleRate = ") >= 0:
            print("found fSampleRate in", line)
            foundit = i
            break
    if foundit != None:
        del const_meth[foundit]
    const_meth = "\n".join(const_meth)
    rate_name = "AR" if rate == "a" else "BR"
    const_meth = const_meth.replace("fSampleRate", rate_name)
    return const_meth + "\n"


def generate_update_run_channel(classname, fhfiles, names, signature):
    """The update method inspects all inputs and decides what specific inner

    loop should be called. Inner loops are in channel methods and real_run()
    will make an indirect method call to invoke this method. The update
    method will be called whenever there is a change to an input in case
    the channel method should change.

    fhfiles is a list of fhfile of the form
        [filename, signature (ab_b), output (classname)]
    names is a list of implementation (process) parameter names
    signature is a Signature objects, the Arco type signature for this class

    returns a string defining update_run_channel()

    The "new" algorithm generates a decision tree based on parameter
    rates from left to right (first decision is based on the first
    parameter).

    We first collect all implementations where the parameter's rate
    is a and all with b. If one of these is empty, then we have no
    choice but to upsample or downsample the parameter, so the code
    is either:
        if (param->rate == 'a') {
            param = new Dnsampleb(-1, param->chans, param, LOWPASS500);
        }
    or
        if (param->rate == 'b') {
            param = new Upsample(-1, param->chans, param);
        }
        ... continue with remaining parameters ...
    
    Alternatively, if there are implementations for both a-rate and
    b-rate, we write:
        if (param->rate == 'a') {
            ... continue with remaining parameters ...
        } else {
            ... continue with remaining parameters ...
        }

    This is naturally recursive so we have a helper function.
    """

    def decision_tree(index, indent_level, fhfiles, names, signature,
                      classname, urc):
        """Recursive helper function to select implementation
        names is a list of parameter names from process in the FAUST 
        implementation. As decision_tree recurses, the names supplied
        by the index'th parameter in signature are removed so the
        first name in names corresponds to the index parameter in signature
        """
        # ignore 'c' parameters
        while len(names) > 0 and signature.params[index].abtype == 'c':
            names = names[1 : 0]
            index += 1
        if len(names) == 0:
            assert(len(fhfiles) == 1)  # we should have a unique choice
            indent = "    " * indent_level
            urc += f"{indent}new_run_channel = " + \
                   f"&{classname}::chan{fhfiles[0][1]};\n"
            return urc

        # split the fhfiles based on the rate at index
        audio_rate_files = []
        block_rate_files = []
        for fhfile in fhfiles:
            if fhfile[1][index + 1] == 'a':  # [1] is the signature
                audio_rate_files.append(fhfile)
            elif fhfile[1][index + 1] == 'b' or fhfile[1][index + 1] == 'c':
                block_rate_files.append(fhfile)
            else:
                raise Exception("bad fhfile signature? " + fhfile[1])
        pname = signature.params[index].name
        # remove the parameter names supplied by the index'th parameter
        # in signature
        names = names[signature.params[index].chans : ]
        index += 1
        indent = "    " * indent_level
        if len(audio_rate_files) == 0 or len(block_rate_files) == 0:
            if len(audio_rate_files) == 0:  # must downsample
                urc += f"{indent}if ({pname}->rate == 'a') " + "{\n" + \
                       f"{indent}    {pname} = " + \
                               f"new Dnsampleb(-1, {pname}->chans, " + \
                               f"{pname}, LOWPASS500);\n" + \
                       indent + "}\n"
                fhfiles = block_rate_files
            else:  # must upsample
                urc += f"{indent}if ({pname}->rate == 'b') " + "{\n" + \
                       f"{indent}    {pname} = " + \
                               f"new Upsample(-1, {pname}->chans, " + \
                               f"{pname });\n" + \
                       indent + "}\n"
                fhfiles = audio_rate_files
            if len(names) == 0:  # no more parameters, make a decision
                assert len(fhfiles) == 1  # we should have a unique choice left
                urc += f"{indent}new_run_channel = " + \
                               f"&{classname}::chan{fhfiles[0][1]};\n"
            else:
                urc = decision_tree(index, indent_level, fhfiles, names,
                                    signature, classname, urc)
        else:  # handle left (a) branch and right (b) branch separately
            # first, the left (a) branch:
            if len(audio_rate_files) == 1:
                urc += f"{indent}if ({pname}->rate == 'a') {{\n"
                urc += f"{indent}    new_run_channel = "
                urc +=     f"&{classname}::chan{audio_rate_files[0][1]};\n"
            else:
                urc += f"{indent}if ({pname}->rate == 'a') {{\n"
                urc = decision_tree(index, indent_level + 1,
                            audio_rate_files, names, signature, classname, urc)
            # now, handle the right (b) branch:
            urc += f"{indent}}} else {{\n"
            if len(block_rate_files) == 1:
                urc += f"{indent}    new_run_channel = "\
                       f"&{classname}::chan{block_rate_files[0][1]};\n"
            else:
                urc = decision_tree(index, indent_level + 1,
                        block_rate_files, names, signature, classname, urc)
            urc += f"{indent}}}\n"

        return urc


    urc = "    void update_run_channel() {\n"  # urc is "update run channel"
    urc += "        // initialize run_channel based on input types\n"
    urc += f"        void ({classname}::*new_run_channel)(" + \
                               f"{classname}_state *state);\n"
    print("fhfiles:", fhfiles)
    need_else = False
    cond = ""

    if len(names) == 0:
        urc += "        new_run_channel = &chan_;"
    else:
        urc = decision_tree(0, 2, fhfiles, names,
                            signature, classname, urc)
    if len(fhfiles) > 1:
        urc += "        if (new_run_channel != run_channel) {\n"
        urc += "            initialize_channel_states();\n"
        urc += "            run_channel = new_run_channel;\n        }\n"
    else:
        urc += "        run_channel = new_run_channel;\n"
        
    urc += "    }\n\n"
    return urc


def generate_arco_h(classname, impl, signature, rate, fhfiles, outf):
    """
    Write the .h file which is most of the ugen dsp code to outf.

    fhfiles is a list of fhfile of the form:
        [filename, signature (ab_b), output (classname)]
    impl is an Implementation object
    """

    global fsrc, fimpl  # faust .dsp file and faust-generated .fh file
    print("** generate_arco_h for", classname)

    print("/*", classname.lower(),  "-- unit generator for arco", file=outf)
    print(" *\n * generated by f2a.py\n */\n", file=outf)
    print("/*------------- BEGIN FAUST PREAMBLE -------------*/\n", file=outf)
    loc = find_class_declaration(classname, fimpl)
    if loc == None: return
    while fimpl[loc - 1] == "\n":
        loc -= 1
    print(fimpl[0 : loc], file=outf)
    print("/*-------------- END FAUST PREAMBLE --------------*/\n", file=outf)

    print(f"extern const char *{classname}_name;\n", file=outf)

    ## Declare the class
    print(f"class {classname} : public Ugen {{\npublic:", file=outf)

    print("impl.param_names", impl.param_names)
    # from here, we do not write directly to outf because we need to alter
    # state_struct after generating the channel methods, so we accumulate
    # all the needed code in strings to write later...

    instvars, state_struct, state_struct2 = generate_state_struct(classname)
    if instvars == True: return

    var_decls = ""
    sig_params = [p.name for p in signature.params]
    ab_params = [p.name for p in signature.params if p.abtype != 'c']
    for i, p in enumerate(sig_params):
        if signature.params[i].abtype == 'c':
            var_decls += f"    float {p};\n"
        else:
            var_decls += f"    Ugen_ptr {p};\n"
        print("**** signature.params[i]", signature.params[i], 
              signature.params[i].fixed)
        # if only 1 channel or not fixed, we need stride
# TODO: Try to understand this...
#        if (signature.params[i].chans > 1 or \
#            not signature.params[i].fixed) and \
#           signature.params[i].abtype != 'c':
        if signature.params[i].abtype != 'c':
            var_decls += f"    int {p}_stride;\n"
            var_decls += f"    Sample_ptr {p}_samps;\n\n"

    for iv in instvars:
        if iv.isconst:
            arrayspec = f"[{iv.arrayspec}]" if iv.arrayspec != "" else ""
            var_decls += f"    {iv.type} {iv.name}{arrayspec};\n"

    ## Generate the constructor
    chan_param = "" if signature.output.fixed else ", int nchans"
    constructor = f"\n    {classname}(int id{chan_param}"
    for i, p in enumerate(sig_params):
        if signature.params[i].abtype == 'c':
            constructor += f", float {p}_"
        else:
            constructor += f", Ugen_ptr {p}_"
    chan_param = str(signature.output.chans) if signature.output.fixed \
                 else "nchans"
    constructor += f") :\n            Ugen(id, '{rate}', {chan_param}) {{\n"

    for p in sig_params:
        constructor += f"        {p} = {p}_;\n"

    if len(sig_params) > 0:
        constructor += "        flags = CAN_TERMINATE;\n"

    # initialize states
    constructor += "        states.set_size(chans);\n"
    constants = initialize_constants(classname, instvars, rate)
    if constants == True:  # error occurred
        return
    constructor += constants

    # call init_<var> for every non-constant parameter
    for p in ab_params:
        constructor += f"        init_{p}({p});\n"
    if rate == 'a':
        constructor += f"        run_channel = (void ({classname}::*)("
        constructor +=                          f"{classname}_state *)) 0;\n"
        constructor += "        update_run_channel();\n"
    constructor += "    }\n\n"

    ## Generate the destructor
    constructor += f"    ~{classname}() {{\n"
    for p in ab_params:
        constructor += f"        {p}->unref();\n"
    constructor += "    }\n\n"

    ## Generate name() method
    constructor += f"    const char *classname() {{"
    constructor += f" return {classname}_name; }}\n\n"

    ## Generate update_run_channel method to set the run_channel
    update_run_channel = ""
    if rate == 'a':
        update_run_channel = generate_update_run_channel(classname, fhfiles,
                                                impl.param_names, signature)

    pr_sources = ""
    if len(sig_params) > 0:
        pr_sources += "    void print_sources(int indent, bool print_flag) {\n"
        for p in ab_params:
            pr_sources += f'        {p}->print_tree(indent, print_flag,' + \
                                                               f' "{p}");\n'
        pr_sources += "    }\n\n"

    methods = ""
    ## Generate repl_* methods to set inputs
    for p in ab_params:
        methods += f"    void repl_{p}(Ugen_ptr ugen) {{\n"
        methods += f"        {p}->unref();\n"
        methods += f"        init_{p}(ugen);\n"
        if rate == 'a':
            methods += "        update_run_channel();\n"
        methods += "    }\n\n"

    ## Generate set_* methods to set constant rate input parameters
    for p in ab_params:
        methods += f"    void set_{p}(int chan, float f) {{\n"
        methods += f'        {p}->const_set(chan, f, "{classname}::set_{p}");'
        methods += "\n    }\n\n"

    ## Generate init_* methods shared by constructor and repl_* methods
    for i, p in enumerate(sig_params):
        if signature.params[i].abtype == 'c':
            continue  # no init_ for constant parameters
        # with "fixed" channel counts, we only use stride when the input
        # has multiple channels. NULL means there is no stride variable
        # because there is no loop over a variable number of channels
        # (channel counts are fixed) and since there is only one channel,
        # we do not have to step by stride to access successive channels.
        stride_expr = "NULL"
        if signature.params[i].chans > 1 or not signature.params[i].fixed:
            stride_expr = "&" + p + "_stride"
        methods += f"    void init_{p}(Ugen_ptr ugen) {{ " + \
                   f"init_param(ugen, {p}, {stride_expr}); }}\n\n"
    
    ## Generate channel methods -- process one channel of input/output
    if rate == 'a':
        (channel_meths, slow_vars) = generate_channel_methods(fhfiles,
                           classname, signature, instvars, impl, rate)
        if channel_meths == True:
            return
    else:
        channel_meths = ""
        slow_vars = []

    # Finish state_struct by declaring slow_vars
    for slow_var in slow_vars:
        state_struct += f"        Sample {slow_var}_prev;\n"

    # Generate state initialization code (this will be inserted near
    # the top of the file within the constructor, but created here
    # after we've determined the state needed for interpolated
    # signals.
    constr_init = generate_state_init(classname, instvars, slow_vars)
    if constr_init == True: 
        return

    initializer_code = generate_initializer_code(fimpl, classname, rate)
    if initializer_code == True:
        return

    ## Generate real_run method -- compute all output channels from inputs
    real_run = "    void real_run() {\n"
    for p in ab_params:
        real_run += f"        {p}_samps = {p}->run(current_block);"
        real_run += "  // update input\n"

    # add tests for termination
    if len(terminate_info) > 0:
        real_run += "        if ((("
        join_with_or = ""
        for p in terminate_info:
            real_run += join_with_or + p + "->flags"
            join_with_or = " | "
        real_run += ") & TERMINATED) &&\n" + \
                    "            (flags & CAN_TERMINATE)) {\n" + \
                    "            terminate(ACTION_TERM);\n        }\n"

    # do the signal computation
    real_run += f"        {classname}_state *state = &states[0];\n"
    if signature.output.fixed:   # just call run_channel once
        indent = ""
    else:  # create a loop to call run_channel for each channel
        real_run += "        for (int i = 0; i < chans; i++) {\n"
        indent = "    "
    if rate == 'a':
        real_run += indent + "        (this->*run_channel)(state);\n"
    else:  # there is only one b-rate computation, so put the channel
           #     method inline:
        cm, slow_vars = generate_channel_method(fhfiles[0], classname, 
                               signature, instvars, impl, fimpl, rate)
        if cm == True:
            return
        # extract and indent the body of the compute function
        cm = ["    " + line for line in cm[1].split("\n")]
        real_run += "\n".join(cm)
    if not signature.output.fixed:  # update input pointers and close the loop
        if rate == 'a':
            real_run += "            state++;\n"
            real_run += "            out_samps += BL;\n"
        else:
            real_run += "\n            state++;\n"
            real_run += "            out_samps++;\n"
        for p in impl.param_names:
            real_run += f"            {p}_samps += {p}_stride;\n"
        real_run += "        }\n"
    real_run += "    }\n"
    real_run += "};\n"
    real_run += "#endif\n"
    all_h = [state_struct, state_struct2, var_decls, constructor, constr_init,
             update_run_channel, pr_sources, methods, channel_meths, real_run]
    for part in all_h:
        print(part, file=outf, end="")
    return initializer_code


def get_params_info(classname, ugen_src, rate):
    global fsrc, fimpl  # ugen file (.ugen) and implementation (.fh)
    """
    read FAUST implementation from file
    file: the .ugen file
    rate: ugen output rate
    returns: Implementation object
    """
    if rate == None:
        print("Error: no rate determined for", classname)
        return None

    impl = prepare_implementation(ugen_src)

    if rate == 'b':
        # this is a bit of a hack, but block rate does no interpolation, so
        # turn off all interpolation:
        impl.param_interp = [False] * len(impl.param_names)

    elif rate != 'a':
        print(f"Error: rate for {classname} is invalid ({rate})")
        return None

    return impl



def run_faust(classname, file, outfile):
    """run the faust command to generate outfile"""
    blockrate = ""
    if file.find("b.dsp") >= 0:
        blockrate = "-os "  # one sample if b-rate
    print("In f2a.py, PATH=" + os.environ['PATH'])
    print(f"Running command: faust -light -cn {classname}",
          f"{blockrate}{file} -o {outfile}")
    err = os.system(f"faust -light -cn {classname} " +
                    f"{blockrate}{file} -o {outfile}")
    if err != 0:
        print("Error: quit because of error reported by Faust")
        return True
    else:
        print("     Translation completed, generated ", outfile)
        return False



def get_signature_for(classname, src):
    """Get the signature for classname from the .ugen file"""
    signatures = get_signatures(src)
    if signatures == None:
        return None
    for sig in signatures:
        if sig.name == classname.lower():
            return sig
    print(f"Error: could not find signature for {classname}")
    return None



def main():
    global fsrc, fimpl, terminate_info
    # find files
    source = sys.argv[1]  # can pass "sine_aa_a.dsp" or just "sine"
    classname = source.split("_")[0] 
    files = glob.glob(classname + "_*.dsp")
    classnamelc = classname
    classname = classname.title()
    # fhfiles is a list: [[dspfile, signature, faustoutput],
    #                    [dspfile, signature, faustoutput], ...]
    #    where signature is like "ab_a" and faustoutput is a classname
    fhfiles = []
    main_file = None  # where to look for parameter definitions

    # translate files
    if len(files) == 0:
        print("**** no .dsp files found for classname", classname)
        exit(-1)
    output_rate = None
    # this is used to check for consistent .dsp files
    top_signature = None  # has a's or b's combined with c's
    a_max = 0
    a_max_file = None
    for file in files:
        print("**** Translating", file)
        # see if this is the audio-only version:
        path = Path(file)
        signature = str(path.with_suffix(""))[len(classname) : ]
        if output_rate and output_rate != signature[-1]:
            print(f"Error: output rate is {output_rate} but input file is",
                  file)
            return
        output_rate = signature[-1]

        # top_signature has a, b, or c for each parameter. It has a
        # if *any* variant has an a for that parameter; it has b if
        # *no* variant has an a for that parameter; it has c if *all*
        # variants have c for that parameter. If *any* variant has a
        # c for a parameter, then *every* variant and the top_signature
        # must have c or an Error is printed. 
        if not top_signature:
            top_signature = signature
        for i in range(len(signature)):
            if top_signature[i] == 'b' and signature[i] == 'a':
                top_signature = top_signature[ : i] + 'a' + \
                                top_signature[i + 1 : ]
            if top_signature[i] != 'c' and signature[i] == 'c':
                print("Error: top signature", top_signature,
                      "is not consistent with", file)
                return
        if "b" not in signature or "a" not in signature:
            main_file = file
        else:  # note sure why we wanted main_file to be all-a-and-c or
               # all-b-and-c, and that's not always the case, so let's
               # maximize the number of a's and see what happens
            if signature.count('a') > a_max:
                a_max = signature.count('a')
                a_max_file = file
        if output_rate == 'b' and "a" in signature:
            print("Error: output is b-rate, but input has audio:", file)
            return
        outfile = str(path.with_suffix(".fh"))
        fhfiles.append([file, signature, outfile])
        if run_faust(classname, file, outfile): return

    # not sure if this fallback will work, but here goes...
    # can't have all-a-and-c so use max-a
    if not main_file and a_max > 0:
        main_file = a_max_file

    print("** after translation, main file: ", main_file,
          "top signature", top_signature)
    
    # get .ugen source lines
    filename = classname + '.ugen'
    src = None
    try:
        with open(filename, 'r') as file:
            src = file.readlines()
    except FileNotFoundError:
        if output_rate == 'b' and classname[-1] == 'b':
            filename = classname[:-1] + '.ugen'
            try:
                with open(filename, 'r') as file:
                    src = file.readlines()
            except FileNotFoundError:
                pass
        if not src:
            print(f"File {filename} not found")
            return None
 
    # find signature corresponding to Arco signature, and params_info
    # corresponding to FAUST "process" implementation.
    signature = get_signature_for(classnamelc, src)
    print("get_signature_for:", signature)
    impl = get_params_info(classnamelc, src, output_rate)
    if impl == None: return
    print("impl:", impl)

    # read the main_file into fsrc
    with open(main_file) as f:
        fsrc = f.read()
    fsrc = fsrc.replace("\t", "    ")

    # find terminate_info
    loc = fsrc.find("declare terminate")
    print("terminate declare at", loc)
    terminate_info = []
    if loc >= 0:
        loc = fsrc.find('"', loc)  # first quote
        if loc < 0:
            print("Error: could not find string after declare terminate")
            return
        loc2 = fsrc.find('"', loc + 1)
        print("terminate loc", loc, "loc2", loc2)
        terminate_info = fsrc[loc + 1 : loc2].replace(",", " ").split()
    print("terminate_info", terminate_info)

    # read main faust output file for use by translators:
    path = Path(main_file)
    main_impl = str(path.with_suffix(".fh"))
    print("main faust implemenation file:", main_impl)
    with open(main_impl, "r") as inp:
        fimpl = inp.read()
    fimpl = fimpl.replace("\t", "    ")

    # open and generate the Arco Ugen
    with open(classnamelc + ".h", "w") as outf:
        init_code = generate_arco_h(classname, impl, signature, output_rate,
                                      fhfiles, outf)
    with open(classnamelc + ".cpp", "w") as outf:
        generate_arco_cpp(classname, impl, signature, output_rate, init_code, outf)
    
main()
