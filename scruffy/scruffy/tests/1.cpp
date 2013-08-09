// Test for various global variable declarations

#if 0
// TODO Handle symbols with the same name properly.
//      Currently, only the first symbol appears in the output,
//      and the others are silently dropped.
//
//      This should be handled in CppParser_Impl::acceptDeclaration().
int A;
int *A;
#endif

// TEST (uncomment)
//#if 0
int  a;
int *b;
int &c;
int const d;
const int (*e);
const double f ();
//#endif

// FIXME Parameter @f is parsed incorrectly
long double (*g) (int a, void (*f ()) ()) const /* FIXME This 'const' is only for class methods, right? */;

void z (void (*g) ()) const;

// FIXME Parsed incorrectly
void (*y ()) ();

