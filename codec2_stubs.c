/*
 *  Symbols libcodec2 references that solver_minimal_1 does not export. A side
 *  module whose imports are not all resolved never instantiates at all -
 *  emscripten reports it by crashing inside its own reportUndefinedSymbols,
 *  which names nothing - so each has to be defined here.
 *
 *  The maths ones are the single-precision variants: the solver exports the
 *  double-precision functions but not their float counterparts, so each is
 *  routed through the double version. That is exactly what a libm without a
 *  dedicated float path does, and costs nothing in accuracy.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

float acosf(float x) { return (float)acos((double)x); }
float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
float cosf(float x) { return (float)cos((double)x); }
float sinf(float x) { return (float)sin((double)x); }
float expf(float x) { return (float)exp((double)x); }
float exp2f(float x) { return (float)exp2((double)x); }
float log10f(float x) { return (float)log10((double)x); }

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func)
{
	fprintf(stderr, "[Codec2Dec] assertion failed: %s at %s:%u in %s\n",
	        expr ? expr : "?", file ? file : "?", line, func ? func : "?");
	abort();
}
