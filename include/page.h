#ifndef INCLUDED_PAGE_H
#define INCLUDED_PAGE_H

typedef enum {
	COMP_TYPE_NULL,

	COMP_TYPE_EDIT_LINES,
	COMP_TYPE_EDIT_STATUS,
	COMP_TYPE_EDIT_VIEW,

	COMP_TYPE_MAX,
} comp_type_t;

typedef int (*comp_proc_t)(compid_t compid, call_t call);

struct component_class {
	char name[NAME_MAX];
	comp_proc_t proc;
};

struct component {
	comp_type_t type;
	unsigned hidden: 1;
	unsigned dead: 1;
	unsigned bind: 8;
	pageid_t parent;
	int x, y, w, h;
};

compid_t comp_new(comp_type_t type, pageid_t parent); 
void comp_delete(compid_t compid);
struct binding_mode *comp_getbindmode(compid_t compid);
#define comp_show(compid) all_comps[compid].hidden = false
#define comp_hide(compid) all_comps[compid].hidden = true 
#define comp_setrelativepos(compid, newx, newy, neww, newh) do { \
		struct component *cmp = all_comps + (compid); \
		struct page *page = (struct page*) (all_pages + cmp->parent); \
		cmp->x = (newx) + page->x; \
		cmp->x = (newy) + page->y; \
		cmp->w = (neww); \
		cmp->h = (newh); \
	} while(0)
#define comp_getclassname(compid) comp_classes[all_comps[compid].type].name
#define comp_getparent(compid) all_comps[compid].parent

typedef enum {
	PAGE_TYPE_NULL,

	PAGE_TYPE_EDIT,

	PAGE_TYPE_MAX,
} page_type_t;

typedef int (*page_proc_t)(pageid_t pageid, call_t call);

struct page_class {
	char name[NAME_MAX];
	page_proc_t proc;
	size_t size;
};

struct page {
	page_type_t type;
	int x, y, w, h;
};

#define page_get(cast, id) (cast) (all_pages + (size_t) (id) * sizeof(void*))
pageid_t page_new(page_type_t type);
void page_displayall(int x, int y, int w, int h);

extern struct page_class page_classes[PAGE_TYPE_MAX];
extern void *all_pages;
extern pageid_t n_pages;

extern struct component_class comp_classes[COMP_TYPE_MAX];
extern struct component *all_comps;
extern compid_t n_comps;
extern compid_t focus_comp;
extern int focus_x, focus_y;


// page_edit.c
struct page_edit {
	page_type_t type;
	int x, y, w, h;
	compid_t lines;
	compid_t status;
	compid_t view;
	bufferid_t bufid;
	size_t selection;
	int vScroll, hScroll;
};

#endif
