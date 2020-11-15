#include <stdio.h>
#include <math.h>

#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspkernel.h>

extern"C"
{
#include "../client/ref.h"
}

#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	unsigned		width, height;			// coordinates from main game
} viddef_t;
#endif

extern	viddef_t	vid;

extern	unsigned	d_8to24table[256];

typedef vec_t vec2_t[2];

typedef struct glvert_s
{
	vec2_t	st;
	vec3_t	xyz;
} glvert_t;

typedef struct glpoly_s
{
	struct		glpoly_s	*next;
	struct		glpoly_s	*chain;
	int			numverts;
	int			flags;		// for SURF_UNDERWATER

	// This is a variable sized array, and hence must be the last element in
	// this structure.
	//
	// The array is (numverts * 2) in size. The first half are regular
	// vertices, and the second half have copies of the first half's XYZs but
	// keep the light map texture coordinates. This makes the vertices easier
	// to render on the PSP.
	glvert_t	verts[1];
} glpoly_t;	

typedef enum imagetype_e 
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef byte texel;

typedef struct image_s 
{
	 // Source.
	char name[MAX_QPATH];
	imagetype_t type;
	int	  registration_sequence;
	
	// Source.
	char	identifier[64];
	int		original_width;
	int		original_height;
    int	    stretch_to_power_of_two;
    int		texnum;		// gl texture binding
    int     swizzle;  // For speed up
    int     bpp; // Bit per pixel

	// Texture description.
	int		format;
	int		filter;
	int		width;
	int		height;
	int 	mipmaps;

	// Buffers.
	texel*	ram;
	texel*	vram;
	struct msurface_s* texturechain;
} image_t;

extern	image_t		*r_notexture;
extern	image_t		*r_particletexture;

#define	MAX_TEXTURES 1024

extern int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern refdef_t r_newrefdef;
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

extern cvar_t* r_norefresh;
extern cvar_t* r_lefthand;
extern cvar_t* r_drawentities;
extern cvar_t* r_drawworld;
extern cvar_t* r_speeds;
extern cvar_t* r_fullbright;
extern cvar_t* r_novis;
extern cvar_t* r_nocull;
extern cvar_t* r_lerpmodels;

extern cvar_t* r_lightlevel;

extern cvar_t* gu_vsync;
extern cvar_t* gu_tgasky;
extern cvar_t* gu_particletype;
extern cvar_t* gu_subdivide;
extern cvar_t* gu_cinscale;
extern cvar_t* gu_clippingepsilon;
extern cvar_t* vid_gamma;
extern cvar_t* vid_out;
extern cvar_t* gu_mipmaps_func;
extern cvar_t* gu_mipmaps_bias;
extern cvar_t* gu_mipmaps;
extern cvar_t* gu_tex_scale_down;
extern cvar_t* gu_monolightmap;
extern cvar_t* gu_lightmap_mode;
extern cvar_t* gu_lightmap;
extern cvar_t* gu_modulate;
extern cvar_t* gu_dynamic;
extern cvar_t* gu_shadows;

#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01f

#define SCREENSHOT_PATH "ms0:/PICTURE/quake2/"

void Draw_InitLocal (void);
image_t* Draw_FindPic(char *name);

void Draw_GetPicSize(int* w, int* h, char* name);
void Draw_Pic(int x, int y, char* name);
void Draw_Image(int x, int y, const image_t* img);
void Draw_StretchPic(int x, int y, int w, int h, char* name);
void Draw_StretchImage(int x, int y, int w, int h, const image_t* img);
void Draw_Raw(int x, int y, int w, int h, int cols, int rows, byte* data);
void Draw_TileClear(int x, int y, int w, int h, char* name);
void Draw_Char(int x, int y, int num);
void Draw_Text(int x, int y, const char* text);
void Draw_Fill(int x, int y, int w, int h, int c);
void Draw_FadeScreen(void);

/*
 *
 */

void R_RotateForEntity(entity_t* e);

void R_DrawAliasModel(entity_t* e);

void R_DrawBrushModel(entity_t* e);

/**
 *
 */
 
void GU_View2D(void);
void GU_View3D(void);

void GU_DefaultState(void);

void GU_BindPalette(const unsigned char* palette);

void GU_Screenshot(const char* filename);

void GU_RotateForEntity(entity_t* e);

extern image_t* lightmap_textures[128];
extern int registration_sequence;
extern int currenttexture;

void     GL_UnloadTexture(int texture_index);
image_t	*GL_FindImage (char *name, imagetype_t type);
void     GL_Bind (int texture_index);
image_t *GL_LoadPic (const char *identifier, int width, int height, const byte *data, qboolean stretch_to_power_of_two, int filter, int mipmap_level, imagetype_t type, int bits);
image_t *GL_LoadPicLM (const char *identifier, int width, int height, const byte *data, int bpp, int filter, qboolean update, imagetype_t type, int dynamic);
void     GL_FreeUnusedImages (void);
void	 GL_InitImages (void);
void	 GL_ShutdownImages (void);
void     GL_SetTexturePalette( unsigned palette[256] );
unsigned char GL_GetTexGamma(int i);
void     GL_UpdateGamma(void);

void R_BeginRegistration(const char* world);
void R_EndRegistration(void);

void R_InitParticleTexture(void);

struct image_s* R_RegisterSkin(const char *name);
struct model_s* R_RegisterModel(const char *name);
void            R_RegisterSky(const char* name, float rotate, vec3_t axis);

void R_InitImages(void);
void R_ShutdownImages(void);
void R_SetPalette ( const unsigned char *palette);

void R_DrawSkyBox(void);
void R_ClearSkyBox(void);

void R_DrawParticles(void);

void R_LightPoint (vec3_t p, vec3_t color);

void R_Screenshot_f(void);

void R_MarkLeaves(void);
void R_DrawWorld(void);

extern	int		c_brush_polys, c_alias_polys;
extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern refimport_t ri;

void		GLimp_BeginFrame( float camera_separation );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
int     	GLimp_SetMode( int *width, int *height );
void		GLimp_AppActivate( qboolean active );
void		GLimp_EnableLogging( qboolean enable );
void		GLimp_LogNewFrame( void );
extern "C" void		RW_InitKey(void);
extern "C" void		RW_ShutdownKey(void);

