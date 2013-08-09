#include <libmary/libmary.h>


using namespace M;

class MyException : public Exception
{
public:
    Ref<String> toString ()
    {
        if (cause)
            return makeString ("MyException 0x", fmt_hex, (UintPtr) this, ": ", cause->toString()->mem());
        else
            return makeString ("MyException 0x", fmt_hex, (UintPtr) this);
    }

    MyException ()
    {
        errs->println (_func, "0x", fmt_hex, (UintPtr) this);
    }

    ~MyException ()
    {
        errs->println (_func, "0x", fmt_hex, (UintPtr) this);
    }
};

static mt_throws Result test ()
{
    exc_throw <MyException> ();
    return Result::Failure;
}

static void excPushScopeTest ()
{
    errs->println ("--- begin exc_pus_scope test ---");
    {
        exc_throw <MyException> ();
        errs->println ("A exc: 0x", fmt_hex, (UintPtr) (Exception*) exc);

        {
            errs->println ("exc_push_scope");
            exc_push_scope ();
            errs->println ("exc: 0x", fmt_hex, (UintPtr) (Exception*) exc);

            errs->println ("exc_throw");
            exc_throw <MyException> ();

            errs->println ("exc_push");
            exc_push <MyException> ();

            errs->println ("exc_none");
            exc_none ();

            errs->println ("exc_throw");
            exc_throw <MyException> ();

            errs->println ("exc_push");
            exc_push <MyException> ();

            errs->println ("exc: 0x", fmt_hex, (UintPtr) (Exception*) exc);
            errs->println ("exception: ", exc->toString());

            {
                errs->println ("exc_push_scope");
                exc_push_scope ();

                errs->println ("exc_pop_scope");
                exc_pop_scope ();
            }

            errs->println ("exc: 0x", fmt_hex, (UintPtr) (Exception*) exc);
            errs->println ("exception: ", exc->toString());

            errs->println ("exc_pop_scope");
            exc_pop_scope ();
        }

        errs->println ("A exc: 0x", fmt_hex, (UintPtr) (Exception*) exc);

        errs->println ("exc_none");
        exc_none ();
        errs->println ("exc: 0x", fmt_hex, (UintPtr) (Exception*) exc);
    }
    errs->println ("--- end exc_push_scope test ---");
}

int main (void)
{
    libMaryInit ();

    errs->println ("Hello, World!");

    {
        NativeFile file ("my_test_file", 0 /* open_flags */, File::AccessMode::WriteOnly);
        if (exc)
            logs->println ("Open: ", exc->toString());

        if (!file.println ("TEST OUTPUT"))
            logs->println ("Print: ", exc->toString());

        if (!file.close ())
            logs->println ("Close: ", exc->toString ());
    }

    if (!test ()) {
        logs->println ("test: ", exc->toString());
        exc_none ();
    }

    excPushScopeTest ();

    return 0;
}

