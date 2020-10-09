#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

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
	volatile u32 next_work_order;
	u32 *pixels;
	u32 width;
	u32 height;
	u32 __pad;

	struct WorkOrder* work_orders;
};

float lerp(float a, float b, float t)
{
	return (1.0f - t) * a + t * b;
}

u32 get_pixel_color(u32 x, u32 y, u32 width, u32 height)
{
	float x_interp = lerp(0.0, 255.0, (float)x / width);
	float y_interp = lerp(0.0, 255.0, (float)y / height);

	u32 color = 0;

	u32 red = 0;
	u32 green = (u32)x_interp;
	u32 blue = (u32)y_interp;

	u32 red_bits_to_shift = 16;
	u32 green_bits_to_shift = 8;
	u32 blue_bits_to_shift = 0;

	color |= (red << red_bits_to_shift);
	color |= (green << green_bits_to_shift);
	color |= (blue << blue_bits_to_shift);

	return color; 
}

u32 interlocked_add(volatile u32* v, u32 addend)
{
	return __sync_fetch_and_add(v, addend);	
}

u8 render_strip(struct WorkQueue* work_queue)
{
	//interlocked add, return previous.
	u32 current_work_order = interlocked_add(&work_queue->next_work_order, 1);
	//LOG_UINT(current_work_order);

	if (current_work_order >= work_queue->work_order_count)
	{
		//LOG_STRING("NOTHING TO DO");
		return 0;
	}
	struct WorkOrder current = work_queue->work_orders[current_work_order];

	for (u32 y = current.y_start; y <= current.y_end; ++y)
	{
		for (u32 x = 0; x < work_queue->width; ++x)
		{
			u32 array_index = y * work_queue->width + x;
			//LOG_UINT(array_index);
			assert(array_index < work_queue->width * work_queue->height);

			work_queue->pixels[array_index] = get_pixel_color(x, y, 
					work_queue->width, work_queue->height);
		}
	}
	return 1;
}

void worker_function(struct WorkQueue* work_queue)
{
	while (render_strip(work_queue))
	{
	}

	//NOTE(stanisz): what does it really do? does it join?
	pthread_exit(0);
}

void create_worker_thread(struct WorkQueue* work_queue, pthread_t *thread_ids, u32 i)
{
	pthread_create(&thread_ids[i], 0, (void *)&worker_function, work_queue);
}

int main(i32 argc, char** argv)
{
	UNUSED(argc);
	UNUSED(argv);

	u32 width = 2400;
	u32 height = 1400;
	u32 *pixels = (u32*)malloc(sizeof(u32) * width * height);

	struct WorkQueue work_queue = {};
	//NOTE(stanisz): in pixels.
	const u32 strip_size = 50;
	work_queue.pixels = pixels;
	work_queue.width = width;
	work_queue.height = height;
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

	u32 num_threads = 7;
	pthread_t thread_ids[100];

	//NOTE(stanisz): Fencing - making sure that thread 0 has
	// finished writing to the memory and that this memory is
	// now available for reading from the thread that are 
	// about to be created.
	interlocked_add((volatile u32*)&work_queue.next_work_order, 0);

	for (u32 i = 0; i < num_threads; ++i)
	{
		create_worker_thread(&work_queue, thread_ids, i);
	}

	for (u32 i = 0; i < num_threads; ++i)
	{
		pthread_join(thread_ids[i], 0);
	}
	writeImage(width, height, pixels, "paper.bmp");

	free(pixels);
	free(work_queue.work_orders);
	return 0;
}	
