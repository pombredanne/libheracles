import os
import ctypes as c
from unittest import TestCase
from heracles import Heracles, Tree, TreeNode

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

    def test_get_and_put(self):
        gtext = self.lens.put(self.tree, self.text)
        ntree = self.lens.get(gtext)
        check_equal_tree(self, self.tree, ntree)

    def test_delete_row(self):
        self.tree.remove(self.tree['1'])
        text = self.lens.put(self.tree, "")
        ntree = self.lens.get(text)
        check_equal_tree(self, self.tree, ntree)

class TreeTest(TestCase):
    def setUp(self):
        self.h = Heracles(LENSES_PATH)
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
        self.t['test'] = "2"
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
        t['test'] = "2"
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

