/*
Quake I Copyright (C) 2005-2006 Peter Mackay
Quake II Copyright (C) 2008-2009 Crow_bar
*/
#include <algorithm>
#include <cstddef>

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <malloc.h>
#include "keyboard/danzeff.h"
#include <vram.h>

#ifdef _WIN32
# define ALIGNED(x)
#else
# define ALIGNED(x) __attribute__((aligned(x)))
#endif

extern "C"
{
#include "../ref_soft/r_local.h"
}

#define DEF_SCREEN_WIDTH  480
#define DEF_SCREEN_HEIGHT 272 
#define MIN_SCREEN_WIDTH  320
#define MIN_SCREEN_HEIGHT 200 
#define MAX_SCREEN_WIDTH  480
#define MAX_SCREEN_HEIGHT 272 

#define DEF_RENDER_WIDTH  320
#define DEF_RENDER_HEIGHT 200 
#define MIN_RENDER_WIDTH  320
#define MIN_RENDER_HEIGHT 200 
#define MAX_RENDER_WIDTH  480
#define MAX_RENDER_HEIGHT 272 

namespace quake2
{
	namespace video
	{
		// Types.
		typedef ScePspRGBA8888	pixel;
		typedef u8				texel;

		// Constants.
		static size_t			screen_width   = DEF_SCREEN_WIDTH;
		static size_t			screen_height  = DEF_SCREEN_HEIGHT;
		static size_t			render_width   = DEF_RENDER_WIDTH;
		static size_t			render_height  = DEF_RENDER_HEIGHT;
		static const size_t		texture_width  = 512;
		static const size_t		texture_height = 512;
		static const size_t		palette_size   = 256;
		static void* const		vramBase       = sceGeEdramGetAddr();
		static pixel* 			displayBuffer  = static_cast<pixel*>(vramBase);
		static pixel* 			backBuffer	   = static_cast<pixel*>(displayBuffer + (512 * screen_height));
		static texel* 			texture0	   = reinterpret_cast<texel*>(displayBuffer + 2 * (512 * screen_height));
		static texel* 			texture1	   = texture0 + (texture_width * texture_height);

		// Regular globals.
		static pixel ALIGNED(64)	texture_palette[palette_size];

		//! The GU display list.
		//! @note	Aligned to 64 bytes so it doesn't share a cache line with anything.
		unsigned int	            *display_list;
	}
}

using namespace quake2;
using namespace quake2::video;

/*
** SWimp_SetPalette
**
** System specific palette setting routine.  A NULL palette means
** to use the existing palette.  The palette is expected to be in
** a padded 4-byte xRGB format.
*/
void SWimp_SetPalette( const unsigned char *palette )
{
	if ( !palette )
	      palette = ( const unsigned char * ) sw_state.currentpalette;

	// Convert the palette to PSP format.
	for(pixel* color = &texture_palette[0]; color < &texture_palette[palette_size]; ++color)
	{
		const unsigned int r = *palette++; //r
		const unsigned int g = *palette++; //g
		const unsigned int b = *palette++; //b
		const unsigned int a = *palette++; //a
		*color = GU_RGBA(r, g, b, a);
	}

	// Upload the palette.
	sceGuClutMode(GU_PSM_8888, 0, palette_size - 1, 0);
	sceKernelDcacheWritebackRange(texture_palette, sizeof(texture_palette));
	sceGuClutLoad(palette_size / 8, texture_palette);
}

int SWimp_Init( void *hInstance, void *wndProc )
{
	render_width  = MIN_RENDER_WIDTH;
	render_height = MIN_RENDER_HEIGHT;
	screen_width  = MAX_SCREEN_WIDTH;
	screen_height = MAX_SCREEN_HEIGHT;
	
    display_list = (unsigned int *)memalign(64, sizeof(int) * 128 * 1024);
	if(!display_list)
	{
		Sys_Error("No memory for displayList\n");
    }
    
	// Initialise the GU.
	sceGuInit();
	
	// Set up the GU.
	sceGuStart(GU_DIRECT, display_list);
	{
		// Set the draw and display buffers.
		void* const displayBufferInVRAM = reinterpret_cast<char*>(displayBuffer) - reinterpret_cast<size_t>(vramBase);
		void* const backBufferInVRAM = reinterpret_cast<char*>(backBuffer) - reinterpret_cast<size_t>(vramBase);

		sceGuDrawBuffer(GU_PSM_8888, backBufferInVRAM, 512);
		sceGuDispBuffer(MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, displayBufferInVRAM, 512);

		// Set the rendering offset and viewport.
		sceGuOffset(2048 - (MAX_SCREEN_WIDTH / 2), 2048 - (MAX_SCREEN_HEIGHT / 2));
		sceGuViewport(2048, 2048, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT);

		// Set up scissoring.
		sceGuEnable(GU_SCISSOR_TEST);
		sceGuScissor(0, 0, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT);

		// Set up texturing.
		sceGuEnable(GU_TEXTURE_2D);
		sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
		sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	}
	sceGuFinish();
	sceGuSync(0,0);

	// Turn on the display.
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	return true;
}

static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	// Set up Quake's video parameters.
	vid.buffer			= texture0;
	vid.rowbytes		= texture_width;
	vid.width			= render_width;
	vid.height			= render_height;
	
	Com_Printf ("Rendered  at ( %d x %d ) \n",vid.width, vid.height);
	Com_Printf ("Displayed at ( %d x %d ) \n",screen_width, screen_height);
	
	// This is an awful hack to get around a division by zero later on...
	r_refdef.vrect.width	= render_width;
	r_refdef.vrect.height	= render_height;

	// Start a render.
	sceGuStart(GU_DIRECT, display_list);

  return true;
}

/*
** SWimp_SetMode
*/
rserr_t SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	rserr_t retval = rserr_ok;

	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );
	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics( fullscreen ) )
	{
		// failed to set a valid mode in windowed mode
		printf("failed to set a valid mode in windowed mode\n");
		return rserr_invalid_mode;
	}

    ri.Vid_NewWindow (*pwidth, *pheight);

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );
	
	return retval;
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/
void SWimp_Shutdown(void)
{
	// Finish rendering.
	sceGuFinish();
	sceGuSync(0,0);

	// Shut down the display.
	sceGuTerm();
	
	// Free the buffers.
	if(display_list)
	   free(display_list);
}

void SWimp_EndFrame (void)
{
	if (danzeff_isinitialized())
    {
	  danzeff_render();
	}
	
    // Finish rendering.
	sceGuFinish();
	sceGuSync(0, 0);

	//sceDisplayWaitVblankStart();

	// Switch the buffers.
	sceGuSwapBuffers();

	// Start a new render.
	sceGuStart(GU_DIRECT, display_list);

    // Allocate memory in the display list for some vertices.
	struct Vertex
	{
		SceUShort16	u, v;
		SceShort16	x, y, z;
	} * const vertices = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * 2));

	int dif_x = (MAX_SCREEN_WIDTH/2)  - (screen_width/2);
	int dif_y = (MAX_SCREEN_HEIGHT/2) - (screen_height/2);


	// Set up the vertices.
	vertices[0].u	= 0;
	vertices[0].v	= 0;
	vertices[0].x	= dif_x;
	vertices[0].y	= dif_y;
	vertices[0].z	= 0;
	vertices[1].u	= render_width;
	vertices[1].v	= render_height;
	vertices[1].x	= screen_width + dif_x;
	vertices[1].y	= screen_height + dif_y;
	vertices[1].z	= 0;

	// Flush the data cache.
	sceKernelDcacheWritebackRange(vid.buffer, texture_width * texture_height);

	// Set the texture.
	sceGuTexImage(0, texture_width, texture_height, texture_width, vid.buffer);

	// Draw the sprite.
	sceGuDrawArray(
		GU_SPRITES,
		GU_VERTEX_16BIT | GU_TEXTURE_16BIT | GU_TRANSFORM_2D,
		2,
		0,
		vertices);

	// Swap Quake's buffers.
	texel* const newTexture = (vid.buffer == texture0) ? texture1 : texture0;
	vid.buffer	= newTexture;
}

/*
** SWimp_AppActivate
*/
void SWimp_AppActivate( qboolean active )
{
}
