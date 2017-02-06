#include <stdio.h>

#include <lib_print.h>
#include <lib_sum.h>

int main(int argc, char* argv[])
{
	printf("Running...\n");
	print_hello();
	printf("The sum of 2 and 3 is %d\n", sum2(2, 3));
	return 0;
}
