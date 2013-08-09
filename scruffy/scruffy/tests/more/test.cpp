class Z
{
    void f ();

#if 0
    class B
    {
	int a;
	int b;

	class C
	{
	    class D
	    {
	    };
	};
    };
#endif
};

void
Z::f ()
{
}

#if 0
class A : public Z
{
};
#endif

#if 0
namespace X
{
    class B;

    namespace Z {
	class C1;
	class C2;
    }
}
#endif

#if 0
class A;

class B
{
};

class A : public B
{
    int a;

    class Z {};
#if 0
    class C {
	double d;
    };
#endif

    void f ()
    {
	int a;
    }
};

void A::f ()
{
}

void f ()
{
    a ++;
}
#endif

#if 0
class A
{
};
#endif

#if 0
void f ()
{
    for (int a = 0; int b = 1; i++)
	a *= 2;

    while (a + 6);

    while  (1) {
    }

    do; while (1);

    do
	int a;
    while (a ++);

    do {
	i ++;
	++ z;
    } while (p ^ c);
}
#endif

#if 0
int z = 123 + 13 + b * 2, b;

#if 0
class A
{
    A ()
    {
    }

    A ()
    {
    }

    void f ()
    {
    }
};
#endif

#if 0
void f ()
{
}
#endif

#if 0
// FIXME statement_acceptors.isEmpty()
void A::f ()
{
}
#endif

#if 0
// FIXME statement_acceptors.isEmpty()
A::A ()
{
}
#endif

void k (const int &f = 123)
{
    sizeof (int (*a) (b));
    f (c, z) (a);

    if (a + 3 * 4) {
	f (c, z) (a);
	a ++;
	++ c;
    } else {
	b --;
    }

    if (int a = ++ i) {
    }
}

//#if 0
void f (int a, int b)
{
    // FIXME This should be treated as an expression (!).
    f ();

    // FIXME This should be treated as an expression (!).
    f (int (a));

    long int *&a;
    long double lf = 1;
    double long fm = lf + *a;

    dynamic_cast <int> (a);
// FIXME temporal namespace + this is not a specifier
//    static_cast <class Foo> (b ++);

    "a";

    a;

    a [0];

    f (a);

    int (a);

    a.b;
    a->b;

    a ++;
    a --;

    dynamic_cast <int*> (0);
    static_cast <int> (static_cast <char> (0));
    reinterpret_cast <char (*) (int)> (static_cast <int &> (a));
    const_cast <int const &> (b);

    typeid (a ++);
    typeid (int (*) (double long &a));

//#if 0
// OK
    ++ a;
    -- a;
    *a;
    &a;
    +a;
    -a;
    !a;
    ~a;

    sizeof (a);
    sizeof (int const *&);

    (int) 0;

    a * b;
    a / b;
    a % b;

    a + b;
    a - b;

    a << b;
    a >> b;

    a < b;
    a > b;
    a <= b;
    a >= b;

    a == b;
    a != b;

    a & b;
    a ^ b;
    a | b;

    a && b;
    a || b;

    a = b;
    a *= b;
    a /= b;
    a %= b;
    a += b;
    a -= b;
    a >>= b;
    a <<= b;
    a &= b;
    a ^= b;
    a |= b;

    a, b;
    a, b, c;

    throw a ++;
//#endif
}
//#endif

#endif
