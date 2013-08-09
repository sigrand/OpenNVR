#include <libmary/libmary.h>


using namespace M;


class EventSource : public Object
{
public:
    struct EventHandler
    {
	typedef void (*ManWalksCallback) (ConstMemory const &man_name,
					  Uint32 man_age,
					  void *cb_data);

	typedef void (*BirdFliesCallback) (ConstMemory const &bird_kind,
					   void *cb_data);

	ManWalksCallback manWalks;

	BirdFliesCallback birdFlies;
    };

    typedef void (*EclipseCallback) (void *cb_data);

private:
    Informer_<EventHandler> event_informer;
    Informer<EclipseCallback> eclipse_informer;

    struct InformManWalks_Data
    {
	ConstMemory const &man_name;
	Uint32 man_age;

	InformManWalks_Data (ConstMemory const &man_name,
			     Uint32 const man_age)
	    : man_name (man_name),
	      man_age (man_age)
	{
	}
    };

    static void informManWalks (EventHandler * const event_handler,
				void * const cb_data,
				void * const _inform_data)
    {
	InformManWalks_Data * const inform_data = static_cast <InformManWalks_Data*> (_inform_data);
	event_handler->manWalks (inform_data->man_name, inform_data->man_age, cb_data);
    }

    struct InformBirdFlies_Data
    {
	ConstMemory const &bird_kind;

	InformBirdFlies_Data (ConstMemory const &bird_kind)
	    : bird_kind (bird_kind)
	{
	}
    };

    static void informBirdFlies (EventHandler * const event_handler,
				 void * const cb_data,
				 void * const _inform_data)
    {
	InformBirdFlies_Data * const inform_data = static_cast <InformBirdFlies_Data*> (_inform_data);
	event_handler->birdFlies (inform_data->bird_kind, cb_data);
    }

    static void informEclipse (EclipseCallback cb,
			       void * const cb_data,
			       void * const /* inform_data */)
    {
	cb (cb_data);
    }

public:
    void fireManWalks (ConstMemory const &man_name,
		       Uint32 const man_age)
    {
	InformManWalks_Data inform_data (man_name, man_age);
	event_informer.informAll (informManWalks, &inform_data);
    }

    void fireBirdFlies (ConstMemory const &bird_kind)
    {
	InformBirdFlies_Data inform_data (bird_kind);
	event_informer.informAll (informBirdFlies, &inform_data);
    }

    void fireEclipse ()
    {
	eclipse_informer.informAll (informEclipse, NULL /* inform_cb_data */);
    }

    Informer_<EventHandler>* getEventInformer ()
    {
	return &event_informer;
    }

    Informer<EclipseCallback>* getEclipseInformer ()
    {
	return &eclipse_informer;
    }

    EventSource ()
	: event_informer   (this, &mutex),
	  eclipse_informer (this, &mutex)
    {
    }
};

static void manWalksCallback (ConstMemory   const &man_name,
			      Uint32        const  man_age,
			      void        * const /* cb_data */)
{
    logD_ (_func, man_name, ", ", man_age);
}

static void birdFliesCallback (ConstMemory   const &bird_kind,
			       void        * const /* cb_data */)
{
    logD_ (_func, bird_kind);
}

static EventSource::EventHandler my_event_handler = {
    manWalksCallback,
    birdFliesCallback
};

static void eclipseCallback (void * const /* cb_data */)
{
    logD_ (_func);
}

int main (void)
{
    libMaryInit ();

    Ref<EventSource> const event_source = grab (new EventSource);

    event_source->getEventInformer()->subscribe (
	    &my_event_handler, NULL /* cb_data */, NULL /* ref_data */, NULL /* coderef_container */);
    event_source->getEclipseInformer()->subscribe (
	    eclipseCallback, NULL /* cb_data */, NULL /* ref_data */, NULL /* coderef_container */);

    event_source->fireManWalks ("Just a man", 26);
    event_source->fireBirdFlies ("Phoenix");
    event_source->fireEclipse ();

    return 0;
}

