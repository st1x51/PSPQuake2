#include <valarray>
#include <vector>
#include <malloc.h>
#include <reent.h>
#include <pspgu.h>
#include <pspkernel.h>

#include "gu_local.h"
#include "gu_dxt.h"
//#include "vram.hpp"
#include <vram.h>

//fast memcpy
#define FAST_MEMCPY

//full flush data cache
#define FULL_FLUSH


#ifdef FAST_MEMCPY
#include "../psp/pspdmac.h"
#endif

bool tex_scale_down = true;
int currenttexture = 0;

static qboolean gu_imagesinit = false;

static byte			 intensitytable[256];
static unsigned char gammatable[256];

cvar_t		*intensity;

#define	MAX_GLTEXTURES	1024
image_t	gltextures[MAX_GLTEXTURES];
int		gltextures_used[MAX_GLTEXTURES];
int			numgltextures;

void GL_Bind (int texture_index)
{
	// Binding the currently bound texture?
	if (currenttexture == texture_index)
	{
		// Don't bother.
		return;
	}

	// Remember the current texture.
	currenttexture = texture_index;

	// Which texture is it?
	const image_t& texture = gltextures[texture_index];

	// Set the texture mode.
	
	sceGuTexMode(texture.format, texture.mipmaps , 0, texture.swizzle);

	if (texture.mipmaps > 0 && gu_mipmaps->value > 0) 
	{
		sceGuTexSlope(0.4f);
		sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_LINEAR_MIPMAP_LINEAR);
		sceGuTexLevelMode(gu_mipmaps_func->value, gu_mipmaps_bias->value); // manual slope setting
	}
	else 	
	{
		sceGuTexFilter(texture.filter, texture.filter);
	}
	
 	// Set the texture image.
	const void* const texture_memory = texture.vram ? texture.vram : texture.ram;
	sceGuTexImage(0, texture.width, texture.height, texture.width, texture_memory);

	if (texture.mipmaps > 0 && gu_mipmaps->value > 0)
	{
		int size = (texture.width * texture.height * texture.bpp);
		int offset = size;
		int div = 2;

		for (int i = 1; i <= texture.mipmaps; i++)
		{
			void* const texture_memory2 = ((byte*) texture_memory)+offset;
            sceGuTexImage(i, texture.width/div, texture.height/div, texture.width/div, texture_memory2);
			offset += size/(div*div);
			div *=2;
		}
	}

}

/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================


typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void R_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}
*/
//=======================================================

void GL_ResampleTexture(const byte *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	const unsigned int fracstep = inwidth * 0x10000 / outwidth;
	for (int i = 0; i < outheight ; ++i, out += outwidth)
	{
		const byte*		inrow	= in + inwidth * (int)(i * inheight / outheight);
		unsigned int	frac	= fracstep >> 1;
		for (int j = 0; j < outwidth; ++j, frac += fracstep)
		{
			out[j] = inrow[frac >> 16];
		}
	}
}

/*
================
GL_ResampleTexture
================
*/
void GL_Resample32Texture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	byte		*pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i=0 ; i<outwidth ; i++)
	{
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for (i=0 ; i<outwidth ; i++)
	{
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight/outheight);

		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
      ((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
      ((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
    }
	}
}

static void swizzle_fast(u8* out, const u8* in, unsigned int width, unsigned int height)
{
	unsigned int blockx, blocky;
	unsigned int j;

	unsigned int width_blocks = (width / 16);
	unsigned int height_blocks = (height / 8);

	unsigned int src_pitch = (width-16)/4;
	unsigned int src_row = width * 8;

	const u8* ysrc = in;
	u32* dst = (u32*)out;

	for (blocky = 0; blocky < height_blocks; ++blocky)
	{
		const u8* xsrc = ysrc;
		for (blockx = 0; blockx < width_blocks; ++blockx)
		{
			const u32* src = (u32*)xsrc;
			for (j = 0; j < 8; ++j)
			{
				*(dst++) = *(src++);
				*(dst++) = *(src++);
				*(dst++) = *(src++);
				*(dst++) = *(src++);
				src += src_pitch;
			}
			xsrc += 16;
		}
		ysrc += src_row;
	}
}

/*
=======================================
TextureConvector
=======================================
From DE2 by Christoph Arnold "charnold"
modify by Crow_bar
=======================================
*/
void TextureConvector(unsigned char *in32, unsigned char *out16, int w, int h, int format)
{
	int texel;

	int size = w * h;

	for (texel = 0; texel < size; texel++)
	{
		if (format == GU_PSM_4444)
		{
			*(out16)    = (*in32>>4) & 0x0f; in32++; // r
			*(out16++) |= (*in32)    & 0xf0; in32++; // g
			*(out16)    = (*in32>>4) & 0x0f; in32++; // b
			*(out16++) |= (*in32)    & 0xf0; in32++; // a
		}
		else if (format == GU_PSM_5650)
		{
			unsigned char r,g,b;

			r = (*in32>>3) & 0x1f; in32++;	// r = 5 bit
			g = (*in32>>2) & 0x3f; in32++;	// g = 6 bit
			b = (*in32>>3) & 0x1f; in32++;	// b = 5 bit
								   in32++;	// a = 0 bit

			*(out16)	= r;				// alle   5 bits von r auf lower  5 bits von out16
			*(out16++) |= (g<<5) & 0xe0;	// lower  3 bits von g auf higher 3 bits von out16
			*(out16)	= (g>>3) & 0x07;	// higher 3 bits von g auf lower  3 bits von out16
			*(out16++) |= (b<<3) & 0xf8;    // alle   5 bits von b auf higher 5 bits von out16

		}
		else if (format == GU_PSM_5551)
		{
			unsigned char r,g,b,a;

			r = (*in32>>3) & 0x1f; in32++;	// r = 5 bit
			g = (*in32>>3) & 0x1f; in32++;	// g = 5 bit
			b = (*in32>>3) & 0x1f; in32++;	// b = 5 bit
			a = (*in32>>7) & 0x01; in32++;	// a = 1 bit

			*(out16)	= r;				// alle   5 bits von r auf lower  5 bits von out16
			*(out16++) |= (g<<5) & 0xe0;	// lower  3 bits von g auf higher 3 bits von out16
			*(out16)	= (g>>3) & 0x03;	// higher 2 bits von g auf lower  2 bits von out16
			*(out16)   |= (b<<2) & 0x7c;    // alle   5 bits von b auf bits 3-7      von out16
			*(out16++) |= (a<<7) & 0x80;    //        1 bit  von a auf bit    8      von out16
		}
	}
}

/*
================
GL_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/

void GL_LightScaleTexture (byte *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( only_gamma )
	{
		int		i, c;
		byte	*p;

		p = in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=4)
		{
			p[0] = gammatable[intensitytable[p[0]]];
			p[1] = gammatable[intensitytable[p[1]]];
			p[2] = gammatable[intensitytable[p[2]]];
		}
	}

}

void GL_Upload8(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for swizzling.
	std::size_t buffer_size = texture.width * texture.height;
	std::vector<byte> unswizzled(buffer_size);
 

	if (texture.mipmaps > 0) 
	{
		int size_incr = buffer_size/4;
		for (int i= 1;i <= texture.mipmaps;i++) 
		{
			buffer_size += size_incr;
			size_incr = size_incr/4;
		}
	}
	
	// Do we need to resize?
	if (texture.stretch_to_power_of_two)
	{
		// Resize.
		GL_ResampleTexture(data, width, height, &unswizzled[0], texture.width, texture.height);
	}
	else
	{
		// Straight copy.
		for (int y = 0; y < height; ++y)
		{
			const byte* const	src	= data + (y * width);
			byte* const			dst = &unswizzled[y * texture.width];
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(dst, width);
			    sceKernelDcacheWritebackInvalidateRange(src, width);
	        #else
                sceKernelDcacheWritebackRange(dst, width);
                sceKernelDcacheWritebackRange(src, width);
            #endif

		    sceDmacMemcpy(dst, src, width); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(dst, width);
            #else
			    sceKernelDcacheWritebackRange(dst, width);
            #endif
#else
			memcpy(dst, src, width);
#endif
		}
	}
	
	// Swizzle to system RAM.
	swizzle_fast(texture.ram, &unswizzled[0], texture.width, texture.height);

	if (texture.mipmaps > 0) 
	{
		int size = (texture.width * texture.height);
		int offset = size;
		int div = 2;

		for (int i = 1; i <= texture.mipmaps;i++) 
		{
			GL_ResampleTexture(data, width, height, &unswizzled[0], texture.width/div, texture.height/div);
			swizzle_fast(texture.ram+offset, &unswizzled[0], texture.width/div, texture.height/div);
			offset += size/(div*div);
			div *=2;
		}
	}
	
	unswizzled.clear();

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_size);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_size);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_size);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_size); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_size);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_size);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_size);
#endif
}

void GL_Upload32to16(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for swizzling.
	std::size_t buffer_sizesrc = texture.width * texture.height * 4;
	std::vector<byte> unswizzled(buffer_sizesrc);

	// Create a temporary buffer to use as a source for converting.
	std::size_t buffer_sizedst = texture.width * texture.height * 2;
	std::vector<byte> bpp(buffer_sizedst);

	if (texture.mipmaps > 0)
	{
		int size_incr = buffer_sizedst/4;
		for (int i= 1;i <= texture.mipmaps;i++)
		{
			buffer_sizedst += size_incr;
			size_incr = size_incr/4;
		}
	}

	// Do we need to resize?
	if (texture.stretch_to_power_of_two)
	{
		// Resize.
		GL_Resample32Texture((unsigned int*)data, width, height, (unsigned int*)&unswizzled[0], texture.width, texture.height);
	}
	else
	{
		// Straight copy.
		for (int y = 0; y < height; ++y)
		{
			const byte* const	src	= data + (y * width * 4);
			byte* const			dst = &unswizzled[y * texture.width * 4];
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(dst, width * 4);
			    sceKernelDcacheWritebackInvalidateRange(src, width * 4);
	        #else
                sceKernelDcacheWritebackRange(dst, width * 4);
                sceKernelDcacheWritebackRange(src, width * 4);
            #endif

		    sceDmacMemcpy(dst, src, width * 4); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(dst, width * 4);
            #else
			    sceKernelDcacheWritebackRange(dst, width * 4);
            #endif
#else
			memcpy(dst, src, width * 4);
#endif
		}
	}

    //GL_LightScaleTexture ((unsigned char*)&unswizzled[0], texture.width, texture.height, !texture.mipmaps);

    // 32 to 16.
	TextureConvector((unsigned char*)&unswizzled[0], (unsigned char*)&bpp[0], texture.width, texture.height, texture.format);

	// Swizzle to system RAM.
	swizzle_fast(texture.ram, &bpp[0], texture.width * 2, texture.height);

	if (texture.mipmaps > 0)
	{
		int size = (texture.width * texture.height * 2);
		int offset = size;
		int div = 2;

		for (int i = 1; i <= texture.mipmaps;i++)
		{
			GL_Resample32Texture((unsigned int*)data, width, height, (unsigned int*)&unswizzled[0], texture.width/div, texture.height/div);
			TextureConvector((unsigned char*)&unswizzled[0], (unsigned char*)&bpp[0], texture.width, texture.height, texture.format);
			swizzle_fast(texture.ram+offset, &bpp[0], (texture.width/div) * 2, texture.height/div);
			offset += size/(div*div);
			div *=2;
		}
	}

    unswizzled.clear();
	bpp.clear();

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_sizedst);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_sizedst);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_sizedst);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_sizedst);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_sizedst); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_sizedst);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_sizedst);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_sizedst);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_sizedst);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_sizedst);
#endif
}

void GL_Upload16(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for swizzling.
	const std::size_t buffer_size = texture.width * texture.height * 2;
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
			    sceKernelDcacheWritebackInvalidateRange(data, buffer_size);
	        #else
                sceKernelDcacheWritebackRange(texture.ram, buffer_size);
                sceKernelDcacheWritebackRange(data, buffer_size);
            #endif

		    sceDmacMemcpy((void *) texture.ram, (void *) data, buffer_size); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
            #else
			    sceKernelDcacheWritebackRange(texture.ram, buffer_size);
            #endif
#else
			memcpy((void *) texture.ram, (void *) data, buffer_size);
#endif


	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_size);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_size);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_size);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_size); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_size);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_size);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_size);
#endif
}

void GL_Upload32(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	//GL_LightScaleTexture ((byte*)data, width, height, !texture.mipmaps);
	// Create a temporary buffer to use as a source for swizzling.
	std::size_t buffer_size = texture.width * texture.height * 4;
	std::vector<byte> unswizzled(buffer_size);

	if (texture.mipmaps > 0)
	{
		int size_incr = buffer_size/4;
		for (int i= 1;i <= texture.mipmaps;i++)
		{
			buffer_size += size_incr;
			size_incr = size_incr/4;
		}
	}

	// Do we need to resize?
	if (texture.stretch_to_power_of_two)
	{
		// Resize.
		GL_Resample32Texture((unsigned int*)data, width, height, (unsigned int*)&unswizzled[0], texture.width, texture.height);
	}
	else
	{
		// Straight copy.
		for (int y = 0; y < height; ++y)
		{
			const byte* const	src	= data + (y * width * 4);
			byte* const			dst = &unswizzled[y * texture.width * 4];
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(dst, width * 4);
			    sceKernelDcacheWritebackInvalidateRange(src, width * 4);
	        #else
                sceKernelDcacheWritebackRange(dst, width * 4);
                sceKernelDcacheWritebackRange(src, width * 4);
            #endif

		    sceDmacMemcpy(dst, src, width * 4); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(dst, width * 4);
            #else
			    sceKernelDcacheWritebackRange(dst, width * 4);
            #endif
#else
			memcpy(dst, src, width * 4);
#endif
		}
	}

	// Swizzle to system RAM.
	swizzle_fast(texture.ram, &unswizzled[0], texture.width * 4, texture.height);

	if (texture.mipmaps > 0)
	{
		int size = (texture.width * texture.height * 4);
		int offset = size;
		int div = 2;

		for (int i = 1; i <= texture.mipmaps;i++)
		{
			GL_Resample32Texture((unsigned int*)data, width, height, (unsigned int*)&unswizzled[0], texture.width/div, texture.height/div);
			swizzle_fast(texture.ram+offset, &unswizzled[0], (texture.width/div) * 4, texture.height/div);
			offset += size/(div*div);
			div *=2;
		}
	}

	unswizzled.clear();

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_size);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_size);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_size);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_size); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_size);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_size);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_size);
#endif
}

void GL_UploadDXT(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	if(texture.bpp != 1 &&
	   texture.bpp != 4)
	{
       Sys_Error("GL_UploadDXT support only 1, 4 bit per pixel");
	}


	// Create a temporary buffer to use as a source for swizzling.
	std::size_t buffer_sizesrc = texture.width * texture.height * texture.bpp;
	std::vector<byte> unswizzled(buffer_sizesrc);

	std::size_t buffer_sizedst = NULL;
	switch(texture.format)
	{
     case GU_PSM_DXT3:
	 case GU_PSM_DXT5:
	// case GU_PSM_DXT3_EXT:
	// case GU_PSM_DXT5_EXT:
	   buffer_sizedst = texture.width * texture.height;
	   break;
	 case GU_PSM_DXT1:
	 //case GU_PSM_DXT1_EXT:
	   buffer_sizedst = texture.width * texture.height / 2;
       break;
	 default:
	   Sys_Error("GL_UploadDXT support only dxt textures format");
	}

	// Do we need to resize?
	if (texture.stretch_to_power_of_two)
	{
		// Resize.
		if(texture.bpp == 1)
		  GL_ResampleTexture(data, width, height, &unswizzled[0], texture.width, texture.height);
		else
          GL_Resample32Texture((unsigned int*)data, width, height, (unsigned int*)&unswizzled[0], texture.width, texture.height);
	}
	else
	{
		// Straight copy.
		for (int y = 0; y < height; ++y)
		{
			const byte* const	src	= data + (y * width * texture.bpp);
			byte* const			dst = &unswizzled[y * texture.width * texture.bpp];
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(dst, width * texture.bpp);
			    sceKernelDcacheWritebackInvalidateRange(src, width * texture.bpp);
	        #else
                sceKernelDcacheWritebackRange(dst, width * texture.bpp);
                sceKernelDcacheWritebackRange(src, width * texture.bpp);
            #endif

		    sceDmacMemcpy(dst, src, width * texture.bpp); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(dst, width * texture.bpp);
            #else
			    sceKernelDcacheWritebackRange(dst, width * texture.bpp);
            #endif
#else
			memcpy(dst, src, width * texture.bpp);
#endif
		}
	}
	tx_compress_dxtn(texture.bpp, texture.width, texture.height, (const unsigned char *)&unswizzled[0], texture.format, (unsigned char *)texture.ram);
	unswizzled.clear();

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_sizedst);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_sizedst);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_sizedst);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_sizedst);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_sizedst); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_sizedst);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_sizedst);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_sizedst);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_sizedst);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_sizedst);
#endif
}

void GL_Upload8_A(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for swizzling.
	const std::size_t buffer_size = texture.width * texture.height;
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
			    sceKernelDcacheWritebackInvalidateRange(data, buffer_size);
	        #else
                sceKernelDcacheWritebackRange(texture.ram, buffer_size);
                sceKernelDcacheWritebackRange(data, buffer_size);
            #endif

		    sceDmacMemcpy((void *) texture.ram, (void *) data, buffer_size); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
            #else
			    sceKernelDcacheWritebackRange(texture.ram, buffer_size);
            #endif
#else
			memcpy((void *) texture.ram, (void *) data, buffer_size);
#endif
	
	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_size);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_size);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_size);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_size); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_size);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_size);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_size);
#endif
}

void GL_Upload16_A(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for swizzling.
	const std::size_t buffer_size = texture.width * texture.height * 2;
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
			    sceKernelDcacheWritebackInvalidateRange(data, buffer_size);
	        #else
                sceKernelDcacheWritebackRange(texture.ram, buffer_size);
                sceKernelDcacheWritebackRange(data, buffer_size);
            #endif

		    sceDmacMemcpy((void *) texture.ram, (void *) data, buffer_size); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
            #else
			    sceKernelDcacheWritebackRange(texture.ram, buffer_size);
            #endif
#else
			memcpy((void *) texture.ram, (void *) data, buffer_size);
#endif

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_size);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_size);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_size);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_size); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_size);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_size);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_size);
#endif
}

void GL_Upload32to16_A(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for converting.
	std::size_t buffer_sizedst = texture.width * texture.height * 2;

    //GL_LightScaleTexture ((unsigned char*)data, texture.width, texture.height, !texture.mipmaps);

    // 32 to 16.
	TextureConvector((unsigned char*)data, (unsigned char*)texture.ram, texture.width, texture.height, texture.format);

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_sizedst);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_sizedst);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_sizedst);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_sizedst);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_sizedst); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_sizedst);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_sizedst);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_sizedst);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_sizedst);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_sizedst);
#endif
}

void GL_Upload32_A(int texture_index, const byte *data, int width, int height)
{
	if ((texture_index < 0) || (texture_index >= MAX_GLTEXTURES) || gltextures_used[texture_index] == 0)
	{
		Sys_Error("Invalid texture index %d", texture_index);
	}

	const image_t& texture = gltextures[texture_index];

	// Check that the texture matches.
	if ((width != texture.original_width) != (height != texture.original_height))
	{
		Sys_Error("Attempting to upload a texture which doesn't match the destination");
	}

	// Create a temporary buffer to use as a source for swizzling.
	const std::size_t buffer_size = texture.width * texture.height * 4;
#ifdef FAST_MEMCPY

		    #ifdef FULL_FLUSH
			    sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
			    sceKernelDcacheWritebackInvalidateRange(data, buffer_size);
	        #else
                sceKernelDcacheWritebackRange(texture.ram, buffer_size);
                sceKernelDcacheWritebackRange(data, buffer_size);
            #endif

		    sceDmacMemcpy((void *) texture.ram, (void *) data, buffer_size); //fast memcpy

            #ifdef FULL_FLUSH
	            sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
            #else
			    sceKernelDcacheWritebackRange(texture.ram, buffer_size);
            #endif
#else
			memcpy((void *) texture.ram, (void *) data, buffer_size);
#endif

	// Copy to VRAM?
	if (texture.vram)
	{
#ifdef FAST_MEMCPY
	#ifdef FULL_FLUSH
		    sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
		    sceKernelDcacheWritebackInvalidateRange(texture.ram,  buffer_size);
	#else
			sceKernelDcacheWritebackRange(texture.vram, buffer_size);
			sceKernelDcacheWritebackRange(texture.ram,  buffer_size);
	#endif

	    sceDmacMemcpy(texture.vram, texture.ram, buffer_size); //fast memcpy

#else
		memcpy(texture.vram, texture.ram, buffer_size);
#endif

	#ifdef FULL_FLUSH
		     sceKernelDcacheWritebackInvalidateRange(texture.vram, buffer_size);
	#else
		     sceKernelDcacheWritebackRange(texture.vram, buffer_size);
	#endif
	}

	// Flush the data cache.
#ifdef FULL_FLUSH
	sceKernelDcacheWritebackInvalidateRange(texture.ram, buffer_size);
#else
	sceKernelDcacheWritebackRange(texture.ram, buffer_size);
#endif
}

static std::size_t round_up(std::size_t size)
{
	static const float	denom	= 1.0f / logf(2.0f);
	const float			logged	= logf(size) * denom;
	const float			ceiling	= ceilf(logged);
	return 1 << static_cast<int>(ceiling);
}


static std::size_t round_down(std::size_t size)
{
	static const float	denom	= 1.0f / logf(2.0f);
	const float			logged	= logf(size) * denom;
	const float			floor	= floorf(logged);
	return 1 << static_cast<int>(floor);
}

int texturecount = 0;
void GL_UnloadTexture(int texture_index) 
{
	if (gltextures_used[texture_index])
	{
		image_t& texture = gltextures[texture_index];
		
		printf("Unloading: %s\n",texture.identifier);
		// Source.
		strcpy(texture.identifier,"");
		texture.original_width = 0;
		texture.original_height = 0;
		texture.stretch_to_power_of_two = false;
	 
		// Texture description.
		texture.format = GU_PSM_T8;
		texture.filter = GU_LINEAR;
		texture.width = 0;
		texture.height = 0;
		texture.mipmaps = 0;
	    texture.bpp = 1;
	    texture.swizzle = 0;
		
		texture.registration_sequence = 0;

		texture.type = it_wall;
		texture.texnum = 0;
	
		// Buffers.
		if (texture.ram != NULL)
		{
			free(texture.ram);
			texture.ram = NULL;
		}
		if (texture.vram != NULL) 
		{
			vfree(texture.vram);
			texture.vram = NULL;
		}		
	  texturecount--;
	  numgltextures--;
	}
	
	gltextures_used[texture_index] = 0;
}
image_t *GL_LoadPic (const char *identifier, int width, int height, const byte *data, qboolean stretch_to_power_of_two, int filter, int mipmap_level, imagetype_t type, int bits)
{
	int texture_index = -1;
  
	tex_scale_down = gu_tex_scale_down->value == true;
	
	// See if the texture is already present.
	if (identifier[0])
	{
		for (int i = 0; i < MAX_GLTEXTURES; ++i)
		{
			if (gltextures_used[i] == 1)
			{
				image_t& texture = gltextures[i];
				if (!strcmp (identifier, texture.identifier))
				{
					texture.texnum = i;
					texture.registration_sequence = registration_sequence;
					return &texture;
				}
			}	
		}
	}
 
 	// Out of textures?
	if (numgltextures == MAX_GLTEXTURES)
	{
		Sys_Error("Out of OpenGL textures");
	}

	// Use the next available texture.
	texturecount++;
	numgltextures++;
	texture_index = numgltextures;

	for (int i = 0; i < MAX_GLTEXTURES; ++i)
	{
		if (gltextures_used[i] == 0)
		{
			texture_index = i;
			break;
		}
	}	
	image_t& texture = gltextures[texture_index];
	gltextures_used[texture_index] = 1;
    texture.registration_sequence = registration_sequence;

	// Fill in the source data.
	strcpy(texture.identifier, identifier);
	texture.original_width			= width;
	texture.original_height			= height;
	texture.stretch_to_power_of_two	= stretch_to_power_of_two != false;

	// Fill in the texture description.
	if(bits == 32)
	{
	  texture.format			= GU_PSM_DXT1;
	  texture.bpp               = 4;
	  texture.swizzle           = 0;
	}
	else	
	{
	  texture.format			= GU_PSM_T8;
	  texture.bpp               = 1;
	  texture.swizzle           = 1;
	}
	
	texture.type        = type;
 
	if(type == it_pic)
	{
		texture.filter			= GU_NEAREST;
	}
	else
	    texture.filter			= filter;
	
	texture.mipmaps			= 0;
 
 	if (tex_scale_down == true && texture.stretch_to_power_of_two == true) 
	{
		texture.width			= std::max(round_down(width), 32U);
		texture.height			= std::max(round_down(height),32U);
	} 
	else 
	{
		texture.width			= std::max(round_up(width), 32U);
		texture.height			= std::max(round_up(height),32U);
	}

    for (int i=0; i <= mipmap_level;i++)
	{
		int div = (int) powf(2,i);
		if ((texture.width / div) > 16 && (texture.height / div) > 16 ) 
		{
			texture.mipmaps = i;
		}
	}

	// Do we really need to resize the texture?
	if (texture.stretch_to_power_of_two)
	{
		// Not if the size hasn't changed.
		texture.stretch_to_power_of_two = (texture.width != width) || (texture.height != height);
	}

	// Allocate the RAM.
	std::size_t buffer_size = NULL;
	switch(texture.format)
	{
	  case GU_PSM_DXT1:
	  //case GU_PSM_DXT1_EXT:
	    buffer_size = texture.width * texture.height / 2;
		break;
	  case GU_PSM_T8:
	  case GU_PSM_DXT3:
	  case GU_PSM_DXT5:
	 // case GU_PSM_DXT3_EXT:
	 // case GU_PSM_DXT5_EXT:
        buffer_size = texture.width * texture.height;
        break;
      case GU_PSM_T32:
      case GU_PSM_8888:
        buffer_size = texture.width * texture.height * 4;
        break;
    }
    
	ri.Con_Printf(PRINT_DEVELOPER ,"Loading: %s [%dx%d](%0.2f KB)\n",texture.identifier,texture.width,texture.height, (float) buffer_size);
	
	if (texture.mipmaps > 0) 
	{
		int size_incr = buffer_size/4;
		for (int i= 1;i <= texture.mipmaps;i++) 
		{
			buffer_size += size_incr;
			size_incr = size_incr/4;
		}
	}

	texture.ram	= static_cast<texel*>(memalign(16, buffer_size));
	
	if (!texture.ram)
	{
		Sys_Error("Out of RAM for textures.");
	}

	// Allocate the VRAM. quake::vram::allocate
	texture.vram = static_cast<texel*>(valloc(buffer_size));

	switch(texture.format)
	{
     case GU_PSM_T32:
	 case GU_PSM_8888:
       GL_Upload32(texture_index, data, width, height);
       break;
     case GU_PSM_T8:
       GL_Upload8(texture_index, data, width, height);
       break;
	 case GU_PSM_DXT1:
	 case GU_PSM_DXT3:
	 case GU_PSM_DXT5:
	 //case GU_PSM_DXT3_EXT:
	 //case GU_PSM_DXT5_EXT:
       GL_UploadDXT(texture_index, data, width, height);
	    break;
	}

 	if (texture.vram && texture.ram) 
	{
		free(texture.ram);
		texture.ram = NULL;
	}

	// Done.
	texture.texnum =  texture_index;
	return &texture;
}

image_t *GL_LoadPicLM (const char *identifier, int width, int height, const byte *data, int bpp, int filter, qboolean update, imagetype_t type, int dynamic)
{
	int texture_index = -1;
  
	tex_scale_down = gu_tex_scale_down->value == true;
	
	// See if the texture is already present.
	if (identifier[0])
	{
		for (int i = 0; i < MAX_GLTEXTURES; ++i)
		{
			if (gltextures_used[i])
			{
				image_t& texture = gltextures[i];

				if (!strcmp (identifier, texture.identifier))
				{
					if (update == false) 
					{
						texture.registration_sequence = registration_sequence;
						return &texture;
					} 
					else 
					{
						texture_index = i;
						break;
					}
				}
			}
		}
	}

 	if (update == false || texture_index == -1)
	{
		// Out of textures?
		if (numgltextures == MAX_GLTEXTURES)
		{
			Sys_Error("Out of OpenGL textures");
		}
	
		// Use the next available texture.
		numgltextures++;
		texturecount++;
		texture_index = numgltextures;
		
		for (int i = 0; i < MAX_GLTEXTURES; ++i)
		{
			if (gltextures_used[i] == 0)
			{
				texture_index = i;
				break;
			}
		}	
		image_t& texture = gltextures[texture_index];
		gltextures_used[texture_index] = 1;
	    texture.registration_sequence = registration_sequence;
		
		// Fill in the source data.
		strcpy(texture.identifier, identifier);
		texture.type                = type;
		texture.original_width		= width;
		texture.original_height		= height;
        texture.bpp                 = bpp;
        texture.texnum              = texture_index;

		texture.stretch_to_power_of_two	= false;

		// Fill in the texture description.
		if(!dynamic)
		{
			if (texture.bpp == 1)
			{
				texture.format		= GU_PSM_T8;
				texture.swizzle     = 1;
	        }
			else if (texture.bpp == 4)
			{
				switch(int(gu_lightmap_mode->value))
				{
				  case 0:
	               texture.format		= GU_PSM_5650;
	               texture.swizzle      = 1;
	               break;
	              case 1:
	               texture.format		= GU_PSM_5551;
	               texture.swizzle      = 1;
	               break;
	              case 2:
	               texture.format		= GU_PSM_4444;
	               texture.swizzle      = 1;
	               break;
	              case 3:
	               texture.format		= GU_PSM_8888;
	               texture.swizzle      = 1;
	               break;
                  case 4:
	               texture.format		= GU_PSM_DXT1;
	               texture.swizzle      = 0;
	               break;
	              case 5:
	               texture.format		= GU_PSM_DXT3;
	               texture.swizzle      = 0;
	               break;
	              case 6:
	               texture.format		= GU_PSM_DXT5;
	               texture.swizzle      = 0;
	               break;
				}
	        }
        }
        else
        {
            texture.format		= GU_PSM_8888;
            texture.swizzle     = 0; //disable swizzle for save fps
		}
		texture.filter			= filter;
		texture.mipmaps			= 0;
		
		if (tex_scale_down == true && texture.stretch_to_power_of_two == true) 
		{
			texture.width		= std::max(round_down(width),  16U);
			texture.height		= std::max(round_down(height), 16U);
		} 
		else 
		{
			texture.width		= std::max(round_up(width),  16U);
			texture.height		= std::max(round_up(height), 16U);
		}

		// Allocate the RAM.
		std::size_t buffer_size = NULL;
		switch(texture.format)
		{
		  case GU_PSM_DXT1:
		    buffer_size = texture.width * texture.height / 2;
			break;
		  case GU_PSM_T8:
		  case GU_PSM_DXT3:
		  case GU_PSM_DXT5:
	        buffer_size = texture.width * texture.height;
	        break;
          case GU_PSM_T32:
		  case GU_PSM_8888:
	        buffer_size = texture.width * texture.height * 4;
	        break;
          case GU_PSM_5650:
		  case GU_PSM_5551:
		  case GU_PSM_4444:
	        buffer_size = texture.width * texture.height * 2;
	        break;
	    }

		texture.ram	= static_cast<texel*>(memalign(16, buffer_size));

		if (!texture.ram)
		{
			Sys_Error("Out of RAM for lightmap textures.");
		}
	
		// Allocate the VRAM.
/*
		if(texture.vram)
		   Sys_Error("Vram is not free\n");
		texture.vram = static_cast<texel*>(valloc(buffer_size));
*/
		// Upload the texture.
		switch(texture.format)
		{
         case GU_PSM_T32:
		 case GU_PSM_8888:
           if(texture.swizzle)
           {
	         GL_Upload32(texture_index, data, width, height);
		   }
		   else
		   {
             GL_Upload32_A(texture_index, data, width, height);
           }
	       break;
         case GU_PSM_5650:
		 case GU_PSM_5551:
		 case GU_PSM_4444:
           if(texture.swizzle)
           {
              GL_Upload32to16(texture_index, data, width, height);
		   }
		   else
           {
              GL_Upload32to16_A(texture_index, data, width, height);
		   }
           break;
	     case GU_PSM_T8:
           if(texture.swizzle)
           {
		      GL_Upload8(texture_index, data, width, height);
		   }
		   else
		   {
		      GL_Upload8_A(texture_index, data, width, height);
		   }
	       break;
		 case GU_PSM_DXT1:
		 case GU_PSM_DXT3:
		 case GU_PSM_DXT5:
	       GL_UploadDXT(texture_index, data, width, height);
		    break;
		}

		texture.texnum =  texture_index;
		if (texture.vram && texture.ram) 
		{
			free(texture.ram);
			texture.ram = NULL;
		}	
	    return &texture;
	}
	else 
	{	
		image_t& texture = gltextures[texture_index];
	
		if ((width == texture.original_width) &&
           (height == texture.original_height))
		{
			// Upload the texture.
			switch(texture.format)
			{
	         case GU_PSM_T32:
			 case GU_PSM_8888:
	           if(texture.swizzle)
	           {
		         GL_Upload32(texture_index, data, width, height);
			   }
			   else
			   {
	             GL_Upload32_A(texture_index, data, width, height);
	           }
		       break;
	         case GU_PSM_5650:
			 case GU_PSM_5551:
			 case GU_PSM_4444:
	           if(texture.swizzle)
	           {
	              GL_Upload32to16(texture_index, data, width, height);
			   }
			   else
	           {
	              GL_Upload32to16_A(texture_index, data, width, height);
			   }
	           break;
		     case GU_PSM_T8:
	           if(texture.swizzle)
	           {
			      GL_Upload8(texture_index, data, width, height);
			   }
			   else
			   {
			      GL_Upload8_A(texture_index, data, width, height);
			   }
		       break;
			 case GU_PSM_DXT1:
			 case GU_PSM_DXT3:
			 case GU_PSM_DXT5:
		       GL_UploadDXT(texture_index, data, width, height);
			    break;
			}
		}

	    texture.texnum =  texture_index;

		if (texture.vram && texture.ram) 
		{
			free(texture.ram);
			texture.ram = NULL;
		}	

	    return &texture;
	}

	// Done.
  
}

/*
=================================================================

PCX LOADING

=================================================================
*/

/*
==============
LoadPCX
==============
*/
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	len = ri.FS_LoadFile (filename, (void **)&raw);
	if (!raw)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t*)raw;

  pcx->xmin = LittleShort(pcx->xmin);
  pcx->ymin = LittleShort(pcx->ymin);
  pcx->xmax = LittleShort(pcx->xmax);
  pcx->ymax = LittleShort(pcx->ymax);
  pcx->hres = LittleShort(pcx->hres);
  pcx->vres = LittleShort(pcx->vres);
  pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
  pcx->palette_type = LittleShort(pcx->palette_type);

	raw = (byte*)&pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		ri.Con_Printf (PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	out = static_cast<byte*>(malloc((pcx->ymax+1) * (pcx->xmax+1)));

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = static_cast<byte*>(malloc(768));
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free (*pic);
		*pic = NULL;
	}

	ri.FS_FreeFile (pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/
void LoadTGA (char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	int		length;
	TargaHeader		targa_header;
	byte			*targa_rgba;
	byte tmp[2];

	*pic = NULL;

	//
	// load the file
	//
	length = ri.FS_LoadFile (name, (void **)&buffer);
	if (!buffer)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad tga file %s\n", name);
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.y_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.width = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.height = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type!=2 
		&& targa_header.image_type!=10) 
		Sys_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type !=0 
		|| (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		Sys_Error ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = static_cast<byte*>(malloc (numPixels*4));
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment
	
	if (targa_header.image_type==2) {  // Uncompressed, RGB images
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) {
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) {
					case 24:
							
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
					case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
						default:
								blue = 0;
								green = 0;
								red = 0;
								alphabyte = 0;
								break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
						}
						column++;
						if (column==columns) { // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}

	ri.FS_FreeFile (buffer);
}

/*
================
GL_LoadWal
================
*/
image_t *GL_LoadWal (char *name)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		ri.Con_Printf (PRINT_ALL, "GL_FindImage: can't load %s\n", name);
	      return r_notexture;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);
 
    int level = 0;
	if (gu_mipmaps->value > 0)
		level = 3;
	
	image = GL_LoadPic (name, width, height, (byte *)mt + ofs, true, GU_LINEAR, level, it_wall, 8);

	ri.FS_FreeFile ((void *)mt);

	return image;
}

/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
image_t	*GL_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len;
	byte	*pic, *palette;
	int		width, height;

	if (!name)
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: NULL name");
	len = strlen(name);
	if (len<5)
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad name: %s", name);

	for(i = 0; i < MAX_TEXTURES; i++) 
	{
		// PSP_FIXME: add type compare
		if(!strcmp(name, gltextures[i].identifier)) 
		{
			gltextures[i].registration_sequence = registration_sequence;
			return &gltextures[i];
		}
	}

	//
	// load the pic from disk
	//
	pic = NULL;
	palette = NULL;
	if (!strcmp(name+len-4, ".pcx"))
	{
		LoadPCX (name, &pic, &palette, &width, &height);
		if (!pic)
			return NULL; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);

	  image = GL_LoadPic (name, width, height, (byte *)pic, true, GU_LINEAR, 0, type, 8);
	}
	else if (!strcmp(name+len-4, ".wal"))
	{
		image = GL_LoadWal (name);
	}

	else if (!strcmp(name+len-4, ".tga"))
	{
		LoadTGA (name, &pic, &width, &height);
		if (!pic)
			return NULL; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);
		image = GL_LoadPic (name, width, height, (byte*)pic, true, GU_LINEAR, 0, type, 32);
	}
	else
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad extension on: %s", name);


	if (pic)
	{
		free(pic);
	}
	if (palette)
	{	
		free(palette);
	}
	return image;
}



/*
===============
R_RegisterSkin
===============
*/
struct image_s *GL_RegisterSkin (char *name)
{
	return GL_FindImage (name, it_skin);
}

/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/ 

void GL_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;
    int countex = 0;
  	
	printf("texture count:%i\n",texturecount);
	printf("GL Count: %i\n",numgltextures);
	countex = texturecount;
  
	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;
	
 	for (i=0, image=gltextures ; i<MAX_TEXTURES; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		if (image->type == it_pic)
			continue;		// don't free pics
		
		// free it
		GL_UnloadTexture(image->texnum);
		memset (image, 0, sizeof(*image));
	}
    printf("GL U_Count: %i\n",numgltextures);
    printf("texture count:%i unloads: %i\n",texturecount, countex-texturecount);
}

void GL_FreeAllImages (void)
{
	int		i;
	image_t	*image;
    int countex = 0;

	countex = texturecount;
  
 	for (i=0, image=gltextures ; i<MAX_TEXTURES; i++, image++)
	{
        // never free r_notexture or particle texture
	 	if(r_notexture->texnum == image->texnum)
 		    continue;
		if(r_particletexture->texnum == image->texnum)
 		    continue;

		if (!image->registration_sequence)
			continue;		// free image_t slot

		if (image->type == it_pic)
			continue;		// don't free pics
		
		// free it
		GL_UnloadTexture(image->texnum);
		memset (image, 0, sizeof(*image));
	}
}

int Draw_GetPalette (void)
{
	int		i;
	int		r, g, b;
	unsigned	v;
	byte	*pic, *pal;
	int		width, height;

    memset(d_8to24table,      0, sizeof(d_8to24table));

	// get the palette

	LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (i=0 ; i<256 ; i++)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = v;
	}
  
	d_8to24table[255] = 0x00ffffff;
 
	free (pic);
	free (pal);

	return 0;
}

void GL_UpdateGamma(void)
{
	float g = vid_gamma->value;

    for (int i = 0; i < 256; i++ )
	{
		if (g == 1)
		{
			gammatable[i] = i;
		}
		else
		{
			float inf;

			inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;
			gammatable[i] = inf;
		}
	}

	for (int i =0 ; i<256 ; i++)
	{
		int j = i*intensity->value;
		if (j > 255)
			j = 255;
		intensitytable[i] = j;
	}
}

unsigned char GL_GetTexGamma(int i)
{
	 if(!gu_imagesinit)
 	     return i;
	 return gammatable[i];
}


/*
===============
GL_InitImages
===============
*/
void	GL_InitImages (void)
{
    if(gu_imagesinit)
	{
		Sys_Error("Called GU_InitImages() without GU_ShutdownImages()");
	}
	
    // init intensity conversions
	intensity = ri.Cvar_Get ("intensity", "2", 0);

	if ( intensity->value <= 1 )
		ri.Cvar_Set( "intensity", "1" );
 
    gu_imagesinit = true;

	memset(gltextures,        0, sizeof(gltextures));
	memset(gltextures_used,   0, sizeof(gltextures_used));
	memset(lightmap_textures, 0, sizeof(lightmap_textures)); //Clear state for lightmaps

	numgltextures = 0;
	registration_sequence = 1;

    GL_UpdateGamma();
    Draw_GetPalette();
    R_SetPalette (NULL);
}

/*
===============
GL_ShutdownImages
===============
*/
void	GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;
    int countex = 0;

	printf("texture count:%i\n",texturecount);
	countex = texturecount;
    printf("GL Count: %i\n",numgltextures);
	for (i=0, image=gltextures ; i<MAX_TEXTURES; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		
		// free it
		GL_UnloadTexture (image->texnum);
		memset (image, 0, sizeof(*image));
	}
    printf("GL U_Count: %i\n",numgltextures);
    printf("texture count:%i unloads: %i\n",texturecount, countex-texturecount);
    gu_imagesinit = false;
}
