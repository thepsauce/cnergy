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
 * ^W l >w1
 * ## Move to the left window
 * ^W h <w1
 * ***************************
 * Binds specific to window types always have priority.
 *
 ******************
 * Using include *
 ******************
 * *************************
 * include "file.cng"
 * *************************
 * This will include the file file.cng, note that the path is
 * relative to the working directory (TODO: maybe change this)
 *
 ******************************
 * Loops, caching and any key *
 ******************************
 * *********************************
 * f ** (( #-- & !find %1 $A ))
 * *********************************
 * Here is a breakdown of this command line:
 * f							If the user presses f
 *   **							If the user pressed any other key
 *       ((                  ))	Loop until the inside fails
 *          #--					Decrement the register
 *              &				Stop the loop if the register is 0 or if find fails
 *               !find %1		Find the char the user pressed
 *                        $		Move the cursor
 *                         A	to the return value of find
 */

/**
 * Fatal internal errors have a negative value
 * Success has the value of 0
 * When a function wants to fully commit for a path through the constructs it returns COMMIT
 * When a function has to abort it returns SKIP
 * When a function has to abort and it's not possible to recover, it returns MUSTSKIP
 * When a syntax construct was read correctly and the next can being, FINISH is returns
 */
enum {
	OUTOFMEMORY = -1,
	SUCCESS,
	FAIL,
	COMMIT,
	FINISH,
};

/**
 * Errors cause the parser to throw all away after it finished, warnings just give information
 */
typedef enum {
	WARN_,
	WARN_MAX,

	ERR_FILEIO,
	ERR_TOOMANYFILES,
	ERR_INVALID_SYNTAX,
	ERR_INVALID_KEY,
	ERR_INVALID_COMMAND,
	ERR_EXPECTED_COLON_MODE,
	ERR_NEED_SPACE_KEYS,
	ERR_EXPECTED_LINE_BREAK_MODE,
	ERR_WORD_TOO_LONG,
	ERR_INVALID_KEY_NAME,
	ERR_BIND_OUTSIDEOF_MODE,
	ERR_BIND_NEEDS_MODE,
	ERR_EXPECTED_PARAM,
	ERR_INVALID_PARAM,
	ERR_WINDOW_DEL,
	ERR_INVALID_COMMAND_NAME,
	ERR_LOOP_TOO_DEEP,
	ERR_END_OF_LOOP_WITHOUT_START,
	ERR_EMPTY_LOOP,
	ERR_XOR_LOOP,
	ERR_MODE_NOT_EXIST,
	ERR_EXPECTED_WORD_AT,
	ERR_EXPECTED_MODE_NAME,
	ERR_INVALID_WINDOW_TYPE,
} parser_error_t;

struct parser {
	struct parser_file {
		FILE *fp;
		fileid_t file;
	} files[16];
	unsigned nFiles;
	int c, prev_c;
	ptrdiff_t num;
	char word[64];
	char *str;
	size_t nStr;
	struct binding_mode mode;
	window_type_t windowType;
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
	int loopStack[32];
	int nLoopStack;
	void *program;
	size_t szProgram;
	struct {
		parser_error_t err;
		long pos;
		fileid_t file;
	} errStack[32];
	unsigned nErrStack;
	unsigned nErrors;
	unsigned nWarnings;
};

// parse.c
/** Return a string representing the error code */
const char *parser_strerror(parser_error_t err);
/** Open another file */
int parser_open(struct parser *parser, fileid_t file);
/** Read the next character from the file */
int parser_getc(struct parser *parser);
void parser_ungetc(struct parser *parser);
void parser_pusherror(struct parser *parser, parser_error_t err);
int parser_addappendrequest(struct parser *parser, struct append_request *request);
int parser_cleanup(struct parser *parser);
/**
 * Read the next word, must have a character ready before calling
 *
 * Examples of words:
 * hello
 * bana_na
 * __bread_12
 **/
int parser_getword(struct parser *parser);
/**
 * Read the next number, must have a character ready before calling
 *
 * Examples of numbers:
 * +
 * -
 * +123
 * 948
 * -2
 */
int parser_getnumber(struct parser *parser);
/**
 * Read the next string, must have a character ready before calling
 *
 * Examples of strings:
 * "Hello there"
 * "Long\tstring\nWith\tEscape\tcharacters"
 * "\""
 */
int parser_getstring(struct parser *parser);
/**
 * Read the next mode name
 *
 * Examples of mode names:
 * normal
 * edit::normal
 * fileview::insert
 */
int parser_getmodename(struct parser *parser);
/** Skip blanks (space and tab) */
int parser_skipspace(struct parser *parser);
/** Read the next syntax construct */
int parser_next(struct parser *parser);

// parse_mode.c
/**
 * Reads a bind mode and adds it to parser->modes
 *
 * Examples:
 * $insert:
 * normal:
 * .supp:
 * visual*:
 */
int parser_checkbindword(struct parser *parser);
int parser_getbindmodeprefix(struct parser *parser);
int parser_getbindmodesuffix(struct parser *parser);
int parser_getbindmodeextensions(struct parser *parser);
int parser_addbindmode(struct parser *parser);

// parse_key_call.c
/**
 * Reads a key and adds it to the key list inside of parser
 *
 * Examples:
 * home
 * 4
 * a
 * >
 * ^A
 * ^
 *
 * So either there is a word, a character with a following blank or a character with a leading '^'
 */
int parser_getkey(struct parser *parser);
/**
 * Reads a key list and stores it inside parser->keys
 *
 * Examples:
 * a b
 * d l, x x
 * home ^A, A
 */
int parser_getkeylist(struct parser *parser);
/**
 * Reads an instruction and stores it inside of parser->program
 * May add requests
 *
 * Examples:
 * >1
 * $x-1
 * `N
 * .home
 * !paste
 * ((
 * ))
 * >w4
 */
int parser_getinstruction(struct parser *parser);
int parser_getprogram(struct parser *parser);
int parser_addbind(struct parser *parser);

// parse_include.c
int parser_checkincludeword(struct parser *parser);
int parser_addinclude(struct parser *parser);

#endif
