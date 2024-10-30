# makedspmakefile.py -- dspmanifest.txt -> dspmakefile and
#                       dspsources.cmakeinclude
#
# Roger B. Dannenberg
# April 2023
#
# Run this in the application direcotry, e.g. arco/apps/test

import sys
import os

WIN32 = (os.name == 'nt')
PY = "python" if WIN32 else "python3"

USE_PFFFT = True  # use PFFFT (new) instead of ffts for FFTs

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

NONFAUST = ["thru", "zero", "zerob", "vu", "probe", "pwl", "pwlb", "delay", \
            "allpass", "mix", "fileplay", "filerec", "fileio", "nofileio", \
            "recplay", "olapitchshift", "feedback", "granstream", "pwe", \
            "pweb", "flsyn", "pv", "yin", "trig", "dualslewb", "dnsampleb", \
            "smoothb", "route", "multx", "fader", "sum", "sumb", "mathugen", \
            "mathugenb", "unaryugen", "unaryugenb", "onset", "chorddetect", "o2audioio", \
            "spectralcentroid", "spectralrolloff", "tableosc", "tableoscb", \
            "stdistr", "blend", "blendb"]

MATHUGENS = ["mult", "add", "sub", "ugen_div", "ugen_max", "ugen_min", \
             "ugen_clip", "ugen_pow", "ugen_less", "ugen_greater", "ugen_soft_clip", \
             "ugen_powi", "ugen_rand", "ugen_sample_hold", "ugen_quantize", "ugen_rli", \
             "ugen_hzdiff", "ugen_tan", "ugen_atan2", "ugen_sin", "ugen_cos"]

UNARYUGENS = ["ugen_abs", "ugen_neg", "ugen_exp", "ugen_log", "ugen_log10", \
              "ugen_log2", "ugen_sqrt", "ugen_step_to_hz", "ugen_hz_to_step", \
              "ugen_vel_to_lin", "ugen_lin_to_vel"]

# compute all names for which there is an a-rate or b-rate version, but not
# both. Put the "missing" name in NONEXIST:
NONEXIST = []  # names here are computed in the following for-loop:

for ugclass in NONFAUST:
    # case 1: ugclass b-rate is specified, but not ugclass a-rate
    if ugclass[-1 : ] == "b":
        arate_name = ugclass[ : -1]
        if arate_name not in NONFAUST:
            NONEXIST.append(arate_name)
    # case 2: ugclass a-rate is specified, but not ugclass b-rate
    elif (ugclass + "b") not in NONFAUST:
        NONEXIST.append(ugclass + "b")

print(NONEXIST)

def append_to_srp_srcs(spec, path, nonfaust):
    """add a Serpent source file to be included in allugens.srp.
    spec - the specification (ugen name) in manifest.txt
    path - the path for the .srp file
    nonfaust - is this a non-Faust Ugen? If so, the .srp file should
        already exist. Otherwise, the .srp file will be created by
        the dspmakefile.
    """
    global srp_srcs
    if nonfaust and not os.path.isfile(path):
        print('ERROR: dspmanifest.txt specifies "', spec, '" but "', \
              path, '" does not exist.', sep = "", file=sys.stderr)
    else:
        srp_srcs.append(path)



def make_makefile(arco_path, manifest, outf):
    """make the makefile for faust-based dsp .cpp and .h files"""
    global srp_srcs

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

    for ugen in manifest:
        basename = ugen
        if ugen[-1 : ] == "*":
            basename = ugen[0 : -1]
        if basename in NONFAUST:
            # special case: mult.srp selects to instantiate either Mult or
            # Multx, and the Serpent code is in srp_path, but named math.srp:
            if basename == "multx":
                if "math" in manifest or "mathb" in manifest or \
                   "math*" in manifest:
                    continue  # multx is handled by math, already in list
                else:
                    basename = "mathugen"
            source = srp_path + basename + ".srp"
            append_to_srp_srcs(ugen, source, True)
        else:
            sources.append(ugens_path + basename + "/" + basename + ".cpp")
            if basename == "mult":  # get "mult" code from srp_path,
                                    # not ugens_path:
                append_to_srp_srcs(ugen, srp_path + "mult.srp", True)
            elif basename != "fader":  # fader is included by arco.srp
                append_to_srp_srcs(ugen, ugens_path + basename + "/" + \
                                         basename + ".srp", False)

    print("all:  ", end="", file=outf)
    for source in sources:
        print(" \\\n    " + source, end="", file=outf)
    print(" \\\n    allugens.srp\n\n", file=outf)
    
    # for each source, write the command to generate it
    # this generates all variants (a-rate, b-rate) and both .cpp and .h
    for source in sources:
        sans_extension = source[ : -4]
        src_path, basename = sans_extension.rsplit("/", 1)
        cmd = "sh " + src_path + "/generate_" + basename + ".sh"
        # For make, u2f.py command has to be on the same line as the
        # cd so it runs in the right directory. Make/Unix can use
        # the ";" separator for commands:
        cdsep = "; "
        if WIN32:
            cmd = src_path + "/generate_" + basename + ".bat"
            # For Windows NMake, the cd command can go on a
            # separate line (and I don't think ";" would work):
            cdsep = "\n\t"
        print(source + ": " + sans_extension + ".ugen " + \
              arco_path + "/preproc/u2f.py", file=outf)
        print("\tcd " + src_path + cdsep + PY + " " + arco_path + \
              "/preproc/u2f.py " + basename, file=outf)
        print("\tcd " + src_path + cdsep + cmd, file=outf)
        # does NMake not restore the current directory? Not sure.
        if WIN32:
            print("\tcd " + arco_app_path + "\n", file=outf)
        print("\n", file=outf)

    # write the code to make allugens.srp
    # nmake needs dspmakefile to be a target, even though we never use
    # make or nmake to make dspmakefile; we only want to depend on it
    # and regenerate allugens when dspmakefile changes:
    print("dspmakefile:", file=outf)
    if WIN32:
        print('\tdir dspmakefile', file=outf)
    print('\techo "ERROR: dspmakefile does not exist!"', file=outf)
    print("", file=outf)

    print("allugens.srp: dspmakefile", end="", file=outf)
    for src in srp_srcs:
        print(" " + src, end="", file=outf)
    if WIN32:
        print("\n\tdel allugens.srp", file=outf)
        print("\tcopy NUL allugens.srp", file=outf)
    else:
        print("\n\trm -f allugens.srp", file=outf)
        print("\ttouch allugens.srp", file=outf)
    for src in srp_srcs:
        if WIN32:
            print('\techo. >> allugens.srp', file=outf)
            print('\techo # ---- included from', src, \
                  '----" >> allugens.srp', file=outf)
            print('\techo. >> allugens.srp', file=outf)
            src = src.replace('/', '\\')  # convert to Windows path
            print("\ttype", src, ">> allugens.srp", file=outf)
            # this newline also is needed for files with no final newline:
            print("\techo. >> allugens.srp", file=outf)  # blank line separator
        else:
            print('\techo "\\n# ---- included from', src, \
                  '----\\n" >> allugens.srp', file=outf)
            print("\tcat", src, ">> allugens.srp", file=outf)
            # this newline also is needed for files with no final newline:
            print("\techo >> allugens.srp", file=outf)  # blank line separator


def make_inclfile(arco_path, manifest, outf):
    "make the CMake include file that lists all the sources"

    need_ringbuf = False  # need to compile with ringbuf.h
    need_fastrand = False # need to compile with fastrand.h
    need_flsyn_lib = False  # special: ugen needs fluidsynth library
    need_onset_lib = False
    need_fft = False  # need to compile and link with ffts files
    need_windowed_input = False  # need to compile and link with windowedinput.h
        # (this is an abstract superclass; perhaps multiple ugens depend on it)
    need_dcblocker = False # need to compile and link with dcblocker.h
    need_audioblob = False  # need to compile and link with audioblob.h
                            # (for o2audioio)
    need_wavetables = False  # need to compile and link with wavetables.{cpp,h}
    
    # we need either fileio or nofileio
    # use fileio if either fileio or fileplay or filerec is in manifest
    #
    if ("fileplay" in manifest) or ("filerec" in manifest) or \
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

    ## Compute Dependencies
    if "fileio" in manifest:  # need fileiothread
        print("    " + arco_path + "/arco/src/fileiothread.cpp",
                       arco_path + "/arco/src/fileiothread.h",
              file=outf)

    if "granstream" in manifest:  # add ringbuf which granstream depends on
        need_ringbuf = True
        need_fastrand = True

    if "pv" in manifest:  # add cmupv which pv depends on
        print("    " + arco_path + "/cmupv/src/cmupv.c",
                       arco_path + "/cmupv/src/cmupv.h\n",
              "    " + arco_path + "/cmupv/src/internal.c",
                       arco_path + "/cmupv/src/internal.h\n",
              "    " + arco_path + "/cmupv/src/resamp.h",
              file=outf)
        need_fft = True

    if "yin" in manifest:
        need_windowed_input = True

    if ("delay" in manifest) or ("granstream" in manifest):
        need_dcblocker = True

    if ("o2audioio" in manifest):
        need_audioblob = True

    if "onset" in manifest:  # add modal implementation files
        df_path = arco_path + "/modal/modal/detectionfunctions/"
        print("    " + df_path + "detectionfunctions.cpp",
                      df_path + "detectionfunctions.h\n",
              "    " + df_path + "mq.cpp",
                      df_path + "mq.h", file=outf)
        ms_path = arco_path + "/modal/src/"
        print("    " + ms_path + "onsetdetection.cpp",
                      ms_path + "onsetdetection.h", file=outf)
        need_fft = True
    
    # add chromagram and chord detection implementation files:
    if "chorddetect" in manifest:
        print("    src/Chromagram.cpp src/Chromagram.h\n",
              "    src/ChordDetector.cpp src/ChordDetector.h", file=outf)
        need_fft = True
    
    if "spectralcentroid" in manifest:  # add FFTCalculator implementation files
        need_fft = True
    
    if "spectralrolloff" in manifest:  # add FFTCalculator implementation files
        need_fft = True

    if "tableosc" in manifest or "tableoscb" in manifest or \
       "tableosc*" in manifest:
        need_wavetables = True

    ## Include source files to satisfy dependencies
    if need_ringbuf:
        print("    src/ringbuf.h", file=outf)

    if need_fastrand:
        print("    src/fastrand.h", file=outf)

    if need_fft:
        if USE_PFFFT:
            print("    " + arco_path + "/pffft/pffft.c",
                          arco_path + "/pffft/pffft.h\n",
                  "    " + arco_path + "/pffft/ffts_compat.cpp",
                          arco_path + "/pffft/ffts_compat.h",
                  file=outf)
        else:
            print("    " + arco_path + "/ffts/src/fftext.c",
                          arco_path + "/ffts/src/fftext.h\n",
                  "    " + arco_path + "/ffts/src/fftlib.c",
                          arco_path + "/ffts/src/fftlib.h\n",
                  "    " + arco_path + "/ffts/src/matlib.c",
                          arco_path + "/ffts/src/matlib.h",
                  file=outf)
        print("    src/FFTCalculator.cpp src/FFTCalculator.h", file=outf)

    if need_windowed_input:
        print("    src/windowedinput.h", file=outf)

    if need_dcblocker:
        print("    src/dcblock.h", file=outf)

    if need_audioblob:
        print("    src/audioblob.h src/audioblob.cpp", file=outf)

    if need_wavetables:
        print("    src/wavetables.cpp src/wavetables.h", file=outf)

    ## Add source files for specified ugens
    print("Adding these unit generator implementations to the executable:")
    for ugen in manifest:
        both = False
        basename = ugen
        if ugen[-1 : ] == "*":
            both = True
            basename = ugen[0 : -1]
        if basename in NONFAUST:
            src = "src/"
            if basename in NONEXIST:
                print('ERROR: dspmanifest.txt specifies "', ugen, \
                      '" but "',  basename, '.cpp" does not exist.', \
                     sep = "", file=sys.stderr)
            if both and (basename + "b") in NONEXIST:
                print('ERROR: dspmanifest.txt specifies "', ugen, \
                      '" but "', basename, 'b.cpp" does not exist.', \
                      sep = "", file=sys.stderr)
        else:
            src = arco_path + "/ugens/" + basename + "/"

        # if manifest had ugen*, include both ugen and ugenb. Otherwise,
        # just include the specified ugen.
        sources = "    " + src + basename + ".cpp " + src + basename + ".h"
        print(sources, file=outf)
        print(sources)
        if both:
            sources = "    " + src + basename + "b.cpp " + src + \
                      basename + "b.h"
            print(sources, file=outf)
            print(sources)
        if basename == "flsyn":
            need_flsyn_lib = True

    print(")", file=outf)
    print("\ntarget_sources(arcolib PRIVATE ${ARCO_SRC})", file=outf)
    if need_flsyn_lib:
        print("target_link_libraries(arcolib PRIVATE", file=outf)
        print("    debug ${FLSYN_DBG_LIB} optimized ${FLSYN_OPT_LIB})",
              file=outf)

        print('if(NOT (GLIB_DBG_LIB STREQUAL "ignore"))', file=outf)
        print('  if(NOT EXISTS "${GLIB_DBG_LIB}")', file=outf)
        print('    message(FATAL_ERROR "Could not find ${GLIB_DBG_LIB},',
              'fix the definition in apps/common/libraries.txt")', file=outf)
        print("  endif()", file=outf)
        print('  if(NOT EXISTS "${GLIB_OPT_LIB}")', file=outf)
        print('    message(FATAL_ERROR "Could not find ${GLIB_OPT_LIB},',
              'fix the definition in apps/common/libraries.txt")', file=outf)
        print("  endif()", file=outf)
        print("  target_link_libraries(arcolib PRIVATE", file=outf)
        print("      debug ${GLIB_DBG_LIB} optimized ${GLIB_OPT_LIB})",
              file=outf)
        print("endif()", file=outf)
              
        print('if(NOT (INTL_DBG_LIB STREQUAL "ignore"))', file=outf)
        print('  if(NOT EXISTS "${INTL_DBG_LIB}")', file=outf)
        print('    message(FATAL_ERROR "Could not find ${INTL_DBG_LIB},',
              'fix the definition in apps/common/libraries.txt")', file=outf)
        print("  endif()", file=outf)
        print('  if(NOT EXISTS "${INTL_OPT_LIB}")', file=outf)
        print('    message(FATAL_ERROR "Could not find ${INTL_OPT_LIB},',
              'fix the definition in apps/common/libraries.txt")', file=outf)
        print("  endif()", file=outf)
        print("  target_link_libraries(arcolib PRIVATE", file=outf)
        print("      debug ${INTL_DBG_LIB} optimized ${INTL_OPT_LIB})",
              file=outf)
        print("endif()", file=outf)

#        print("target_link_libraries(arcolib PRIVATE", file=outf)
#        print("    debug ${INTL_DBG_LIB} optimized ${INTL_OPT_LIB})",
#              file=outf)

        # On windows, we build fluidsynth from sources and do not use/enable
        # readline library. On non-windows, I think we use an installer or
        # prebuilt binary that needs readline.
        if not WIN32:
            print("target_link_libraries(arcolib PRIVATE", file=outf)
            print("    debug readline optimized readline)", file=outf)
        # make this one public - I don't want to ever link curses in twice,
        # and I'm not sure how this interacts with Arco Server's use of
        # curses. Not even sure why Fluidsynth needs curses if I'm just
        # trying to synthesize sound and not running it as an application.
#        print("target_link_libraries(arcolib PUBLIC", file=outf)
#        print("    debug ${CURSES_LIB} optimized ${CURSES_LIB})", file=outf)
#        print("set(ARCO_TARGET_LINK_OBJC true PARENT_SCOPE)", file=outf)

    # ADDITION FOR CHROMAGRAM following the flsyn format
#    if need_chromagram_lib:
#        print("target_link_libraries(arcolib PRIVATE", file=outf)
#        print("    debug ${CHROMAGRAM_DBG_LIB} optimized ${CHROMAGRAM_OPT_LIB})",
#              file=outf)
#
#        print('if(NOT EXISTS "${CHROMAGRAM_DBG_LIB}")', file=outf)
#        print('  message(FATAL_ERROR "Could not find ${CHROMAGRAM_DBG_LIB}, ' + \
#              'delete CHROMAGRAM_DBG_LIB from cache and fix in ' + \
#              'apps/common/libraries.txt")', file=outf)
#        print("endif()", file=outf)
#        print('if(NOT EXISTS "${CHROMAGRAM_OPT_LIB}")', file=outf)
#        print('  message(FATAL_ERROR "Could not find ${CHROMAGRAM_OPT_LIB}, ' + \
#              'delete CHROMAGRAM_OPT_LIB from cache and fix in ' + \
#              'apps/common/libraries.txt")', file=outf)
#        print("endif()", file=outf)
#              
#        print("target_link_libraries(arcolib PRIVATE", file=outf)
#        print("    debug readline optimized readline)", file=outf)

#    if need_fftw:
#        print("target_link_libraries(arcolib PRIVATE", file=outf)
#        print("    debug ${FFTW_DBG_LIB} optimized ${FFTW_OPT_LIB})",
#              file=outf)
#
#        print('if(NOT EXISTS "${FFTW_DBG_LIB}")', file=outf)
#        print('  message(FATAL_ERROR "Could not find ${FFTW_DBG_LIB}, ' + \
#              'delete FFTW_DBG_LIB from cache and fix in ' + \
#              'apps/common/libraries.txt")', file=outf)
#        print("endif()", file=outf)
#        print('if(NOT EXISTS "${FFTW_OPT_LIB}")', file=outf)
#        print('  message(FATAL_ERROR "Could not find ${FFTW_OPT_LIB}, ' + \
#              'delete FFTW_OPT_LIB from cache and fix in ' + \
#              'apps/common/libraries.txt")', file=outf)
#        print("endif()", file=outf)
#
#        print("target_link_libraries(arcolib PRIVATE", file=outf)
#        print("    debug readline optimized readline)", file=outf)

def is_a_unit_generator(line):
    "determine if line describes a unit generator (not empty, not a comment)"
    # length must be 2 or more to have a string and a newline
    return len(line) > 1 and line[0] != "#"


def need_ugen(manifest, name):
    """ Take names of the form xxx or xxxb (but not xxx*) and insert them
    as needed into manifest. If you insert both, in any order, they will
    be replaced with the form xxx*. If you try to insert a duplicate, it
    will be ignored.  Also, if the name or name - "b" is in MATHUGENS,
    use "mathugen" or "mathugenb" in place of name since "mathugen" and
    "mathugenb" implement all of the names in MATHUGENS.  Similarly for
    UNARYUGENS.
    """
    base = None
    if name[-1] == "b":
        base = name[0 : -1]

    if name in manifest:
        return
    if base and ((base + "*") in manifest):
        return
    elif (name + "*") in manifest:
        return

    # check for MATHUGENS:
    if (base and (base in MATHUGENS)):  # must be xxxb form
        need_ugen(manifest, "mathugenb")
        return
    elif (name in MATHUGENS):  # no base, so must be xxx form
        need_ugen(manifest, "mathugen")
        return

    # check for UNARYUGENS:
    if (base and (base in UNARYUGENS)):  # must be xxxb form
        need_ugen(manifest, "unaryugenb")
        return
    elif (name in UNARYUGENS):  # no base, so must be xxx form
        need_ugen(manifest, "unaryugen")
        return

    # now we know name is not covered by manifest yet, insert something:
    if name + "b" in manifest:
        manifest.remove(name + "b")
        manifest.append(name + "*")
    elif base and (base in manifest):
        manifest.remove(base)
        manifest.append(base + "*")
    else:
        manifest.append(name)


def main():
    global arco_app_path
    if len(sys.argv) != 5:
        print("Usage: " + PY + " makedspmakefile.py arco_path " + \
              "dspmanifest.txt dspmakefile dspsources.cmakeinclude")
        exit(-1)
    arco_path = sys.argv[1]
    manifest_name = sys.argv[2]
    arco_app_path = os.path.dirname(manifest_name)
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
    # remove trailing blanks
    manifest = [ugen.strip() for ugen in manifest]
    # remove duplicates and subsets
    manifest2 = []
    # remove duplicates; remove xxx if xxx* exists;
    # remove xxxb if xxx* exists; replace xxx and xxxb
    # with xxx* if both xxx and xxxb exist:
    for ugen in manifest:
        if ugen[-1] == "*":
            base = ugen[0 : -1]
            need_ugen(manifest2, base)
            need_ugen(manifest2, base + "b")
        else:
            need_ugen(manifest2, ugen)

    need_ugen(manifest2, "thru")
    need_ugen(manifest2, "zero")
    need_ugen(manifest2, "zerob")
    need_ugen(manifest2, "fader")
    need_ugen(manifest2, "sum")

    manifest = manifest2  # use the "cleaned up" and canonical manifest

    with open(makefile_name, "w") as outf:
        make_makefile(arco_path, manifest, outf)
    with open(inclfile_name, "w") as outf:
        make_inclfile(arco_path, manifest, outf)
    print("Merged the following files to form allugens.srp: ")
    for src in srp_srcs:
        print("   ", src)

main()
