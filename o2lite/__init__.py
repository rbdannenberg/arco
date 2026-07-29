"""Compatibility package for o2lite imports.

This re-exports the existing o2litepy implementation so code can import
from o2lite while still running directly from the repo via PYTHONPATH.
"""

from o2litepy import O2lite, O2blob, o2lite
