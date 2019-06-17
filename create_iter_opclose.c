#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{

	FILE *fptr;
	char *name = argv[1];
	int bytes = 0;
	char *write_str="check bkp files\n";
	int len = strlen(write_str);
	int max_len = 65536;

	while (bytes < max_len) {
		fptr = fopen(name, "a");
		if (fptr != NULL) {
			fprintf(fptr, write_str);
			bytes += len;
			fclose(fptr);
		}
		else {
			break;
		}
	}

	return 0;
}
