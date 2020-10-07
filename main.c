#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

//NOTE(stanisz): requires stdio.h to be included before this line
#include "utils.c"

int main(i32 argc, char** argv)
{
	UNUSED(argc);
	UNUSED(argv);

	u32 width = 900;
	u32 height = 600;
	u32 *pixels = (u32*) malloc(sizeof(u32) * width * height);

	writeImage(width, height, pixels, "paper.bmp");

	free(pixels);
	return 0;
}	
