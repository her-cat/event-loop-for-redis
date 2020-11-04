#include <stdio.h>
#include "ae.h"

int main() {
	printf("Hello, World! %d\n", aeCreateEventLoop(10)->setsize);
	return 0;
}
