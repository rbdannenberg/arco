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

Faust keeps track of parameters but does not put them in a specific
order. The main (at least) .dsp file should have a declaration like:
    declare description "Sine(amp, freq) - Sine Ugen for Arco"
where parameters are named in order, and anything after the parameter
list is ignored to allow for a description.

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
from pathlib import Path

def generate_arco_cpp(classname, params_info, rate, initializer_code, outf):
    """
    write the .cpp file to outf
    params_info has the form [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
        interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
        and where the 3rd element may be omitted for a-rate parameters
    rate is ? (currently not used)
    """

    global fsrc
    print("/*", classname.lower(),  "-- unit generator for arco", file=outf)
    print(" *\n * generated by f2a.py\n */\n", file=outf)
    print('#include "arcougen.h"', file=outf)
    cnlc = classname.lower()
    print('#include "' + cnlc + '.h"\n', file=outf)
    print("const char *" + classname + '_name = "' + classname + '";\n',
          file=outf)

    inputs = ""
    for p in params_info:
        inputs += ", int32 " + p[0]
    print("/* O2SM INTERFACE: /arco/" + cnlc + "/new",
          "int32 id, int32 chans" + inputs + ";\n */", file=outf)
    print("void arco_" + cnlc + "_new(O2SM_HANDLER_ARGS)\n{", file=outf)
    print("    // begin unpack message (machine-generated):", file=outf)
    print("    // end unpack message\n", file=outf)
    parameters = ""
    for p in params_info:
        print("    ANY_UGEN_FROM_ID(" + p[0] + "_ugen, " + p[0] + ",",
              '"arco_' + cnlc + '_new");', file=outf)
        parameters += ", " + p[0] + "_ugen"
    print("\n    new", classname + "(id, chans" + parameters + ");", file=outf)
    print("}\n\n", file=outf)
    
    for p in params_info:
        print("/* O2SM INTERFACE: /arco/" + cnlc + "/repl_" + p[0],
              "int32 id, int32", p[0] + "_id;\n */", file=outf)
        print('static void arco_' + cnlc + '_repl_' + p[0] +
              '(O2SM_HANDLER_ARGS)\n{', file=outf)
        print("    // begin unpack message (machine-generated):", file=outf)
        print("    // end unpack message\n", file=outf)
        
        print("    UGEN_FROM_ID(" + classname + ",", cnlc + ", id,",
              '"arco_' + cnlc + '_repl_' + p[0] + '");', file=outf)
        print("    ANY_UGEN_FROM_ID(" + p[0] + ",", p[0] + "_id,",
              '"arco_' + cnlc + '_repl_' + p[0] + '");', file=outf)
        print("    " + cnlc + "->repl_" + p[0] + "(" + p[0] + ");\n}\n\n",
              file=outf)

        print("/* O2SM INTERFACE: /arco/" + cnlc + "/set_" + p[0],
              "int32 id, int32 chan, float val;\n */", file=outf)
        print('static void arco_' + cnlc + '_set_' + p[0] +
              '(O2SM_HANDLER_ARGS)\n{', file=outf)
        print("    // begin unpack message (machine-generated):", file=outf)
        print("    // end unpack message\n", file=outf)
        print("    UGEN_FROM_ID(" + classname + ",", cnlc + ", id,",
              '"arco_' + cnlc + '_set_' + p[0] + '");', file=outf)
        print("    " + cnlc + "->set_" + p[0] + "(chan, val);\n}\n\n",
              file=outf)

    print("static void", cnlc + "_init()\n{", file=outf)
    print("    // O2SM INTERFACE INITIALIZATION: (machine generated)",
          file=outf)
    print('    o2sm_method_new("/arco/' + cnlc + \
           '/new", "ii' + "i" * len(params_info) + '", arco_' + cnlc + \
           "_new, NULL, true, true);", file=outf)
    for p in params_info:
        print('    o2sm_method_new("/arco/' + cnlc + '/repl_' + p[0] + \
              '", "ii, arco_' + cnlc + '_repl_' + p[0] + 
              ', NULL, true, true);', file=outf)
        print('    o2sm_method_new("/arco/' + cnlc + '/set_' + p[0] + \
              '", "if, arco_set_' + p[0] + ', NULL, true, true);', file=outf)
    print("    // END INTERFACE INITIALIZATION", file=outf)

    print(initializer_code, file=outf)
    print("}\n", file=outf)

    print("Initializer", cnlc + "_init_obj(" + cnlc + "_init);", file=outf)

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


def extract_method(classname, methodname, src):
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
    loc = src.find("class " + classname)
    if loc < 0:
        print("Error: extract_method could not find class", classname)
        return None
    loc = src.find(" " + methodname + "(", loc)
    if loc < 0:
        print('Note: could not find "' + methodname + '"')
        return None
    loc2 = src.find("{", loc)
    if loc2 < 0:
        print("Error: could not find body of classInit")
        return None
    loc2 = find_matching_brace(src, loc2)
    if loc2 < 0: return None
    # capture to end of line (move loc2 just after newline)
    while loc2 < len(src) and src[loc2 - 1] != '\n':
        loc2 += 1
    # capture from beginning of line:
    while loc > 0 and src[loc] != '\n':
        loc -= 1
    return src[loc + 1 : loc2]


def generate_initializer_code(impl, classname, rate):
    """find classInit in Faust code and use it to create class_init for Arco
    returns True if there is an error, otherwise returns initializer code
    (a string).
    """

    init_code = ""
    rate_name = "AR" if rate == "a" else "BR"

    # find classInit method
    class_init = extract_method(classname, "classInit", impl)
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
    static_init = extract_method(classname, "staticInit", impl)
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
            decls += "    " + var_type + " " + local_var + "[" + \
                     str(local_vars[local_var]) + "];\n"

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
        incr_decls = "        Sample " + var + "_incr = (" + \
            var + " - state->" + var + "_prev) * BL_RECIP;\n" + \
            "        Sample " + var + "_fast = state->" + var + "_prev;\n" + \
            "        state->" + var + "_prev = " + var + ";\n"
        insert_loc = line_next(body, loc)
        body = string_insert(body, incr_decls, insert_loc)
        loc = insert_loc + len(incr_decls);

    # now, loc is at the beginning of the line with the inner loop
    loc = skip_token(body, loc)  # skip beyond line break so next stmt works...
    loc = line_next(body, loc)  # skip to line after "for..."
    # now, loc is at the beginning of the inner loop body
    for var in slow_vars:
        incr_stmt = "            " + var + "_fast += " + var + "_incr;\n"
        body = string_insert(body, incr_stmt, loc)
        loc += len(incr_stmt);
    # replace every slow var in the inner loop with fSlowN_fast:
    body_begin = body[0 : loc]
    body_end = body[loc : ]
    for var in slow_vars:
        body_end = body_end.replace(var, var + "_fast")
    return (body_begin + body_end, slow_vars)


def find_b_rate_parameter_faust_names(classname, params_info, src):
    """ find b-rate parameter faust names from buildUserInterface
    method. For each parameter found, add an extra element to
    the corresponding parameter in params_info to the element
    will look like [parameter_name, interpolated?, faust_name]
    """
    print("find_b_rate_parameter_faust_names called", classname)
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
            for p in params_info:  # make parameter[][2] be faust name
                if p[0] == arco_name:
                    p.append(faust_name)
                    break;
    if printed_something:  # print a separator for readability
        print("------\n")


def generate_channel_method(fhfile, classname, instvars, params_info,
                            src, rate):
    """Generate and return one channel method. Return True on error
    The method is returned in 5 parts:
        the declaration
        the body
        the close brackets "}  }"
    so that it can be used as is, or it can be used as inline
    statements in real_run for b-rate unit generators that have only
    one channel method and don't need indirection of (this->*run_channel)(...)

    fhfile is [filename, signature (ab_b), output (classname)]
    params_info has the form
        [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
    interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
    and where the 3rd element may be omitted for a-rate parameters
    """
    print("generate_channel_method", classname, instvars, params_info, rate)
    param_types = fhfile[1][ : -2]  # e.g. ab_a -> ab
    print("------compute method for " + fhfile[2])

    # full copy to avoid side effecting the original params_info list
    params_info = [p.copy() for p in params_info]
    if 'b' in param_types or 'c' in param_types:  # update params_info
        find_b_rate_parameter_faust_names(classname, params_info, src)
    compute = extract_method(classname, "compute", src)
    print(compute + "-----")
    if compute == None: return True
    compute = compute.replace("\t", "    ")
    print("params_info", repr(params_info))

    ## The Declaration is stored in rslt[0]:
    rslt = ["    void chan" + fhfile[1] + "(" + classname + "_state *state) {"]

    compute = compute.split("\n")[1 : -2]
    # lines are separated; remove the line that gets output0
    foundit = None
    for i, line in enumerate(compute):
        if line.find("FAUSTFLOAT* output0 = ") >= 0:
            foundit = i
            break;
    if foundit != None:
        print("found output0, deleting", compute[foundit], "line", foundit)
        del compute[foundit]
    compute = "\n".join(compute)
    if rate == 'a':
        # change output0[i0] to *out_samps++;
        compute = compute.replace("output0[i0]", "*out_samps++");
    else:  # change outputs[0] = ... to *out_samps = ...
        compute = compute.replace("outputs[0]", "*out_samps++")
        # modify compute by pre-pending body of control() method after
        # fixing up references to fControl[] here and in compute
        control = extract_method(classname, "control", src)
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

        i = 0
        while compute.find("fControl[" + str(i) + "]") >= 0:
            print("found fControl[" + str(i) + "]")
            compute = compute.replace("fControl[" + str(i) + "]",
                                      "tmp_" + str(i))
            i += 1
        # add declarations for tmp_0, tmp_1, ...
        for j in range(i):
            compute = compute.replace("    tmp_" + str(j) + " = ",
                                      "    FAUSTFLOAT tmp_" + str(j) + " = ")
        # there are references to fEntry0, etc., but these are replaced later

    # change count to BL
    compute = compute.replace("count", "BL")

    # change each inputs[n] to <param>_samps:
    # careful: n is the number of the a-rate input, so if inputs are ba,
    #   n will be 0 for the 2nd parameter
    i = 0
    for param in params_info:
        arco_name = param[0] + "_samps"
        if len(param) > 2:
            faust_name = param[2]  # b-rate fEntry0, fEntry1, etc.
            arco_name = "*" + arco_name  # substitute with float
        else:
            faust_name = "inputs[" + str(i) + "]"  # a-rate inputs[0], ...
            i += 1  # increment only for each a-rate parameter
        print("replace", faust_name, arco_name)  # substitute address of block
        compute = compute.replace(faust_name, arco_name)
        # also replace fControl[i] with arco_name
        if faust_name.find("fEntry") == 0:
            faust_control_var = "fControl[" + faust_name[6 : ] + "]"
            print("replace", faust_control_var, arco_name)
            compute = compute.replace(faust_control_var, arco_name)

    # change each instvar to state->instvar
    for iv in instvars:
        if not iv.isconst:
            compute = compute.replace(iv.name, "state->" + iv.name)

    # add code for interpolated parameters
    print("add code for interpolation, params_info is", params_info)
    varlist = []
    for i, p in enumerate(params_info):
        if p[1] and fhfile[1][1 + i] != 'a':  # interpolated flag and b-rate
            varlist.append(p[0])

    if len(varlist) > 0:
        (compute, slow_vars) = insert_interpolation(compute, varlist)
        if compute == None: return True
    else:
        slow_vars = []

    print("** final compute method for " + fhfile[2] + "\n" + compute +\
          "\n-----")
    rslt.append(compute)
    rslt.append("    }\n")
    print( "generate_channel_method", classname, "returns", rslt, slow_vars)
    return (rslt, slow_vars)


def generate_channel_methods(fhfiles, classname, instvars, params_info, rate):
    """
    create channel methods, which are specific inner loops that compute one
    channel of output assuming a specific combination of a-rate and b-rate
    inputs. We have to generate a channel method for each combination.

    fhfiles is list of fhfile of the form
        [filename, signature (ab_b), output (classname)]
    params_info has the form [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
        interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
        and where the 3rd element may be omitted for a-rate parameters
    
    Return True on error or string containing all the methods on success
    """
    print("******* generate_channel_methods fhfiles", fhfiles)
    methods_src = []
    all_slow_vars = []
    for fhfile in fhfiles:
        with open(fhfile[2], "r") as inf:
            print("        ******* generate_channel_methods fhfile", fhfile)
            cm, slow_vars = generate_channel_method(fhfile, classname, instvars,
                                                   params_info, inf.read(), rate)
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
        self.arrayspec = ""

    def __str__(self):
        return "<Vardecl: " + self.type + " " + self.name + \
               " " + self.arrayspec + " " + str(self.isconst) + ">"

    def __repr__(self):
        return str(self)


def find_class_declaration(classname, fimpl):
    """Return the location of the class declaration"""
    loc = fimpl.find("class " + classname + " ")
    if loc < 0:
        loc = fimpl.find("class " + classname + ":")
        if loc < 0:
            print("Error: find_private_variables could not find class",
                  classname)
            return None
    return loc


def find_private_variables(fimpl, classname):
    """
    Find the private variables in the declaration of classname.
    remove fsampleRate, fConst*
    Return: list of dictionaries with type, name, arrayspec, e.g. 
              [{"type": "float", "name": "fRec1", "arraylen": "2"}, 
               {"type": "float", "name": "f2", "arraylen": ""}, ...]
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
            # initialize with type and const
            instvar = Vardecl(decl[0], p.find("fConst") >= 0)
            bracket = decl[1].find("[")
            if bracket < 0:
                instvar.name = decl[1][0 : -1]  # remove semicolon
            else:
                instvar.name = decl[1][0 : bracket]  # remove semicolon
                instvar.arrayspec = decl[1][bracket + 1 : -2]
            rslt.append(instvar)
    print("private variables:", repr(rslt))
    return rslt


def generate_state_struct(classname, params_info):
    """
    Generate the state structure, which contains Ugen state for one channel.
    The state structure is replicated for each channel in the constructor.

    params_info has the form 
        [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
    interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
    and where the 3rd element is added by find_b_rate_parameter_faust_names
    as a side effect of this function, and 3rd element may be omitted 
    for a-rate parameters.

    returns a tuple: (list of instance variables, code as a string) 
    """

    global fimpl
    state_struct = "    struct " + classname + "_state {\n"
    # find private variables which include Faust instance variables
    privates = find_private_variables(fimpl, classname)
    #find_b_rate_parameter_faust_names(classname, params_info, fimpl)
    #if privates == None:
    #    return True
    #b_rate_variables = [p[2] for p in params_info if len(p) > 2]
    #print("b_rate_variables", b_rate_variables)

    # declare instance variables from Faust here:
    for p in privates:
        if not p.isconst: # and not p.name in b_rate_variables:
            arrayspec = "[" + p.arrayspec + "];\n" if p.arrayspec != "" \
                        else ";\n"
            state_struct += "        " + p.type + " " + p.name + arrayspec

    # also declare a _prev variable for every interpolated parameter:
    # print("gss params_info", params_info)
    # for p in params_info:
    #     if p[1]:
    #         print("        float", p[0] + "_prev;", file=outf)
    state_struct2 = "    };\n    Vec<" + classname + "_state> states;\n"
    state_struct2 += "    void (" + classname + "::*run_channel)(" + \
                     classname + "_state *state);\n\n"
    return (privates, state_struct, state_struct2)


def generate_state_init(classname, instvars, slow_vars, params_info):
    """
    output loop body that initializes states[i], returns True if error

    params_info has the form
         [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
    interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
    and where the 3rd element may be omitted for a-rate parameters
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
            clear_meth = clear_meth.replace(var.name, "states[i]." + var.name)
    print("** body of clear meth\n" + clear_meth + "\n------")
    
    state_init = "    void initialize_channel_states() {\n"
    state_init += "        for (int i = 0; i < chans; i++) {\n"
    state_init += clear_meth + "\n"
    # initialize _prev variables whether we use them or not:
    for p in slow_vars:
        state_init += "            states[i]." + p + "_prev = 0.0f;\n"
    state_init += "        }\n    }\n\n"
    print("** state_init\n" + state_init + "\n------")
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


def generate_update_run_channel(classname, fhfiles, params_info):
    """The update method inspects all inputs and decides what specific inner

    loop should be called. Inner loops are in channel methods and real_run()
    will make an indirect method call to invoke this method. The update
    method will be called whenever there is a change to an input in case
    the channel method should change.

    fhfiles is a list of fhfile of the form
        [filename, signature (ab_b), output (classname)]
    params_info has the form
        [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
    interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
    and where the 3rd element may be omitted for a-rate parameters
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

    def decision_tree(index, indent_level, fhfiles, params_info,
                      classname, urc):
        """Recursive helper function to select implementation"""
        # split the fhfiles based on the rate at index
        audio_rate_files = []
        block_rate_files = []
        for fhfile in fhfiles:
            if fhfile[1][index + 1] == 'a':  # [1] is the signature
                audio_rate_files.append(fhfile)
            elif fhfile[1][index + 1] == 'b':  # [1] is the signature
                block_rate_files.append(fhfile)
            else:
                raise Exception("bad fhfile signature? " + fhfile[1])
        pname = params_info[index][0]
        if len(audio_rate_files) == 0 or len(block_rate_files) == 0:
            if len(audio_rate_files) == 0:  # must downsample
                urc += ("    " * indent_level) + \
                       "if (" + pname + "->rate == 'a') {\n" + \
                       ("    " * (indent_level + 1)) + \
                       pname + " = new Dnsampleb(-1, " + pname + "->chans, " + \
                           pname + ", LOWPASS500);\n" + \
                       ("    " * indent_level) + "}\n"
                fhfiles = block_rate_files
            else:  # must upsample
                pname = params_info[index][0]
                urc += ("    " * indent_level) + \
                       "if (" + pname + "->rate == 'b') {\n" + \
                       ("    " * (indent_level + 1)) + \
                       pname + " = new Upsample(-1, " + pname + "->chans, " + \
                           pname + ");\n" + ("    " * indent_level) + "}\n"
                fhfiles = audio_rate_files

            index += 1
            if index == len(params_info):  # no more parameters, make a decision
                assert len(fhfiles) == 1  # we should have a unique choice left
                urc += ("    " * indent_level) + "new_run_channel = &" + \
                       classname + "::chan" + fhfiles[0][1] + ";\n"
            else:
                urc = decision_tree(index, indent_level, fhfiles, params_info,
                                    classname, urc)
        else:  # handle left (a) branch and right (b) branch separately
            # first, the left (a) branch:
            if len(audio_rate_files) == 1:
                urc += ("    " * indent_level) + \
                       "if (" + pname + "->rate == 'a') {\n"
                urc += ("    " * (indent_level + 1)) + \
                       "new_run_channel = &" + classname + "::chan" + \
                       audio_rate_files[0][1] + ";\n"
            else:
                urc += ("    " * indent_level) + \
                       "if (" + pname + "->rate == 'a') {\n"
                urc = decision_tree(index + 1, indent_level + 1,
                              audio_rate_files, params_info, classname, urc)
            urc += ("    " * indent_level) + "} else {\n"
            if len(block_rate_files) == 1:
                urc += ("    " * (indent_level + 1)) + \
                        "new_run_channel = &" + classname + "::chan" + \
                        block_rate_files[0][1] + ";\n"
            else:
                urc = decision_tree(index + 1, indent_level + 1,
                              block_rate_files, params_info, classname, urc)
            urc += ("    " * indent_level) + "}\n"

        return urc


    urc = "    void update_run_channel() {\n"  # urc is "update run channel"
    urc += "        // initialize run_channel based on input types\n"
    urc += "        void (" + classname + "::*new_run_channel)(" + \
           classname + "_state *state);\n"
    print("fhfiles:", fhfiles)
    need_else = False
    cond = ""

    if len(params_info) == 0:
        urc += "        new_run_channel = &chan_;"
    else:
        urc = decision_tree(0, 3, fhfiles, params_info, classname, urc)
    urc += "        if (new_run_channel != run_channel) {\n"
    urc += "            initialize_channel_states();\n"
    urc += "            run_channel = new_run_channel;\n        }\n    }\n\n"
    return urc


def generate_arco_h(classname, params_info, rate, fhfiles, outf):
    """
    Write the .h file which is most of the ugen dsp code to outf.

    fhfiles is a list of fhfile of the form:
        [filename, signature (ab_b), output (classname)]
    params_info has the form 
        [['x1', False, 'fEntry0'], ['x2', False, 'fEntry1']]
    interpreted as [[arconame, interpolated, faust_b-rate_name], ...]
    and where the 3rd element may be omitted for a-rate parameters
    """

    global fsrc, fimpl
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

    print("extern const char *" + classname + "_name;\n", file=outf)

    ## Declare the class
    print("class", classname, ": public Ugen {\npublic:", file=outf)

    parameters = [p[0] for p in params_info]
    print("parameters", parameters)
    # from here, we do not write directly to outf because we need to alter
    # state_struct after generating the channel methods, so we accumulate
    # all the needed code in strings to write later...

    instvars, state_struct, state_struct2 = \
            generate_state_struct(classname, params_info)
    if instvars == True: return

    var_decls = ""
    for p in parameters:
        var_decls += "    Ugen_ptr " + p + ";\n    int " + p + "_stride;\n"
        var_decls += "    Sample_ptr " + p + "_samps;\n\n"

    for iv in instvars:
        if iv.isconst:
            arrayspec = "[" + iv.arrayspec + "]" if iv.arrayspec != "" else ""
            var_decls += "    " + iv.type + " " + iv.name + arrayspec + ";\n"

    ## Generate the constructor
    constructor = "\n    " + classname + "(int id, int nchans"
    for p in parameters:
        constructor += ", Ugen_ptr " + p + "_"
    constructor += ") :\n            Ugen(id, '" + rate + "', nchans) {\n"

    for p in parameters:
        constructor += "        " + p + " = " + p + "_;\n"

    if len(parameters) > 0:
        constructor += "        flags = CAN_TERMINATE;\n"

    # initialize states
    constructor += "        states.set_size(chans);\n"
    constants = initialize_constants(classname, instvars, rate)
    if constants == True:  # error occurred
        return
    constructor += constants

    # call init_<var> for every parameter
    for p in parameters:
        constructor += "        init_" + p + "(" + p + ");\n"
    if rate == 'a':
        constructor += "        run_channel = (void (" + classname + \
                       "::*)(" + classname + "_state *)) 0;\n"
        constructor += "        update_run_channel();\n"
    constructor += "    }\n\n"

    ## Generate the destructor
    constructor += "    ~" + classname + "() {\n"
    for p in parameters:
        constructor += "        " + p + "->unref();\n"
    constructor += "    }\n\n"

    ## Generate name() method
    constructor += "    const char *classname() { return " + classname + \
                   "_name; }\n\n"

    ## Generate update_run_channel method to set the run_channel
    update_run_channel = ""
    if rate == 'a':
        update_run_channel = generate_update_run_channel(classname, fhfiles,
                                                         params_info)

    pr_sources = ""
    if len(parameters) > 0:
        pr_sources += "    void print_sources(int indent, bool print_flag) {\n"
        for p in parameters:
            pr_sources += "        " + p + \
                          '->print_tree(indent, print_flag, "' + p + '");\n'
        pr_sources += "    }\n\n"

    methods = ""
    ## Generate repl_* methods to set inputs
    for p in parameters:
        methods += "    void repl_" + p + "(Ugen_ptr ugen) {\n"
        methods += "        " + p + "->unref();\n"
        methods += "        init_" + p + "(ugen);\n"
        if rate == 'a':
            methods += "        update_run_channel();\n"
        methods += "    }\n\n"

    ## Generate set_* methods to set constant rate input parameters
    for p in parameters:
        methods += "    void set_" + p + "(int chan, float f) {\n"
        methods += "        " + p + "->const_set(chan, f, "
        methods += '"' + classname + "::set_" + p + '");\n'
        methods += "    }\n\n"

    ## Generate init_* methods shared by constructor and repl_* methods
    for p in parameters:
        methods += "    void init_" + p + \
                   "(Ugen_ptr ugen) { init_param(ugen, " + p + ", " + \
                   p + "_stride); }\n\n"
    
    ## Generate channel methods -- process one channel of input/output
    if rate == 'a':
        (channel_meths, slow_vars) = generate_channel_methods(fhfiles,
                                classname, instvars, params_info, rate)
        if channel_meths == True:
            return
    else:
        channel_meths = ""
        slow_vars = []

    # Finish state_struct by declaring slow_vars
    for slow_var in slow_vars:
        state_struct += "        Sample " + slow_var + "_prev;\n"

    # Generate state initialization code (this will be inserted near
    # the top of the file within the constructor, but created here
    # after we've determined the state needed for interpolated
    # signals.
    constr_init = generate_state_init(classname, instvars, slow_vars,
                                      params_info)
    if constr_init == True: 
        return

    initializer_code = generate_initializer_code(fimpl, classname, rate)
    if initializer_code == True:
        return

    ## Generate real_run method -- compute all output channels from inputs
    real_run = "    void real_run() {\n"
    for p in parameters:
        real_run += "        " + p + "_samps = " + \
                    p + "->run(current_block); // update input\n"

    # add tests for termination
    if len(terminate_info) > 0:
        real_run += "        if ((("
        join_with_or = ""
        for p in terminate_info:
            real_run += join_with_or + p + "->flags"
            join_with_or = " | "
        real_run += ") & TERMINATED) &&\n" + \
                    "            (flags & CAN_TERMINATE)) {\n" + \
                    "            terminate();\n        }\n"

    # do the signal computation
    real_run += "        " + classname + "_state *state = &states[0];\n"
    real_run += "        for (int i = 0; i < chans; i++) {\n"
    if rate == 'a':
        real_run += "            (this->*run_channel)(state);\n"
    else:  # there is only one b-rate computation, so put the channel
           #     method inline:
        cm, slow_vars = generate_channel_method(fhfiles[0], classname, instvars,
                                                params_info, fimpl, rate)
        if cm == True:
            return
        # extract and indent the body of the compute function
        cm = ["    " + line for line in cm[1].split("\n")]
        real_run += "\n".join(cm)
    real_run += "            state++;\n"
    for p in parameters:
        real_run += "            " + p + "_samps += " + p + "_stride;\n"
    real_run += "        }\n"
    real_run += "    }\n"
    real_run += "};\n"
    real_run += "#endif\n"
    all_h = [state_struct, state_struct2, var_decls, constructor, constr_init,
             update_run_channel, pr_sources, methods, channel_meths, real_run]
    for part in all_h:
        print(part, file=outf, end="")
    return initializer_code


def get_params_info(file, rate, ugen_params):
    global fsrc, fimpl  # main faust source file (.dsp) and implementation (.fh)
    """
    extract parameter names and interpolated status from file
    file: main .dsp file
    rate: ugen output rate
    ugen_params: list of parameter names
    returns: [[p1, False], [p2, True], ...] where boolean indicates
        interpolation when the output is audio, or on error, True.
    """
    with open(file) as f:
        fsrc = f.read()
    fsrc = fsrc.replace("\t", "    ")
    if rate == None:
        print("Error: no rate determined for", file)
        return True
    if rate == 'a':
        return get_params_a(fsrc, ugen_params)
    elif rate == 'b':
        return get_params_b(fsrc)
    else:
        print("Error: rate for", file, "is invalid (" + rate + ")")
        return True


def get_params_a(fsrc, ugen_params):
    """
    extract parameter names and interpolated status from file
    fsrc: the input file, a FAUST .dsp file
    ugen_params: list of parameter names derived from .ugen file
    returns: [[p1, False], [p2, True], ...] where boolean indicates
        interpolation when the output is audio, or on error, True
    """
    """
    print("** get_params_a fsrc:\n", fsrc, "\n-------")
    loc = fsrc.find("process(")
    if loc < 0:
        loc = fsrc.find("process")
        if loc < 0:
            print('Error: could not find "process"')
            return True
        # process = ... is legal, implies empty parameter list
        loc = skip_to_non_space(fsrc, loc + 7)  # search after "process"
        if fsrc[loc] != "=":
            print('Erorr: did not find "=" after "process"')
            return True
        params = []
    else:
        loc2 = fsrc.find(")", loc)
        if loc2 < 0:
            print("Error: could not find ')' after \"process(\"")
            return True
        params = fsrc[loc + 8 : loc2]
        params = params.replace(",", " ").split()
    """
    params = ugen_params
    # now look for interpolated declaration
    interpolated = []
    loc = fsrc.find("declare interpolated")
    print("interpolated declare at", loc, "SOURCE\n", fsrc, "\n--------")
    if loc >= 0:
        loc = fsrc.find('"', loc)  # first quote
        if loc < 0:
            print("Error: could not find string after declare interpolated")
            return True
        loc2 = fsrc.find('"', loc + 1)
        print("interpolated loc", loc, "loc2", loc2)
        interpolated = fsrc[loc + 1 : loc2].replace(",", " ").split()
    print("interpolated", interpolated)

    # build result
    for i in range(len(params)):
        params[i] = [params[i], params[i] in interpolated]
    return params


def get_params_b(fsrc):
    """
    extract parameter names from block-rate-output file
    returns: [[p1, False], [p2, False], ...] where boolean indicates
        interpolation, so always False; return True on error
    """
    # find the nentries to get the parameter list
    loc = fsrc.find("nentry(")
    params = []
    while loc >= 0:
        loc = fsrc.find('"', loc)
        if loc < 0:
            print('Error: get_params_b did not find \'"\' after "nentry("')
            return True
        loc2 = fsrc.find('"', loc + 1)
        if loc2 < 0:
            print('Error: get_params_b did not find 2nd \'"\' after "nentry("')
            return True
        params.append([fsrc[loc + 1 : loc2], False])
        loc = fsrc.find("nentry(", loc2)
    return params


def run_faust(classname, file, outfile):
    """run the faust command to generate outfile"""
    blockrate = ""
    if file.find("b.dsp") >= 0:
        blockrate = "-os "  # one sample if b-rate
    print("In f2a.py, PATH=" + os.environ['PATH'])
    err = os.system("faust -light -cn " + classname + " " + blockrate +
                    file + " -o " + outfile)
    if err != 0:
        print("Error: quit because of error reported by Faust")
        return True
    else:
        print("     Translation completed, generated ", outfile)
        return False



def extract_ugen_params(classname):
    """Get the parameter names in a list from classname.ugen"""
    filename = classname + '.ugen'
    try:
        with open(filename, 'r') as file:
            content = file.read()
        
        # Find the word "process" followed by "("
        process_index = content.find('process')
        if process_index == -1:
            print(f"No 'process(' found in {filename}")
            return []
        
        # Find the opening and closing parentheses
        start_index = content.find('(', process_index)
        end_index = content.find(')', start_index)
        
        if start_index == -1 or end_index == -1:
            print(f"No paren after 'process' in {filename}")
            return []
        
        # Extract the comma-separated words between "(" and ")"
        params_str = content[start_index + 1:end_index]
        params = [param.strip() for param in params_str.split(',')]
        return params
    except FileNotFoundError:
        print(f"File {filename} not found")
        return []


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
            print("Error: output rate is", output_rate,
                  "but input file is", file)
            return
        output_rate = signature[-1]

        # top_signature has a, b, or c for each parameter. It has a
        # if *any* variant has an a for that parameter; it has b if
        # *no* variant has an a for that parameter; it has c if *all*
        # variants have c for that parameter. It *any* variant has a
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
            
    # find params_info - "master" parameter list is in the .ugen file
    #   but we also use the main_file
    ugen_params = extract_ugen_params(classname)
    print("extract_ugen_params:", ugen_params)
    params_info = get_params_info(main_file, output_rate, ugen_params)
    if params_info == True: return
    print("get_params_info:", params_info)

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
        init_code = generate_arco_h(classname, params_info, output_rate,
                                    fhfiles, outf)
    with open(classnamelc + ".cpp", "w") as outf:
        generate_arco_cpp(classname, params_info, output_rate, init_code, outf)
    
main()
