#ifndef INCLUDED_PARSE_H
#define INCLUDED_PARSE_H

// parse.c
// parse_bindmode.c
// parse_include.c
// parse_key_call.c
/**
 * Parser to parse .cng or .cnergy files to an internal format that can be run.
 * This is an example of such file:
 * *******************
 * bind edit::normal:
 * a >1 :insert
 * bind $edit::insert:
 * escape :normal
 * *******************
 * This program will add two modes to the edit window type where each has
 * two binds defined by a key sequence and then the action (switch mode in this case).
 * Flags for modes are:
 * $ - Typing is possible in this mode
 * * - Selection is possible in this mode
 * . - Supplementary mode (only used while parsing)
 *
 * The edit:: can be omitted to add the bind to the default/all mode. For instance:
 * ***************************
 * bind normal:
 * ## Move to the right window
 * ^Wl >w1
 * ## Move to the left window
 * ^Wh <w1
 * ***************************
 * Binds specific to window types always have priority.
 *
 ******************
 * Using include *
 ******************
 * *************************
 * include "cng/file.cng"
 * *************************
 * This will include the file file.cng, note that the path is
 * relative to the working directory (TODO: maybe change this)
 *
 */

/**
 * Errors cause the parser to throw all away after it finished, warnings just give information
 */
typedef enum {
	// Legend:
	// ERR - general error
	// ERRK - error for key binds
	// ERRIP - error for instruction integer parameter
	// ERRINSTR - error for instruction
	// ERRBIND - error for bind 
	// ERRBM - error for bindmode
	SUCCESS,
	FAIL,
	/* Put warning codes here */
	WARN_MAX,
	/**
	 * Put error codes here, if an error code is reported,
	 * the next resolve method will be used to jump over more potentially erroneous code
	 **/
	ERR_INVALID,
	ERR_OUTOFMEMORY,
	ERRFILE_TOOMANY,
	ERRFILE_IO,
	ERRINCLUDE_STRING,
	ERRK_ALLOCATING,
	ERRK_EOF_BACKSLASH,
	ERRK_MISSING_CLOSING_ANGLE_BRACKET,
	ERRK_NON_EXISTENT_NAME,
	ERRK_EXPECTED_WORD,
	ERRK_INVALID,
	ERRINSTR_INVALID,
	ERRBM_COLON,
	ERRBM_LINEBREAK,
	ERRBM_NOSPACE,
	ERRBM_NEWLINE,
	ERRBM_WORD,
	ERRBM_INVALID,
	ERRINSTR_INVALIDIP,
	ERRINSTR_COLONWORD,
	ERRINSTR_INVALIDNAME,
	ERRINSTR_JMPWORD,
	ERRINSTR_INVALIDWORD,
	ERRIP_WORD,
	ERRIP_REGISTER,
	ERRIP_HASH,
	ERRIP_NUMBER,
	ERRINSTR_WINDOWDEL,
	ERRPROG_MISSING_OPENING,
	ERRPROG_MISSING_CLOSING,
	ERRPROG_MISSING_LABEL,
	ERRW_TOO_LONG,
	ERR_EXPECTED_MODE_NAME,
	ERR_INVALID_WINDOWTYPE,
	ERRBIND_LABEL,
	ERRBIND_OUTSIDE,
	ERR_MAX,
} parser_error_t;

struct parser {
	struct parser_file {
		FILE *fp;
		fileid_t file;
	} streams[16];
	unsigned nStreams;
	int c;
	int prev_c;

	struct parser_token {
		long pos;
		fileid_t file;
		int type;
		char value[512];
	} tokens[16];
	unsigned nTokens;
	unsigned nPeekedTokens;
	int (*tokengetter)(struct parser *parser, struct parser_token *tok);

	struct binding_mode *firstModes[WINDOW_MAX];
	struct binding_mode *curMode;
	// the requests are necessary to allow usage before declaration
	// append requests are added when a mode should be extended by another
	struct append_request {
		struct binding_mode *mode;
		window_type_t windowType;
		char donor[64];
	} *appendRequests;
	size_t nAppendRequests;

	int *keys;
	unsigned nKeys;
	void *program;
	size_t szProgram;
	void *data;
	size_t szData;
	struct parser_label {
		char name[64];
		uintptr_t address;
	} *labels;
	size_t nLabels;
	struct parser_label_request {
		char name[64];
		uintptr_t address;
	} *labelRequests;
	size_t nLabelRequests;

	struct {
		parser_error_t err;
		struct parser_token token;
	} errStack[32];
	unsigned nErrStack;
	unsigned nErrors;
	unsigned nWarnings;
};

// parse.c
int parseint(const char *str, ptrdiff_t *pInt);
int parsewindowtype(const char *str, window_type_t *pType);
int parser_getc(struct parser *parser);
int parser_ungetc(struct parser *parser);
long parser_tell(struct parser *parser);
int parser_getcommontoken(struct parser *parser);
void parser_setcontext(struct parser *parser, int (*tokengetter)(struct parser *parser, struct parser_token *tok));
unsigned parser_peektoken(struct parser *parser, struct parser_token *tok);
unsigned parser_peektokens(struct parser *parser, struct parser_token *toks, unsigned nToks);
void parser_consumespace(struct parser *parser);
void parser_consumeblank(struct parser *parser);
void parser_consumetoken(struct parser *parser);
void parser_consumetokens(struct parser *parser, unsigned nToks);
parser_error_t parser_pusherror(struct parser *parser, parser_error_t err);
int parser_addappendrequest(struct parser *parser, struct append_request *request);
int parser_windowtype(struct parser *parser, const char *name, window_type_t *pType);
/** Run the parser on a file (set the memory of the parser to 0 before starting to use it) */
int parser_run(struct parser *parser, fileid_t file);
int parser_cleanup(struct parser *parser);

// parse_mode.c
/**
 * Reads a bind mode and adds it to parser->modes
 *
 * Examples:
 * $insert:
 * normal: foo
 * .supp:
 * visual*: foo, bar
 */
int parser_getbindmode(struct parser *parser);

/**
 * Reads a bind
 *
 * Examples:
 * abc, fk >1
 * x $x-1
 * b `C ; >1
 * a decc ; >wC
 */
int parser_getbind(struct parser *parser);

/**
 * Reads keys
 *
 * Examples:
 * ab<escape>
 * d, e, f
 */
int parser_getkeys(struct parser *parser);

int parser_getprogram(struct parser *parser);

#endif
