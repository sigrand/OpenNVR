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


#include <libmary/log.h>

#include <libmary/deferred_processor.h>


namespace M {

static LogGroup libMary_logGroup_dp ("deferred_processor", LogLevel::I);

mt_mutex (unsafe_deferred_processor->mutex) void
DeferredProcessor_Registration::rescheduleTask (DeferredProcessor_Task * const mt_nonnull task)
{
    logD (dp, _func, "0x", fmt_hex, (UintPtr) this);

    if (task->scheduled ||
	task->processing)
    {
	return;
    }

    task_list.append (task);
    task->scheduled = true;

    if (scheduled)
	return;

    // It is safe to use 'weak_deferred_processor->getUnsafePtr()' at this point,
    // because this method is called by DeferredProcessor itself.
    weak_deferred_processor.getUnsafePtr()->registration_list.append (this);
    scheduled = true;
}

void
DeferredProcessor_Registration::scheduleTask (DeferredProcessor_Task * const mt_nonnull task,
                                              bool                     const permanent)
{
    logD (dp, _func, "0x", fmt_hex, (UintPtr) this);

    CodeDepRef<DeferredProcessor> const deferred_processor = weak_deferred_processor;
    if (!deferred_processor)
        return;

    deferred_processor->mutex.lock ();

    if (task->scheduled ||
	task->processing)
    {
	assert (task->registration == this);
	deferred_processor->mutex.unlock ();
	return;
    }

    assert (!task->registration || task->registration == this);
    task->registration = this;

    task->scheduled = true;
    if (!permanent) {
	task_list.append (task);
	task->permanent = false;

	if (scheduled) {
	    deferred_processor->mutex.unlock ();
	    return;
	}

	deferred_processor->registration_list.append (this);
	scheduled = true;

	deferred_processor->mutex.unlock ();

	if (deferred_processor->backend)
	    deferred_processor->backend.call (deferred_processor->backend->trigger);
    } else {
	permanent_task_list.append (task);
	task->permanent = true;

	if (permanent_scheduled) {
	    deferred_processor->mutex.unlock ();
	    return;
	}

	deferred_processor->permanent_registration_list.append (this);
	permanent_scheduled = true;

	deferred_processor->mutex.unlock ();
    }
}

void
DeferredProcessor_Registration::revokeTask (DeferredProcessor_Task * const mt_nonnull task)
{
    CodeDepRef<DeferredProcessor> const deferred_processor = weak_deferred_processor;
    if (!deferred_processor)
        return;

    deferred_processor->mutex.lock ();

    if (task->permanent &&
	task->scheduled)
    {
	assert (permanent_scheduled);

	task->scheduled = false;

	permanent_task_list.remove (task);
	if (permanent_task_list.isEmpty ()) {
	    deferred_processor->permanent_registration_list.remove (this);
	    permanent_scheduled = false;
	}
    }

    if (task->processing) {
	assert (!task->scheduled);
	deferred_processor->processing_task_list.remove (task);
	task->processing = false;
    } else
    if (!task->permanent &&
	task->scheduled)
    {
	assert (scheduled);

	task_list.remove (task);

	task->scheduled = false;

	if (task_list.isEmpty()) {
	    deferred_processor->registration_list.remove (this);
	    scheduled = false;
	}
    }

    task->registration = NULL;

    deferred_processor->mutex.unlock ();
}

void
DeferredProcessor_Registration::release ()
{
    CodeDepRef<DeferredProcessor> const deferred_processor = weak_deferred_processor;
    if (!deferred_processor)
        return;

    deferred_processor->mutex.lock ();

    {
	DeferredProcessor_TaskList::iter iter (task_list);
	while (!task_list.iter_done (iter)) {
	    DeferredProcessor_Task * const task = task_list.iter_next (iter);
	    if (task->processing) {
		assert (!task->scheduled || task->permanent);
		deferred_processor->processing_task_list.remove (task);
		task->scheduled = false;
		task->processing = false;
		task->registration = NULL;
	    }
            // This allows for self-referencing tasks.
            task->self_ref.selfUnref ();
	}
    }
    task_list.clear ();

    {
	DeferredProcessor_PermanentTaskList::iter iter (permanent_task_list);
	while (!permanent_task_list.iter_done (iter)) {
	    DeferredProcessor_Task * const task = permanent_task_list.iter_next (iter);
	    if (task->processing) {
		assert (!task->scheduled || task->permanent);
		deferred_processor->processing_task_list.remove (task);
		task->scheduled = false;
		task->processing = false;
		task->registration = NULL;
	    }
            // This allows for self-referencing tasks.
            task->self_ref.selfUnref ();
	}
    }
    permanent_task_list.clear ();

    if (scheduled) {
	deferred_processor->registration_list.remove (this);
	scheduled = false;
    }

    if (permanent_scheduled) {
	deferred_processor->permanent_registration_list.remove (this);
	permanent_scheduled = false;
    }

    deferred_processor->mutex.unlock ();
}

bool
DeferredProcessor::process ()
{
    logD (dp, _func, "0x", fmt_hex, (UintPtr) this);

    mutex.lock ();

    assert (!processing);
    processing = true;

    {
	RegistrationList::iter reg_iter (registration_list);
	while (!registration_list.iter_done (reg_iter)) {
	    Registration * const reg = registration_list.iter_next (reg_iter);
	    assert (reg->scheduled);

	    TaskList::iter task_iter (reg->task_list);
	    while (!reg->task_list.iter_done (task_iter)) {
		Task * const task = reg->task_list.iter_next (task_iter);
		assert (!task->permanent);
		task->scheduled = false;
		task->processing = true;
	    }
	    processing_task_list.stealAppend (reg->task_list.getFirst(), reg->task_list.getLast());
	    reg->task_list.clear ();

	    registration_list.remove (reg);
	    reg->scheduled = false;
	}
    }

    {
	PermanentRegistrationList::iter reg_iter (permanent_registration_list);
	while (!permanent_registration_list.iter_done (reg_iter)) {
	    Registration * const reg = permanent_registration_list.iter_next (reg_iter);
	    assert (reg->permanent_scheduled);

	    PermanentTaskList::iter task_iter (reg->permanent_task_list);
	    while (!reg->permanent_task_list.iter_done (task_iter)) {
		Task * const task = reg->permanent_task_list.iter_next (task_iter);
		assert (task->permanent);
		task->processing = true;
		processing_task_list.append (task);
	    }
	}
    }

    bool force_extra_iteration = false;
    {
        while (!processing_task_list.isEmpty()) {
	    Task * const task = processing_task_list.getFirst();

	    processing_task_list.remove (task);
	    task->processing = false;

	    bool extra_iteration_needed = false;
	    if (task->cb.call_ret_mutex_ (mutex, &extra_iteration_needed)) {
		if (extra_iteration_needed) {
		    if (task->permanent) {
			if (task->scheduled) {
			    assert (task->registration);
			    force_extra_iteration = true;
			} else {
			    assert (!task->registration);
			}
		    } else {
			if (task->registration) {
			    task->registration->rescheduleTask (task);
                            // Not setting force_extra_iteration, because
                            // !registration_list.isEmpy() check below is enough.
                        }
		    }
#if 0
// 'self_ref' is supposed to be nullified in task callback.
                } else {
                    if (!task->permanent)
                        task->self_ref.selfUnref ();
#endif
                }
            }
	}
    }

    processing = false;

    if (force_extra_iteration ||
	!registration_list.isEmpty())
    {
	mutex.unlock ();
	logD (dp, _func, "extra iteration needed");
	return true;
    }

    mutex.unlock ();
    return false;
}

void
DeferredProcessor::trigger ()
{
    if (backend)
	backend.call (backend->trigger);
}

DeferredProcessor::DeferredProcessor (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      processing (false)
{
}

}

