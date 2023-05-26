#include "cnergy.h"

#if _PLATFORM != _X11

int
clipboard_copy(const char *text, U32 nText)
{
	return -1;
}

int
clipboard_paste(char **text)
{
	return -1;
}

#endif
