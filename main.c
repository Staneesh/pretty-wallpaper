#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

//NOTE(stanisz): requires stdio.h to be included before this line
#include "utils.c"

struct WorkOrder
{
	u32 y_start;
	u32 y_end;
};

struct WorkQueue
{
	u32 work_order_count;
	u32 next_work_order;

	struct WorkOrder* work_orders;
};

int main(i32 argc, char** argv)
{
	UNUSED(argc);
	UNUSED(argv);

	u32 width = 900;
	u32 height = 600;
	u32 *pixels = (u32*)malloc(sizeof(u32) * width * height);

	struct WorkQueue work_queue = {};
	//NOTE(stanisz): in pixels.
	const u32 strip_size = 20;
	work_queue.work_orders = (struct WorkOrder*)malloc(sizeof(struct WorkOrder) * \
			ceil((float)height / strip_size));

		
	for (u32 y = 0; ; y+= strip_size)
	{
		struct WorkOrder new_order = {};
		++work_queue.work_order_count;

		u32 work_index = y / strip_size;
	
		if (y + strip_size >= height - 1)
		{
			new_order.y_start = y;
			new_order.y_end = height - 1;
			work_queue.work_orders[work_index] = new_order;

			break;
		}
		else
		{
			new_order.y_start = y;	
			new_order.y_end = y + strip_size - 1;
			work_queue.work_orders[work_index] = new_order;
		}	
	}

	for (u32 i = 0; i < work_queue.work_order_count; ++i)
	{
		LOG_UINT(work_queue.work_orders[i].y_start);
	}
	writeImage(width, height, pixels, "paper.bmp");

	free(pixels);
	free(work_queue.work_orders);
	return 0;
}	
