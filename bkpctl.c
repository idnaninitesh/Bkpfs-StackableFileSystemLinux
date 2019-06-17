// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "linux/bkpfs.h"


/*
 * check is string is numeric or not
 * returns 1 if valid, else 0
 */
int is_numeric(char *ver)
{

	int i = 0;

	while (ver[i] != '\0' && isdigit(ver[i]))
		i++;

	if (ver[i] == '\0')
		return 1;

	return 0;

}

/*
 * validate the l/d/v/r flags and if version is valid or not semantically
 * returns 0 if valid, else -1
 */
int validate_flags(int lflag, int dflag, int vflag, int rflag,
			int hflag, char *ver, int rem_args)
{
	int err = -1;

	if (hflag) {
		if (lflag || dflag || vflag || rflag || rem_args != 0)
			return err;
	} else {

		if (lflag) {
			if (dflag || vflag || rflag)
				return err;
			if (strlen(ver) != 0)
				return err;
		}

		if (dflag) {
			if (lflag || vflag || rflag || strlen(ver) == 0)
				return err;
			if (!(strcmp(ver, "oldest") == 0 ||
				strcmp(ver, "newest") == 0 ||
					strcmp(ver, "all") == 0))
				return err;
		}

		if (vflag) {
			if (lflag || dflag || rflag || strlen(ver) == 0)
				return err;
			if (!(strcmp(ver, "oldest") == 0 ||
				strcmp(ver, "newest") == 0
					|| is_numeric(ver)))
				return err;
		}

		if (rflag) {
			if (lflag || dflag || vflag || strlen(ver) == 0)
				return err;
			if (!(strcmp(ver, "newest") == 0
					|| is_numeric(ver)))
				return err;
		}

		if (rem_args != 1)
			return err;

	}

	return 0;
}

/*
 * validate if file exists, has read perm and is regular
 * return 0 is valid, else -1
 */
int validate_file(char *origfile_name)
{

	int err = -1;
	struct stat origfile_buf;

	if (access(origfile_name, R_OK))
		return err;

	if (stat(origfile_name, &origfile_buf) == -1)
		return err;

	if (!(origfile_buf.st_mode & S_IFREG))
		return err;
	return 0;
}


int main(int argc, char *argv[])
{

	char usage[] = "\n"
	"Usage:  ./bkpctl -[d|v|r] VERSION FILE\n"
	"or:  ./bkpctl -l FILE\n"
	"Arguments to be passed for usage.\n"
	"FILE\tvalid regular file name in mounted dir\n"
	"-l\t\tlist version file names created for FILE\n"
	"-d\t\tdelete the specified VERSION file for FILE "
	"(must be oldest, newest or all)\n"
	"-v\t\tview the data in specified VERSION file for FILE "
	"(must be oldest, newest or V)\n"
	"-r\t\trestore the specified VERSION file for FILE "
	"(must be newest or V)\n\n";

	int rc = 0;
	int lflag = 0;
	int dflag = 0;
	int vflag = 0;
	int rflag = 0;
	int hflag = 0;
	int opt;
	int fd;
	char *ver = "";
	char *origfile_name = "";
	struct ioctl_readargs arg;
	FILE *fptr;
	char oper_str[101];
	unsigned long op = 0;
	int temp_flag = 1;

	while ((opt = getopt(argc, argv, "hld:v:r:")) != -1) {
		switch (opt) {
		case 'l':
			lflag = 1 << 0;
			break;
		case 'd':
			dflag = 1 << 1;
			ver = optarg;
			break;
		case 'v':
			vflag = 1 << 2;
			ver = optarg;
			break;
		case 'r':
			rflag = 1 << 3;
			ver = optarg;
			break;
		case 'h':
			hflag = 1 << 4;
			break;
		default:
			printf(usage);
			exit(EXIT_FAILURE);
		}
	}

	rc = validate_flags(lflag, dflag, vflag, rflag, hflag,
						ver, argc-optind);

	if (rc) {
		printf("Invalid flag argument(s) passed\n");
		printf(usage);
		exit(EXIT_FAILURE);
	}


	if (hflag) {
		printf(usage);
	} else if (lflag || dflag || vflag || rflag) {

		origfile_name = argv[optind];
		rc = validate_file(origfile_name);

		if (rc) {
			printf("Invalid file(s) passed\n");
			printf(usage);
			exit(EXIT_FAILURE);
		}

		if (lflag) {
			arg.version = "all";
			arg.buf_len = MAX_FILE_NAMELIST_LEN;
			op = BKPFS_VER_LIST;
			strcpy(oper_str, "Listed version files:\n");
		} else if (dflag) {
			arg.version = ver;
			arg.buf_len = MAX_FILE_NAME_LEN;
			op = BKPFS_VER_DEL;
			strcpy(oper_str, "Deleted ");
			strcat(oper_str, ver);
			strcat(oper_str, " version file");
		} else if (vflag) {
			arg.version = ver;
			arg.buf_len = 10;
			op = BKPFS_VER_VIEW;
			strcpy(oper_str, ver);
			strcat(oper_str, " version file data:");
		} else if (rflag) {
			arg.version = ver;
			arg.buf_len = MAX_FILE_NAME_LEN;
			op = BKPFS_VER_RESTORE;
			strcpy(oper_str, "Restored ");
			strcat(oper_str, ver);
			strcat(oper_str, " version file in ");
		}

		arg.version_len = strlen(arg.version);
		arg.pos = 0;
		arg.buffer = malloc(arg.buf_len);
		memset(arg.buffer, '\0', arg.buf_len);
		fptr = fopen(origfile_name, "r");
		fd = fileno(fptr);
		if (fd) {
			if (op == BKPFS_VER_VIEW) {
				do {
					rc = ioctl(fd, op,
						(unsigned long) &arg);
					if (rc > 0 && temp_flag) {
						printf("%s\n", oper_str);
						temp_flag = 0;
					}
					printf("%s", arg.buffer);
					memset(arg.buffer, '\0', arg.buf_len);
				} while (rc > 0);
				if (rc)
					printf("File operation returned "
						"%d (errno=%d)\n", rc, errno);
			} else {
				rc = ioctl(fd, op, (unsigned long) &arg);
				if (rc == 0) {
					printf("File operation successful\n");
					printf("%s%s\n", oper_str, arg.buffer);
				} else {
					printf("File operation returned "
						"%d (errno=%d)\n", rc, errno);
				}
			}
		}

		free(arg.buffer);
	} else {
		printf("Invalid flag passed\n");
		printf(usage);
		exit(EXIT_FAILURE);
	}

	exit(rc);

}
