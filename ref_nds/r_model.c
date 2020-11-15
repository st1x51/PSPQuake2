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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "r_local.h"
#include "../null/ds.h"

#define COLOURED_LIGHTS
//#define BOXTESTED_BSP

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	256
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept seperate
model_t	mod_inline[MAX_MOD_KNOWN];

int		registration_sequence;
int		modfilelen;

//===============================================================================


/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	model_t	*mod;
	int		total;

	total = 0;
	ri.Con_Printf (PRINT_ALL,"Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		ri.Con_Printf (PRINT_ALL, "%8i : %s\n",mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}
	ri.Con_Printf (PRINT_ALL, "Total resident: %i\n", total);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	unsigned *buf;
	int		i;
	
	if (!name[0])
		ri.Sys_Error (ERR_DROP,"Mod_ForName: NULL name");

	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
			ri.Sys_Error (ERR_DROP, "bad inline model number");
		return &mod_inline[i];
	}

	//
	// search the currently loaded models
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			return mod;
			
	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			ri.Sys_Error (ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}
	strcpy (mod->name, name);
	
	//
	// load the file
	//
	modfilelen = ri.FS_LoadFile (mod->name, (void **)&buf);
	if (!buf)
	{
		if (crash)
			ri.Sys_Error (ERR_DROP,"Mod_NumForName: %s not found", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}
	
	loadmodel = mod;

	//
	// fill it in
	//

	// call the apropriate loader
	
	switch (LittleLong(*(unsigned *)buf))
	{
	case IDALIASHEADER:
		loadmodel->extradata = Hunk_Begin (0x100000);
		Mod_LoadAliasModel (mod, buf);
		break;
		
	case IDSPRITEHEADER:
		loadmodel->extradata = Hunk_Begin (0x10000);
		Mod_LoadSpriteModel (mod, buf);
		break;
	
	case IDBSPHEADER:
		loadmodel->extradata = Hunk_Begin (0x580000);
		Mod_LoadBrushModel (mod, buf);
		break;

	default:
		ri.Sys_Error (ERR_DROP,"Mod_NumForName: unknown fileid for %s", mod->name);
		break;
	}

	loadmodel->extradatasize = Hunk_End ();

	ri.FS_FreeFile (buf);

	return mod;
}


/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model || !model->nodes)
		ri.Sys_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != -1)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;	
	out = decompressed;

#if 0
	memcpy (out, in, row);
#else
	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
//			*out++ = 0xff;
			byte_write(out, 0xff);
			out++;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
//			*out++ = *in++;
			byte_write(out, *in);
			out++;
			in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
//			*out++ = 0;
			byte_write(out, 0);
			out++;
			c--;
		}
	} while (out - decompressed < row);
#endif
	
	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS],
		model);
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;


/*
=================
Mod_LoadLighting

Converts the 24 bit lighting down to 8 bit
by taking the brightest component
=================
*/
#ifdef COLOURED_LIGHTS
void Mod_LoadLighting (lump_t *l)
{
	int		i, size;
	byte	*in;

	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	size = l->filelen;
//	loadmodel->lightdata = Hunk_Alloc (size);
	ds_set_malloc_base(MEM_XTRA);
	loadmodel->lightdata = (byte *)ds_malloc(size);
	ds_set_malloc_base(MEM_MAIN);
	
	if (loadmodel->lightdata == NULL)
		Sys_Error("couldn\'t allocate memory for lighting\n");
	
	in = (byte *)(mod_base + l->fileofs);
	for (i=0 ; i<size ; i += 3, in+=3)
	{
		byte_write(&loadmodel->lightdata[i], in[0]);
		byte_write(&loadmodel->lightdata[i + 1], in[1]);
		byte_write(&loadmodel->lightdata[i + 2], in[2]);
	}
}
#else
void Mod_LoadLighting (lump_t *l)
{
	int		i, size;
	byte	*in;

	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	size = l->filelen/3;
//	loadmodel->lightdata = Hunk_Alloc (size);
	ds_set_malloc_base(MEM_XTRA);
	loadmodel->lightdata = (byte *)ds_malloc(size);
	ds_set_malloc_base(MEM_MAIN);
	
	if (loadmodel->lightdata == NULL)
		Sys_Error("couldn\'t allocate memory for lighting\n");
	
	in = (byte *)(mod_base + l->fileofs);
	for (i=0 ; i<size ; i++, in+=3)
	{
		if (in[0] > in[1] && in[0] > in[2])
			//loadmodel->lightdata[i] = in[0];
			byte_write(&loadmodel->lightdata[i], in[0]);
		else if (in[1] > in[0] && in[1] > in[2])
//			loadmodel->lightdata[i] = in[1];
			byte_write(&loadmodel->lightdata[i], in[1]);
		else
//			loadmodel->lightdata[i] = in[2];
			byte_write(&loadmodel->lightdata[i], in[2]);
	}
}
#endif


int		r_leaftovis[MAX_MAP_LEAFS];
int		r_vistoleaf[MAX_MAP_LEAFS];
int		r_numvisleafs;

void	R_NumberLeafs (mnode_t *node)
{
	mleaf_t	*leaf;
	int		leafnum;

	if (node->contents != -1)
	{
		leaf = (mleaf_t *)node;
		leafnum = leaf - loadmodel->leafs;
		if (leaf->contents & CONTENTS_SOLID)
			return;
		r_leaftovis[leafnum] = r_numvisleafs;
		r_vistoleaf[r_numvisleafs] = leafnum;
		r_numvisleafs++;
		return;
	}

	R_NumberLeafs (node->children[0]);
	R_NumberLeafs (node->children[1]);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	int		i;

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}
	loadmodel->vis = (dvis_t *)Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);
	for (i=0 ; i<loadmodel->vis->numclusters ; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	short *scaled_out;
	int			i, count;

	in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mvertex_t *)Hunk_Alloc ( (count+8)*sizeof(*out));		// extra for skybox

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
	
	in = (dvertex_t *)(mod_base + l->fileofs);
	scaled_out = (short *)Hunk_Alloc ( (count+8)*sizeof(short) * 3);		// extra for skybox
	loadmodel->real_vertexes = scaled_out;

	for ( i=0 ; i<count ; i++, in++, scaled_out += 3)
	{
		scaled_out[0] = (short)(LittleFloat (in->point[0]) * DS_MINI_GEOM_SCALE);
		scaled_out[1] = (short)(LittleFloat (in->point[1]) * DS_MINI_GEOM_SCALE);
		scaled_out[2] = (short)(LittleFloat (in->point[2]) * DS_MINI_GEOM_SCALE);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (dmodel_t *)Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		out->headnode = LittleLong (in->headnode);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (dedge_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (medge_t *)Hunk_Alloc ( (count + 13) * sizeof(*out));	// extra for skybox

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int 	i, j, count;
	float	len1, len2;
	char	name[MAX_QPATH];
	int		next;

	in = (texinfo_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t *)Hunk_Alloc ( (count+6)*sizeof(*out));	// extra for skybox
	
//	out = (mtexinfo_t *)ds_malloc( (count+6)*sizeof(*out));
	
	if (out == NULL)
	{
		printf("%d main heap\n", count_largest_block_kb());
		Sys_Error("ran out of memory allocating texinfo\n");
	}

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
		{
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->short_vecs[0][j] = (short)(LittleFloat (in->vecs[0][j]) * DS_MINI_GEOM_SCALE);
		}
		
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;
#if 0
		if (len1 + len2 < 0.001)
			out->mipadjust = 1;		// don't crash
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 + 0.1 );
#endif

		out->flags = LittleLong (in->flags);

		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->texinfo + next;

		Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);
		out->image = R_FindImage (name, it_wall);
//		out->image = NULL;
//		printf("findimage ok\n");
		if (!out->image)
		{
			out->image = r_notexture_mip; // texture not found
			out->flags = 0;
		}
	}
//	printf("counting animation frames\n");

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = (int)floor(mins[i]/16);
		bmaxs[i] = (int)ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (s->extents[i] < 16)
			s->extents[i] = 16;	// take at least one cache block
//		if ( !(tex->flags & (SURF_WARP|SURF_SKY)) && s->extents[i] > 256)
//			ri.Sys_Error (ERR_DROP,"Bad surface extents");
	}
}


/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (dface_t  *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t *)Hunk_Alloc ( (count+6)*sizeof(*out));	// extra for skybox

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		if (out->numedges < 3)
			ri.Sys_Error (ERR_DROP,"Surface with %s edges", out->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);
				
	// lighting info is converted from 24 bit on disk to 8 bit

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
//			out->styles[i] = in->styles[i];
			byte_write(&out->styles[i], in->styles[i]);

#ifdef COLOURED_LIGHTS
		i = LittleLong(in->lightofs);
		
		if (i == -1)
			out->samples = (unsigned char *)(1 << 10);		//cos we check for this number later
		else
		{
			int total[3];
			total[0] = total[1] = total[2] = 255;
			
			out->samples = (unsigned char *)Hunk_Alloc(3);
			
			if (out->samples == NULL)
				Sys_Error("failed to allocate surface light colour\n");

			if (loadmodel->lightdata != NULL)
			{
				int smax = (out->extents[0]>>4)+1;
				int tmax = (out->extents[1]>>4)+1;
				
				total[0] = total[1] = total[2] = 0;
				int index;
				
				for (index = 0; index < smax * tmax; index++)
				{
					total[0] += loadmodel->lightdata[i + index * 3];
					total[1] += loadmodel->lightdata[i + index * 3 + 1];
					total[2] += loadmodel->lightdata[i + index * 3 + 2];
				}
				
				total[0] = total[0] / (smax * tmax);
				total[1] = total[1] / (smax * tmax);
				total[2] = total[2] / (smax * tmax);
			}
			
			byte_write(&out->samples[0], total[0]);
			byte_write(&out->samples[1], total[1]);
			byte_write(&out->samples[2], total[2]);
		}
#else
		i = LittleLong(in->lightofs) / 3;
		
		if (i == -1)
			out->samples = (unsigned char *)(1 << 10);		//cos we check for this number later
		else
		{
			int total = 255;

			if (loadmodel->lightdata != NULL)
			{
				int smax = (out->extents[0]>>4)+1;
				int tmax = (out->extents[1]>>4)+1;

				total = 0;
				int index;

				for (index = 0; index < smax * tmax; index++)
					total += loadmodel->lightdata[i + index];

				total = total / (smax * tmax);
			}

			out->samples = (unsigned char *)total;
		}
#endif
		
	// set the drawing flags flag
		
		if (!out->texinfo->image)
			continue;
		if (out->texinfo->flags & SURF_SKY)
		{
			out->flags |= SURF_DRAWSKY;
			continue;
		}
		
		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}
//==============
//PGM
		// this marks flowing surfaces as turbulent, but with the new
		// SURF_FLOW flag.
		if (out->texinfo->flags & SURF_FLOWING)
		{
			out->flags |= SURF_DRAWTURB | SURF_FLOW;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}
//PGM
//==============
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (dnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Hunk_Alloc ( count*sizeof(*out));
//	out = (mnode_t *)ds_malloc(count*sizeof(*out));
	
	if (out == NULL)
	{
		printf("%d main heap\n", count_largest_block_kb());
		Sys_Error("failed to allocate memory for map nodes\n");
	}

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
#ifdef BOXTESTED_BSP
		for (j=0 ; j<3 ; j++)
		{
			out->bt_minmaxs[j] = LittleShort (in->mins[j]) * DS_MINI_GEOM_SCALE;
			out->bt_minmaxs[3+j] = LittleShort (in->maxs[j] - in->mins[j]) * DS_MINI_GEOM_SCALE;
		}
#else
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
#endif
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		out->contents = CONTENTS_NODE;	// differentiate from leafs
		
		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count;

	in = (dleaf_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *)Hunk_Alloc ( count*sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);
	}	
}


/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (short *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t **)Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			ri.Sys_Error (ERR_DROP,"Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int *)Hunk_Alloc ( (count+24)*sizeof(*out));	// extra for skybox

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}

/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP,"MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mplane_t *)Hunk_Alloc ( (count+6)*sizeof(*out));		// extra for skybox
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			out->int_normal[j] = (int)(in->normal[j] * 8192);
			
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->int_dist = (int)in->dist * 8192;
		
//		out->type = LittleLong (in->type);
//		out->signbits = bits;
		byte_write(&out->type, LittleLong (in->type));
		byte_write(&out->signbits, bits);
	}
}

int generate_texcoords(msurface_t *fa, model_t *model, unsigned int *texcoord_buffer, unsigned int buffer_size)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges;

	pedges = model->edges;
	lnumverts = fa->numedges;
	
	if (buffer_size < lnumverts)
		Sys_Error("trying to generate texture co-ordinates but the buffer is too small\nwe wanted %d, but we\'ve only got %d\n",
				lnumverts, buffer_size);
	
	short int *r_pcurrentrealvertbase = model->real_vertexes;
//	unsigned int *tex_coords = fa->texture_coordinates = (unsigned int *)Hunk_Alloc(lnumverts * sizeof(unsigned int));
	unsigned int *tex_coords = texcoord_buffer;
	
	short x, y, z, s, t;
	int count;
	
	for (count = 0; count < lnumverts; count++)
	{
		short this_vert[3];
		int index;
		bool less_than;
		medge_t *r_pedge;
		
		lindex = model->surfedges[fa->firstedge + count];
		less_than = lindex <= 0;
		
		if (less_than)
			r_pedge = &pedges[-lindex];
		else
			r_pedge = &pedges[lindex];
		
		index = r_pedge->v[less_than] * 3;
		
		this_vert[0] = r_pcurrentrealvertbase[index];
		this_vert[1] = r_pcurrentrealvertbase[index + 1];
		this_vert[2] = r_pcurrentrealvertbase[index + 2];
		
		short short_s = (DotProduct(this_vert, fa->texinfo->short_vecs[0]) >> DS_MINI_GEOM_SHIFT) + fa->texinfo->short_vecs[0][3];
		short short_t = (DotProduct(this_vert, fa->texinfo->short_vecs[1]) >> DS_MINI_GEOM_SHIFT) + fa->texinfo->short_vecs[1][3];
		
		s = short_s >> (DS_MIP_LEVEL - DS_SCALE_FUNK);
		t = short_t >> (DS_MIP_LEVEL - DS_SCALE_FUNK);

		*tex_coords = PACK_TEXTURE(t, s);
		
		tex_coords++;
	}
	
	return lnumverts;
}

int added_to_hunk = 0;
int generate_surfaces(msurface_t *fa, model_t *model, unsigned int *texcoord_buffer)
{
	int			i, lindex, num_verts;
	medge_t		*pedges;

	pedges = model->edges;
	num_verts = fa->numedges;
	
	short int *r_pcurrentrealvertbase = model->real_vertexes;
	
	//12 words per triangle + 1 for the number of words in the list
	unsigned int *cmd_list = (unsigned int *)Hunk_Alloc((((num_verts - 2) * 12) + 1) * sizeof(unsigned int));
	added_to_hunk += (((num_verts - 2) * 12) + 1) * sizeof(unsigned int);
	
//#define NORMAL_TEST(x, y, z) ((fa->plane->normal[0] == x) && (fa->plane->normal[1] == y) && (fa->plane->normal[2] == z))
	if (fa->texinfo->flags & SURF_SKY)
	{
//		if (NORMAL_TEST(0, 0, 1))
//			fa->flags |= SURF_SKY_BOTTOM;
//		else if (NORMAL_TEST(0, 0, -1))
//			fa->flags |= SURF_SKY_TOP;
//		
//		else if (NORMAL_TEST(0, 1, 0))
//			fa->flags |= SURF_SKY_FRONT;
//		else if (NORMAL_TEST(0, -1, 0))
//			fa->flags |= SURF_SKY_BACK;
//		
//		else if (NORMAL_TEST(1, 0, 0))
//			fa->flags |= SURF_SKY_LEFT;
//		else if (NORMAL_TEST(-1, 0, 0))
//			fa->flags |= SURF_SKY_RIGHT;
		
		fa->display_list = NULL;
		return 0;
	}
	
	fa->display_list = (cmd_list + 1);
	
	short init_x[2], init_y[2], init_z[2];
	short x_curr, y_curr, z_curr;
	
	unsigned int init_uv[2];
	unsigned int uv_curr;
	
	cmd_list[0] = (num_verts - 2) * 12;
	unsigned int *cmd_ptr = &cmd_list[1];
	
	for (int count = 0; count < 2; count++)
	{
		short short_local[3];

		short this_vert[3];
		int index;
		bool less_than;
		medge_t *r_pedge;

		lindex = model->surfedges[fa->firstedge + count];
		less_than = lindex <= 0;

		if (less_than)
			r_pedge = &pedges[-lindex];
		else
			r_pedge = &pedges[lindex];

		index = r_pedge->v[less_than] * 3;

		init_x[count] = r_pcurrentrealvertbase[index];
		init_y[count] = r_pcurrentrealvertbase[index + 1];
		init_z[count] = r_pcurrentrealvertbase[index + 2];
//		init_uv[count] = fa->texture_coordinates[count];
		init_uv[count] = texcoord_buffer[count];
	}

	for (int count = 2; count < num_verts; count++)
	{
		short short_local[3];

		short this_vert[3];
		int index;
		bool less_than;
		medge_t *r_pedge;

		lindex = model->surfedges[fa->firstedge + count];
		less_than = lindex <= 0;

		if (less_than)
			r_pedge = &pedges[-lindex];
		else
			r_pedge = &pedges[lindex];

		index = r_pedge->v[less_than] * 3;

//		uv_curr = fa->texture_coordinates[count];
		uv_curr = texcoord_buffer[count];
		x_curr = r_pcurrentrealvertbase[index];
		y_curr = r_pcurrentrealvertbase[index + 1];
		z_curr = r_pcurrentrealvertbase[index + 2];
		
		*cmd_ptr++ = FIFO_COMMAND_PACK(FIFO_BEGIN,
				FIFO_TEX_COORD, FIFO_VERTEX16,
				FIFO_TEX_COORD);

		*cmd_ptr++ = 0;
		*cmd_ptr++ = init_uv[0];
		*cmd_ptr++ = (init_y[0] << 16) | (init_x[0] & 0xffff);
		*cmd_ptr++ = ((unsigned int)(unsigned short)init_z[0]);
		*cmd_ptr++ = init_uv[1];

		*cmd_ptr++ = FIFO_COMMAND_PACK(FIFO_VERTEX16,
				FIFO_TEX_COORD, FIFO_VERTEX16,
				FIFO_NOP);

		*cmd_ptr++ = (init_y[1] << 16) | (init_x[1] & 0xffff);
		*cmd_ptr++ = ((unsigned int)(unsigned short)init_z[1]);
		*cmd_ptr++ = uv_curr;
		*cmd_ptr++ = (y_curr << 16) | (x_curr & 0xffff);
		*cmd_ptr++ = ((unsigned int)(unsigned short)z_curr);

		init_x[1] = x_curr;
		init_y[1] = y_curr;
		init_z[1] = z_curr;
		init_uv[1] = uv_curr;
	}
	
	
	return num_verts;
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	dmodel_t 	*bm;
	
//	loadmodel->type = mod_brush;
	byte_write(&loadmodel->type, mod_brush);
	if (loadmodel != mod_known)
		ri.Sys_Error (ERR_DROP, "Loaded a brush model after the world");
	
	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		ri.Sys_Error (ERR_DROP,"Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	
	ds_free(loadmodel->lightdata);
	loadmodel->lightdata = NULL;
	
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	r_numvisleafs = 0;
	R_NumberLeafs (loadmodel->nodes);
	
//
// set up the submodels
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;
		if (starmod->firstnode >= loadmodel->numnodes)
			ri.Sys_Error (ERR_DROP, "Inline model %i has bad firstnode", i);

		VectorCopy (bm->maxs, starmod->maxs);
		VectorCopy (bm->mins, starmod->mins);
	
		if (i == 0)
			*loadmodel = *starmod;
	}

	R_InitSkyBox ();
	
	unsigned int *tex_coord_buffer = (unsigned int *)ds_malloc(100 * sizeof(unsigned int));
	
	if (tex_coord_buffer == NULL)
		Sys_Error("insufficient memory for texture co-ord buffer\n");
	
	for (i = 0; i < mod->numsurfaces; i++)
	{
		mod->surfaces[i].texture_coordinates = NULL;

//		if ( mod->surfaces[i].flags & SURF_DRAWTURB )
//			continue;
//
//		if ( mod->surfaces[i].flags & SURF_DRAWSKY )
//			continue;

		generate_texcoords(mod->surfaces + i, mod, tex_coord_buffer, 100);
		generate_surfaces(mod->surfaces + i, mod, tex_coord_buffer);
		
//		printf("%.2f\n", (float)i / mod->numsurfaces * 100.0f);
	}
	
	ds_free(tex_coord_buffer);
	
	DC_FlushAll();
	
//	printf("%d\n", added_to_hunk);
//	while(1);
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j;
	dmdl_t				*pinmodel, *pheader;
	dstvert_t			*pinst, *poutst;
	dtriangle_t			*pintri, *pouttri;
	daliasframe_t		*pinframe, *poutframe;
	int					*pincmd, *poutcmd;
	int					version;

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

	pheader = (dmdl_t *)Hunk_Alloc (LittleLong(pinmodel->ofs_end));
	
	// byte swap the header fields and sanity check
	for (i=0 ; i<sizeof(dmdl_t)/4 ; i++)
		((int *)pheader)[i] = LittleLong (((int *)buffer)[i]);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		ri.Sys_Error (ERR_DROP, "model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	if (pheader->num_xyz <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no vertices", mod->name);

	if (pheader->num_xyz > MAX_VERTS)
		ri.Sys_Error (ERR_DROP, "model %s has too many vertices", mod->name);

	if (pheader->num_st <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no st vertices", mod->name);

	if (pheader->num_tris <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no triangles", mod->name);

	if (pheader->num_frames <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no frames", mod->name);

//
// load base s and t vertices (not used in gl version)
//
	pinst = (dstvert_t *) ((byte *)pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *) ((byte *)pheader + pheader->ofs_st);

	for (i=0 ; i<pheader->num_st ; i++)
	{
		poutst[i].s = LittleShort (pinst[i].s);
		poutst[i].t = LittleShort (pinst[i].t);
	}

//
// load triangle lists
//
	pintri = (dtriangle_t *) ((byte *)pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *) ((byte *)pheader + pheader->ofs_tris);

	for (i=0 ; i<pheader->num_tris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[j]);
		}
	}

//
// load the frames
//
	for (i=0 ; i<pheader->num_frames ; i++)
	{
		pinframe = (daliasframe_t *) ((byte *)pinmodel 
			+ pheader->ofs_frames + i * pheader->framesize);
		poutframe = (daliasframe_t *) ((byte *)pheader 
			+ pheader->ofs_frames + i * pheader->framesize);

		ds_memcpy (poutframe->name, pinframe->name, sizeof(poutframe->name));
		for (j=0 ; j<3 ; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}
		// verts are all 8 bit, so no swapping needed
		ds_memcpy (poutframe->verts, pinframe->verts, 
			pheader->num_xyz*sizeof(dtrivertx_t));

	}

//	mod->type = mod_alias;
	byte_write(&mod->type, mod_alias);

	//
	// load the glcmds
	//
	pincmd = (int *) ((byte *)pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *) ((byte *)pheader + pheader->ofs_glcmds);
	for (i=0 ; i<pheader->num_glcmds ; i++)
		poutcmd[i] = LittleLong (pincmd[i]);


	// register all skins
	ds_memcpy ((char *)pheader + pheader->ofs_skins, (char *)pinmodel + pheader->ofs_skins,
		pheader->num_skins*MAX_SKINNAME);
	for (i=0 ; i<pheader->num_skins ; i++)
	{
		mod->skins[i] = R_FindImage ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME, it_skin);
	}
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	dsprite_t	*sprin, *sprout;
	int			i;

	sprin = (dsprite_t *)buffer;
	sprout = (dsprite_t *)Hunk_Alloc (modfilelen);

	sprout->ident = LittleLong (sprin->ident);
	sprout->version = LittleLong (sprin->version);
	sprout->numframes = LittleLong (sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
		ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > MAX_MD2SKINS)
		ri.Sys_Error (ERR_DROP, "%s has too many frames (%i > %i)",
				 mod->name, sprout->numframes, MAX_MD2SKINS);

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++)
	{
		sprout->frames[i].width = LittleLong (sprin->frames[i].width);
		sprout->frames[i].height = LittleLong (sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong (sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong (sprin->frames[i].origin_y);
		memcpy (sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		mod->skins[i] = R_FindImage (sprout->frames[i].name, it_sprite);
	}

//	mod->type = mod_sprite;
	byte_write(&mod->type, mod_sprite);
}

//=============================================================================

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
//int hit_count = 0;
void R_BeginRegistration (char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs
	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);

//	D_FlushCaches ();
	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = ri.Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(mod_known[0].name, fullname) || flushmap->value)
	{
//		Mod_Free (&mod_known[0]);
		Mod_FreeAll();
		R_ShutdownImages();
	}
	
//	printf("%d extra ram free\n", count_largest_extra_block_mb());
//	
//	if (hit_count == 1)
//		while(1);
//	hit_count++;
	
	r_worldmodel = R_RegisterModel (fullname);
	R_NewMap ();
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s *R_RegisterModel (char *name)
{
	model_t	*mod;
	int		i;
	dsprite_t	*sprout;
	dmdl_t		*pheader;

	mod = Mod_ForName (name, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		if (mod->type == mod_sprite)
		{
			sprout = (dsprite_t *)mod->extradata;
			for (i=0 ; i<sprout->numframes ; i++)
				mod->skins[i] = R_FindImage (sprout->frames[i].name, it_sprite);
		}
		else if (mod->type == mod_alias)
		{
			pheader = (dmdl_t *)mod->extradata;
			for (i=0 ; i<pheader->num_skins ; i++)
				mod->skins[i] = R_FindImage ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME, it_skin);
//PGM
			mod->numframes = pheader->num_frames;
//PGM
		}
		else if (mod->type == mod_brush)
		{
			for (i=0 ; i<mod->numtexinfo ; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;
		}
	}
	return mod;
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Hunk_Free (mod->extradata);
			memset (mod, 0, sizeof(*mod));
		}
		else
		{	// make sure it is paged in
			Com_PageInMemory ((byte *)mod->extradata, mod->extradatasize);
		}
	}

	R_FreeUnusedImages ();
}


//=============================================================================

/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	Hunk_Free (mod->extradata);
	memset (mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].extradatasize)
			Mod_Free (&mod_known[i]);
	}
}
