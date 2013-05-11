import os
from sys import version_info as _pyver
import string
import glob
import ctypes
import ctypes.util

def _findLib(name):
    ld_path = os.getenv("LD_LIBRARY_PATH")
    for p in string.split(ld_path, ":"):
        if (p[:-1] != "/"):
            p += "/"
        path = "%slib%s*.so" % (p,name)
        files = glob.glob(path)
        files.sort()
        if (len(files) > 0):
            return os.path.basename(files[0])
    return None

def _dlopen(*args):
    """Search for one of the libraries given as arguments and load it.
    Returns the library.
    """
    libs = [l for l in [ ctypes.util.find_library(a) for a in args ] if l]
    if (len(libs) == 0):
        libs = libs = [l for l in [ _findLib(a) for a in args ] if l]
    lib  = reduce(lambda x, y: x or ctypes.cdll.LoadLibrary(y), libs, None)
    if not lib:
        raise ImportError("Unable to import lib%s!" % args[0])
    return lib

libpython = _dlopen(*["python" + _v % _pyver[:2]
                       for _v in ("%d.%d", "%d%d")])
libpython.PyFile_AsFile.restype = ctypes.c_void_p
libheracles = _dlopen("heracles")
