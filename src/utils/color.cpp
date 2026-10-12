/*
 * The copyright in this software is being made available under the 2-clauses 
 * BSD License, included below. This software may be subject to other third 
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux 
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "openjpeg.h"
#include "color.h"


/*--------------------------------------------------------
Matrix for sYCC, Amendment 1 to IEC 61966-2-1

Y :   0.299   0.587    0.114   :R
Cb:  -0.1687 -0.3312   0.5     :G
Cr:   0.5    -0.4187  -0.0812  :B

Inverse:

R: 1        -3.68213e-05    1.40199      :Y
G: 1.00003  -0.344125      -0.714128     :Cb - 2^(prec - 1)
B: 0.999823  1.77204       -8.04142e-06  :Cr - 2^(prec - 1)

-----------------------------------------------------------*/
static void sycc_to_rgb(int offset, int upb, int y, int cb, int cr,
	int *out_r, int *out_g, int *out_b)
{
	int r, g, b;

	cb -= offset; cr -= offset;
	r = y + (int)(1.402 * (float)cr);
	if(r < 0) r = 0; else if(r > upb) r = upb; *out_r = r;

	g = y - (int)(0.344 * (float)cb + 0.714 * (float)cr);
	if(g < 0) g = 0; else if(g > upb) g = upb; *out_g = g;

	b = y + (int)(1.772 * (float)cb);
	if(b < 0) b = 0; else if(b > upb) b = upb; *out_b = b;
}

static void sycc444_to_rgb(opj_image_t *img)
{
	int *d0, *d1, *d2, *r, *g, *b;
	const int *y, *cb, *cr;
	unsigned int maxw, maxh, max, i;
	int offset, upb;

	upb = (int)img->comps[0].prec;
	offset = 1<<(upb - 1); upb = (1<<upb)-1;

	maxw = (unsigned int)img->comps[0].w; maxh = (unsigned int)img->comps[0].h;
	max = maxw * maxh;

	y = img->comps[0].data;
	cb = img->comps[1].data;
	cr = img->comps[2].data;

	d0 = r = (int*)malloc(sizeof(int) * (size_t)max);
	d1 = g = (int*)malloc(sizeof(int) * (size_t)max);
	d2 = b = (int*)malloc(sizeof(int) * (size_t)max);

	for(i = 0U; i < max; ++i)
	{
		sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
		++y; ++cb; ++cr; ++r; ++g; ++b;
	}
	free(img->comps[0].data); img->comps[0].data = d0;
	free(img->comps[1].data); img->comps[1].data = d1;
	free(img->comps[2].data); img->comps[2].data = d2;

}/* sycc444_to_rgb() */

static void sycc422_to_rgb(opj_image_t *img)
{	
	int *d0, *d1, *d2, *r, *g, *b;
	const int *y, *cb, *cr;
	unsigned int maxw, maxh, max;
	int offset, upb;
	unsigned int i, j;

	upb = (int)img->comps[0].prec;
	offset = 1<<(upb - 1); upb = (1<<upb)-1;

	maxw = (unsigned int)img->comps[0].w; maxh = (unsigned int)img->comps[0].h;
	max = maxw * maxh;

	y = img->comps[0].data;
	cb = img->comps[1].data;
	cr = img->comps[2].data;

	d0 = r = (int*)malloc(sizeof(int) * (size_t)max);
	d1 = g = (int*)malloc(sizeof(int) * (size_t)max);
	d2 = b = (int*)malloc(sizeof(int) * (size_t)max);

	for(i=0U; i < maxh; ++i)
	{
		for(j=0U; j < (maxw & ~(unsigned int)1U); j += 2U)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b; ++cb; ++cr;
		}
		if (j < maxw) {
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b; ++cb; ++cr;
		}
	}
	free(img->comps[0].data); img->comps[0].data = d0;
	free(img->comps[1].data); img->comps[1].data = d1;
	free(img->comps[2].data); img->comps[2].data = d2;

#if defined(USE_JPWL) || defined(USE_MJ2)
	img->comps[1].w = maxw; img->comps[1].h = maxh;
	img->comps[2].w = maxw; img->comps[2].h = maxh;
#else
	img->comps[1].w = (OPJ_UINT32)maxw; img->comps[1].h = (OPJ_UINT32)maxh;
	img->comps[2].w = (OPJ_UINT32)maxw; img->comps[2].h = (OPJ_UINT32)maxh;
#endif
	img->comps[1].dx = img->comps[0].dx;
	img->comps[2].dx = img->comps[0].dx;
	img->comps[1].dy = img->comps[0].dy;
	img->comps[2].dy = img->comps[0].dy;

}/* sycc422_to_rgb() */

static void sycc420_to_rgb(opj_image_t *img)
{
	int *d0, *d1, *d2, *r, *g, *b, *nr, *ng, *nb;
	const int *y, *cb, *cr, *ny;
	unsigned int maxw, maxh, max;
	int offset, upb;
	unsigned int i, j;

	upb = (int)img->comps[0].prec;
	offset = 1<<(upb - 1); upb = (1<<upb)-1;

	maxw = (unsigned int)img->comps[0].w; maxh = (unsigned int)img->comps[0].h;
	max = maxw * maxh;

	y = img->comps[0].data;
	cb = img->comps[1].data;
	cr = img->comps[2].data;

	d0 = r = (int*)malloc(sizeof(int) * (size_t)max);
	d1 = g = (int*)malloc(sizeof(int) * (size_t)max);
	d2 = b = (int*)malloc(sizeof(int) * (size_t)max);

	for(i=0U; i < (maxh & ~(unsigned int)1U); i += 2U)
	{
		ny = y + maxw;
		nr = r + maxw; ng = g + maxw; nb = b + maxw;

		for(j=0; j < (maxw & ~(unsigned int)1U); j += 2U)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;

			sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
			++ny; ++nr; ++ng; ++nb;
			sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
			++ny; ++nr; ++ng; ++nb; ++cb; ++cr;
		}
		if(j < maxw)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
			++y; ++r; ++g; ++b;

			sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
			++ny; ++nr; ++ng; ++nb; ++cb; ++cr;
		}
		y += maxw; r += maxw; g += maxw; b += maxw;
	}
	if(i < maxh)
	{
		for(j=0U; j < (maxw & ~(unsigned int)1U); j += 2U)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);

			++y; ++r; ++g; ++b;

			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);

			++y; ++r; ++g; ++b; ++cb; ++cr;
		}
		if(j < maxw)
		{
			sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
		}
	}

	free(img->comps[0].data); img->comps[0].data = d0;
	free(img->comps[1].data); img->comps[1].data = d1;
	free(img->comps[2].data); img->comps[2].data = d2;

#if defined(USE_JPWL) || defined(USE_MJ2)
	img->comps[1].w = maxw; img->comps[1].h = maxh;
	img->comps[2].w = maxw; img->comps[2].h = maxh;
#else
	img->comps[1].w = (OPJ_UINT32)maxw; img->comps[1].h = (OPJ_UINT32)maxh;
	img->comps[2].w = (OPJ_UINT32)maxw; img->comps[2].h = (OPJ_UINT32)maxh;
#endif
	img->comps[1].dx = img->comps[0].dx;
	img->comps[2].dx = img->comps[0].dx;
	img->comps[1].dy = img->comps[0].dy;
	img->comps[2].dy = img->comps[0].dy;

}/* sycc420_to_rgb() */

void color_sycc_to_rgb(opj_image_t *img)
{
	if(img->numcomps < 3)
	{
		img->color_space = OPJ_CLRSPC_GRAY;
		return;
	}

	if((img->comps[0].dx == 1)
	&& (img->comps[1].dx == 2)
	&& (img->comps[2].dx == 2)
	&& (img->comps[0].dy == 1)
	&& (img->comps[1].dy == 2)
	&& (img->comps[2].dy == 2))/* horizontal and vertical sub-sample */
  {
		sycc420_to_rgb(img);
  }
	else
	if((img->comps[0].dx == 1)
	&& (img->comps[1].dx == 2)
	&& (img->comps[2].dx == 2)
	&& (img->comps[0].dy == 1)
	&& (img->comps[1].dy == 1)
	&& (img->comps[2].dy == 1))/* horizontal sub-sample only */
  {
		sycc422_to_rgb(img);
  }
	else
	if((img->comps[0].dx == 1)
	&& (img->comps[1].dx == 1)
	&& (img->comps[2].dx == 1)
	&& (img->comps[0].dy == 1)
	&& (img->comps[1].dy == 1)
	&& (img->comps[2].dy == 1))/* no sub-sample */
  {
		sycc444_to_rgb(img);
  }
	else
  {
		fprintf(stderr,"%s:%d:color_sycc_to_rgb\n\tCAN NOT CONVERT\n", __FILE__,__LINE__);
		return;
  }
	img->color_space = OPJ_CLRSPC_SRGB;

}/* color_sycc_to_rgb() */

/*
 * This code has been adopted from sjpx_openjpeg.c of ghostscript
 */
void color_esycc_to_rgb(opj_image_t *image)
{
	int y, cb, cr, sign1, sign2, val;
	unsigned int w, h, max, i;
	int flip_value = (1 << (image->comps[0].prec-1));
	int max_value = (1 << image->comps[0].prec) - 1;
	
	if(image->numcomps < 3) return;
	
	w = image->comps[0].w;
	h = image->comps[0].h;
	
	sign1 = (int)image->comps[1].sgnd;
	sign2 = (int)image->comps[2].sgnd;
	
	max = w * h;
	
	for(i = 0; i < max; ++i)
	{
		
		y = image->comps[0].data[i]; cb = image->comps[1].data[i]; cr = image->comps[2].data[i];
		
		if( !sign1) cb -= flip_value;
		if( !sign2) cr -= flip_value;
		
		val = (int)
		((float)y - (float)0.0000368 * (float)cb
		 + (float)1.40199 * (float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[0].data[i] = val;
		
		val = (int)
		((float)1.0003 * (float)y - (float)0.344125 * (float)cb
		 - (float)0.7141128 * (float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[1].data[i] = val;
		
		val = (int)
		((float)0.999823 * (float)y + (float)1.77204 * (float)cb
		 - (float)0.000008 *(float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[2].data[i] = val;
	}
	image->color_space = OPJ_CLRSPC_SRGB;

}/* color_esycc_to_rgb() */
