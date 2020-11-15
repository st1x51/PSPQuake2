#include "../ref_gu/gu_local.h"
#include "../ref_gu/gu_model.h"
#include "keyboard/danzeff.h"

#include <algorithm>
#include <cstddef>
#include <vram.h>

#ifdef _WIN32
# define ALIGNED(x)
#else
# define ALIGNED(x) __attribute__((aligned(x)))
#endif

namespace quake2
{
	namespace video
	{

		// Types.
		//typedef ScePspRGB565	pixel;
        typedef ScePspRGBA8888	pixel;
		typedef u8				texel;
		typedef u16				depth_value;

		// Constants.
		static const std::size_t	screen_width	 = 480;
		static const std::size_t	screen_height	 = 272;
		static const std::size_t	palette_size	 = 256;
		static pixel* 				display_buffer	 = 0;
		static pixel*				draw_buffer		 = 0;
		static depth_value*			depth_buffer	 = 0;

		//! The GU display list.
		//! @note	Aligned to 64 bytes so it doesn't share a cache line with anything.
		unsigned int ALIGNED(64)	display_list[262144];

	}
}
using namespace quake2;
using namespace quake2::video;

extern int netConnected;

ScePspRGBA8888 ALIGNED(16)	d_8to24table[256];
ScePspRGBA8888 ALIGNED(16)	temptable[256];

int GLimp_SetMode( int *width, int *height)
{
	*width  = screen_width;
	*height = screen_height;

	ri.Vid_NewWindow (screen_width, screen_height);

	return 1;
}

void GLimp_BeginFrame( float camera_separation )
{
    sceGuStart(GU_DIRECT, display_list);
}

void GLimp_EndFrame( void )
{
	 if (danzeff_isinitialized())
     {
	    danzeff_render();
	 }
	 
	 sceGuFinish();
	 sceGuSync(0,0);

	 if(gu_vsync->value || netConnected)
     {
		sceDisplayWaitVblankStart();
	 }

	 // Switch the buffers.
	 sceGuSwapBuffers();
	 std::swap(draw_buffer, display_buffer);
}

int GLimp_Init( void *hinstance, void *hWnd )
{
	display_buffer	= static_cast<pixel*>(valloc((screen_height * 512) * sizeof(pixel)));
	if (!display_buffer)
	{
		Com_Printf("Couldn't allocate display buffer");
	    return 0;
	}
	draw_buffer	= static_cast<pixel*>(valloc((screen_height * 512) * sizeof(pixel)));
	if (!draw_buffer)
	{
		Com_Printf("Couldn't allocate draw buffer");
	    return 0;
	}
	depth_buffer	= static_cast<depth_value*>(valloc((screen_height * 512) * sizeof(depth_value)));
	if (!depth_buffer)
	{
		Com_Printf("Couldn't allocate depth buffer");
	    return 0;
	}

	// Initialise the GU.
	sceGuInit();

	// Set up the GU.
	sceGuStart(GU_DIRECT, display_list);
	{
		sceGuDrawBuffer(GU_PSM_8888, vrelptr(draw_buffer), 512);
		sceGuDispBuffer(screen_width, screen_height, vrelptr(display_buffer), 512);
		sceGuDepthBuffer(vrelptr(depth_buffer), 512);

		// Set the rendering offset and viewport.
		sceGuOffset(2048 - (screen_width / 2), 2048 - (screen_height / 2));
		sceGuViewport(2048, 2048, screen_width, screen_height);

		// Set up scissoring.
		sceGuEnable(GU_SCISSOR_TEST);
		sceGuScissor(0, 0, screen_width, screen_height);

		// Set up texturing.
		sceGuEnable(GU_TEXTURE_2D);
    
		// Set up clearing.
		sceGuClearDepth(65535);
		sceGuClearColor(GU_COLOR(0,0,0,1));

		// Set up depth.
		sceGuDepthRange(0, 65535);
		sceGuDepthFunc(GU_LEQUAL);
		sceGuEnable(GU_DEPTH_TEST);
		
		// Set the matrices.
		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumUpdateMatrix();
		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();
		sceGumUpdateMatrix();
		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();
		sceGumUpdateMatrix();

		// Set up culling.
		sceGuFrontFace(GU_CW);
		sceGuEnable(GU_CULL_FACE);
		sceGuEnable(GU_CLIP_PLANES);
	 
	    // Set up blending.
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	}	
    sceGuFinish();
	sceGuSync(0,0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
	
    vidref_val = VIDREF_GL;

    return 1;
}

void GLimp_Shutdown( void )
{
    sceGuFinish();
	sceGuSync(0,0);
	
	sceGuTerm();
 
 	// Free the buffers.
	vfree(depth_buffer);
	depth_buffer = 0;
	vfree(draw_buffer);
	draw_buffer = 0;
	vfree(display_buffer);
	display_buffer = 0;
}

void GL_SetTexturePalette( unsigned palette[256] )
{
    memcpy(temptable, palette, sizeof(temptable));
	sceGuClutMode(GU_PSM_8888, 0, 255, 0);
	sceKernelDcacheWritebackRange(temptable, sizeof(temptable));
	sceGuClutLoad(256 / 8, temptable);
}

int    GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
  return 0;
}

void		GLimp_AppActivate( qboolean active )
{
}

void		GLimp_EnableLogging( qboolean enable )
{
}

void		GLimp_LogNewFrame( void )
{
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader 
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/* 
================== 
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	byte		*buffer;
	char		picname[80]; 
	char		checkname[MAX_OSPATH];
	int			i;
	FILE		*f;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot", ri.FS_Gamedir());
    Sys_Mkdir (checkname);

// 
// find a file name to save it to 
// 
	strcpy(picname,"quake00.tga");

	for (i=0 ; i<=99 ; i++) 
	{ 
		picname[5] = i/10 + '0'; 
		picname[6] = i%10 + '0'; 
		Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/%s", ri.FS_Gamedir(), picname);
		f = fopen (checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose (f);
	} 

	if (i==100) 
	{
		ri.Con_Printf (PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n"); 
		return;
 	}
	
	buffer = static_cast<byte*>(malloc(screen_width*screen_height*3 + 18));
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = screen_width&255;
	buffer[13] = screen_width>>8;
	buffer[14] = screen_height&255;
	buffer[15] = screen_height>>8;
	buffer[16] = 24;	// pixel size

	// Get the pixels and swap the colours from ARGB to BGR.
	i = 18;
	for (int y = 0; y < screen_height; ++y)
	{
		const pixel* src = display_buffer + ((screen_height - y - 1) * 512);
		for (int x = 0; x < screen_width; ++x)
		{
			const pixel argb = *src++;
		
			buffer[i++]	= (argb >> 16) & 0xff;
			buffer[i++]	= (argb >> 8) & 0xff;
			buffer[i++]	= argb & 0xff;
/*
			// conver from RGB 565 to RGB 888
			buffer[i++]	= ((argb >> 11) & 0x1f) << 3;
			buffer[i++]	= ((argb >> 5) & 0x3f) << 2;
			buffer[i++]	= (argb & 0x1f) << 3;
*/
		}
	}

	f = fopen (checkname, "wb");
	fwrite (buffer, 1, screen_width*screen_height*3 + 18, f);
	fclose (f);

	free (buffer);
	ri.Con_Printf (PRINT_ALL, "Wrote %s\n", picname);
} 
