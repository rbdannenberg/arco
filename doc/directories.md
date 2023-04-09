# What's Where in the Repository

**Roger B. Dannenberg**

This repo has the following directories:

- `apps` -- applications where you can build customized versions of Arco servers with or without Serpent.
- `arco` -- the Arco main synthesis engine code
- `preproc` -- preprocessors written in Python to translate Arco's .ugen files, which add some annotations to FAUST source, into to FAUST .dsp files. Also runs FAUST, manipulates the output to create Arco unit generators.
- `serpent` -- shared code for building Serpent interpreter combined with Arco synthesis engine running in separate thread with an O2 shared memory interface.
- `server` -- builds a command-line application acting as an O2 server with
Arco synthesis engine running in a separater thread with an O2 shared memory interface. This should really just contain the shared code, and actual server instances should be built in the apps directory so that they can have custom code.
- `ugens` -- Arco unit generators implemented in .ugen files and translated by a combination of FAUST and preprocessors to Arco unit generators.

