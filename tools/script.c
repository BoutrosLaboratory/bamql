#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	char **args = calloc(argc + 2, sizeof(char *));
	int i;
	args[0] = "bamql";
	args[1] = "-q";
	for (i = 1; i <= argc; i++) {
		args[i + 1] = argv[i];
	}
	execvp("bamql", args);
	perror("bamql");
	return 1;
}
