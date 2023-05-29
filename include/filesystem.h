#ifndef INLUDED_FILESYSTEM_H
#define INLUDED_FILESYSTEM_H

#include "base.h"

#define FI_TYPE_DIR 1
#define FI_TYPE_REG 2
#define FI_TYPE_EXEC 3
#define FI_TYPE_OTHER 4

struct file_info {
	U64 hash;
	U32 type;
	// inode change
	time_t chgTime;
	// last modification time
	time_t modTime;
	// last access time
	time_t accTime;
};

int getfileinfo(struct file_info *fi, const char *path);

#endif
