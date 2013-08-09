#include <libmary/libmary.h>


using namespace M;


int main (void)
{
    libMaryInit ();
    errs->print ("Hello, World!\n");
    errs->print (toString ("Hi!\n"));
    errs->print (toString (1765)).print ("\n");
    errs->print (12.0);
    errs->print ("\n");
    errs->print ("0x").print (13, fmt_hex).print ("\n");

#if 0
    logD ();
    logI ();
    logW ();
    logE ();
    logF ();
#endif

}

