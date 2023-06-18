#include "cnergy.h"

struct environment main_environment;

bool edit_call(windowid_t winid, call_t call);
bool bufferviewer_call(windowid_t winid, call_t call);
bool fileviewer_call(windowid_t winid, call_t call);

bool (*window_types[])(windowid_t winid, call_t call) = {
	[WINDOW_EDIT] = edit_call,
	[WINDOW_BUFFERVIEWER] = bufferviewer_call,
	[WINDOW_FILEVIEWER] = fileviewer_call,
};

// TODO: when this list is complete, add binary search or hashing (same goes for translate_key)
// Note that this is used by the parser (parser_key_call.c)
const struct call_info infoCalls[CALL_MAX] = {
	[CALL_COLORWINDOW] = { "COLOR", 0 },
	[CALL_CLOSEWINDOW] = { "CLOSE", 0 },
	[CALL_NEWWINDOW] = { "NEW", 0 },
	[CALL_OPENWINDOW] = { "OPEN", 0 },
	[CALL_HSPLIT] = { "HSPLIT", 0 },
	[CALL_VSPLIT] = { "VSPLIT", 0 },
	[CALL_QUIT] = { "QUIT", 0 },

	[CALL_DELETESELECTION] = { "DELETE", 0 },
	[CALL_COPY] = { "COPY", 0 },
	[CALL_PASTE] = { "PASTE", 0 },
	[CALL_UNDO] = { "UNDO", 0 },
	[CALL_REDO] = { "REDO", 0 },
	[CALL_WRITEFILE] = { "WRITE", 0 },
	[CALL_READFILE] = { "READ", 0 },

	[CALL_FIND] = { "FIND", 1 },

	[CALL_CHOOSE] = { "CHOOSE", 0 },
	[CALL_SORTALPHABETICAL] = { "SORT_ALPHABETICAL", 0 },
	[CALL_SORTCHANGETIME] = { "SORT_CHANGETIME", 0 },
	[CALL_SORTMODIFICATIONTIME] = { "SORT_MODIFICATIONTIME", 0 },
	[CALL_TOGGLESORTTYPE] = { "TOGGLE_SORT_TYPE", 0 },
	[CALL_TOGGLESORTREVERSE] = { "TOGGLE_SORT_REVERSE", 0 },
	[CALL_TOGGLEHIDDEN] = { "TOGGLE_HIDDEN", 0 },
};

static void
environment_recursiverender(windowid_t winid)
{
	/* Do not need to render empty windows */
	if(window_getarea(winid) != 0)
		(*window_types[window_gettype(winid)])(winid, CALL_RENDER);
	if(window_getbelow(winid) != ID_NULL)
		environment_recursiverender(window_getbelow(winid));
	if(window_getright(winid) != ID_NULL)
		environment_recursiverender(window_getright(winid));
}

void
environment_call(call_t call)
{
	switch(call) {
	case CALL_RENDER:
		environment_recursiverender(focus_window);
		break;
	case CALL_SETMODE: {
		const char *const mode = (const char*) main_environment.A;
		window_setbindmode(focus_window,
				mode_find(window_gettype(focus_window), mode));
		break;
	}
	case CALL_CLOSEWINDOW:
		window_close(focus_window);
		if(!n_windows) {
			endwin();
			printf("all windows closed exit\n");
			exit(0);
		}
		break;
	case CALL_MOVEWINDOW_RIGHT:
	case CALL_MOVEWINDOW_BELOW: {
		const ptrdiff_t amount = main_environment.A;
		ptrdiff_t i = 0;
		// get the right directional function
		windowid_t (*const next)(windowid_t) =
			call == CALL_MOVEWINDOW_RIGHT ?
				(amount > 0 ? window_right : window_left) :
			amount > 0 ? window_below : window_above;
		const ptrdiff_t di = amount < 0 ? -1 : 1;
		for(windowid_t wid = focus_window; i != amount && (wid = (*next)(wid)) != ID_NULL; i += di)
			focus_window = wid;
		main_environment.z = i == amount;
		break;
	}
	case CALL_VSPLIT: {
		windowid_t winid;

		winid = window_dup(focus_window);
		if(winid == ID_NULL) {
			main_environment.z = false;
			break;
		}
		window_attach(focus_window, winid, ATT_WINDOW_HORIZONTAL);
		break;
	}
	case CALL_HSPLIT: {
		const windowid_t winid = window_dup(focus_window);
		if(winid == ID_NULL) {
			main_environment.z = false;
			break;
		}
		window_attach(focus_window, winid, ATT_WINDOW_VERTICAL);
		break;
	}
	case CALL_OPENWINDOW: {
		const windowid_t winid = window_new(WINDOW_FILEVIEWER);
		if(winid == ID_NULL) {
			main_environment.z = false;
			break;
		}
		window_delete(focus_window);
		// it is safe to use a deleted window as only a flag gets set
		window_copylayout(winid, focus_window);
		break;
	}
	case CALL_NEWWINDOW: {
		bufferid_t bufid;
		windowid_t winid;

		bufid = buffer_new(0);
		if(bufid == ID_NULL) {
			main_environment.z = false;
			break;
		}
		winid = edit_new(bufid, NULL);
		if(winid != ID_NULL) {
			window_attach(focus_window, winid, ATT_WINDOW_UNSPECIFIED);
			focus_window = winid;
		} else {
			buffer_free(bufid);
			main_environment.z = false;
		}
		break;
	}
	case CALL_COLORWINDOW:
		// TODO: open a color window or focus an existing one
		break;
	default:
		// not a general bind call
		break;
	}
	(*window_types[window_gettype(focus_window)])(focus_window, call);
};

int
environment_loadandexec(void *program, size_t szProgram)
{
	/* load program */
	memcpy(main_environment.memory + main_environment.mp, program, szProgram);
	main_environment.ip = main_environment.mp;
	main_environment.mp += szProgram;
	main_environment.sp = sizeof(main_environment.memory);
#define REGISTER_LD(r) \
	case INSTR_LD##r: main_environment.r = *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break
#define REGISTER_LDO(r) \
	case INSTR_LD##r##O: main_environment.r = (ptrdiff_t) main_environment.memory; break
#define REGISTER_COMPATIBLELD(r1, r2) \
	case INSTR_LD##r1##r2: main_environment.r1 = main_environment.r2; break
#define REGISTER_LDMEM(r) \
	case INSTR_LD##rMEMB: main_environment.r = (ptrdiff_t) *(char*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_LD##rMEMI: main_environment.r = (ptrdiff_t) *(int*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_LD##rMEMP: main_environment.r = (ptrdiff_t) *(ptrdiff_t*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break
#define REGISTER_STACK(r) \
	case INSTR_PSH##r: main_environment.sp -= sizeof(ptrdiff_t); *(ptrdiff_t*) (main_environment.memory + main_environment.sp) = main_environment.r; break; \
	case INSTR_POP##r: main_environment.r = *(ptrdiff_t*) (main_environment.memory + main_environment.sp); main_environment.sp += sizeof(ptrdiff_t); break
#define REGISTER_MATH(r) \
	case INSTR_ADD##r: main_environment.r += *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_SUB##r: main_environment.r -= *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_MUL##r: main_environment.r *= *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_DIV##r: main_environment.r /= *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_INC##r: main_environment.r++; break; \
	case INSTR_DEC##r: main_environment.r--; break

	while(1) {
		switch(*(uint8_t*) (main_environment.memory + main_environment.ip++)) {
		/* ldr [val] */
		REGISTER_LD(A);
		REGISTER_LD(B);
		REGISTER_LD(M);
		REGISTER_LD(N);
		/* ldor [val] */
		REGISTER_LDO(A);
		REGISTER_LDO(B);
		REGISTER_LDO(M);
		REGISTER_LDO(N);
		/* ldr1 r2 */
		REGISTER_COMPATIBLELD(A, B);
		REGISTER_COMPATIBLELD(A, M);
		REGISTER_COMPATIBLELD(A, N);

		REGISTER_COMPATIBLELD(B, A);
		REGISTER_COMPATIBLELD(B, M);
		REGISTER_COMPATIBLELD(B, N);

		REGISTER_COMPATIBLELD(M, B);
		REGISTER_COMPATIBLELD(M, A);
		REGISTER_COMPATIBLELD(M, N);

		REGISTER_COMPATIBLELD(N, B);
		REGISTER_COMPATIBLELD(N, A);
		REGISTER_COMPATIBLELD(N, M);
		/* pushr */
		/* popr */
		REGISTER_STACK(A);
		REGISTER_STACK(B);
		REGISTER_STACK(M);
		REGISTER_STACK(N);
		/* [opr]r [val] */
		/* [opr]r */
		REGISTER_MATH(A);
		REGISTER_MATH(B);
		REGISTER_MATH(M);
		REGISTER_MATH(N);
		case INSTR_JMP: main_environment.ip += *(size_t*) (main_environment.memory + main_environment.ip); break;
		case INSTR_JZ:
			main_environment.ip += main_environment.z ? *(ptrdiff_t*) (main_environment.memory + main_environment.ip) :
				(ptrdiff_t) sizeof(ptrdiff_t);
			break;
		case INSTR_CALL: environment_call(*(call_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(call_t); break;
		case INSTR_EXIT: return *(int*) (main_environment.memory + main_environment.ip);
		default:
			return -1;
		}
	}
}
