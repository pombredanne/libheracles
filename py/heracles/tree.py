import ctypes as c
from structs import struct_tree, struct_tree_p
from libs import libheracles

class Tree(object):
    def __init__(self, parent=None, first=None, heracles=None):
        assert(isinstance(parent, TreeNode) or parent is None)
        assert(isinstance(first, struct_tree_p) or first is None)
        if parent is not None:
            self.first = parent.htree.children
            self.heracles = parent.heracles
        else:
            assert(heracles is not None)
            self.first = first
            self.heracles = heracles
        self.parent = parent
        self._items = self._init_children()

    def insert(self, index, node):
        assert(isinstance(node, TreeNode))
        self._add_child(node=node, index=index)

    def append(self, node):
        assert(isinstance(node, TreeNode))
        self._add_child(node=node)

    def has_key(self, name):
        if isinstance(name, str):
            for item in self._items:
                if item.label == name:
                    return True
            return False
        elif isinstance(name, int):
            return name > 0 and name < len(self._items)

    def _update_index(self, _from, value):
        for item in self._items:
            try:
                label = int(item.label)
            except:
                continue
            if label > _from:
                item.label = str(label + value)

    # Assistance methods

    def _init_children(self):
        items = []
        child = self.first
        first = True
        while True:
            if not child:
                break
            if not first : old_child = new_child
            new_child = TreeNode(self.heracles, htree=child.contents, 
                    parent=self.parent, tree=self)
            if not first : 
                old_child.next = new_child
            else:
                first = False
            items.append(new_child)
            child = child.contents.next
            
        return items

    def _update_first(self, node):
        pointer = node.pointer
        if self.parent is not None:
            self.parent.htree.children = pointer
        self.first = pointer

    def _add_child(self, node=None, label=None, value=None, index=None):
        index = len(self._items) if index is None else index
        assert(abs(index) <= len(self._items))
        previous = self._items[index - 1] if index > 0 else None
        next = self._items[index] if index < len(self._items) - 1 else None
        if node is None:
            node = TreeNode(self.heracles, parent=self.parent, label=label, value=value,
                    next=next, tree=self)
        else:
            node.parent = self.parent
            if label is not None:
                node.label = label
            if value is not None:
                node.value = value
            if next is not None:
                node.next = next
            node.tree = self
        if previous is not None:
            previous.next = node
        else:
            self._update_first(node)
        self._items.insert(index, node)
        return node

    def insert_new(self, index, label=None, value=None):
        return self._add_child(index=index, label=label, value=value)

    def append_new(self, label=None, value=None):
        return self._add_child(label=label, value=value)

    def remove(self, child):
        previous = child.previous
        try:
            index = int(child.label)
            self._update_index(index, -1)
        except:
            pass
        if previous is None:
            self._update_first(child.next)
        else:
            if child.next:
                previous.next = child.next
            else:
                previous.next = None
        self._items.remove(child)
        child.next = None
        child.parent = None

    def serialize(self):
        res = []
        for item in self:
            res.append(item.serialize())
        return res

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

    def __delitem__(self, item):
        self.remove(item)

    def __repr__(self):
        return "<Heracles.Tree [%s]>" % ",".join(map(str, self._items))

    def __len__(self):
        return len(self._items)


class TreeNode(object):
    def __init__(self, heracles, htree=None, parent=0, label=None, 
            value=None, next=0, tree=None):
        assert(isinstance(htree, struct_tree) or htree is None)
        assert(isinstance(parent, TreeNode) or parent is None or parent == 0)
        assert(isinstance(next, TreeNode) or next is None or next == 0)
        assert(isinstance(tree, Tree) or tree is None)
        self.heracles = heracles
        self.tree = tree
        if htree is None:
            self.htree = struct_tree()
            self._self_contained = True
            self.htree.span = None
        else:
            self.htree = htree
            self._self_contained = False
        if label is not None:
            self.label = label
        if value is not None:
            self.value = value
        self.children = Tree(parent=self)
        if parent != 0:
            self.parent = parent
        if next != 0:
            self.next = next

    @property
    def previous(self):
        if self.tree is not None:
            for node in self.tree:
                if node.next == self:
                    return node
            return None
        raise Exception("You cannot find previous without tree")

    @property
    def parent(self):
        return self._parent

    @parent.setter
    def parent(self, value):
        assert(isinstance(value, TreeNode) or value is None)
        self._parent = value
        self.htree.parent = None if value is None else value.pointer

    @property
    def next(self):
        return self._next

    @next.setter
    def next(self, value):
        assert(isinstance(value, TreeNode) or value is None)
        self._next = value
        self.htree.next = None if value is None else value.pointer

    @property
    def label(self):
        return "" if self.htree.label is None else self.htree.label

    @label.setter
    def label(self, label):
        assert(isinstance(label, str) or label is None)
        if label == "":
            label = None
        self.htree.label = label if label is None else c.c_char_p(label)

    @property
    def value(self):
        return "" if self.htree.value is None else self.htree.value

    @value.setter
    def value(self, value):
        assert(isinstance(value, str) or value is None)
        if value == "":
            value = None
        self.htree.value = value if value is None else c.c_char_p(value)

    @property
    def root(self):
        return self.parent == self

    @property
    def pointer(self):
        return c.pointer(self.htree)

    def serialize(self):
        res = {}
        res['label'] = self.label
        res['value'] = self.value
        res['children'] = self.children.serialize()
        return res

    def __repr__(self):
        return "<Heracles.TreeNode label:'%s' value:'%s' children:%d>" % (str(self.label),
                str(self.value), len(self.children))

    def __del__(self):
        if not self._self_contained:
            libheracles.free_tree_node(self.pointer)


