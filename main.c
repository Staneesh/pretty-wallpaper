#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>
#include <time.h>
#include <xmmintrin.h>
#include <immintrin.h>

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

	struct WorkOrder* work_orders;
	
	real32 c_real;
	real32 c_imaginary;
	u32 rand_state; 
	u32 _pad; 
};

struct Color
{
	u8 red;
	u8 green;
	u8 blue;
};

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

real32 random_zero_one(u32* v)
{
	u32 x = *v;
	
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;

	*v = x;

	return (real32)x / UINT_MAX;
}

struct Color random_color(u32* xorshift_state)
{
	struct Color result = {};

	result.red = random_zero_one(xorshift_state) * 255.0f;
	result.green = random_zero_one(xorshift_state) * 255.0f;
	result.blue = random_zero_one(xorshift_state) * 255.0f;

	return result;
}

//NOTE(stanisz): this is a function computing wide e^x
// (from the web)
__m128 exp_ps(__m128 x) {
	__m128 _ps_exp_hi = _mm_set_ps1(88.3762626647949f);
	__m128 _ps_exp_lo = _mm_set1_ps(-88.3762626647949f);
	__m128 _ps_cephes_LOG2EF = _mm_set1_ps(1.44269504088896341f);
	__m128 _ps_cephes_exp_C1 = _mm_set1_ps(0.693359375f);
	__m128 _ps_cephes_exp_C2 = _mm_set1_ps(-2.12194440e-4f);
	__m128 _ps_cephes_exp_p0 = _mm_set1_ps(1.9875691500E-4f);
	__m128 _ps_cephes_exp_p1 = _mm_set1_ps(1.3981999507E-3f);
	__m128 _ps_cephes_exp_p2 = _mm_set1_ps(8.3334519073E-3f);
	__m128 _ps_cephes_exp_p3 = _mm_set1_ps(4.1665795894E-2f);
	__m128 _ps_cephes_exp_p4 = _mm_set1_ps(1.6666665459E-1f);
	__m128 _ps_cephes_exp_p5 = _mm_set1_ps(5.0000001201E-1f);

	__m128 one = _mm_set_ps1(1.0f);
	__m128 _ps_0p5 = _mm_set_ps1(0.5f);
	__m128i _pi32_0x7f = _mm_set_epi32(0x7f, 0x7f, 0x7f, 0x7f);

	x = _mm_min_ps(x, _ps_exp_hi);
	x = _mm_max_ps(x, _ps_exp_lo);

	/* express exp(x) as exp(g + n*log(2)) */
	__m128 fx = _mm_mul_ps(x, _ps_cephes_LOG2EF);
	fx = _mm_add_ps(fx, _ps_0p5);

	/* how to perform a floorf with SSE: just below */
	__m128i emm0 = _mm_cvttps_epi32(fx);
	__m128 tmp  = _mm_cvtepi32_ps(emm0);
	/* if greater, substract 1 */
	__m128 mask = _mm_cmpgt_ps(tmp, fx);
	mask = _mm_and_ps(mask, one);
	fx = _mm_sub_ps(tmp, mask);
	tmp = _mm_mul_ps(fx, _ps_cephes_exp_C1);
	__m128 z = _mm_mul_ps(fx, _ps_cephes_exp_C2);
	x = _mm_sub_ps(x, _mm_add_ps(tmp, z));
	z = _mm_mul_ps(x,x);
	__m128 y = _ps_cephes_exp_p0;
	y = _mm_mul_ps(y, x);
	y = _mm_add_ps(y, _ps_cephes_exp_p1);
	y = _mm_mul_ps(y, x);
	y = _mm_add_ps(y, _ps_cephes_exp_p2);
	y = _mm_mul_ps(y, x);
	y = _mm_add_ps(y, _ps_cephes_exp_p3);
	y = _mm_mul_ps(y, x);
	y = _mm_add_ps(y, _ps_cephes_exp_p4);
	y = _mm_mul_ps(y, x);
	y = _mm_add_ps(y, _ps_cephes_exp_p5);
	y = _mm_mul_ps(y, z);
	y = _mm_add_ps(y, _mm_add_ps(x, one));

	/* build 2^n */
	emm0 = _mm_cvttps_epi32(fx);
	emm0 = _mm_add_epi32(emm0, _pi32_0x7f);
	emm0 = _mm_slli_epi32(emm0, 23);
	__m128 pow2n = _mm_castsi128_ps(emm0);
	y = _mm_mul_ps(y, pow2n);

	return y;
}

__m128 lerp_m128(const __m128* a, const __m128* b, const __m128* t)
{
	__m128 one_minus_t = _mm_sub_ps(_mm_set_ps1(1.0f), *t);
	return _mm_add_ps(_mm_mul_ps(one_minus_t, *a),
			_mm_mul_ps(*t, *b));
}

__m128 negative_m128(const __m128* a)
{
	return _mm_mul_ps(*a, _mm_set_ps1(-1.0f));
}

//NOTE(stanisz): calculates pixels of coords: 
// (x, y), (x+1, y), (x+2, y), (x+3, y).
void get_pixel_colors_wide(u32 x, u32 y, u32 width, u32 height,
		real32 c_real, real32 c_imaginary, struct Color colors[4])
{
	__m128 c_imaginary_wide = _mm_set_ps1(c_imaginary);
	__m128 c_real_wide = _mm_set_ps1(c_real);

	__m128 radius = _mm_set_ps1(1.5f);
	__m128 negative_radius = negative_m128(&radius);
	__m128 radius_squared = _mm_mul_ps(radius, radius);
	
	__m128 x_wide = _mm_set_ps((float)x, (float)x+1, (float)x+2, (float)x+3);
	__m128 y_wide = _mm_set_ps1((float)y);
	__m128 tx = _mm_div_ps(x_wide, _mm_set_ps1(width));
	__m128 ty = _mm_div_ps(y_wide, _mm_set_ps1(height));

	__m128 z_real_scaled = lerp_m128(&negative_radius, &radius, &tx);
	__m128 z_imaginary_scaled = lerp_m128(&negative_radius, &radius, &ty);

	__m128 z_real_squared = _mm_mul_ps(z_real_scaled, z_real_scaled);
	__m128 z_imaginary_squared = _mm_mul_ps(z_imaginary_scaled, z_imaginary_scaled);

	__m128 real_sq_plus_im_sq = _mm_add_ps(z_real_squared, z_imaginary_squared);

	__m128 i = _mm_set_ps1(0);

	__m128 smooth_color = _mm_set_ps1(0.0f);

	u32 iter_limit = 10000;

	for(u32 _i = 0; _i < iter_limit; ++_i)
	{
		__m128 temp = _mm_sub_ps(z_real_squared, z_imaginary_squared);
		__m128 z_re_z_im = _mm_mul_ps(z_real_scaled, z_imaginary_scaled);
		__m128 z_re_z_im_times_2 = _mm_mul_ps(_mm_set_ps1(2.0f), z_re_z_im);

		z_imaginary_scaled = _mm_add_ps(z_re_z_im_times_2, c_imaginary_wide);
		z_real_scaled = _mm_add_ps(temp, c_real_wide);

		z_real_squared = _mm_mul_ps(z_real_scaled, z_real_scaled);
		z_imaginary_squared = _mm_mul_ps(z_imaginary_scaled, z_imaginary_scaled);
		real_sq_plus_im_sq = _mm_add_ps(z_real_squared, z_imaginary_squared);

		//NOTE(stanisz): This computations are used in determining the smooth
		// value of a color of a given pixel
		__m128 length = _mm_sqrt_ps(real_sq_plus_im_sq);
		__m128 comparison = _mm_cmple_ps(real_sq_plus_im_sq, radius_squared);
		__m128 lanes_increment = _mm_and_ps(comparison, _mm_set_ps1(1));

		__m128 exp_term = exp_ps(negative_m128(&length));
		smooth_color = _mm_add_ps(smooth_color, 
				_mm_mul_ps(exp_term, lanes_increment));


		i = _mm_add_ps(i, lanes_increment);
		u32 mask = _mm_movemask_ps(comparison);
		if (mask == 0)
		{
			break;
		}
	}
	
	//NOTE(stanisz): after this line, smooth_color is in the interval [0, 1].
	// (From stackoverflow, but tested so its fine.) 
	smooth_color = _mm_div_ps(smooth_color, _mm_set_ps1(iter_limit));
	__m128 value = _mm_mul_ps(_mm_set_ps1(100.0f), smooth_color); 
	
	for (i32 i_temp = 3; i_temp >= 0; --i_temp)
	{
		struct Color rgb_color = hsb_to_rgb(value[i_temp], 
				value[i_temp]*3 + 0.01,
				1.0f - value[i_temp]);
		colors[3 - i_temp].red = rgb_color.red;
		colors[3 - i_temp].blue = rgb_color.blue;
		colors[3 - i_temp].green = rgb_color.green;
	}
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
		for (u32 x = 0; x < work_queue->width;)
		{
			u32 array_index = y * work_queue->width + x;

			assert(array_index < work_queue->width * work_queue->height);

			struct Color colors[4] = {};
			get_pixel_colors_wide(x, y, work_queue->width, work_queue->height,
					work_queue->c_real, work_queue->c_imaginary, colors);
			
			for (u32 i = array_index; i < array_index + 4; i++)
			{
				u32 color = 0;

				u32 red = colors[i - array_index].red;
				u32 blue = colors[i - array_index].blue;
				u32 green = colors[i - array_index].green;

				//NOTE(stanisz): color packing is currently linux-specific.	
				u32 red_bits_to_shift = 16;
				u32 green_bits_to_shift = 8;
				u32 blue_bits_to_shift = 0;

				color |= (red << red_bits_to_shift);
				color |= (green << green_bits_to_shift);
				color |= (blue << blue_bits_to_shift);
			
				work_queue->pixels[i] = color;
			}

			x+= 4;
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

	work_queue.rand_state = (time(0) * 997)%100000009;
		
	//NOTE(stanisz): these values produce nice images (found on the web).
	work_queue.c_real = 0.37+cos(work_queue.rand_state*1.23462673423)*0.04;
	work_queue.c_imaginary= sin(work_queue.rand_state*1.43472384234)*0.10+0.50;

	//NOTE(stanisz): number of additional threads to spawn 
	// (apart from the initial one).
	u32 num_threads = 8;
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
