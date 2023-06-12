#include "cnergy.h"

static struct regex_node *all_nodes;
static regex_nodeid_t n_nodes;

typedef uint16_t regex_tests_t[256 / 8 / sizeof(uint16_t)];

enum {
	FREGEX_NODE_REPEAT		= 1 << 0,
	FREGEX_NODE_OPTIONAL	= 1 << 1,
	FREGEX_NODE_EXIT		= 1 << 1,
};

struct regex_node {
	unsigned flags;
	int attr;
	regex_tests_t tests;
	regex_nodeid_t *nodes;
	unsigned nNodes;
};

#define REGEX_CHECK(t, b) ((t)[(uint8_t)(b)>>4]&(1<<((uint8_t)(b)&0xf)))
#define REGEX_SET(t, b) ((t)[(uint8_t)(b)>>4]|=(1<<((uint8_t)(b)&0xf)))
#define REGEX_TOGGLE(t, b) ((t)[(uint8_t)(b)>>4]^=(1<<((uint8_t)(b)&0xf)))

static regex_nodeid_t
regex_newnode(struct regex_node *node)
{
	struct regex_node *newNodes;

	if(!n_nodes)
		n_nodes++;
	newNodes = realloc(all_nodes, sizeof(*all_nodes) * (n_nodes + 1));
	if(!newNodes)
		return -1;
	all_nodes = newNodes;
	all_nodes[n_nodes] = *node;
	return n_nodes++;
}

static int
regex_addnode(regex_nodeid_t parent, regex_nodeid_t child)
{
	struct regex_node *p;
	regex_nodeid_t *newNodes;

	p = all_nodes + parent;
	newNodes = realloc(p->nodes, sizeof(*p->nodes) * (p->nNodes + 1));
	if(!newNodes)
		return -1;
	p->nodes = newNodes;
	p->nodes[p->nNodes++] = child;
	return 0;
}

static int
regex_getchar(const char **pStr)
{
	const char *str;
	int c;

	str = *pStr;
	if(!*str)
		return -1;
	if(*str == '\\') {
		str++;
		switch(*str) {
		case 0: return -1;
		case 'a': c = '\a'; break;
		case 'b': c = '\b'; break;
		case 'e': c = 0x1b; break;
		case 'f': c = '\f'; break;
		case 'n': c = '\n'; break;
		case 'r': c = '\r'; break;
		case 't': c = '\t'; break;
		case 'v': c = '\v'; break;
		case 'x':
			if(!isxdigit(*(str + 1)) || !isxdigit(*(str + 2)))
				return -1;
			str++;
			c = *str >= 'a' ? *str - 'a' + 10 :
					*str >= 'A' ? *str - 'A' + 10 :
					*str - '0';
			str++;
			c <<= 4;
			c |= *str >= 'a' ? *str - 'a' + 10 :
					*str >= 'A' ? *str - 'A' + 10 :
					*str - '0';
			break;
		default: c = *str; break;
		}
	} else {
		c = *str;
	}
	*pStr = str;
	return c;
}

static int
regex_gettests(const char **pStr, regex_tests_t tests)
{
	const char *str;
	bool invert = false;
	int c;

	str = *pStr;
	memset(tests, 0, 16 * sizeof(*tests));
	str++; // skip over '['
	if(*str == '^') {
		str++;
		if(*str == ']') {
			REGEX_SET(tests, '^');
			return 0;
		}
		if(*str == '-' && *(str + 1) != ']') {
			REGEX_SET(tests, '^');
			c = '^';
		} else {
			invert = true;
			if((c = regex_getchar(&str)) == -1)
				return -1;
			str++;
		}
	} else {
		if(*str == ']')
			return 0;
		if((c = regex_getchar(&str)) == -1)
			return -2;
		str++;
	}
	while(REGEX_SET(tests, c), *str != ']') {
		if(*str == '-') {
			int other_c;

			if(*(str + 1) == ']') {
				c = '-';
			} else {
				str++;
				if((other_c = regex_getchar(&str)) == -1)
					return -3;
				for(char from = c, to = other_c; from != to; from++)
					REGEX_SET(tests, from);
				c = other_c;
			}
		} else {
			if((c = regex_getchar(&str)) == -1)
				return -4;
		}
		str++;
	}
	if(invert)
		for(unsigned i = 0; i < 16; i++)
			tests[i] = ~tests[i];
	*pStr = str;
	return 0;
}

static int
regex_getnode(const char **pStr, struct regex_node *node)
{
	const char *str;
	int c;

	memset(node, 0, sizeof(*node));
	str = *pStr;
	switch(*str) {
	case 0:
		return 1;
	case '\\': {
		bool invert = false;

		switch(*(str + 1)) {
		case 0: return -5;
		case 'S':
			invert = true;
		/* fall through */
		case 's':
			REGEX_TOGGLE(node->tests, ' ');
			REGEX_TOGGLE(node->tests, '\t');
			REGEX_TOGGLE(node->tests, '\f');
			REGEX_TOGGLE(node->tests, '\v');
			REGEX_TOGGLE(node->tests, '\r');
			REGEX_TOGGLE(node->tests, '\n');
			str++;
			break;
		case 'W':
			invert = true;
		/* fall through */
		case 'w':
			for(int i = 'a'; i <= 'z'; i++)
				REGEX_TOGGLE(node->tests, i);
			for(int i = 'A'; i <= 'Z'; i++)
				REGEX_TOGGLE(node->tests, i);
			REGEX_TOGGLE(node->tests, '_');
			str++;
			break;
		case 'D':
			invert = true;
		/* fall through */
		case 'd':
			for(int i = '0'; i <= '9'; i++)
				REGEX_TOGGLE(node->tests, i);
			str++;
			break;
		default:
			if((c = regex_getchar(&str)) == -1)
				return -6;
			REGEX_SET(node->tests, c);
			break;
		}
		if(invert)
			for(unsigned i = 0; i < 16; i++)
				node->tests[i] = ~node->tests[i];
		str++;
		break;
	}
	case '.':
		memset(node->tests, 0xff, 16 * sizeof(*node->tests));
		REGEX_TOGGLE(node->tests, '\n');
		str++;
		break;
	case '[':
		if(regex_gettests(&str, node->tests) < 0)
			return -7;
		str++;
		break;
	default:
		if((c = regex_getchar(&str)) == -1)
			return -8;
		REGEX_SET(node->tests, c);
		str++;
		break;
	}
	node->flags = 0;
	switch(*str) {
	case '+': node->flags |= FREGEX_NODE_REPEAT; str++; break;
	case '*': node->flags |= FREGEX_NODE_REPEAT | FREGEX_NODE_OPTIONAL; str++; break;
	case '?': node->flags |= FREGEX_NODE_OPTIONAL; str++; break;
	}
	printf("\tflags=%x, tests=", node->flags);
	for(unsigned i = 0; i < 256; i++)
		if(REGEX_CHECK(node->tests, i) && isprint(i))
			printf("%c", i);
	printf("\n");
	*pStr = str;
	return 0;
}

static int
regex_getgroup(struct regex_matcher *matcher, const char **pStr, struct regex_group *group)
{
	const char *str;
	struct regex_node node;
	struct regex_group subGroup;

	str = *pStr;
	group->head = 0;
	group->tail = 0;
	while(1) {
		switch(*str) {
		case 0:
		case '(':
		case ')':
		case '|':
			*pStr = str;
			return 0;
		case '\\':
			// unnamed capture group (by index)
			if(*(str + 1) >= '1' && *(str + 1) <= '9') {
				int index;

				str++;
				index = *str - '1';
				subGroup = matcher->unnGroups[index];
				if(!subGroup.head)
					return -12;
				break;
			}
			// named capture group
			if(*(str + 1) == '<') {
				unsigned i;
				const char *name;
				unsigned nName;

				str++;
				name = str;
				while(isalpha(*str))
					str++;
				if(*str != '>')
					return -13;
				nName = str - name;
				for(i = 0; i < matcher->nGroups; i++)
					if(!strncmp(matcher->groups[i].name, name, nName) && matcher->groups[i].name[nName]) {
						subGroup = matcher->groups[i].group;
						break;
					}
				if(i == matcher->nGroups)
					return -14;
				str++;
				break;
			}
		/* fall through */
		default: {
			regex_nodeid_t id;

			if(regex_getnode(&str, &node) < 0)
				return -15;
			id = regex_newnode(&node);
			subGroup.head = id;
			subGroup.tail = id;
			break;
		}
		}
		if(!group->head) {
			group->head = subGroup.head;
		} else {
			if(regex_addnode(group->tail, subGroup.head) < 0)
				return -16;
		}
		group->tail = subGroup.tail;
	}
	// can't return here (while loop before)
	// return 0;
}

struct regex_layer {
	regex_nodeid_t *nodes;
	unsigned nNodes;
};

static int
regex_layer_addnode(struct regex_layer *layer, regex_nodeid_t node)
{
	regex_nodeid_t *newNodes;

	newNodes = realloc(layer->nodes, sizeof(*layer->nodes) * (layer->nNodes + 1));
	if(!newNodes)
		return -1;
	layer->nodes = newNodes;
	layer->nodes[layer->nNodes++] = node;
	return 0;
}

static int
regex_layer_connect(struct regex_layer *layer, regex_nodeid_t node)
{
	for(unsigned i = 0; i < layer->nNodes; i++)
		if(regex_addnode(layer->nodes[i], node) < 0)
			return -1;
	return 0;
}

static void
regex_layer_replace(struct regex_layer *dest, struct regex_layer *src)
{
	free(dest->nodes);
	*dest = *src;
	memset(src, 0, sizeof(*src));
}

static void
regex_layer_markexit(struct regex_layer *layer)
{
	for(unsigned i = 0; i < layer->nNodes; i++)
		all_nodes[layer->nodes[i]].flags |= FREGEX_NODE_EXIT;
}

static int
regex_parse(struct regex_matcher *matcher, regex_nodeid_t root, const char *pattern)
{
	int code = 0;
	struct regex_group group;
	regex_nodeid_t node;
	struct regex_layer topLayer;
	struct regex_layer bottomLayer;

	memset(&topLayer, 0, sizeof(topLayer));
	memset(&bottomLayer, 0, sizeof(bottomLayer));
	if(regex_layer_addnode(&topLayer, root) < 0)
		return -1;
	while(*pattern) {
		printf("%s\n", pattern);
		sleep(1);
		if((code = regex_getgroup(matcher, &pattern, &group)) < 0)
			goto ret;
		if(regex_layer_connect(&topLayer, group.head) < 0) {
			code = -1;
			goto ret;
		}
		if(regex_layer_addnode(&bottomLayer, group.tail) < 0) {
			code = -1;
			goto ret;
		}
		if(*pattern != '|')
			regex_layer_replace(&topLayer, &bottomLayer);
		else
			pattern++;
	}
	regex_layer_markexit(&bottomLayer);
ret:
	free(topLayer.nodes);
	free(bottomLayer.nodes);
	return code;
}

int
regex_addpattern(struct regex_matcher *matcher, const char *pattern)
{
	struct regex_node node;
	regex_nodeid_t root;
	int code;

	/**
	 * If the parser fails, then we want to clean all memory.
	 * Good thing is that we store all memory within all_nodes and
	 * the only dynamically allocated memory within a node struct is the nodes array
	 * which we free on fail.
	 * If we fail, we can set the n_nodes back to the start as if nothing ever happened.
	 */
	const regex_nodeid_t start = n_nodes;

	printf("pattern=%s\n", pattern);
	memset(&node, 0, sizeof(node));
	root = regex_newnode(&node);
	if((code = regex_parse(matcher, root, pattern)) < 0) {
		for(regex_nodeid_t n = start; n < n_nodes; n++)
			free(all_nodes[n].nodes);
		n_nodes = start; // set it back (rewind time :O)
	}
	return code;
}

