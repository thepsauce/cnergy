#ifndef __x11__

#include "cnergy.h"

int
clipboard_copy(const char *text, U32 nText)
{
	(void) text;
	(void) nText;
	return -1;
}

int
clipboard_paste(char **text)
{
	(void) text;
	return -1;
}

#endif
