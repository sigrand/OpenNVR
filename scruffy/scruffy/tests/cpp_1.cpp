// C++ preprocessor test

/* C-style comment */

/* Two comments */ /* on the same line */

/* Multiline
   comment */ // C++ style comment

#define A(a) a

#define B(a, b) A(a##b)

#define C B(0x, 0)

#if 0
#error Unreachable
#endif

#if (0 || defined (UNDEFINED_MACRO) || (3 * 2 - 6) / 1)
#error Unreachable
#endif

#if defined A
#if 1
#define D
#endif
#else
#define E
#endif

#if defined E
#error Unreachable
#endif

#if !defined (E) && defined D
// Ok
#else
#error Unreachable
#endif

int a = C;

// TODO: Make scruffy accept empty programs
int dummy;

