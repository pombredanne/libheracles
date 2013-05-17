import ctypes as c
from heracles.structs import struct_tree, struct_tree_p
from heracles.libs import libheracles
from heracles.exceptions import HeraclesTreeError, HeraclesTreeLabelError
from heracles.util import check_int

class RawTree(object):
    def __init__(self, raw_tree_p):
        self.raw_tree_p = raw_tree_p

    def __iter__(self):
        node_p = self.raw_tree_p
        while True:
            if not node_p:
                break
            yield node_p.contents
            node_p = node_p.contents.next

class LabelNodeList(object):
    @classmethod
    def build(cls, tree, label):
        i = 0
        for item in tree._items:
            if item.label == label:
                i += 1
        if i == 0:
            raise KeyError("Unable to find item '%s'" % label)
        else:
            return cls(tree, label)

    # Masked methods used to access node properties in case there
    # is just one node to simplify syntax

    def _test_single_node_label(self):
        if len(self) != 1:
            raise HeraclesTreeLabelError("Single node label methods only accesible with one node")

    @property
    def value(self):
        self._test_single_node_label()
        return self[0].value

    @value.setter
    def value(self, value):
        self._test_single_node_label()
        self[0].value = value

    # Common list methods

    def __init__(self, tree, label):
        self.tree = tree
        self.label = label

    def remove(self, item):
        if not item in self:
            raise ValueError("Node not in list")
        self.tree.remove(item)

    def insert(self, index, item):
        assert(isinstance(index, int))
        assert(isinstance(item, str) or isinstance(item, TreeNode))
        if isinstance(item, str):
            item = TreeNode(self.tree.heracles, value=item, label=self.label, tree=self.tree)
        else:
            item.label = self.label
        l = len(self)
        if index < 0:
            index = l - index
        if index < 0:
            index = 0
        if index >= l:
            self.append(item)
        else:
            t_item = self[index]
            t_index = t_item.index
            self.tree.insert(t_index, item)

    def append(self, item):
        assert(isinstance(item, str) or isinstance(item, TreeNode))
        if isinstance(item, str):
            item = TreeNode(self.tree.heracles, value=item, label=self.label, tree=self.tree)
        else:
            item.label = self.label
        self.tree.append(item)

    def __iter__(self):
        for item in self.tree._items:
            if item.label == self.label:
                yield item

    def __getitem__(self, index):
        assert(isinstance(index, int))
        l = len(self)
        if index < 0:
            index = l + index
        if not 0 <= index < l:
            raise IndexError("Index out of range")
        for i, item in enumerate(self):
            if i == index:
                return item

    def __setitem__(self, index, value):
        assert(isinstance(value, str) or isinstance(value, TreeNode))
        item = self[index]
        if isinstance(value, str):
            item.value = value
        elif isinstance(value, TreeNode):
            value.label = self.label
            tree_index = item.index
            self.tree.remove(item)
            self.tree.insert(tree_index, item)

    def __delitem__(self, item):
        self.remove(item)

    def __contains__(self, item):
        for x in self:
            if x == item:
                return True
        return False

    def __len__(self):
        i = 0
        for item in self:
            i += 1
        return i

class Tree(object):
    tree_classes = []
    node_classes = []

    @classmethod
    def build_from_raw_tree(cls, first=None, heracles=None, parent=None):
        if first is not None:
            raw_tree_p = first
        elif parent is not None:
            raw_tree_p = parent.htree.children
        else:
            raise HeraclesTreeError('Must provide first node or parent to build tree')
        for tc in cls.tree_classes:
            assert(issubclass(tc, cls))
            if tc.check_tree(raw_tree_p):
                return tc(heracles=heracles, first=first, parent=parent)
        return cls(heracles=heracles, first=first, parent=parent)
        
    @classmethod
    def check_tree(cls, raw_tree_p):
        return True

    @classmethod
    def get_node_class(cls, raw_node):
        for n_c in cls.node_classes:
            if n_c.check_node(raw_node):
                return n_c
        return TreeNode

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

    def insert_new(self, index, label=None, value=None):
        return self._add_child(index=index, label=label, value=value)

    def append_new(self, label=None, value=None):
        return self._add_child(label=label, value=value)

    def serialize(self):
        res = []
        for item in self:
            res.append(item.serialize())
        return res

    # Assistance methods

    def _init_children(self):
        items = []
        child = self.first
        first = True
        new_child = None
        while True:
            if not child:
                break
            if not first : old_child = new_child
            node_class = self.get_node_class(child.contents)
            new_child = node_class(self.heracles, htree=child.contents, 
                    parent=self.parent, tree=self)
            if not first : 
                old_child.next = new_child
            else:
                first = False
            items.append(new_child)
            child = child.contents.next
        if new_child is not None:
            new_child.next = None
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

    # Special methods

    def __iter__(self):
        for child in self._items:
            yield child

    def __getitem__(self, name):
        if isinstance(name, basestring):
            return LabelNodeList.build(self, name)
        elif isinstance(name, int):
            return self._items[name]
        else:
            raise KeyError("Unsupported key type '%s'" % str(type(name)))

    def __setitem__(self, name, value):
        assert(isinstance(value, str) or isinstance(value, TreeNode))
        assert(isinstance(name, str) or isinstance(name, int))
        if isinstance(name, str):
            if isinstance(value, str):
                self._add_child(label=name, value=value)
            elif isinstance(value, TreeNode):
                value.label = name
                self.append(value)
        elif isinstance(name, int):
            if isinstance(value, str):
                self[name].value = value
            elif isinstance(value, TreeNode):
                l = len(self)
                if name < 0:
                    name = l + name
                if name < 0:
                    name = 0
                if name < l:
                    oitem = self[name]
                    self.remove(oitem)
                    self.insert(name, value)
                else:
                    self.append(value)

    def __delitem__(self, item):
        self.remove(item)

    def __repr__(self):
        return "%s [%s]>" % (self.__class__.__name__,",".join(map(str, self._items)))

    def __len__(self):
        return len(self._items)

class ListTree(Tree):
    @classmethod
    def check_tree(cls, raw_tree_p):
        i = 1
        pointer = raw_tree_p
        failed = False
        while True:
            if not pointer:
                break
            r_node = pointer.contents
            try:
                v = int(r_node.label)
                if v == i:
                    i += 1
                else:
                    failed = True
                    break
            except:
                pass
            finally:
                pointer = r_node.next
        return i > 1 and not failed

    def insert(self, index, value):
        assert(isinstance(value, TreeNode) or isinstance(value, str))
        assert(isinstance(index, int))
        if isinstance(value, str):
            node = TreeNode(self.heracles, tree=self, value=value, label=str(index + 1))
        else:
            node = value
        if index < 0:
            index = len(self) - 1
        next = self[index]
        if index > 0:
            t_index = self[index - 1].index + 1
        else:
            t_index = 0
            self.first = c.pointer(node.htree)
        previous = next.previous
        if previous is not None:
            previous.next = node
        node.next = next
        node.parent = self.parent
        node.tree = self
        label = index + 1
        node.label = str(label)
        self._update_index(label, 1)
        self._items.insert(t_index, node)

    def append(self, value):
        assert(isinstance(value, TreeNode) or isinstance(value, str))
        if isinstance(value, str):
            node = TreeNode(self.heracles, tree=self, value=value)
        else:
            node = value
        last = self[-1]
        last.next = node
        node.parent = self.parent
        node.tree = self
        node.label = str(len(self) + 1)
        self._items.append(node)

    def remove(self, node):
        assert(isinstance(node, TreeNode))
        next = node.next
        if node.index == 0:
            self.first = c.pointer(next.htree)
        else:
            previous = node.previous
            previous.next = next
        self._items.remove(node)
        self._update_index(int(node.label) + 1, -1)


    def has_key(self, name):
        if isinstance(name, str):
            for item in self._items:
                if item.label == name:
                    return True
            return False
        elif isinstance(name, int):
            return name > 0 and name < len(self._items)

    def _update_index(self, _from, value):
        for item in self:
            label = int(item.label)
            if label >= _from:
                item.label = str(label + value)

    def __iter__(self):
        for child in self._items:
            if check_int(child.label): 
                yield child

    def __getitem__(self, name):
        assert(isinstance(name, int))
        l = len(self)
        if name < 0:
            name = l + name
        if not 0 <= name < l:
            raise IndexError("List index out of range")
        for i, item in enumerate(self):
            if i == name:
                return item
        raise KeyError("Index out of range")

    def __setitem__(self, name, value):
        assert(isinstance(value, str))
        node = self[name]
        node.value = value

    def __len__(self):
        for i, item in enumerate(self):
            pass
        return i+1

# Update Tree object to detect ListTree objects
Tree.tree_classes = [ListTree]

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
            for node in self.tree._items:
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
    def index(self):
        if self.tree is not None:
            for i, node in enumerate(self.tree._items):
                if node == self:
                    return i

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


