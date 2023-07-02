#include "cnergy.h"

struct environment main_environment;

// TODO: maybe when this list is complete, add binary search or hashing
// Note that this is used by the parser (parse_instruction.c)
const char *callNames[CALL_MAX] = {
	[CALL_NULL] = "",
	[CALL_VSPLIT] = "VSPLIT",
	[CALL_HSPLIT] = "HSPLIT",
	[CALL_COLORPAGE] = "COLOR",
	[CALL_OPENPAGE] = "OPEN",
	[CALL_CLOSEPAGE] = "CLOSE",
	[CALL_NEWPAGE] = "NEW",
	[CALL_MOVERIGHTPAGE] = "MOVERIGHTPAGE",
	[CALL_MOVEBELOWPAGE] = "MOVEBELOWPAGE",
	[CALL_QUIT] = "QUIT",

	[CALL_NORMAL] = "NORMAL",
	[CALL_INSERT] = "INSERT",
	[CALL_VISUAL] = "VISUAL",
	[CALL_ASSERTSTRING] = "ASSERTSTRING",
	[CALL_ASSERTCHAR] = "ASSERTCHAR",
	[CALL_MOVECURSOR] = "MOVECURSOR",
	[CALL_MOVEHORZ] = "MOVEHORZ",
	[CALL_MOVEVERT] = "MOVEVERT",
	[CALL_INSERTSTRING] = "INSERTSTRING",
	[CALL_INSERTCHAR] = "INSERTCHAR",
	[CALL_DELETE] = "DELETE",
	[CALL_DELETELINE] = "DELETELINE",
	[CALL_DELETESELECTION] = "DELETESELECTION",
	[CALL_COPY] = "COPY",
	[CALL_PASTE] = "PASTE",
	[CALL_UNDO] = "UNDO",
	[CALL_REDO] = "REDO",
	[CALL_WRITEFILE] = "WRITE",
	[CALL_READFILE] = "READ",
	[CALL_FIND] = "FIND",

	[CALL_CONFIRM] = "CONFIRM",
	[CALL_CANCEL] = "CANCEL",
	[CALL_SWAP] = "SWAP",
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
	[INSTR_JNZ] = "JNZ", \
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

void
environment_call(call_t call)
{
	switch(call) {
	case CALL_NORMAL:
		break;
	case CALL_INSERT:
		break;
	case CALL_VISUAL:
		break;
	case CALL_CLOSEPAGE:
		page_close(comp_getparent(focus_comp));
		if(!n_pages) {
			endwin();
			printf("all pages closed exit\n");
			exit(0);
		}
		break;
	default:
		break;
	}
};

int
environment_loadandexec(void *program, size_t szProgram)
{
	const size_t oldmp = main_environment.mp;
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
	case INSTR_JNZ: \
		main_environment.ip += !main_environment.z ? *(ptrdiff_t*) (main_environment.memory + main_environment.ip) : \
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
			main_environment.mp = oldmp;
			return -1;
		}
	}
	main_environment.mp = oldmp;
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
	const size_t oldmp = main_environment.mp;
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
	case INSTR_JNZ: printf("jnz %zd", *(size_t*) (main_environment.memory + main_environment.ip)); main_environment.ip += (ptrdiff_t) sizeof(ptrdiff_t); break; \
	case INSTR_CALL: printf("call %s", callNames[*(call_t*) (main_environment.memory + main_environment.ip)]); main_environment.ip += sizeof(call_t); break; \
	case INSTR_EXIT: printf("exit %u", *(int*) (main_environment.memory + main_environment.ip)); main_environment.ip += sizeof(int); break;

	while(main_environment.ip < main_environment.mp) {
		const instr_t instr = *(instr_t*) (main_environment.memory + main_environment.ip);
		main_environment.ip += sizeof(instr_t);
		switch(instr) {
		ALLINSTRUCTIONS
		default:
			main_environment.mp = oldmp;
			return -1;
		}
		printf("\n");
	}
	main_environment.mp = oldmp;
	return 0;
}
