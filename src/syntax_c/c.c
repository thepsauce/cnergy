#include "cnergy.h"

enum {
	C_STATE_DEFAULT,
	C_STATE_STRING,

	C_STATE_NUMBER,
	C_STATE_NUMBER_ZERO,
	C_STATE_NUMBER_DECIMAL,
	C_STATE_NUMBER_HEX,
	C_STATE_NUMBER_OCTAL,
	C_STATE_NUMBER_BINARY,
	C_STATE_NUMBER_EXP,
	C_STATE_NUMBER_NEGEXP,
	C_STATE_NUMBER_FLOAT,
	C_STATE_NUMBER_ERRSUFFIX,
	C_STATE_NUMBER_LSUFFIX,
	C_STATE_NUMBER_USUFFIX,

	C_STATE_PREPROC,
	C_STATE_PREPROC_COMMON,
	C_STATE_PREPROC_DEFINE,
	C_STATE_PREPROC_INCLUDE,
	C_STATE_PREPROC_UNDEF,

	C_STATE_LINECOMMENT,
	C_STATE_MULTICOMMENT,
};

static const struct {
	int pair;
	// note: be sure to increase this number when a list exceeds words; the word list is null terminated
	const char *words[64];
} C_keywords[] = {
	{ 12, {
		"__auto_type",
		"char",
		"double",
		"float",
		"int",
		"long",
		"short",
		"signed",
		"typedef",
		"typeof",
		"unsigned",
		"void",
		"FILE",
		NULL,
	} },
	{ 4, {
		"auto",
		"const",
		"enum",
		"extern",
		"inline",
		"register",
		"static",
		"struct",
		"union",
		"volatile",
		NULL,
	} },
	{ 10, {
		"break",
		"case",
		"continue",
		"default",
		"do",
		"else",
		"for",
		"goto",
		"if",
		"return",
		"switch",
		"while",
		NULL,
	} },
	{ 14, {
		"M_E",
		"M_PI",
		"M_PI",
		"M_E",
		"M_LOG2E",
		"M_LOG10E",
		"M_LN2",
		"M_LN10",
		"M_PI",
		"M_PI_2",
		"M_PI_4",

		"M_1_PI",
		"M_2_PI",
		"M_2_SQRTPI",
		"M_SQRT2",
		"M_SQRT1_2",

		NULL,
	} },
	{ 14, {
		"__LINE__",
		"__FILE__",
		"__DATE__",
		"__TIME__",
		"__STDC__",
		"__STDC_VERSION__",
		"__STDC_HOSTED__",

		"sizeof",
		NULL,
	} },
	{ 14, {
		"CHAR_BIT",
		"MB_LEN_MAX",
		"MB_CUR_MAX",

		"UCHAR_MAX",
		"UINT_MAX",
		"ULONG_MAX",
		"USHRT_MAX",

		"CHAR_MIN",
		"INT_MIN",
		"LONG_MIN",
		"SHRT_MIN",

		"CHAR_MAX",
		"INT_MAX",
		"LONG_MAX",
		"SHRT_MAX",

		"SCHAR_MIN",
		"SINT_MIN",
		"SLONG_MIN",
		"SSHRT_MIN",

		"SCHAR_MAX",
		"SINT_MAX",
		"SLONG_MAX",
		"SSHRT_MAX",
		"__func__",
		"__VA_ARGS__",
		NULL,
	} },
	{ 14, {
		"LLONG_MIN",
		"LLONG_MAX",
		"ULLONG_MAX",

		"INT8_MIN",
		"INT16_MIN",
		"INT32_MIN",
		"INT64_MIN",

		"INT8_MAX",
		"INT16_MAX",
		"INT32_MAX",
		"INT64_MAX",

		"UINT8_MAX",
		"UINT16_MAX",
		"UINT32_MAX",
		"UINT64_MAX",

		"INT_LEAST8_MIN",
		"INT_LEAST16_MIN",
		"INT_LEAST32_MIN",
		"INT_LEAST64_MIN",

		"INT_LEAST8_MAX",
		"INT_LEAST16_MAX",
		"INT_LEAST32_MAX",
		"INT_LEAST64_MAX",

		"UINT_LEAST8_MAX",
		"UINT_LEAST16_MAX",
		"UINT_LEAST32_MAX",
		"UINT_LEAST64_MAX",

		"INT_FAST8_MIN",
		"INT_FAST16_MIN",
		"INT_FAST32_MIN",
		"INT_FAST64_MIN",

		"INT_FAST8_MAX",
		"INT_FAST16_MAX",
		"INT_FAST32_MAX",
		"INT_FAST64_MAX",

		"UINT_FAST8_MAX",
		"UINT_FAST16_MAX",
		"UINT_FAST32_MAX",
		"UINT_FAST64_MAX",

		"INTPTR_MIN",
		"INTPTR_MAX",
		"UINTPTR_MAX",

		"INTMAX_MIN",
		"INTMAX_MAX",
		"UINTMAX_MAX",

		"PTRDIFF_MIN",
		"PTRDIFF_MAX",
		"SIG_ATOMIC_MIN",
		"SIG_ATOMIC_MAX",

		"SIZE_MAX",
		"WCHAR_MIN",
		"WCHAR_MAX",
		"WINT_MIN",
		"WINT_MAX",
		NULL,
	} },
	{ 14, {
		"FLT_RADIX",
		"FLT_ROUNDS",
		"FLT_DIG",
		"FLT_MANT_DIG",
		"FLT_EPSILON",
		"DBL_DIG",
		"DBL_MANT_DIG",
		"DBL_EPSILON",

		"LDBL_DIG",
		"LDBL_MANT_DIG",
		"LDBL_EPSILON",
		"FLT_MIN",
		"FLT_MAX",
		"FLT_MIN_EXP",
		"FLT_MAX_EXP",
		"FLT_MIN_10_EXP",
		"FLT_MAX_10_EXP",

		"DBL_MIN",
		"DBL_MAX",
		"DBL_MIN_EXP",
		"DBL_MAX_EXP",
		"DBL_MIN_10_EXP",
		"DBL_MAX_10_EXP",
		"LDBL_MIN",
		"LDBL_MAX",
		"LDBL_MIN_EXP",
		"LDBL_MAX_EXP",

		"LDBL_MIN_10_EXP",
		"LDBL_MAX_10_EXP",
		"HUGE_VAL",
		NULL,
	} },
	{ 14, {
		"CLOCKS_PER_SEC",
		"NULL",
		"LC_ALL",
		"LC_COLLATE",
		"LC_CTYPE",
		"LC_MONETARY",

		"LC_NUMERIC",
		"LC_TIME",
		NULL,
	} },
	{ 14, {
		"SIG_DFL",
		"SIG_ERR",
		"SIG_IGN",
		"SIGABRT",
		"SIGFPE",
		"SIGILL",
		"SIGHUP",
		"SIGINT",
		"SIGSEGV",
		"SIGTERM",
		// Add POSIX signals as well...
		"SIGABRT",
		"SIGALRM",
		"SIGCHLD",
		"SIGCONT",
		"SIGFPE",
		"SIGHUP",
		"SIGILL",
		"SIGINT",
		"SIGKILL",
		"SIGPIPE",
		"SIGQUIT",
		"SIGSEGV",

		"SIGSTOP",
		"SIGTERM",
		"SIGTRAP",
		"SIGTSTP",
		"SIGTTIN",
		"SIGTTOU",
		"SIGUSR1",
		"SIGUSR2",
		NULL,
	} },
	{ 14, {
		"_IOFBF",
		"_IOLBF",
		"_IONBF",
		"BUFSIZ",
		"EOF",
		"WEOF",
		"FOPEN_MAX",
		"FILENAME_MAX",
		"L_tmpnam",
		NULL,
	} },
	{ 14, {
		"SEEK_CUR",
		"SEEK_END",
		"SEEK_SET",
		"TMP_MAX",
		"EXIT_FAILURE",
		"EXIT_SUCCESS",
		"RAND_MAX",

		"stdin",
		"stdout",
		"stderr",
		// POSIX 2001, in unistd.h
		"STDIN_FILENO",
		"STDOUT_FILENO",
		"STDERR_FILENO",
		// used in assert.h
		"NDEBUG",
		NULL,
	} },
	{ 14, {
		// POSIX 2001
		"SIGBUS",
		"SIGPOLL",
		"SIGPROF",
		"SIGSYS",
		"SIGURG",
		"SIGVTALRM",
		"SIGXCPU",
		"SIGXFSZ",
		// non-POSIX signals
		"SIGWINCH",
		"SIGINFO",
		// Add POSIX errors as well.		List comes from:
		// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/errno.h.html
		"E2BIG",
		"EACCES",
		"EADDRINUSE",
		"EADDRNOTAVAIL",
		"EAFNOSUPPORT",
		"EAGAIN",
		"EALREADY",
		"EBADF",

		"EBADMSG",
		"EBUSY",
		"ECANCELED",
		"ECHILD",
		"ECONNABORTED",
		"ECONNREFUSED",
		"ECONNRESET",
		"EDEADLK",

		"EDESTADDRREQ",
		"EDOM",
		"EDQUOT",
		"EEXIST",
		"EFAULT",
		"EFBIG",
		"EHOSTUNREACH",
		"EIDRM",
		"EILSEQ",

		"EINPROGRESS",
		"EINTR",
		"EINVAL",
		"EIO",
		"EISCONN",
		"EISDIR",
		"ELOOP",
		"EMFILE",
		"EMLINK",
		"EMSGSIZE",
		NULL,
	} },
	{ 14, {
		"EMULTIHOP",
		"ENAMETOOLONG",
		"ENETDOWN",
		"ENETRESET",
		"ENETUNREACH",
		"ENFILE",
		"ENOBUFS",
		"ENODATA",

		"ENODEV",
		"ENOENT",
		"ENOEXEC",
		"ENOLCK",
		"ENOLINK",
		"ENOMEM",
		"ENOMSG",
		"ENOPROTOOPT",
		"ENOSPC",
		"ENOSR",

		"ENOSTR",
		"ENOSYS",
		"ENOTBLK",
		"ENOTCONN",
		"ENOTDIR",
		"ENOTEMPTY",
		"ENOTRECOVERABLE",
		"ENOTSOCK",
		"ENOTSUP",

		"ENOTTY",
		"ENXIO",
		"EOPNOTSUPP",
		"EOVERFLOW",
		"EOWNERDEAD",
		"EPERM",
		"EPIPE",
		"EPROTO",

		"EPROTONOSUPPORT",
		"EPROTOTYPE",
		"ERANGE",
		"EROFS",
		"ESPIPE",
		"ESRCH",
		"ESTALE",
		"ETIME",
		"ETIMEDOUT",

		"ETXTBSY",
		"EWOULDBLOCK",
		"EXDEV",
		NULL,
	} },
};

int C_bracketPairs[] = {
	4, 12, 10,
};

static const struct {
	char c, c2, to[4];
} C_chars[] = {
	{ ',',   0, "" },
	{ '.',   0, "" },
	{ ';',   0, "" },
	{ ':',   0, "" },
	{ '+',   0, "" },
	{ '-', '>', { 0xe2, 0x86, 0x92, 0 } },
	{ '-',   0, "" },
	{ '*',   0, "" },
	{ '/',   0, "" },
	{ '%',   0, "" },
	{ '<', '=', { 0xe2, 0x89, 0xa4, 0 } },
	{ '>', '=', { 0xe2, 0x89, 0xa5, 0 } },
	{ '<', '<', { 0xc2, 0xab, 0 } },
	{ '>', '>', { 0xc2, 0xbb, 0 } },
	{ '<',   0, "" },
	{ '>',   0, "" },
	{ '=',   0, "" },
	{ '?',   0, "" },
	{ '!', '=', { 0xe2, 0x89, 0xa0, 0 } },
	{ '!', '!', { 0xe2, 0x80, 0xbc, 0 } },
	{ '!',   0, "" },
	{ '~',   0, "" },
	{ '&',   0, "" },
	{ '|',   0, "" },
};

int
c_state_common(struct state *s)
{
	switch(s->data[s->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const U32 startWord = s->index;
		while(isalnum(s->data[++s->index]) || s->data[s->index] == '_');
		const U32 nWord = s->index - startWord;
		s->index--;
		for(U32 k = 0; k < ARRLEN(C_keywords); k++)
		for(const char *const *ws = C_keywords[k].words, *w; (w = *ws); ws++)
			if(!strncmp(w, s->data + startWord, nWord) && !w[nWord]) {
				s->attr = COLOR_PAIR(C_keywords[k].pair);
				return 0;
			}
		if(!state_findword(s, s->data + startWord, nWord))
			s->attr = COLOR_PAIR(5);
		break;
	}
	case '.':
		if(!isdigit(s->data[s->index + 1]))
			goto case_char;
		state_push(s, s->state);
		s->state = C_STATE_NUMBER_FLOAT;
		s->attr = COLOR_PAIR(14);
		break;
	case '0':
		state_push(s, s->state);
		s->state = C_STATE_NUMBER_ZERO;
		s->attr = COLOR_PAIR(14);
		break;
	case '1' ... '9':
		state_push(s, s->state);
		s->state = C_STATE_NUMBER_DECIMAL;
		s->attr = COLOR_PAIR(14);
		break;
	case '\"':
		state_push(s, s->state);
		s->state = C_STATE_STRING;
		s->attr = COLOR_PAIR(11);
		break;
	case '\'':
	char_start:
		if(s->data[s->index + 1] == '\\') {
			U32 hexChars = 0;

			s->attr = A_REVERSE | COLOR_PAIR(2);
			s->index++;
			switch(s->data[s->index + 1]) {
			case '\n':
				s->index++;
				goto char_start;
			case 'a':
			case 'b':
			case 'e':
			case 'f':
			case 'n':
			case 'r':
			case 't':
			case 'v':
			case '\\':
			case '\"':
			case '\'':
				break;
			case 'U': hexChars = 8; break;
			case 'u': hexChars = 4; break;
			case 'x': hexChars = 2; break;
			default:
				return 0;
			}
			s->index++;
			while(hexChars) {
				if(!isxdigit(s->data[s->index + 1]))
					return 0;
				s->index++;
				hexChars--;
			}
			s->attr = COLOR_PAIR(10);
		} else if(isprint(s->data[s->index + 1])) {
			s->index++;
			s->attr = COLOR_PAIR(14);
		}
		if(s->data[s->index + 1] != '\'')
			s->attr = A_REVERSE | COLOR_PAIR(2);
		else
			s->index++;
		break;
	case '(':
	case '[':
	case '{':
		if(state_addparan(s) <= 0)
			s->attr = COLOR_PAIR(4);
		else
			s->attr = A_BOLD | COLOR_PAIR(7);
		break;
	case ')':
	case ']':
	case '}': {
		const char counter = s->data[s->index] == ')' ? '(' :
						s->data[s->index] == ']' ? '[' : '{';
		switch(state_addcounterparan(s, counter)) {
		case 0: s->attr = COLOR_PAIR(4); break;
		case 1: s->attr = A_BOLD | COLOR_PAIR(7); break;
		case 2: s->attr = A_REVERSE | COLOR_PAIR(2); break;
		}
		break;
	}
	case '/':
		if(s->data[s->index + 1] == '/') {
			s->index++;
			state_push(s, s->state);
			s->state = C_STATE_LINECOMMENT;
			s->attr = COLOR_PAIR(5);
			break;
		} else if(s->data[s->index + 1] == '*') {
			s->index++;
			state_push(s, s->state);
			s->state = C_STATE_MULTICOMMENT;
			s->attr = COLOR_PAIR(5);
			break;
		}
	/* fall through */
	default:
	case_char:
		for(U32 c = 0; c < ARRLEN(C_chars); c++)
			if(C_chars[c].c == s->data[s->index] &&
					C_chars[c].c2 == s->data[s->index + 1]) {
				s->attr = COLOR_PAIR(13);
				s->conceal = C_chars[c].to;
				s->index++;
				return 0;
			}
		for(U32 c = 0; c < ARRLEN(C_chars); c++)
			if(C_chars[c].c == s->data[s->index] && !C_chars[c].c2) {
				s->attr = COLOR_PAIR(13);
				break;
			}
	}
	return 0;
}

#include "string.h"
#include "number.h"
#include "preproc.h"
#include "comment.h"

int
c_state_default(struct state *s)
{
	switch(s->data[s->index]) {
	case '#':
		s->state = C_STATE_PREPROC;
		s->attr = COLOR_PAIR(5);
		break;
	default: {
		const char c = s->data[s->index];
		s->attr = 0;
		c_state_common(s);
		// if there was a word but no keyword, look ahead for a function call
		if(!s->attr && (isalpha(c) || c == '_')) {
			U32 i = s->index;
			while(isspace(s->data[i + 1]))
				i++;
			if(s->data[i + 1] == '(')
				s->attr = A_BOLD;
		}
	}
	}
	return 0;
}

int (*c_states[])(struct state *s) = {
	[C_STATE_DEFAULT] = c_state_default,

	[C_STATE_STRING] = c_state_string,

	[C_STATE_NUMBER_ZERO] = c_state_number_zero,
	[C_STATE_NUMBER_DECIMAL] = c_state_number_decimal,
	[C_STATE_NUMBER_HEX] = c_state_number_hex,
	[C_STATE_NUMBER_OCTAL] = c_state_number_octal,
	[C_STATE_NUMBER_BINARY] = c_state_number_binary,
	[C_STATE_NUMBER_EXP] = c_state_number_exp,
	[C_STATE_NUMBER_NEGEXP] = c_state_number_negexp,
	[C_STATE_NUMBER_FLOAT] = c_state_number_float,
	[C_STATE_NUMBER_ERRSUFFIX] = c_state_number_errsuffix,
	[C_STATE_NUMBER_LSUFFIX] = c_state_number_lsuffix,
	[C_STATE_NUMBER_USUFFIX] = c_state_number_usuffix,

	[C_STATE_PREPROC] = c_state_preproc,
	[C_STATE_PREPROC_COMMON] = c_state_preproc_common,
	[C_STATE_PREPROC_DEFINE] = c_state_preproc_define,
	[C_STATE_PREPROC_INCLUDE] = c_state_preproc_include,
	[C_STATE_PREPROC_UNDEF] = c_state_preproc_undef,

	[C_STATE_LINECOMMENT] = c_state_linecomment,
	[C_STATE_MULTICOMMENT] = c_state_multicomment,
};

