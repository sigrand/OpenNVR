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


#ifndef LIBMARY__PAGE_POOL__H__
#define LIBMARY__PAGE_POOL__H__


#include <libmary/types.h>
#include <libmary/array.h>
#include <libmary/atomic.h>
#include <libmary/mutex.h>
#include <libmary/output_stream.h>


namespace M {

// TODO Per-thread page pools. When a page is released in a given thread,
//      it is put into page pool of that thread, which may be different
//      from the one we took the page from.

// TODO PagePool should be referenced while there's any referenced page.
//      This also means that PagePools should be independent objects,
//      not dependent ones.
class PagePool : public DependentCodeReferenced
{
private:
    Mutex mutex;

public:
    class PageListHead;

    class Page
    {
	friend class PagePool;
        friend class PageListHead;

    private:
	AtomicInt refcount;

	Page *next_pool_page;
	Page *next_msg_page;

	Page& operator = (Page const &);
	Page (Page const &);

	Page (int const refcount = 1) : refcount (refcount) {}

    public:
	Size data_len;

	Page*  getNextMsgPage () const { return next_msg_page; }
	Byte*  getData        () const { return (Byte*) this + sizeof (*this); }
        int    getRefcount    () const { return refcount.get(); }
	Memory mem            () const { return Memory (getData(), data_len); }
    };

    class PageListHead
    {
    public:
	Page *first;
	Page *last;

        void appendList (PageListHead * const mt_nonnull list)
        {
            if (list->first) {
                last->next_msg_page = list->first;
                last = list->last;
            }
        }

        void appendPages (Page * const first)
        {
            Page *last = first;
            if (last) {
                while (last->next_msg_page)
                    last = last->next_msg_page;
            }

            PageListHead pages;
            pages.first = first;
            pages.last = last;

            appendList (&pages);
        }

	bool isEmpty () const
	{
	    return first == NULL;
	}

	void reset ()
	{
	    first = NULL;
	    last = NULL;
	}

	PageListHead ()
	    : first (NULL),
	      last  (NULL)
	{
	}
    };

    class PageListArray : public Array
    {
    private:
	Page * const first_page;
        Size const first_offset;
	Size const data_len;

        // 'cached_page' and 'cached_pos' *must* hold the last accessed
        // position in the array.
	Page *cached_page;
	Size cached_pos;
        Size last_in_page_offset;

	void doGetSet (Size offset,
		       Byte       * const data_get,
		       Byte const * const data_set,
		       Size data_len,
		       bool get);

    public:
	void get (Size offset,
		  Memory const &mem);

	void set (Size offset,
		  ConstMemory const &mem);

        Page* getLastAccessedPage () const
        {
            return cached_page;
        }

        Size getLastAccessedInPageOffset () const
        {
            return last_in_page_offset;
        }

        Page* getNextPageToAccess () const
        {
            assert (cached_page);
            assert (last_in_page_offset <= cached_page->data_len);
            if (last_in_page_offset == cached_page->data_len)
                return cached_page->getNextMsgPage();

            return cached_page;
        }

        Size getNextInPageOffset () const
        {
            assert (cached_page);
            assert (last_in_page_offset <= cached_page->data_len);
            if (last_in_page_offset == cached_page->data_len)
                return 0;

            return last_in_page_offset;
        }

        // Deprecated
	PageListArray (Page * const first_page,
		       Size const data_len)
	    : first_page   (first_page),
              first_offset (0),
	      data_len     (data_len),
	      cached_page  (NULL),
	      cached_pos   (0),
              last_in_page_offset (0)
	{
	}

        PageListArray (Page * const first_page,
                       Size   const offset,
                       Size   const data_len)
            : first_page   (first_page),
              first_offset (offset),
              data_len     (data_len + offset),
              cached_page  (first_page),
              cached_pos   (0),
              last_in_page_offset (0)
        {
        }
    };

    class PageListOutputStream : public OutputStream
    {
    private:
	PagePool * const page_pool;
	PageListHead * const page_list;

    public:
      mt_iface (OutputStream)

	mt_throws Result write (ConstMemory   const mem,
				Size        * const ret_nwritten)
	{
	    page_pool->getFillPages (page_list, mem);

	    if (ret_nwritten)
		*ret_nwritten = mem.len();

	    return Result::Success;
	}

	mt_throws Result flush ()
	{
	  // No-op
	    return Result::Success;
	}

      mt_iface_end

	PageListOutputStream (PagePool     * const mt_nonnull page_pool,
			      PageListHead * const mt_nonnull page_list)
	    : page_pool (page_pool),
	      page_list (page_list)
	{
	}
    };

    struct Statistics
    {
	Count num_spare_pages;
	Count num_busy_pages;
    };

private:
    mt_const Size const page_size;
    mt_const Count min_pages;

    Count num_pages;

    Page *first_spare_page;

    mt_mutex (mutex) Page* grabPage ();

    void doGetPages (PageListHead * mt_nonnull page_list,
		     ConstMemory const &mem,
		     bool fill);

public:
    Statistics stats;

    Size getPageSize () const { return page_size; }

    void getFillPages (PageListHead * mt_nonnull page_list,
		       ConstMemory const &mem);

    void getFillPagesFromPages (PageListHead * mt_nonnull page_list,
                                Page         * mt_nonnull from_page,
                                Size          from_offset,
                                Size          from_len);

    void getPages (PageListHead * mt_nonnull page_list,
		   Size len);

    void pageRef   (Page * mt_nonnull page);
    void pageUnref (Page * mt_nonnull page);

    void msgRef   (Page *first_page);
    void msgUnref (Page *first_page);

    // printToPages() should never fail.
    template <class ...Args>
    void printToPages (PageListHead * const mt_nonnull page_list, Args const &...args)
    {
	PageListOutputStream pl_outs (this, page_list);
	pl_outs.print (args...);
    }

    void setMinPages (Count min_pages);

    PagePool (Object *coderef_container,
              Size    page_size,
	      Count   min_pages);

    ~PagePool ();

    static void dumpPages (OutputStream * mt_nonnull outs,
                           PageListHead * mt_nonnull page_list,
                           Size          first_page_offs = 0);

    static Size countPageListDataLen (Page * const first_page,
                                      Size   const msg_offset)
    {
        Size pages_data_len = 0;
        {
            PagePool::Page *cur_page = first_page;
            if (cur_page) {
                assert (msg_offset <= cur_page->data_len);
                pages_data_len += cur_page->data_len - msg_offset;
                cur_page = cur_page->getNextMsgPage ();
                while (cur_page) {
                    pages_data_len += cur_page->data_len;
                    cur_page = cur_page->getNextMsgPage ();
                }
            }
        }

        return pages_data_len;
    }

    static bool msgEqual (Page *left_page,
                          Page *right_page)
    {
        for (;;) {
            if (!left_page && !right_page)
                break;

            if (!left_page || !right_page)
                return false;

            if (left_page->data_len != right_page->data_len)
                return false;

            if (memcmp (left_page->getData(), right_page->getData(), left_page->data_len))
                return false;

            left_page  = left_page->getNextMsgPage();
            right_page = right_page->getNextMsgPage();
        }

        return true;
    }
};

}


#endif /* LIBMARY__PAGE_POOL__H__ */

