#include "cnergy.h"

int all_settings[SET_MAX];

int
main(int argc, char **argv)
{
	if(argc < 2) {
		fprintf(stderr, "expected one or more arguments (regex expressions)\n");
		return -1;
	}
	struct regex_matcher matcher;
	for(int a = 1; a < argc; a++) {
		int code;
		if((code = regex_addpattern(&matcher, argv[a])) < 0) {
			fprintf(stderr, "epression no. %d failed with code %d\n", a, code);
			return -1;
		}
	}
	return 0;
}
