#include "cnergy.h"

static inline void
renderview(struct component *comp)
{

}

int
edit_view_proc(compid_t compid, call_t call)
{
	struct component *const comp = all_comps + compid;
	switch(call) {
	case COMP_RENDER:
		renderview(comp);
		break;
	default:
		break;
	}
	return 0;
}

static inline void
renderstatus(struct component *comp)
{

}

int
edit_status_proc(compid_t compid, call_t call)
{
	struct component *const comp = all_comps + compid;
	switch(call) {
	case COMP_RENDER:
		renderstatus(comp);
		break;
	default:
		break;
	}
	return 0;
}

static inline void
renderlines(struct component *comp)
{
	struct page_edit *const page = page_get(struct page_edit*, comp->parent);
	for(int y = 0; y < comp->h; y++) {
		const int n = y + page->vScroll + 1;
		mvprintw(comp->y + y, comp->x, "%*d", comp->w - 1, n);
	}
}

int
edit_lines_proc(compid_t compid, call_t call)
{
	struct component *const comp = all_comps + compid;
	switch(call) {
	case COMP_RENDER:
		renderlines(comp);
		break;
	default:
		break;
	}
	return 0;
}

int
page_edit_proc(pageid_t pageid, call_t call)
{
	struct page_edit *const page = page_get(struct page_edit*, pageid);
	switch(call) {
	case PAGE_CONSTRUCT:
		page->lines = comp_new(COMP_TYPE_EDIT_LINES, pageid);
		page->status = comp_new(COMP_TYPE_EDIT_STATUS, pageid);
		page->view = comp_new(COMP_TYPE_EDIT_VIEW, pageid);
		break;
	case PAGE_LAYOUT: {
		int x;

		if(!page->w || !page->h) {
			comp_hide(page->lines);
			comp_hide(page->status);
			comp_hide(page->view);
			break;
		}
		if(page->w < 8) {
			comp_hide(page->lines);
			x = 0;
		} else {
			comp_show(page->lines);
			x = 5;
		}
		comp_show(page->status);
		comp_show(page->view);
		comp_setrelativepos(page->lines, 0, 0, x, page->h);
		comp_setrelativepos(page->status, 0, page->h - 1, page->w, 1);
		comp_setrelativepos(page->view, 0, x, page->w - x, page->h - 1);
		break;
	}
	case PAGE_DESTRUCT:
		comp_delete(page->lines);
		comp_delete(page->status);
		comp_delete(page->view);
		break;
	default:
		break;
	}
	return 0;
}
