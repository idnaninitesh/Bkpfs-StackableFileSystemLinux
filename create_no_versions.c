#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[])
{

	FILE *fptr;
	char *name = argv[1];

	fptr = fopen(name, "w");
	if (fptr != NULL)
		fclose(fptr);

	return 0;
}
