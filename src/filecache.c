#include "cnergy.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/** The first entry of file_caches is at the base_directory */
static struct filecache *file_caches;
static fileid_t n_file_caches;
static char base_directory[PATH_MAX];
/** Safety mechanism for the convenience of the programmer */
static unsigned locks = 0;

static struct filecache *
fc_addcache(struct filecache *parent, const char *name, unsigned nName)
{
	struct filecache *newFileCaches;
	struct filecache *fc;
	fileid_t *newChildren;

	assert(!locks && "a lock is still in place, unluck all filecaches before continuing");

	newFileCaches = realloc(file_caches, sizeof(*file_caches) * (n_file_caches + 1));
	if(!newFileCaches)
		return NULL;
	file_caches = newFileCaches;
	fc = file_caches + n_file_caches;
	memset(fc, 0, sizeof(*fc));
	memcpy(fc->name, name, nName);
	fc->name[nName] = 0;
	fc->parent = parent - file_caches;
	newChildren = realloc(parent->children, sizeof(*parent->children) * (parent->nChildren + 1));
	if(!newChildren)
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
		struct timeval tv;
		time_t time;

		fc->flags = FC_VIRTUAL;
		gettimeofday(&tv, NULL);
		time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		fc->mode = 0;
		fc->ctime = time;
		fc->mtime = time;
		fc->atime = time;
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
	getcwd(base_directory, sizeof(base_directory));
	file_caches = malloc(sizeof(*file_caches) * 20);
	n_file_caches = 1;

	DIR *dir;
	struct dirent *ent;
	struct filecache *fc;
	struct filecache *f;
	unsigned len;
	unsigned n;

	fc = file_caches;
	memset(fc, 0, sizeof(*fc));
	fc_getdata(fc, base_directory);
	dir = opendir(base_directory);
	if(dir) {
		len = strlen(base_directory);
		while((ent = readdir(dir))) {
			n = strlen(ent->d_name);
			// check for . or ..
			if(ent->d_name[0] == '.' && (n == 1 || (n == 2 && ent->d_name[1] == '.')))
				continue;
			if(!(f = fc_addcache(fc, ent->d_name, n)))
				return;
			base_directory[len] = '/';
			memcpy(base_directory + len + 1, ent->d_name, n);
			fc_getdata(f, base_directory);
			base_directory[len] = 0;
		}
	}
}

struct filecache *
fc_lock(fileid_t id)
{
	locks++;
	return file_caches + id;
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
	assert(id < n_file_caches && "file is invalid");
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
		if(!home)
			return -1;
		pushBack = path;
		path = home;
	}
	if(path[0] == '/') {
		// go to the root
		if(base_directory[0] != '/' || base_directory[1])
			goto err; // don't have anything cached inside the root
		path++;
	}
walk:
	name = path;
	nName = 0;
	// find end of name segment
	while(*path && *(path++) != '/')
		nName++;
	if(nName >= NAME_MAX)
		return -1;
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
			if(from == file_caches) {
				// check if we are at the root /
				if(base_directory[0] == '/' && !base_directory[1])
					goto walk;
				goto err;
			}
			from = file_caches + from->parent;
			goto walk;
		}
	}
	for(unsigned i = 0; i < from->nChildren; i++) {
		struct filecache *fc;
		fc = file_caches + from->children[i];
		if(!strncmp(fc->name, name, nName) && !fc->name[nName]) {
			from = fc;
			goto walk;
		}
	}
err:
	*endPath = pushBack ? : name;
	*pFc = from;
	return -1;
}

int
fc_getrelativepath(fileid_t file, char *dest, size_t maxDest)
{
	struct filecache *fc;
	unsigned n;
	const size_t sMaxDest = maxDest;
	fc = file_caches + file;
	dest += --maxDest;
	*dest = 0;
	while(fc != file_caches) {
		if(!maxDest)
			return -1;
		dest--;
		if(dest[1])
			*dest = '/';
		maxDest--;
		n = strlen(fc->name);
		if(maxDest < n)
			return -1;
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
	const size_t sMaxDest = maxDest;
	dest += --maxDest;
	*dest = 0;
	fc = file_caches + file;
	while(1) {
		const char *name;

		if(!maxDest)
			return -1;
		dest--;
		if(dest[1])
			*dest = '/';
		maxDest--;
		name = fc == file_caches ? base_directory : fc->name;
		n = strlen(name);
		if(maxDest < n)
			return -1;
		dest -= n;
		maxDest -= n;
		memcpy(dest, name, n);
		if(fc == file_caches)
			break;
		fc = file_caches + fc->parent;
	}
	memmove(dest - maxDest, dest, sMaxDest - maxDest);
	return 0;
}

FILE *
fc_open(fileid_t file, const char *mode)
{
	char path[PATH_MAX];
	FILE *fp;

	if(fc_getrelativepath(file, path, sizeof(path)) < 0)
		return NULL;
	fp = fopen(path, mode);
	if(!fp)
		return NULL;
	return fp;
}

// TODO: be able to go in a directory that is above the base directory
fileid_t
fc_cache(fileid_t file, const char *path)
{
	struct filecache *fc, newfc;
	const char *name;
	unsigned nName;
	size_t len, n;
	char curPath[PATH_MAX];

	if(!fc_locate(file_caches + file, path, &path, &fc))
		return fc - file_caches; // already cached
	n = strlen(path);
	len = 0;
	memcpy(curPath + len, path, n);
	len += n;
	curPath[len] = 0;
	// cache the file by creating a path from a valid node to the file
walk:
	name = path;
	nName = 0;
	// find end of name segment
	while(*path && *(path++) != '/')
		nName++;
	if(nName >= NAME_MAX)
		return -1;
	if(!nName)
		return fc - file_caches;
	memset(&newfc, 0, sizeof(newfc));
	memcpy(newfc.name, name, nName);
	newfc.name[nName] = 0;
	if(!(fc = fc_addcache(fc, name, nName)))
		return -1;
	fc_getdata(fc, curPath);
	goto walk;
}

int
fc_recache(fileid_t file)
{
	char path[PATH_MAX];
	struct filecache *fc;

	fc = file_caches + file;
	fc_getrelativepath(file, path, sizeof(path));
	fc_getdata(fc, path);
	if(fc->flags & FC_COLLAPSED) {
		DIR *dir;
		struct dirent *ent;
		struct filecache *f;
		unsigned len;
		unsigned n;

		dir = opendir(path);
		if(dir) {
			len = strlen(path);
			while((ent = readdir(dir))) {
				n = strlen(ent->d_name);
				// check for . or ..
				if(ent->d_name[0] == '.' && (n == 1 || (n == 2 && ent->d_name[1] == '.')))
					continue;
				if(!(f = fc_addcache(fc, ent->d_name, n)))
					return -1;
				path[len] = '/';
				memcpy(path + len + 1, ent->d_name, n);
				fc_getdata(f, path);
				path[len] = 0;
			}
		}
	}
	return 0;
}

fileid_t
fc_find(fileid_t file, const char *path)
{
	struct filecache *fc;

	if(!fc_locate(file_caches + file, path, &path, &fc))
		return fc - file_caches;
	return 0;
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
