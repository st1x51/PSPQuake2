#include "../g_local.h"

#ifdef _WIN32
#include <direct.h>
#endif

static int version;
int recursion_count;

void NAV_WriteActiveNodes(active_node_data_t *active_node_data, char *unitname)
{
    file_t *f;
    char filename[MAX_QPATH];
    int i;
    int version = ROUTE_VERSION;

    /* Build filename */
    strcpy(filename, ROUTE_SUBDIR);
    strcat(filename, DIR_SLASH);
    strcat(filename, unitname);
    strcat(filename, ".");
    strcat(filename, ROUTE_EXT);

    f = gi.FileOpen (filename, FS_MODE_WRITE | FS_PATH_GAMEDIR);
    if (!f)
        gi.error("Couldn't open %s for writing.", filename);

    gi.FileWrite (f, &version, sizeof(version));
    gi.FileWrite (f, &active_node_data->node_count, sizeof(active_node_data->node_count));

    for (i = 0; i < active_node_data->node_count; i++)
        NAV_WriteNode(f, active_node_data, active_node_data->nodes[i]);

	gi.FileClose (f);

    active_node_data->modified = false;

    if (nav_debug->value != 0.0f)
        gi.dprintf("Saved nodes.\n\n");
}

void NAV_WriteNode(file_t *f, active_node_data_t *active_node_data, node_t *node)
{
    int i, j;
    short path;
    byte packed;
    byte route_index;
    qboolean low_nibble;

    packed = 0;
    low_nibble = false;

    gi.FileWrite (f, &node->timestamp, sizeof(node->timestamp));
    gi.FileWrite (f, node->origin, sizeof(node->origin));
    gi.FileWrite (f, node->jump_vel, sizeof(node->jump_vel));
    gi.FileWrite (f, &node->node_type, sizeof(node->node_type));
    gi.FileWrite (f, &node->goal_index, sizeof(node->goal_index));
    gi.FileWrite (f, &node->waterlevel, sizeof(node->waterlevel));
    gi.FileWrite (f, &node->yaw, sizeof(node->yaw));
    gi.FileWrite (f, &node->cast_group, sizeof(node->cast_group));
    gi.FileWrite (f, node->visible_nodes, sizeof(node->visible_nodes));
    gi.FileWrite (f, &node->num_visible, sizeof(node->num_visible));

    for (i = 0; i < active_node_data->node_count; i++)
    {
        route_index = 0;

        if (node->index != i)
        {
            path = node->routes[i].path;

            if (path >= 0)
            {
                for (j = 0; j < node->num_visible; j++)
                {
                    if (node->visible_nodes[j] == path)
                    {
                        route_index = j + 1;
                        break;
                    }
                }

                if (!route_index)
                {
                    gi.dprintf("NAV_WriteNode: unreachable path index in route (%i - %i)\n", node->index, i);
                }
            }
        }

        if (low_nibble)
        {
            packed += route_index;
            gi.FileWrite (f, &packed, sizeof(packed));
            packed = 0;
        }
        else
        {
            packed += (route_index << 4);
        }

        low_nibble = !low_nibble;
    }

    if (low_nibble)
        gi.FileWrite (f, &packed, sizeof(packed));
}

void NAV_ReadActiveNodes(active_node_data_t *active_node_data, char *unitname)
{
    file_t *f;
    char filename[MAX_QPATH];
    int i, base_index;

    NAV_InitActiveNodes(active_node_data);

    strcpy(filename, ROUTE_SUBDIR);
    strcat(filename, DIR_SLASH);
    strcat(filename, unitname);
    strcat(filename, ".");
    strcat(filename, ROUTE_EXT);

    f = gi.FileOpen (filename, FS_MODE_READ | FS_PATH_GAMEDIR | FS_TYPE_RFS);
    if (!f)
        return;

    /* read header */
    gi.FileRead (f, &version, sizeof(version));

    /* supported versions (v3+) */
    if (version >= ROUTE_VERSION - 1)
    {
        gi.FileRead (f, &active_node_data->node_count, sizeof(active_node_data->node_count));

        for (i = 0; i < active_node_data->node_count; i++)
        {
            active_node_data->nodes[i] = gi.TagMalloc(sizeof(node_t), TAG_GAME);

            memset(active_node_data->nodes[i], 0, sizeof(node_t));
            active_node_data->nodes[i]->index = i;

            NAV_ReadNode(f, active_node_data, active_node_data->nodes[i]);

            NAV_AddNodeToCells(active_node_data, active_node_data->nodes[i]);
        }
    }

    gi.FileClose (f);

    /* rebuild runtime-only data */
    NAV_CalculateDistances(active_node_data);
    active_node_data->modified = false;

    if (nav_debug->value)
    {
//            gi.dprintf("%i nodes loaded, %ik used\n", active_node_data->node_count, ((sizeof(node_t) + 32) * active_node_data->node_count) / 1024);
		gi.dprintf("%i nodes loaded, %ik used\n", active_node_data->node_count, (active_node_data->node_count * 2928 + 4) >> 10);
    }
}

void NAV_FindGoalEnt(node_t *node)
{
    edict_t *ent, *best;
    float bestdist, d;
    vec3_t v;

    if (node->node_type & NODE_BUTTON)
    {
        best = NULL;
        bestdist = 99999.0f;

        for (ent = NULL; (ent = G_Find(ent, FOFS(classname), "func_button")); )
        {
            if (ent->deadflag)
                continue;

            d = VectorDistance(node->origin, ent->mins);
            if (d < bestdist)
            {
                bestdist = d;
                best = ent;
            }

            d = VectorDistance(node->origin, ent->maxs);
            if (d < bestdist)
            {
                bestdist = d;
                best = ent;
            }
        }

        if (best && bestdist < 256.0f)
        {
            node->goal_ent = best;
            best->deadflag = 1;
            best->nav_data.flags |= ND_STATIC;
            best->nav_data.cache_node = node->index;
        }
        else if (developer->value)
        {
            gi.dprintf("NAV: Unable to locate a button to match button node\n");
        }
    }
    else if (node->node_type & NODE_PLAT)
    {
        best = NULL;
        bestdist = 99999.0f;

        for (ent = NULL; (ent = G_Find(ent, FOFS(classname), "path_corner")); )
        {
            if (ent->deadflag)
                continue;

            if (ent->s.origin[2] > node->origin[2])
                continue;

            VectorSubtract(ent->s.origin, node->origin, v);

            d = VectorLength(v);
            if (d < bestdist)
            {
                bestdist = d;
                best = ent;
            }
        }

        if (best && bestdist < 512.0f)
        {
            best->deadflag = 1;
            best->nav_data.cache_node = node->index;
        }
        else
        {
            gi.dprintf("NAV: Unable to locate a path_corner to mark\n");
        }

        for (ent = NULL; (ent = G_Find(ent, FOFS(classname), "func_train")); )
        {
            if (ent->deadflag)
                continue;
            
			VectorSubtract(ent->absmax, node->origin, v);
			v[2] = 0.0f;

            d = VectorLength(v);
            if (d < bestdist)
            {
                bestdist = d;
                best = ent;
            }
        }

        for (ent = NULL; (ent = G_Find(ent, FOFS(classname), "func_lift")); )
        {
            if (ent->deadflag)
                continue;

			VectorSubtract(ent->absmax, node->origin, v);
			v[2] = 0.0f;

            d = VectorLength(v);
            if (d < bestdist)
            {
                bestdist = d;
                best = ent;
            }
        }

        if (best && bestdist < 512.0f)
        {
            node->goal_ent = best;
            best->nav_data.cache_node = node->index;
        }
        else if (developer->value)
        {
            gi.dprintf("NAV: Unable to locate a plat to attach to node\n");
        }
    }
}

void NAV_ReadNode(file_t *f, active_node_data_t *active_node_data, node_t *node)
{
    int i;
    byte packed;
    byte hi, lo;

    gi.FileRead (f, &node->timestamp, sizeof(node->timestamp));
    gi.FileRead (f, node->origin, sizeof(node->origin));
    gi.FileRead (f, node->jump_vel, sizeof(node->jump_vel));
    gi.FileRead (f, &node->node_type, sizeof(node->node_type));
    gi.FileRead (f, &node->goal_index, sizeof(node->goal_index));
    gi.FileRead (f, &node->waterlevel, sizeof(node->waterlevel));
    gi.FileRead (f, &node->yaw, sizeof(node->yaw));

    if (version >= ROUTE_VERSION)
        gi.FileRead (f, &node->cast_group, sizeof(node->cast_group));

    if (version < 2)//ROUTE_VERSION
        gi.FileRead (f, node->visible_nodes, sizeof(short) * 16);
    else
        gi.FileRead (f, node->visible_nodes, sizeof(node->visible_nodes));

    gi.FileRead (f, &node->num_visible, sizeof(node->num_visible));

    if (version < 2)//ROUTE_VERSION
    {
        byte vis;

        for (i = 0; i < active_node_data->node_count; ++i)
        {
            gi.FileRead (f, &vis, sizeof(vis));
            node->routes[i].path = node->visible_nodes[vis];
            node->routes[i].dist = 0;
        }
    }
    else
    {
        i = 0;

        while (i < active_node_data->node_count)
        {
            gi.FileRead (f, &packed, sizeof(packed));

            hi = packed >> 4;
            lo = packed & 0x0F;

            if (hi)
                node->routes[i].path = node->visible_nodes[hi - 1];
            else
                node->routes[i].path = -1;

            node->routes[i].dist = 0;
            ++i;

            if (i >= active_node_data->node_count)
                break;

            if (lo)
                node->routes[i].path = node->visible_nodes[lo - 1];
            else
                node->routes[i].path = -1;

            node->routes[i].dist = 0;
            ++i;
        }
    }

    NAV_FindGoalEnt(node);
}

void NAV_CalculateDistances(active_node_data_t *active_node_data)
{
    node_t *node;
    int i, j;

    if (!active_node_data->node_count)
        return;

    /* PASS 1: direct visible links */
    for (i = 0; i < active_node_data->node_count; ++i)
    {
        node = active_node_data->nodes[i];

        if (!node->num_visible)
            continue;

        for (j = 0; j < node->num_visible; ++j)
        {
            int target = node->visible_nodes[j];

            node->routes[target].path = target;
            node->routes[target].dist = (int)VectorDistance(node->origin, active_node_data->nodes[target]->origin);

            if (node->routes[target].dist == 0)
                node->routes[target].dist = 1;
        }
    }

    /* PASS 2: indirect routes */
    for (i = 0; i < active_node_data->node_count; ++i)
    {
        node = active_node_data->nodes[i];

        if (!node->num_visible)
            continue;

        for (j = 0; j < active_node_data->node_count; ++j)
        {
            if (i == j)
                continue;

            if (node->routes[j].dist != 0)
                continue;

            if (node->routes[j].path < 0)
                continue;

            recursion_count = 0;

            if (NAV_CalculateRouteDistance(active_node_data, node, active_node_data->nodes[j]) < 0)
            {
                node->routes[j].path = -1;
                node->routes[j].dist = 0;
            }
        }
    }
}

int NAV_CalculateRouteDistance(active_node_data_t *active_node_data, node_t *src, node_t *dest)
{
    int dist;
    int idx = dest->index;

    if (src->routes[idx].dist)
        return src->routes[idx].dist;

    if (src->routes[idx].path < 0)
    {
        if (nav_debug->value)
            gi.dprintf("NAV_CalculateRouteDistance: Route has broken chain.\n");

        return -1;
    }

    if (recursion_count++ > active_node_data->node_count + 1)
    {
        if (nav_debug->value)
            gi.dprintf("NAV_CalculateRouteDistance: Recursive link, breaking.\n");

        src->routes[idx].path = -1;
        return -1;
    }

    dist = NAV_CalculateRouteDistance(active_node_data, active_node_data->nodes[src->routes[idx].path], dest);

    if (dist < 0)
    {
        src->routes[idx].path = -1;
        return -1;
    }

    if (dist >= 60000)
        dist = 60000;

    src->routes[idx].dist = src->routes[src->routes[idx].path].dist + (unsigned short)dist;

    return src->routes[idx].dist;
}