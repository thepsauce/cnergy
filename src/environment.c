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

// TODO: maybe when this list is complete, add binary search or hashing
// Note that this is used by the parser (parser_bind.c)
const char *callNames[CALL_MAX] = {
	[CALL_SETMODE] = "SETMODE",
	[CALL_VSPLIT] = "VSPLIT",
	[CALL_HSPLIT] = "HSPLIT",
	[CALL_COLORWINDOW] = "COLORWINDOW",
	[CALL_OPENWINDOW] = "OPENWINDOW",
	[CALL_CLOSEWINDOW] = "CLOSEWINDOW",
	[CALL_NEWWINDOW] = "NEWWINDOW",
	[CALL_MOVEWINDOW_RIGHT] = "MOVEWINDOW_RIGHT",
	[CALL_MOVEWINDOW_BELOW] = "MOVEWINDOW_BELOW",
	[CALL_QUIT] = "QUIT",

	[CALL_CREATE] = "CREATE",
	[CALL_DESTROY] = "DESTROY",
	[CALL_RENDER] = "RENDER",
	[CALL_TYPE] = "TYPE",
	[CALL_ASSERT] = "ASSERT",
	[CALL_ASSERTCHAR] = "ASSERTCHAR",
	[CALL_MOVECURSOR] = "MOVECURSOR",
	[CALL_MOVEHORZ] = "MOVEHORZ",
	[CALL_MOVEVERT] = "MOVEVERT",
	[CALL_INSERT] = "INSERT",
	[CALL_INSERTCHAR] = "INSERTCHAR",
	[CALL_DELETE] = "DELETE",
	[CALL_DELETELINE] = "DELETELINE",
	[CALL_DELETESELECTION] = "DELETESELECTION",
	[CALL_COPY] = "COPY",
	[CALL_PASTE] = "PASTE",
	[CALL_UNDO] = "UNDO",
	[CALL_REDO] = "REDO",
	[CALL_WRITEFILE] = "WRITEFILE",
	[CALL_READFILE] = "READFILE",
	[CALL_FIND] = "FIND",

	[CALL_CHOOSE] = "CHOOSE",
	[CALL_TOGGLEHIDDEN] = "TOGGLEHIDDEN",
	[CALL_TOGGLESORTTYPE] = "TOGGLESORTTYPE",
	[CALL_TOGGLESORTREVERSE] = "TOGGLESORTREVERSE",
	[CALL_SORTMODIFICATIONTIME] = "SORTMODIFICATIONTIME",
	[CALL_SORTALPHABETICAL] = "SORTALPHABETICAL",
	[CALL_SORTCHANGETIME] = "SORTCHANGETIME",
};

const char *instrNames[INSTR_MAX] = {
#define PRESTRING(s) #s
#define REGISTER_LD(r) \
	[INSTR_LD##r] = PRESTRING(LD##r),
#define REGISTER_LDO(r) \
	[INSTR_LD##r##O] = PRESTRING(LD##r##O),
#define REGISTER_COMPATIBLELD(r1, r2) \
	[INSTR_LD##r1##r2] = PRESTRING(LD##r1##r2),
#define REGISTER_LDMEM(r) \
	[INSTR_LD##r##BYTE] = PRESTRING(LD##r), \
	[INSTR_LD##r##INT] = PRESTRING(LD##r), \
	[INSTR_LD##r##PTR] = PRESTRING(LD##r),
#define REGISTER_STACK(r) \
	[INSTR_PSH##r] = PRESTRING(PSH##r), \
	[INSTR_POP##r] = PRESTRING(POP##r),
#define REGISTER_MATH(r) \
	[INSTR_ADD##r] = PRESTRING(ADD##r), \
	[INSTR_SUB##r] = PRESTRING(SUB##r), \
	[INSTR_MUL##r] = PRESTRING(MUL##r), \
	[INSTR_DIV##r] = PRESTRING(DIV##r), \
	[INSTR_INC##r] = PRESTRING(INC##r), \
	[INSTR_DEC##r] = PRESTRING(DEC##r),
#define REGISTER_MISC \
	[INSTR_JMP] = "JMP", \
	[INSTR_JZ] = "JZ", \
	[INSTR_CALL] = "CALL", \
	[INSTR_EXIT] = "EXIT",
	ALLINSTRUCTIONS
#undef REGISTER_LD
#undef REGISTER_LDO
#undef REGISTER_COMPATIBLELD
#undef REGISTER_LDMEM
#undef REGISTER_STACK
#undef REGISTER_MATH
#undef REGISTER_MISC
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
	case INSTR_LD##r: main_environment.r = *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break;
#define REGISTER_LDO(r) \
	case INSTR_LD##r##O: main_environment.r = (ptrdiff_t) main_environment.memory; break;
#define REGISTER_COMPATIBLELD(r1, r2) \
	case INSTR_LD##r1##r2: main_environment.r1 = main_environment.r2; break;
#define REGISTER_LDMEM(r) \
	case INSTR_LD##r##BYTE: main_environment.r = (ptrdiff_t) *(char*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_LD##r##INT: main_environment.r = (ptrdiff_t) *(int*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_LD##r##PTR: main_environment.r = (ptrdiff_t) *(ptrdiff_t*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break;
#define REGISTER_STACK(r) \
	case INSTR_PSH##r: main_environment.sp -= sizeof(ptrdiff_t); *(ptrdiff_t*) (main_environment.memory + main_environment.sp) = main_environment.r; break; \
	case INSTR_POP##r: main_environment.r = *(ptrdiff_t*) (main_environment.memory + main_environment.sp); main_environment.sp += sizeof(ptrdiff_t); break;
#define REGISTER_MATH(r) \
	case INSTR_ADD##r: main_environment.r += *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_SUB##r: main_environment.r -= *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_MUL##r: main_environment.r *= *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_DIV##r: main_environment.r /= *(ptrdiff_t*) (main_environment.memory + main_environment.ip); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_INC##r: main_environment.r++; break; \
	case INSTR_DEC##r: main_environment.r--; break;
#define REGISTER_MISC \
	case INSTR_JMP: main_environment.ip += *(size_t*) (main_environment.memory + main_environment.ip); break; \
	case INSTR_JZ: \
		main_environment.ip += main_environment.z ? *(ptrdiff_t*) (main_environment.memory + main_environment.ip) : \
			(ptrdiff_t) sizeof(ptrdiff_t); \
		break; \
	case INSTR_CALL: environment_call(*(call_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(call_t); break; \
	case INSTR_EXIT: return *(int*) (main_environment.memory + main_environment.ip); \

	while(1) {
		const instr_t instr = *(instr_t*) (main_environment.memory + main_environment.ip);
		main_environment.ip += sizeof(instr_t);
		switch(instr) {
		ALLINSTRUCTIONS
		default:
			return -1;
		}
	}
#undef REGISTER_LD
#undef REGISTER_LDO
#undef REGISTER_COMPATIBLELD
#undef REGISTER_LDMEM
#undef REGISTER_STACK
#undef REGISTER_MATH
#undef REGISTER_MISC
}

int
environment_loadandprint(void *program, size_t szProgram)
{
	/* load program */
	memcpy(main_environment.memory + main_environment.mp, program, szProgram);
	main_environment.ip = main_environment.mp;
	main_environment.mp += szProgram;
	main_environment.sp = sizeof(main_environment.memory);
#define REGISTER_LD(r) \
	case INSTR_LD##r: printf("ld"#r" %zd", *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break;
#define REGISTER_LDO(r) \
	case INSTR_LD##r##O: printf("ldo"#r); break;
#define REGISTER_COMPATIBLELD(r1, r2) \
	case INSTR_LD##r1##r2: printf("ld"#r1#r2); break;
#define REGISTER_LDMEM(r) \
	case INSTR_LD##r##BYTE: printf("ld"#r" byte %zd", (ptrdiff_t) *(char*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip))); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_LD##r##INT: printf("ld"#r" byte %zd", (ptrdiff_t) *(int*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip))); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_LD##r##PTR: printf("ld"#r" ptr %zd", (ptrdiff_t) *(ptrdiff_t*) (main_environment.memory + *(ptrdiff_t*) (main_environment.memory + main_environment.ip))); main_environment.ip += sizeof(ptrdiff_t); break;
#define REGISTER_STACK(r) \
	case INSTR_PSH##r: printf("psh"#r); break; \
	case INSTR_POP##r: printf("pop"#r); break;
#define REGISTER_MATH(r) \
	case INSTR_ADD##r: printf("add"#r" %zd", *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_SUB##r: printf("sub"#r" %zd", *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_MUL##r: printf("mul"#r" %zd", *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_DIV##r: printf("div"#r" %zd", *(ptrdiff_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(ptrdiff_t); break; \
	case INSTR_INC##r: printf("inc"#r); break; \
	case INSTR_DEC##r: printf("dec"#r); break;
#define REGISTER_MISC \
	case INSTR_JMP: printf("jmp %zd", *(size_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += (ptrdiff_t) sizeof(ptrdiff_t); break; \
	case INSTR_JZ: printf("jz %zd", *(size_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += (ptrdiff_t) sizeof(ptrdiff_t); break; \
	case INSTR_CALL: printf("call %s", callNames[*(call_t*) (main_environment.memory + main_environment.ip)]); main_environment.ip += sizeof(call_t); break; \
	case INSTR_EXIT: printf("exit %u", *(int*) (main_environment.memory + main_environment.ip)); break;

	while(main_environment.ip < main_environment.mp) {
		const instr_t instr = *(instr_t*) (main_environment.memory + main_environment.ip);
		main_environment.ip += sizeof(instr_t);
		switch(instr) {
		ALLINSTRUCTIONS
		default:
			return -1;
		}
		printf("\n");
	}
	return 0;
}
