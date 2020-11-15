#include "gu_local.h"

image_t		*draw_chars;

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_pic);
}

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = GL_FindImage (fullname, it_pic);
	}
	else
		gl = GL_FindImage (name+1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize(int* w, int* h, char* name) 
{
	image_t* img = Draw_FindPic(name);
	if(!img) 
	{
		*w = *h = 0;
		return;
	}

	*w = img->original_width;
	*h = img->original_height;
}

/*
=============
Draw_Image
=============
*/
void Draw_Image(int x, int y, const image_t* img) 
{
	GL_Bind(img->texnum);

	struct vertex
	{
		short			u, v;
		short			x, y, z;
	};

	vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 2));

	vertices[0].u = 0;
	vertices[0].v = 0;
	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = 0;
 
 	vertices[1].u = img->width;
	vertices[1].v = img->height;
	vertices[1].x = x + img->original_width;
	vertices[1].y = y + img->original_height;
	vertices[1].z = 0;

    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic(int x, int y, char* name) 
{
	image_t* img = Draw_FindPic(name);
	if(!img) 
	{
		return;
	}

	Draw_Image(x, y, img);
}

/*
=============
Draw_StrechImage
=============
*/
void Draw_StretchImage(int x, int y, int w, int h, image_t* img) 
{
	GL_Bind(img->texnum);

	typedef struct 
	{
		short s, t;
		short x, y, z;
	} VERT;


	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].s = 0;
	v[0].t = 0;
	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;

	v[1].s = img->width;
	v[1].t = img->height;
	v[1].x = x + w;
	v[1].y = y + h;
	v[1].z = 0;

	sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

/*
=============
Draw_StrechPic
=============
*/
void Draw_StretchPic(int x, int y, int w, int h, char* name) 
{
	image_t* img = Draw_FindPic(name);
	if(!img) 
	{
		return;
	}

	Draw_StretchImage(x, y, w, h, img);
}

/*
=============
Draw_Raw
=============
*/
void GL_ResampleTexture(const byte *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight);
void Draw_Raw(int x, int y, int w, int h, int cols, int rows, byte* data) 
{
    image_t		*stretchimage;
    unsigned char image8[256*256];


	GL_ResampleTexture(data, cols, rows, image8,  256, 256);
	stretchimage = GL_LoadPicLM ("***cinematic***", 256, 256, (byte*)image8, 1, GU_LINEAR, true, it_sprite, 0);
	
	if(!gu_cinscale->value) 
	{
		Draw_StretchImage(x + (480 - cols) / 2, y + (272 - rows) / 2, cols, rows, stretchimage);
	} 
	else 
	{
		Draw_StretchImage(0, 0, 480, 272, stretchimage);
	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}
  
	typedef struct 
	{
		short s, t;
		short x, y, z;
	} VERT;

	GL_Bind (image->texnum);

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].s = x/64.0; 
	v[0].t = y/64.0;
	v[0].x = x; 
	v[0].y = y;
    v[0].z = 0;

	v[1].s = (x+w)/64.0;
	v[1].t = (y+h)/64.0;
	v[1].x = x+w; 
	v[1].y = y+h;
	v[1].z = 0;
 
  sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);

}

/*
=============
Draw_Fade
=============
*/
void Draw_FadeScreen(void) 
{
	sceGuEnable(GU_BLEND);
	sceGuDisable(GU_TEXTURE_2D);
    sceGuColor (GU_COLOR(0, 0, 0, 0.8));
    
    struct vertex
	{
		short	x, y, z;
	};

	vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 2));

	vertices[0].x		= 0;
	vertices[0].y		= 0;
	vertices[0].z		= 0;
	vertices[1].x		= 480;
	vertices[1].y		= 272;
	vertices[1].z		= 0;

	sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D,
		2, 0, vertices);

	sceGuColor (GU_COLOR(1, 1, 1, 1));
	sceGuEnable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
}

/*
=============
Draw_Fill
=============
*/
void Draw_Fill(int x, int y, int w, int h, int c) 
{
    union
	{
		unsigned	c;
		byte		v[4];
	} color;

	if ( (unsigned)c > 255)
		Sys_Error (ERR_FATAL, "Draw_Fill: bad color");

	sceGuDisable(GU_TEXTURE_2D);
	color.c = d_8to24table[c];

	sceGuColor(GU_COLOR(color.v[0]/255.0,
		color.v[1]/255.0,
		color.v[2]/255.0,1));


	typedef struct 
	{
		short x, y, z;
	} VERT;


	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;

	v[1].x = x + w;
	v[1].y = y + h;
	v[1].z = 0;

	sceGumDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);

    sceGuColor (GU_COLOR(1, 1, 1, 1));
	sceGuEnable(GU_TEXTURE_2D);
}

/*
=============
Draw_Char
=============
*/
void Draw_Char(int x, int y, int num) 
{
	num &= 255;
	if((num & 127) == 32) 
	{
		return;
	}
	if(y <= -8) 
	{
		return;
	}

	GL_Bind (draw_chars->texnum);

	typedef struct 
	{
		short s, t;
		short x, y, z;
	} VERT;
	
	short row = (num >> 4) << 3;
	short col = (num & 15) << 3;

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].s = col,
	v[0].t = row;
	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;

	v[1].s = col + 8;
	v[1].t = row + 8;
	v[1].x = x + 8;
	v[1].y = y + 8;
	v[1].z = 0;
	sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}
