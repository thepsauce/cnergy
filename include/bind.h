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
	size_t nKeys;
	size_t szProgram;
	char *keys;
	void *program;
};

struct binding_mode {
	char name[NAME_MAX];
	unsigned nBindings;
	struct binding *bindings;
};

#define mode_isnormal(mode) !(mode_isvisual(mode)||mode_isinsert(mode)||mode_issupp(mode))
#define mode_isvisual(mode) ((mode)->name[strlen((mode)->name)-1]=='*')
#define mode_isinsert(mode) ((mode)->name[0]=='$')
#define mode_issupp(mode) ((mode)->name[0]=='.')

extern struct binding_mode *all_modes;
extern unsigned n_modes;

void debug_modes_print(struct binding_mode *modes, unsigned nModes);
char *mode_allockeys(const char *keys, size_t nKeys);
char *mode_allocstring(const char *string, size_t nString);
void *mode_allocprogram(const void *program, size_t szProgram);
struct binding_mode *mode_new(const char *name);
int mode_addbind(struct binding_mode *mode,
		const struct binding *bind);
int mode_addallbinds(struct binding_mode *mode,
		const struct binding_mode *from);
struct binding_mode *mode_find(const char *name);
int bind_find(const char *keys, struct binding **pBind);

#endif
