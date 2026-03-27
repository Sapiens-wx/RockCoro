#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string>

//  Created by Yassaman Ommi on 2018-11-27.

namespace rockcoro {

enum RBTreeColor { RED, BLACK };

struct RBTreeNode {
    void *_data = nullptr;
    RBTreeColor _color = RED;
    RBTreeNode *_left = nullptr;
    RBTreeNode *_right = nullptr;
    RBTreeNode *_parent = nullptr;

    RBTreeNode()
    {
    }
    explicit RBTreeNode(void *val)
        : _data(val)
    {
    }
};

// ===== only used before a node allocator is implemented =====
static RBTreeNode *get_rbtree_node()
{
    return new RBTreeNode;
}
static void release_rbtree_node(RBTreeNode *node)
{
    delete node;
}
// ==========

template <typename T, typename Comparer> class RBTree {
private:
    using Node = RBTreeNode;

    Comparer _compare;
    Node *_root;
    Node *_nil;

    // rotations
    void leftRotate(Node *x);
    void rightRotate(Node *y);

    void fixInsert(Node *z);
    void fixDelete(Node *x);

    void transplant(Node *u, Node *v);
    void deleteNode(Node *z);

    Node *findNode(T *val, Node **parent = nullptr);
    Node *leftMost(Node *node) const;
    Node *rightMost(Node *node) const;

    void destroy(Node *node);

    void printHelper(Node *node, const std::string &prefix, bool isLeft) const;

public:
    RBTree();
    ~RBTree();

    bool insert(T *val);
    bool erase(T *val);
    T *find(T *val);

    T *minimum();
    T *maximum();

    void printTree() const;
};

// ================= constructor =================

template <typename T, typename Comparer>
RBTree<T, Comparer>::RBTree()
    : _compare()
{
    _nil = get_rbtree_node();
    _nil->_color = BLACK;
    _nil->_left = _nil;
    _nil->_right = _nil;
    _nil->_parent = _nil;

    _root = _nil;
}

template <typename T, typename Comparer> RBTree<T, Comparer>::~RBTree()
{
    destroy(_root);
    release_rbtree_node(_nil);
}

// ================= rotations =================

template <typename T, typename Comparer> void RBTree<T, Comparer>::leftRotate(Node *x)
{
    Node *y = x->_right;
    x->_right = y->_left;

    if (y->_left != _nil)
        y->_left->_parent = x;

    y->_parent = x->_parent;

    if (x->_parent == _nil)
        _root = y;
    else if (x == x->_parent->_left)
        x->_parent->_left = y;
    else
        x->_parent->_right = y;

    y->_left = x;
    x->_parent = y;
}

template <typename T, typename Comparer> void RBTree<T, Comparer>::rightRotate(Node *y)
{
    Node *x = y->_left;
    y->_left = x->_right;

    if (x->_right != _nil)
        x->_right->_parent = y;

    x->_parent = y->_parent;

    if (y->_parent == _nil)
        _root = x;
    else if (y == y->_parent->_left)
        y->_parent->_left = x;
    else
        y->_parent->_right = x;

    x->_right = y;
    y->_parent = x;
}

// ================= insert =================

template <typename T, typename Comparer> bool RBTree<T, Comparer>::insert(T *val)
{
    Node *parent = _nil;
    Node *cur = _root;

    while (cur != _nil) {
        parent = cur;
        if (_compare(val, (T *)cur->_data))
            cur = cur->_left;
        else if (_compare((T *)cur->_data, val))
            cur = cur->_right;
        else
            return false; // duplicate
    }

    Node *node = get_rbtree_node();
    node->_data = val;
    node->_left = _nil;
    node->_right = _nil;
    node->_parent = parent;
    node->_color = RED;

    if (parent == _nil)
        _root = node;
    else if (_compare(val, (T *)parent->_data))
        parent->_left = node;
    else
        parent->_right = node;

    fixInsert(node);
    return true;
}

// ================= fixInsert =================

template <typename T, typename Comparer> void RBTree<T, Comparer>::fixInsert(Node *z)
{
    while (z->_parent->_color == RED) {
        if (z->_parent == z->_parent->_parent->_left) {
            Node *y = z->_parent->_parent->_right;

            if (y->_color == RED) {
                z->_parent->_color = BLACK;
                y->_color = BLACK;
                z->_parent->_parent->_color = RED;
                z = z->_parent->_parent;
            } else {
                if (z == z->_parent->_right) {
                    z = z->_parent;
                    leftRotate(z);
                }
                z->_parent->_color = BLACK;
                z->_parent->_parent->_color = RED;
                rightRotate(z->_parent->_parent);
            }
        } else {
            Node *y = z->_parent->_parent->_left;

            if (y->_color == RED) {
                z->_parent->_color = BLACK;
                y->_color = BLACK;
                z->_parent->_parent->_color = RED;
                z = z->_parent->_parent;
            } else {
                if (z == z->_parent->_left) {
                    z = z->_parent;
                    rightRotate(z);
                }
                z->_parent->_color = BLACK;
                z->_parent->_parent->_color = RED;
                leftRotate(z->_parent->_parent);
            }
        }
    }
    _root->_color = BLACK;
}

// ================= erase =================

template <typename T, typename Comparer> bool RBTree<T, Comparer>::erase(T *val)
{
    Node *z = findNode(val);
    if (z == _nil)
        return false;

    deleteNode(z);
    return true;
}

// ================= delete =================

template <typename T, typename Comparer> void RBTree<T, Comparer>::deleteNode(Node *z)
{
    Node *y = z;
    Node *x;
    RBTreeColor yOriginalColor = y->_color;

    if (z->_left == _nil) {
        x = z->_right;
        transplant(z, z->_right);
    } else if (z->_right == _nil) {
        x = z->_left;
        transplant(z, z->_left);
    } else {
        y = leftMost(z->_right);
        yOriginalColor = y->_color;
        x = y->_right;

        if (y->_parent == z)
            x->_parent = y;
        else {
            transplant(y, y->_right);
            y->_right = z->_right;
            y->_right->_parent = y;
        }

        transplant(z, y);
        y->_left = z->_left;
        y->_left->_parent = y;
        y->_color = z->_color;
    }

    if (yOriginalColor == BLACK)
        fixDelete(x);

    release_rbtree_node(z);
}

// ================= fixDelete =================

template <typename T, typename Comparer> void RBTree<T, Comparer>::fixDelete(Node *x)
{
    while (x != _root && x->_color == BLACK) {
        if (x == x->_parent->_left) {
            Node *w = x->_parent->_right;

            if (w->_color == RED) {
                w->_color = BLACK;
                x->_parent->_color = RED;
                leftRotate(x->_parent);
                w = x->_parent->_right;
            }

            if (w->_left->_color == BLACK && w->_right->_color == BLACK) {
                w->_color = RED;
                x = x->_parent;
            } else {
                if (w->_right->_color == BLACK) {
                    w->_left->_color = BLACK;
                    w->_color = RED;
                    rightRotate(w);
                    w = x->_parent->_right;
                }
                w->_color = x->_parent->_color;
                x->_parent->_color = BLACK;
                w->_right->_color = BLACK;
                leftRotate(x->_parent);
                x = _root;
            }
        } else {
            Node *w = x->_parent->_left;

            if (w->_color == RED) {
                w->_color = BLACK;
                x->_parent->_color = RED;
                rightRotate(x->_parent);
                w = x->_parent->_left;
            }

            if (w->_right->_color == BLACK && w->_left->_color == BLACK) {
                w->_color = RED;
                x = x->_parent;
            } else {
                if (w->_left->_color == BLACK) {
                    w->_right->_color = BLACK;
                    w->_color = RED;
                    leftRotate(w);
                    w = x->_parent->_left;
                }
                w->_color = x->_parent->_color;
                x->_parent->_color = BLACK;
                w->_left->_color = BLACK;
                rightRotate(x->_parent);
                x = _root;
            }
        }
    }
    x->_color = BLACK;
}

// ================= helpers =================

template <typename T, typename Comparer>
typename RBTree<T, Comparer>::Node *RBTree<T, Comparer>::findNode(T *val, Node **parent)
{
    Node *cur = _root;
    Node *prev = _nil;

    while (cur != _nil) {
        prev = cur;

        if (_compare(val, (T *)cur->_data))
            cur = cur->_left;
        else if (_compare((T *)cur->_data, val))
            cur = cur->_right;
        else {
            if (parent)
                *parent = prev;
            return cur;
        }
    }

    if (parent)
        *parent = prev;
    return _nil;
}

template <typename T, typename Comparer> T *RBTree<T, Comparer>::find(T *val)
{
    Node *n = findNode(val);
    return (n == _nil) ? nullptr : (T *)n->_data;
}

template <typename T, typename Comparer>
typename RBTree<T, Comparer>::Node *RBTree<T, Comparer>::leftMost(Node *node) const
{
    while (node->_left != _nil)
        node = node->_left;
    return node;
}

template <typename T, typename Comparer>
typename RBTree<T, Comparer>::Node *RBTree<T, Comparer>::rightMost(Node *node) const
{
    while (node->_right != _nil)
        node = node->_right;
    return node;
}

template <typename T, typename Comparer> T *RBTree<T, Comparer>::minimum()
{
    if (_root == _nil)
        return nullptr;
    return (T *)leftMost(_root)->_data;
}

template <typename T, typename Comparer> T *RBTree<T, Comparer>::maximum()
{
    if (_root == _nil)
        return nullptr;
    return (T *)rightMost(_root)->_data;
}

// Transplant function used in deletion
template <typename T, typename Comparer> void RBTree<T, Comparer>::transplant(Node *u, Node *v)
{
    if (u->_parent == _nil)
        _root = v;
    else if (u == u->_parent->_left)
        u->_parent->_left = v;
    else
        u->_parent->_right = v;

    v->_parent = u->_parent;
}

template <typename T, typename Comparer> void RBTree<T, Comparer>::destroy(Node *node)
{
    if (node == _nil)
        return;

    destroy(node->_left);
    destroy(node->_right);

    release_rbtree_node(node);
}

// ================= print =================

template <typename T, typename Comparer>
void RBTree<T, Comparer>::printHelper(Node *node, const std::string &prefix, bool isLeft) const
{
    if (node == _nil)
        return;

    // 打印当前节点
    printf("%s", prefix.c_str());

    printf("%s", isLeft ? "└─ " : "┬─ ");

    printf("%4.4llu%c\n", (unsigned long long)node->_data, node->_color == RED ? '*' : ' ');

    // 构造下一层的 prefix
    std::string newPrefix = prefix + (isLeft ? "\t" : "│\t");

    // 先打印右子树（让结构更直观）
    printHelper(node->_right, newPrefix, false);

    // 再打印左子树
    printHelper(node->_left, newPrefix, true);
}

template <typename T, typename Comparer> void RBTree<T, Comparer>::printTree() const
{
    printHelper(_root, "", true);
}

} // namespace rockcoro