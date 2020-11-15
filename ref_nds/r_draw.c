/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// draw.c

#include "r_local.h"
#include "../null/ds.h"
#include "../quake_ipc.h"

msurface_t *r_alpha_surfaces = NULL;

#define DS_BRIGHTNESS 0
#define COLOURED_LIGHTS
#define DYNAMIC_LIGHTS
//#define USE_HDR

image_t		*draw_chars;				// 8*8 graphic characters
extern int render_flags;

void D_DrawTri(int u, int v, int iz, float s, float t,
	int u2, int v2, int iz2, float s2, float t2,
	int u3, int v3, int iz3, float s3, float t3, int texture_handle);

void D_DrawQuad(int u, int v, int iz, float s, float t,
	int u2, int v2, int iz2, float s2, float t2,
	int u3, int v3, int iz3, float s3, float t3,
	int u4, int v4, int iz4, float s4, float t4, int texture_handle);

image_t *R_TextureAnimation (mtexinfo_t *tex) __attribute__((section(".itcm"), long_call));

//=============================================================================

/*
================
Draw_FindPic
================
*/
image_t *Draw_FindPic (char *name)
{
	image_t	*image;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
		image = R_FindImage (fullname, it_pic);
	}
	else
		image = R_FindImage (name+1, it_pic);

	return image;
}



/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	int dummy_handle = register_texture_deferred("Simon\'s dummy texture", (unsigned char *)0x2000000, 8, 8, false, 0, 0, 0);
	
	if (dummy_handle != 0)
		Sys_Error("binding a dummy texture but the handle wasn\'t zero!\n");
	
	set_gui_loading();
	draw_chars = Draw_FindPic ("conchars");
	set_gui_not_loading();
	
	if (draw_chars)
		ds_reinit_console(draw_chars->pixels[0]);
	
//	int sizeX, sizeY;
//	get_texture_sizes(draw_chars->texture_handle, &sizeX, &sizeY, NULL, NULL);
//	printf("texture is %d, %d %d\n", draw_chars->texture_handle, sizeX, sizeY);
//	while(1);
}



/*
================
Draw_Char

Draws one 8*8 graphics character
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_3D_Char(int x, int y, int num)
{
	int tex_x = next_size_up(draw_chars->width);
	int tex_y = next_size_up(draw_chars->height);
	
	int start_x = (num & 0xf) * 8;
	int start_y = (num >> 4) * 8;
	
//	int sizeX, sizeY;
//	get_texture_sizes(draw_chars->texture_handle, &sizeX, &sizeY, NULL, NULL);
//	printf("texture is %d, %d %d\n", draw_chars->texture_handle, sizeX, sizeY);

//	D_DrawQuad(0, 50, 1, 0, 0,
//			256, 50, 1, 1, 0,
//			256, 180, 1, 1, 1,
//			0, 180, 1, 0, 1,
//			draw_chars->texture_handle);
	
	D_DrawQuad(x, y, 1,				(float)start_x / tex_x,			(float)start_y / tex_y,
				x + 8, y, 1,		(float)(start_x + 8) / tex_x,	(float)start_y / tex_y,
				x + 8, y + 8, 1,	(float)(start_x + 8) / tex_x,	(float)(start_y + 8) / tex_y,
				x, y + 8, 1,		(float)start_x / tex_x,			(float)(start_y + 8) / tex_y,
				draw_chars->texture_handle);
}

void Draw_Char (int x, int y, int num)
{
	Draw_3D_Char(x, y, num);
	return;
#if 1
	unsigned short *text_map = (unsigned short *)(((9)*0x800)+0x6200000);
	
	if ((x >> 3) >=0 && (x >> 3) < 32)
		if ((y >> 3) >= 0 && (y >> 3) < 23)
			text_map[(y >> 3) * 32 + (x >> 3)] = num;
#else
	byte			*dest;
	byte			*source;
	int				drawline;	
	int				row, col;

	num &= 255;

	if (num == 32 || num == 32+128)
		return;

	if (y <= -8)
		return;			// totally off screen

//	if ( ( y + 8 ) >= vid.height )
	if ( ( y + 8 ) > vid.height )		// PGM - status text was missing in sw...
		return;

#ifdef PARANOID
	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		ri.Sys_Error (ERR_FATAL,"Con_DrawCharacter: (%i, %i)", x, y);
	if (num < 0 || num > 255)
		ri.Sys_Error (ERR_FATAL,"Con_DrawCharacter: char %i", num);
#endif

	row = num>>4;
	col = num&15;
	source = draw_chars->pixels[0] + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else
		drawline = 8;


	dest = vid.buffer + y*vid.rowbytes + x;

	while (drawline--)
	{
		if (source[0] != TRANSPARENT_COLOR)
			dest[0] = source[0];
		if (source[1] != TRANSPARENT_COLOR)
			dest[1] = source[1];
		if (source[2] != TRANSPARENT_COLOR)
			dest[2] = source[2];
		if (source[3] != TRANSPARENT_COLOR)
			dest[3] = source[3];
		if (source[4] != TRANSPARENT_COLOR)
			dest[4] = source[4];
		if (source[5] != TRANSPARENT_COLOR)
			dest[5] = source[5];
		if (source[6] != TRANSPARENT_COLOR)
			dest[6] = source[6];
		if (source[7] != TRANSPARENT_COLOR)
			dest[7] = source[7];
		source += 128;
		dest += vid.rowbytes;
	}
#endif
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_StretchPicImplementation
=============
*/
void Draw_StretchPicImplementation (int x, int y, int w, int h, image_t	*pic)
{
//	byte			*dest, *source;
//	int				v, u, sv;
//	int				height;
//	int				f, fstep;
//	int				skip;
//
//	if ((x < 0) ||
//		(x + w > vid.width) ||
//		(y + h > vid.height))
//	{
//		ri.Sys_Error (ERR_FATAL,"Draw_Pic: bad coordinates");
//	}
//
//	height = h;
//	if (y < 0)
//	{
//		skip = -y;
//		height += y;
//		y = 0;
//	}
//	else
//		skip = 0;
//
//	dest = vid.buffer + y * vid.rowbytes + x;
//
//	for (v=0 ; v<height ; v++, dest += vid.rowbytes)
//	{
//		sv = (skip + v)*pic->height/h;
//		source = pic->pixels[0] + sv*pic->width;
//		if (w == pic->width)
//			memcpy (dest, source, w);
//		else
//		{
//			f = 0;
//			fstep = pic->width*0x10000/w;
//			for (u=0 ; u<w ; u+=4)
//			{
//				dest[u] = source[f>>16];
//				f += fstep;
//				dest[u+1] = source[f>>16];
//				f += fstep;
//				dest[u+2] = source[f>>16];
//				f += fstep;
//				dest[u+3] = source[f>>16];
//				f += fstep;
//			}
//		}
//	}
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *name)
{
	image_t	*pic;

	pic = Draw_FindPic (name);
	if (!pic)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}
	Draw_StretchPicImplementation (x, y, w, h, pic);
}

/*
=============
Draw_StretchRaw
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	image_t	pic;

	pic.pixels[0] = data;
	pic.width = cols;
	pic.height = rows;
	Draw_StretchPicImplementation (x, y, w, h, &pic);
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *name)
{
	image_t			*pic;
	byte			*dest, *source;
	int				v, u;
	int				tbyte;
	int				height;

	pic = Draw_FindPic (name);
	if (!pic)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

//	if ((x < 0) ||
//		(x + pic->width > vid.width) ||
//		(y + pic->height > vid.height))
//		return;	//	ri.Sys_Error (ERR_FATAL,"Draw_Pic: bad coordinates");

	height = pic->height;
	source = pic->pixels[0];
	if (y < 0)
	{
		height += y;
		source += pic->width*-y;
		y = 0;
	}
	
	int tex_x = next_size_up(pic->width);
	int tex_y = next_size_up(pic->height);
	
//	D_DrawTri(x, y, 1, 0, 0,
//				x + pic->width, y, 1, (float)pic->width / tex_x, 0,
//				x + pic->width, y + pic->height, 1, (float)pic->width / tex_x, (float)pic->height / tex_y,
//				pic->texture_handle);
//	D_DrawTri(x, y, 1, 0, 0,
//				x + pic->width, y + pic->height, 1, (float)pic->width / tex_x, (float)pic->height / tex_y,
//				x, y + pic->height, 1, 0, (float)pic->height / tex_y,
//				pic->texture_handle);
	
	D_DrawQuad(x, y, 1, 0, 0,
				x + pic->width, y, 1, 					(float)pic->width / tex_x, 0,
				x + pic->width, y + pic->height, 1,		(float)pic->width / tex_x, (float)pic->height / tex_y,
				x, y + pic->height, 1,					0, (float)pic->height / tex_y,
				pic->texture_handle);

//	dest = vid.buffer + y * vid.rowbytes + x;
//
//	if (!pic->transparent)
//	{
//		for (v=0 ; v<height ; v++)
//		{
//			memcpy (dest, source, pic->width);
//			dest += vid.rowbytes;
//			source += pic->width;
//		}
//	}
//	else
//	{
//		if (pic->width & 7)
//		{	// general
//			for (v=0 ; v<height ; v++)
//			{
//				for (u=0 ; u<pic->width ; u++)
//					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
//						dest[u] = tbyte;
//
//				dest += vid.rowbytes;
//				source += pic->width;
//			}
//		}
//		else
//		{	// unwound
//			for (v=0 ; v<height ; v++)
//			{
//				for (u=0 ; u<pic->width ; u+=8)
//				{
//					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
//						dest[u] = tbyte;
//					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
//						dest[u+1] = tbyte;
//					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
//						dest[u+2] = tbyte;
//					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
//						dest[u+3] = tbyte;
//					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
//						dest[u+4] = tbyte;
//					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
//						dest[u+5] = tbyte;
//					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
//						dest[u+6] = tbyte;
//					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
//						dest[u+7] = tbyte;
//				}
//				dest += vid.rowbytes;
//				source += pic->width;
//			}
//		}
//	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *name)
{
//	int			i, j;
//	byte		*psrc;
//	byte		*pdest;
//	image_t		*pic;
//	int			x2;
//
//	if (x < 0)
//	{
//		w += x;
//		x = 0;
//	}
//	if (y < 0)
//	{
//		h += y;
//		y = 0;
//	}
//	if (x + w > vid.width)
//		w = vid.width - x;
//	if (y + h > vid.height)
//		h = vid.height - y;
//	if (w <= 0 || h <= 0)
//		return;
//
//	pic = Draw_FindPic (name);
//	if (!pic)
//	{
//		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", name);
//		return;
//	}
//	x2 = x + w;
//	pdest = vid.buffer + y*vid.rowbytes;
//	for (i=0 ; i<h ; i++, pdest += vid.rowbytes)
//	{
//		psrc = pic->pixels[0] + pic->width * ((i+y)&63);
//		for (j=x ; j<x2 ; j++)
////			pdest[j] = psrc[j&63];
//			byte_write(&pdest[j], psrc[j&63]);
//	}
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
//	byte			*dest;
//	int				u, v;
//
//	if (x+w > vid.width)
//		w = vid.width - x;
//	if (y+h > vid.height)
//		h = vid.height - y;
//	if (x < 0)
//	{
//		w += x;
//		x = 0;
//	}
//	if (y < 0)
//	{
//		h += y;
//		y = 0;
//	}
//	if (w < 0 || h < 0)
//		return;
//	dest = vid.buffer + y*vid.rowbytes + x;
//	for (v=0 ; v<h ; v++, dest += vid.rowbytes)
//		for (u=0 ; u<w ; u++)
////			dest[u] = c;
//			byte_write(&dest[u], c);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
//	int			x,y;
//	byte		*pbuf;
//	int	t;
//
//	for (y=0 ; y<vid.height ; y++)
//	{
//		pbuf = (byte *)(vid.buffer + vid.rowbytes*y);
//		t = (y & 1) << 1;
//
//		for (x=0 ; x<vid.width ; x++)
//		{
//			if ((x & 3) != t)
//				pbuf[x] = 0;
//		}
//	}
}

#define RGB8(r,g,b)  (((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10))
unsigned char poly = 255;

int drawn_polys = 0;
int drawn_tris = 0;

mtexinfo_t *last_texinfo = NULL;

float hdr_scale = 1.0f;
unsigned int this_frame_max = 0, this_frame_average = 0, this_frame_total = 0;

//unsigned int cmd_list[16];

inline void ds_draw_list(unsigned int* list, unsigned int count) __attribute__ ((no_instrument_function));
inline void ds_draw_list(unsigned int* list, unsigned int count)
{
	while(DMA_CR(0) & DMA_BUSY);
	
	DMA_SRC(0) = (uint32)list;
	DMA_DEST(0) = 0x4000400;
	DMA_CR(0) = DMA_FIFO | count;
}

void R_RenderPoly(msurface_t *fa) __attribute__((section(".itcm"), long_call));
void R_RenderPoly(msurface_t *fa)
{
	medge_t *pedges = currentmodel->edges;
	unsigned int num_verts = fa->numedges;
	unsigned int count;
	int lindex;
	
//	bool using_transparency = false;
//	
//	if (fa->texinfo->flags & ( SURF_TRANS66 | SURF_TRANS33 ))
//	{
//		using_transparency = true;
//		ds_polyfmt(1, 15, 0, POLY_CULL_NONE);
//	}
	
	drawn_polys++;
	
	if (last_texinfo != fa->texinfo)
	{
		image_t *t = R_TextureAnimation (fa->texinfo);
		
//		int w, h, big_w, big_h;
//		get_texture_sizes(t->texture_handle, &w, &h, &big_w, &big_h);
//		
//		if ((w == big_w) && (h == big_h))
			bind_texture(t->texture_handle);
//		else
//			bind_texture(-1);
		
		last_texinfo = fa->texinfo;
	}
	
	render_flags |= fa->flags;
	
#ifdef COLOURED_LIGHTS
	//set up the fake colour-based lighmapping
	int lightmap_val[3];
	
	if ((int)fa->samples != (1 << 10))
	{
		lightmap_val[0] = fa->samples[0];
		lightmap_val[1] = fa->samples[1];
		lightmap_val[2] = fa->samples[2];
	}
	else
		lightmap_val[0] = lightmap_val[1] = lightmap_val[2] = 255;
		
	int comb_brightness[3];
	
	int total_mult[3];
	total_mult[0] = total_mult[1] = total_mult[2] = 0;
	
	for (count = 0; (count < MAXLIGHTMAPS) && (fa->styles[count] != 255); count++)
	{
		total_mult[0] += r_newrefdef.lightstyles[fa->styles[count]].int_rgb[0];
		total_mult[1] += r_newrefdef.lightstyles[fa->styles[count]].int_rgb[1];
		total_mult[2] += r_newrefdef.lightstyles[fa->styles[count]].int_rgb[2];
	}
	
	comb_brightness[0] = lightmap_val[0] * total_mult[0];
	comb_brightness[1] = lightmap_val[1] * total_mult[1];
	comb_brightness[2] = lightmap_val[2] * total_mult[2];
	
#ifdef USE_HDR
	for (int count = 0; count < 3; count++)
	{
		comb_brightness[count] = (int)((float)comb_brightness[count] * hdr_scale);
		
		if (comb_brightness[count] > this_frame_max)
			this_frame_max = comb_brightness[count];
		
		this_frame_average += comb_brightness[count];
		this_frame_total++;
	}
	
	for (int count = 0; count < 3; count++)
		comb_brightness[count] = (int)((float)comb_brightness[count] * hdr_scale * hdr_scale * hdr_scale);
#endif
	
#ifdef DYNAMIC_LIGHTS
	if (fa->dlightbits)
		for (int lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++)
		{
			if ( !(fa->dlightbits & (1<<lnum) ) )
				continue;		// not lit by this light
	
			dlight_t *dl = &r_newrefdef.dlights[lnum];
			float rad = fabs(dl->intensity);
			//PGM
			//=====
	
			float dist = DotProduct (dl->origin, fa->plane->normal) - fa->plane->dist;
			rad -= fabs(dist);
			if (rad < 32)
				continue;
	
			comb_brightness[0] += rad;
			comb_brightness[1] += rad;
			comb_brightness[2] += rad;
		}
#endif
	
	for (int count = 0; count < 3; count++)
	{
		comb_brightness[count] += DS_BRIGHTNESS;
		
		if (comb_brightness[count] > 255)
			comb_brightness[count] = 255;
//		if (comb_brightness[count] < 0)
//			comb_brightness[count] = 0;
	}
	
	if (count)
		DS_COLOUR3B(comb_brightness[0], comb_brightness[1], comb_brightness[2]);
	else
		DS_COLOUR3B(255, 255, 255);
#else
	//set up the fake colour-based lighmapping
	int lightmap_val = 255;
	if ((int)fa->samples != (1 << 10))
		lightmap_val = (int)fa->samples;

	int comb_brightness = 1;
	int added = 0;

	for (count = 0; count < MAXLIGHTMAPS && fa->styles[count] != 255; count++, added++)
		comb_brightness += lightmap_val * r_newrefdef.lightstyles[fa->styles[count]].int_white;

	comb_brightness += DS_BRIGHTNESS;

	if (comb_brightness > 255)
		comb_brightness = 255;
	if (comb_brightness < 0)
		comb_brightness = 0;

	if (added == 0)
		comb_brightness = 255;

	DS_COLOUR3B(comb_brightness, comb_brightness, comb_brightness);
#endif
	
#if 0
	short init_x[2], init_y[2], init_z[2];
//	short init_s[2], init_t[2];
	short x_curr, y_curr, z_curr, s_curr, t_curr;
	
	unsigned int init_uv[2];
	unsigned int uv_curr;
	
//	if (drawn_polys == -1)
	{
		for (count = 0; count < 2; count++)
		{
			short short_local[3];

			short this_vert[3];
			int index;
			bool less_than;
			medge_t *r_pedge;

			lindex = currentmodel->surfedges[fa->firstedge + count];
			less_than = lindex <= 0;

			if (less_than)
				r_pedge = &pedges[-lindex];
			else
				r_pedge = &pedges[lindex];

			index = r_pedge->v[less_than] * 3;

			init_x[count] = r_pcurrentrealvertbase[index];
			init_y[count] = r_pcurrentrealvertbase[index + 1];
			init_z[count] = r_pcurrentrealvertbase[index + 2];
			init_uv[count] = fa->texture_coordinates[count];
		}
		
//		cmd_list[0] = 12;

		for (count = 2; count < num_verts; count++)
		{
			short short_local[3];

			short this_vert[3];
			int index;
			bool less_than;
			medge_t *r_pedge;

			lindex = currentmodel->surfedges[fa->firstedge + count];
			less_than = lindex <= 0;

			if (less_than)
				r_pedge = &pedges[-lindex];
			else
				r_pedge = &pedges[lindex];

			index = r_pedge->v[less_than] * 3;

			uv_curr = fa->texture_coordinates[count];

			x_curr = r_pcurrentrealvertbase[index];
			y_curr = r_pcurrentrealvertbase[index + 1];
			z_curr = r_pcurrentrealvertbase[index + 2];
			
//			unsigned int *cmd_ptr = &cmd_list[1];
//			
//			*cmd_ptr++ = FIFO_COMMAND_PACK(FIFO_BEGIN,
//				FIFO_TEX_COORD, FIFO_VERTEX16,
//				FIFO_TEX_COORD);
//			
//			*cmd_ptr++ = 0;
//			*cmd_ptr++ = init_uv[0];
//			*cmd_ptr++ = (init_y[0] << 16) | (init_x[0] & 0xffff);
//			*cmd_ptr++ = ((unsigned int)(unsigned short)init_z[0]);
//			*cmd_ptr++ = init_uv[1];
//			
//			*cmd_ptr++ = FIFO_COMMAND_PACK(FIFO_VERTEX16,
//					FIFO_TEX_COORD, FIFO_VERTEX16,
//					FIFO_NOP);
//			
//			*cmd_ptr++ = (init_y[1] << 16) | (init_x[1] & 0xffff);
//			*cmd_ptr++ = ((unsigned int)(unsigned short)init_z[1]);
//			*cmd_ptr++ = uv_curr;
//			*cmd_ptr++ = (y_curr << 16) | (x_curr & 0xffff);
//			*cmd_ptr++ = ((unsigned int)(unsigned short)z_curr);
//
//			glCallList((const u32 *)cmd_list);

			DS_BEGIN_TRIANGLE();

			GFX_TEX_COORD = init_uv[0];
			DS_VERTEX3V16(init_x[0], init_y[0], init_z[0]);

			GFX_TEX_COORD = init_uv[1];
			DS_VERTEX3V16(init_x[1], init_y[1], init_z[1]);

			GFX_TEX_COORD = uv_curr;
			DS_VERTEX3V16(x_curr, y_curr, z_curr);
			
			drawn_tris++;

			init_x[1] = x_curr;
			init_y[1] = y_curr;
			init_z[1] = z_curr;
			init_uv[1] = uv_curr;
		}
	}
#endif
	if (fa->display_list)
		ds_draw_list(fa->display_list, (num_verts - 2) * 12);
	
//	if (using_transparency == true)
//		ds_polyfmt(0, 31, 0, POLY_CULL_NONE);
}

void R_ZDrawSubmodelPolys (model_t *pmodel)
{
	int			i, numsurfaces;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;

	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
	numsurfaces = pmodel->nummodelsurfaces;
	
//	int big_modelorg[3];
//	big_modelorg[0] = modelorg[0] * 1024;
//	big_modelorg[1] = modelorg[1] * 1024;
//	big_modelorg[2] = modelorg[2] * 1024;
	
	bool transparent = false;

	for (i=0 ; i<numsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal);
		dot -= pplane->dist;
//		dot = DotProduct (modelorg, pplane->normal) - (pplane->bigdist >> 10);
//		dot = (DotProductII(big_modelorg, pplane->bignormal) - pplane->bigdist) >> 10;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if ((psurf->texinfo->flags & ( SURF_TRANS66 | SURF_TRANS33 )) && !transparent)
			{
				transparent = true;
				ds_polyfmt(1, 15, 0, POLY_CULL_NONE);
			}
			if (!(psurf->texinfo->flags & ( SURF_TRANS66 | SURF_TRANS33 )) && transparent)
			{
				transparent = false;
				ds_polyfmt(0, 31, 0, POLY_CULL_NONE);
			}
			
		// FIXME: use bounding-box-based frustum clipping info?
			R_RenderPoly (psurf);
		}
	}
	
	ds_polyfmt(0, 31, 0, POLY_CULL_NONE);
}

float tri_z;

void reset_triz(void)
{
	tri_z = 1.0f;
}

void next_triz(void)
{
	tri_z += 0.0f;
}

#ifdef R21
#define glTexCoord2t16(x, y) glTexCoord2t16(y, x)
#endif

void D_DrawQuad(int u, int v, int iz, float s, float t,
	int u2, int v2, int iz2, float s2, float t2,
	int u3, int v3, int iz3, float s3, float t3,
	int u4, int v4, int iz4, float s4, float t4, int texture_handle)
{
	float tri_scale = 1.0f / 256;
	
	u -= 127;
	u2 -= 127;
	u3 -= 127;
	u4 -= 127;
	
	v -= 95;
	v2 -= 95;
	v3 -= 95;
	v4 -= 95;
	
	if (!bind_texture(texture_handle))
		return;
	
//	bind_texture(texture_handle);
	
	int tex_width, tex_height;
	
	get_texture_sizes(texture_handle, NULL, NULL, &tex_width, &tex_height);
	
	DS_BEGIN_QUAD();
	
	DS_COLOUR3B(255, 255, 255);
	
	glTexCoord2t16((short)((t * tex_height) * 16), (short)((s * tex_width) * 16));
	glVertex3f((float)u * tri_scale, -(float)v * tri_scale, tri_z);
	
	glTexCoord2t16((short)((t2 * tex_height) * 16), (short)((s2 * tex_width) * 16));
	glVertex3f((float)u2 * tri_scale, -(float)v2 * tri_scale, tri_z);
	
	glTexCoord2t16((short)((t3 * tex_height) * 16), (short)((s3 * tex_width) * 16));
	glVertex3f((float)u3 * tri_scale, -(float)v3 * tri_scale, tri_z);
	
	glTexCoord2t16((short)((t4 * tex_height) * 16), (short)((s4 * tex_width) * 16));
	glVertex3f((float)u4 * tri_scale, -(float)v4 * tri_scale, tri_z);
	
	next_triz();
}
