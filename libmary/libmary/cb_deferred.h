/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012 Dmitry Shatrov

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


#ifndef LIBMARY__CB_DEFERRED__H__
#define LIBMARY__CB_DEFERRED__H__


#include <libmary/cb.h>
#include <libmary/deferred_processor.h>


namespace M {

// Cb<T>::call_deferred() is mostly an excersice with variadic templates.
// It provides a shorter way to send messages using DeferredProcessor.
//
// This is not the most efficient way to send such messages, though,
// because call_deferred() foes a malloc for every invocation.
// It also involves excessive refcount update operations.

class DeferredProcessor;

// Cb_CallData expands Args into an equivalent of struct { Arg1 a1; Arg2 a2; ... };
// One can then call a function using a function pointer to which (a2, a2, ...) args
// will be passed.
template <class CB, class ...Args> class Cb_CallData;

// Using partial specialization to extract the 1st arg, because g++ 4.6
// doesn't support doing this the easy way.
template <class CB, class T, class ...Args>
class Cb_CallData <CB, T, Args...>
{
private:
    T param;
    Cb_CallData <CB, Args...> data;

public:
    template <class ...CallArgs>
    void call (Cb<CB> * const cb, CallArgs... args)
    {
        data.call (cb, args..., param);
    }

    Cb_CallData (T param,
                 Args... args)
        : param (param),
          data (args...)
    {
    }
};

template <class CB>
class Cb_CallData <CB>
{
public:
    template <class ...CallArgs>
    static void call (Cb<CB> * const cb, CallArgs... args)
    {
        cb->call_ (args...);
    }
};

template <class CB, class ...Args>
class Cb_SpecificDeferredCall : public Referenced
{
private:
    Cb<CB> cb;
    VirtRef extra_ref_data;
    Cb_CallData <CB, Args...> data;

    static bool taskCallback (void * const _self)
    {
        Cb_SpecificDeferredCall * const self = static_cast <Cb_SpecificDeferredCall*> (_self);
        self->data.call (&self->cb);
        // TODO Review self-ref logics carefully.
        //      It looks like there's a logical error (double-free possible?)
        assert (self->task.self_ref.ptr());
        self->task.self_ref.selfUnref ();
        return false /* do not reschedule */;
    }

public:
    DeferredProcessor_Task task;

    Cb_SpecificDeferredCall (CB                * const cb_func,
                             void              * const cb_data,
                             WeakCodeRef const * const weak_code_ref,
                             VirtReferenced    * const ref_data,
                             VirtReferenced    * const extra_ref_data,
                             Args... args)
        : cb (cb_func, cb_data, weak_code_ref, ref_data),
          extra_ref_data (extra_ref_data),
          data (args...)
    {
        task.cb = CbDesc<DeferredProcessor_TaskCallback> (&taskCallback,
                                                          this /* cb_data */,
                                                          NULL /* coderef_container */,
                                                          NULL /* ref_data */);
        task.self_ref = this;
    }
};

template <class T>
template <class CB, class ...Args>
void Cb<T>::call_deferred (DeferredProcessor_Registration * const mt_nonnull def_reg,
                           CB                             * const tocall,
                           VirtReferenced                 * const extra_ref_data,
                           Args                            ...args) const
{
    if (!tocall)
        return;

    Cb_SpecificDeferredCall <CB, Args...> * const deferred_call =
            new (std::nothrow) Cb_SpecificDeferredCall <CB, Args...> (tocall,
                                                                      cb_data,
                                                                      &weak_code_ref,
                                                                      ref_data.ptr(),
                                                                      extra_ref_data,
                                                                      args...);
    // Note that 'def_reg' will reset deferred_call->task.self_ref when released,
    // which will delete 'deferred_call' object.
    def_reg->scheduleTask (&deferred_call->task, false /* permanent */);
    // 'deferred_call' references itself via task.self_ref.
    deferred_call->unref();
}

#if 0
// The idea of deferred locking looks nice, but it is probably not safe enough
// because of possible use of synchronization/locking mechanisms other than M::StateMutex.

template <class CB, class ...Args>
void call_deferredIfLocked (CB tocall, Args const &...args) const
{
    if (!tocall)
        return false;

    {
        LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
        if (tlocal->state_mutex_counter > 0) {
            DEBUG (
                printf ("0x%lx %s: state_mutex_counter > 0\n", (unsigned long) this, _func_name);
            )

            call_deferred (tocall, args...);
            return;
        }
    }

    call (tocall, args...);
}
#endif

}


#endif /* LIBMARY__CB_DEFERRED__H__ */

