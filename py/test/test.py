import os
import ctypes as c
from unittest import TestCase
from heracles import Heracles, TreeNode, Tree, ListTree

def check_equal_addresses(p1, p2):
    return c.addressof(p1) == c.addressof(p2)

def check_equal_tree(test, tree1, tree2):
    for t1, t2 in zip(tree1, tree2):
        test.assertEqual(t1.label, t2.label)
        test.assertEqual(t1.value, t2.value)
        check_equal_tree(test, t1.children, t2.children)

def check_serialized_equal(test, data, tree):
    for s, o in zip(data, tree):
        test.assertEqual(s['label'], o.label)
        test.assertEqual(s['value'], o.value)
        check_serialized_equal(test, s['children'], o.children)

LENSES_PATH = os.path.abspath("../lenses/")

class HeraclesTest(TestCase):
    def setUp(self):
        self.h = Heracles(LENSES_PATH)
        self.text = file('./test/data/sources.list').read()
        self.lens = self.h.lenses['Aptsources']
        self.tree = self.lens.get(self.text)

    def test_list(self):
        self.assertTrue(isinstance(self.tree, ListTree))

    def test_get_and_put(self):
        gtext = self.lens.put(self.tree, self.text)
        ntree = self.lens.get(gtext)
        check_equal_tree(self, self.tree, ntree)

    def xtest_delete_row(self):
        self.tree.remove(self.tree['1'])
        text = self.lens.put(self.tree, "")
        ntree = self.lens.get(text)
        check_equal_tree(self, self.tree, ntree)

class FilterTest(TestCase):
    def setUp(self):
        self.h = Heracles(LENSES_PATH)
        self.text = file('./test/data/sources.list').read()
        self.lens = self.h.lenses['Aptsources']

    def test_filter(self):
        self.assertTrue(self.lens.check_path("/etc/apt/sources.list"))
        self.assertFalse(self.lens.check_path("/etc/sources.list"))

    def test_get_lens(self):
        l = self.h.get_lens_by_path("/etc/apt/sources.list")
        self.assertEqual(l.name, "Aptsources")
        l = self.h.get_lens_by_path("/etc/sudoers")
        self.assertEqual(l.name, "Sudoers")
        l = self.h.get_lens_by_path("/etc/xudoers")
        self.assertTrue(l is None)

class TreeBuildTest(TestCase):
    def setUp(self):
        self.h = Heracles()

    def test_build_list_tree(self):
        t = self.h.new_tree()
        t['1'] = "a"
        t['2'] = "b"
        t['3'] = "c"
        self.assertTrue(ListTree.check_tree(t.first))
        nt = Tree.build_from_raw_tree(heracles=self.h, first=t.first)
        self.assertTrue(isinstance(nt, ListTree))

    def test_build_list_tree_2(self):
        t = self.h.new_tree()
        t['3'] = "a"
        t['2'] = "b"
        t['1'] = "c"
        self.assertFalse(ListTree.check_tree(t.first))
        nt = Tree.build_from_raw_tree(heracles=self.h, first=t.first)
        self.assertFalse(isinstance(nt, ListTree))

    def test_build_list_tree_3(self):
        t = self.h.new_tree()
        t['1'] = "c"
        t['1'] = "x"
        t['2'] = "b"
        t['3'] = "a"
        self.assertFalse(ListTree.check_tree(t.first))
        nt = Tree.build_from_raw_tree(heracles=self.h, first=t.first)
        self.assertFalse(isinstance(nt, ListTree))

class ListTreeTest(TestCase):
    def setUp(self):
        self.h = Heracles()
        t = self.h.new_tree()
        t['1'] = "a"
        t['2'] = "b"
        t['#comment'] = "Tio"
        t['3'] = "c"
        self.t = Tree.build_from_raw_tree(heracles=self.h, first=t.first)

    def check_tree(self, node):
        self.assertEqual(node.parent, self.t.parent)
        self.assertEqual(node.tree, self.t)

    def check_node_first(self, node):
        self.check_tree(node)
        self.assertTrue(check_equal_addresses(self.t.first.contents, node.htree))

    def check_node_next(self, n1, n2):
        self.check_tree(n1)
        self.check_tree(n2)
        self.assertEqual(n1.next, n2)
        self.assertTrue(check_equal_addresses(n1.htree.next.contents, n2.htree))

    def check_index_correspondence(self):
        for i, item in enumerate(self.t):
            self.assertEqual(i+1, int(item.label))

    def test_list_tree(self):
        self.assertEqual(self.t[0].value, "a")
        self.assertEqual(self.t[1].value, "b")
        self.assertEqual(self.t[2].value, "c")
        self.t[0] = "x"
        self.assertEqual(self.t[0].value, "x")

    def test_insert_first(self):
        n = TreeNode(heracles=self.h,  value="x")
        self.t.insert(0, n)
        self.check_node_first(self.t[0])
        self.check_node_next(self.t[0], self.t[1])
        self.assertEqual(self.t[0].label, "1")
        self.check_index_correspondence()

    def test_insert_middle(self):
        n = TreeNode(heracles=self.h,  value="x")
        self.t.insert(1, n)
        self.check_node_next(self.t[0], self.t[1])
        self.check_node_next(self.t[1], self.t[2])
        self.assertEqual(self.t[1].label, "2")
        self.check_index_correspondence()

    def test_insert_last(self):
        n = TreeNode(heracles=self.h,  value="x")
        self.t.insert(2, n)
        self.assertEqual(self.t[2].label, "3")
        self.check_index_correspondence()

    def test_append(self):
        n = TreeNode(heracles=self.h,  value="x")
        last = self.t._items[-1]
        self.t.append(n)
        self.assertEqual(self.t[3].label, "4")
        self.assertEqual(self.t[-1], n)
        self.check_node_next(last, n)
        self.check_index_correspondence()

    def test_insert_first_str(self):
        self.t.insert(0, "x")
        self.check_node_first(self.t[0])
        self.check_node_next(self.t[0], self.t[1])
        self.assertEqual(self.t[0].label, "1")
        self.check_index_correspondence()

    def test_insert_middle_str(self):
        self.t.insert(1, "x")
        self.check_node_next(self.t[0], self.t[1])
        self.check_node_next(self.t[1], self.t[2])
        self.assertEqual(self.t[1].label, "2")
        self.check_index_correspondence()

    def test_insert_last_str(self):
        self.t.insert(2, "x")
        self.assertEqual(self.t[2].label, "3")
        self.check_index_correspondence()

    def test_append_str(self):
        last = self.t._items[-1]
        self.t.append("x")
        self.assertEqual(self.t[3].label, "4")
        self.assertEqual(self.t[-1].value, "x")
        self.assertEqual(last.next.value, "x")
        self.check_index_correspondence()

    def test_remove_first(self):
        first = self.t._items[1]
        node = self.t[0]
        self.t.remove(node)
        self.check_node_first(first)
        self.check_node_next(node, first)
        self.check_index_correspondence()

    def test_remove_middle(self):
        node = self.t[1]
        previous = node.previous
        next = node.next
        self.t.remove(node)
        self.check_node_next(previous, next)
        self.check_index_correspondence()

    def test_remove_last(self):
        last = self.t[-1]
        previous = last.previous
        self.t.remove(last)
        self.assertTrue(previous.next == None)

class TreeNodeLabelTest(TestCase):
    def setUp(self):
        self.h = Heracles()
        t = self.h.new_tree()
        t['a'] = "a"
        t['#comment'] = "comment"
        t['b'] = "b1"
        t['#comment'] = "comment"
        t['b'] = "b2"
        t['c'] = "c1"
        t['#comment'] = "comment"
        t['b'] = "b3"
        t['#comment'] = "comment"
        t['c'] = "c2"
        self.t = Tree.build_from_raw_tree(heracles=self.h, first=t.first)

    def test_label(self):
        self.assertEqual(self.t['a'].value , "a")
        self.assertEqual(len(self.t['b']) , 3)
        self.assertEqual(self.t['b'][0].value , "b1")
        self.assertEqual(self.t['b'][1].value , "b2")
        self.assertEqual(self.t['b'][2].value , "b3")
        self.assertEqual(self.t['b'][-1].value , "b3")
        self.assertEqual(self.t['b'][-2].value , "b2")
        self.assertEqual(self.t['b'][-3].value , "b1")
        self.assertEqual(len(self.t['c']) , 2)

    def test_insert_first(self):
        bs = self.t['b']
        bs.insert(0, "b0")
        self.assertEqual(bs[0].value, "b0")

    def test_insert_first2(self):
        n = TreeNode(heracles=self.h,  value="b0")
        bs = self.t['b']
        bs.insert(0, n)
        self.assertEqual(bs[0].value, "b0")

    def test_insert_middle(self):
        bs = self.t['b']
        bs.insert(1, "b0")
        self.assertEqual(bs[1].value, "b0")

    def test_insert_middle2(self):
        n = TreeNode(heracles=self.h,  value="b0")
        bs = self.t['b']
        bs.insert(1, n)
        self.assertEqual(bs[1].value, "b0")

    def test_append(self):
        bs = self.t['b']
        bs.append("b0")
        self.assertEqual(bs[3].value, "b0")

    def test_append_str(self):
        n = TreeNode(heracles=self.h,  value="b0")
        bs = self.t['b']
        bs.append(n)
        self.assertEqual(bs[3].value, "b0")

class TreeTest(TestCase):
    def setUp(self):
        self.h = Heracles()
        self.t = self.h.new_tree()

    def load_tree(self):
        self.t['test1'] = "1" 
        p = self.t[0]
        self.t['test2'] = "2"
        self.t['test3'] = "3"
        p.children['test1.1'] = "1"
        p.children['test1.2'] = "2"
        p.children['test1.3'] = "3"

    def test_new_tree(self):
        self.assertTrue(len(self.t)==0)

    def test_children(self):
        self.assertTrue(len(self.t) == 0)
        self.t['test'] = "1" 
        self.assertTrue(len(self.t) == 1)
        self.assertTrue(self.t['test'].value == "1")
        self.assertTrue(self.t[0].value == "1")
        n = self.t['test'] 
        n.value = "2"
        self.assertTrue(len(self.t) == 1)
        self.assertTrue(self.t['test'].value == "2")
        self.assertTrue(self.t[0].value == "2")
        self.t['test2'] = "3"
        self.assertTrue(len(self.t) == 2)
        self.assertTrue(self.t['test2'].value == "3")
        self.assertTrue(self.t[1].value == "3")
        t = self.t[0].children
        t['test'] = "1" 
        self.assertTrue(len(t) == 1)
        self.assertTrue(t['test'].value == "1")
        self.assertTrue(t[0].value == "1")
        n = t['test'] 
        n.value = "2"
        self.assertTrue(len(t) == 1)
        self.assertTrue(t['test'].value == "2")
        self.assertTrue(t[0].value == "2")
        t['test2'] = "3"
        self.assertTrue(len(t) == 2)
        self.assertTrue(t['test2'].value == "3")
        self.assertTrue(t[1].value == "3")

    def test_insert_new_middle(self):
        self.load_tree()
        self.t.insert_new(1, label="test_insert", value="0")
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[1].value == "0")
        self.assertTrue(self.t[0].next == self.t[1])
        self.assertTrue(self.t[1].next == self.t[2])
        self.assertTrue(len(self.t) == 4)
        t = self.t[0].children
        t.insert_new(1, label="test_insert", value="0")
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[1].value == "0")
        self.assertTrue(t[0].next == t[1])
        self.assertTrue(check_equal_addresses(t[0].htree.next.contents,
            t[1].htree))
        self.assertTrue(t[1].next == t[2])
        self.assertTrue(check_equal_addresses(t[1].htree.next.contents,
            t[2].htree))
        self.assertTrue(len(t) == 4)

    def test_insert_new_first(self):
        self.load_tree()
        self.t.insert_new(0, label="test_insert", value="0")
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[0].value == "0")
        self.assertTrue(len(self.t) == 4)
        self.assertTrue(self.t[0].next == self.t[1])
        self.assertTrue(check_equal_addresses(self.t[0].htree.next.contents,
            self.t[1].htree))
        self.assertTrue(check_equal_addresses(self.t.first.contents, 
                self.t[0].htree))
        t = self.t[1].children
        t.insert_new(0, label="test_insert", value="0")
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[0].value == "0")
        self.assertTrue(len(t) == 4)
        self.assertTrue(t[0].next == t[1])
        self.assertTrue(check_equal_addresses(t[0].htree.next.contents,
            t[1].htree))
        self.assertTrue(check_equal_addresses(t.first.contents, 
                t[0].htree))
    def test_insert_new_last(self):
        self.load_tree()
        self.t.insert_new(3, label="test_insert", value="0")
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[3].value == "0")
        self.assertTrue(len(self.t) == 4)
        self.assertTrue(self.t[2].next == self.t[3])
        self.assertTrue(check_equal_addresses(self.t[2].htree.next.contents,
            self.t[3].htree))
        t = self.t[0].children
        t.insert_new(3, label="test_insert", value="0")
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[3].value == "0")
        self.assertTrue(len(t) == 4)
        self.assertTrue(t[2].next == t[3])
        self.assertTrue(check_equal_addresses(t[2].htree.next.contents,
            t[3].htree))

    def test_append_new(self):
        self.load_tree()
        self.t.append_new(label="test_insert", value="0")
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[3].value == "0")
        self.assertTrue(len(self.t) == 4)
        self.assertTrue(self.t[2].next == self.t[3])
        self.assertTrue(check_equal_addresses(self.t[2].htree.next.contents,
            self.t[3].htree))
        t = self.t[0].children
        t.append_new(label="test_insert", value="0")
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[3].value == "0")
        self.assertTrue(len(t) == 4)
        self.assertTrue(t[2].next == t[3])
        self.assertTrue(check_equal_addresses(t[2].htree.next.contents,
            t[3].htree))

    def test_insert_middle(self):
        self.load_tree()
        t = TreeNode(heracles=self.h, label="test_insert", value="0")
        self.t.insert(1, t)
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[1].value == "0")
        self.assertTrue(self.t[0].next == self.t[1])
        self.assertTrue(check_equal_addresses(self.t[0].htree.next.contents,
            self.t[1].htree))
        self.assertTrue(self.t[1].next == self.t[2])
        self.assertTrue(check_equal_addresses(self.t[1].htree.next.contents,
            self.t[2].htree))
        self.assertTrue(len(self.t) == 4)

    def test_insert_middle_2(self):
        self.load_tree()
        n0 = self.t[0]
        t = n0.children
        n = TreeNode(heracles=self.h, label="test_insert", value="0")
        t.insert(1, n)
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[1].value == "0")
        self.assertTrue(t[0].next == t[1])
        self.assertTrue(check_equal_addresses(t[0].htree.next.contents,
            t[1].htree))
        self.assertTrue(t[1].next == t[2])
        self.assertTrue(check_equal_addresses(t[1].htree.next.contents,
            t[2].htree))
        self.assertTrue(len(t) == 4)

    def test_insert_first(self):
        self.load_tree()
        t = TreeNode(heracles=self.h, label="test_insert", value="0")
        self.t.insert(0, t)
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[0].value == "0")
        self.assertTrue(len(self.t) == 4)
        self.assertTrue(self.t[0].next == self.t[1])
        self.assertTrue(check_equal_addresses(self.t[0].htree.next.contents,
            self.t[1].htree))
        self.assertTrue(check_equal_addresses(self.t.first.contents, 
                self.t[0].htree))

    def test_insert_first_2(self):
        self.load_tree()
        n0 = self.t[0]
        t = n0.children
        n = TreeNode(heracles=self.h, label="test_insert", value="0")
        t.insert(0, n)
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[0].value == "0")
        self.assertTrue(len(t) == 4)
        self.assertTrue(t[0].next == t[1])
        self.assertTrue(check_equal_addresses(t[0].htree.next.contents,
            t[1].htree))
        self.assertTrue(check_equal_addresses(t.first.contents, t[0].htree))
        self.assertTrue(check_equal_addresses(t.parent.htree.children.contents, 
                t[0].htree))
        self.assertTrue(check_equal_addresses(t[0].parent.htree.children.contents, 
                t[0].htree))

    def test_insert_last(self):
        self.load_tree()
        t = TreeNode(heracles=self.h, label="test_insert", value="0")
        self.t.insert(3, t)
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[3].value == "0")
        self.assertTrue(len(self.t) == 4)
        self.assertTrue(self.t[2].next == self.t[3])
        self.assertTrue(check_equal_addresses(self.t[2].htree.next.contents,
            self.t[3].htree))

    def test_insert_last_2(self):
        self.load_tree()
        n0 = self.t[0]
        t = n0.children
        n = TreeNode(heracles=self.h, label="test_insert", value="0")
        t.insert(3, n)
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[3].value == "0")
        self.assertTrue(len(t) == 4)
        self.assertTrue(t[2].next == t[3])
        self.assertTrue(t[3].parent == n0)
        self.assertTrue(check_equal_addresses(t[3].htree.parent.contents,
            n0.htree))
        self.assertTrue(check_equal_addresses(t[2].htree.next.contents,
            t[3].htree))

    def test_append(self):
        self.load_tree()
        t = TreeNode(heracles=self.h, label="test_insert", value="0")
        self.t.append(t)
        self.assertTrue(self.t["test_insert"].value == "0")
        self.assertTrue(self.t[3].value == "0")
        self.assertTrue(len(self.t) == 4)
        self.assertTrue(self.t[2].next == self.t[3])
        self.assertTrue(check_equal_addresses(self.t[2].htree.next.contents,
            self.t[3].htree))

    def test_append_2(self):
        self.load_tree()
        n0 = self.t[0]
        t = n0.children
        n = TreeNode(heracles=self.h, label="test_insert", value="0")
        t.append(n)
        self.assertTrue(t["test_insert"].value == "0")
        self.assertTrue(t[3].value == "0")
        self.assertTrue(len(t) == 4)
        self.assertTrue(t[2].next == t[3])
        self.assertTrue(t[3].parent == n0)
        self.assertTrue(check_equal_addresses(t[3].htree.parent.contents,
            n0.htree))
        self.assertTrue(check_equal_addresses(t[2].htree.next.contents,
            t[3].htree))

    def test_remove_first(self):
        self.load_tree()
        n = self.t[0]
        self.t.remove(n)
        self.assertTrue(n.parent is None)
        self.assertTrue(n.next is None)
        n2 = self.t[0]
        self.assertFalse(n == n2)
        self.assertTrue(self.t.first.contents.label == n2.htree.label)
        self.assertTrue(check_equal_addresses(self.t.first.contents, 
                n2.htree))

    def test_remove_first_2(self):
        self.load_tree()
        t = self.t[0].children
        n = t[0]
        t.remove(n)
        self.assertTrue(n.parent is None)
        self.assertTrue(n.next is None)
        n2 = t[0]
        self.assertFalse(n == n2)
        self.assertTrue(t.first.contents.label == n2.htree.label)
        self.assertTrue(check_equal_addresses(t.first.contents, 
                n2.htree))
        self.assertTrue(check_equal_addresses(n2.parent.htree.children.contents,
                n2.htree))

    def test_remove_middle(self):
        self.load_tree()
        n = self.t[1]
        self.t.remove(n)
        self.assertTrue(n.parent is None)
        self.assertTrue(n.next is None)
        n1 = self.t[0]
        n2 = self.t[1]
        self.assertFalse(n == n2)
        self.assertTrue(check_equal_addresses(n1.htree.next.contents, n2.htree))
        t = self.t[0].children
        n = t[1]
        t.remove(n)
        self.assertTrue(n.parent is None)
        self.assertTrue(n.next is None)
        n1 = t[0]
        n2 = t[1]
        self.assertFalse(n == n2)
        self.assertTrue(check_equal_addresses(n1.htree.next.contents, n2.htree))

    def test_remove_last(self):
        self.load_tree()
        n = self.t[2]
        self.t.remove(n)
        self.assertTrue(n.parent is None)
        self.assertTrue(n.next is None)
        n2 = self.t[1]
        self.assertTrue(n2.next is None)
        t = self.t[0].children
        n = t[2]
        t.remove(n)
        self.assertTrue(n.parent is None)
        self.assertTrue(n.next is None)
        n2 = t[1]
        self.assertTrue(n2.next is None)

    def test_serialize(self):
        self.load_tree()
        data = self.t.serialize()
        check_serialized_equal(self, data, self.t)

class ParsedTreeTest(TestCase):
    def setUp(self):
        self.h = Heracles(LENSES_PATH)
        self.text = file('./test/data/sources.list').read()
        self.lens = self.h.lenses['Aptsources']
        self.tree = self.lens.get(self.text)

    def check_tree(self, tree):
        struct = tree.first
        for node in tree:
            self.assertEqual(node.tree, tree)
            check_equal_addresses(struct, c.pointer(node.htree))
            if node.parent is not None:
                check_equal_addresses(struct.contents.parent, c.pointer(node.parent.htree))
            self.check_tree(node.children)
            struct = struct.contents.next

    def test_structure(self):
        self.check_tree(self.tree)

if __name__ == "__main__":
    import unittest
    unittest.main()

