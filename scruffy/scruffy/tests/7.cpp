class A
{
    void m ()
    {
	int a;
    }
};

namespace Namespace_A {
    namespace Namespace_B {
	void f ();

	void g ()
	{
	}
    }
}

namespace Namespace_C {
    namespace Namespace_D {
	class F;
	class G;
    }

    namespace Namespace_D {
	class F : public A
	{
	public:
	    void f_method ()
	    {
		int i;
		while (i) {
		    f_method ();
		}
	    }
	};
    }
}

//int * const * a_;
//int * const * &a = a_;

void f (int * const * &a)
{
}

int main (void)
{
}

