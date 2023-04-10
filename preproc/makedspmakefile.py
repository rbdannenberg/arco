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
"""

NONFAUST = ["pwl", "pwlb", "delay", "mix"]


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
    for ugen in manifest:
        if ugen not in NONFAUST:
            if ugen[-1 : ] == "*":
                sources.append(ugen[0 : -1])
                # sources.append(ugen[0 : -1] + "b")
            else:
                sources.append(ugen)

    print("all:", file=outf)
    for source in sources:
        print(" " + arco_path + "/ugens/" + source + ".cpp", 
              end="", file=outf)
    print("\n", file=outf)
    
    # for each source, write the command to generate it
    # this generates all variants (a-rate, b-rate) and both .cpp and .h
    for source in sources:
        src_path = arco_path + "/ugens/" + source + "/"
        print(src_path + source + ".cpp: " + src_path + source + ".ugen", file=outf)
        print("\tcd " + arco_path + "/ugens/" + source + \
              "; python3 " + arco_path + "/preproc/u2f.py " + source,
              file=outf)
        print("\tcd " + arco_path + "/ugens/" + source + \
              "; sh generate_" + source + ".sh", file=outf)
        print("\n", file=outf)



def make_inclfile(arco_path, manifest, outf):
    "make the CMake include file that lists all the sources"

    print("set(ARCO_SRC ${ARCO_SRC}", file=outf)

    for ugen in manifest:
        ug = ugen[0 : -1]
        if ugen in NONFAUST:
            print("    src/" + ugen + ".cpp src/" + ugen + ".h", file=outf)
        elif ugen[-1 : ] == "*":
            src = arco_path + "/ugens/" + ug + "/" + ug
            print("    " + src + ".cpp " + src + ".h", file=outf)
        else:
            src = arco_path + "/ugens/" + ug + "/" + ug + "b"
            print("    " + arco_path + "/" + src + "/" + src + ".cpp " + \
                  arco_path + "/" + src + "/" + src + ".h", file=outf)

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
