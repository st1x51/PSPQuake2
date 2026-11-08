/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2023 Sergey Galushko

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

// gu_draw.c

#include "gu_local.h"

#define SLICE_W			64
#define NUM_SLICES(w)	(((w) + SLICE_W - 1) / SLICE_W)

image_t		*draw_chars;
#ifdef KINGPIN_ENGINE
image_t		*draw_chars2;
image_t		*draw_chars_upper;
image_t		*draw_bigchars;
#endif

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
#ifdef KINGPIN_ENGINE
	draw_chars = GL_FindImage ("pics/conchars.tga", IMG_TYPE_PIC | IMG_FILTER_NEAREST);
	draw_chars2 = GL_FindImage ("pics/conchars3.tga", IMG_TYPE_PIC | IMG_FILTER_NEAREST);
	draw_chars_upper = GL_FindImage ("pics/conchars2.tga", IMG_TYPE_PIC | IMG_FILTER_NEAREST);
	draw_bigchars = GL_FindImage ("pics/conchars_high_11x11.tga", IMG_TYPE_PIC | IMG_FILTER_NEAREST);

	if (!draw_chars)
		draw_chars = GL_FindImage ("pics/conchars.pcx", IMG_TYPE_PIC | IMG_FILTER_NEAREST);
#else
	draw_chars = GL_FindImage ("pics/conchars.pcx", IMG_TYPE_PIC | IMG_FILTER_NEAREST);
#endif
}



/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
#ifdef KINGPIN_ENGINE
void Draw_CharImage (image_t *font, int x, int y, int num)
{
	int				row, col;
	short			hrow, hcol, size;

	num &= 255;

	if ( (num&127) == 32 )
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	size = 8;
	hrow = row * size;
	hcol = col * size;

	GL_Bind (font);

	gu_vert_htv_t* const out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t ) * 2);
	out[0].u = hcol;
	out[0].v = hrow;
	out[0].x = x;
	out[0].y = y;
	out[0].z = 0;
	out[1].u = hcol + size;
	out[1].v = hrow + size;
	out[1].x = x + size;
	out[1].y = y + size;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);
}

void Draw_Char (int x, int y, int num)
{
	Draw_CharImage (draw_chars, x, y, num);
}

void Draw_Char2 (int x, int y, int num)
{
	Draw_CharImage (draw_chars2, x, y, num);
}

/*
================
Draw_CharUpper
================
*/
void Draw_CharUpper (int x, int y, int num)
{
	if ((num > 0x60 && num < 0x7b) || (num > 0xe0 && num < 0xfb))
		num -= 0x20;
	num &= 255;

	if ((num & 127) == 32 || y <= -8)
		return;

	sceGuDisable (GU_ALPHA_TEST);
	sceGuEnable (GU_BLEND);

	GL_TexEnv (GU_TFX_MODULATE);

	Draw_CharImage (draw_chars_upper, x, y, num);

	GL_TexEnv (GU_TFX_REPLACE);

	sceGuEnable (GU_ALPHA_TEST);
	sceGuDisable (GU_BLEND);
}

/*
================
Draw_BigChar
================
*/
void Draw_BigChar (int x, int y, int num, float *color)
{
	int		glyph, row, col;
	short	hrow, hcol;
	const short size = 11;

	glyph = (num & 127) - 32;
	if (glyph <= 0 || y <= -11)
		return;

	row = glyph / 11;
	col = glyph % 11;
	hrow = row * size;
	hcol = col * size;

	GL_TexEnv (GU_TFX_MODULATE);
	sceGuColor (GU_HCOLOR_3FV(color));
	GL_Bind (draw_bigchars);

	gu_vert_htv_t* const out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t ) * 2);
	out[0].u = hcol;
	out[0].v = hrow;
	out[0].x = x;
	out[0].y = y;
	out[0].z = 0;
	out[1].u = hcol + size;
	out[1].v = hrow + size;
	out[1].x = x + size;
	out[1].y = y + size;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);

	GL_TexEnv (GU_TFX_REPLACE);
	sceGuColor (GU_HCOLOR_DEFAULT);
}

#else
void Draw_Char (int x, int y, int num)
{
	int				row, col;
	short			hrow, hcol, size;

	num &= 255;

	if ( (num&127) == 32 )
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	size = 8;
	hrow = row * size;
	hcol = col * size;

	GL_Bind (draw_chars);

	gu_vert_htv_t* const out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t ) * 2);
	out[0].u = hcol;
	out[0].v = hrow;
	out[0].x = x;
	out[0].y = y;
	out[0].z = 0;
	out[1].u = hcol + size;
	out[1].v = hrow + size;
	out[1].x = x + size;
	out[1].y = y + size;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);
}
#endif

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	image_t *image;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
#ifdef KINGPIN_ENGINE
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.tga", name);
#else
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
#endif
		image = GL_FindImage (fullname, IMG_TYPE_PIC);
	}
	else
		image = GL_FindImage (name+1, IMG_TYPE_PIC);

	return image;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *image;

	image = Draw_FindPic (pic);
	if (!image)
	{
		*w = *h = -1;
		return;
	}
	*w = image->width;
	*h = image->height;
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	image_t *image;
	int num_slices;
	int i, x1, x2, sx, sw;
	float u_step;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	num_slices = NUM_SLICES (w);
	u_step = (image->sh - image->sl) / (float)w;

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuDisable (GU_ALPHA_TEST);

	GL_Bind (image);

	gu_vert_htv_t* out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t) * 2 * num_slices);
	gu_vert_htv_t* out_ptr = out;

	for (i = 0; i < num_slices; i++)
	{
		sx = i * SLICE_W;
		sw = (sx + SLICE_W > w) ? (w - sx) : SLICE_W;

		out_ptr->u = image->sl + (sx * u_step);
		out_ptr->v = image->tl;
		out_ptr->x = x + sx;
		out_ptr->y = y;
		out_ptr->z = 0;
		out_ptr++;

		out_ptr->u = image->sl + ((sx + sw) * u_step);
		out_ptr->v = image->th;
		out_ptr->x = x + sx + sw;
		out_ptr->y = y + h;
		out_ptr->z = 0;
		out_ptr++;
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * num_slices, 0, out);

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuEnable (GU_ALPHA_TEST);
}

#ifdef KINGPIN_ENGINE
/*
=============
Draw_StretchPicTransparent
=============
*/
void Draw_StretchPicTransparent (int x, int y, int w, int h, char *pic, float alpha)
{
	image_t *image;
	int num_slices;
	int i, x1, x2, sx, sw;
	float u_step;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	num_slices = NUM_SLICES (w);
	u_step = (image->sh - image->sl) / (float)w;

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuDisable (GU_ALPHA_TEST);

	sceGuDisable (GU_ALPHA_TEST);
	sceGuEnable (GU_BLEND);

	GL_TexEnv (GU_TFX_MODULATE);

	sceGuColor (GU_HCOLOR_4F(1, 1, 1, alpha));

	GL_Bind (image);

	gu_vert_htv_t* out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t) * 2 * num_slices);
	gu_vert_htv_t* out_ptr = out;

	for (i = 0; i < num_slices; i++)
	{
		sx = i * SLICE_W;
		sw = (sx + SLICE_W > w) ? (w - sx) : SLICE_W;

		out_ptr->u = image->sl + (sx * u_step);
		out_ptr->v = image->tl;
		out_ptr->x = x + sx;
		out_ptr->y = y;
		out_ptr->z = 0;
		out_ptr++;

		out_ptr->u = image->sl + ((sx + sw) * u_step);
		out_ptr->v = image->th;
		out_ptr->x = x + sx + sw;
		out_ptr->y = y + h;
		out_ptr->z = 0;
		out_ptr++;
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * num_slices, 0, out);

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuEnable (GU_ALPHA_TEST);

	GL_TexEnv (GU_TFX_REPLACE);

	sceGuEnable (GU_ALPHA_TEST);
	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);
}
#endif

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t *image;
	int num_slices;
	int i, w, sx, sw;
	float u_step;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	w = image->width;
	num_slices = NUM_SLICES(w);
	u_step = (image->sh - image->sl) / (float)w;

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuDisable (GU_ALPHA_TEST);

	GL_Bind (image);

	gu_vert_htv_t* out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t) * 2 * num_slices);
	gu_vert_htv_t* out_ptr = out;

	for (i = 0; i < num_slices; i++)
	{
		sx = i * SLICE_W;
		sw = (sx + SLICE_W > w) ? (w - sx) : SLICE_W;

		out_ptr->u = image->sl + (sx * u_step);
		out_ptr->v = image->tl;
		out_ptr->x = x + sx;
		out_ptr->y = y;
		out_ptr->z = 0;
		out_ptr++;

		out_ptr->u = image->sl + ((sx + sw) * u_step);
		out_ptr->v = image->th;
		out_ptr->x = x + sx + sw;
		out_ptr->y = y + image->height;
		out_ptr->z = 0;
		out_ptr++;
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * num_slices, 0, out);

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuEnable (GU_ALPHA_TEST);
}

#ifdef KINGPIN_ENGINE
/*
=============
Draw_PicTransparent
=============
*/
void Draw_PicTransparent (int x, int y, char *pic, float alpha)
{
	image_t *image;
	int num_slices;
	int i, w, sx, sw;
	float u_step;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	w = image->width;
	num_slices = NUM_SLICES(w);
	u_step = (image->sh - image->sl) / (float)w;

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuDisable (GU_ALPHA_TEST);

	sceGuDisable (GU_ALPHA_TEST);
	sceGuEnable (GU_BLEND);

	GL_TexEnv (GU_TFX_MODULATE);

	sceGuColor (GU_HCOLOR_4F(1, 1, 1, alpha));

	GL_Bind (image);

	gu_vert_htv_t* out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t) * 2 * num_slices);
	gu_vert_htv_t* out_ptr = out;

	for (i = 0; i < num_slices; i++)
	{
		sx = i * SLICE_W;
		sw = (sx + SLICE_W > w) ? (w - sx) : SLICE_W;

		out_ptr->u = image->sl + (sx * u_step);
		out_ptr->v = image->tl;
		out_ptr->x = x + sx;
		out_ptr->y = y;
		out_ptr->z = 0;
		out_ptr++;

		out_ptr->u = image->sl + ((sx + sw) * u_step);
		out_ptr->v = image->th;
		out_ptr->x = x + sx + sw;
		out_ptr->y = y + image->height;
		out_ptr->z = 0;
		out_ptr++;
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * num_slices, 0, out);

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuEnable (GU_ALPHA_TEST);

	GL_TexEnv (GU_TFX_REPLACE);

	sceGuEnable (GU_ALPHA_TEST);
	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);
}
#endif

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t *image;
	int num_slices;
	int i, sx, sw, x1, x2;
	float u_step;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	num_slices = NUM_SLICES (w);

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuDisable (GU_ALPHA_TEST);

	GL_Bind (image);

	gu_vert_htv_t* out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t) * 2 * num_slices);
	gu_vert_htv_t* out_ptr = out;

	for (i = 0; i < num_slices; i++)
	{
		sx = i * SLICE_W;
		sw = (sx + SLICE_W > w) ? (w - sx) : SLICE_W;

		x1 = x + sx;
		x2 = x1 + sw;

		out_ptr->u = x1;
		out_ptr->v = y;
		out_ptr->x = x1;
		out_ptr->y = y;
		out_ptr->z = 0;
		out_ptr++;

		out_ptr->u = x2;
		out_ptr->v = y + h;
		out_ptr->x = x2;
		out_ptr->y = y + h;
		out_ptr->z = 0;
		out_ptr++;
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * num_slices, 0, out);

	//if (!(image->flags & IMG_FLAG_HAS_ALPHA))
	//	sceGuEnable (GU_ALPHA_TEST);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	if ( (unsigned)c > 255)
		ri.Sys_Error (ERR_FATAL, "Draw_Fill: bad color");

	sceGuDisable (GU_TEXTURE_2D);
	sceGuColor (d_8to24table[c]);

	gu_vert_hv_t* const out = (gu_vert_hv_t*)sceGuGetMemory (sizeof(gu_vert_hv_t) * 2);
	out[0].x = x;
	out[0].y = y;
	out[0].z = 0;
	out[1].x = x + w;
	out[1].y = y + h;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);

	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuEnable (GU_TEXTURE_2D);
}

#ifdef KINGPIN_ENGINE
/*
=============
Draw_FillRGBA

Fills a box of pixels with a single color(rgba)
=============
*/
void Draw_FillRGBA (int x, int y, int w, int h, float r, float g, float b, float a)
{
	sceGuDisable (GU_TEXTURE_2D);
	sceGuDisable (GU_ALPHA_TEST);
	sceGuEnable (GU_BLEND);

	GL_TexEnv (GU_TFX_MODULATE);

	sceGuColor (GU_HCOLOR_4F(r, g, b, a));

	gu_vert_hv_t* const out = (gu_vert_hv_t*)sceGuGetMemory (sizeof(gu_vert_hv_t) * 2);
	out[0].x = x;
	out[0].y = y;
	out[0].z = 0;
	out[1].x = x + w;
	out[1].y = y + h;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);

	GL_TexEnv (GU_TFX_REPLACE);

	sceGuEnable (GU_ALPHA_TEST);
	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuEnable (GU_TEXTURE_2D);
}
#endif

/*
=============
Draw_Line
=============
*/
void Draw_Line (int x0, int y0, int x1, int y1, int c)
{
	if ( (unsigned)c > 255)
		ri.Sys_Error (ERR_FATAL, "Draw_Fill: bad color");

	sceGuDisable (GU_TEXTURE_2D);
	sceGuColor (d_8to24table[c]);

	gu_vert_hv_t* const out = (gu_vert_hv_t*)sceGuGetMemory (sizeof(gu_vert_hv_t) * 2);
	out[0].x = x0;
	out[0].y = y0;
	out[0].z = 0;
	out[1].x = x1;
	out[1].y = y1;
	out[1].z = 0;
	sceGuDrawArray (GU_LINES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);

	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuEnable (GU_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	sceGuEnable (GU_BLEND);
	sceGuDisable (GU_TEXTURE_2D);
	sceGuColor (GU_HCOLOR_4UB(0, 0, 0, 0xcc));

	gu_vert_hv_t* const out = (gu_vert_hv_t*)sceGuGetMemory (sizeof(gu_vert_hv_t) * 2);
	out[0].x = 0;
	out[0].y = 0;
	out[0].z = 0;
	out[1].x = vid.width;
	out[1].y = vid.height;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);

	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuEnable (GU_TEXTURE_2D);
	sceGuDisable (GU_BLEND);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	int num_slices;
	int i, x1, x2;
	float x1r, x2r;

	num_slices = NUM_SLICES (w);

	sceKernelDcacheWritebackRange(data, cols * rows);

	sceGuTexMode (GU_PSM_T8, 0, 0, GU_FALSE);
	sceGuTexFilter (GU_LINEAR, GU_LINEAR);
	sceGuTexImage (0, 512, 256, cols, data);

	gl_state.currenttexture = NULL;

	gu_vert_htv_t* out = (gu_vert_htv_t*)sceGuGetMemory (sizeof(gu_vert_htv_t) * 2 * num_slices);
	gu_vert_htv_t* out_ptr = out;

	for (i = 0; i < num_slices; i++)
	{
		x1r = (float)(i * SLICE_W) / (float)w;
		x2r = (float)((i + 1) * SLICE_W > w ? w : (i + 1) * SLICE_W) / (float)w;

		x1 = x + (i * SLICE_W);
		x2 = x + ((i + 1) * SLICE_W > w ? w : (i + 1) * SLICE_W);

		out_ptr->u = x1r * cols;
		out_ptr->v = 0;
		out_ptr->x = x1;
		out_ptr->y = y;
		out_ptr->z = 0;
		out_ptr++;

		out_ptr->u = x2r * cols;
		out_ptr->v = rows;
		out_ptr->x = x2;
		out_ptr->y = y + h;
		out_ptr->z = 0;
		out_ptr++;
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * num_slices, 0, out);
}
