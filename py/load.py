import os
import ctypes as c
from structs import struct_heracles
from hexceptions import get_exception
from libs import libheracles, libpython


class Lenses(object):
    def __get__(self, obj, obj_type=None):
        self.heracles = obj
        self._handle = obj._handle
        return self

    def __iter__(self):
        module = self._handle.contents.module
        while True:
            if not module:
                break
            lens = Lens(self.heracles, module)
            if lens.lens:
                yield lens
            module = module.contents.next

    def __getitem__(self, name):
        for module in self:
            if module.name == name:
                return module
        raise KeyError("Unable to find module %s" % name)

class Heracles(object):
    lenses = Lenses()

    def __init__(self, loadpath=None, flags=0):
        if not isinstance(loadpath, basestring) and loadpath != None:
            raise TypeError("loadpath MUST be a string or None!")
        if not isinstance(flags, int):
            raise TypeError("flag MUST be a flag!")

        hera_init = libheracles.hera_init
        hera_init.restype = c.POINTER(struct_heracles)

        self._handle = hera_init(loadpath, flags)
        if not self._handle:
            raise RuntimeError("Unable to create Heracles object!")

        self.error = self._handle.contents.error.contents

    def catch_exception(self):
        exception = get_exception(self)
        if exception is not None:
            raise Exception

    def __repr__(self):
        return "<Heracles object>"

    def __del__(self):
        hera_close = libheracles.hera_close
        hera_close(self._handle)

class Lens(object):
    def __init__(self, heracles, module):
        self.heracles = heracles
        self.module = module.contents
        self.name = self.module.name
        transform = self.module.autoload
        self.lens = transform.contents.lens.contents if transform else None

    def get(self, text):
        return None

    def put(self, tree, text):
        return None
    
    def __repr__(self):
        return "<Heracles.Lens '%s'>" % self.name

class Tree(object):
    def __init__(self, heracles, tree):
        self.heracles = heracles
        self.tree = tree

modules = os.path.abspath("../lenses/")
print modules
h = Heracles(modules)
print h._handle.contents.nmodpath
print h.lenses["AptConf"]
del(h)

