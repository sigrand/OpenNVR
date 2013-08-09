/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef __LIBMARY__INTRUSIVE_AVL_TREE__H__
#define __LIBMARY__INTRUSIVE_AVL_TREE__H__


#include <libmary/extractor.h>
#include <libmary/comparator.h>
#include <libmary/iterator.h>


namespace M {

// TODO addUniqueFor(bool *ret_exists) == lookupOrAddFor

class IntrusiveAvlTree_Default;

/*c Intrusive AVL tree node. */
template <class D = IntrusiveAvlTree_Default>
class IntrusiveAvlTree_Node
{
    template <class T, class TreeName> friend class IntrusiveAvlTree_common;
    template <class T, class Extractor, class Comparator, class TreeName> friend class IntrusiveAvlTree_anybase;
    template <class T, class Extractor, class Comparator, class TreeName, class Base> friend class IntrusiveAvlTree;

private:
    // TODO Public accessors in IntrusiveAvlTree_common

    IntrusiveAvlTree_Node *top;   //< The parent node.
    IntrusiveAvlTree_Node *left;  /*< The left subnode
				   *  (with a lesser or equal key value). */
    IntrusiveAvlTree_Node *right; /*< The right subnode
				   *  (with a greater or equal key value). */

    int  balance; /*< The balance of the node. '-1' means that
		   *  the left subtree is taller than the right one.
		   *  '1' means that the right subtree is taller than
		   *  the left one. '0' means that right and left
		   *  subtrees of this node are equally tall. */
};

// Note: It is not recommended to refer to this class directly.
// Use IntrusiveAvlTree<>:: instead.
//
template <class T, class TreeName = IntrusiveAvlTree_Default>
class IntrusiveAvlTree_common
{
public:
  // Note: No virtual destructor here.

    typedef IntrusiveAvlTree_Node<TreeName> Node;

    /* Goes from top to bottom, from left to right. */
    template <class Base = EmptyBase>
    class TopLeftIterator_ : public StatefulIterator<T*, Base>
    {
    protected:
	Node *node;

    public:
	T* next ()
	{
	    Node* ret = node;

	    assert (node);

	    if (node->left != NULL)
		node = node->left;
	    else
	    if (node->right != NULL) {
		node = node->right;
	    } else {
		while (node->top != NULL) {
		    if (node->top->right == node ||
			node->top->right == NULL)
		    {
			node = node->top;
		    } else {
			node = node->top->right;
			break;
		    }
		}

		if (node->top == NULL)
		    node = NULL;
	    }

	    return static_cast <T*> (ret);
	}

	bool done ()
	{
	    return node == NULL;
	}

	TopLeftIterator_ (IntrusiveAvlTree_common<T, TreeName> const &tree)
	{
	    node = tree.top;
	}

	TopLeftIterator_ (Node * const node)
	{
	    this->node = node;
	}
    };
    typedef TopLeftIterator_<> TopLeftIterator;

    /* Goes from bottom to top, from left to right.
     * This iterator is useful to walk the tree in ascending order. */
    template <class Base = EmptyBase>
    class BottomLeftIterator_ : public StatefulIterator<T*, Base>
    {
    protected:
	Node *node;

    public:
	T* next ()
	{
	    Node* ret = node;

	    assert (node);

	    if (node->right != NULL) {
		node = node->right;
		while (node->left != NULL)
		    node = node->left;
	    } else
	    if (node->top != NULL) {
		for (;;) {
		    if (node->top->left == node) {
			node = node->top;
			break;
		    } else {
			node = node->top;
			if (node->top == NULL) {
			    node = NULL;
			    break;
			}
		    }
		}
	    } else
		node = NULL;

	    return static_cast <T*> (ret);
	}

	bool done ()
	{
	    return node == NULL;
	}

	BottomLeftIterator_ (IntrusiveAvlTree_common<T, TreeName> const &tree)
	{
	    node = tree.top;
	    if (node != NULL) {
		while (node->left != NULL)
		    node = node->left;
	    }
	}

	BottomLeftIterator_ (Node * const node)
	{
	    this->node = node;
	}
    };
    typedef BottomLeftIterator_<> BottomLeftIterator;

    /* Goes from bottom to top, from right to left.
     * This iterator is useful to walk the tree in descending order. */
    template <class Base = EmptyBase>
    class BottomRightIterator_ : public StatefulIterator<T*, Base>
    {
    protected:
	Node *node;

    public:
	T* next ()
	{
	    Node * const ret = node;

	    assert (node);

	    if (node->left != NULL) {
		node = node->left;
		while (node->right != NULL)
		    node = node->right;
	    } else
	    if (node->top != NULL) {
		for (;;) {
		    if (node->top->right == node) {
			node = node->top;
			break;
		    } else {
			node = node->top;
			if (node->top == NULL) {
			    node = NULL;
			    break;
			}
		    }
		}
	    } else
		node = NULL;

	    return static_cast <T*> (ret);
	}

	bool done ()
	{
	    return node == NULL;
	}

	BottomRightIterator_ (IntrusiveAvlTree_common<T, TreeName> const &tree)
	{
	    node = tree.top;
	    if (node != NULL) {
		while (node->right != NULL)
		    node = node->right;
	    }
	}

	BottomRightIterator_ (Node * const node)
	{
	    this->node = node;
	}
    };
    typedef BottomRightIterator_<> BottomRightIterator;

    template <class Base = EmptyBase>
    class Iterator_ : public BottomLeftIterator_<Base>
    {
    public:
	Iterator_ (IntrusiveAvlTree_common<T, TreeName> const &tree)
	    : BottomLeftIterator_<Base> (tree)
	{
	}

	Iterator_ (Node * const node)
	    : BottomLeftIterator_<Base> (node)
	{
	}
    };
    typedef Iterator_<> Iterator;

    template <class Base = EmptyBase>
    class InverseIterator_ : public BottomRightIterator_<Base>
    {
    public:
	InverseIterator_ (IntrusiveAvlTree_common<T, TreeName> const &tree)
	    : BottomRightIterator_<Base> (tree)
	{
	}

	InverseIterator_ (Node * const node)
	    : BottomRightIterator_<Base> (node)
	{
	}
    };
    typedef InverseIterator_<> InverseIterator;

    /*> The root node of the tree. */
    Node *top;
};

template < class T,
	   class Extractor,
	   class Comparator,
	   class TreeName >
class IntrusiveAvlTree_anybase
	: public IntrusiveAvlTree_common<T, TreeName>
{
public:
    template <class IteratorBase = EmptyBase>
    class SameKeyIterator_ : public StatefulIterator< typename IntrusiveAvlTree_common<T, TreeName>::Node&,
						      IteratorBase >
    {
    private:
	typename IntrusiveAvlTree_common<T, TreeName>::Node const * const leftmost_node;
	typename IntrusiveAvlTree_common<T, TreeName>::BottomLeftIterator iter;

	typename IntrusiveAvlTree_common<T, TreeName>::Node *next_node;
	// Helper flag to avoid calling Comparator::equals() multiple times.
	Bool is_done;

    public:
	T* next ()
	{
	    typename IntrusiveAvlTree_common<T, TreeName>::Node * const res_node = next_node;

	    if (iter.done ())
		next_node = NULL;
	    else
		next_node = iter.next ();

	    return static_cast <T*> (res_node);
	}

	bool done ()
	{
	    if (next_node == NULL)
		return true;

	    if (is_done)
		return true;

	    if (!Comparator::equals (Extractor::getValue (* static_cast <T*> (leftmost_node)),
				     Extractor::getValue (* static_cast <T*> (next_node))))
	    {
		is_done = true;
		return true;
	    }

	    return false;
	}

	SameKeyIterator_ (typename IntrusiveAvlTree_common<T, TreeName>::Node * const leftmost_node)
	    : leftmost_node (leftmost_node),
	      iter (leftmost_node)
	{
	    if (iter.done ())
		next_node = NULL;
	    else
		next_node = iter.next ();
	}
    };
    typedef SameKeyIterator_<> SameKeyIterator;
};

/*c Balanced binary tree (AVL tree). */
template < class T,
	   class Extractor = DirectExtractor<T>,
	   class Comparator = DirectComparator<T>,
	   class TreeName = IntrusiveAvlTree_Default,
	   class Base = EmptyBase >
class IntrusiveAvlTree : public IntrusiveAvlTree_anybase< T,
							  Extractor,
							  Comparator,
							  TreeName >,
			 public Base
{
protected:
    typename IntrusiveAvlTree_common<T, TreeName>::Node* rotateSingleLeft (typename IntrusiveAvlTree_common<T, TreeName>::Node * const node)
    {
	/* "bub" stands for "bubble" - the node that
	 * pops up to the top after performing the rotation. */
	typename IntrusiveAvlTree_common<T, TreeName>::Node * const bub = node->right;

	node->right = bub->left;
	if (bub->left != NULL)
	    bub->left->top = node;

	bub->top = node->top;
	if (node->top != NULL) {
	    if (node->top->left == node)
		node->top->left = bub;
	    else
		node->top->right = bub;
	}

	bub->left = node;
	node->top = bub;

	if (bub->balance == 0) {
	    bub->balance = -1;
	    node->balance = 1;
	} else {
	    bub->balance = 0;
	    node->balance = 0;
	}

	if (IntrusiveAvlTree_common<T, TreeName>::top == node)
	    IntrusiveAvlTree_common<T, TreeName>::top = bub;

	return bub;
    }

    typename IntrusiveAvlTree_common<T, TreeName>::Node* rotateSingleRight (typename IntrusiveAvlTree_common<T, TreeName>::Node * const node)
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node * const bub = node->left;

	node->left = bub->right;
	if (bub->right != NULL)
	    bub->right->top = node;

	bub->top = node->top;
	if (node->top != NULL) {
	    if (node->top->right == node)
		node->top->right = bub;
	    else
		node->top->left = bub;
	}

	bub->right = node;
	node->top = bub;

	if (bub->balance == 0) {
	    bub->balance = 1;
	    node->balance = -1;
	} else {
	    bub->balance = 0;
	    node->balance = 0;
	}

	if (IntrusiveAvlTree_common<T, TreeName>::top == node)
	    IntrusiveAvlTree_common<T, TreeName>::top = bub;

	return bub;
    }

    typename IntrusiveAvlTree_common<T, TreeName>::Node* rotateDoubleLeft (typename IntrusiveAvlTree_common<T, TreeName>::Node * const node)
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node * const bub = node->right->left;

	bub->top = node->top;
	if (node->top != NULL) {
	    if (node->top->left == node)
		node->top->left = bub;
	    else
		node->top->right = bub;
	}

	node->right->left = bub->right;
	if (bub->right != NULL)
	    bub->right->top = node->right;

	bub->right = node->right;
	node->right->top = bub;

	node->right = bub->left;
	if (bub->left != NULL)
	    bub->left->top = node;

	bub->left = node;
	node->top = bub;

	if (bub->balance == 0) {
	    node->balance = 0;
	    bub->right->balance = 0;
	} else
	if (bub->balance == -1) {
	    node->balance = 0;
	    bub->right->balance = 1;
	} else {
	    node->balance = -1;
	    bub->right->balance = 0;
	}
	bub->balance = 0;

	if (IntrusiveAvlTree_common<T, TreeName>::top == node)
	    IntrusiveAvlTree_common<T, TreeName>::top = bub;

	return bub;
    }

    typename IntrusiveAvlTree_common<T, TreeName>::Node* rotateDoubleRight (typename IntrusiveAvlTree_common<T, TreeName>::Node * const node)
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node * const bub = node->left->right;

	bub->top = node->top;
	if (node->top != NULL) {
	    if (node->top->right == node)
		node->top->right = bub;
	    else
		node->top->left = bub;
	}

	node->left->right = bub->left;
	if (bub->left != NULL)
	    bub->left->top = node->left;

	bub->left = node->left;
	node->left->top = bub;

	node->left = bub->right;
	if (bub->right != NULL)
	    bub->right->top = node;

	bub->right = node;
	node->top = bub;

	if (bub->balance == 0) {
	    node->balance = 0;
	    bub->left->balance = 0;
	} else
	if (bub->balance == 1) {
	    node->balance = 0;
	    bub->left->balance = -1;
	} else {
	    node->balance = 1;
	    bub->left->balance = 0;
	}
	bub->balance = 0;

	if (IntrusiveAvlTree_common<T, TreeName>::top == node)
	    IntrusiveAvlTree_common<T, TreeName>::top = bub;

	return bub;
    }

    // Returns a non-null pointer to an existing node if @unique is true and
    // there's a node with the same value in the tree already.
    T* doAdd (T    * const value,
	      bool   const unique)
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = static_cast <typename IntrusiveAvlTree_common<T, TreeName>::Node*> (value);
	bool left = false;

	node->top     = NULL;
	node->left    = NULL;
	node->right   = NULL;
	node->balance = 0;

	typename IntrusiveAvlTree_common<T, TreeName>::Node *upper = IntrusiveAvlTree_common<T, TreeName>::top;
	while (upper != NULL) {
	    if (unique) {
		if (Comparator::equals (Extractor::getValue (* static_cast <T*> (upper)),
					Extractor::getValue (*value)))
		{
		    return static_cast <T*> (upper);
		}
	    }

	    if (Comparator::greater (Extractor::getValue (* static_cast <T*> (upper)),
				     Extractor::getValue (*value)))
	    {
		if (upper->left != NULL)
		    upper = upper->left;
		else {
		    upper->left = node;
		    left = true;
		    break;
		}
	    } else {
		if (upper->right != NULL)
		    upper = upper->right;
		else {
		    upper->right = node;
		    left = false;
		    break;
		}
	    }
	}

	node->top = upper;
	if (upper == NULL)
	    IntrusiveAvlTree_common<T, TreeName>::top = node;

	node = node->top;
	while (node != NULL) {
	    if (left == false) {
		if (node->balance == 1) {
		    if (node->right->balance == -1)
			node = rotateDoubleLeft (node);
		    else
			node = rotateSingleLeft (node);
		} else
		    node->balance ++;
	    } else {
		if (node->balance == -1) {
		    if (node->left->balance == 1)
			node = rotateDoubleRight (node);
		    else
			node = rotateSingleRight (node);
		} else
		    node->balance --;
	    }

	    if (node->top == NULL ||
		node->balance == 0)
		break;

	    if (node->top->left == node)
		left = true;
	    else
		left = false;

	    node = node->top;
	}

	return NULL;
    }

public:
    bool isEmpty () const
    {
	return IntrusiveAvlTree_common<T, TreeName>::top == NULL;
    }

    /*m Adds a unique node to the tree.
     *
     * Returns a non-null pointer to an existing node if there's a node with
     * the same value in the tree already.
     */
    T* addUnique (T * const value)
    {
	return doAdd (value, true /* unique */);
    }

    /*m Adds a node to the tree.
     */
    void add (T * const value)
    {
	doAdd (value, false /* unique */);
    }

    /*m Remove a node from the tree.
     *
     * @node The node to remove from the tree.
     */
    void remove (typename IntrusiveAvlTree_common<T, TreeName>::Node *node)
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *repl = NULL,
                                                            *tobalance = NULL;
	bool left = false;

	if (node->balance == 0 &&
	    node->left    == NULL)
	{
	    if (node->top != NULL) {
		if (node->top->left == node) {
		    node->top->left = NULL;
		    left = true;
		} else {
		    node->top->right = NULL;
		    left = false;
		}

		tobalance = node->top;
	    } else
		IntrusiveAvlTree_common<T, TreeName>::top = NULL;
	} else {
	    if (node->balance == 1) {
		repl = node->right;
		while (repl->left != NULL)
		    repl = repl->left;

		if (repl->top != node) {
		    repl->top->left = repl->right;
		    if (repl->right != NULL)
			repl->right->top = repl->top;
		    left = true;
		} else
		    left = false;
	    } else {
		repl = node->left;
		while (repl->right != NULL)
		    repl = repl->right;

		if (repl->top != node) {
		    repl->top->right = repl->left;
		    if (repl->left != NULL)
			repl->left->top = repl->top;
		    left = false;
		} else
		    left = true;
	    }

	    repl->balance = node->balance;

	    if (repl->top != node)
		tobalance = repl->top;
	    else
		tobalance = repl;

	    repl->top = node->top;
	    if (node->left != repl) {
		repl->left = node->left;
		if (node->left != NULL)
		    node->left->top = repl;
	    }
	    if (node->right != repl) {
		repl->right = node->right;
		if (node->right != NULL)
		    node->right->top = repl;
	    }

	    if (node->top != NULL) {
		if (node->top->left == node)
		    node->top->left = repl;
		else
		    node->top->right = repl;
	    }

	    if (IntrusiveAvlTree_common<T, TreeName>::top == node)
		IntrusiveAvlTree_common<T, TreeName>::top = repl;
	}

	node = tobalance;
	while (node != NULL) {
	    if (left) {
		if (node->balance == 1) {
		    if (node->right->balance == -1)
			node = rotateDoubleLeft (node);
		    else
			node = rotateSingleLeft (node);
		} else
		    node->balance ++;
	    } else {
		if (node->balance == -1) {
		    if (node->left->balance == 1)
			node = rotateDoubleRight (node);
		    else
			node = rotateSingleRight (node);
		} else
		    node->balance --;
	    }

	    if (node->top == NULL ||
		node->balance != 0)
		break;

	    if (node->top->left == node)
		left = true;
	    else
		left = false;

	    node = node->top;
	}
    }

    /*m Clear the tree (i.e., remove all nodes from the tree). */
    void clear ()
    {
	IntrusiveAvlTree_common<T, TreeName>::top = NULL;
    }

    /*m Find a node with the specified key value by
     * unconditional traversal of the whole tree.
     *
     * If there is no node with the key value requested,
     * then NULL will be returned.
     *
     * @c The key value of the node to find. */
    template <class C>
    T* lookup (C const &c) const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top;
	while (node != NULL) {
	    if (Comparator::equals (Extractor::getValue (* static_cast <T*> (node)), c))
		break;

	    if (Comparator::greater (Extractor::getValue (* static_cast <T*> (node)), c))
		node = node->left;
	    else
		node = node->right;
	}

	return static_cast <T*> (node);
    }

    template <class C>
    T* lookupLeftmost (C const &c) const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top;
	typename IntrusiveAvlTree_common<T, TreeName>::Node *last_match = NULL;

	for (;;) {
	    while (node != NULL) {
		if (Comparator::equals (Extractor::getValue (* static_cast <T*> (node)), c)) {
		    last_match = node;
		    break;
		}

		if (last_match != NULL) {
		    node = node->right;
		    continue;
		}

		if (Comparator::greater (Extractor::getValue (* static_cast <T*> (node)), c))
		    node = node->left;
		else
		    node = node->right;
	    }

	    if (node == NULL)
		break;

	    node = node->left;
	}

	return static_cast <T*> (last_match);
    }

    /*m Find a node with the specified key value by
     * unconditional traversal of the whole tree.
     *
     * This is an <b>inefficient</b> (O(N) complexity)
     * way of searching for a node in the tree.
     *
     * If there is no node with the key value requested,
     * then NULL will be returned.
     *
     * @c The key value of the node to find. */
    template <class C>
    T* lookupByTraversal (C const &c) const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top;
	while (node != NULL) {
	    if (Comparator::equals (Extractor::getValue (* static_cast <T*> (node)), c))
		break;

	    if (node->left != NULL)
		node = node->left;
	    else
	    if (node->right != NULL) {
		node = node->right;
	    } else {
		while (node->top != NULL) {
		    if (node->top->right == node ||
			node->top->right == NULL)
		    {
			node = node->top;
		    } else {
			node = node->top->right;
			break;
		    }
		}

		if (node->top == NULL)
		    node = NULL;
	    }
	}

	return static_cast <T*> (node);
    }

    // TODO Check semantics and correctness of this method. Applies to class AvlTree as well.
    template <class C>
    T* getFirstGreater (C const &c) const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top,
						  *greater = NULL;

	while (node != NULL) {
	    if (Comparator::greater (Extractor::getValue (* static_cast <T*> (node)), c)) {
		greater = node;
		node = node->left;
	    } else
		node = node->right;
	}

	return static_cast <T*> (greater);
    }

    // TODO Check semantics and correctness of this method. Applies to class AvlTree as well.
    template <class C>
    T* getLastLesser (C const &c) const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top,
						  *lesser = NULL;

	while (node != NULL) {
	    if (!Comparator::greater (Extractor::getValue (* static_cast <T*> (node)), c)) {
		lesser = node;
		node = node->right;
	    } else
		node = node->left;
	}

	return static_cast <T*> (lesser);
    }

    T* getLeftmost () const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top,
						  *leftmost = NULL;

	while (node != NULL) {
	    leftmost = node;
	    node = node->left;
	}

	return static_cast <T*> (leftmost);
    }

    T* getRightmost () const
    {
	typename IntrusiveAvlTree_common<T, TreeName>::Node *node = IntrusiveAvlTree_common<T, TreeName>::top,
						  *rightmost = NULL;

	while (node != NULL) {
	    rightmost = node;
	    node = node->right;
	}

	return static_cast <T*> (rightmost);
    }

    T* getNextTo (typename IntrusiveAvlTree_common<T, TreeName>::Node * const node) const
    {
	if (node == NULL)
	    return NULL;

	typename IntrusiveAvlTree_common<T, TreeName>::Node *ret;

	if (node->right != NULL) {
	    ret = node->right;
	    while (ret->left != NULL)
		ret = ret->left;

	    return ret;
	} else
	if (node->top != NULL) {
	    ret = node;
	    for (;;) {
		if (ret->top->left == ret) {
		    ret = ret->top;
		    break;
		} else {
		    ret = ret->top;
		    if (ret->top == NULL) {
			ret = NULL;
			break;
		    }
		}
	    }
	} else
	    ret = NULL;

	return static_cast <T*> (ret);
    }

    T* getPreviousTo (typename IntrusiveAvlTree_common<T, TreeName>::Node * const node) const
    {
	if (node == NULL)
	    return NULL;

	typename IntrusiveAvlTree_common<T, TreeName>::Node *ret;

	if (node->left != NULL) {
	    ret = node->left;
	    while (ret->right != NULL)
		ret = ret->right;

	    return ret;
	} else
	if (node->top != NULL) {
	    ret = node;
	    for (;;) {
		if (ret->top->right == ret) {
		    ret = ret->top;
		    break;
		} else {
		    ret = ret->top;
		    if (ret->top == NULL) {
			ret = NULL;
			break;
		    }
		}
	    }
	} else
	    ret = NULL;

	return static_cast <T*> (ret);
    }

    IntrusiveAvlTree () {
	IntrusiveAvlTree_common<T, TreeName>::top = NULL;
    }
};

}


#endif /* __LIBMARY__INTRUSIVE_AVL_TREE__H__ */

