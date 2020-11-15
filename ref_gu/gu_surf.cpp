#include "gu_local.h"
#include "gu_model.h"
#include "clipping.hpp"

using namespace quake;

static vec3_t	modelorg;		// relative to viewpoint

msurface_t* r_alpha_surfaces = 0;
msurface_t* r_sky_surfaces = 0;

unsigned int r_polys;

#define DYNAMIC_LIGHT_WIDTH  128
#define DYNAMIC_LIGHT_HEIGHT 128

#define LIGHTMAP_BYTES 4

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	 128

image_t* lightmap_textures[MAX_LIGHTMAPS];

int		c_visible_lightmaps;
int		c_visible_textures;

extern int currenttexture;

typedef struct
{
	int internal_format;
	int	current_lightmap_texture;

	msurface_t	*lightmap_surfaces[MAX_LIGHTMAPS];

	int			allocated[BLOCK_WIDTH];

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte		lightmap_buffer[4*BLOCK_WIDTH*BLOCK_HEIGHT];
} gllightmapstate_t;

static gllightmapstate_t gl_lms;

void GL_Bind(int texture_index);

void GU_BindLM(const image_t* img);
image_t* GU_LoadLM(const char* name, int w, int h, int bpp, unsigned char* data);
qboolean LM_AllocBlock (int w, int h, int *x, int *y);
void LM_InitBlock( void );
void LM_UploadBlock( qboolean dynamic );

image_t* R_TextureAnimation(mtexinfo_t *tex) 
{
	if(!tex->next) 
	{
		return tex->image;
	}

	int c = currententity->frame % tex->numframes;
	while(c) 
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}

void R_MarkLeaves(void)
{
	byte	*vis;
	byte	fatvis[MAX_MAP_LEAFS/8];
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;

	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL) 
	{
		return;
	}

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && /* !r_novis->value && */ r_viewcluster != -1)
		return;

	r_visframecount++;
	r_oldviewcluster  = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if( r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS(r_viewcluster, r_worldmodel);
	// may have to combine two clusters because of solid water boundaries
	if(r_viewcluster2 != r_viewcluster) {
		memcpy(fatvis, vis, (r_worldmodel->numleafs+7)/8);
		vis = Mod_ClusterPVS(r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs+31)/32;
		for(i=0 ; i<c ; i++) {
			((int *)fatvis)[i] |= ((int *)vis)[i];
		}
		vis = fatvis;
	}
	
	for(i=0, leaf=r_worldmodel->leafs; i<r_worldmodel->numleafs; i++, leaf++) {
		int cluster = leaf->cluster;
		if(cluster == -1) {
			continue;
		}

		if(vis[cluster>>3] & (1<<(cluster&7))) {
			node = (mnode_t*)leaf;
			do {
				if(node->visframe == r_visframecount) {
					break;
				}
				node->visframe = r_visframecount;
				node = node->parent;
			} while(node);
		}
	}
}

qboolean R_CullBox(vec3_t mins, vec3_t maxs) 
{

	if(r_nocull->value)
	{
		return false;
	}

	int i;
	for(i = 0; i < 4; i++)
	{
		if(BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
		{
			return true;
		}
	}
	return false;
}

static inline void DrawGLPoly (glpoly_t *p)
{
	// Does this poly need clipped?
	const int				unclipped_vertex_count	= p->numverts;
	const glvert_t* const	unclipped_vertices		= p->verts;
	if (clipping::is_clipping_required(
		unclipped_vertices,
		unclipped_vertex_count))
	{
		// Clip the polygon.
		const glvert_t*	clipped_vertices;
		std::size_t		clipped_vertex_count;
		clipping::clip(
			unclipped_vertices,
			unclipped_vertex_count,
			&clipped_vertices,
			&clipped_vertex_count);

		// Did we have any vertices left?
		if (clipped_vertex_count)
		{
			// Copy the vertices to the display list.
			const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
			glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
			memcpy(display_list_vertices, clipped_vertices, buffer_size);

			// Draw the clipped vertices.
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
				clipped_vertex_count, 0, display_list_vertices);
		}
	}
	else
	{
		// Draw the poly directly.
		sceGuDrawArray(
			GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
			unclipped_vertex_count, 0, unclipped_vertices);
	}
}
/*
static inline void DrawGLPolyLM (glpoly_t *p)
{
	// Does this poly need clipped?
	

	const int				unclipped_vertex_count	= p->numverts;
	const glvert_t* const	unclipped_vertices		= &(p->verts[p->numverts]);
	
	if (clipping::is_clipping_required(
		unclipped_vertices,
		unclipped_vertex_count))
	{
		// Clip the polygon.
		const glvert_t*	clipped_vertices;
		std::size_t		clipped_vertex_count;
		clipping::clip(
			unclipped_vertices,
			unclipped_vertex_count,
			&clipped_vertices,
			&clipped_vertex_count);

		// Did we have any vertices left?
		if (clipped_vertex_count)
		{
			// Copy the vertices to the display list.
			const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
			glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
			memcpy(display_list_vertices, clipped_vertices, buffer_size);

			// Draw the clipped vertices.
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
				clipped_vertex_count, 0, display_list_vertices);
		}
	}
	else
	{
	
		// Draw the poly directly.
		sceGuDrawArray(
			GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
			unclipped_vertex_count, 0, unclipped_vertices);
	}
}
*/
//============
//PGM
/*
================
DrawGLFlowingPoly -- version of DrawGLPoly that handles scrolling texture
================
*/
void DrawGLFlowingPoly (msurface_t *fa)
{
    const float real_time	= static_cast<float>(r_newrefdef.time);
	float	scroll = -64 * ( (real_time / 40.0) - (int)(real_time / 40.0) );
	if(scroll == 0.0)
		scroll = -64.0;

	for (const glpoly_t* p = fa->polys; p; p = p->next)
	{
		// Allocate memory for this polygon.
		const int		unclipped_vertex_count	= p->numverts;
		glvert_t* const	unclipped_vertices		=
			static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));

		// Generate each vertex.
		const glvert_t*	src			= p->verts;
		const glvert_t*	last_vertex = src + unclipped_vertex_count;
		glvert_t*		dst			= unclipped_vertices;
		while (src != last_vertex)
		{
			// Get the input UVs.
			const float	os = src->st[0];
			const float	ot = src->st[1];

			// Fill in the vertex data.
			dst->st[0] = os + scroll;
			dst->st[1] = ot;
			dst->xyz[0] = src->xyz[0];
			dst->xyz[1] = src->xyz[1];
			dst->xyz[2] = src->xyz[2];

			// Next vertex.
			++src;
			++dst;
		}

		// Do these vertices need clipped?
		if (clipping::is_clipping_required(unclipped_vertices, unclipped_vertex_count))
		{
			// Clip the polygon.
			const glvert_t*	clipped_vertices;
			std::size_t		clipped_vertex_count;
			clipping::clip(
				unclipped_vertices,
				unclipped_vertex_count,
				&clipped_vertices,
				&clipped_vertex_count);

			// Any vertices left?
			if (clipped_vertex_count)
			{
				// Copy the vertices to the display list.
				const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
				glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
				memcpy(display_list_vertices, clipped_vertices, buffer_size);

				// Draw the clipped vertices.
				sceGuDrawArray(
					GU_TRIANGLE_FAN,
					GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
					clipped_vertex_count, 0, display_list_vertices);
			}
		}
		else
		{
			// Draw the vertices.
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
				unclipped_vertex_count, 0, unclipped_vertices);
		}
	}
}
//PGM
//============

/*
** DrawGLPolyChain
*/
void DrawGLPolyChain( glpoly_t *p, float soffset, float toffset )
{
	if ( soffset == 0 && toffset == 0 )
	{
		for ( ; p; p = p->next)
		{
			// Allocate memory for this polygon.
			const int		unclipped_vertex_count	= p->numverts;
			glvert_t* const	unclipped_vertices		=
				static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));
	
			// Generate each vertex.
			const glvert_t*	src			= &(p->verts[p->numverts]);
			const glvert_t*	last_vertex = src + unclipped_vertex_count;
			glvert_t*		dst			= unclipped_vertices;
			while (src != last_vertex)
			{
				// Get the input UVs.
				const float	os = src->st[0];
				const float	ot = src->st[1];
	
				// Fill in the vertex data.
				dst->st[0] = os;
				dst->st[1] = ot;
				dst->xyz[0] = src->xyz[0];
				dst->xyz[1] = src->xyz[1];
				dst->xyz[2] = src->xyz[2];
	
				// Next vertex.
				++src;
				++dst;
			}
	
			// Do these vertices need clipped?
			if (clipping::is_clipping_required(unclipped_vertices, unclipped_vertex_count))
			{
				// Clip the polygon.
				const glvert_t*	clipped_vertices;
				std::size_t		clipped_vertex_count;
				clipping::clip(
					unclipped_vertices,
					unclipped_vertex_count,
					&clipped_vertices,
					&clipped_vertex_count);
	
				// Any vertices left?
				if (clipped_vertex_count)
				{
					// Copy the vertices to the display list.
					const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
					glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
					memcpy(display_list_vertices, clipped_vertices, buffer_size);
	
					// Draw the clipped vertices.
					sceGuDrawArray(
						GU_TRIANGLE_FAN,
						GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
						clipped_vertex_count, 0, display_list_vertices);
				}
			}
			else
			{
				// Draw the vertices.
				sceGuDrawArray(
					GU_TRIANGLE_FAN,
					GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
					unclipped_vertex_count, 0, unclipped_vertices);
			}
		}
	}
	else
	{
		for ( ; p; p = p->next)
		{
			// Allocate memory for this polygon.
			const int		unclipped_vertex_count	= p->numverts;
			glvert_t* const	unclipped_vertices		=
				static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));
	
			// Generate each vertex.
			const glvert_t*	src			= &(p->verts[p->numverts]);
			const glvert_t*	last_vertex = src + unclipped_vertex_count;
			glvert_t*		dst			= unclipped_vertices;
			while (src != last_vertex)
			{
				// Get the input UVs.
				const float	os = src->st[0];
				const float	ot = src->st[1];
	
				// Fill in the vertex data.
				dst->st[0] = os - soffset;
				dst->st[1] = ot - toffset;
				dst->xyz[0] = src->xyz[0];
				dst->xyz[1] = src->xyz[1];
				dst->xyz[2] = src->xyz[2];
	
				// Next vertex.
				++src;
				++dst;
			}
	
			// Do these vertices need clipped?
			if (clipping::is_clipping_required(unclipped_vertices, unclipped_vertex_count))
			{
				// Clip the polygon.
				const glvert_t*	clipped_vertices;
				std::size_t		clipped_vertex_count;
				clipping::clip(
					unclipped_vertices,
					unclipped_vertex_count,
					&clipped_vertices,
					&clipped_vertex_count);
	
				// Any vertices left?
				if (clipped_vertex_count)
				{
					// Copy the vertices to the display list.
					const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
					glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
					memcpy(display_list_vertices, clipped_vertices, buffer_size);
	
					// Draw the clipped vertices.
					sceGuDrawArray(
						GU_TRIANGLE_FAN,
						GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
						clipped_vertex_count, 0, display_list_vertices);
				}
			}
			else
			{
				// Draw the vertices.
				sceGuDrawArray(
					GU_TRIANGLE_FAN,
					GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
					unclipped_vertex_count, 0, unclipped_vertices);
			}
		}
	}
}
void EmitWaterPolys (msurface_t *fa);
void R_SetCacheState( msurface_t *surf );
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride);
/*
** R_BlendLightMaps
**
** This routine takes all the given light mapped surfaces in the world and
** blends them into the framebuffer.
*/
void R_BlendLightmaps (void)
{
	int			i;
	msurface_t	*surf, *newdrawsurf = 0;

	// don't bother if we're set to fullbright
	if (r_fullbright->value)
		return;
		
	if (!r_worldmodel->lightdata)
		return;

	// don't bother writing Z
	sceGuDepthMask(GU_TRUE);

  //set the appropriate blending mode unless we're only looking at the
  //lightmaps.
	
	/*
	** set the appropriate blending mode unless we're only looking at the
	** lightmaps.
	*/
	if (!gu_lightmap->value)
	{
		sceGuEnable (GU_BLEND);

		if ( gu_monolightmap->string[0] != '0' )
		{
			switch (gu_monolightmap->string[0])
			{
			case 'I':
				sceGuBlendFunc(GU_ADD, GU_DST_COLOR, GU_SRC_COLOR, 0, 0);
				break;
			case 'L':
				sceGuBlendFunc(GU_ADD, GU_DST_COLOR, GU_SRC_COLOR, 0, 0);
				break;
			case 'A':
			default:
				sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
				break;
			}
		}
		else
		{
			sceGuBlendFunc(GU_ADD, GU_DST_COLOR, GU_SRC_COLOR, 0, 0);
		}
	}

	if ( currentmodel == r_worldmodel )
		c_visible_lightmaps = 0;


    //render static lightmaps first
	for ( i = 1; i < MAX_LIGHTMAPS; i++ )
	{
		if ( gl_lms.lightmap_surfaces[i] )
		{
			if (currentmodel == r_worldmodel)
				c_visible_lightmaps++;

			GL_Bind( lightmap_textures[i]->texnum );

			for ( surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain )
			{
				if ( surf->polys )
					DrawGLPolyChain( surf->polys, 0, 0 );
			}
		}
	}
	/*
	** render dynamic lightmaps
	*/

	if (gu_dynamic->value)
	{
		LM_InitBlock();

	    GL_Bind( lightmap_textures[0]->texnum );
	
		if (currentmodel == r_worldmodel)
			c_visible_lightmaps++;

		newdrawsurf = gl_lms.lightmap_surfaces[0];

		for ( surf = gl_lms.lightmap_surfaces[0]; surf != 0; surf = surf->lightmapchain )
		{
			int		smax, tmax;
			byte	*base;

			smax = (surf->extents[0]>>4)+1;
			tmax = (surf->extents[1]>>4)+1;

			if ( LM_AllocBlock( smax, tmax, &surf->dlight_s, &surf->dlight_t ) )
			{
				base = gl_lms.lightmap_buffer;
				base += ( surf->dlight_t * BLOCK_WIDTH + surf->dlight_s ) * LIGHTMAP_BYTES;

				R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);
			}
			else
			{
				msurface_t *drawsurf;

				// upload what we have so far
				LM_UploadBlock( true );

				// draw all surfaces that use this lightmap
				for ( drawsurf = newdrawsurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain )
				{
					if ( drawsurf->polys )
						DrawGLPolyChain( drawsurf->polys,
							           ( drawsurf->light_s - drawsurf->dlight_s ) * ( 1.0 / 128.0 ),
									   ( drawsurf->light_t - drawsurf->dlight_t ) * ( 1.0 / 128.0 ) );
				}

				newdrawsurf = drawsurf;

				// clear the block
				LM_InitBlock();

				// try uploading the block now
				if ( !LM_AllocBlock( smax, tmax, &surf->dlight_s, &surf->dlight_t ) )
				{
					Sys_Error("Consecutive calls to LM_AllocBlock(%d,%d) failed (dynamic)\n", smax, tmax );
				}

				base = gl_lms.lightmap_buffer;
				base += ( surf->dlight_t * BLOCK_WIDTH + surf->dlight_s ) * LIGHTMAP_BYTES;

				R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);
			}
		}

		//
		// draw remainder of dynamic lightmaps that haven't been uploaded yet
		//
		if ( newdrawsurf )
			LM_UploadBlock( true );

		for ( surf = newdrawsurf; surf != 0; surf = surf->lightmapchain )
		{
			if ( surf->polys )
				DrawGLPolyChain( surf->polys, ( surf->light_s - surf->dlight_s ) * ( 1.0 / 128.0 ), ( surf->light_t - surf->dlight_t ) * ( 1.0 / 128.0 ) );
		}
	}

	//restore state
	sceGuDisable (GU_BLEND);
	sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDepthMask(GU_FALSE);
}
extern ScePspFMatrix4	r_world_matrix;
/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	int			maps;
	image_t		*image;
	qboolean is_dynamic = false;
	
	c_brush_polys++;

	image = R_TextureAnimation (fa->texinfo);

	if (fa->flags & SURF_DRAWTURB)
	{	
	    GL_Bind( image->texnum );
/*
		// warp texture, no lightmaps
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGB);
		glColor4f( gl_state.inverse_intensity,
			        gl_state.inverse_intensity,
					gl_state.inverse_intensity,
					1.0F );
*/
		EmitWaterPolys (fa);
/*
		sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGB);
*/
		return;
	}
	else
	{
		GL_Bind( image->texnum );
	}

//======
//PGM
	if(fa->texinfo->flags & SURF_FLOWING)
		DrawGLFlowingPoly (fa);
	else
		DrawGLPoly (fa->polys);
//PGM
//======

	/*
	** check for lightmap modification
	*/
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		if ( r_newrefdef.lightstyles[fa->styles[maps]].white != fa->cached_light[maps] )
			goto dynamic;
	}

	// dynamic this frame or dynamic previously
	if ( ( fa->dlightframe == r_framecount ) )
	{
dynamic:
			if ( gu_dynamic->value )
		  {
				if (!( fa->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP ) ) )
				{
					is_dynamic = true;
				}
		  }
	}

    if ( is_dynamic )
	{
		if ( ( fa->styles[maps] >= 32 || fa->styles[maps] == 0 ) && ( fa->dlightframe != r_framecount ) )
		{
			unsigned	temp[34*34];
			int			smax, tmax;

			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;

			R_BuildLightMap( fa, (byte*)((void *)temp), smax*4 );
			R_SetCacheState( fa );
      
			//lightmap_textures[fa->lightmaptexturenum] = GL_LoadPicLM (va("lmst %i",fa->lightmaptexturenum), fa->light_s, fa->light_t, (byte*)temp, LIGHTMAP_BYTES, GU_LINEAR, false, it_sky);

			fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
			gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
		}
		else
		{
			fa->lightmapchain = gl_lms.lightmap_surfaces[0];
			gl_lms.lightmap_surfaces[0] = fa;
		}
	}
	else
	{
	  fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
	  gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
    }
}


#define	MAX_GLTEXTURES	1024
extern image_t	gltextures[MAX_GLTEXTURES];
extern int			numgltextures;

void R_DrawTextureChains(void) 
{
	int i;
	image_t* img;
  
	c_visible_textures = 0;
	
	for(i = 0, img = &gltextures[0]; i < MAX_GLTEXTURES; i++, img++) 
	{
		if (!img->registration_sequence)
				continue;
		
		msurface_t* surf = img->texturechain;
		if(!surf) 
		{
			continue;
		}
    
		c_visible_textures++;
		
		for(; surf; surf = surf->texturechain) 
		{
			R_RenderBrushPoly(surf);
		}
		
		img->texturechain = 0;
	}
}


void R_DrawAlphaSurfaces(void) 
{
    msurface_t* s;
    int intens = 1;

	 //
	// go back to the world matrix
	//
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadMatrix(&r_world_matrix);
	sceGumUpdateMatrix();
	sceGumMatrixMode(GU_MODEL);
  
	sceGuEnable(GU_BLEND);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);
	sceGuDepthMask(GU_TRUE);
	
	for(s = r_alpha_surfaces; s; s = s->texturechain) 
	{
		GL_Bind(s->texinfo->image->texnum);

	    c_brush_polys++;

		if (s->texinfo->flags & SURF_TRANS33)
			sceGuColor (GU_COLOR(intens,intens,intens,0.33));
		else if (s->texinfo->flags & SURF_TRANS66)
			sceGuColor (GU_COLOR(intens,intens,intens,0.66));
		else
			sceGuColor (GU_COLOR(intens,intens,intens,1));
		
		if (s->flags & SURF_DRAWTURB)
			EmitWaterPolys (s);
		else if(s->texinfo->flags & SURF_FLOWING)			// PGM	9/16/98
			DrawGLFlowingPoly (s);							// PGM
		else
			DrawGLPoly (s->polys);
	}
    sceGuDepthMask(GU_FALSE);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuDisable(GU_BLEND);

	r_alpha_surfaces = NULL;
}
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_DrawInlineBModel(void) 
{

	int			k;
	dlight_t	*lt;
	lt = r_newrefdef.dlights;
	for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
	{
			R_MarkLights (lt, 1<<k, currentmodel->nodes + currentmodel->firstnode);
	}

	
	msurface_t* surf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

	if ( currententity->flags & RF_TRANSLUCENT )
	{
		sceGuEnable (GU_BLEND);
		sceGuColor (GU_COLOR(1,1,1,0.25));
	    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);
	}

	int i;
	for(i = 0; i < currentmodel->nummodelsurfaces; i++, surf++) 
	{
		cplane_t* plane = surf->plane;
		
		float dot = DotProduct(modelorg, plane->normal) - plane->dist;
	
		if(((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
		  (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) 
			{
                if (surf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66) )
			    {	  // add to the translucent chain
				      surf->texturechain = r_alpha_surfaces;
				      r_alpha_surfaces = surf;
			    }
                else
			    {
					  R_RenderBrushPoly( surf );
			    }
			}
	}
  
	if ( !(currententity->flags & RF_TRANSLUCENT) )
	{
			R_BlendLightmaps ();
	}
	else
	{
		sceGuDisable (GU_BLEND);
		sceGuColor (GU_COLOR(1,1,1,1));
		sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGB);
	}
}

void R_DrawBrushModel(entity_t* e) 
{
	qboolean rotated;
	vec3_t mins, maxs;

	if(currentmodel->nummodelsurfaces == 0) 
	{
		return;
	}

	currententity = e;
    currenttexture = -1;

	if(e->angles[0] || e->angles[1] || e->angles[2]) 
	{
		rotated = true;
		int i;
		for(i = 0; i < 3; i++) 
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	} 
	else 
	{
		rotated = false;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if(R_CullBox(mins, maxs)) 
	{
		return;
	}

    memset (gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	VectorSubtract(r_newrefdef.vieworg, e->origin, modelorg);
	if(rotated) 
	{
		vec3_t temp;
		vec3_t forward, right, up;
		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] =  DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] =  DotProduct(temp, up);
	}
 
 	sceGumPushMatrix();

	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];

	GU_RotateForEntity(e);
    sceGumUpdateMatrix();

clipping::begin_brush_model();

	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];

	R_DrawInlineBModel();

clipping::end_brush_model();

	sceGumPopMatrix();

}
void R_AddSkySurface (msurface_t *fa);
void R_RecursiveWorldNode(mnode_t *node) 
{
	int			side, sidebit;
	msurface_t	*surf;
	float		dot;
	image_t*	image;

	if(node->contents == CONTENTS_SOLID)
	{
	
		return;		// solid
	}

	if(node->visframe != r_visframecount) 
	{
		return;
	}

	if(R_CullBox(node->minmaxs, node->minmaxs+3)) 
	{
		return;
	}

// if a leaf node, draw stuff
	if(node->contents != -1) {
		mleaf_t* pleaf = (mleaf_t*)node;

		// check for door connected areas
		if(r_newrefdef.areabits) {
			if(!(r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)))) {
				return;
			}
		}

		msurface_t** mark = pleaf->firstmarksurface;
		int c = pleaf->nummarksurfaces;
		if(c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while(--c);
		}

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	cplane_t* plane = node->plane;

	switch(plane->type) {
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if(dot >= 0) 
	{
		side = 0;
		sidebit = 0;
	} 
	else 
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side]);

	// draw stuff
	int c;
	for(c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c; c--, surf++) 
	{
		
		if(surf->visframe != r_framecount) 
		{
			continue;
		}

		if((surf->flags & SURF_PLANEBACK) != sidebit) 
		{
			continue;		// wrong side
		}


		if(surf->texinfo->flags & SURF_SKY) 
		{ // just adds to visible sky bounds
			R_AddSkySurface (surf);
		} 
		else if (surf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66)) 
		{
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		} 
		else 
		{
			image = R_TextureAnimation(surf->texinfo);
			surf->texturechain = image->texturechain;
			image->texturechain = surf;
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side]);
}

void R_DrawWorld(void)
{
	entity_t ent;

	if (!r_drawworld->value)
		return;

	r_polys = 0;

	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL) 
	{
		return;
	}

	currentmodel = r_worldmodel;

	VectorCopy(r_newrefdef.vieworg, modelorg);
 
    // auto cycle the world frame for texture animation
	memset (&ent, 0, sizeof(ent));
	ent.frame = (int)(r_newrefdef.time*2);
	currententity = &ent;
 
    currenttexture = -1;
	
	//sceGuColor(0xffffffff);
 
    memset (gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	R_ClearSkyBox ();

	R_RecursiveWorldNode(currentmodel->nodes);

	R_DrawTextureChains();

    R_BlendLightmaps ();
  
	R_DrawSkyBox();
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

void LM_InitBlock( void )
{
	memset( gl_lms.allocated, 0, sizeof( gl_lms.allocated ) );
}

void LM_UploadBlock( qboolean dynamic )
{
	int texture = 0;
	int height = 0;

	if ( dynamic )
	{
		texture = 0;
	}
	else
	{
		texture = gl_lms.current_lightmap_texture;
	}

	if ( dynamic )
	{
		int i;

		for ( i = 0; i < BLOCK_WIDTH; i++ )
		{
			if ( gl_lms.allocated[i] > height )
				height = gl_lms.allocated[i];
		}

		lightmap_textures[0] = GL_LoadPicLM ("Dynamic_lm", BLOCK_WIDTH,  BLOCK_HEIGHT, gl_lms.lightmap_buffer, LIGHTMAP_BYTES, GU_LINEAR, true, it_sky, 1);
	}
	else
	{
		lightmap_textures[texture] = GL_LoadPicLM (va("lmst %i",texture), BLOCK_WIDTH, BLOCK_HEIGHT, gl_lms.lightmap_buffer, LIGHTMAP_BYTES, GU_LINEAR, false, it_sky, 0);

		if ( ++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS )
		  Sys_Error("LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n" );

	}

}

// returns a texture number and the position inside it
qboolean LM_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;

	best = BLOCK_HEIGHT;

	for (i=0 ; i<BLOCK_WIDTH-w ; i++)
	{
		best2 = 0;

		for (j=0 ; j<w ; j++)
		{
			if (gl_lms.allocated[i+j] >= best)
				break;
			if (gl_lms.allocated[i+j] > best2)
				best2 = gl_lms.allocated[i+j];
		}
		if (j == w)
		{	// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
		return false;

	for (i=0 ; i<w ; i++)
		gl_lms.allocated[*x + i] = best + h;

	return true;
}

/*
================
GL_BuildPolygonFromSurface
================
*/
void R_BuildPolygonFromSurface(msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;
	vec3_t		total;

    // reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	VectorClear (total);
	//
	// draw texture
	//
	poly = static_cast<glpoly_t*>(Hunk_Alloc (sizeof(glpoly_t) + (lnumverts * 2 - 1) * sizeof(glvert_t)));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->image->original_width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->image->original_height;

		VectorAdd (total, vec, total);
		VectorCopy (vec, poly->verts[i].xyz);
		poly->verts[i].st[0] = s;
		poly->verts[i].st[1] = t;

	    //
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;
   
		VectorCopy(vec, poly->verts[i + lnumverts].xyz);
		poly->verts[i + lnumverts].st[0] = s;
		poly->verts[i + lnumverts].st[1] = t;
	}

	poly->numverts = lnumverts;

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	if ( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ) )
	{
		LM_UploadBlock( false );
		LM_InitBlock();
		if ( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ) )
		{
			Sys_Error("Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax );
		}
	}

	surf->lightmaptexturenum = gl_lms.current_lightmap_texture;

	base = gl_lms.lightmap_buffer;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	R_SetCacheState( surf );
	R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);
}


/*
==================
GL_BeginBuildingLightmaps

==================
*/
void GL_BeginBuildingLightmaps (model_t *m)
{
	static lightstyle_t	lightstyles[MAX_LIGHTSTYLES];
	int				i;
	unsigned		dummy[BLOCK_WIDTH*BLOCK_HEIGHT];

	memset( gl_lms.allocated, 0, sizeof(gl_lms.allocated) );

	r_framecount = 1;		// no dlightcache

	/*
	** setup the base lightstyles so the lightmaps won't have to be regenerated
	** the first time they're seen
	*/
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}
	r_newrefdef.lightstyles = lightstyles;
/*
	if (!gl_state.lightmap_textures)
	{
		gl_state.lightmap_textures	= TEXNUM_LIGHTMAPS;
//		gl_state.lightmap_textures	= gl_state.texture_extension_number;
//		gl_state.texture_extension_number = gl_state.lightmap_textures + MAX_LIGHTMAPS;
	}
*/
	gl_lms.current_lightmap_texture = 1;

	/*
	** if mono lightmaps are enabled and we want to use alpha
	** blending (a,1-a) then we're likely running on a 3DLabs
	** Permedia2.  In a perfect world we'd use a GL_ALPHA lightmap
	** in order to conserve space and maximize bandwidth, however 
	** this isn't a perfect world.
	**
	** So we have to use alpha lightmaps, but stored in GL_RGBA format,
	** which means we only get 1/16th the color resolution we should when
	** using alpha lightmaps.  If we find another board that supports
	** only alpha lightmaps but that can at least support the GL_ALPHA
	** format then we should change this code to use real alpha maps.
	*/
	//gl_lms.internal_format = gl_tex_solid_format;

	lightmap_textures[0] = GL_LoadPicLM ("Dynamic_lm", BLOCK_WIDTH, BLOCK_HEIGHT, (byte*)dummy, LIGHTMAP_BYTES, GU_LINEAR, false, it_sky, 1);

}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
	LM_UploadBlock( false );
}
