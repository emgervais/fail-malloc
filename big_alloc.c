#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// 1000 allocation per 100 KiB = 100 000 KiB = 100 MiB
#define NALLOCS 10
#define ALLOC_SIZE 1024*100 // 100 KiB

int main(int argc, const char *argv[])
{
	int i;
	int **pp;
	bool failed = false;

	pp = malloc(NALLOCS * sizeof(int *));
	for(i = 1; i <= NALLOCS; i++)
	{
		pp[i] = malloc(ALLOC_SIZE);
		if (!pp[i])
		{
			perror("malloc");
			printf("Failed after %d allocations\n", i);
			failed = true;
			break;
		}
		memset(pp[i], 0xA, 100);
	}

	if (!failed)
		printf("Successfully allocated %d bytes\n", NALLOCS * ALLOC_SIZE);

	for(i = 0; i < NALLOCS; i++)
	{
		if (pp[i])
			free(pp[i]);
	}
	free(pp);

	return 0;
}
