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


#ifndef LIBMARY__VSLAB__H__
#define LIBMARY__VSLAB__H__


#include <libmary/vstack.h>
#include <libmary/intrusive_list.h>
#include <libmary/log.h>


namespace M {

template <class T> class VSlab;
template <class T> class VSlabRef;

// TODO Merge VSlabBase into VSlab
class VSlabBase
{
    template <class T> friend class VSlabRef;
    template <class T> friend class VSlab;

public:
    class Node
    {
    public:
	Size refcount;
    };

    virtual void free (Node *node) = 0;

    virtual ~VSlabBase () {}
};

template <class T>
mt_unsafe class VSlab : public VSlabBase
{
    template <class C> friend class VSlabRef;

private:
    class Node : public VSlabBase::Node,
		 public IntrusiveListElement<>
    {
    public:
	T obj;
    };

public:
    // TODO Either VSlabRef or VSlab::Ref should go away.
    class Ref
    {
	friend class VSlab;
	template <class C> friend class VSlabRef;

    private:
	VSlab *vslab;
	Node *node;

	Ref (VSlab * const vslab /* non-null */,
	     Node  * const node  /* non-null */)
	    : vslab (vslab),
	      node (node)
	{
	    node->refcount ++;
	}

    public:
	operator T* () const
	{
	    return &node->obj;
	}

	T& operator * () const
	{
	    return node->obj;
	}

	Ref& operator = (Ref const &ref)
	{
	    if (this == &ref)
		return *this;

	    if (node != NULL) {
		assert (node->refcount != 0);
		node->refcount --;
		if (node->refcount == 0)
		    vslab->free (node);
	    }

	    vslab = ref->vslab;
	    node = ref->node;

	    node->refcount ++;

	    return *this;
	}

	Ref (Ref const &ref)
	    : vslab (ref.vslab),
	      node (ref.node)
	{
	    node->refcount ++;
	}

	~Ref ()
	{
	    assert (node->refcount != 0);
	    node->refcount --;
	    if (node->refcount == 0)
		vslab->free (node);
	}
    };

    typedef Node* AllocKey;

private:
    IntrusiveList<Node> free_nodes;

#if 0
// Enable this after getting rid of AllocKey
    void free (Node * const node)
    {
	free_nodes.push_front (*node);

      // Note that no destructors are called.
    }
#endif

    virtual void free (VSlabBase::Node * const node)
    {
	free (static_cast <Node*> (node));
    }

public:
    VStack vstack;

    // Deprecated method
    T* alloc (AllocKey * const ret_key)
    {
	Node *node = NULL;
	if (free_nodes.isEmpty ()) {
	    node = new (vstack.push_malign (sizeof (Node), alignof (Node))) Node;
	    node->refcount = 1;
	} else {
	    node = free_nodes.getFirst ();
	    node->refcount = 1;
	    free_nodes.remove (free_nodes.getFirst ());
	}

	if (ret_key)
	    *ret_key = node;

	return &node->obj;
    }

    // Deprecated method
    // TODO The API is misleading. The same @size should be specified
    // for all alloc() calls on the same VSlab.
//#warning What's @size and does it play well with alignment?
    T* alloc (Size const size,
	      AllocKey * const ret_key)
    {
	Node *node = NULL;
	if (free_nodes.isEmpty ()) {
	    node = new (vstack.push_malign (sizeof (Node) - sizeof (T) + size, alignof (Node))) Node;
	    node->refcount = 1;
	} else {
	    node = free_nodes.getFirst();
	    node->refcount = 1;
	    free_nodes.remove (node);
	}

	if (ret_key)
	    *ret_key = node;

	return &node->obj;
    }

    Ref alloc ()
    {
	Node *node = NULL;
	if (free_nodes.isEmpty ()) {
	    node = new (vstack.push_malign (sizeof (Node), alignof (Node))) Node;
	    node->refcount = 0;
	} else {
	    node = free_nodes.getFirst ();
	    node->refcount = 0;
	    free_nodes.remove (free_nodes.getFirst ());
	}

	return Ref (this, node);
    }

    // TODO The API is misleading. The same @size should be specified
    // for all alloc() calls on the same VSlab.
    Ref alloc (Size const size)
    {
//	logD_ (_func, size);

	Node *node = NULL;
	if (free_nodes.isEmpty ()) {
	    node = new (vstack.push_malign (sizeof (Node) - sizeof (T) + size, alignof (Node))) Node;
	    node->refcount = 0;
	} else {
	    node = free_nodes.getFirst ();
	    node->refcount = 0;
	    free_nodes.remove (free_nodes.getFirst ());
	}

	return Ref (this, node);
    }

    // Deprecated method
    void free (AllocKey const key)
    {
	Node * const node = key;
	free_nodes.prepend (node);
      // Note that no destructors are called.
    }

    // TODO prealloc in bytes, not in instances of T (the latter makes no sense).
    VSlab (Size prealloc = 1024)
	: vstack (prealloc * sizeof (T) /* sizeof Node would be better */)
    {}

    ~VSlab ()
    {
      // TODO Call destructors for all free_nodes
    }
};

template <class T>
class VSlabRef
{
    template <class C> friend class VSlabRef;

private:
    VSlabBase *vslab;
    VSlabBase::Node *node;
    T *obj;

    VSlabRef (VSlabBase       * const vslab /* non-null */,
	      VSlabBase::Node * const node  /* non-null */,
	      T               * const obj   /* non-null */)
	: vslab (vslab),
	  node (node),
	  obj (obj)
    {
	node->refcount ++;
    }

public:
    bool isNull () const
    {
	return node == NULL;
    }

    operator T* () const
    {
	return obj;
    }

    T* operator -> () const
    {
	return obj;
    }

    T& operator * () const
    {
	return *obj;
    }

    VSlabRef& operator = (VSlabRef const &ref)
    {
	if (this == &ref)
	    return *this;

	if (node != NULL) {
	    assert (node->refcount != 0);
	    node->refcount --;
	    if (node->refcount == 0)
		vslab->free (node);
	}

	vslab = ref.vslab;
	node = ref.node;
	obj = ref.obj;

	if (node != NULL)
	    node->refcount ++;

	return *this;
    }

#if 0
    template <class C>
    VSlabRef& operator = (VSlabRef<C> const &ref)
    {
	if (node != NULL) {
	    assert (node->refcount != 0);
	    node->refcount --;
	    if (node->refcount == 0)
		vslab->free (node);
	}

	vslab = ref.vslab;
	node = ref.node;
	obj = ref.obj;

	if (node != NULL)
	    node->refcount ++;

	return *this;
    }
#endif

    template <class C>
    VSlabRef& operator = (typename VSlab<C>::Ref const &ref)
    {
	if (node != NULL) {
	    assert (node->refcount != 0);
	    node->refcount --;
	    if (node->refcount == 0)
		vslab->free (node);
	}

	vslab = ref.vslab;
	node = ref.node;
	obj = ref.obj;

	if (node != NULL)
	    node->refcount ++;

	return *this;
    }

    VSlabRef (VSlabRef const &ref)
	: vslab (ref.vslab),
	  node (ref.node),
	  obj (ref.obj)
    {
	if (node != NULL)
	    node->refcount ++;
    }

    template <class C>
    VSlabRef (VSlabRef<C> const &ref)
	: vslab (ref.vslab),
	  node (ref.node),
	  obj (ref.obj)
    {
	if (node != NULL)
	    node->refcount ++;
    }

    template <class C>
    VSlabRef (typename VSlab<C>::Ref const &ref)
	: vslab (ref.vslab),
	  node (ref.node),
	  obj (ref.obj)
    {
	if (node != NULL)
	    node->refcount ++;
    }

    template <class C>
    static VSlabRef forRef (typename VSlab<C>::Ref const &ref)
    {
	return VSlabRef (ref.vslab, ref.node, &ref.node->obj);
    }

    VSlabRef ()
	: vslab (NULL),
	  node (NULL),
	  obj (NULL)
    {
    }

    ~VSlabRef ()
    {
	if (node == NULL)
	    return;

	assert (node->refcount != 0);
	node->refcount --;
	if (node->refcount == 0)
	    vslab->free (node);
    }
};

}


#endif /* LIBMARY__VSLAB__H__ */

