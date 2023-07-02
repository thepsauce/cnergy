#include "cnergy.h"

#define DEFINE_PAGE_PROC(name) int name(pageid_t pageid, call_t call)
#define DEFINE_COMP_PROC(name) int name(compid_t compid, call_t)

DEFINE_PAGE_PROC(page_edit_proc);
struct page_class page_classes[PAGE_TYPE_MAX] = {
	[PAGE_TYPE_EDIT] = { "edit", page_edit_proc, sizeof(struct page_edit) },
};

void *all_pages;
pageid_t n_pages;

DEFINE_COMP_PROC(edit_lines_proc);
DEFINE_COMP_PROC(edit_status_proc);
DEFINE_COMP_PROC(edit_view_proc);
struct component_class comp_classes[COMP_TYPE_MAX] = {
	[COMP_TYPE_EDIT_LINES] = { "edit:lines", edit_lines_proc },
	[COMP_TYPE_EDIT_STATUS] = { "edit:status", edit_status_proc },
	[COMP_TYPE_EDIT_VIEW] = { "edit:view", edit_view_proc },
};

struct component *all_comps;
compid_t n_comps;
compid_t focus_comp;
int focus_x, focus_y;

pageid_t
page_new(page_type_t type)
{
	void *page;
	pageid_t pageid;
	size_t sz, szAligned;
	pageid_t units;

	if(type >= PAGE_TYPE_MAX)
		return ID_NULL;
	sz = page_classes[type].size;
	szAligned = (sz + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
	units = szAligned / sizeof(void*);
	page = realloc(all_pages, sizeof(void*) * (n_pages + units));
	if(page == NULL)
		return ID_NULL;
	all_pages = page;
	pageid = n_pages;
	page += n_pages;
	n_pages += units;
	memset(page, 0, sz);
	*(page_type_t*) page = type;
	page_classes[type].proc(pageid, PAGE_CONSTRUCT);
	return pageid;
}

void
page_close(pageid_t pageid)
{
}

compid_t
comp_new(comp_type_t type, pageid_t parent)
{
	struct component *comp;
	compid_t compid;

	for(compid = 0; compid < n_comps; compid++)
		if(all_comps[compid].dead)
			break;
	if(compid == n_comps) {
		comp = realloc(all_comps, sizeof(*all_comps)
				* (n_comps + 1));
		if(comp == NULL)
			return ID_NULL;
		all_comps = comp;
		n_comps++;
	} else {
		comp = all_comps;
	}
	comp += compid;
	memset(comp, 0, sizeof(*comp));
	comp->type = type;
	comp->parent = parent;
	return compid;
}

void
comp_delete(compid_t compid)
{
	all_comps[compid].dead = true;
	/* update the number of components if needed */
	for(compid_t cid = compid; cid < n_comps; cid++)
		if(!all_comps[cid].dead)
			/* we cannot cap the n_comps because there are still components above that are alive */
			return;
	/* find the first component that is alive */
	while(compid > 0) {
		compid--;
		if(!all_comps[compid].dead)
			break;
	}
	n_comps = compid;
}

struct binding_mode *
comp_getbindmode(compid_t compid)
{
	struct binding_mode *mode;
	char name[NAME_MAX];
	const unsigned bind = all_comps[compid].bind;
	if(bind == 1) {
		name[0] = '$';
		name[1] = 0;
	} else {
		name[0] = 0;
	}
	strcat(name, comp_getclassname(compid));
	if(bind == 2)
		strcat(name, "*");
	mode = mode_find(name);
	if(!mode) {
		if(bind == 1)
			mode = mode_find("$");
		else if(bind == 2)
			mode = mode_find("*");
		else
			mode = mode_find("");
	}
	return mode;
}

void
page_displayall(int x, int y, int w, int h)
{
	struct page *page;
	/* layout the pages (just the first for testing now) */
	page = page_get(struct page*, 0);
	page->x = x;
	page->y = y;
	page->w = w;
	page->h = h;
	page_classes[page->type].proc(0, PAGE_LAYOUT);
	/* render all components */
	for(compid_t i = 0; i < n_comps; i++)
		if(!all_comps[i].dead && !all_comps[i].hidden)
			comp_classes[all_comps[i].type].proc(i, COMP_RENDER);
}
