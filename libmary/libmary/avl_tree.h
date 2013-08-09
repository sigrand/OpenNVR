/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#ifndef LIBMARY__AVL_TREE__H__
#define LIBMARY__AVL_TREE__H__


#include <libmary/extractor.h>
#include <libmary/comparator.h>
#include <libmary/iterator.h>


namespace M {

// TODO addUniqueFor(bool *ret_exists) == lookupOrAddFor

// Note: It is not recommended to refer to this class directly.
// Use AvlTree<>:: instead.
//
// TODO Rename to AvlTree_common
template <class T>
class AvlTreeBase
{
public:
  // Note: No virtual destructor here.

    /*s AVL tree node. */
    struct Node
    {
	Node *top;	//< The parent node.
	Node *left;	/*< The left subnode
			 *  (with a lesser or equal key value). */
	Node *right;	/*< The right subnode
			 *  (with a greater or equal key value). */
	int  balance;	/*< The balance of the node. '-1' means that
			 *  the left subtree is taller than the right one.
			 *  '1' means that the right subtree is taller than
			 *  the left one. '0' means that right and left
			 *  subtrees of this node are equally tall. */

	T value;	//< The value associated with this node.
    };

    /* Goes from top to bottom, from left to right. */
    class TopLeftIterator : public StatefulIterator< typename AvlTreeBase<T>::Node& >
    {
    protected:
	typename AvlTreeBase<T>::Node *node;

    public:
        bool operator == (TopLeftIterator const &iter) const { return node == iter.node; }
        bool operator != (TopLeftIterator const &iter) const { return node != iter.node; }

	typename AvlTreeBase<T>::Node& next ()
	{
	    typename AvlTreeBase<T>::Node* ret = node;

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

	    return *ret;
	}

	bool done ()
	{
	    return node == NULL;
	}

	TopLeftIterator (AvlTreeBase<T> const &tree)
	{
	    node = tree.top;
	}

	TopLeftIterator (typename AvlTreeBase<T>::Node *node)
	{
	    this->node = node;
	}
    };

    /* Goes from bottom to top, from left to right.
     * This iterator is useful to walk the tree in ascending order. */
    class BottomLeftIterator : public StatefulIterator< typename AvlTreeBase<T>::Node& >
    {
    protected:
	typename AvlTreeBase<T>::Node *node;

    public:
	typename AvlTreeBase<T>::Node& next ()
	{
	    typename AvlTreeBase<T>::Node* ret = node;

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

	    return *ret;
	}

	bool done ()
	{
	    return node == NULL;
	}

	BottomLeftIterator (AvlTreeBase<T> const &tree)
	{
	    node = tree.top;
	    if (node != NULL) {
		while (node->left != NULL)
		    node = node->left;
	    }
	}

	BottomLeftIterator (typename AvlTreeBase<T>::Node *node)
	{
	    this->node = node;
	}
    };

    /* Goes from bottom to top, from right to left.
     * This iterator is useful to walk the tree in descending order. */
    class BottomRightIterator : public StatefulIterator< typename AvlTreeBase<T>::Node& >
    {
    protected:
	typename AvlTreeBase<T>::Node *node;

    public:
	typename AvlTreeBase<T>::Node& next ()
	{
	    typename AvlTreeBase<T>::Node * const ret = node;

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

	    return *ret;
	}

	bool done ()
	{
	    return node == NULL;
	}

	BottomRightIterator (AvlTreeBase<T> const &tree)
	{
	    node = tree.top;
	    if (node != NULL) {
		while (node->right != NULL)
		    node = node->right;
	    }
	}

	BottomRightIterator (typename AvlTreeBase<T>::Node * const node)
	{
	    this->node = node;
	}
    };

    class Iterator : public BottomLeftIterator
    {
    public:
	Iterator (AvlTreeBase<T> const &tree)
	    : BottomLeftIterator (tree)
	{
	}

	Iterator (typename AvlTreeBase<T>::Node * const node)
	    : BottomLeftIterator (node)
	{
	}
    };

    class InverseIterator : public BottomRightIterator
    {
    public:
	InverseIterator (AvlTreeBase<T> const &tree)
	    : BottomRightIterator (tree)
	{
	}

	InverseIterator (typename AvlTreeBase<T>::Node * const node)
	    : BottomRightIterator (node)
	{
	}
    };


  // ________________________________ iterator _________________________________

    class iterator
    {
    private:
        TopLeftIterator iter;

    public:
        iterator (AvlTreeBase<T> const &tree) : iter (tree) {}
        iterator (typename AvlTreeBase<T>::Node * const node) : iter (node) {}

        bool operator == (iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (iterator const &iter) const { return this->iter != iter.iter; }

        bool done () /* const */ { return iter.done (); }
        typename AvlTreeBase<T>::Node* next () { return &iter.next (); }
    };


  // _______________________________ bl_iterator _______________________________

    class bl_iterator
    {
    private:
        BottomLeftIterator iter;

    public:
        bl_iterator (AvlTreeBase<T> const &tree) : iter (tree) {}
        bl_iterator (typename AvlTreeBase<T>::Node * const node) : iter (node) {}

        bool operator == (iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (iterator const &iter) const { return this->iter != iter.iter; }

        bool done () /* const */ { return iter.done (); }
        typename AvlTreeBase<T>::Node* next () { return &iter.next (); }
    };

  // ___________________________________________________________________________


    /*> The root node of the tree. */
    Node *top;

    Iterator createIterator () const
    {
	return Iterator (*this);
    }
};

template < class T,
	   class Extractor,
	   class Comparator >
class AvlTree_anybase
	: public AvlTreeBase<T>
{
};

/*c Balanced binary tree (AVL tree). */
template < class T,
	   class Extractor = DirectExtractor<T>,
	   class Comparator = DirectComparator<T>,
	   class Base = EmptyBase >
class AvlTree : public AvlTree_anybase< T,
					Extractor,
					Comparator >,
		public Base
{
public:
    template <class IteratorBase = EmptyBase>
    class SameKeyIterator_ : public StatefulIterator< typename AvlTreeBase<T>::Node&,
						      IteratorBase >
    {
    private:
	typename AvlTreeBase<T>::Node const * const leftmost_node;
	typename AvlTreeBase<T>::BottomLeftIterator iter;

	typename AvlTreeBase<T>::Node *next_node;
	// Helper flag to avoid calling Comparator::equals() multiple times.
	Bool is_done;

    public:
	typename AvlTreeBase<T>::Node& next ()
	{
	    typename AvlTreeBase<T>::Node &res_node = *next_node;

	    if (iter.done ())
		next_node = NULL;
	    else
		next_node = &iter.next ();

	    return res_node;
	}

	bool done ()
	{
	    if (next_node == NULL)
		return true;

	    if (is_done)
		return true;

	    if (!Comparator::equals (Extractor::getValue (leftmost_node->value),
				     Extractor::getValue (next_node->value)))
	    {
		is_done = true;
		return true;
	    }

	    return false;
	}

	SameKeyIterator_ (typename AvlTreeBase<T>::Node * const leftmost_node)
	    : leftmost_node (leftmost_node),
	      iter (leftmost_node)
	{
	    if (iter.done ())
		next_node = NULL;
	    else
		next_node = &iter.next ();
	}
    };

    typedef SameKeyIterator_<> SameKeyIterator;

    #ifdef MyCpp_AvlTree_SameKeyDataIterator_base
    #error redefinition
    #endif
    #define MyCpp_AvlTree_SameKeyDataIterator_base						\
	    StatefulExtractorIterator < T&,							\
					MemberExtractor< typename AvlTreeBase<T>::Node,	\
							 T,					\
							 &AvlTreeBase<T>::Node::value >,	\
					SameKeyIterator,					\
					IteratorBase >

    template <class IteratorBase = EmptyBase>
    class SameKeyDataIterator_ : public MyCpp_AvlTree_SameKeyDataIterator_base
    {
    public:
	SameKeyDataIterator_ (typename AvlTreeBase<T>::Node * const leftmost_node)
	    : MyCpp_AvlTree_SameKeyDataIterator_base (SameKeyIterator (leftmost_node))
	{
	}
    };
    typedef SameKeyDataIterator_<> SameKeyDataIterator;

    #undef MyCpp_AvlTree_SameKeyDataIterator_base

    SameKeyDataIterator createSameKeyDataIterator ()
    {
	return SameKeyDataIterator (*this);
    }

protected:
    typename AvlTreeBase<T>::Node* rotateSingleLeft (typename AvlTreeBase<T>::Node *node)
    {
	/* "bub" stands for "bubble" - the node that
	 * pops up to the top after performing the rotation. */
	typename AvlTreeBase<T>::Node *bub = node->right;

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

	if (AvlTreeBase<T>::top == node)
	    AvlTreeBase<T>::top = bub;

	return bub;
    }

    typename AvlTreeBase<T>::Node* rotateSingleRight (typename AvlTreeBase<T>::Node *node)
    {
	typename AvlTreeBase<T>::Node *bub = node->left;

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

	if (AvlTreeBase<T>::top == node)
	    AvlTreeBase<T>::top = bub;

	return bub;
    }

    typename AvlTreeBase<T>::Node* rotateDoubleLeft (typename AvlTreeBase<T>::Node *node)
    {
	typename AvlTreeBase<T>::Node *bub = node->right->left;

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

	if (AvlTreeBase<T>::top == node)
	    AvlTreeBase<T>::top = bub;

	return bub;
    }

    typename AvlTreeBase<T>::Node* rotateDoubleRight (typename AvlTreeBase<T>::Node *node)
    {
	typename AvlTreeBase<T>::Node *bub = node->left->right;

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

	if (AvlTreeBase<T>::top == node)
	    AvlTreeBase<T>::top = bub;

	return bub;
    }

public:
#if 0
    // TODO
    // (testing...)
    typename AvlTreeBase<T>::Node* add (T value)
    {
	typename AvlTreeBase<T>::Node *node = addForValue (value);
	node->value = value;
	return node;
    }
#endif

    bool isEmpty () const
    {
	return AvlTreeBase<T>::top == NULL;
    }

    /*m Add a node to the tree.
     *
     * The value passed to this method will be
     * assigned to the <c>value</c> field of the <t>Node</t> structure
     * using the appropriate conversion constructor for type <t>T</t>.
     *
     * @value The value that will be associated with the new node. */
    typename AvlTreeBase<T>::Node* add (T const &value)
    {
	typename AvlTreeBase<T>::Node *node = addForValue (value);
	node->value = value;
	return node;
    }

    /*m Add a node to the tree without actual node's
     * value assignment.
     *
     * This method acts just like the <c>add</c> method, except that
     * it does not assign the passed value to the newly created
     * <t>Node</t> structure, and thus the copying/conversion
     * constructor is not called.
     *
     * This method is useful, if the value of type <t>C</t> is
     * actually the key for this AVL tree, but there is no
     * appropriate conversion constructor for type <t>T</t>.
     *
     * @value The value that will be associated with the new node. */

    typename AvlTreeBase<T>::Node* addForValue (T const &value)
    {
	return addFor (Extractor::getValue (value));
    }

    template <class C>
    typename AvlTreeBase<T>::Node* addFor (C const &value)
    {
	typename AvlTreeBase<T>::Node *node = new typename AvlTreeBase<T>::Node;
	typename AvlTreeBase<T>::Node *ret = node;
	bool left = false;

	node->top     = NULL;
	node->left    = NULL;
	node->right   = NULL;
	node->balance = 0;

	typename AvlTreeBase<T>::Node *upper = AvlTreeBase<T>::top;
	while (upper != NULL) {
	    if (Comparator::greater (Extractor::getValue (upper->value), value)) {
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
	    AvlTreeBase<T>::top = node;

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

	return ret;
    }

    /*m Remove a node from the tree.
     *
     * @node The node to remove from the tree. */
    void remove (typename AvlTreeBase<T>::Node *node)
    {
	typename AvlTreeBase<T>::Node *repl = NULL,
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
		AvlTreeBase<T>::top = NULL;

	    delete node;
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

	    if (AvlTreeBase<T>::top == node)
		AvlTreeBase<T>::top = repl;

	    delete node;
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

    /*m Clear the tree (i.e., delete all nodes from the tree). */
    void clear ()
    {
	typename AvlTreeBase<T>::Node *node,
				      *tmp;

	node = AvlTreeBase<T>::top;
	while (node != NULL) {
	    while (1) {
		if (node->left != NULL)
		    node = node->left;
		else
		if (node->right != NULL)
		    node = node->right;
		else
		    break;
	    }

	    while (1) {
		tmp = node;
		node = node->top;
		delete tmp;

		if (node == NULL)
		    break;

		if (node->left == tmp) {
		    node->left = NULL;
		    if (node->right != NULL) {
			node = node->right;
			break;
		    }
		} else
		    node->right = NULL;
	    }
	}

	AvlTreeBase<T>::top = NULL;
    }

    template <class C>
    typename AvlTreeBase<T>::Node* lookupValue (C &value) const
    {
	return lookup (Extractor::getValue (value));
    }

    /*m Find a node with the specified key value by
     * unconditional traversal of the whole tree.
     *
     * If there is no node with the key value requested,
     * then NULL will be returned.
     *
     * @c The key value of the node to find. */
    template <class C>
    typename AvlTreeBase<T>::Node* lookup (const C &c) const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top;
	while (node != NULL) {
	    if (Comparator::equals (Extractor::getValue (node->value), c))
		break;

	    if (Comparator::greater (Extractor::getValue (node->value), c))
		node = node->left;
	    else
		node = node->right;
	}

	return node;
    }

    template <class C>
    typename AvlTreeBase<T>::Node* lookupLeftmostValue (C &value) const
    {
	return lookupLeftmost (Extractor::getValue (value));
    }

    template <class C>
    typename AvlTreeBase<T>::Node* lookupLeftmost (C const &c) const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top;

	typename AvlTreeBase<T>::Node *last_match = NULL;

	for (;;) {
	    while (node != NULL) {
		if (Comparator::equals (Extractor::getValue (node->value), c)) {
		    last_match = node;
		    break;
		}

		if (last_match != NULL) {
		    node = node->right;
		    continue;
		}

		if (Comparator::greater (Extractor::getValue (node->value), c))
		    node = node->left;
		else
		    node = node->right;
	    }

	    if (node == NULL)
		break;

	    node = node->left;
	}

	return last_match;
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
    typename AvlTreeBase<T>::Node* lookupByTraversal (const C &c) const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top;
	while (node != NULL) {
	    if (Comparator::equals (Extractor::getValue (node->value), c))
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

	return node;
    }

    template <class C>
    typename AvlTreeBase<T>::Node* getFirstGreater (const C &c) const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top,
				      *greater = NULL;

	while (node != NULL) {
	    if (Comparator::greater (Extractor::getValue (node->value), c)) {
		greater = node;
		node = node->left;
	    } else
		node = node->right;
	}

	return greater;
    }

    template <class C>
    typename AvlTreeBase<T>::Node* getLastLesser (const C &c) const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top,
				      *lesser = NULL;

	while (node != NULL) {
	    if (!Comparator::greater (Extractor::getValue (node->value), c)) {
		lesser = node;
		node = node->right;
	    } else
		node = node->left;
	}

	return lesser;
    }

    typename AvlTreeBase<T>::Node* getLeftmost () const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top,
				      *leftmost = NULL;

	while (node != NULL) {
	    leftmost = node;
	    node = node->left;
	}

	return leftmost;
    }

    typename AvlTreeBase<T>::Node* getRightmost () const
    {
	typename AvlTreeBase<T>::Node *node = AvlTreeBase<T>::top,
				      *rightmost = NULL;

	while (node != NULL) {
	    rightmost = node;
	    node = node->right;
	}

	return rightmost;
    }

    typename AvlTreeBase<T>::Node* getNextTo (typename AvlTreeBase<T>::Node *node) const
    {
	if (node == NULL)
	    return NULL;

	typename AvlTreeBase<T>::Node *ret;

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

	return ret;
    }

    typename AvlTreeBase<T>::Node *getPreviousTo (typename AvlTreeBase<T>::Node *node) const
    {
	if (node == NULL)
	    return NULL;

	typename AvlTreeBase<T>::Node *ret;

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

	return ret;
    }

    AvlTree () {
	AvlTreeBase<T>::top = NULL;
    }

    ~AvlTree () {
	clear ();
    }
};

}


#endif /* LIBMARY__AVL_TREE__H__ */

