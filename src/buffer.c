#include "editor.h"

#define cwidth(x, c) ({ \
	__auto_type _c = (c); \
	__auto_type _x = (x); \
	_c == '\t' ? TABSIZE - _x % TABSIZE : _c < 32 ? 2 : 1; \
})
		
// Returns the width this would visually take up
// Note: Newlines are not handled
static inline U32
widthnstr(const char *str, U32 nStr)
{
	U32 width = 0;

   for(U32 i = 0; i < nStr; i++)
		width += cwidth(width, str[i]);
	return width;
}

static inline U32
widthstr(const char *str)
{
	return widthnstr(str, strlen(str));
}

I32 
unsafe_buffer_movecursor(struct buffer *buf, I32 distance)
{
	/*This method retained the gap content
	I32 d;
	char *s;
	U32 n;

	if(distance < 0) {
		d = -distance;
		s = buf->data + buf->iGap - d;
	} else {
		d = distance;
		s = buf->data + buf->iGap;
	}
	n = buf->nGap + d;
	while(1) {
		d %= n;
		if(!d)
			break;
		n -= d;
		if(distance < 0) {
			for(U32 i = d; i; ) {
				i--;
				const char tmp = s[n + i];
				s[n + i] = s[i];
				s[i] = tmp;
			}
		} else {
			for(U32 i = 0; i < d; i++) {
				const char tmp = *s;
				*s = s[n];
				s[n] = tmp;
				s++;
			}
		}
	}*/
	// TODO: Maybe delay moving of the gap to the moment content is inserted or deleted
	if(distance < 0)
		memmove(buf->data + buf->iGap + buf->nGap + distance,
				buf->data + buf->iGap + distance, -distance);
	else
		memmove(buf->data + buf->iGap,
				buf->data + buf->iGap + buf->nGap, distance);
	buf->iGap += distance;
	return distance;
}

static inline I32
clamp(const struct buffer *buf, I32 amount)
{
	if(amount > (I32) (buf->nData - buf->iGap))
		return buf->nData - buf->iGap;
	// TODO: quickfix in place because -INT32_MIN doesn't exist
	if(amount == INT32_MIN || -amount > (I32) buf->iGap)
		return -buf->iGap;
	return amount;
}

// Returns the moved amount (safe version of unsafe_buffer_movecursor that also updates vct)
I32
buffer_movecursor(struct buffer *buf, I32 distance)
{
	distance = clamp(buf, distance);
	unsafe_buffer_movecursor(buf, distance);
	const U32 col = buffer_col(buf);
	buf->vct = widthnstr(buf->data + buf->iGap - col, col);
	return distance;
}

// Move the cursor horizontally by a given distance
// Returns the moved amount
I32
buffer_movehorz(struct buffer *buf, I32 distance)
{
	U32 i, iFirst, left, right;

	i = buf->iGap;
	if(distance == INT32_MAX) {
		i += buf->nGap;
		iFirst = i;
		for(; i < buf->nData + buf->nGap; i++)
			if(buf->data[i] == '\n')
				break;
		buf->vct = UINT32_MAX;
		return unsafe_buffer_movecursor(buf, i - iFirst);
	}
	if(distance > 0) {
		i += buf->nGap;
		iFirst = i;
		for(; i < buf->nData + buf->nGap && distance; i++, distance--)
			if(buf->data[i] == '\n')
				break;
		left = right = i;
		while(left != buf->iGap + buf->nGap) {
			if(buf->data[left - 1] == '\n')
				break;
			left--;
		}
		buf->vct = widthnstr(buf->data + left, right - left);
		left -= buf->nGap;
	} else {
		iFirst = i;
		for(; i > 0 && distance; i--, distance++)
			if(buf->data[i - 1] == '\n')
				break;
		left = i;
		buf->vct = 0;
	}
	right = left;
	while(left > 0) {
		if(buf->data[left - 1] == '\n')
			break;
		left--;
	}
	buf->vct += widthnstr(buf->data + left, right - left);
	return unsafe_buffer_movecursor(buf, i - iFirst);
}

// Move the cursor up or down by the specified number of lines
// Returns the moved amount
I32
buffer_movevert(struct buffer *buf, I32 distance)
{
	U32 i, maxTravel;
	const I32 cDistance = distance;
	I32 moved;

	i = buf->iGap;
	if(distance < 0) {
		while(distance) {
			while(i > 0 && buf->data[i - 1] != '\n')
				i--;
			if(!i)
				break;
			distance++;
			i--;
		}
		if(distance == cDistance)
			return 0;
		while(i > 0 && buf->data[i - 1] != '\n')
			i--;
		for(U32 travelled = 0; travelled < buf->vct; i++) {
			if(buf->data[i] == '\n')
				break;
			travelled += cwidth(travelled, buf->data[i]);
		}
		moved = i - buf->iGap;
	} else {
		i += buf->nGap;
		while(distance) {
			while(i < buf->nData + buf->nGap && buf->data[i] != '\n')
				i++;
			if(i == buf->nData + buf->nGap)
				break;
			distance--;
			i++;
		}
		if(distance == cDistance)
			return 0;
		if(i == buf->nData + buf->nGap)
			// i > 0 not needed since we know that we crossed over a \n already
			while(buf->data[i - 1] != '\n')
				i--;
		for(U32 travelled = 0; i < buf->nData + buf->nGap && travelled < buf->vct; i++) {
			if(buf->data[i] == '\n')
				break;
			travelled += cwidth(travelled, buf->data[i]);
		}
		moved = i - buf->nGap - buf->iGap;
	}
	unsafe_buffer_movecursor(buf, moved);
	return cDistance - distance;
}

static struct event *
buffer_addevent(struct buffer *buf)
{
	struct event *ev;

	if(buf->nEvents > buf->iEvent) {
		for(U32 i = buf->iEvent; i < buf->nEvents; i++) {
			switch(buf->events[i].type) {
			case EVENT_INSERT:
				free(buf->events[i].ins);
				break;
			case EVENT_DELETE:
				free(buf->events[i].del);
				break;
			case EVENT_REPLACE:
				free(buf->events[i].ins);
				free(buf->events[i].del);
				break;
			}
		}
	}
	buf->events = realloc(buf->events, sizeof(*buf->events) * (buf->iEvent + 1));
	if(!buf->events)
		return NULL;
	ev = buf->events + buf->iEvent;
	buf->nEvents = ++buf->iEvent;
	return ev;
}

// Returns the number of characters inserted
U32
buffer_insert(struct buffer *buf, const char *str)
{
	U32 i;
	struct event *ev;
	const U32 n = strlen(str);
	if(!n)
		return 0;
	// if gap is too small to insert n characters, increase gap size
	if(n > buf->nGap) {
		char *const newData = realloc(buf->data, buf->nData + n + BUFFER_GAP_SIZE);
		if(!newData)
			return 0;
		memmove(newData + buf->iGap + n + BUFFER_GAP_SIZE,
				newData + buf->iGap + buf->nGap,
				buf->nData - buf->iGap);
		buf->nGap = n + BUFFER_GAP_SIZE;
		buf->data = newData;
	}
	// insert string into gap
	memcpy(buf->data + buf->iGap, str, n);
	buf->iGap += n;
	buf->nGap -= n;
	buf->nData += n;
	// update vct
	const U32 prevVct = buf->vct;
	for(i = n; i > 0; i--)
		if(str[i - 1] == '\n') {
			buf->vct = 0;
			break;
		}
	buf->vct += widthnstr(str + i, n - i);
	// try to join events
	if(buf->iEvent) {
		ev = buf->events + buf->iEvent - 1;
		if(ev->type == EVENT_INSERT && ev->iGap + ev->nIns == buf->iGap - n) {
			ev->ins = realloc(ev->ins, ev->nIns + n);
			memcpy(ev->ins + ev->nIns, str, n);
			ev->vct = buf->vct;
			ev->nIns += n;
			return n;
		}
	}
	// add event
	if(!(ev = buffer_addevent(buf)))
		return n;
	*ev = (struct event) {
		.flags = 0,
		.type = EVENT_INSERT,
		.iGap = buf->iGap - n,
		.vct = buf->vct,
		.prevVct = prevVct,
		.ins = malloc(n),
		.nIns = n,
	};
	if(!ev->ins)
		return n;
	memcpy(ev->ins, str, n);
	return n;
}

// Returns the number of characters inserted
U32
buffer_insert_file(struct buffer *buf, FILE *fp)
{
	char *s;

	fseek(fp, 0, SEEK_END);
	const long n = ftell(fp);
	if(n <= 0 || !(s = malloc(n + 1)))
		return 0;
	fseek(fp, 0, SEEK_SET);
	fread(s, 1, n, fp);
	s[n] = 0;
	const U32 nins = buffer_insert(buf, s);
	free(s);
	return nins;
}

// Returns the amount that was deleted
I32
buffer_delete(struct buffer *buf, I32 amount)
{
	struct event *ev;

	amount = clamp(buf, amount);
	if(!amount)
		return 0;
	// add event
	// TODO: CODE REPETITION
	if(!(ev = buffer_addevent(buf)))
		return amount;
	*ev = (struct event) {
		.flags = 0,
		.type = EVENT_DELETE,
		.prevVct = buf->vct,
		.del = malloc(ABS(amount)),
		.nDel = ABS(amount),
	};
	if(!ev->del)
		return 0;
	if(amount > 0) {
		memcpy(ev->del, buf->data + buf->iGap + buf->nGap, amount);
		ev->iGap = buf->iGap;
		ev->vct = buf->vct;
		buf->nGap += amount;
		buf->nData -= amount;
	} else {
		buf->iGap += amount;
		memcpy(ev->del, buf->data + buf->iGap, -amount);
		ev->iGap = buf->iGap;
		ev->vct = buf->vct;
		buf->nGap -= amount;
		buf->nData += amount;
		// update vct
		for(U32 i = buf->iGap; i < buf->iGap - amount; i++)
			if(buf->data[i] == '\n') {
				const U32 end = i;
				for(; i > 0; i--)
					if(buf->data[i - 1] == '\n')
						break;
				buf->vct = widthnstr(buf->data + i, end - i);
				return amount;
			}
		buf->vct -= widthnstr(buf->data + buf->iGap, -amount);
	}
	return amount;
}

// Returns the amount of lines that was deleted
I32
buffer_deleteline(struct buffer *buf, I32 amount)
{
	struct event *ev;
	U32 l, r;
	U32 nLinesDeleted = 0;

	if(!amount)
		return 0;
	l = buf->iGap;
	r = buf->iGap + buf->nGap;
	for(I32 a = ABS(amount);; ) {
		for(; l > 0; l--)
			if(buf->data[l - 1] == '\n')
				break;
		for(; r < buf->nData + buf->nGap; r++)
			if(buf->data[r] == '\n')
				break;
		a--;
		if(!a)
			break;
		if(amount < 0) {
			if(!l)
				break;
			l--;
		}
		if(amount > 0) {
			if(r == buf->nData + buf->nGap)
				break;
			r++;
		}
		nLinesDeleted++;
	}
	if(r != buf->nData + buf->nGap)
		r++;
	else if(l)
		l--;
	amount = r - l - buf->nGap;
	buf->iGap = l;
	buf->nGap = r - l;
	buf->nData -= amount;
	// add event
	// TODO: CODE REPETITION
	if(!(ev = buffer_addevent(buf)))
		return nLinesDeleted;
	*ev = (struct event) {
		.flags = 0,
		.type = EVENT_DELETE,
		.iGap = buf->iGap,
		.vct = buf->vct,
		.prevVct = buf->vct,
		.del = malloc(amount),
		.nDel = amount,
	};
	if(!ev->del)
		return nLinesDeleted;
	memcpy(ev->del, buf->data + l, amount);
	return nLinesDeleted;
}

int
buffer_undo(struct buffer *buf)
{
	if(!buf->iEvent)
		return 0;
	const struct event ev = buf->events[--buf->iEvent];
	switch(ev.type) {
	case EVENT_INSERT:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		buf->nGap += ev.nIns;
		buf->nData -= ev.nIns;
		break;
	case EVENT_DELETE:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		memmove(buf->data + ev.iGap, ev.del, ev.nDel);
		buf->iGap += ev.nDel;
		buf->nGap -= ev.nDel;
		buf->nData += ev.nDel;
		break;
	case EVENT_REPLACE:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		memmove(buf->data + ev.iGap, ev.del, ev.nDel);
		buf->iGap -= ev.nIns;
		buf->iGap += ev.nDel;
		buf->nGap += ev.nIns;
		buf->nGap -= ev.nDel;
		buf->nData -= ev.nIns;
		buf->nData += ev.nDel;
		break;
	}
	buf->vct = ev.prevVct;
	return 1;
}

int
buffer_redo(struct buffer *buf)
{
	if(buf->iEvent == buf->nEvents)
		return 0;
	const struct event ev = buf->events[buf->iEvent++];
	switch(ev.type) {
	case EVENT_INSERT:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		memmove(buf->data + buf->iGap, ev.ins, ev.nIns);
		buf->iGap += ev.nIns;
		buf->nGap -= ev.nIns;
		buf->nData += ev.nIns;
		break;
	case EVENT_DELETE:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		buf->nGap += ev.nDel;
		buf->nData -= ev.nDel;
		break;
	case EVENT_REPLACE:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		memmove(buf->data + ev.iGap, ev.ins, ev.nIns);
		buf->iGap += ev.nIns;
		buf->iGap -= ev.nDel;
		buf->nGap -= ev.nIns;
		buf->nGap += ev.nDel;
		buf->nData += ev.nIns;
		buf->nData -= ev.nDel;
		break;
	}
	buf->vct = ev.vct;
	return 1;
}

// Returns the index of the line containing the cursor
U32
buffer_line(const struct buffer *buf)
{
	U32 line = 0;

	for(U32 i = buf->iGap; i > 0; i--)
		if(buf->data[i - 1] == '\n')
			line++;
	return line;
}

// Returns the index of the column containing the cursor
U32
buffer_col(const struct buffer *buf)
{
	U32 i;

	for(i = buf->iGap; i > 0; i--)
		if(buf->data[i - 1] == '\n')
			break;
	return buf->iGap - i;
}

// Returns the length of the line
U32
buffer_getline(const struct buffer *buf, U32 line, char *dest, U32 maxDest)
{
	U32 i, j, k;
	const U32 n = buf->nData + buf->nGap;
	// find the start and end positions of the line
	i = j = k = 0;
	while(k < line) {
		if(i == buf->iGap)
			i += buf->nGap;
		if(i >= n)
			break;
		if(buf->data[i] == '\n')
			k++;
		i++;
	}
	j = i;
	while(1) {
		if(j == buf->iGap)
			j += buf->nGap;
		if(j >= n)
			break;
		if(buf->data[j] == '\n')
			break;
		j++;
	}
	// copy the line to the destination buffer
	const U32 len = MIN(j - i, maxDest - 1);
	memcpy(dest, buf->data + i, len);
	dest[len] = '\0';
	return len;
}
