/*  Moment Video Server - High performance media server
    Copyright (C) 2011, 2012 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <libmary/libmary.h>
#include <cstring>

#include <libmary/module_init.h>
#include <moment/libmoment.h>

#ifdef MOMENT_CTEMPLATE
#include <ctemplate/template.h>
#endif


// TODO These header macros are the same as in rtmpt_server.cpp
#define MOMENT_FILE__HEADERS_DATE \
	Byte date_buf [unixtimeToString_BufSize]; \
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_FILE__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: Keep-Alive\r\n"
//	"Cache-Control: max-age=604800\r\n"
//	"Cache-Control: public\r\n"
//	"Cache-Control: no-cache\r\n"

#define MOMENT_FILE__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__304_HEADERS \
	"HTTP/1.1 304 Not Modified\r\n" \
	MOMENT_FILE__COMMON_HEADERS

#define MOMENT_FILE__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not Found\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__400_HEADERS(content_length) \
	"HTTP/1.1 400 Bad Request\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FILE__500_HEADERS(content_length) \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	MOMENT_FILE__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"


using namespace M;

namespace Moment {

namespace {

class PathEntry : public BasicReferenced
{
public:
    Ref<String> path;
    Ref<String> prefix;

    MConfig::Varlist varlist;
    bool enable_varlist_defaults;
};

static Ref<String> this_http_server_addr;
static Ref<String> this_admin_server_addr;
static Ref<String> this_rtmp_server_addr;
static Ref<String> this_rtmpt_server_addr;

static List< Ref<PathEntry> > path_list;

static mt_const MConfig::Varlist glob_varlist;
static mt_const bool glob_enable_varlist_defaults = true;

static MomentServer *moment = NULL;
static PagePool *page_pool = NULL;

static Result momentFile_sendTemplate (HttpRequest *http_req,
                                       ConstMemory  path_dir,
				       ConstMemory  full_path,
				       ConstMemory  filename,
				       Sender      * mt_nonnull sender,
				       ConstMemory  mime_type,
                                       MConfig::Varlist *varlist,
                                       bool         enable_varlist_defaults,
                                       ConstMemory  strings_filename,
                                       ConstMemory  stringvars_filename);

static Result momentFile_sendMemory (ConstMemory  mem,
				     Sender      * mt_nonnull sender,
				     ConstMemory  mime_type);

Result httpRequest (HttpRequest   * const mt_nonnull req,
		    Sender        * const mt_nonnull conn_sender,
		    Memory const  & /* msg_body */,
		    void         ** const mt_nonnull /* ret_msg_data */,
		    void          * const _path_entry)
{
    PathEntry * const path_entry = static_cast <PathEntry*> (_path_entry);

    logD_ (_func, req->getRequestLine());

  // TODO On Linux, we could do a better job with sendfile() or splice().

    ConstMemory file_path;
    {
	ConstMemory full_path = req->getFullPath();
	if (full_path.len() > 0
	    && full_path.mem() [0] == '/')
	{
	    full_path = full_path.region (1);
	}

	ConstMemory const prefix = path_entry->prefix->mem();
	if (full_path.len() < prefix.len()
	    || memcmp (full_path.mem(), prefix.mem(), prefix.len()))
	{
	    logE_ (_func, "full_path \"", full_path, "\" does not match prefix \"", prefix, "\"");

	    MOMENT_FILE__HEADERS_DATE;
	    ConstMemory const reply_body = "500 Internal Server Error";
	    conn_sender->send (
		    page_pool,
		    true /* do_flush */,
		    MOMENT_FILE__500_HEADERS (reply_body.len()),
		    "\r\n",
		    reply_body);
	    if (!req->getKeepalive())
		conn_sender->closeAfterFlush();

	    logA_ ("file 500 ", req->getClientAddress(), " ", req->getRequestLine());

	    return Result::Success;
	}

	file_path = full_path.region (prefix.len());
    }
    logD_ (_func, "file_path: ", file_path);

    while (file_path.len() > 0
	   && file_path.mem() [0] == '/')
    {
	file_path = file_path.region (1);
    }

//    logD_ (_func, "file_path: ", file_path);
    if (file_path.len() == 0) {

	file_path = "index.html";

#if 0
	logE_ (_func, "Empty file path\n");

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "400 Bad Request";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_FILE__400_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	return Result::Success;
#endif
    }

    ConstMemory mime_type = "text/plain";
    bool try_template = false;
    bool try_lang = false;
    Size ext_length = 0;
    ConstMemory ext;
    {
#ifdef PLATFORM_WIN32
        // MinGW doesn't have memrchr().
        void const *dot_ptr = NULL;
        {
            unsigned char const * const mem = file_path.mem();
            Size const len = file_path.len();
            for (Size i = len; i > 0; --i) {
                if (mem [i - 1] == '.') {
                    dot_ptr = mem + (i - 1);
                    break;
                }
            }
        }
#else
	void const * const dot_ptr = memrchr (file_path.mem(), '.', file_path.len());
#endif
	if (dot_ptr) {
	    ext = file_path.region ((Byte const *) (dot_ptr) + 1 - file_path.mem());
            if (equal (ext, "ts"))
                mime_type = "video/MP2T";
            else
            if (equal (ext, "m3u8"))
//                mime_type = "application/x-mpegURL";
                mime_type = "application/vnd.apple.mpegurl";
            else
	    if (equal (ext, "html")) {
		mime_type = "text/html";
		try_template = true;
                try_lang = true;
		ext_length = 5;
	    } else
	    if (equal (ext, "json")) {
		// application/json doesn't work on client side somewhy.
		// mime_type = "application/json";
		try_template = true;
		ext_length = 0;
	    } else
	    if (equal (ext, "css"))
		mime_type = "text/css";
	    else
	    if (equal (ext, "js"))
		mime_type = "application/javascript";
	    else
	    if (equal (ext, "swf"))
		mime_type = "application/x-shockwave-flash";
	    else
	    if (equal (ext, "png"))
		mime_type = "image/png";
	    else
	    if (equal (ext, "jpg"))
		mime_type = "image/jpeg";
            else
            if (equal (ext, "mp4"))
                mime_type = "video/mp4";
            else
            if (equal (ext, "mov"))
                mime_type = "video/quicktime";
	}
    }
//    logD_ (_func, "try_template: ", try_template);

    StRef<String> const filename = st_makeString (path_entry->path->mem(),
                                                  !path_entry->path->isNull() ? "/" : "",
                                                  file_path);
    NativeFile native_file;
    logD_ (_func, "Trying ", filename->mem());
    if (!native_file.open (filename->mem(),
                           0 /* open_flags */,
                           File::AccessMode::ReadOnly))
    {
        struct AcceptedLanguage {
            ConstMemory lang;
            double      weight;
            bool        extra;
        };

        struct AcceptedLanguageComparator {
            static bool greater (AcceptedLanguage const &left,
                                 AcceptedLanguage const &right)
            {
                return (left.weight > right.weight)
                       || (left.weight == right.weight && left.extra < right.extra);
            }

            static bool equals (AcceptedLanguage const &left,
                                AcceptedLanguage const &right)
                { return left.weight == right.weight && left.extra == right.extra; }
        };

        typedef AvlTree< AcceptedLanguage,
                         DirectExtractor<AcceptedLanguage>,
                         AcceptedLanguageComparator >
                AcceptedLanguageTree;

        typedef AvlTree< AcceptedLanguageTree::Node*,
                         MemberExtractor< AcceptedLanguageTree::Node,
                                          AcceptedLanguage,
                                          &AcceptedLanguageTree::Node::value,
                                          ConstMemory,
                                          MemberExtractor< AcceptedLanguage,
                                                           ConstMemory,
                                                           &AcceptedLanguage::lang > >,
                         MemoryComparator<> >
                AcceptedLanguageSet;

        // Keep 'langs' around, because it holds string references.
        List<HttpRequest::AcceptedLanguage> langs;
        AcceptedLanguageTree lang_tree;
        AcceptedLanguageSet  lang_set;
        StRef<String> strings_filename;
        StRef<String> stringvars_filename;
        if (try_lang) {
          // TODO Set default language in config etc.

            HttpRequest::parseAcceptLanguage (req->getAcceptLanguage(), &langs);

            logD_ (_func, "accepted languages (", req->getAcceptLanguage(), "):");
            {
                List<HttpRequest::AcceptedLanguage>::iter iter (langs);
                while (!langs.iter_done (iter)) {
                    List<HttpRequest::AcceptedLanguage>::Element * const el = langs.iter_next (iter);
                    HttpRequest::AcceptedLanguage * const req_alang = &el->data;
                    logD_ (req_alang->lang, ", weight ", req_alang->weight);

                    AcceptedLanguage alang;
                    alang.lang   = req_alang->lang->mem();
                    alang.weight = req_alang->weight;
                    alang.extra  = false;
                    {
                        AcceptedLanguageSet::Node * const node = lang_set.lookup (alang.lang);
                        if (!node) {
                            lang_set.add (lang_tree.add (alang));
                        } else
                        if (node->value->value.extra) {
                            AcceptedLanguageTree::Node * const tmp_node = node->value;
                            lang_set.remove (node);
                            lang_tree.remove (tmp_node);
                            lang_set.add (lang_tree.add (alang));
                        }
                    }

                    Byte const * const dash = (Byte const *) memchr (alang.lang.mem(), '-', alang.lang.len());
                    if (dash && dash != alang.lang.mem()) {
                        AcceptedLanguage extra_alang;
                        extra_alang.lang   = ConstMemory (alang.lang.mem(), dash - alang.lang.mem());
                        extra_alang.weight = alang.weight;
                        extra_alang.extra  = true;
                        if (!lang_set.lookup (extra_alang.lang))
                            lang_set.add (lang_tree.add (extra_alang));
                    }
                }
            }

            if (!lang_set.lookup (ConstMemory ("en"))) {
              // Default language with the lowest priority.
                AcceptedLanguage alang;
                alang.lang = ConstMemory ("en");
                // HTTP allows only positive weights, so '-1.0' is always the lowest.
                alang.weight = -1.0;
                alang.extra = true;
                lang_tree.add (alang);
            }

            {
                AcceptedLanguageTree::BottomRightIterator iter (lang_tree);
                while (!iter.done()) {
                    AcceptedLanguage * const alang = &iter.next ().value;
                    logD_ (_func, "Trying strings for language \"", alang->lang, "\"");

                    // TODO Use Vfs::stat instead.
                    StRef<String> const tmp_filename =
                            st_makeString (path_entry->path->mem(),
                                           path_entry->path->len() ? "/" : "", "strings.",
                                           alang->lang,
                                           ".tpl");
                    NativeFile tmp_file;
                    if (tmp_file.open (tmp_filename->mem(), 0 /* open_flags */, File::AccessMode::ReadOnly)) {
                        strings_filename = tmp_filename;
                        break;
                    }
                }
            }

            {
                AcceptedLanguageTree::BottomRightIterator iter (lang_tree);
                while (!iter.done()) {
                    AcceptedLanguage * const alang = &iter.next ().value;
                    logD_ (_func, "Trying stringvars for language \"", alang->lang, "\"");

                    // TODO Use Vfs::stat instead.
                    StRef<String> const tmp_filename =
                            st_makeString (path_entry->path->mem(),
                                           path_entry->path->len() ? "/" : "", "strings.",
                                           alang->lang,
                                           ".var");
                    NativeFile tmp_file;
                    if (tmp_file.open (tmp_filename->mem(), 0 /* open_flags */, File::AccessMode::ReadOnly)) {
                        stringvars_filename = tmp_filename;
                        break;
                    }
                }
            }
        }

#ifdef MOMENT_CTEMPLATE
	if (try_template) {
            logD_ (_func, "Trying .tpl");
            do {
                StRef<String> const filename_tpl =
                        st_makeString (filename->mem().region (0, filename->mem().len() - ext_length),
                                       ".tpl");
                {
                  // Avoiding ctemplate warnings on stderr.
                    NativeFile tmp_file;
                    if (!tmp_file.open (filename_tpl->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly)) {
                        break;
                    }
                }
                if (momentFile_sendTemplate (
                            req,
                            path_entry->path->mem(),
                            req->getFullPath(),
                            filename_tpl->mem(),
                            conn_sender,
                            mime_type,
                            &path_entry->varlist,
                            path_entry->enable_varlist_defaults,
                            strings_filename ? strings_filename->mem() : ConstMemory(),
                            stringvars_filename ? stringvars_filename->mem() : ConstMemory()))
                {
                    if (!req->getKeepalive())
                        conn_sender->closeAfterFlush();

                    logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

                    return Result::Success;
                }
            } while (0);
        }
#endif

        bool opened = false;
        if (equal (ext, "html")) {
#ifdef MOMENT_CTEMPLATE
            if (try_template) {
                AcceptedLanguageTree::BottomRightIterator iter (lang_tree);
                while (!iter.done()) {
                    AcceptedLanguage * const alang = &iter.next ().value;
                    logD_ (_func, "Trying .tpl for language \"", alang->lang, "\"");
                    do {
                        StRef<String> const filename_tpl =
                                st_makeString (filename->mem().region (0, filename->mem().len() - ext_length),
                                               ".",
                                               alang->lang,
                                               ".tpl");
                        {
                          // Avoiding ctemplate warnings on stderr.
                            NativeFile tmp_file;
                            if (!tmp_file.open (filename_tpl->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly))
                                break;
                        }
                        if (momentFile_sendTemplate (
                                    req,
                                    path_entry->path->mem(),
                                    req->getFullPath(),
                                    filename_tpl->mem(),
                                    conn_sender,
                                    mime_type,
                                    &path_entry->varlist,
                                    path_entry->enable_varlist_defaults,
                                    strings_filename ? strings_filename->mem() : ConstMemory(),
                                    stringvars_filename ? stringvars_filename->mem() : ConstMemory()))
                        {
                            if (!req->getKeepalive())
                                conn_sender->closeAfterFlush();

                            logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

                            return Result::Success;
                        }
                    } while (0);
                }
            }
#endif

            {
                AcceptedLanguageTree::BottomRightIterator iter (lang_tree);
                while (!iter.done()) {
                    AcceptedLanguage * const alang = &iter.next ().value;
                    logD_ (_func, "Trying .html for language \"", alang->lang, "\"");

                    if (native_file.open (st_makeString (filename->mem().region (0, filename->mem().len() - ext_length),
                                                         ".",
                                                         alang->lang,
                                                         ".html")->mem(),
                                          0 /* open_flags */,
                                          File::AccessMode::ReadOnly))
                    {
                        opened = true;
                        break;
                    }
                }
            }
        } // if ("html")

        if (!opened) {
            logE_ (_func, "Could not open \"", filename, "\": ", exc->toString());

            MOMENT_FILE__HEADERS_DATE;
            ConstMemory const reply_body = "404 Not Found";
            conn_sender->send (
                    page_pool,
                    true /* do_flush */,
                    MOMENT_FILE__404_HEADERS (reply_body.len()),
                    "\r\n",
                    reply_body);
            if (!req->getKeepalive())
                conn_sender->closeAfterFlush();

            logA_ ("file 404 ", req->getClientAddress(), " ", req->getRequestLine());

            return Result::Success;
        }
    }

    bool got_mtime = false;
    struct tm mtime;

    if (native_file.getModificationTime (&mtime))
        got_mtime = true;
    else
        logE_ (_func, "native_file.getModificationTime() failed: ", exc->toString());

    if (got_mtime) {
        bool if_none_match__any = false;
        List<HttpRequest::EntityTag> etags;
        {
            ConstMemory const mem = req->getIfNoneMatch();
            if (!mem.isEmpty())
                HttpRequest::parseEntityTagList (mem, &if_none_match__any, &etags);
        }

        bool send_not_modified = false;
        if (if_none_match__any) {
          // We have already opened the file, so it does exist.
            send_not_modified = true;
        } else {
            bool got_if_modified_since = false;
            struct tm if_modified_since;
            {
                ConstMemory const mem = req->getIfModifiedSince();
                if (!mem.isEmpty()) {
                    if (parseHttpTime (mem, &if_modified_since))
                        got_if_modified_since = true;
                    else
                        logW_ (_func, "Could not parse HTTP time: ", mem);
                }
            }

            if (got_if_modified_since ||
                if_none_match__any    ||
                !etags.isEmpty())
            {
                bool expired = false;
                if (compareTime (&mtime, &if_modified_since) == ComparisonResult::Greater)
                    expired = true;

                bool etag_match = etags.isEmpty();
                {
                    List<HttpRequest::EntityTag>::iter iter (etags);
                    while (!etags.iter_done (iter)) {
                        HttpRequest::EntityTag * const etag = &etags.iter_next (iter)->data;

                        struct tm etag_time;
                        if (parseHttpTime (etag->etag->mem(), &etag_time)) {
                            if (compareTime (&mtime, &etag_time) == ComparisonResult::Equal) {
                                etag_match = true;
                                break;
                            }
                        } else {
                            logW_ (_func, "Could not parse etag time: ", etag->etag->mem());
                        }
                    }
                }

                if (etag_match && !expired)
                    send_not_modified = true;
            }
        }

        if (send_not_modified) {
            MOMENT_FILE__HEADERS_DATE;
            conn_sender->send (
                    page_pool,
                    true /* do_flush */,
                    MOMENT_FILE__304_HEADERS,
                    "\r\n");
            if (!req->getKeepalive())
                conn_sender->closeAfterFlush();

            logA_ ("file 304 ", req->getClientAddress(), " ", req->getRequestLine());

            return Result::Success;
        }
    }

    NativeFile::FileStat stat;
    if (!native_file.stat (&stat)) {
	logE_ (_func, "native_file.stat() failed: ", exc->toString());

	MOMENT_FILE__HEADERS_DATE;
	ConstMemory const reply_body = "500 Internal Server Error";
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		MOMENT_FILE__500_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	logA_ ("file 500 ", req->getClientAddress(), " ", req->getRequestLine());

	return Result::Success;
    }

    MOMENT_FILE__HEADERS_DATE;

    Byte mtime_buf [unixtimeToString_BufSize];
    Size mtime_len = 0;
    if (got_mtime)
        mtime_len = timeToHttpString (Memory::forObject (mtime_buf), &mtime);

    conn_sender->send (
	    page_pool,
            // TODO No need to flush here? (Remember about HEAD path)
	    true /* do_flush */,
	    MOMENT_FILE__OK_HEADERS (mime_type, stat.size),
            // TODO Send "ETag:" ?
            got_mtime ? "Last-Modified: " : "", 
            got_mtime ? ConstMemory (mtime_buf, mtime_len) : ConstMemory(),
            got_mtime ? "\r\n" : "",
	    "\r\n");

    if (equal (req->getMethod(), "HEAD")) {
	if (!req->getKeepalive())
	    conn_sender->closeAfterFlush();

	logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

	return Result::Success;
    }

    PagePool::PageListHead page_list;

    // TODO This doesn't work well with large files (eats too much memory).
    Size total_sent = 0;
    Byte buf [65536];
    for (;;) {
	Size toread = sizeof (buf);
	if (stat.size - total_sent < toread)
	    toread = stat.size - total_sent;

	Size num_read;
	IoResult const res = native_file.read (Memory (buf, toread), &num_read);
	if (res == IoResult::Error) {
	    logE_ (_func, "native_file.read() failed: ", exc->toString());
	    conn_sender->flush ();
	    conn_sender->closeAfterFlush ();
	    return Result::Success;
	}
	assert (num_read <= toread);

	// TODO Double copy - not very smart.
	page_pool->getFillPages (&page_list, ConstMemory (buf, num_read));
	total_sent += num_read;
	assert (total_sent <= stat.size);
	if (total_sent == stat.size)
	    break;

	if (res == IoResult::Eof)
	    break;
    }

#if 0
    {
      // DEBUG

        Count size = 0;
        PagePool::Page *cur_page = page_list.first;
        while (cur_page) {
            size += cur_page->data_len;
            cur_page = cur_page->getNextMsgPage();
        }

        logD_ (_func, "total data length: ", size);
    }
#endif

    conn_sender->sendPages (page_pool, page_list.first, true /* do_flush */);

    assert (total_sent <= stat.size);
    if (total_sent != stat.size) {
	logE_ (_func, "File size mismatch: total_sent: ", total_sent, ", stat.size: ", stat.size);
	conn_sender->closeAfterFlush ();
	logA_ ("file 200 ", req->getRequestLine());
	return Result::Success;
    }

    if (!req->getKeepalive())
	conn_sender->closeAfterFlush();

    logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());

//    logD_ (_func, "done");
    return Result::Success;
}

#ifdef MOMENT_CTEMPLATE
namespace {
    class SendTemplate_PageRequest : public MomentServer::PageRequest
    {
    private:
	mt_const HttpRequest *http_req;
	mt_const ctemplate::TemplateDictionary *dict;

    public:
      mt_iface (MomentServer::PageRequest)

	ConstMemory getParameter (ConstMemory const name)
            { return http_req->getParameter (name); }

	IpAddress getClientAddress ()
            { return http_req->getClientAddress(); }

	void addHashVar (ConstMemory const name,
			 ConstMemory const value)
	{
	    logD_ (_func, "name: ", name, ", value: ", value);
	    dict->SetValue (String (name).cstr(), String (value).cstr());
	}

	void showSection (ConstMemory const name)
            { dict->ShowSection (String (name).cstr()); }

      mt_iface_end

	SendTemplate_PageRequest (HttpRequest * const http_req,
				  ctemplate::TemplateDictionary * const dict)
	    : http_req (http_req),
	      dict (dict)
	{}
    };
}

class VarlistSectionHashEntry : public HashEntry<>
{
public:
    StRef<String> sect_name;
    bool enabled;
};

typedef Hash< VarlistSectionHashEntry,
              Memory,
              MemberExtractor< VarlistSectionHashEntry,
                               StRef<String>,
                               &VarlistSectionHashEntry::sect_name,
                               Memory,
                               AccessorExtractor< String,
                                                  Memory,
                                                  &String::mem > >,
              MemoryComparator<> >
        VarlistSectionHash;

static void fillDictionaryFromVarlist (ctemplate::TemplateDictionary * const mt_nonnull dict,
                                       MConfig::Varlist              * const mt_nonnull varlist,
                                       VarlistSectionHash            * const mt_nonnull sect_hash)
{
    {
        MConfig::Varlist::VarList::iterator iter (varlist->var_list);
        while (!iter.done()) {
            MConfig::Varlist::Var * const var = iter.next ();
            ConstMemory const name  = var->getName();
            ConstMemory const value = var->getValue();
            logD_ (_func, "SetValue: ", name, " = ", value);
            dict->SetValue (ctemplate::TemplateString ((char const *) name.mem(), name.len()),
                            ctemplate::TemplateString ((char const *) value.mem(), value.len()));
        }
    }

    {
        MConfig::Varlist::SectionList::iterator iter (varlist->section_list);
        while (!iter.done()) {
            MConfig::Varlist::Section * const sect = iter.next ();

            VarlistSectionHashEntry *entry = sect_hash->lookup (sect->getName());
            if (!entry) {
                entry = new (std::nothrow) VarlistSectionHashEntry;
                assert (entry);
                entry->sect_name = st_grab (new (std::nothrow) String (sect->getName()));
                sect_hash->add (entry);
            }
            entry->enabled = sect->getEnabled ();
        }
    }
}

static void loadVarlist (ConstMemory        const path,
                         MConfig::Varlist * const varlist)
{
    // TODO Create varlist_parser only once.
    MConfig::VarlistParser varlist_parser;
    if (!varlist_parser.parseVarlist (path, varlist))
        logD_ (_func, "Could not parse varlist ", path);
}

static Result momentFile_sendTemplate (HttpRequest * const http_req,
                                       ConstMemory   const path_dir,
				       ConstMemory   const full_path,
				       ConstMemory   const filename,
				       Sender      * const mt_nonnull sender,
				       ConstMemory   const mime_type,
                                       MConfig::Varlist * const varlist,
                                       bool          const enable_varlist_defaults,
                                       ConstMemory   const strings_filename,
                                       ConstMemory   const stringvars_filename)
{
    logD_ (_func, "full_path: ", full_path, ", filename: ", filename);

  // TODO Fill the dictionary only once for consecutive multi-language .tpl attempts.

    // TODO There should be a better way.
    ctemplate::mutable_default_template_cache()->ReloadAllIfChanged (ctemplate::TemplateCache::LAZY_RELOAD);

    ctemplate::TemplateDictionary dict ("tmpl");

    if (strings_filename.len()) {
        ctemplate::TemplateDictionary * const inc_dict = dict.AddIncludeDictionary ("STRINGS");
        inc_dict->SetFilename (ctemplate::TemplateString ((char const *) strings_filename.mem(), strings_filename.len()));
    }

    {
        char const val_name [] = "ThisHttpServerAddr";
        dict.SetValue (ctemplate::TemplateString (val_name,
                                                  sizeof (val_name) - 1), 
                       ctemplate::TemplateString ((char const *) this_http_server_addr->mem().mem(),
                                                  this_http_server_addr->mem().len()));
    }
    {
        char const val_name [] = "ThisAdminServerAddr";
        dict.SetValue (ctemplate::TemplateString (val_name,
                                                  sizeof (val_name) - 1),
                       ctemplate::TemplateString ((char const *) this_admin_server_addr->mem().mem(),
                                                  this_admin_server_addr->mem().len()));
    }
    {
        char const val_name [] = "ThisRtmpServerAddr";
        dict.SetValue (ctemplate::TemplateString (val_name,
                                                  sizeof (val_name) - 1),
                       ctemplate::TemplateString ((char const *) this_rtmp_server_addr->mem().mem(),
                                                  this_rtmp_server_addr->mem().len()));
    }
    {
        char const val_name [] = "ThisRtmptServerAddr";
        dict.SetValue (ctemplate::TemplateString (val_name,
                                                  sizeof (val_name) - 1),
                       ctemplate::TemplateString ((char const *) this_rtmpt_server_addr->mem().mem(),
                                                  this_rtmpt_server_addr->mem().len()));
    }

    {
        // ctemplate has ShowSection(), but doesn't have HideSection() or alike.
        // We have to filter sections in a separate hash because of this.
        VarlistSectionHash sect_hash;

        if (stringvars_filename.len()) {
            MConfig::Varlist varlist;
            loadVarlist (stringvars_filename, &varlist);
            fillDictionaryFromVarlist (&dict, &varlist, &sect_hash);
        }

        if (enable_varlist_defaults) {
            MConfig::Varlist varlist;
            loadVarlist (st_makeString (path_dir, path_dir.len() ? "/" : "", "var.defaults")->mem(), &varlist);
            fillDictionaryFromVarlist (&dict, &varlist, &sect_hash);
        }

        {
            Ref<MConfig::Varlist> const server_varlist = moment->getDefaultVarlist();
            fillDictionaryFromVarlist (&dict, server_varlist, &sect_hash);
        }

        fillDictionaryFromVarlist (&dict, &glob_varlist, &sect_hash);
        if (varlist)
            fillDictionaryFromVarlist (&dict, varlist, &sect_hash);

        VarlistSectionHash::iterator iter (sect_hash);
        while (!iter.done()) {
            VarlistSectionHashEntry * const entry = iter.next ();

            StRef<String> sect_name;
            if (entry->enabled)
                sect_name = st_makeString (entry->sect_name->mem(), "_ON");
            else
                sect_name = st_makeString (entry->sect_name->mem(), "_OFF");

            logD_ (_func, "ShowSection: ", sect_name);
            dict.ShowSection (ctemplate::TemplateString ((char const *) sect_name->mem().mem(),
                                                         sect_name->mem().len()));

            delete entry;
        }
    }

#if 0
    {
      // MD5 auth test.

        ConstMemory const client_text = "192.168.0.1 12345";
        Ref<String> const timed_text = makeString (((Uint64) getUnixtime() + 1800) / 3600 /* auth timestamp */,
                                                   " ",
                                                   client_text,
                                                   "password");
        logD_ (_func, "timed_text: ", timed_text->mem());
        unsigned char hash_buf [32];
        getMd5HexAscii (timed_text->mem(), Memory::forObject (hash_buf));
        Ref<String> const auth_str = makeString (client_text, "|", Memory::forObject (hash_buf));
        dict.SetValue ("MomentAuthTest", auth_str->cstr());
    }
#endif

    {
	SendTemplate_PageRequest page_req (http_req, &dict);
	MomentServer::PageRequestResult const res = moment->processPageRequest (&page_req, full_path);
	// TODO Check 'res' and react.
    }

    std::string str;
    if (!ctemplate::ExpandTemplate (grab (new (std::nothrow) String (filename))->cstr(),
				    ctemplate::DO_NOT_STRIP,
				    &dict,
				    &str))
    {
	logD_ ("could not expand template \"", filename, "\": ", str.c_str());
	return Result::Failure;
    }

//    logD_ ("template \"", filename, "\" expanded: ", str.c_str());
//    logD_ ("template \"", filename, "\" expanded");

    return momentFile_sendMemory (ConstMemory ((Byte const *) str.data(), str.length()), sender, mime_type);
}

static Result momentFile_sendMemory (ConstMemory   const mem,
				     Sender      * const mt_nonnull sender,
				     ConstMemory   const mime_type)
{
    MOMENT_FILE__HEADERS_DATE;
    sender->send (page_pool,
		  false /* do_flush */,
		  MOMENT_FILE__OK_HEADERS (mime_type, mem.len()),
		  "\r\n");

    PagePool::PageListHead page_list;
    page_pool->getFillPages (&page_list, mem);
    // TODO pages of zero length => (behavior - ?)
    sender->sendPages (page_pool, page_list.first, true /* do_flush */);

    return Result::Success;
}
#endif

static HttpService::HttpHandler const http_handler = {
    httpRequest,
    NULL /* httpMessageBody */
};

void momentFile_addPathEntry (PathEntry   * const path_entry,
                              HttpService * const http_service)
{
    logD_ (_func, "Adding path \"", path_entry->path, "\", prefix \"", path_entry->prefix, "\"");

    http_service->addHttpHandler (
	    CbDesc<HttpService::HttpHandler> (&http_handler, path_entry, NULL /* coderef_container */),
	    path_entry->prefix->mem());

    path_list.append (path_entry);
}

void momentFile_addPath (ConstMemory   const path,
			 ConstMemory   const prefix,
			 HttpService * const http_service)
{
    Ref<PathEntry> const path_entry = grab (new (std::nothrow) PathEntry);
    path_entry->path = grab (new (std::nothrow) String (path));
    path_entry->prefix = grab (new (std::nothrow) String (prefix));
    path_entry->enable_varlist_defaults = glob_enable_varlist_defaults;

    momentFile_addPathEntry (path_entry, http_service);
}

void momentFile_addPathForSection (MConfig::Section * const section,
				   HttpService      * const http_service)
{
    logD_ (_func_);

    ConstMemory path;
    {
	MConfig::Option * const opt = section->getOption ("path");
	MConfig::Value *val;
	if (opt
	    && (val = opt->getValue()))
	{
	    path = val->mem();
	}
    }

    ConstMemory prefix;
    {
	{
	    MConfig::Option * const opt = section->getOption ("prefix");
	    MConfig::Value *val;
	    if (opt
		&& (val = opt->getValue()))
	    {
		prefix = val->mem();
	    }
	}

	if (prefix.len() > 0
	    && prefix.mem() [0] == '/')
	{
	    prefix = prefix.region (1);
	}
    }

    Ref<PathEntry> const path_entry = grab (new (std::nothrow) PathEntry);
    path_entry->path = grab (new (std::nothrow) String (path));
    path_entry->prefix = grab (new (std::nothrow) String (prefix));
    path_entry->enable_varlist_defaults = glob_enable_varlist_defaults;

    if (MConfig::Section * const vars_section = section->getSection ("vars")) {
        logD_ (_func, "got \"vars\" section");

        if (MConfig::Attribute * const defaults_attr = vars_section->getAttribute ("defaults")) {
            switch (MConfig::strToBoolean (defaults_attr->getValue())) {
                case MConfig::Boolean_True:
                case MConfig::Boolean_Default:
                    logD_ (_func, "vars defaults enabled");
                    path_entry->enable_varlist_defaults = true;
                    break;
                case MConfig::Boolean_False:
                    logD_ (_func, "vars defaults disabled");
                    path_entry->enable_varlist_defaults = false;
                    break;
                case MConfig::Boolean_Invalid:
                    logE_ (_func, "Invalid value for vars:defaults attribute: ", defaults_attr->getValue());
                    break;
            }
        }

        MConfig::parseVarlistSection (vars_section, &path_entry->varlist);
    } else {
        logD_ (_func, "no \"vars\" section");
    }

    momentFile_addPathEntry (path_entry, http_service);
}

// Multiple file paths can be specified like this:
//     mod_file {
//         { path = "/home/user/files"; prefix = "files"; }
//         { path = "/home/user/trash"; prefix = "trash"; }
//     }
//
void momentFileInit ()
{
    logI_ (_func, "Initializing mod_file");

    moment = MomentServer::getInstance();
//    ServerApp * const server_app = moment->getServerApp();
    MConfig::Config * const config = moment->getConfig();
    HttpService * const http_service = moment->getHttpService();

    page_pool = moment->getPagePool ();

    {
	ConstMemory const opt_name = "mod_file/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Boolean_False) {
	    logI_ (_func, "Static HTTP content module (mod_file) is not enabled. "
		   "Set \"", opt_name, "\" option to \"y\" to enable.");
	    return;
	}
    }

    {
	ConstMemory const opt_name = "moment/this_http_server_addr";
	ConstMemory const opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.isNull()) {
	    this_http_server_addr = grab (new (std::nothrow) String (opt_val));
	} else {
	    this_http_server_addr = grab (new (std::nothrow) String ("127.0.0.1:8080"));
	    logI_ (_func, opt_name, " config parameter is not set. "
		   "Defaulting to ", this_http_server_addr);
	}
    }

    {
        ConstMemory const opt_name = "moment/this_admin_server_addr";
        ConstMemory const opt_val = config->getString (opt_name);
        logI_ (_func, opt_name, ": ", opt_val);
        if (!opt_val.isNull()) {
            this_admin_server_addr = grab (new (std::nothrow) String (opt_val));
        } else {
            this_admin_server_addr = grab( new (std::nothrow) String ("127.0.0.1:8082"));
            logI_ (_func, opt_name, " config parameters is not set. "
                   "Defaulting to ", this_admin_server_addr);
        }
    }

    {
	ConstMemory const opt_name = "moment/this_rtmp_server_addr";
	ConstMemory const opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.isNull()) {
	    this_rtmp_server_addr = grab (new (std::nothrow) String (opt_val));
	} else {
	    this_rtmp_server_addr = grab (new (std::nothrow) String ("127.0.0.1:1935"));
	    logI_ (_func, opt_name, " config parameter is not set. "
		   "Defaulting to ", this_rtmp_server_addr);
	}
    }

    {
	ConstMemory const opt_name = "moment/this_rtmpt_server_addr";
	ConstMemory const opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ": ", opt_val);
	if (!opt_val.isNull()) {
	    this_rtmpt_server_addr = grab (new (std::nothrow) String (opt_val));
	} else {
	    this_rtmpt_server_addr = grab (new (std::nothrow) String ("127.0.0.1:8080"));
	    logI_ (_func, opt_name, " config parameter is not set. "
		   "Defaulting to ", this_rtmpt_server_addr);
	}
    }

    bool got_path = false;
    do {
	MConfig::Section * const modfile_section = config->getSection ("mod_file");
	if (!modfile_section)
	    break;

        // Dealing with "vars" section first to figure out the value of
        // 'glob_enable_varlist_defaults'.
        if (MConfig::Section * const vars_section = modfile_section->getSection ("vars")) {
            if (MConfig::Attribute * const defaults_attr = vars_section->getAttribute ("defaults")) {
                switch (MConfig::strToBoolean (defaults_attr->getValue())) {
                    case MConfig::Boolean_True:
                    case MConfig::Boolean_Default:
                        logD_ (_func, "glob vars defaults enabled");
                        glob_enable_varlist_defaults = true;
                        break;
                    case MConfig::Boolean_False:
                        logD_ (_func, "glob vars defaults disabled");
                        glob_enable_varlist_defaults = false;
                        break;
                    case MConfig::Boolean_Invalid:
                        logE_ (_func, "Invalid value for vars:defaults attribute: ", defaults_attr->getValue());
                        break;
                }
            }

            MConfig::parseVarlistSection (vars_section, &glob_varlist);
        }

	MConfig::Section::iter iter (*modfile_section);
	while (!modfile_section->iter_done (iter)) {
	    got_path = true;

	    MConfig::SectionEntry * const sect_entry = modfile_section->iter_next (iter);
	    if (sect_entry->getType() == MConfig::SectionEntry::Type_Section) {
                MConfig::Section * const section = static_cast <MConfig::Section*> (sect_entry);
		if (sect_entry->getName().len() == 0)
                    momentFile_addPathForSection (section, http_service);
            }
	}
    } while (0);

    if (!got_path) {
      // Default setup.
	momentFile_addPath ("/opt/moment/myplayer", "moment", http_service);
	momentFile_addPath ("/opt/moment/mychat", "mychat", http_service);
    }
}

void momentFileUnload ()
{
}

} // namespace {}

} // namespace Moment


namespace M {

void libMary_moduleInit ()
{
    Moment::momentFileInit ();
}

void libMary_moduleUnload()
{
    Moment::momentFileUnload ();
}

}

