/*
** RW_SDL.C
**
** This file contains ALL Linux specific stuff having to do with the
** software refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** SWimp_EndFrame
** SWimp_Init
** SWimp_InitGraphics
** SWimp_SetPalette
** SWimp_Shutdown
** SWimp_SwitchFullscreen
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "SDL/SDL.h"
#include "../ref_soft/r_local.h"
#include "keyboard/danzeff.h"

static unsigned int sdl_palettemode;

static qboolean                 X11_active = false;

static SDL_Surface *surface;

viddef_t	viddef;
extern int psp_width;
extern int psp_height;

/*****************************************************************************/

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
int SWimp_Init( void *hInstance, void *wndProc )
{
	vid.width  = psp_width;
	vid.height = psp_height;				

	viddef = vid;

  vidref_val = VIDREF_SOFT;

  if (SDL_WasInit(SDL_INIT_AUDIO|SDL_INIT_CDROM|SDL_INIT_VIDEO) == 0) 
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0) 
		{
			Sys_Error("SDL Init failed: %s\n", SDL_GetError());
			return false;
		}
	} 
	else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) 
	{
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) 
		{
			Sys_Error("SDL Init failed: %s\n", SDL_GetError());
			return false;
		}
	}
  
//	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
	return true;
}

static void SetSDLIcon()
{
}

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	const SDL_VideoInfo *vinfo;
	int flags;

	/* Just toggle fullscreen if that's all that has been changed */	
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) 
	{
		int isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);
	
		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen == isfullscreen)
			return true;
	}
	  
	// free resources in use
	if (surface)
	{
		printf("surface clear\n");
		SDL_FreeSurface(surface);
  } 
/* 
	Okay, I am going to query SDL for the "best" pixel format.
	If the depth is not 8, use SetPalette with logical pal, 
	else use SetColors.
	
	Hopefully this works all the time.
*/
	vinfo = SDL_GetVideoInfo();
	sdl_palettemode = (vinfo->vfmt->BitsPerPixel == 8) ? (SDL_PHYSPAL|SDL_LOGPAL) : SDL_LOGPAL;
	flags = SDL_SWSURFACE;//| SDL_HWPALETTE;
	if (fullscreen)
	{
		printf("fullscreen mode\n");
		flags |= SDL_FULLSCREEN;
	}
	SetSDLIcon();
	
	//Sys_Error("Test error:");

	if ((surface = SDL_SetVideoMode(vid.width, vid.height, 8, flags)) == NULL) 
	{
		Sys_Error("(SOFTSDL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return false;
	}
	SDL_WM_SetCaption("Quake II", "Quake II");

	SDL_ShowCursor(0);

  vid.rowbytes = surface->pitch;
	vid.buffer = surface->pixels;

	X11_active = true;

	return true;
}

void RW_InitKey (void)
{
    danzeff_load(); 
    danzef_set_screen(surface);
    danzeff_moveTo(170,0);
}

void RW_ShutdownKey (void)
{
    danzeff_free();
}

/*is does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/
void SWimp_EndFrame (void)
{
	  /*SDL_Flip(surface);*/
    if (danzeff_isinitialized())
    {			 
	    danzeff_render();
		} 
		SDL_UpdateRect(surface, 0, 0, 0, 0);
    
#if (defined(PSP_PSPOSK) && PSP_PSPOSK > 0)
	if(sceUtilityOskGetStatus() == OSK_VISIBLE) 
	{
		sceUtilityOskUpdate(2);
	  sceDisplayWaitVblankStart();
	}
#endif

}

/*
** SWimp_SetMode
*/
rserr_t SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	rserr_t retval = rserr_ok;

	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

//	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
//	{
//		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
//		return rserr_invalid_mode;
//	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics( fullscreen ) ) {
		// failed to set a valid mode in windowed mode
		printf("failed to set a valid mode in windowed mode\n");
		return rserr_invalid_mode;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );
	
	return retval;
}

/*
** SWimp_SetPalette
**
** System specific palette setting routine.  A NULL palette means
** to use the existing palette.  The palette is expected to be in
** a padded 4-byte xRGB format.
*/
void SWimp_SetPalette( const unsigned char *palette )
{
	SDL_Color colors[256];
	
	int i;

	if (!X11_active)
		return;

	if ( !palette )
	        palette = ( const unsigned char * ) sw_state.currentpalette;
 
	for (i = 0; i < 256; i++) {
		colors[i].r = palette[i*4+0];
		colors[i].g = palette[i*4+1];
		colors[i].b = palette[i*4+2];
	}

	SDL_SetPalette(surface, sdl_palettemode, colors, 0, 256);
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/

void SWimp_Shutdown( void )
{
	printf("Shutdown Graphic");
  
	if (surface)
		SDL_FreeSurface(surface);
	
	surface = NULL;
	
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		
	X11_active = false;
}

/*
** SWimp_AppActivate
*/
void SWimp_AppActivate( qboolean active )
{
}


