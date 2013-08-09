// Test for expressions

class A
{
    A ()
    {
    }
};

class B
{
    B ();
};

B::B ()
{
}

void f ()
{
    z ();

    // TODO Handle destructors properly
//    ~z ();

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

