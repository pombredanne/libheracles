import os
import ctypes as c
from unittest import TestCase
from heracles import Heracles, Tree

def check_equal_addresses(p1, p2):
    return c.addressof(p1) == c.addressof(p2)

LENSES_PATH = os.path.abspath("../../lenses/")

class HeraclesTest(TestCase):
    def setUp(self):
        file('/etc/apt/sources.list').read()

class TreeTest(TestCase):
    def setUp(self):
        self.h = Heracles(LENSES_PATH)
        self.t = self.h.new_tree()

    def load_tree(self):
        self.t.children['test1'] = "1" 
        p = self.t.children[0]
        self.t.children['test2'] = "2"
        self.t.children['test3'] = "3"
        p.children['test1.1'] = "1"
        p.children['test1.2'] = "2"
        p.children['test1.3'] = "3"

    def test_new_tree(self):
        self.assertTrue(self.t.root)

    def test_children(self):
        self.assertTrue(len(self.t.children) == 0)
        self.t.children['test'] = "1" 
        self.assertTrue(len(self.t.children) == 1)
        self.assertTrue(self.t.children['test'].value == "1")
        self.assertTrue(self.t.children[0].value == "1")
        self.t.children['test'] = "2"
        self.assertTrue(len(self.t.children) == 1)
        self.assertTrue(self.t.children['test'].value == "2")
        self.assertTrue(self.t.children[0].value == "2")
        self.t.children['test2'] = "3"
        self.assertTrue(len(self.t.children) == 2)
        self.assertTrue(self.t.children['test2'].value == "3")
        self.assertTrue(self.t.children[1].value == "3")

    def test_insert(self):
        self.load_tree()
        self.t.children.insert_new(1, label="test_insert", value="0")
        self.assertTrue(self.t.children["test_insert"].value == "0")
        self.assertTrue(self.t.children[1].value == "0")
        self.assertTrue(len(self.t.children) == 4)

    def test_remove_first(self):
        self.load_tree()
        t = self.t.children[0]
        self.t.children.remove(t)
        self.assertTrue(t.parent is None)
        self.assertTrue(t.next is None)
        t2 = self.t.children[0]
        self.assertFalse(t == t2)
        self.assertTrue(self.t.htree.children.contents.label == t2.htree.label)
        self.assertTrue(check_equal_addresses(self.t.htree.children.contents, 
                t2.htree))

    def test_remove_middle(self):
        self.load_tree()
        t = self.t.children[1]
        self.t.children.remove(t)
        self.assertTrue(t.parent is None)
        self.assertTrue(t.next is None)
        t1 = self.t.children[0]
        t2 = self.t.children[1]
        self.assertFalse(t == t2)
        self.assertTrue(check_equal_addresses(t1.htree.next.contents, t2.htree))
 
    def test_remove_last(self):
        self.load_tree()
        t = self.t.children[2]
        self.t.children.remove(t)
        self.assertTrue(t.parent is None)
        self.assertTrue(t.next is None)
        t2 = self.t.children[1]
        self.assertTrue(t2.next is None)

    def test_subchildren(self):
        self.t.children['test'] = "1"
        

