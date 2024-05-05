Musical Onset Database And Library (Modal)
==========================================

Overview
--------

Modal is a cross-platform library for musical onset detection, written in C++ and Python.
It is provided here under the terms of the GNU General Public License.
It consists of code for several different types of Onset Detection Function (ODF), code for
real-time and non-real-time onset detection, and a means for comparing the performance of
different ODFs.

All ODFs are implemented in both C++ and Python. The code for Onset detection and ODF comparison
is currently only available in Python.

Modal also includes a database of musical samples with hand-annotated onset locations for ODF 
evaluation purposes. The database can be found in the downloads section, it is not included 
in the repository. 
The database is a hierarchical database, stored in the [HDF5](http://www.hdfgroup.org/HDF5/) format.


**Note:** The code needed to replicate the results from the paper
"Real-time Detection of Musical Onsets with Linear Prediction and Sinusoidal Modelling", published
in the EURASIP Journal on Advances in Signal Processing (2011) can now be found here:
http://github.com/johnglover/eurasip2011


Contributing
------------

Send any comments, queries, suggestions or bug reports to john dot c dot glover at nuim dot ie.


Contents
--------

.. toctree::
   :maxdepth: 2

   installation
   use


.. Indices and tables
.. ==================

.. * :ref:`genindex`
.. * :ref:`modindex`
.. * :ref:`search`
