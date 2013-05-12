import ctypes as c
from heracles.structs import struct_heracles, struct_tree, struct_lns_error
from heracles.exceptions import get_exception
from heracles.libs import libheracles
from heracles.tree import Tree

class HeraclesLenses(object):
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
    lenses = HeraclesLenses()

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

    def new_tree(self):
        tree = Tree(self)
        tree.parent = tree
        return tree

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
        self.lens = transform.contents.lens if transform else None

    def _catch_error(self, err):
        if err:
            raise Exception(err.contents.message)

    def get(self, text):
        hera_get = libheracles._hera_get
        hera_get.restype = c.POINTER(struct_tree)
        error = c.POINTER(struct_lns_error)()
        tree = hera_get(self.lens, c.c_char_p(text), error)
        self._catch_error(error)
        return Tree(self.heracles, htree=tree)

    def put(self, tree, text):
        tree = tree.pointer
        hera_put = libheracles._hera_put
        hera_put.restype = c.c_char_p
        error = c.POINTER(struct_lns_error)()
        result = hera_put(self.lens, tree, c.c_char_p(text), error)
        self._catch_error(error)
        return result
    
    def __repr__(self):
        return "<Heracles.Lens '%s'>" % self.name

