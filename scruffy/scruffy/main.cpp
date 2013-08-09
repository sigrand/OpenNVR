/*  Scruffy - C/C++ parser and source code analyzer
    Copyright (C) 2011 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <cstdlib>
#include <cstdio>

#include <mycpp/mycpp.h>
#include <mycpp/file.h>
#include <mycpp/cached_file.h>
#include <mycpp/io.h>

#include <pargen/parser.h>

#include <scruffy/preprocessor.h>
#include <scruffy/preprocessor_util.h>
#include <scruffy/list_pp_item_stream.h>
#include <scruffy/list_token_stream.h>
//#include <scruffy/pp_item_stream_token_stream.h>
//#include <scruffy/name_tracker.h>
//#include <scruffy/test_dispatcher.h>
#include <scruffy/cpp_parser.h>
#include <scruffy/dump_context.h>

#include "cpp_pargen.h"


#define SCRUFFY__CATCH_EXCEPTIONS

//#define SCRUFFY__DEBUG_DUMP

#define SCRUFFY__AST
#define SCRUFFY__AST_DUMP
#define SCRUFFY__RESULT_DUMP


using namespace MyCpp;
using namespace MyLang;
using namespace Pargen;
using namespace Scruffy;

static bool
scruffy_do_test (char const * const filename)
{
    Ref<File> file;
    Bool file_closed;
#ifdef SCRUFFY__CATCH_EXCEPTIONS
    try {
#endif
	Ref<Grammar> grammar = create_cpp_grammar ();

//	errf->print ("--- BEGIN OPTIMIZING ---\n").pendl ();
	optimizeGrammar (grammar);
//	errf->print ("--- END OPTIMIZING ---\n").pendl ();
	// TEST
//	return 0;

	fprintf (stderr, "+");

	file = File::createDefault (filename,
				    0 /* open_flags */,
				    AccessMode::ReadOnly);
	// TEST
	file = grab (new CachedFile (file, 4096 /* page_size */, 256 /* max_pages */));

	Ref<CppPreprocessor> preprocessor = grab (new CppPreprocessor (file));
	preprocessor->performPreprocessing ();
	Ref< List_< Ref<PpItem>, SimplyReferenced > > pp_items = preprocessor->getPpItems ();
	fprintf (stderr, "!");

// Preprocessor output dump.
	errf->print (spellPpItems (pp_items.ptr ()));

	List< Ref<Token> > token_list;
	ppItemsToTokens (pp_items, &token_list);
	fprintf (stderr, "~");

	ParserElement *cpp_element = NULL;
	for (Size i = 0; i < 1 /* 80 */; i++) {
	    // TEST
    //	errf->print (ppItemsToString (pp_items.ptr ())).pendl ();

    //	Ref<PpItemStream> pp_stream =
    //		grab (static_cast <PpItemStream*> (
    //			new ListPpItemStream (pp_items->first, FilePosition ())));
	    Ref<TokenStream> token_stream =
    //		grab (static_cast <TokenStream*> (
    //			new PpItemStreamTokenStream (pp_stream)));
		    grab (new ListTokenStream (&token_list));

    //	errf->print ("Parsing...").pendl ();

#if 0
	    Ref<Grammar> grammar = create_test_grammar ();
	    Ref<TestLookupData> lookup_data = grab (new TestLookupData);
	    List< Ref<ParserElement> > out_list;
	    parse (token_stream, lookup_data, grammar, &out_list);
	    errf->print ("out_list.getNumElements(): ").print (out_list.getNumElements ()).pendl ();
#endif

	    Ref<CppParser> cpp_parser =
#ifdef SCRUFFY__AST
		    grab (new CppParser (ConstMemoryDesc::forString ("AST")));
#else
		    grab (new CppParser (ConstMemoryDesc::forString ("default")));
#endif

	    cpp_element = NULL;
	    parse (token_stream,
		   cpp_parser->getLookupData (),
		   cpp_parser->getLookupData () /* user_data */,
		   grammar,
		   &cpp_element,
#ifdef SCRUFFY__AST
		   ConstMemoryDesc::forString ("AST"),
		   Pargen::createParserConfig (false) /* upwards_jumps */,
#else
		   ConstMemoryDesc::forString ("default"),
		   Pargen::createParserConfig (true) /* upwards_jumps */,
#endif
#ifdef SCRUFFY__DEBUG_DUMP
		   true  /* debug_dump */
#else
		   false /* debug_dump */
#endif
		   );

	    if (cpp_element == NULL ||
		token_stream->getNextToken ().getLength () > 0)
	    {
		throw Scruffy::ParsingException (token_stream->getFilePosition (),
						 String::forData ("EOF not reached"));
	    }

	    fprintf (stderr, "*");

	    errf->print ("\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n").pendl ();

#ifdef SCRUFFY__AST_DUMP
	    dump_cpp_grammar (static_cast <Cpp_Grammar*> (cpp_element));
	    errf->pflush ();
#endif

    //	errf->print ("OK").pendl ();

	    errf->print ("\n==============================\n").pendl ();

#ifdef SCRUFFY__RESULT_DUMP
	    {
		Cpp::DumpContext dump_ctx (outf, 4 /* tab_size */);
    //	    dump_ctx.dumpNamespace (cpp_parser->getRootNamespace ());
		dump_ctx.dumpTranslationUnit (cpp_parser->getRootNamespace ());
		errf->print ("\n");
		outf->oflush ();
	    }
#endif
	}

#if 0
	errf->print ("\n==============================\n").pendl ();

	{
	    Cpp::DumpContext dump_ctx (outf, 4 /* tab_size */);
	    dump_ctx.dumpNamespace (cpp_parser->getRootNamespace ());
	    outf->oflush ();
	}
#endif

	file_closed = true;
	file->close (true /* flush_data */);
#ifdef SCRUFFY__CATCH_EXCEPTIONS
   } catch (Exception &exc) {
	printException (errf, exc);

	if (!file.isNull () &&
	    !file_closed)
	{
	    file_closed = true;
	    try {
		file->close (true /* flush_data */);
	    } catch (Exception &exc) {
		printException (errf, exc);
	    }
	}

	return false;
    }
#endif // SCRUFFY__CATCH_EXCEPTIONS

    return true;
}

int main (int argc, char **argv)
{
    myCppInit ();

    if (argc < 2) {
	errf->print ("Not enough arguments").pendl ();
	return EXIT_FAILURE;
    }

    const char * const filename = argv [1];

    for (Size i = 0; i < 1 /* 50 */; i++) {
	if (!scruffy_do_test (filename))
	    return EXIT_FAILURE;

	fprintf (stderr, ".");
    }

    return 0;
}

