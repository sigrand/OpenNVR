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


#include <libmary/types.h>
#include <new>
#include <cstdio>

#include <libmary/log.h>
#include <libmary/util_dev.h>

#include <libmary/page_pool.h>


namespace M {

static LogGroup libMary_logGroup_pool ("pool", LogLevel::I);

void
PagePool::PageListArray::doGetSet (Size        offset,
				   Byte       * const data_get,
				   Byte const * const data_set,
				   Size const data_len,
				   bool const get)
{
    if (data_len == 0)
	return;

    assert (offset + first_offset >= offset);
    offset += first_offset;
    assert (offset + data_len >= offset);

    Page *cur_page;
    Size cur_pos;
    if (cached_page &&
	offset >= cached_pos)
    {
	cur_page = cached_page;
	cur_pos = cached_pos;
    } else {
	cur_page = first_page;
	cur_pos = 0;
    }

    Size data_pos = 0;

    for (;;) {
	assert (cur_page);
	assert (cur_pos + cur_page->data_len > cur_pos);

	if (cur_pos + cur_page->data_len <= offset) {
	    cur_pos += cur_page->data_len;
	    cur_page = cur_page->next_msg_page;
	    continue;
	}

	Size copy_from;
	if (cur_pos < offset) {
	    copy_from = offset - cur_pos;
	    assert (copy_from < cur_page->data_len);
	} else {
	    copy_from = 0;
	}

	Size copy_to;
	if (offset + data_len < cur_pos + cur_page->data_len) {
	    copy_to = offset + data_len - cur_pos;
	    assert (copy_to < cur_page->data_len);
	} else {
	    copy_to = cur_page->data_len;
	}

	assert (copy_to > copy_from);
	Size copy_len = copy_to - copy_from;

	if (get)
	    memcpy (data_get + data_pos, cur_page->getData() + copy_from, copy_len);
	else
	    memcpy (cur_page->getData() + copy_from, data_set + data_pos, copy_len);

	data_pos += copy_len;
	assert (data_pos <= data_len);
	if (data_pos == data_len) {
            // Exit point.
            cached_page = cur_page;
            cached_pos = cur_pos;
            last_in_page_offset = copy_from + copy_len;
	    break;
        }

	cur_pos += cur_page->data_len;
	cur_page = cur_page->next_msg_page;
    }
}

void
PagePool::PageListArray::get (Size const offset,
			      Memory const &mem)
{
    doGetSet (offset, mem.mem() /* data_get */, NULL /* data_set */, mem.len(), true /* get */);
}

void
PagePool::PageListArray::set (Size const offset,
			      ConstMemory const &mem)
{
    doGetSet (offset, NULL /* data_get */, mem.mem() /* data_set */, mem.len(), false /* get */);
}

mt_mutex (mutex) PagePool::Page*
PagePool::grabPage ()
{
    Page *page;
    if (!first_spare_page) {
        // Default page refcount is 1.
        page = new (new (std::nothrow) Byte [sizeof (Page) + page_size]) Page;
        assert (page);

        logD (pool, _func, "new page 0x", fmt_hex, (UintPtr) page);

        ++num_pages;
    } else {
        page = first_spare_page;
        first_spare_page = first_spare_page->next_pool_page;

        pageRef (page);

        logD (pool, _func, "spare page 0x", fmt_hex, (UintPtr) page);

        assert (stats.num_spare_pages > 0);
        --stats.num_spare_pages;
    }

    page->next_msg_page = NULL;

    ++stats.num_busy_pages;
    return page;
}

void
PagePool::doGetPages (PageListHead * const mt_nonnull page_list,
		      ConstMemory    const &mem,
		      bool           const  fill)
{
    Byte const *cur_data = mem.mem ();
    Size cur_data_len = mem.len ();

    if (page_list->last != NULL) {
	Page * const page = page_list->last;
	if (page->data_len < page_size) {
//	    logD (pool, _func, "adding data to page 0x", fmt_hex, (UintPtr) page);

	    Size tocopy = cur_data_len;
	    if (cur_data_len > page_size - page->data_len)
		tocopy = page_size - page->data_len;

	    if (fill)
		memcpy (page->getData() + page->data_len, cur_data, tocopy);

	    page->data_len += tocopy;
	    cur_data += tocopy;
	    cur_data_len -= tocopy;
	}
    }

    if (cur_data_len == 0)
        return;

    mutex.lock ();
    while (cur_data_len > 0) {
        // TODO Grab all pages with mutex locked, then unlock the mutex, then do memcpy.
        //      Don't forget about proper synchronization for memcpy.
        //
        //      It'd also be better to decide how many pages are needed and malloc them
        //      in advance before locking 'mutex' (bulk grab).
	Page * const page = grabPage ();
	{
	  // Dealing with the linked list.

	    if (!page_list->first)
		page_list->first = page;

	    if (page_list->last)
		page_list->last->next_msg_page = page;

	    page_list->last = page;
	}

	Size const tocopy = (cur_data_len <= page_size ? cur_data_len : page_size);

	if (fill)
	    memcpy (page->getData(), cur_data, tocopy);

	page->data_len = tocopy;
	cur_data += tocopy;
	cur_data_len -= tocopy;
    }
    mutex.unlock ();
}

void
PagePool::getFillPages (PageListHead * const mt_nonnull page_list,
			ConstMemory const &mem)
{
    doGetPages (page_list, mem, true /* fill */);
}

void
PagePool::getFillPagesFromPages (PageListHead * const mt_nonnull page_list,
                                 Page         * mt_nonnull from_page,
                                 Size           from_offset,
                                 Size           from_len)
{
    if (from_len == 0)
        return;

    while (from_page && from_offset >= from_page->data_len) {
        from_offset -= from_page->data_len;
        from_page = from_page->getNextMsgPage();
    }
    assert (from_page || (from_len == 0 && from_offset == 0));

    if (page_list->last != NULL) {
        Page * const page = page_list->last;
        if (page->data_len < page_size) {
            Size tocopy = page_size - page->data_len;
            if (tocopy > from_len)
                tocopy = from_len;

            PageListArray arr (from_page, 0 /* offset */, from_len);
            arr.get (from_offset, Memory (page->getData() + page->data_len, tocopy));

            page->data_len += tocopy;
            from_len -= tocopy;

            from_page = arr.getLastAccessedPage();
            from_offset = arr.getLastAccessedInPageOffset();
            if (from_offset == from_page->data_len) {
                from_page = from_page->getNextMsgPage();
                assert (from_page || from_len == 0);
                from_offset = 0;
            }
        }
    }

    if (from_len == 0)
        return;

    mutex.lock ();
    while (from_len > 0) {
        Page * const page = grabPage ();
	{
	  // Dealing with the linked list.

	    if (!page_list->first)
		page_list->first = page;

	    if (page_list->last)
		page_list->last->next_msg_page = page;

	    page_list->last = page;
	}

        Size tocopy = page_size;
        if (tocopy > from_len)
            tocopy = from_len;

        PageListArray arr (from_page, 0 /* offset */, from_len);
        arr.get (from_offset, Memory (page->getData(), tocopy));

        page->data_len = tocopy;
        from_len -= tocopy;

        from_page = arr.getLastAccessedPage();
        from_offset = arr.getLastAccessedInPageOffset();
        if (from_offset == from_page->data_len) {
            from_page = from_page->getNextMsgPage();
            assert (from_page || from_len == 0);
            from_offset = 0;
        }
    }
    mutex.unlock ();
}

void
PagePool::getPages (PageListHead * const mt_nonnull page_list,
		    Size const len)
{
    doGetPages (page_list, ConstMemory ((Byte*) NULL, len), false /* fill */);
}

void
PagePool::pageRef (Page * const mt_nonnull page) 
{
    logD (pool, _func, "0x", fmt_hex, (UintPtr) page, ": ", fmt_def, page->refcount.get());
    page->refcount.inc ();
}

void
PagePool::pageUnref (Page * const mt_nonnull page)
{
    logD (pool, _func, "0x", fmt_hex, (UintPtr) page, ": ", fmt_def, page->refcount.get());

    if (!page->refcount.decAndTest ())
	return;

    mutex.lock ();

    assert (stats.num_busy_pages > 0);
    --stats.num_busy_pages;

    if (stats.num_spare_pages < min_pages) {
	logD (pool, _func, "num_pages: ", num_pages, ", spare page 0x", fmt_hex, (UintPtr) page);

	page->next_pool_page = first_spare_page;
	first_spare_page = page;
	++stats.num_spare_pages;
    } else {
	logD (pool, _func, "num_pages: ", num_pages, ", freeing page 0x", fmt_hex, (UintPtr) page);

	assert (num_pages > 0);
	--num_pages;
	mutex.unlock ();

	page->~Page();
	delete[] (Byte*) page;

	return;
    }

    mutex.unlock ();
}

void
PagePool::msgRef (Page * const first_page)
{
    logD (pool, _func_);

    Page *cur_page = first_page;
    while (cur_page) {
	Page * const next_page = cur_page->next_msg_page;
	pageRef (cur_page);
	cur_page = next_page;
    }
}

void
PagePool::msgUnref (Page * const first_page)
{
    logD (pool, _func_);

    Page *cur_page = first_page;
    while (cur_page) {
	Page * const next_page = cur_page->next_msg_page;
        // TODO bulk unref (single lock)
	pageUnref (cur_page);
	cur_page = next_page;
    }
}

void
PagePool::setMinPages (Count const min_pages)
{
    mutex.lock ();

    this->min_pages = min_pages;

    while (stats.num_spare_pages < min_pages) {
        Page * const page =
                new (new (std::nothrow) Byte [sizeof (Page) + page_size]) Page (0);
        assert (page);

        page->next_pool_page = first_spare_page;
        first_spare_page = page;

        ++stats.num_spare_pages;
        ++num_pages;
    }

    while (stats.num_spare_pages > min_pages) {
        assert (first_spare_page && num_pages);

        Page * const page = first_spare_page;
        first_spare_page = first_spare_page->next_pool_page;

        page->~Page();
        delete[] (Byte*) page;

        assert (stats.num_spare_pages > 0);
        --stats.num_spare_pages;
        --num_pages;
    }

    mutex.unlock ();
}

PagePool::PagePool (Object * const coderef_container,
                    Size     const page_size,
		    Count    const min_pages)
    : DependentCodeReferenced (coderef_container),
      page_size (page_size),
      min_pages (min_pages),
      num_pages (min_pages),
      first_spare_page (NULL)
{
    Page *prv_page = NULL;
    for (Count i = 0; i < min_pages; ++i) {
//	fprintf (stderr, "allocating %lu bytes #%lu\n",
//		 (unsigned long) (sizeof (Page) + page_size), (unsigned long) i);

	Page * const page =
                new (new (std::nothrow) Byte [sizeof (Page) + page_size]) Page (0 /* TODO: Set to 1 for testing (should be 0) */);
	assert (page);
	if (!first_spare_page)
	    first_spare_page = page;

	if (prv_page)
	    prv_page->next_pool_page = page;

	prv_page = page;
    }
    if (prv_page)
	prv_page->next_pool_page = NULL;

    stats.num_busy_pages = 0;
    stats.num_spare_pages = min_pages;
}

PagePool::~PagePool ()
{
    mutex.lock ();

    if (stats.num_busy_pages) {
	logW_ (_func, stats.num_busy_pages, " busy pages lost");
	// Not freeing any pages (debugging mode).
	return;
    }

    Page *cur_page = first_spare_page;
    while (cur_page) {
	Page * const next_page = cur_page->next_pool_page;

	cur_page->~Page();
	delete[] (Byte*) cur_page;

	cur_page = next_page;
    }

    mutex.unlock ();
}

void
PagePool::dumpPages (OutputStream * const mt_nonnull outs,
                     PageListHead * const mt_nonnull page_list,
                     Size           const first_page_offs)
{
    Page *page = page_list->first;
    Count i = 1;
    while (page) {
        outs->print ("\nPage #", i, "\n");

        if (page == page_list->first)
            hexdump (outs, page->mem().region (first_page_offs));
        else
            hexdump (outs, page->mem());

        page = page->getNextMsgPage();
        ++i;
    }
    outs->print ("\n");
    outs->flush ();
}

}

