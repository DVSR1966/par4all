from __future__ import with_statement # this is to work with python2.5

from validation import vworkspace
import validate_fusion

with vworkspace() as w:
    w.all_functions.validate_fusion()
