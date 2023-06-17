#include "cnergy.h"

struct buffer *all_buffers;
bufferid_t n_buffers;

bufferid_t
buffer_new(fileid_t file)
{
	struct buffer *newBuffers;
	bufferid_t bufid;
	struct buffer *buf;

	// find buffer with the same file
	if(file != ID_NULL)
		for(bufid = 0; bufid < n_buffers; bufid++)
			if(all_buffers[bufid].file == file)
				return bufid;
	newBuffers = dialog_realloc(all_buffers, sizeof(*all_buffers) * (n_buffers + 1));
	if(!newBuffers)
		return ID_NULL;
	all_buffers = newBuffers;
	bufid = n_buffers++;
	buf = all_buffers + bufid;
	memset(buf, 0, sizeof(*buf));
	if(file != ID_NULL) {
		FILE *const fp = fc_open(file, "r");
		if(fp != NULL) {
			fseek(fp, 0, SEEK_END);
			const long n = ftell(fp);
			buf->data = dialog_alloc(BUFFER_GAP_SIZE + n);
			if(buf->data == NULL)
				return ID_NULL;
			buf->nData = n;
			fseek(fp, 0, SEEK_SET);
			fread(buf->data + BUFFER_GAP_SIZE, 1, n, fp);
			fclose(fp);
		} else {
			buf->data = dialog_alloc(BUFFER_GAP_SIZE);
			if(buf->data == NULL)
				return ID_NULL;
		}
		buf->file = file;
	} else {
		buf->data = dialog_alloc(BUFFER_GAP_SIZE);
		if(buf->data == NULL)
			return ID_NULL;
	}
	buf->nGap = BUFFER_GAP_SIZE;
	return bufid;
}

void
buffer_free(bufferid_t bufid)
{
	struct buffer *const buf = all_buffers + bufid;
	free(buf->data);
	for(unsigned i = 0; i < buf->nEvents; i++) {
		switch(buf->events[i].type) {
		case BUFFER_EVENT_INSERT:
			free(buf->events[i].ins);
			break;
		case BUFFER_EVENT_DELETE:
			free(buf->events[i].del);
			break;
		case BUFFER_EVENT_REPLACE:
			free(buf->events[i].ins);
			free(buf->events[i].del);
			break;
		}
	}
	free(buf->events);
}

int
buffer_save(bufferid_t bufid)
{
	struct buffer *const buf = all_buffers + bufid;
	if(buf->file == ID_NULL) {
		fileid_t file = ID_NULL; // TODO: add choosefile call again
		if(file == ID_NULL)
			return 1;
		buf->file = file;
	}
	if(fc_recache(buf->file) == 1) {
		const int m = messagebox("File not in line", "The file you're trying to write to was modified outside the editor, do you still want to write to it?", "[Y]es", "[N]o", NULL);
		if(m != 0)
			return 1;
	}
	FILE *const fp = fc_open(buf->file, "w");
	if(fp == NULL)
		return -1;
	fwrite(buf->data, 1, buf->iGap, fp);
	fwrite(buf->data + buf->iGap + buf->nGap, 1, buf->nData - buf->iGap, fp);
	fclose(fp);
	buf->saveEvent = buf->iEvent;
	return 0;
}

ptrdiff_t
unsafe_buffer_movecursor(struct buffer *buf, ptrdiff_t distance)
{
	/*This method retained the gap content
	ptrdiff_t d;
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

static inline ptrdiff_t
buffer_cnvdist(const struct buffer *buf, ptrdiff_t distance)
{
	return utf8_cnvdist(buf->data, buf->nData + buf->nGap, buf->iGap + (distance > 0 ? buf->nGap : 0), distance);
}

ptrdiff_t
buffer_movecursor(bufferid_t bufid, ptrdiff_t distance)
{
	struct buffer *const buf = all_buffers + bufid;
	distance = buffer_cnvdist(buf, distance);
	unsafe_buffer_movecursor(buf, distance);
	buf->vct = buffer_col(bufid);
	return distance;
}

ptrdiff_t
buffer_movehorz(bufferid_t bufid, ptrdiff_t distance)
{
	size_t i, iFirst, left, right;
	struct buffer *const buf = all_buffers + bufid;
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

ptrdiff_t
buffer_movevert(bufferid_t bufid, ptrdiff_t distance)
{
	size_t i;
	ptrdiff_t moved;
	const ptrdiff_t cDistance = distance;
	struct buffer *const buf = all_buffers + bufid;
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
		for(int travelled = 0; travelled < buf->vct;) {
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
		for(int travelled = 0; i < buf->nData + buf->nGap && travelled < buf->vct;) {
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

static struct buffer_event *
buffer_addevent(bufferid_t bufid)
{
	struct buffer_event *ev, *newEvents;
	struct buffer *const buf = all_buffers + bufid;
	if(buf->nEvents > buf->iEvent) {
		for(unsigned i = buf->iEvent; i < buf->nEvents; i++) {
			switch(buf->events[i].type) {
			case BUFFER_EVENT_INSERT:
				free(buf->events[i].ins);
				break;
			case BUFFER_EVENT_DELETE:
				free(buf->events[i].del);
				break;
			case BUFFER_EVENT_REPLACE:
				free(buf->events[i].ins);
				free(buf->events[i].del);
				break;
			}
		}
	}
	newEvents = dialog_realloc(buf->events, sizeof(*buf->events) * (buf->iEvent + 1));
	if(!newEvents)
		return NULL;
	buf->events = newEvents;
	ev = buf->events + buf->iEvent;
	buf->nEvents = ++buf->iEvent;
	return ev;
}

size_t
buffer_insert(bufferid_t bufid, const char *str, size_t nStr)
{
	struct buffer_event *ev;
	struct buffer *const buf = all_buffers + bufid;
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
	buf->vct = buffer_col(bufid);
	// try to join events
	if(buf->iEvent) {
		ev = buf->events + buf->iEvent - 1;
		if(ev->type == BUFFER_EVENT_INSERT && ev->iGap + ev->nIns == buf->iGap - nStr) {
			ev->ins = dialog_realloc(ev->ins, ev->nIns + nStr);
			memcpy(ev->ins + ev->nIns, str, nStr);
			ev->vct = buf->vct;
			ev->nIns += nStr;
			return nStr;
		}
	}
	// add event
	if(!(ev = buffer_addevent(bufid)))
		return nStr;
	*ev = (struct buffer_event) {
		.flags = 0,
		.type = BUFFER_EVENT_INSERT,
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

size_t
buffer_insert_file(bufferid_t bufid, fileid_t file)
{
	char *s;
	FILE *const fp = fc_open(file, "r");
	if(!fp)
		return 0;
	fseek(fp, 0, SEEK_END);
	const long n = ftell(fp);
	if(n <= 0 || !(s = dialog_alloc(n)))
		return 0;
	fseek(fp, 0, SEEK_SET);
	fread(s, 1, n, fp);
	const size_t nIns = buffer_insert(bufid, s, n);
	free(s);
	return nIns;
}

ptrdiff_t
buffer_delete(bufferid_t bufid, ptrdiff_t amount)
{
	struct buffer_event *ev;
	struct buffer *const buf = all_buffers + bufid;
	amount = buffer_cnvdist(buf, amount);
	if(!amount)
		return 0;
	// add event
	if(!(ev = buffer_addevent(bufid)))
		return amount;
	*ev = (struct buffer_event) {
		.flags = 0,
		.type = BUFFER_EVENT_DELETE,
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
	buf->vct = buffer_col(bufid);
	return amount;
}

ptrdiff_t
buffer_deleteline(bufferid_t bufid, ptrdiff_t amount)
{
	struct buffer_event *ev;
	size_t l, r;
	size_t nLinesDeleted = 0;
	struct buffer *const buf = all_buffers + bufid;
	if(!amount)
		return 0;
	l = buf->iGap;
	r = buf->iGap + buf->nGap;
	for(ptrdiff_t a = ABS(amount);; ) {
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
	if(!(ev = buffer_addevent(bufid)))
		return nLinesDeleted;
	*ev = (struct buffer_event) {
		.flags = 0,
		.type = BUFFER_EVENT_DELETE,
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
buffer_undo(bufferid_t bufid)
{
	struct buffer *const buf = all_buffers + bufid;
	if(!buf->iEvent)
		return 0;
	const struct buffer_event ev = buf->events[--buf->iEvent];
	switch(ev.type) {
	case BUFFER_EVENT_INSERT:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		buf->nGap += ev.nIns;
		buf->nData -= ev.nIns;
		break;
	case BUFFER_EVENT_DELETE:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		memmove(buf->data + ev.iGap, ev.del, ev.nDel);
		buf->iGap += ev.nDel;
		buf->nGap -= ev.nDel;
		buf->nData += ev.nDel;
		break;
	case BUFFER_EVENT_REPLACE:
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
buffer_redo(bufferid_t bufid)
{
	struct buffer *const buf = all_buffers + bufid;
	if(buf->iEvent == buf->nEvents)
		return 0;
	const struct buffer_event ev = buf->events[buf->iEvent++];
	switch(ev.type) {
	case BUFFER_EVENT_INSERT:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		memmove(buf->data + buf->iGap, ev.ins, ev.nIns);
		buf->iGap += ev.nIns;
		buf->nGap -= ev.nIns;
		buf->nData += ev.nIns;
		break;
	case BUFFER_EVENT_DELETE:
		unsafe_buffer_movecursor(buf, ev.iGap - buf->iGap);
		buf->nGap += ev.nDel;
		buf->nData -= ev.nDel;
		break;
	case BUFFER_EVENT_REPLACE:
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

int
buffer_line(bufferid_t bufid)
{
	int line = 0;
	struct buffer *const buf = all_buffers + bufid;
	for(size_t i = buf->iGap; i > 0; i--)
		if(buf->data[i - 1] == '\n')
			line++;
	return line;
}

int
buffer_col(bufferid_t bufid)
{
	size_t i;
	struct buffer *const buf = all_buffers + bufid;
	for(i = buf->iGap; i > 0; i--)
		if(buf->data[i - 1] == '\n')
			break;
	return utf8_widthnstr(buf->data + i, buf->iGap - i);
}

size_t
buffer_getline(bufferid_t bufid, int line, char *dest, size_t maxDest)
{
	size_t i, j;
	int k;
	struct buffer *const buf = all_buffers + bufid;
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
