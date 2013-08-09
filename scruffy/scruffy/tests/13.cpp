#if 0
namespace A {
    template <class T> f (/* T t */);

    namespace B {
	template <class C, int a, double b> g ();
    }

    template < template <class C> class D, class G > m ();
}

template < template <class C> class D > void z ();
#endif

#if 0
template <class C> void f ()
{
}

template <class C, int a> void f2 ()
{
}

void m ()
{
    f <int> ();
    f2 <long double*, 10 + 1> ();
}
#endif

template <class T> f ();

void m ()
{
    static_cast < int* > (1);

    f < int * const * > ();
}

