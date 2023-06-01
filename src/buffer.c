#include "cnergy.h"

struct buffer **all_buffers;
unsigned n_buffers;

struct buffer *
buffer_new(const char *file)
{
	struct buffer **newBuffers;
	struct buffer *buf;

	// find buffer with the same file
	if(file) {
		for(unsigned i = 0; i < n_buffers; i++) {
			buf = all_buffers[i];
			if(buf->file && !strcmp(buf->file, file))
				return buf;
		}
	}

	newBuffers = dialog_realloc(all_buffers, sizeof(*all_buffers) * (n_buffers + 1));
	if(!newBuffers)
		return NULL;
	buf = dialog_alloc(sizeof(*buf));
	if(!buf)
		return NULL;
	memset(buf, 0, sizeof(*buf));
	if(file) {
		FILE *fp;

		fp = fopen(file, "r");
		if(fp && (buf->file = const_alloc(file, strlen(file) + 1))) {
			struct file_info fi;

			if(getfileinfo(&fi, file) < 0)
				buf->fileTime = 0;
			else
				buf->fileTime = fi.modTime;
			fseek(fp, 0, SEEK_END);
			const long n = ftell(fp);
			buf->data = dialog_alloc(BUFFER_GAP_SIZE + n);
			if(!buf->data) {
				free(buf);
				return NULL;
			}
			buf->nData = n;
			fseek(fp, 0, SEEK_SET);
			fread(buf->data + BUFFER_GAP_SIZE, 1, n, fp);
			fclose(fp);
		}
	} else {
		buf->data = dialog_alloc(BUFFER_GAP_SIZE);
		if(!buf->data) {
			free(buf);
			return NULL;
		}
	}
	buf->nGap = BUFFER_GAP_SIZE;
	all_buffers = newBuffers;
	all_buffers[n_buffers++] = buf;
	return buf;
}

void
buffer_free(struct buffer *buf)
{
	free(buf->data);
	for(unsigned i = 0; i < buf->nEvents; i++) {
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
	free(buf->events);
	free(buf);
}

int
buffer_save(struct buffer *buf)
{
	FILE *fp;
	struct file_info fi;

	if(!buf->file) {
		const char *file = NULL; // TODO: add choosefile call again
		if(!file)
			return 1;
		buf->file = const_alloc(file, strlen(file) + 1);
		if(!buf->file)
			return -1;
	}
	if(getfileinfo(&fi, buf->file) < 0)
		return -1;
	if(fi.modTime != buf->fileTime) {
		const int m = messagebox("File not in line", "The file you're trying to write to was modified outside the editor, do you still want to write to it?", "[Y]es", "[N]o", NULL);
		if(m != 0)
			return 1;
	}
	fp = fopen(buf->file, "w");
	if(!fp)
		return -1;
	fwrite(buf->data, 1, buf->iGap, fp);
	fwrite(buf->data + buf->iGap + buf->nGap, 1, buf->nData - buf->iGap, fp);
	fclose(fp);
	buf->saveEvent = buf->iEvent;
	getfileinfo(&fi, buf->file);
	buf->fileTime = fi.modTime;
	return 0;
}

ssize_t
unsafe_buffer_movecursor(struct buffer *buf, ssize_t distance)
{
	/*This method retained the gap content
	ssize_t d;
	char *s;
	size_t n;

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
			for(size_t i = d; i; ) {
				i--;
				const char tmp = s[n + i];
				s[n + i] = s[i];
				s[i] = tmp;
			}
		} else {
			for(size_t i = 0; i < d; i++) {
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

static inline ssize_t
buffer_cnvdist(const struct buffer *buf, ssize_t distance)
{
	return utf8_cnvdist(buf->data, buf->nData + buf->nGap, buf->iGap + (distance > 0 ? buf->nGap : 0), distance);
}

// Returns the moved amount (safe version of unsafe_buffer_movecursor that also updates vct)
ssize_t
buffer_movecursor(struct buffer *buf, ssize_t distance)
{
	distance = buffer_cnvdist(buf, distance);
	unsafe_buffer_movecursor(buf, distance);
	buf->vct = buffer_col(buf);
	return distance;
}

// Move the cursor horizontally by a given distance
// Returns the moved amount
ssize_t
buffer_movehorz(struct buffer *buf, ssize_t distance)
{
	size_t i, iFirst, left, right;

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
		for(; i < buf->nData + buf->nGap && distance; distance--) {
			if(buf->data[i] == '\n')
				break;
			i += utf8_len(buf->data + i, buf->nData + buf->nGap - i);
		}
		left = right = i;
		while(left != buf->iGap + buf->nGap) {
			if(buf->data[left - 1] == '\n')
				break;
			left--;
		}
		buf->vct = utf8_widthnstr(buf->data + left, right - left);
		left -= buf->nGap;
	} else {
		iFirst = i;
		for(; i > 0 && distance; distance++) {
			if(buf->data[i - 1] == '\n')
				break;
			while((buf->data[--i] & 0xC0) == 0x80)
				if(i > 0 && !(buf->data[i - 1] & 0x80))
					break;
		}
		left = i;
		buf->vct = 0;
	}
	right = left;
	while(left > 0) {
		if(buf->data[left - 1] == '\n')
			break;
		left--;
	}
	buf->vct += utf8_widthnstr(buf->data + left, right - left);
	return unsafe_buffer_movecursor(buf, i - iFirst);
}

// Move the cursor up or down by the specified number of lines
// Returns the moved amount
ssize_t
buffer_movevert(struct buffer *buf, ssize_t distance)
{
	size_t i;
	const ssize_t cDistance = distance;
	ssize_t moved;

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
		for(unsigned travelled = 0; travelled < buf->vct;) {
			if(buf->data[i] == '\n')
				break;
			travelled += utf8_width(buf->data + i, buf->nData + buf->nGap - i, travelled);
			i += utf8_len(buf->data + i, buf->nData + buf->nGap - i);
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
		for(unsigned travelled = 0; i < buf->nData + buf->nGap && travelled < buf->vct;) {
			if(buf->data[i] == '\n')
				break;
			travelled += utf8_width(buf->data + i, buf->nData + buf->nGap - i, travelled);
			i += utf8_len(buf->data + i, buf->nData + buf->nGap - i);
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
		for(unsigned i = buf->iEvent; i < buf->nEvents; i++) {
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
	buf->events = dialog_realloc(buf->events, sizeof(*buf->events) * (buf->iEvent + 1));
	if(!buf->events)
		return NULL;
	ev = buf->events + buf->iEvent;
	buf->nEvents = ++buf->iEvent;
	return ev;
}

// Returns the number of characters inserted
size_t
buffer_insert(struct buffer *buf, const char *str, size_t nStr)
{
	struct event *ev;

	// if gap is too small to insert n characters, increase gap size
	if(nStr > buf->nGap) {
		char *const newData = dialog_realloc(buf->data, buf->nData + nStr + BUFFER_GAP_SIZE);
		if(!newData)
			return 0;
		memmove(newData + buf->iGap + nStr + BUFFER_GAP_SIZE,
				newData + buf->iGap + buf->nGap,
				buf->nData - buf->iGap);
		buf->nGap = nStr + BUFFER_GAP_SIZE;
		buf->data = newData;
	}
	// insert string into gap
	memcpy(buf->data + buf->iGap, str, nStr);
	buf->iGap += nStr;
	buf->nGap -= nStr;
	buf->nData += nStr;
	// update vct
	const unsigned prevVct = buf->vct;
	buf->vct = buffer_col(buf);
	// try to join events
	if(buf->iEvent) {
		ev = buf->events + buf->iEvent - 1;
		if(ev->type == EVENT_INSERT && ev->iGap + ev->nIns == buf->iGap - nStr) {
			ev->ins = dialog_realloc(ev->ins, ev->nIns + nStr);
			memcpy(ev->ins + ev->nIns, str, nStr);
			ev->vct = buf->vct;
			ev->nIns += nStr;
			return nStr;
		}
	}
	// add event
	if(!(ev = buffer_addevent(buf)))
		return nStr;
	*ev = (struct event) {
		.flags = 0,
		.type = EVENT_INSERT,
		.iGap = buf->iGap - nStr,
		.vct = buf->vct,
		.prevVct = prevVct,
		.ins = dialog_alloc(nStr),
		.nIns = nStr,
	};
	if(!ev->ins) {
		buf->nEvents--;
		return nStr;
	}
	memcpy(ev->ins, str, nStr);
	return nStr;
}

// Returns the number of characters inserted
size_t
buffer_insert_file(struct buffer *buf, FILE *fp)
{
	char *s;

	fseek(fp, 0, SEEK_END);
	const long n = ftell(fp);
	if(n <= 0 || !(s = dialog_alloc(n)))
		return 0;
	fseek(fp, 0, SEEK_SET);
	fread(s, 1, n, fp);
	const size_t nins = buffer_insert(buf, s, n);
	free(s);
	return nins;
}

// Returns the amount that was deleted
ssize_t
buffer_delete(struct buffer *buf, ssize_t amount)
{
	struct event *ev;

	amount = buffer_cnvdist(buf, amount);
	if(!amount)
		return 0;
	// add event
	if(!(ev = buffer_addevent(buf)))
		return amount;
	*ev = (struct event) {
		.flags = 0,
		.type = EVENT_DELETE,
		.prevVct = buf->vct,
		.del = dialog_alloc(ABS(amount)),
		.nDel = ABS(amount),
	};
	if(!ev->del) {
		buf->nEvents--;
		return 0;
	}
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
	}
	buf->vct = buffer_col(buf);
	return amount;
}

// Returns the amount of lines that was deleted
ssize_t
buffer_deleteline(struct buffer *buf, ssize_t amount)
{
	struct event *ev;
	size_t l, r;
	size_t nLinesDeleted = 0;

	if(!amount)
		return 0;
	l = buf->iGap;
	r = buf->iGap + buf->nGap;
	for(ssize_t a = ABS(amount);; ) {
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
	if(!(ev = buffer_addevent(buf)))
		return nLinesDeleted;
	*ev = (struct event) {
		.flags = 0,
		.type = EVENT_DELETE,
		.iGap = buf->iGap,
		.vct = buf->vct,
		.prevVct = buf->vct,
		.del = dialog_alloc(amount),
		.nDel = amount,
	};
	if(!ev->del) {
		buf->nEvents--;
		return nLinesDeleted;
	}
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
int
buffer_line(const struct buffer *buf)
{
	int line = 0;

	for(size_t i = buf->iGap; i > 0; i--)
		if(buf->data[i - 1] == '\n')
			line++;
	return line;
}

// Returns the terminal index of the column containing the cursor
int
buffer_col(const struct buffer *buf)
{
	size_t i;

	for(i = buf->iGap; i > 0; i--)
		if(buf->data[i - 1] == '\n')
			break;
	return utf8_widthnstr(buf->data + i, buf->iGap - i);
}

// Returns the length of the line
size_t
buffer_getline(const struct buffer *buf, int line, char *dest, size_t maxDest)
{
	size_t i, j;
	int k;
	const size_t n = buf->nData + buf->nGap;
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
	const size_t len = MIN(j - i, maxDest - 1);
	memcpy(dest, buf->data + i, len);
	dest[len] = '\0';
	return len;
}
