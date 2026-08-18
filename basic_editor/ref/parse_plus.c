#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc >= 2) {
		if (argv[1][0] == '+') {
			char *res1 = strchr(argv[1], '+');
			char *res2 = strchr(argv[1], ':');
			int resint1 = -1, resint2 = -1;
			if (res1 != NULL) {
				resint1 = atoi(res1+1);

			}
			if (res2 != NULL) {
				resint2 = atoi(res2+1);
			}
			printf("Parsed Row %d, Parsed Col %d\n", resint1, resint2);
		}
	}
	int a = atoi("some");
	printf("%d\n", a);
	return 0;
}
