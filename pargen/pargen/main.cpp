/*  Pargen - Flexible parser generator
    Copyright (C) 2011-2013 Dmitry Shatrov

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

#include <libmary/libmary.h>

#include <pargen/file_token_stream.h>

#include <pargen/util.h>
#include <pargen/pargen_task_parser.h>
#include <pargen/header_compiler.h>
#include <pargen/source_compiler.h>


#define DEBUG(a) a
#define DEBUG_OLD(a) ;

#define FUNC_NAME(a) a


using namespace M;
using namespace Pargen;

namespace {
struct Options
{
    StRef<String> module_name;
    StRef<String> namespace_name;
    StRef<String> header_name;

    Bool extmode;

    Bool help;
};
}

static Options options;

static void
print_usage ()
{
    errs->println ("Usage: pargen [options] <file>\n"
                   "Options:\n"
                   "  --module-name\n"
                   "  --namespace\n"
                   "  --header-name\n"
                   "  --extmode\n"
                   "  -h, --help");
}

static bool
cmdline_module_name (const char * /* short_name */,
		     const char * /* long_name */,
		     const char *value,
		     void * /* opt_data */,
		     void * /* callback_data */)
{
    options.module_name = st_grab (new (std::nothrow) String (value));
    return true;
}

static bool
cmdline_namespace_name (const char * /* short_name */,
                        const char * /* long_name */,
                        const char *value,
                        void       * /* opt_data */,
                        void       * /* callback_data */)
{
    options.namespace_name = st_grab (new (std::nothrow) String (value));
    return true;
}

static bool
cmdline_header_name (const char * /* short_name */,
		     const char * /* long_name */,
		     const char *value,
		     void       * /* opt_data */,
		     void       * /* callback_data */)
{
    options.header_name = st_grab (new (std::nothrow) String (value));
    return true;
}

static bool
cmdline_help (const char * /* short_name */,
	      const char * /* long_name */,
	      const char * /* value */,
	      void       * /* opt_data */,
	      void       * /* callback_data */)
{
    options.help = true;
    return true;
}

static bool
cmdline_extmode (const char * /* short_name */,
		 const char * /* long_name */,
		 const char * /* value */,
		 void       * /* opt_data */,
		 void       * /* callback_data */)
{
    options.extmode = true;
    return true;
}

int main (int argc, char **argv)
{
    libMaryInit ();

    {
	const Size num_opts = 5;
	CmdlineOption opts [num_opts];

	opts [0].short_name = NULL;
	opts [0].long_name  = "module-name";
	opts [0].with_value = true;
	opts [0].opt_data   = NULL;
	opts [0].opt_callback = cmdline_module_name;

        opts [1].short_name = NULL;
        opts [1].long_name  = "namespace";
        opts [1].with_value = true;
        opts [1].opt_data   = NULL;
        opts [1].opt_callback = cmdline_namespace_name;

	opts [2].short_name = NULL;
	opts [2].long_name  = "header-name";
	opts [2].with_value = true;
	opts [2].opt_data   = NULL;
	opts [2].opt_callback = cmdline_header_name;

	opts [3].short_name = "h";
	opts [3].long_name  = "help";
	opts [3].with_value = false;
	opts [3].opt_data   = NULL;
	opts [3].opt_callback = cmdline_help;

	opts [4].short_name = NULL;
	opts [4].long_name  = "extmode";
	opts [4].with_value = false;
	opts [4].opt_data   = NULL;
	opts [4].opt_callback = cmdline_extmode;

	ArrayIterator<CmdlineOption> opts_iter (opts, num_opts);
	parseCmdline (&argc, &argv, opts_iter,
		      NULL /* callback */,
		      NULL /* callbackData */);
    }

    if (options.help) {
	print_usage ();
	return 0;
    }

    if (argc != 2)
    {
	print_usage ();
	return EXIT_FAILURE;
    }

    ConstMemory const input_filename (argv [1], strlen (argv [1]));

    if (argc < 2) {
        errs->println ("File not specified");
        return EXIT_FAILURE;
    }

    if (!options.module_name) {
        errs->println ("Module name not specified");
        return EXIT_FAILURE;
    }

    if (!options.namespace_name)
        options.namespace_name = options.module_name;

    if (!options.header_name) {
        errs->println ("Header name not specified");
        return EXIT_FAILURE;
    }

    StRef<String> const header_filename = st_makeString (options.header_name, "_pargen.h");
    StRef<String> const source_filename = st_makeString (options.header_name, "_pargen.cpp");

    StRef<CompilationOptions> const comp_opts = st_grab (new (std::nothrow) CompilationOptions);

    comp_opts->module_name = options.module_name;
    comp_opts->capital_module_name =
            options.extmode ? comp_opts->module_name :
                    capitalizeName (comp_opts->module_name->mem(),
                                    false /* keep_underscore */);
    comp_opts->all_caps_module_name = capitalizeNameAllCaps (comp_opts->module_name->mem());

    comp_opts->capital_namespace_name =
            options.extmode ? options.namespace_name :
                    capitalizeName (options.namespace_name->mem(),
                                    false /* keep_underscore */);

    comp_opts->header_name = options.header_name;
    comp_opts->capital_header_name =
            options.extmode ? comp_opts->module_name :
                    capitalizeName (comp_opts->header_name->mem(),
                                    false /* keep_underscore */);
    comp_opts->all_caps_header_name = capitalizeNameAllCaps (comp_opts->header_name->mem());

    NativeFile file;
    if (!file.open (input_filename, 0 /* open_flags */, FileAccessMode::ReadOnly)) {
        errs->println ("Could not open ", input_filename, ": ", exc->toString());
        return EXIT_FAILURE;
    }

    FileTokenStream file_token_stream (&file,
                                       true /* report_newlines */,
                                       true /* minus_is_alpha */);

    StRef<PargenTask> pargen_task;
    if (!parsePargenTask (&file_token_stream, &pargen_task)) {
        errs->println ("Parsing error: ", exc->toString());
        return EXIT_FAILURE;
    }

    DEBUG_OLD (
      dumpDeclarations (pargen_task);
    )

    file.close (true /* flush_data */);

    NativeFile header_file;
    if (!header_file.open (header_filename->mem(),
                           FileOpenFlags::Create | FileOpenFlags::Truncate,
                           FileAccessMode::ReadWrite))
    {
        errs->println ("Could not open ", header_filename, ": ", exc->toString());
        return EXIT_FAILURE;
    }

    if (!compileHeader (&header_file, pargen_task, comp_opts)) {
        errs->println ("Header file generation error: ", exc->toString());
        return EXIT_FAILURE;
    }

    header_file.close (true /* flush_data */);

    NativeFile source_file;
    if (!source_file.open (source_filename->mem(),
                           FileOpenFlags::Create | FileOpenFlags::Truncate,
                           FileAccessMode::ReadWrite))
    {
        errs->println ("Could not open ", source_filename, ": ", exc->toString());
        return EXIT_FAILURE;
    }

    if (!compileSource (&source_file, pargen_task, comp_opts)) {
        errs->println ("Source file generation error: ", exc->toString());
        return EXIT_FAILURE;
    }

    source_file.close (true /* flush_data */);

    return 0;

#if 0
    } catch (ParsingException &exc) {
	abortIf (exc.fpos.char_pos < exc.fpos.line_pos);
	errf->print ("Parsing exception "
		     "at line ").print (exc.fpos.line + 1).print (", "
		     "character ").print ((exc.fpos.char_pos - exc.fpos.line_pos) + 1)
	     .pendl ()
	     .pendl ();

	printErrorContext (errf, file, exc.fpos);

	errf->pendl ();
	printException (errf, exc);
    }
#endif
}

