#ifdef __unix__

#include "cnergy.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int
getfileinfo(struct file_info *fi, const char *path)
{
	struct stat statbuf;
	bool isDir, isReg, isExec;

	if(lstat(path, &statbuf) < 0)
		return -1;
	isDir = S_ISDIR(statbuf.st_mode);
	isReg = S_ISREG(statbuf.st_mode);
	isExec = statbuf.st_mode & 0111;
	fi->type = isDir ? FI_TYPE_DIR : 
		isExec ? FI_TYPE_EXEC :
		isReg ? FI_TYPE_REG : FI_TYPE_OTHER;
	fi->chgTime = statbuf.st_ctime;
	fi->modTime = statbuf.st_mtime;
	fi->accTime = statbuf.st_atime;
	return 0;
}

#endif
