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


#include <libmary/exception.h>


namespace M {

_libMary_ExcWrapper exc;

void _libMary_do_exc_swap_nounref (LibMary_ThreadLocal * const tlocal)
{
    if (!tlocal->exc_free_stack.isEmpty()) {
        ExceptionBuffer * const exc_buf = tlocal->exc_free_stack.getLast();
        tlocal->exc_free_stack.remove (exc_buf);
        --tlocal->exc_free_stack_size;
        tlocal->exc_buffer.setNoRefUnref (exc_buf);
    } else {
        tlocal->exc_buffer.setNoRefUnref (
                new (std::nothrow) ExceptionBuffer (LIBMARY__EXCEPTION_BUFFER_SIZE));
        assert (tlocal->exc_buffer);
    }

    tlocal->exc = NULL;
}

void _libMary_release_exc_buf (LibMary_ThreadLocal * const tlocal,
                               ExceptionBuffer     *exc_buf)
{
    // 1. Есть буфер "с потолка";
    // 2. Ставим чистый буфер;
    // 3. Делаем reset для буфера из п.1;
    // 4. Снимаем чистый буфер и переходим к п.1,
    //    либо возвращаем его.

    if (!exc_buf->getException()) {
        if (tlocal->exc_free_stack_size < LIBMARY_EXCBUF_POOL_SIZE) {
            tlocal->exc_free_stack.append (exc_buf);
            ++tlocal->exc_free_stack_size;
        } else {
            delete exc_buf;
        }

        return;
    }

    tlocal->exc_block_stack.append (tlocal->exc_buffer);

    for (;;) {
        _libMary_do_exc_swap_nounref (tlocal);

        exc_buf->reset ();

        if (tlocal->exc_free_stack_size < LIBMARY_EXCBUF_POOL_SIZE) {
            tlocal->exc_free_stack.append (exc_buf);
            ++tlocal->exc_free_stack_size;
        } else {
            delete exc_buf;
        }

        exc_buf = tlocal->exc_buffer;
        if (!exc_buf->getException()) {
            if (tlocal->exc_free_stack_size < LIBMARY_EXCBUF_POOL_SIZE) {
                tlocal->exc_free_stack.append (exc_buf);
                ++tlocal->exc_free_stack_size;
            } else {
                delete exc_buf;
            }

            break;
        }
    }

    ExceptionBuffer * const old_buf = tlocal->exc_block_stack.getLast();
    tlocal->exc_block_stack.remove (old_buf);
    tlocal->exc_buffer.setNoRefUnref (old_buf);
    tlocal->exc = old_buf->getException();
}

Ref<ExceptionBuffer> exc_swap ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ExceptionBuffer * const exc_buf = tlocal->exc_buffer;
    _libMary_do_exc_swap_nounref (tlocal);
    return Ref<ExceptionBuffer>::createNoRef (exc_buf);
}

ExceptionBuffer* exc_swap_nounref ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ExceptionBuffer * const exc_buf = tlocal->exc_buffer;
    _libMary_do_exc_swap_nounref (tlocal);
    return exc_buf;
}

void exc_set (ExceptionBuffer * const exc_buf)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ExceptionBuffer * const prv_exc_buf = tlocal->exc_buffer;
    tlocal->exc_buffer.setNoUnref (exc_buf);
    tlocal->exc = exc_buf->getException();
    _libMary_release_exc_buf (tlocal, prv_exc_buf);
}

void exc_set_noref (ExceptionBuffer * const exc_buf)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ExceptionBuffer * const prv_exc_buf = tlocal->exc_buffer;
    tlocal->exc_buffer.setNoRefUnref (exc_buf);
    tlocal->exc = exc_buf->getException();
    _libMary_release_exc_buf (tlocal, prv_exc_buf);
}

void exc_delete (ExceptionBuffer * const exc_buf)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    _libMary_release_exc_buf (tlocal, exc_buf);
}

void exc_push_scope ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    tlocal->exc_block_stack.append (tlocal->exc_buffer);
    _libMary_do_exc_swap_nounref (tlocal);
}

void exc_pop_scope ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ExceptionBuffer * const exc_buf = tlocal->exc_buffer;

    if (!tlocal->exc_block_stack.isEmpty()) {
        ExceptionBuffer * const old_buf = tlocal->exc_block_stack.getLast();
        tlocal->exc_block_stack.remove (old_buf);
        tlocal->exc_buffer.setNoRefUnref (old_buf);
        tlocal->exc = old_buf->getException ();
    } else {
        _libMary_do_exc_swap_nounref (tlocal);
    }

    _libMary_release_exc_buf (tlocal, exc_buf);
}

void exc_none ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ExceptionBuffer * const exc_buf = tlocal->exc_buffer;
    _libMary_do_exc_swap_nounref (tlocal);
    _libMary_release_exc_buf (tlocal, exc_buf);
}

}

