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


#ifndef LIBMARY__TIMERS__H__
#define LIBMARY__TIMERS__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>
#include <libmary/intrusive_avl_tree.h>
#include <libmary/code_referenced.h>
#include <libmary/cb.h>
#include <libmary/mutex.h>
#include <libmary/util_time.h>

namespace M {

// Алгоритм работы таймеров:
// 1. Таймеры группируются по периоду срабатывания;
// 2. Группы таймеров сортируются AVL-деревом;
// 3. После срабатывания таймера переставляем группу по следующему таймеру
//    с тем же интервалом;
// 4. Два типа таймеров: одноразовые и периодические.
//
// Таймеры должны быть MT-safe.

class Timers : public DependentCodeReferenced
{
private:
    Mutex mutex;

    class Timer;
    class TimerChain;

public:
    class TimerKey
    {
        friend class Timers;
    private:
        Timer *timer;
        Timer* operator -> () const { return timer; }
        operator Timer* () const { return timer; }
    public:
        operator bool () const { return timer; }
        TimerKey (Timer * const timer) : timer (timer) {}
        TimerKey () : timer (NULL) {}
    };

    typedef void (TimerCallback) (void *cb_data);

    typedef void (FirstTimerAddedCallback) (void *cb_data);

private:
    class Timer : public IntrusiveListElement<>
    {
    public:
        mt_const Timers * const timers;
        mt_const Object::DeletionSubscriptionKey del_sbn;

	mt_const bool periodical;
        mt_const bool delete_after_tick;
	mt_const Cb<TimerCallback> timer_cb;
	mt_const TimerChain *chain;

	mt_mutex (Timers::mutex) Time due_time;
	mt_mutex (Timers::mutex) bool active;

	Timer (Timers * const timers,
               CbDesc<TimerCallback> const &timer_cb)
	    : timers (timers),
              timer_cb (timer_cb),
	      active (true)
	{
	}
    };

    class IntervalTree_name;
    class ExpirationTree_name;
    class ChainCleanupList_name;

    // A chain of timers with the same expiration interval.
    class TimerChain : public IntrusiveAvlTree_Node<IntervalTree_name>,
		       public IntrusiveAvlTree_Node<ExpirationTree_name>,
                       public IntrusiveListElement<ChainCleanupList_name>
    {
    public:
	mt_const Time interval_microseconds;

	mt_mutex (Timers::mutex) IntrusiveList<Timer> timer_list;
	// Nearest expiration time.
	mt_mutex (Timers::mutex) Time nearest_time;
    };

    typedef IntrusiveAvlTree< TimerChain,
			      MemberExtractor< TimerChain const,
					       Time const,
					       &TimerChain::interval_microseconds >,
			      DirectComparator<Time>,
			      IntervalTree_name >
	    IntervalTree;

    typedef IntrusiveAvlTree< TimerChain,
			      MemberExtractor< TimerChain const,
					       Time const,
					       &TimerChain::nearest_time >,
			      DirectComparator<Time>,
			      ExpirationTree_name >
	    ExpirationTree;

    typedef IntrusiveList< TimerChain, ChainCleanupList_name > ChainCleanupList;

    // Chains sorted by interval_microseconds.
    mt_mutex (mutex) IntervalTree interval_tree;
    // Chains sorted by nearest_time.
    mt_mutex (mutex) ExpirationTree expiration_tree;
    mt_mutex (mutex) TimerChain *expiration_tree_leftmost;

    mt_const Cb<FirstTimerAddedCallback> first_added_cb;

    static void subscriberDeletionCallback (void *_timer);

public:
    // Every call to addTimer() must be matched with a call to deleteTimer().
    TimerKey addTimer (TimerCallback * const cb,
		       void          * const cb_data,
		       Object        * const coderef_container,
		       Time            const time_seconds,
		       bool            const periodical = false)
    {
	return addTimer_microseconds (CbDesc<TimerCallback> (cb, cb_data, coderef_container),
				      time_seconds * 1000000,
				      periodical);
    }

    // Every call to addTimer_microseconds() must be matched with a call to deleteTimer().
    TimerKey addTimer (CbDesc<TimerCallback> const &cb,
		       Time const time_seconds,
		       bool const periodical,
                       bool const auto_delete = true)
    {
	return addTimer_microseconds (cb, time_seconds * 1000000, periodical, auto_delete);
    }

    // Every call to addTimer() must be matched with a call to deleteTimer().
    TimerKey addTimer_microseconds (TimerCallback * const cb,
				    void          * const cb_data,
				    Object        * const coderef_container,
				    Time            const time_microseconds,
				    bool            const periodical)
    {
	return addTimer_microseconds (CbDesc<TimerCallback> (cb, cb_data, coderef_container),
				      time_microseconds,
				      periodical);
    }

    // Every call to addTimer_microseconds() must be matched with a call to deleteTimer().
    TimerKey addTimer_microseconds (CbDesc<TimerCallback> const &cb,
				    Time time_microseconds,
				    bool periodical,
                                    bool auto_delete = true,
                                    bool delete_after_tick = false /* For non-periodical timers only */);

    // FIXME restartTimer() has problematic semantics and should be removed.
    //       In particular, there's no way for the client to guarantee that restartTimer()
    //       is called only before the timer expires, i.e. there's always a risk of hitting
    //       the assertion in restartTimer().
    //
    // A non-periodical timer can be restarted only if it has not yet expired.
    void restartTimer (TimerKey mt_nonnull timer_key);

    void deleteTimer (TimerKey mt_nonnull timer_key);

private:
    void doDeleteTimer (Timer * mt_nonnull timer);

public:
    Time getSleepTime_microseconds ();

    void processTimers ();

    // @cb is called whenever a new timer appears at the head of timer chain,
    // i.e. when the nearest expiration time changes.
    mt_const void setFirstTimerAddedCallback (CbDesc<FirstTimerAddedCallback> const &cb)
    {
	first_added_cb = cb;
    }

    Timers (Object *coderef_container);

    // @cb is called whenever a new timer appears at the head of timer chain,
    // i.e. when the nearest expiration time changes.
     Timers (Object *coderef_container,
             CbDesc<FirstTimerAddedCallback> const &first_added_cb);
    ~Timers ();
};

}


#endif /* LIBMARY__TIMERS__H__ */

