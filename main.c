#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>
#include <time.h>

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

struct Color
{
	u8 red;
	u8 green;
	u8 blue;
};

#define PALETTE_SIZE 10000
struct Color palette[PALETTE_SIZE];
u32 RAND_SEED = 1000; 
real32 C_REAL = -0.4f;
real32 C_IMAGINARY = 0.6f;

float lerp(float a, float b, float t)
{
	return (1.0f - t) * a + t * b;
}

struct Color lerp_color(struct Color* a, struct Color* b, real32 t)
{
	struct Color result = {};

	result.red = (u8)lerp(a->red, b->red, t);
	result.green = (u8)lerp(a->green, b->green, t);
	result.blue = (u8)lerp(a->blue, b->blue, t);

	return result;
}


real32 random_zero_one()
{
	u32 x = RAND_SEED;
	
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;

	RAND_SEED = x;

	return (real32)x / UINT_MAX;
}

struct Color random_color()
{
	struct Color result = {};

	result.red = random_zero_one() * 255.0f;
	result.green = random_zero_one() * 255.0f;
	result.blue = random_zero_one() * 255.0f;

	return result;
}

void generate_pallete()
{
	for (u32 i = 0; i < PALETTE_SIZE; ++i)
	{
		real32 x_plot = (real32) i / PALETTE_SIZE;

		u32 blue_term_u32 = (u32)x_plot;
		if (blue_term_u32 > 255)
		{
			blue_term_u32 = 255;
		}

		struct Color this_color = {0, 0, (u8)blue_term_u32};

		palette[i] = this_color;	
	}
}

u32 get_pixel_color(u32 x, u32 y, u32 width, u32 height)
{
	u32 color = 0;

#if 0
	C_REAL = -0.4f;
	C_IMAGINARY = 0.6f;
#endif

	real32 radius = 1.5f;
	real32 radius_squared = radius * radius;

	real32 z_real_scaled = lerp(-radius, radius, (real32)x / width);
	real32 z_imaginary_scaled = lerp(-radius, radius, (real32)y / height);

	real32 z_real_squared = z_real_scaled * z_real_scaled;
	real32 z_imaginary_squared = z_imaginary_scaled * z_imaginary_scaled;

	u32 i = 0;
	u32 iteration_limit = 1000;

	while (z_real_squared + z_imaginary_squared < radius_squared &&
			i < iteration_limit)
	{
		real32 temp = z_real_squared - z_imaginary_squared;

		z_imaginary_scaled = 2.0f * z_real_scaled * z_imaginary_scaled + C_IMAGINARY;
		z_real_scaled = temp + C_REAL;

		z_real_squared = z_real_scaled * z_real_scaled;
		z_imaginary_squared = z_imaginary_scaled * z_imaginary_scaled;

		++i;
	}
#if 0
	real32 log_2 = 0.30102999566398114f;
	real32 color_intensity = 0;
	real32 log_r = log(radius);

	if (i < iteration_limit)
	{
		real32 log_zn = log(z_real_squared + z_imaginary_squared) / 2;
		real32 nu = log(log_zn / log_r) / log_r;

		color_intensity = (real32)i + 1 - nu;
	}

	struct Color color1 = palette[((u32)floor(color_intensity)) % PALETTE_SIZE];
	struct Color color2 = palette[((u32)floor(color_intensity) + 1) % PALETTE_SIZE];

	real32 fract_intensity = color_intensity - (i32)(floor)color_intensity;
	struct Color color_mix = lerp_color(&color1, &color2, fract_intensity);

	u32 red = (u32)color_mix.red;
	u32 green = (u32)color_mix.green;
	u32 blue = (u32)color_mix.blue;
#else
	struct Color color1 = {0, 0, 0};
	struct Color color2 = {0, 0, 255};

	real32 x_plot = (real32)i / iteration_limit;

	real32 si = (real32)i - log2(log2(z_real_squared + z_imaginary_squared)) + radius_squared;
	real32 contrib_zero_one = si / iteration_limit;
	struct Color color_mix = lerp_color(&color1, &color2, 255.0f * contrib_zero_one);

	u32 red = (u32)color_mix.red;
	u32 green = (u32)color_mix.green;
	u32 blue = (u32)color_mix.blue;

#endif
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

	RAND_SEED = (time(0) * 997)%100000009;
	generate_pallete();

#if 0
	C_REAL = (2.0f * random_zero_one() - 1.0f) * 2.0f;
	C_IMAGINARY = (2.0f * random_zero_one() - 1.0f) * 2.0f;
#endif

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
