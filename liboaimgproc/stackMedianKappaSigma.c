/*****************************************************************************
 *
 * stackMedianKappaSigma.c -- median kappa sigma stacking method
 *
 * Copyright 2019,2023 James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <oa_common.h>
#include <openastro/imgproc.h>

#if HAVE_MATH_H
#include <math.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif


static int	_cmpUint8 ( const void*, const void* );
static int	_cmpUint16 ( const void*, const void* );


int
oaStackMedianKappaSigma8 ( void** frameArray, unsigned int numFrames,
		void* target, unsigned int length, double kappa )
{
	uint8_t*	values;
	uint8_t**	frames = ( uint8_t** ) frameArray;
	uint8_t*	tgt = target;
  unsigned int i, j;
	double total, mean, sigma, delta, min, max;
	unsigned int finalMean, medianPos;
	uint8_t median;

	if (!( values = ( uint8_t* ) malloc ( numFrames ))) {
		return -1;
	}
	medianPos = numFrames >> 1;
	for ( i = 0; i < length; i++ ) {
		mean = 0;
		total = 0;
		for ( j = 0; j < numFrames; j++ ) {
			total += frames[j][i];
			values[j] = frames[j][i];
		}
		qsort ( values, numFrames, sizeof ( uint8_t ), _cmpUint8 );
		median = values[ medianPos ];
		mean = total / numFrames;
		sigma = 0;
		for ( j = 0; j < numFrames; j++ ) {
			delta = frames[j][i] - mean;
			sigma += delta * delta;
		}
		sigma /= ( numFrames - 1 );
		sigma = sqrt ( sigma );
		min = mean - ( kappa * sigma );
		max = mean + ( kappa * sigma );
		finalMean = 0;
		for ( j = 0; j < numFrames; j++ ) {
			if ( frames[j][i] >= min && frames[j][i] <= max ) {
				finalMean += frames[j][i];
			} else {
				finalMean += median;
			}
		}
		*tgt++ = finalMean / numFrames;
	}

	free (( void* ) values );
  return 0;
}


static int
_cmpUint8 ( const void* pa, const void* pb )
{
	uint8_t	a, b;

	a = *(( uint8_t* ) pa );
	b = *(( uint8_t* ) pb );
	return ( a - b );
}


int
oaStackMedianKappaSigma16LE ( void** frameArray, unsigned int numFrames,
		void* target, unsigned int length, double kappa )
{
	uint16_t*			values;
	uint8_t**			frames = ( uint8_t** ) frameArray;
	uint8_t*			tgt = target;
  unsigned int	i, j, v;
	double				total, mean, sigma, delta, min, max;
	unsigned int	finalMean, medianPos;
	uint16_t			median;

	if (!( values = ( uint16_t* ) malloc ( numFrames ))) {
		return -1;
	}
	medianPos = numFrames >> 1;
	for ( i = 0; i < length; i += 2 ) {
		mean = 0;
		total = 0;
		for ( j = 0; j < numFrames; j++ ) {
			v = frames[j][i] + ( frames[j][i+1] << 8 );
			total += v;
			values[j] = v;
		}
		qsort ( values, numFrames, sizeof ( uint16_t ), _cmpUint16 );
		median = values[ medianPos ];
		mean = total / numFrames;
		sigma = 0;
		for ( j = 0; j < numFrames; j++ ) {
			delta = ( frames[j][i] + ( frames[j][i+1] << 8 )) - mean;
			sigma += delta * delta;
		}
		sigma /= ( numFrames - 1 );
		sigma = sqrt ( sigma );
		min = mean - ( kappa * sigma );
		max = mean + ( kappa * sigma );
		finalMean = 0;
		for ( j = 0; j < numFrames; j++ ) {
			v = frames[j][i] + ( frames[j][i+1] << 8 );
			if ( v >= min && v <= max ) {
				finalMean += v;
			} else {
				finalMean += median;
			}
		}
		finalMean /= numFrames;
		*tgt++ = finalMean & 0xff;
		*tgt++ = finalMean >> 8;
	}

	free (( void* ) values );
  return 0;
}


int
oaStackMedianKappaSigma16BE ( void** frameArray, unsigned int numFrames,
		void* target, unsigned int length, double kappa )
{
	uint16_t*			values;
	uint8_t**			frames = ( uint8_t** ) frameArray;
	uint8_t*			tgt = target;
  unsigned int	i, j, v;
	double				total, mean, sigma, delta, min, max;
	unsigned int	finalMean, medianPos;
	uint16_t			median;

	if (!( values = ( uint16_t* ) malloc ( numFrames ))) {
		return -1;
	}
	medianPos = numFrames >> 1;
	for ( i = 0; i < length; i += 2 ) {
		mean = 0;
		total = 0;
		for ( j = 0; j < numFrames; j++ ) {
			v = frames[j][i+1] + ( frames[j][i] << 8 );
			total += v;
			values[j] = v;
		}
		qsort ( values, numFrames, sizeof ( uint16_t ), _cmpUint16 );
		median = values[ medianPos ];
		mean = total / numFrames;
		sigma = 0;
		for ( j = 0; j < numFrames; j++ ) {
			delta = ( frames[j][i+1] + ( frames[j][i] << 8 )) - mean;
			sigma += delta * delta;
		}
		sigma /= ( numFrames - 1 );
		sigma = sqrt ( sigma );
		min = mean - ( kappa * sigma );
		max = mean + ( kappa * sigma );
		finalMean = 0;
		for ( j = 0; j < numFrames; j++ ) {
			v = frames[j][i+1] + ( frames[j][i] << 8 );
			if ( v >= min && v <= max ) {
				finalMean += v;
			} else {
				finalMean += median;
			}
		}
		finalMean /= numFrames;
		*tgt++ = finalMean >> 8;
		*tgt++ = finalMean & 0xff;
	}

	free (( void* ) values );
  return 0;
}


static int
_cmpUint16 ( const void* pa, const void* pb )
{
	uint16_t	a, b;

// FIX ME -- handle byte order
	a = *(( uint16_t* ) pa );
	b = *(( uint16_t* ) pb );
	return ( a - b );
}
