#include <stdio.h>
#include <stdint.h>

//NOTE(stanisz): requires stdio.h to be included before this line
#include "utils.c"

int main(i32 argc, char** argv)
{
	LOG_INT(argc);	
	LOG_POINTER(argv);
	LOG_FLOAT(0.213);
	LOG_STRING("ASDADS");
	LOG_HERE;

	return 0;
}	
