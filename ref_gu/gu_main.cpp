#include "gu_local.h"
#include "gu_model.h"
#include "clipping.hpp"
#include <valloc.h>

using namespace quake;

int			c_brush_polys, c_alias_polys;

image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles

refimport_t ri;

ScePspFMatrix4	r_world_matrix;

model_t* r_worldmodel;

void R_Clear (void);

viddef_t	vid;

refdef_t r_newrefdef;

vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

entity_t* currententity;
model_t*  currentmodel;

cplane_t frustum[4];

int r_visframecount = 0;
int r_framecount = 0;

float v_blend[4];

cvar_t* r_norefresh;
cvar_t* r_drawentities;
cvar_t* r_drawworld;
cvar_t* r_speeds;
cvar_t* r_fullbright;
cvar_t* r_novis;
cvar_t* r_nocull;
cvar_t* r_lerpmodels;
cvar_t* r_lefthand;

cvar_t* gu_vsync;
cvar_t* gu_shadows;
cvar_t* vid_gamma;
cvar_t* vid_out;
cvar_t* gu_particletype;
cvar_t* gu_subdivide;
cvar_t* gu_cinscale;
cvar_t*	gu_tex_scale_down;

cvar_t* r_lightlevel;	// FIXME: This is a HACK to get the client's light level

cvar_t* gu_mipmaps_func;
cvar_t* gu_mipmaps_bias;
cvar_t* gu_mipmaps;

cvar_t* gu_monolightmap;
cvar_t* gu_lightmap_mode;
cvar_t* gu_lightmap;
cvar_t* gu_modulate;

cvar_t* gu_dynamic;
cvar_t* gu_tgasky;
cvar_t* gu_beamseg;
cvar_t* gu_beamtype;

int	r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

static qboolean r_colorclear = true;

unsigned r_rawpalette[256];

void GL_ScreenShot_f (void) ;
void BuildGammaTable (float g);

void R_Register( void )
{
	r_lefthand         = ri.Cvar_Get("hand",              "0",  CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh        = ri.Cvar_Get("r_norefresh",       "0",  0);
	r_fullbright       = ri.Cvar_Get("r_fullbright",      "0",  0);
	r_drawentities     = ri.Cvar_Get("r_drawentities",    "1",  0);
	r_drawworld        = ri.Cvar_Get("r_drawworld",       "1",  0);
	r_novis            = ri.Cvar_Get("r_novis",           "0",  0);
	r_nocull           = ri.Cvar_Get("r_nocull",          "0",  0);
	r_lerpmodels       = ri.Cvar_Get("r_lerpmodels",      "1",  0);
    r_speeds           = ri.Cvar_Get("r_speeds",          "0",  0);

    r_lightlevel       = ri.Cvar_Get("r_lightlevel",      "0",   0);

	gu_vsync           = ri.Cvar_Get("gu_vsync",          "0",   CVAR_ARCHIVE);
	gu_shadows         = ri.Cvar_Get("gu_shadows",        "0",   CVAR_ARCHIVE);
	gu_particletype    = ri.Cvar_Get("gu_particletype",   "1",   CVAR_ARCHIVE);
	gu_subdivide       = ri.Cvar_Get("gu_subdivide",      "1",   CVAR_ARCHIVE);
	gu_cinscale        = ri.Cvar_Get("gu_cinscale",       "1",   CVAR_ARCHIVE);
    gu_tex_scale_down  = ri.Cvar_Get("gu_tex_scale_down", "1",   CVAR_ARCHIVE);
    gu_mipmaps_func    = ri.Cvar_Get("gu_mipmaps_func",   "2",   CVAR_ARCHIVE);
    gu_mipmaps_bias    = ri.Cvar_Get("gu_mipmaps_bias",   "-6",  CVAR_ARCHIVE);
    gu_mipmaps         = ri.Cvar_Get("gu_mipmaps",        "1",   CVAR_ARCHIVE);
    gu_monolightmap    = ri.Cvar_Get("gu_monolightmap",   "0",   CVAR_ARCHIVE);
    gu_lightmap        = ri.Cvar_Get("gu_lightmap",       "0",   CVAR_ARCHIVE);
#ifdef PSP_PHAT
    gu_lightmap_mode   = ri.Cvar_Get("gu_lightmap_mode",  "6",   CVAR_ARCHIVE); //DXT5 format for lightmaps
#else
    gu_lightmap_mode   = ri.Cvar_Get("gu_lightmap_mode",  "2",   CVAR_ARCHIVE);
#endif
    gu_modulate        = ri.Cvar_Get("gl_modulate",       "1",   CVAR_ARCHIVE);
	gu_dynamic         = ri.Cvar_Get("gu_dynamic",        "1",   CVAR_ARCHIVE);
	gu_tgasky          = ri.Cvar_Get("gu_tgasky",         "0",   CVAR_ARCHIVE);
	gu_beamseg         = ri.Cvar_Get("gu_beamseg",        "1",   CVAR_ARCHIVE);
	gu_beamtype        = ri.Cvar_Get("gu_beamtype",       "1",   CVAR_ARCHIVE);
	vid_gamma          = ri.Cvar_Get("vid_gamma",         "0.7", CVAR_ARCHIVE);
	vid_out            = ri.Cvar_Get("vid_out",           "0",   CVAR_ARCHIVE);

	ri.Cmd_AddCommand("screenshot", GL_ScreenShot_f);
}

int R_Init(void) 
{
    int		j;
	extern float r_turbsin[256];
	for ( j = 0; j < 256; j++ )
	{
		r_turbsin[j] *= 0.5;
	}
	
	R_Register();

    // initialize OS-specific parts of OpenGL
	if( !GLimp_Init( 0, 0 ))
	{
		Sys_Error("Could init GU Driver\n");
		return -1;
	}
    if( !GLimp_SetMode( (int*)&vid.width, (int*)&vid.height ) )
    {
        Com_Printf ("ref_gl::R_Init() - could not GLimp_SetMode()\n");
		return -1;
	}
	
	GU_DefaultState();

	GL_InitImages();

	Mod_Init();

	R_InitParticleTexture ();

	Draw_InitLocal ();

	return 1;
}

void R_Shutdown(void) 
{
	ri.Cmd_RemoveCommand("screenshot");

	Mod_Shutdown();

	GL_ShutdownImages();
    /*
	** shut down OS specific OpenGL stuff like contexts, etc.
	*/
	GLimp_Shutdown();
}

void R_BeginFrame(float camera_separation ) 
{
	if ( vid_gamma->modified ) //gammatable update
	{
		GL_UpdateGamma();
		R_SetPalette (NULL);
	    vid_gamma->modified = false;
	}

 	GLimp_BeginFrame( camera_separation );

	GU_DefaultState();

	GU_View2D();
  
	R_Clear ();
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame(void) 
{
	int i;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	if(!(r_newrefdef.rdflags & RDF_NOWORLDMODEL)) 
	{
		mleaf_t	*leaf;
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if(!leaf->contents) 
		{ // look down a bit
			vec3_t temp;
			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if(!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2)) 
			{
				r_viewcluster2 = leaf->cluster;
			}
		} 
		else 
		{ // look up a bit
			vec3_t temp;
			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if(!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2)) 
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (i=0 ; i<4 ; i++) 
	{
		v_blend[i] = r_newrefdef.blend[i];
	}
  
	c_brush_polys = 0;
	c_alias_polys = 0;
	
	// clear out the portion of the screen that the NOWORLDMODEL defines
	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL) 
	{
		sceGuEnable(GU_SCISSOR_TEST);
		sceGuClearColor(GU_COLOR(0.3,0.3,0.3,1));
		sceGuScissor(r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		//sceGuClearColor(GU_COLOR( 1, 0, 0.5, 0.5 ));
		sceGuClearColor(GU_COLOR(0,0,0,1)); //black
		sceGuDisable(GU_SCISSOR_TEST);
	}
}

int SignbitsForPlane (cplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

#if 0
	/*
	** this code is wrong, since it presume a 90 degree FOV both in the
	** horizontal and vertical plane
	*/
	// front side is visible
	VectorAdd (vpn, vright, frustum[0].normal);
	VectorSubtract (vpn, vright, frustum[1].normal);
	VectorAdd (vpn, vup, frustum[2].normal);
	VectorSubtract (vpn, vup, frustum[3].normal);

	// we theoretically don't need to normalize these vectors, but I do it
	// anyway so that debugging is a little easier
	VectorNormalize( frustum[0].normal );
	VectorNormalize( frustum[1].normal );
	VectorNormalize( frustum[2].normal );
	VectorNormalize( frustum[3].normal );
#else
	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2 ) );
#endif

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (void)
{
	vec3_t	shadelight;
    int     index;
	int		i;
	
	if ( currententity->flags & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0f;
	else
		R_LightPoint (currententity->origin, shadelight);

    sceGumPushMatrix();

	GU_RotateForEntity (currententity);

	sceGuDisable (GU_TEXTURE_2D);
	sceGuColor (GU_COLOR(shadelight[0],shadelight[1],shadelight[2], 1.0f));

    // Allocate the vertices.
	struct vertex
	{
		float x, y, z;
	};
    vertex* vertices;
    
	vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 6));
	vertices[0].x = 0;
	vertices[0].y = 0;
	vertices[0].z = 16;
	index = 1;
	for (i=0 ; i<=4 ; i++)
	{
        vertices[index].x = 16 * cos( i * M_PI/2 );
	    vertices[index].y = 16 * sin( i * M_PI/2 );
	    vertices[index].z = 0;
	    index++;
    }
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, index, 0, vertices);

	vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 6));
	vertices[0].x = 0;
	vertices[0].y = 0;
	vertices[0].z = 16;
	index = 1;
	for (i=4 ; i>=0 ; i--)
	{
		vertices[index].x = 16 * cos( i * M_PI/2 );
	    vertices[index].y = 16 * sin( i * M_PI/2 );
	    vertices[index].z = 0;
	    index++;
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, index, 0, vertices);

	sceGuColor(0xffffffff);

	sceGuEnable(GU_TEXTURE_2D);
	sceGumPopMatrix();
}

#define NUM_BEAM_SEGS 6

void R_DrawLine(vec3_t start_points, vec3_t end_points, int num_seg)
{
	vec3_t dir, point;
	float length;
	float step;
	int   i;

	VectorSubtract( end_points, start_points, dir );
	length = VectorLength( dir );
    VectorNormalize( dir );
    step = length / num_seg;
    
	// Allocate the vertices.
	struct vertex
	{
		float x, y, z;
	};
    vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * (num_seg+1)));
	vertices[0].x = start_points[0];
	vertices[0].y = start_points[1];
	vertices[0].z = start_points[2];

	VectorCopy(start_points, point);

	for(i = 1; i < (num_seg+1); i++)
	{
	    VectorMA (point, step, dir, point);

		vertices[i].x = point[0];
	    vertices[i].y = point[1];
	    vertices[i].z = point[2];
	}

	sceGuDrawArray(GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_3D, (num_seg+1), 0, vertices);
}

void R_DrawTriangle(vec3_t	*start_points, vec3_t *end_points, int num_seg)
{
    int i;
	int index = 0;
	// Allocate the vertices.
	struct vertex
	{
		float x, y, z;
	};

    vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * num_seg * 2));

	for(i = 0; i < num_seg; i++)
	{
		vertices[index].x = start_points[i][0];
		vertices[index].y = start_points[i][1];
		vertices[index].z = start_points[i][2];
		index++;

		vertices[index].x = end_points[(i+1)%num_seg][0];
		vertices[index].y = end_points[(i+1)%num_seg][1];
		vertices[index].z = end_points[(i+1)%num_seg][2];
		index++;
	}
	sceGumDrawArray(GU_TRIANGLE_STRIP, GU_VERTEX_32BITF | GU_TRANSFORM_3D, num_seg * 2, 0, vertices);
}

void R_DrawBeam(entity_t* e) 
{
	vec3_t perpvec;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t direction, normalized_direction;
	vec3_t oldorigin, origin;

	float r, g, b;
	int i;

	int beam_type = int( gu_beamtype->value );
    int num_seg   = int( gu_beamseg->value  );

    if(num_seg < 1)
	{
		Cvar_SetValue("gu_beamseg", 1);
	}
	if(num_seg > NUM_BEAM_SEGS)
	{
       Cvar_SetValue("gu_beamseg", NUM_BEAM_SEGS);
	}

    if(beam_type < 1)
    {
		oldorigin[0] = e->oldorigin[0];
		oldorigin[1] = e->oldorigin[1];
		oldorigin[2] = e->oldorigin[2];

		origin[0] = e->origin[0];
		origin[1] = e->origin[1];
		origin[2] = e->origin[2];

		normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
		normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
		normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

		if ( VectorNormalize( normalized_direction ) == 0 )
			return;

		for ( i = 0; i < num_seg; i++ )
		{
			RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/num_seg)*i );
			VectorAdd( start_points[i], origin, start_points[i] );
			VectorAdd( start_points[i], direction, end_points[i] );
		}

		PerpendicularVector( perpvec, normalized_direction );
		VectorScale( perpvec, e->frame / 2, perpvec );
    }

	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_BLEND);
	sceGuDepthMask( GU_TRUE );

	r = ( d_8to24table[e->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24table[e->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24table[e->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;
	sceGuColor(GU_COLOR( r, g, b, e->alpha ));

	if(beam_type < 1)
	{
    	R_DrawTriangle(start_points, end_points, num_seg);
    }
	else
	{
		R_DrawLine(e->oldorigin, e->origin, num_seg);
	}

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuDepthMask( GU_FALSE );

	sceGuColor (0xffffffff);
}

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	float alpha = 1.0F;
	vec3_t	point;
	dsprframe_t	*frame;
	float		*up, *right;
	dsprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (dsprite_t *)currentmodel->extradata;

#if 0
	if (e->frame < 0 || e->frame >= psprite->numframes)
	{
		ri.Con_Printf (PRINT_ALL, "no such sprite frame %i\n", e->frame);
		e->frame = 0;
	}
#endif
	e->frame %= psprite->numframes;

	frame = &psprite->frames[e->frame];

#if 0
	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
	vec3_t		v_forward, v_right, v_up;

	AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
#endif
	{	// normal sprite
		up = vup;
		right = vright;
	}

	if ( e->flags & RF_TRANSLUCENT )
		alpha = e->alpha;

	if ( alpha != 1.0F )
		sceGuEnable( GU_BLEND );

    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuColor(GU_COLOR(1, 1, 1, alpha));

    GL_Bind(currentmodel->skins[e->frame]->texnum);

/*
	if ( alpha == 1.0 )
		sceGuEnable (GU_ALPHA_TEST);
	else
		sceGuDisable( GU_ALPHA_TEST );
*/
	// Allocate memory for this polygon.
	glvert_t* const	vertices =
		static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * 4));

	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
  
	vertices[0].st[0]	= 0.0f;
	vertices[0].st[1]	= 1.0f;
	vertices[0].xyz[0]	= point[0];
	vertices[0].xyz[1]	= point[1];
	vertices[0].xyz[2]	= point[2];

	
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
  
	vertices[1].st[0]	= 0.0f;
	vertices[1].st[1]	= 0.0f;
	vertices[1].xyz[0]	= point[0];
	vertices[1].xyz[1]	= point[1];
	vertices[1].xyz[2]	= point[2];

	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	
	vertices[2].st[0]	= 1.0f;
	vertices[2].st[1]	= 0.0f;
	vertices[2].xyz[0]	= point[0];
	vertices[2].xyz[1]	= point[1];
	vertices[2].xyz[2]	= point[2];


	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
 
    vertices[3].st[0]	= 1.0f;
	vertices[3].st[1]	= 1.0f;
	vertices[3].xyz[0]	= point[0];
	vertices[3].xyz[1]	= point[1];
	vertices[3].xyz[2]	= point[2];
  
	// Draw the clipped vertices.
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 4, 0, vertices);

	//sceGuDisable (GU_ALPHA_TEST);

	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);

	if ( alpha != 1.0F )
		sceGuDisable( GU_BLEND );

	sceGuColor(0xffffffff);
}


/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList(void) 
{
	int i;

	if (!r_drawentities->value)
		return;

	for(i = 0; i < r_newrefdef.num_entities; i++) 
	{
		currententity = &r_newrefdef.entities[i];

		if(currententity->flags & RF_TRANSLUCENT) 
		{
			continue;
		}

#if 0 // REMOVE fixes bug already is in original engine
		if(currententity->flags & RF_BEAM) 
		{
			R_DrawBeam(currententity);
			continue;
		}
#endif

		currentmodel = currententity->model;
		if(!currentmodel) 
		{
			R_DrawNullModel();
			continue;
		}

		switch(currentmodel->type) 
		{
		case mod_alias:
			R_DrawAliasModel(currententity);
			break;
		
		case mod_brush:
			R_DrawBrushModel(currententity);
			break;

		case mod_sprite:
			R_DrawSpriteModel(currententity);
			break;

		default:
			Sys_Error("Bad modeltype");
			break;
		}
	}

	sceGuDepthMask(GU_TRUE);

	for(i = 0; i < r_newrefdef.num_entities; i++) 
	{
		currententity = &r_newrefdef.entities[i];

		if(!(currententity->flags & RF_TRANSLUCENT)) 
		{
			continue;
		}

		if(currententity->flags & RF_BEAM) 
		{
			R_DrawBeam(currententity);
			continue;
		}

		currentmodel = currententity->model;
		if(!currentmodel) 
		{
			R_DrawNullModel();
			continue;
		}

		switch(currentmodel->type) 
		{
		case mod_alias:
			R_DrawAliasModel(currententity);
			break;
			
		case mod_brush:
			R_DrawBrushModel(currententity);
			break;

		case mod_sprite:
			R_DrawSpriteModel(currententity);
			break;

		default:
			Sys_Error("Bad modeltype");
			break;
		}
	}

	sceGuDepthMask(GU_FALSE);
}

void R_PushDlights();
void R_RenderDlights ();

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!v_blend[3])
		return;
  
    sceGuEnable(GU_BLEND);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGB);

	sceGumLoadIdentity();

    struct vertex
	{
		short	x, y, z;
	};

	vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 2));

	vertices[0].x		= 0;
	vertices[0].y		= 0;
	vertices[0].z		= 0;
	vertices[1].x		= vid.width;
	vertices[1].y		= vid.height;
	vertices[1].z		= 0;
 
	sceGuColor (GU_COLOR(v_blend[0],v_blend[1],v_blend[2],v_blend[3]));
	sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
    sceGuColor (0xffffffff);

	sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGB);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
}

/*
================
R_RenderView
================
*/
void R_RenderView(refdef_t* fd) 
{

	if(r_norefresh->value)
		return;

	r_newrefdef = *fd;


	//if(!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
	//	Sys_Error ("R_RenderView: NULL worldmodel");


	if(r_speeds->value) 
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	R_PushDlights();

	R_SetupFrame();

	R_SetFrustum();

	GU_View3D();
 
 	R_MarkLeaves();

	R_DrawWorld();

	R_DrawEntitiesOnList();

    R_RenderDlights ();

	R_DrawParticles();
	
	void R_DrawAlphaSurfaces();

	R_DrawAlphaSurfaces();
 
    R_PolyBlend ();

	if (r_speeds->value)
	{
		ri.Con_Printf (PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys, 
			c_alias_polys, 
			c_visible_textures, 
			c_visible_lightmaps); 
	}
}

/*
================
GU_View2D
================
*/
void GU_View2D(void) 
{
	sceGuOffset(2048 - (vid.width/2), 2048 - (vid.height/2));
	sceGuViewport(2048, 2048, vid.width, vid.height);
	
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(0, 0, vid.width, vid.height);
	
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
    sceGumUpdateMatrix();
  
	sceGumOrtho(0.0f, vid.width, vid.height, 0.0f, -1000.0f, 1000.0f);

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
    sceGumUpdateMatrix();
	
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
	sceGumUpdateMatrix();
	
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB); //////2d
	sceGuDisable(GU_CULL_FACE);

	sceGuEnable(GU_COLOR_TEST);
}

/*
================
GU_View3D
================
*/
void GU_View3D(void) 
{
	int x, x2, y2, y, w, h;

	x  = (int)floorf(r_newrefdef.x * vid.width / vid.width);
	x2 = (int)ceilf((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y  = (int)floorf(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = (int)ceilf (vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

  // fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < vid.width)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < vid.height)
		y++;

	w = x2 - x;
	h = y - y2;

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();

	sceGuViewport(
		2048,
		2048 + (vid.height / 2) - y2 - (h / 2),
		w,
		h);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(x, vid.height - y2 - h, x + w, vid.height - y2);
	
	sceGumPerspective(r_newrefdef.fov_y, (float)r_newrefdef.width/r_newrefdef.height, 4, 4096);
    sceGumUpdateMatrix();

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumRotateX(-90 * (GU_PI / 180.0f));
	sceGumRotateZ(90 * (GU_PI / 180.0f));
	sceGumRotateX(-r_newrefdef.viewangles[2] * (GU_PI / 180.0f));
	sceGumRotateY(-r_newrefdef.viewangles[0] * (GU_PI / 180.0f));
	sceGumRotateZ(-r_newrefdef.viewangles[1] * (GU_PI / 180.0f));
	const ScePspFVector3 translation =
	{
		-r_newrefdef.vieworg[0],
		-r_newrefdef.vieworg[1],
		-r_newrefdef.vieworg[2]
	};
	sceGumTranslate(&translation);

    sceGumStoreMatrix(&r_world_matrix);
	sceGumUpdateMatrix();

	sceGumMatrixMode(GU_MODEL);
 
clipping::begin_frame();

	sceGuEnable(GU_DEPTH_TEST);
	
	sceGuEnable(GU_CULL_FACE);	
	sceGuFrontFace(GU_CW);

	sceGuDisable(GU_COLOR_TEST);
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	sceGuClear (GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
/*	
	if (gl_ztrick->value)
	{
		static int trickframe;

		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
*/
}

/*
=============
R_SetPalette
=============
*/
void R_SetPalette ( const unsigned char *palette)
{
	int		i;

	byte *rp = ( byte * ) r_rawpalette;
	
	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
            rp[i*4+0] = GL_GetTexGamma(palette[i*3+0]);
			rp[i*4+1] = GL_GetTexGamma(palette[i*3+1]);
			rp[i*4+2] = GL_GetTexGamma(palette[i*3+2]);
			rp[i*4+3] = GL_GetTexGamma(0xff);
		}

	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = GL_GetTexGamma(( d_8to24table[i]       ) & 0xff);
			rp[i*4+1] = GL_GetTexGamma(( d_8to24table[i] >> 8  ) & 0xff);
			rp[i*4+2] = GL_GetTexGamma(( d_8to24table[i] >> 16 ) & 0xff);
			rp[i*4+3] = GL_GetTexGamma(0xff);
		}
		printf("Update Pal\n");
	}

    GL_SetTexturePalette(r_rawpalette);
    
	//sceGuClearColor (GU_COLOR(0,0,0,0));
	sceGuClear (GU_COLOR_BUFFER_BIT);
	//sceGuClearColor (GU_COLOR(1, 0, 0.5 , 0.5));
}

/*
====================
R_SetLightLevel

====================
*/
void R_SetLightLevel (void)
{
	vec3_t		shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint (r_newrefdef.vieworg, shadelight);

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
			r_lightlevel->value = 150*shadelight[0];
		else
			r_lightlevel->value = 150*shadelight[2];
	}
	else
	{
		if (shadelight[1] > shadelight[2])
			r_lightlevel->value = 150*shadelight[1];
		else
			r_lightlevel->value = 150*shadelight[2];
	}

}

/*
================
R_RenderFrame
================
*/
void R_RenderFrame(refdef_t *fd) 
{
	R_RenderView(fd);
	R_SetLightLevel();
	GU_View2D();
	GU_DefaultState();
	
	r_colorclear = false;
}

/*
%%%%

%%%%
*/
struct image_s *GL_RegisterSkin (char *name);
extern "C" refexport_t GetRefAPI(refimport_t rimp) 
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.EndRegistration = R_EndRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = GL_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_RegisterSky;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;

	re.DrawFadeScreen = Draw_FadeScreen;

	re.DrawStretchRaw = Draw_Raw;
    re.CinematicSetPalette = R_SetPalette;
	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GLimp_EndFrame;

	return re;
}

#ifdef REF_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	ri.Con_Printf (PRINT_ALL, "%s", text);
}
#endif
