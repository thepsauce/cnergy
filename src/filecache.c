#include "cnergy.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/** The first entry of file_caches is the root */
static struct filecache *file_caches;
static fileid_t n_file_caches;
static fileid_t file_current;
/** Safety mechanism for the convenience of the programmer */
static unsigned locks = 0;

inline fileid_t
fc_getbasefile(void)
{
	return file_current;
}

static struct filecache *
fc_addcache(fileid_t parentid, const char *name, unsigned nName)
{
	struct filecache *newFileCaches;
	struct filecache *fc, *parent;
	fileid_t *newChildren;

	assert(!locks && "a lock is still in place, unluck all filecaches before continuing");
	assert(parentid < n_file_caches && "file id is invalid");
	// check if the file is already cached
	parent = file_caches + parentid;
	for(unsigned i = 0; i < parent->nChildren; i++) {
		fc = file_caches + parent->children[i];
		if(!strncmp(fc->name, name, nName) && !fc->name[nName])
			return fc;
	}
	newFileCaches = dialog_realloc(file_caches, sizeof(*file_caches) * (n_file_caches + 1));
	if(newFileCaches == NULL)
		return NULL;
	file_caches = newFileCaches;
	fc = file_caches + n_file_caches;
	// it is possible that the base pointer of file_caches changed, so resetting the parent pointer is needed here
	parent = file_caches + parentid;
	memset(fc, 0, sizeof(*fc));
	memcpy(fc->name, name, nName);
	fc->name[nName] = 0;
	fc->parent = parentid;
	newChildren = dialog_realloc(parent->children, sizeof(*parent->children) * (parent->nChildren + 1));
	if(newChildren == NULL)
		return NULL;
	parent->children = newChildren;
	parent->children[parent->nChildren++] = n_file_caches;
	n_file_caches++;
	return fc;
}

static inline void
fc_getdata(struct filecache *fc, const char *path)
{
	struct stat sb;

	if(lstat(path, &sb) < 0) {
		fc->flags |= FC_VIRTUAL;
		// if the time is 0, that means it was never set before
		if(!fc->ctime) {
			struct timeval tv;
			time_t time;

			gettimeofday(&tv, NULL);
			time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
			fc->mode = 0;
			fc->ctime = time;
			fc->mtime = time;
			fc->atime = time;
		}
	} else {
		fc->flags &= ~FC_VIRTUAL;
		fc->mode = sb.st_mode;
		fc->ctime = sb.st_ctime;
		fc->mtime = sb.st_mtime;
		fc->atime = sb.st_atime;
	}
}

__attribute__((constructor)) static void
fc_init(void)
{
	struct filecache *fc;
	char path[PATH_MAX];

	if(!getcwd(path, sizeof(path))) {
		perror("getcwd");
		abort();
	}
	file_caches = malloc(sizeof(*file_caches) * 20);
	n_file_caches = 1;

	fc = file_caches;
	memset(fc, 0, sizeof(*fc));
	fc_getdata(fc, "/");
	file_current = fc_cache(0, path);
	file_caches[file_current].flags |= FC_COLLAPSED;
	fc_recache(file_current);
}

struct filecache *
fc_lock(fileid_t fileid)
{
	assert(fileid < n_file_caches && "file is invalid");
	locks++;
	return file_caches + fileid;
}

inline int fc_type(struct filecache *fc)
{
	return (fc->nChildren || S_ISDIR(fc->mode)) ? FC_TYPE_DIR :
		(fc->flags & FC_VIRTUAL) ? FC_TYPE_REG :
		(fc->mode & 0111) ? FC_TYPE_EXEC :
		S_ISREG(fc->mode) ? FC_TYPE_REG :
		FC_TYPE_OTHER;
}

inline bool fc_isdir(struct filecache *fc)
{
	return S_ISDIR(fc->mode);
}

inline bool fc_isreg(struct filecache *fc)
{
	return S_ISREG(fc->mode);
}

inline bool fc_isexec(struct filecache *fc)
{
	return fc->mode & 0111;
}

inline bool fc_iswrite(struct filecache *fc)
{
	return fc->mode & 0222;
}

inline bool fc_isread(struct filecache *fc)
{
	return fc->mode & 0444;
}

void
fc_unlock(struct filecache *fc)
{
	const fileid_t id = fc - file_caches;
	assert(id < n_file_caches && "file id is invalid");
	assert(locks && "there are no locks to unlock (mismatch of lock and unlock)");
	locks--;
}

static int
fc_locate(struct filecache *from, const char *path, const char **endPath, struct filecache **pFc)
{
	const char *pushBack = NULL;
	const char *name;
	unsigned nName;

	if(path[0] == '~') {
		// go to home
		const char *const home = getenv("HOME");
		if(!home) {
			fprintf(stderr, "[%s:%u in fc_locate()] failed to get home directory\n", __FILE__, __LINE__);
			return -1;
		}
		pushBack = path;
		path = home;
	}
	if(path[0] == '/') {
		from = file_caches;
		path++;
	}
walk:
	name = path;
	nName = 0;
	// find end of name segment
	while(*path && *(path++) != '/')
		nName++;
	if(nName >= NAME_MAX) {
		fprintf(stderr, "[%s:%u in fc_locate()] name segment too long (exceeds %u byte)\n",
				__FILE__, __LINE__, (unsigned) NAME_MAX);
		return -1;
	}
	if(!nName) {
		if(pushBack) {
			path = pushBack;
			pushBack = NULL;
			goto walk;
		}
		*endPath = name;
		*pFc = from;
		return 0;
	}
	if(name[0] == '.') {
		// . basically means nothing
		if(nName == 1)
			goto walk;
		// .. means go to the parent directory
		if(nName == 2 && name[1] == '.') {
			from = file_caches + from->parent;
			goto walk;
		}
	}
	// find the next child
	for(unsigned i = 0; i < from->nChildren; i++) {
		struct filecache *const fc = file_caches + from->children[i];
		if(!strncmp(fc->name, name, nName) && !fc->name[nName]) {
			from = fc;
			goto walk;
		}
	}
	*endPath = pushBack ? : name;
	*pFc = from;
	return -1;
}

int
fc_getrelativepath(fileid_t from, fileid_t file, char *dest, size_t maxDest)
{
	struct filecache *fc;
	unsigned n;

	if(maxDest < 2)
		return -1;
	if(from == file) {
		dest[0] = '.';
		dest[1] = 0;
		return 0;
	}
	const size_t sMaxDest = maxDest;
	fc = file_caches + file;
	dest += --maxDest;
	*dest = 0;
	while(fc != file_caches + from) {
		if(fc == file_caches) {
			fprintf(stderr, "[%s:%u in fc_getrelativepath()] file '%s' is not relative to '%s'\n",
					__FILE__, __LINE__, file_caches[from].name, file_caches[file].name);
			return -1;
		}
		n = strlen(fc->name);
		if(maxDest <= n) {
			fprintf(stderr, "[%s:%u in fc_getrelativepath()] path is too long\n", __FILE__, __LINE__);
			return -1;
		}
		dest--;
		if(dest[1])
			*dest = '/';
		maxDest--;
		dest -= n;
		maxDest -= n;
		memcpy(dest, fc->name, n);
		fc = file_caches + fc->parent;
	}
	memmove(dest - maxDest, dest, sMaxDest - maxDest);
	return 0;
}

int
fc_getabsolutepath(fileid_t file, char *dest, size_t maxDest)
{
	struct filecache *fc;
	unsigned n;

	assert(file < n_file_caches && "file id does not exist");
	assert(dest && "destination can not be null");
	if(maxDest < 2)
		goto err_path_too_long;
	const size_t sMaxDest = maxDest;
	dest += --maxDest;
	*dest = 0;
	fc = file_caches + file;
	while(1) {
		if(!maxDest)
			goto err_path_too_long;
		dest--;
		if(dest[1])
			*dest = '/';
		maxDest--;
		if(fc == file_caches)
			break;
		n = strlen(fc->name);
		if(maxDest < n)
			goto err_path_too_long;
		dest -= n;
		maxDest -= n;
		memcpy(dest, fc->name, n);
		fc = file_caches + fc->parent;
	}
	// we have written the buffer from end to start, so we have to shift it to the left to
	// make it from left to right
	memmove(dest - maxDest, dest, sMaxDest - maxDest);
	return 0;
err_path_too_long:
	fprintf(stderr, "[%s:%u in fc_getabsolutepath()] path of file '%s' is too long (exceeding '%zu' bytes)\n", __FILE__, __LINE__, file_caches[file].name, sMaxDest);
	return -1;
}

FILE *
fc_open(fileid_t file, const char *mode)
{
	char path[PATH_MAX];
	FILE *fp;

	assert(file < n_file_caches && "file id does not exist");
	if(fc_getrelativepath(file_current, file, path, sizeof(path)) < 0)
		return NULL;
	fp = fopen(path, mode);
	if(fp == NULL) {
		fprintf(stderr, "[%s:%u in fc_open()] failed opening file '%s'\n", __FILE__, __LINE__, file_caches[file].name);
		return NULL;
	}
	return fp;
}

fileid_t
fc_cache(fileid_t file, const char *path)
{
	struct filecache *fc, newfc;
	const char *name;
	unsigned nName;
	char curPath[PATH_MAX];

	assert(file < n_file_caches && "file id is invalid");
	if(!fc_locate(file_caches + file, path, &path, &fc))
		return fc - file_caches; // already cached
	// cache the file by creating a path from a valid node to the file
walk:
	name = path;
	nName = 0;
	// find end of name segment
	while(*path && *(path++) != '/')
		nName++;
	if(nName >= NAME_MAX) {
		fprintf(stderr, "[%s:%u in fc_cache()] name segment too long (exceeds %u byte) (at='%s')\n",
				__FILE__, __LINE__, (unsigned) NAME_MAX, name);
		return ID_NULL;
	}
	if(!nName)
		return fc - file_caches;
	memset(&newfc, 0, sizeof(newfc));
	memcpy(newfc.name, name, nName);
	newfc.name[nName] = 0;
	if(!(fc = fc_addcache(fc - file_caches, name, nName)))
		return ID_NULL;
	if(!fc_getrelativepath(file_current, fc - file_caches, curPath, sizeof(curPath)))
		fc_getdata(fc, curPath);
	goto walk;
}

int
fc_recache(fileid_t file)
{
	char path[PATH_MAX];

	assert(file < n_file_caches && "file id is invalid");
	fc_getrelativepath(file_current, file, path, sizeof(path));
	fc_getdata(file_caches + file, path);
	if(file_caches[file].flags & FC_COLLAPSED) {
		DIR *dir;
		struct dirent *ent;
		struct filecache *f;

		dir = opendir(path);
		if(dir) {
			const unsigned len = strlen(path);
			path[len] = '/';
			while((ent = readdir(dir))) {
				const unsigned n = strlen(ent->d_name);
				// check for . or ..
				if(ent->d_name[0] == '.' && (n == 1 || (n == 2 && ent->d_name[1] == '.')))
					continue;
				if(!(f = fc_addcache(file, ent->d_name, n)))
					return -1;
				strcpy(path + len + 1, ent->d_name);
				fc_getdata(f, path);
			}
		}
	}
	return 0;
}

fileid_t
fc_find(fileid_t file, const char *path)
{
	struct filecache *fc;

	assert(file < n_file_caches && "file id is invalid");
	if(!fc_locate(file_caches + file, path, &path, &fc))
		return fc - file_caches;
	return ID_NULL;
}

void
printnode(struct filecache *fc, int d)
{
	if(!fc)
		fc = file_caches;
	for(unsigned i = 0; i < fc->nChildren; i++) {
		struct filecache *f = file_caches + fc->children[i];
		printf("%*s%s (%u, %lu)\n", d * 2, "", f->name, f->mode, f->mtime);
		printnode(f, d + 1);
	}
}
