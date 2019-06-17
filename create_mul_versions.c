#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[])
{

	FILE *fptr;
	char *name = argv[1];

	fptr = fopen(name, "a");
	if (fptr != NULL) {
		fprintf(fptr, "first bytes\n");
		fclose(fptr);
	}

	fptr = fopen(name, "a");
	if (fptr != NULL) {
		fprintf(fptr, "bytes\n");
		fprintf(fptr, "second bytes\n");
		fclose(fptr);
	}

	fptr = fopen(name, "a");
	if (fptr != NULL) {
		fprintf(fptr, "second bytes\n");
		fclose(fptr);
	}

	fptr = fopen(name, "a");
	if (fptr != NULL) {
		fprintf(fptr, "fourth bytes\n");
		fclose(fptr);
	}

	return 0;
}
