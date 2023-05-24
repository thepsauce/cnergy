#include "cnergy.h"

// TODO: Make functions

int
cnergy_state_default(struct state *s)
{

	return 0;
}

int
cnergy_state_keylist(struct state *s)
{

	return 0;
}

int
cnergy_state_commandlist(struct state *s)
{

	return 0;
}

int (*cnergy_states[])(struct state *s) = {
	[CNERGY_STATE_DEFAULT] = cnergy_state_default,
	[CNERGY_STATE_KEYLIST] = cnergy_state_keylist,
	[CNERGY_STATE_COMMANDLIST] = cnergy_state_commandlist,
};
