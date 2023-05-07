# makedspmakefile.py -- dspmanifest.txt -> dspmakefile and dspsources.cmakeinclude
#
# Roger B. Dannenberg
# April 2023

import sys
import os

"""
Command line should have three parameters:
    path-to-arco-root
    path-to-dspmanifest.txt
    path-to-dspmakefile
    path-to-dspsources.cmakeinclude

Each line of dspmanifest (not starting with '#') is either a unit
generator name, or a name followed by '*'. The latter stands for
both audio rate and control rate versions. E.g. you could use one
of the following three lines:
    sine  -- audio rate unit generator
    sineb -- control rate unit generator
    sine* -- both audio and control rate generators
In the last case (sine*), both sine.cpp and sineb.cpp are created
in the same sine subdirectory named ugens/sine/

The makefile also makes allugens.srp by appending all the ugen.srp
files corresponding to classes in the manifest. allugens.srp is
written to the same folder as dspmanifest.txt.
"""

NONFAUST = ["thru", "zero", "vu", "pwl", "pwlb", "delay", "mix", \
            "fileplay", "filerec", "fileio", "recplay"]


def make_makefile(arco_path, manifest, outf):
    """make the makefile for faust-based dsp .cpp and .h files"""

    print("# dspmakefile - a makefile for Arco unit generator dsp sources", \
          file=outf)
    print("#\n", file=outf)

    # make a list of all the .cpp files we will generate. Ignore .h files
    # and assume they are created along with .cpp files. Similarly,
    # ignore b-rate versions, so sine* just describes dependent sine.cpp, but
    # when sine.cpp is generated, so are sine.h, sineb.cpp, sineb.h
    sources = []
    srp_srcs = []
    srp_path = arco_path + "/serpent/srp/"
    ugens_path = arco_path + "/ugens/"
    if "thru" not in manifest:
        manifest.append("thru")
    if "zero*" not in manifest:
        manifest.append("zero*")
    for ugen in manifest:
        basename = ugen
        if ugen[-1 : ] == "*":
            basename = ugen[0 : -1]
        if basename in NONFAUST:
            srp_srcs.append(srp_path + basename + ".srp")
        else:
            sources.append(ugens_path + basename + "/" + basename + ".cpp")
            srp_srcs.append(ugens_path + basename + "/" + basename + ".srp")

    print("all:  ", end="", file=outf)
    for source in sources:
        print(" \\\n    " + source, end="", file=outf)
    print(" \\\n    allugens.srp", file=outf)
    
    # for each source, write the command to generate it
    # this generates all variants (a-rate, b-rate) and both .cpp and .h
    for source in sources:
        sans_extension = source[ : -3]
        src_path, basename = sans_extension.rsplit("/", 1)
        print(source + ": " + sans_extension + "ugen " + \
              arco_path + "/preproc/u2f.py", file=outf)
        print("\tcd " + src_path + "; python3 " + arco_path + \
              "/preproc/u2f.py " + source, file=outf)
        print("\tcd " + src_path + "; sh " + src_path + \
              "/generate_" + basename + "sh", file=outf)
        print("\n", file=outf)

    # write the code to make allugens.srp
    print("allugens.srp:", end="", file=outf)
    for src in srp_srcs:
        print(" " + src, end="", file=outf)
    print("\n\trm -f allugens.srp", file=outf)
    print("\ttouch allugens.srp", file=outf)
    for src in srp_srcs:
        print("\tcat", src, ">> allugens.srp", file=outf)


def make_inclfile(arco_path, manifest, outf):
    "make the CMake include file that lists all the sources"

    # we need either fileio or nofileio
    # use fileio if either fileio or fileplay or filerec is in manifest
    #
    if ("fileplay" in manifest) or ("fileio" in manifest) or \
       ("fileio" in manifest):
        needed = "fileio"
        rejected = "nofileio"
    else:
        needed = "nofileio"
        rejected = "fileio"
    if needed not in manifest:
        manifest.append(needed)
    if rejected in manifest:
        manifest.remove(rejected)

    print("set(ARCO_SRC ${ARCO_SRC}", file=outf)

    for ugen in manifest:
        both = False
        basename = ugen
        if ugen[-1 : ] == "*":
            both = True
            basename = ugen[0 : -1]
        if basename in NONFAUST:
            # if manifest had ugen*, include both ugen and ugenb. Otherwise,
            # just include the specified ugen.
            if both or basename == ugen:
                print("    src/" + basename + ".cpp src/" + basename + ".h", \
                      file=outf)
            if both:
                print("    src/" + basename + "b.cpp src/" + basename + "b.h", \
                      file=outf)
        elif both:
            src = arco_path + "/ugens/" + basename + "/" + basename
            print("    " + src + ".cpp " + src + ".h", file=outf)
            print("    " + src + "b.cpp " + src + "b.h", file=outf)
        else:
            src = arco_path + "/ugens/" + basename + "/" + ugen
            print("    " + src + ".cpp " + src + ".h", file=outf)

    print(")", file=outf)


def is_a_unit_generator(line):
    "determine if line describes a unit generator (not empty, not a comment)"
    # length must be 2 or more to have a string and a newline
    return len(line) > 1 and line[0] != "#"


def main():
    if len(sys.argv) != 5:
        print("Usage: python3 makedspmakefile.py arco_path " + \
              "dspmanifest.txt dspmakefile dspsources.cmakeinclude")
        exit(-1)
    arco_path = sys.argv[1]
    manifest_name = sys.argv[2]
    makefile_name = sys.argv[3]
    inclfile_name = sys.argv[4]

    # first remove existing inclfile_name in case we fail
    try:
        os.remove(inclfile_name)
    except:
        print("Did not find or remove", inclfile_name)
    # read the manifest
    with open(manifest_name, "r") as inf:
        manifest = inf.readlines()
    # remove comments
    manifest = list(filter(is_a_unit_generator, manifest))
    # remove newlines
    manifest = [ugen[:-1] for ugen in manifest]

    with open(makefile_name, "w") as outf:
        make_makefile(arco_path, manifest, outf)
    with open(inclfile_name, "w") as outf:
        make_inclfile(arco_path, manifest, outf)


main()
