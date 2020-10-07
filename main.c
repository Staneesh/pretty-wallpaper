#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

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

float lerp(float a, float b, float t)
{
	assert(t >= 0.0f && t <= 1.0f);
		
	return (1.0f - t) * a + b;
}

u32 get_pixel_color(u32 x, u32 y, u32 width, u32 height)
{
	float x_interp = lerp(0.0, 255.0, (float)x / width);
	float y_interp = lerp(0.0, 255.0, (float)y / height);
	u32 color = 0;

	u32 red = 0;
	u32 green = (u32)x_interp;
	u32 blue = (u32)y_interp;

	color |= (red);
	color |= (green << 2);
	color |= (blue << 4);

	return color; 
}

void render_strip(struct WorkQueue* work_queue, u32* pixels, u32 width, u32 height)
{
	//interlocked add, return previous.
	struct WorkOrder current = work_queue->work_orders[work_queue->next_work_order++];

	for (u32 y = current.y_start; y <= current.y_end; ++y)
	{
		for (u32 x = 0; x < width; ++x)
		{
			u32 array_index = y * width + x;

			pixels[array_index] = get_pixel_color(x, y, width, height);
		}
	}
}

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
