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

//NOTE(stanisz): from https://www.cs.rit.edu/~ncs/color/t_convert.html.
struct Color hsb_to_rgb(real32 h, real32 s, real32 b)
{
	int i;
	float f, p, q, t;
	float red, green, blue;
	struct Color result;

	if( s == 0 ) {
		// achromatic (grey)
		
		result.red = b;
		result.green  = b;
		result.blue = b;

		return result;
	}

	h /= 60;			// sector 0 to 5
	i = floor( h );
	f = h - i;			// factorial part of h
	p = b * ( 1 - s );
	q = b * ( 1 - s * f );
	t = b * ( 1 - s * ( 1 - f ) );

	switch( i ) {
		case 0:
			red = b;
			green = t;
			blue = p;
			break;
		case 1:
			red = q;
			green = b;
			blue = p;
			break;
		case 2:
			red = p;
			green = b;
			blue = t;
			break;
		case 3:
			red = p;
			green = q;
			blue = b;
			break;
		case 4:
			red = t;
			green = p;
			blue = b;
			break;
		default:		// case 5:
			red = b;
			green = p;
			blue = q;
			break;
	}

	result.red = (u8)(red * 255.0f);
	result.green = (u8)(green * 255.0f);
	result.blue = (u8)(blue * 255.0f);

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

u32 get_pixel_color(u32 x, u32 y, u32 width, u32 height)
{
	u32 color = 0;

	real32 radius = 1.5f;
	real32 radius_squared = radius * radius;

	real32 z_real_scaled = lerp(-radius, radius, (real32)x / width);
	real32 z_imaginary_scaled = lerp(-radius, radius, (real32)y / height);

	real32 z_real_squared = z_real_scaled * z_real_scaled;
	real32 z_imaginary_squared = z_imaginary_scaled * z_imaginary_scaled;

	u32 i = 0;
	u32 iteration_limit = 1000;

	real32 smooth_color = 0;

	while (z_real_squared + z_imaginary_squared < radius_squared &&
			i < iteration_limit)
	{
		real32 temp = z_real_squared - z_imaginary_squared;

		z_imaginary_scaled = 2.0f * z_real_scaled * z_imaginary_scaled + C_IMAGINARY;
		z_real_scaled = temp + C_REAL;

		z_real_squared = z_real_scaled * z_real_scaled;
		z_imaginary_squared = z_imaginary_scaled * z_imaginary_scaled;

		//NOTE(stanisz): This computations are used in determining the smooth
		// value of a color of a given pixel
		real32 length = sqrt(z_real_squared + z_imaginary_squared);
		smooth_color += exp(-length);

		++i;
	}
	
	//NOTE(stanisz): after this line, smooth_color is in the interval [0, 1].
	// (From stackoverflow, but tested so its fine.) 
	smooth_color /= iteration_limit;

	real32 value = 10.0f * smooth_color; 
	struct Color hsb_color = hsb_to_rgb(value, value + 0.2, value);

	u32 red = hsb_color.red;
	u32 blue = hsb_color.blue;
	u32 green = hsb_color.green;

	//NOTE(stanisz): color packing is currently linux-specific.	
	u32 red_bits_to_shift = 16;
	u32 green_bits_to_shift = 8;
	u32 blue_bits_to_shift = 0;

	color |= (red << red_bits_to_shift);
	color |= (green << green_bits_to_shift);
	color |= (blue << blue_bits_to_shift);

	return color; 
}

//NOTE(stanisz): this function performs thread-safe addition of
// an 'addend' to the 'v'. 
// This funciton is currently linux-specific.
u32 interlocked_add(volatile u32* v, u32 addend)
{
	return __sync_fetch_and_add(v, addend);	
}

//NOTE(stanisz): The function below renders a horizontal strip of the image.
// this is done in a way, so that the working threads will not interfere
// with each other when accessing the data - any given pixel in the image
// can be accessed by only one thread (belongs to exactly one strip).
//
// This function returns zero if there is not anything to render.
// Nonzero (1) otherwise.
u8 render_strip(struct WorkQueue* work_queue)
{
	//NOTE(stanisz): interlocked add, return previous.
	u32 current_work_order = interlocked_add(&work_queue->next_work_order, 1);

	if (current_work_order >= work_queue->work_order_count)
	{
		//NOTE(stanisz): the work is finished as there are no more
		// strips to render.
		return 0;
	}
	struct WorkOrder current = work_queue->work_orders[current_work_order];

	for (u32 y = current.y_start; y <= current.y_end; ++y)
	{
		for (u32 x = 0; x < work_queue->width; ++x)
		{
			u32 array_index = y * work_queue->width + x;

			assert(array_index < work_queue->width * work_queue->height);

			work_queue->pixels[array_index] = get_pixel_color(x, y, 
					work_queue->width, work_queue->height);
		}
	}

	return 1;
}

void worker_function(struct WorkQueue* work_queue)
{
	//NOTE(stanisz): look for a job to do, if none - break.
	while (render_strip(work_queue))
	{
	}

	//TODO(stanisz): what does it really do? does it join?
	pthread_exit(0);
}

//NOTE(stanisz): this function is currently linux-specific.
// It is responsible for creating a worker thread.
void create_worker_thread(struct WorkQueue* work_queue, pthread_t *thread_ids, u32 i)
{
	pthread_create(&thread_ids[i], 0, (void *)&worker_function, work_queue);
}

int main(i32 argc, char** argv)
{
	//TODO(stanisz): support argument passing - number of threads to use?
	UNUSED(argc);
	UNUSED(argv);

	u32 width = 1600 * 2;
	u32 height = 900 * 2;
	u32 *pixels = (u32*)malloc(sizeof(u32) * width * height);

	struct WorkQueue work_queue = {};
	//NOTE(stanisz): in pixels.
	const u32 strip_size = height / 100;
	work_queue.pixels = pixels;
	work_queue.width = width;
	work_queue.height = height;
	work_queue.work_orders = (struct WorkOrder*)malloc(sizeof(struct WorkOrder) * \
			ceil((float)height / strip_size));

		
	//NOTE(stanisz): filling the 'work_queue'.
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
		
	//NOTE(stanisz): these values produce nice images (found on the web).
	C_REAL = 0.37+cos(RAND_SEED*1.23462673423)*0.04;
	C_IMAGINARY = sin(RAND_SEED*1.43472384234)*0.10+0.50;

	//NOTE(stanisz): number of additional threads to spawn 
	// (apart from the initial one).
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
