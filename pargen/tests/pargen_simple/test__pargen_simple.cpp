#include <cstdlib>

#include <mycpp/mycpp.h>
#include <mycpp/util.h>
#include <mycpp/io.h>
#include <mycpp/file.h>

#include <mylang/file_token_stream.h>

#include <pargen/parser.h>

#include "test_pargen.h"

using namespace MyCpp;
using namespace MyLang;
using namespace Pargen;
using namespace MyModule;

namespace MyModule {

class LookupData : public Pargen::LookupData,
		   public virtual SimplyReferenced
{
public:
    void newCheckpoint ()
    {
	// no-op
    }

    void commitCheckpoint ()
    {
	// no-op
    }

    void cancelCheckpoint ()
    {
	// no-op
    }
};

bool
test_Word_match_func (ParserElement *parser_element,
		      void *_data)
{
    errf->print ("test_Word_match_func").pendl ();
    return true;
}

void
test_Word_accept_func (ParserElement *parser_element,
		       void *_data)
{
    errf->print ("test_Word_accept_func").pendl ();
}

}

int main (void)
{
    myCppInit ();

    Ref<File> in_file;
    Bool in_file_closed;
    try {
	in_file = File::createDefault ("test.in", 0 /* open_flags */, AccessMode::ReadOnly);

	Ref<FileTokenStream> file_token_stream = grab (new FileTokenStream (in_file));

	Ref<MyModule::LookupData > lookup_data = grab (new MyModule::LookupData);

	Ref<Grammar> grammar = create_test_grammar ();

	List< Ref<ParserElement> > out_list;
	parse (file_token_stream, lookup_data, lookup_data /* user_data */, grammar, &out_list);

	List< Ref<ParserElement> >::DataIterator grammar_iter (out_list);
	while (!grammar_iter.done ()) {
	    Ref<ParserElement> _grammar = grammar_iter.next ();
	    Test_Grammar * const &grammar = static_cast <Test_Grammar*> (_grammar.ptr ());
	    dump_test_grammar (grammar);
	}

	errf->print ("OK").pendl ();

	in_file_closed = true;
	in_file->close (true /* flush_data */);

	return 0;
    } catch (Exception &exc) {
	printException (errf, exc);

	if (!in_file.isNull () &&
	    !in_file_closed)
	try {
	    in_file->close (true /* flush_data */);
	} catch (Exception &exc) {
	    printException (errf, exc);
	}

	return EXIT_FAILURE;
    }

    abortIfReached ();
}

