#include "cnergy.h"

int
parser_checkincludeword(struct parser *parser)
{
	return !strcmp(parser->word, "include") ? COMMIT : FAIL;
}

int
parser_addinclude(struct parser *parser)
{
	return parser_open(parser, fc_cache(fc_getbasefile(), parser->str)) < 0 ? FAIL : FINISH;
}
