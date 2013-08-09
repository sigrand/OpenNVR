class A
{
public:
//#if 0
    class B
    {
    public:
	class C
	{
	};
    };

    B::C f ()
    {
	int z;
    }
//#endif

#if 0
    void g ()
    {
	int a;
	4 + 3;
    }
#endif
};

#if 0
void z ()
{
    // FIXME
//    long long unsigned c;
    unsigned long c;
}
#endif

