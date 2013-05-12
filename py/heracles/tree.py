import ctypes as c
from structs import struct_tree
from libs import libheracles

class TreeChildren(object):
    def __init__(self, parent):
        self.parent = parent
        self.heracles = parent.heracles
        self._items = self._init_children()

    def insert(self, index, tree):
        assert(isinstance(tree, Tree))
        self._add_child(tree=tree, index=index)

    def has_key(self, name):
        if isinstance(name, str):
            for item in self._items:
                if item.label == name:
                    return True
            return False
        elif isinstance(name, int):
            return name > 0 and name < len(self._items)

    # Assistance methods

    def _init_children(self):
        items = []
        child = self.parent.htree.children
        while True:
            if not child:
                break
            new_child = Tree(self.heracles, child, self.parent)
            items.append(new_child)
            child = child.contents.next
        return items

    def _update_parent(self, child):
        self.parent.htree.children = child.pointer

    def _add_child(self, tree=None, label=None, value=None, index=None):
        index = len(self._items) if index is None else index
        assert(abs(index) <= len(self._items))
        previous = self._items[index - 1] if index > 0 else None
        next = self._items[index + 1] if index < len(self._items) - 2 else None
        if tree is None:
            tree = Tree(self.heracles, parent=self.parent, label=label, value=value,
                    next=next)
        else:
            tree.parent = self.parent
            if label is not None:
                tree.label = label
            if value is not None:
                tree.value = value
            if next is not None:
                tree.next = next
        if previous is not None:
            previous.next = tree
        else:
            self._update_parent(tree)
        self._items.insert(index, tree)
        return tree

    def insert_new(self, index, label=None, value=None):
        return self._add_child(index=index, label=label, value=value)

    def remove(self, child):
        previous = child.previous
        if previous is None:
            self._update_parent(child.next)
        else:
            if child.next:
                previous.next = child.next
            else:
                previous.next = None
        self._items.remove(child)
        child.next = None
        child.parent = None

    # Special methods

    def __iter__(self):
        for child in self._items:
            yield child

    def __getitem__(self, name):
        if isinstance(name, basestring):
            for item in self._items:
                if item.label == name:
                    return item
            raise KeyError("Unable to find child with name '%s'" % name)
        elif isinstance(name, int):
            return self._items[name]
        else:
            raise KeyError("Unsupported key type '%s'" % str(type(name)))

    def __setitem__(self, name, value):
        if self.has_key(name):
            item = self[name]
            item.value = value
        else:
            if isinstance(name, str):
                self._add_child(label=name, value=value)
            else:
                raise KeyError("Invalid identifier type '%s'" % str(type(value)))

    def __del__(self, item):
        self.remove(item)

    def __len__(self):
        return len(self._items)


class Tree(object):
    def __init__(self, heracles, htree=None, parent=None, label=None, 
            value=None, next=None):
        self.heracles = heracles
        if htree is None:
            self.htree = struct_tree()
            self._self_contained = True
        else:
            self.htree = htree
            self._self_contained = False
        self.children = TreeChildren(self)
        self.parent = parent
        self.label = label
        self.value = value
        self.next = next

    @property
    def previous(self):
        if not self.root:
            for tree in self.parent.children:
                if tree.next == self:
                    return tree

    @property
    def parent(self):
        return self._parent

    @parent.setter
    def parent(self, value):
        assert(isinstance(value, Tree) or value is None)
        self._parent = value
        self.htree.parent = None if value is None else value.pointer

    @property
    def next(self):
        return self._next

    @next.setter
    def next(self, value):
        assert(isinstance(value, Tree) or value is None)
        self._next = value
        self.htree.next = None if value is None else value.pointer

    @property
    def label(self):
        return self._label.value

    @label.setter
    def label(self, label):
        assert(isinstance(label, str) or label is None)
        self._label = label if label is None else c.c_char_p(label)
        self.htree.label = self._label

    @property
    def value(self):
        return self._value.value

    @value.setter
    def value(self, value):
        assert(isinstance(value, str) or value is None)
        self._value = value if value is None else c.c_char_p(value)
        self.htree.value = self._value

    @property
    def root(self):
        return self.parent == self

    @property
    def pointer(self):
        return c.pointer(self.htree)

    def __del__(self):
        if not self._self_contained:
            libheracles.free_tree_node(self.pointer)


