import ctypes as c

# c.Structure predefinition

class struct_heracles(c.Structure):
    pass

class struct_string(c.Structure):
    pass

class struct_info(c.Structure):
    pass

class struct_error(c.Structure):
    pass

class struct_span(c.Structure):
    pass

class struct_tree(c.Structure):
    pass

struct_tree_p = c.POINTER(struct_tree)

class struct_module(c.Structure):
    pass

class struct_lens(c.Structure):
    pass

class struct_filter(c.Structure):
    pass

class struct_lns_error(c.Structure):
    pass

# Not defined fields

class struct_pathx_symtab(c.Structure):
    pass

class struct_transform(c.Structure):
    pass

class struct_binding(c.Structure):
    pass

# Load fields

struct_string._fields_ = [('ref', c.c_uint),
                ('str', c.c_char_p)]

struct_info._fields_ = [('error', c.POINTER(struct_error)),
                ('filename', c.POINTER(struct_string)),
                ('first_line', c.c_ushort),
                ('first_column', c.c_ushort),
                ('last_line', c.c_ushort),
                ('last_column', c.c_ushort),
                ('ref', c.c_uint),
                ('flags', c.c_int)]

struct_error._fields_ = [('code', c.c_int),
                ('minor', c.c_int),
                ('details', c.c_char_p),
                ('info', c.POINTER(struct_info)),
                ('hera', c.POINTER(struct_heracles))]

struct_heracles._fields_ = [('origin', struct_tree_p),
                ('root', c.c_char_p),
                ('flags', c.c_uint),
                ('module', c.POINTER(struct_module)),
                ('nmodpath', c.c_size_t),
                ('modpathz', c.c_char_p),
                ('symtab', c.POINTER(struct_pathx_symtab)),
                ('error', c.POINTER(struct_error)),
                ('api_entries', c.c_uint),
                ]

struct_tree._fields_ = [('next', struct_tree_p),
                ('parent', struct_tree_p),
                ('label', c.c_char_p),
                ('children', struct_tree_p),
                ('value', c.c_char_p),
                ('dirty', c.c_int),
                ('span', c.POINTER(struct_span))]

struct_module._fields_ = [('ref', c.c_uint),
                ('next', c.POINTER(struct_module)),
                ('autoload', c.POINTER(struct_transform)),
                ('name', c.c_char_p),
                ('bindings', c.POINTER(struct_binding))]

struct_transform._fields_ = [('ref', c.c_uint),
                ('lens', c.POINTER(struct_lens)),
                ('filter', c.POINTER(struct_filter))]

struct_lns_error._fields_ = [('lens', c.POINTER(struct_lens)),
                ('pos', c.c_int),
                ('path', c.c_char_p),
                ('message', c.c_char_p)]

struct_filter._fields_ = [('ref', c.c_uint),
                ('next', c.POINTER(struct_filter)),
                ('glob', c.POINTER(struct_string)),
                ('include', c.c_uint)]

