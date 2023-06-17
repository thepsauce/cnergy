#ifndef INCLUDED_BIND_H
#define INCLUDED_BIND_H

// bind.c
/**
 * Binds are always defined in .cng files and allow the user to customize the bindings to their heart's content.
 * This also means that there need to be an abstract layer in between the user and the editor which here is the bindining_call
 * which each window type defines for its own although there are also global binding calls
 * For further information, see parser.h
 */

typedef enum {
	FBIND_CALL_USENUMBER 	= 1 << 0,
	FBIND_CALL_AND 			= 1 << 1,
	FBIND_CALL_OR 			= 1 << 2,
	FBIND_CALL_XOR 			= 1 << 3,
	FBIND_CALL_USECACHE 	= 1 << 4,
	FBIND_CALL_USEKEY 		= 1 << 5,
} bind_flags_t;

struct binding_call {
	event_type_t type;
	bind_flags_t flags;
	union {
		char input[64];
		ptrdiff_t param;
		void *ptr;
		char *str;
		char name[64];
	};
};

struct binding {
	unsigned nKeys;
	unsigned nCalls;
	int *keys;
	struct binding_call *calls;
};

enum {
	FBIND_MODE_SUPPLEMENTARY 	= 1 << 0,
	FBIND_MODE_SELECTION 		= 1 << 1,
	FBIND_MODE_TYPE 			= 1 << 2,
};

struct binding_mode {
	char name[64];
	unsigned flags;
	unsigned nBindings;
	struct binding *bindings;
	struct binding_mode *next;
};

extern struct binding_mode *all_modes[WINDOW_MAX];

char *mode_allocstr(const char *str, size_t nStr);
int *mode_allockeys(const int *keys, unsigned nKeys);
struct binding_call *mode_alloccalls(const struct binding_call *calls, unsigned nCalls);
int modes_add(struct binding_mode *modes[WINDOW_MAX]);
int bind_exec(const int *keys, ptrdiff_t param);

#endif
