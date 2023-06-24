#ifndef INCLUDED_FILECACHE_H
#define INCLUDED_FILECACHE_H

/* File caching */
// filecache.c
/**
 * This tries to create a cache of the files and directories
 * of the user.
 */

/** If a cached file has this flag and it's a directory, all sub files/dirs are also cached */
#define FC_COLLAPSED 0x10
/** The node is not in the user's filesystem */
#define FC_VIRTUAL 0x80

struct filecache {
	char name[NAME_MAX];
	unsigned flags;
	unsigned mode;
	time_t atime, ctime, mtime;
	fileid_t *children;
	unsigned nChildren;
	fileid_t parent;
};

/** Similar to getting the current working directory */
fileid_t fc_getbasefile(void);
/** Get the relative file path of given file */
int fc_getrelativepath(fileid_t fromid, fileid_t fileid, char *dest, size_t maxDest);
/** Get the absolute file path of given file */
int fc_getabsolutepath(fileid_t fileid, char *dest, size_t maxDest);
/** Open the file at given fileid */
FILE *fc_open(fileid_t fileid, const char *mode);
/** Get the file cache data */
struct filecache *fc_lock(fileid_t fileid);
/** Get a file type compute from the mode member of fc */
enum {
	FC_TYPE_DIR,
	FC_TYPE_REG,
	FC_TYPE_EXEC, // this is not really a type but it's still good to differentiate files with and without execution rights
	FC_TYPE_OTHER,
};
int fc_type(struct filecache *fc);
bool fc_isdir(struct filecache *fc);
bool fc_isreg(struct filecache *fc);
bool fc_isexec(struct filecache *fc);
bool fc_iswrite(struct filecache *fc);
bool fc_isread(struct filecache *fc);
void fc_unlock(struct filecache *fc);
/** Caches a path that starts from given file */
fileid_t fc_cache(fileid_t fileid, const char *path);
/** Gets the parent of this file */
fileid_t fc_getparent(fileid_t fileid);
/** Compares cached file with real filesystem */
int fc_recache(fileid_t fileid);
/** Finds a cached file that starts from given path */
fileid_t fc_find(fileid_t fileid, const char *path);

#endif
