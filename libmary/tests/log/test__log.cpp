#include <libmary/libmary.h>


using namespace M;


static LogGroup libMary_logGroup_mytest ("mytest", LogLevel::All);


int main (void)
{
    libMaryInit ();

    log_ (LogLevel::Info, "Hello, World!\n");
    logD (mytest, "Starting!\n");
    logD (mytest, "Habahaba, ", "and this is just ", grab (new String ("The beginning\n")));
    logE_ ("Muhahahaha! -> ", 13, " ", 42, "\n");
}

