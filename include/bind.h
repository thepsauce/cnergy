#ifndef INCLUDED_BIND_H
#define INCLUDED_BIND_H

// bind.c
/**
 * Binds are always defined in .cng files and allow the user to customize the bindings to their heart's content.
 * This also means that there need to be an abstract layer in between the user and the editor which here is the bindining_call
 * which each window type defines for its own although there are also global binding calls
 * For further information, see parser.h
 */

struct binding {
	unsigned nKeys;
	size_t szProgram;
	int *keys;
	void *program;
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

int *mode_allockeys(const int *keys, unsigned nKeys);
int modes_add(struct binding_mode *modes[WINDOW_MAX]);
struct binding_mode *mode_find(window_type_t type, const char *name);
int bind_find(const int *keys, struct binding **pBind);

#endif
