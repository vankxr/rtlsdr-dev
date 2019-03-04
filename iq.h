#ifndef __IQ_H__
#define __IQ_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define TAU (2 * INT16_MAX)

typedef struct iq16_t
{
	int16_t i;
	int16_t q;
} iq16_t;

#define INT16_TO_IQ16(x)	\
	((iq16_t) 				\
	{ 						\
		.i = x,				\
		.q = 0	 			\
	})
#define IQ16_CONJUGATE(x)	\
	((iq16_t) 				\
	{ 						\
		.i = x.i,			\
		.q = -x.q 			\
	})
#define IQ16_PRODUCT(x, y) 				\
	((iq16_t) 							\
	{ 									\
		.i = (x.i * y.i) - (x.q * y.q), \
		.q = (x.i * y.q) + (x.q * y.i), \
	})
#define IQ16_SCALAR_PRODUCT(x, y)	\
	((iq16_t) 						\
	{ 								\
		.i = x.i * y,				\
		.q = x.q * y				\
	})
#define IQ16_SCALAR_QUOTIENT(x, y)	\
	((iq16_t) 						\
	{ 								\
		.i = x.i / y,				\
		.q = x.q / y				\
	})

#endif