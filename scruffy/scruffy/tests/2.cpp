// Test for function bodies

void g ()
{
    // TODO Handle symbols with the same name properly.
    //      Interestingly, all these 'a's make it into the output.
    int  a;
#if 0
    int *a;
    int &a;
    int const a;
    const int (*a);
    const double a ();
#endif
}

