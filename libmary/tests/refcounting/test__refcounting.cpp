#include <cstdio>

#include <libmary/libmary.h>


using namespace M;


class BasicA : public BasicReferenced
{
public:
    BasicA ()
    {
	printf ("BasicA()\n");
    }

    ~BasicA ()
    {
	printf ("~BasicA()\n");
    }
};

class A : public Object
{
public:
    void test_unref ()
    {
      StateMutexLock l (&mutex);

	this->unref ();
    }

    A ()
    {
	printf ("0x%lx A()\n", (unsigned long) (Object*) this);
    }

    ~A ()
    {
	printf ("0x%lx ~A()\n", (unsigned long) (Object*) this);
    }
};

class ADeletionCallback_Data : public Referenced
{
public:
    int key;

    ADeletionCallback_Data ()
	: key (13)
    {
    }
};

static void
a_deletion_callback (void * const _data)
{
    static char const * const _func_name = "a_deletion_callback";

    ADeletionCallback_Data * const data = static_cast <ADeletionCallback_Data*> (_data);

    printf ("%s: key: %d\n", _func_name, (int) data->key);
}

int main (void)
{
    static char const * const _func_name = "main";

    libMaryInit ();

    Ref<BasicA> basic_a = grab (new BasicA);

    Ref<A> a = grab (new A);

    printf ("%s: adding deletion callback\n", _func_name);
    {
	Ref<Object> guard_obj = grab (new Object);
	Ref<ADeletionCallback_Data> const data = grab (new ADeletionCallback_Data);
	a->addDeletionCallback (a_deletion_callback, data, data, guard_obj);
	a->addDeletionCallback (a_deletion_callback, data, data, NULL /* guard_obj */);
	printf ("%s: refcount: %lu\n", _func_name, (unsigned long) a->getRefCount ());
    }
    printf ("%s: refcount: %lu\n", _func_name, (unsigned long) a->getRefCount ());

    // Testing weak references.
    printf ("%s: testing weak references\n", _func_name);
    {
	WeakRef<A> weak_a = &*a;

	printf ("%s: refcount #w1: %lu\n", _func_name, (unsigned long) a->getRefCount ());

	assert (weak_a.isValid ());

	Ref<A> real_a = weak_a.getRef ();

	printf ("%s: refcount #w2: %lu\n", _func_name, (unsigned long) a->getRefCount ());

	weak_a = NULL;
	assert (!weak_a.isValid ());
    }
    printf ("%s: refcount #wE: %lu\n", _func_name, (unsigned long) a->getRefCount ());

    printf ("%s: testing deletion queue\n", _func_name);
    {
	A * const tmp_a = new A;
	tmp_a->test_unref ();
    }

    printf ("%s: done\n", _func_name);
}

