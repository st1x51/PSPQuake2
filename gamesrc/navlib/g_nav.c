#include "../g_local.h"

cvar_t	*nav_dynamic;
cvar_t	*nav_debug;
cvar_t	*nav_optimize;

cvar_t	*nav_aipath;

void NAV_InitActiveNodes(active_node_data_t *active_node_data)
{
    // Zero out the active_node_data structure
    memset(active_node_data, 0, sizeof(active_node_data_t));

    // Initialize navigation-related console variables
    nav_dynamic  = gi.cvar("nav_dynamic", "0", 0);
    nav_debug    = gi.cvar("nav_debug", "0", 0);
    nav_optimize = gi.cvar("nav_optimize", "1000", 0);
    nav_aipath   = gi.cvar("nav_aipath", "0", 0);
}

void NAV_PurgeActiveNodes(active_node_data_t *active_node_data)
{
    int i, x, y;
    cell_node_t *cell, *next;

    if (!active_node_data)
        return;

    /*
    ** If dynamic navigation is enabled and the nav data was modified,
    ** write it back out before freeing everything.
    */
    if (nav_dynamic && nav_dynamic->value != 0.0f && active_node_data->modified)
		NAV_WriteActiveNodes(active_node_data, level.mapname);

    /* Free all allocated nodes */
    for (i = 0; i < active_node_data->node_count; i++)
        gi.TagFree(active_node_data->nodes[i]);

    /* Free all cell-node linked lists */
    for (x = 0; x < CELL_AXIS_SUBDIVISION; x++)
    {
        for (y = 0; y < CELL_AXIS_SUBDIVISION; y++)
        {
            cell = active_node_data->cells[x][y];
            while (cell)
            {
                next = cell->next;
                gi.TagFree(cell);
                cell = next;
            }
        }
    }

    /* Free the container itself */
    gi.TagFree(active_node_data);
}

node_t *NAV_CreateNode(edict_t *ent, vec3_t origin, vec3_t jump_vel, short node_type, short goal_index, int waterlevel)
{
    node_t *node;
	int routes;

    if (ent->active_node_data->node_count >= MAX_NODES)
    {
        NAV_dprintf("MAX_NODES reached, unable to create node\n");
        return NULL;
    }

    node = (node_t *)gi.TagMalloc(sizeof(node_t), TAG_GAME);
    memset(node, 0, sizeof(node_t));

    node->index      = ent->active_node_data->node_count;
    node->timestamp  = level.time;

    VectorCopy(origin, node->origin);
    VectorCopy(jump_vel, node->jump_vel);

    node->node_type  = node_type;
    node->goal_index = goal_index;
    node->waterlevel = waterlevel;
    node->yaw        = ent->s.angles[YAW];

    /*
    ** Treat shallow water as non-water if the entity is on the ground.
    */
    
	
	if (node->waterlevel && node->waterlevel <= 2 && (!ent || ent->groundentity))
        node->waterlevel = 0;

    if (ent->last_territory_touched && ent->last_territory_touched->cast_group > 1)
        node->cast_group = ent->last_territory_touched->cast_group;
    else
        node->cast_group = GANG_NUETRAL;

    ent->active_node_data->nodes[ent->active_node_data->node_count++] = node;
    ent->active_node_data->modified = true;

    NAV_CalcNodeSurface(node);
    NAV_AddNodeToCells(ent->active_node_data, node);
    NAV_CalculateVisible(ent->active_node_data, node);
	routes = NAV_CalculateRoutes(ent->active_node_data, node);

    ent->nav_build_data->current_node = node;

    NAV_FindGoalEnt(node);

    if (node_type & NODE_JUMP)
        NAV_dprintf("JUMP ");
    else if (node_type & NODE_LANDING)
        NAV_dprintf("LAND ");
    else if (node_type & NODE_PLAT)
        NAV_dprintf("PLAT ");
    else if (node_type & NODE_TELEPORT)
        NAV_dprintf("TELE ");
    else if (node_type & NODE_BUTTON)
        NAV_dprintf("BUTN ");
    else
        NAV_dprintf("New  ");

    if (node_type & NODE_DUCKING)
        NAV_dprintf(" (ducking) ");

    NAV_dprintf("node #%3i: %2i vis, %3i noroute\n", ent->active_node_data->node_count, node->num_visible, routes);

    if (ent->active_node_data->node_count >= (MAX_NODES - 5))
    {
        gi.cvar_set("nav_dynamic", "0");
        NAV_dprintf("MAX_NODES reached, nav_dynamic disabled.\n");
    }

    return node;
}

void NAV_CalculateVisible(active_node_data_t *active_node_data, node_t *node)
{
    int i;
    int vis;
    int cell_x, cell_y;
    cell_node_t *cell;
    node_t *other;
    float dist;

    for (i = 0; i < MAX_VIS_NODES; i++)
        node->visible_nodes[i] = -1;

    vis = 0;

    if (node->goal_index > -1)
        node->visible_nodes[vis++] = node->goal_index;

    cell_x = NAV_GetCellIndexForAxis(node->origin[0]);
    cell_y = NAV_GetCellIndexForAxis(node->origin[1]);

    cell = active_node_data->cells[cell_x][cell_y];

    while (cell)
    {
        other = cell->node;
        cell = cell->next;

        if (other == node)
            continue;

        if (node->goal_index == other->index)
            continue;

        if (other->goal_index == node->index)
            continue;

        dist = VectorDistance(node->origin, other->origin);

        if (dist > 512.0f)
            continue;

        if (!NAV_Visible(node->origin, other->origin, VIS_PARTIAL, node->node_type & NODE_DUCKING))
            continue;

        if (dist > 48.0f &&
            !NAV_Reachable(node->origin, other->origin, node->waterlevel, other->waterlevel, node->node_type & NODE_DUCKING, REACHABLE_THOROUGH))
            continue;

        node->visible_nodes[vis++] = other->index;

        if (other->num_visible < MAX_VIS_NODES)
            other->visible_nodes[other->num_visible++] = node->index;

        if (vis == MAX_VIS_NODES)
            break;
    }

    node->num_visible = vis;

    if (!vis)
        gi.dprintf("none visible\n");
}

int NAV_CalculateRoutes(active_node_data_t *active_node_data, node_t *node)
{
    qboolean direct_visible;
    int noroute;
    int i, j;

    node_t *dest;
    node_t *best_node;
    node_t *visnode;

    unsigned short best_dist;
    unsigned short new_dist;

    noroute = 0;

    for (i = 0; i < active_node_data->node_count; i++)
    {
        dest = active_node_data->nodes[i];

        node->routes[i].path = -1;
        node->routes[i].dist = 0;

        dest->routes[node->index].path = -1;
        dest->routes[node->index].dist = 0;

        if (i == node->index)
            continue;

        if (!node->num_visible)
            continue;

        best_node = NULL;
        best_dist = 60000;

        for (j = 0; j < node->num_visible; j++)
        {
            visnode = active_node_data->nodes[node->visible_nodes[j]];

            /*
            ** direct path to destination
            */
            if (visnode->index == i)
            {
                best_node = visnode;
                best_dist = (unsigned short)VectorDistance(node->origin, visnode->origin);

                if ((visnode->node_type & NODE_LANDING) &&
                    node->goal_index == visnode->index &&
                    visnode->origin[2] - 48.0f > node->origin[2])
                {
                    best_dist *= 3;
                }

                if (!best_dist)
                    best_dist = 1;

                break;
            }

            /*
            ** indirect route through visible node
            */
            if (visnode->routes[i].path > -1)
            {
                new_dist = (unsigned short)VectorDistance(node->origin, visnode->origin);
                new_dist += visnode->routes[i].dist;

                if (new_dist < best_dist)
                {
                    best_node = visnode;
                    best_dist = new_dist;
                }
            }
        }

        if (!best_node)
        {
            noroute++;
            continue;
        }

        node->routes[i].path = best_node->index;
        node->routes[i].dist = best_dist;

        /*
        ** check if destination can see us directly
        */
        direct_visible = false;

        if (best_node == dest && dest->num_visible)
        {
            for (j = 0; j < dest->num_visible; j++)
            {
                if (dest->visible_nodes[j] == node->index)
                {
                    direct_visible = true;
                    break;
                }
            }
        }

        if (direct_visible)
        {
            dest->routes[node->index].path = node->index;
            dest->routes[node->index].dist = best_dist;
        }
        else if (dest->routes[best_node->index].path > -1)
        {
            dest->routes[node->index].path = dest->routes[best_node->index].path;

            dest->routes[node->index].dist =
                dest->routes[best_node->index].dist +
                (unsigned short)VectorDistance(node->origin, best_node->origin);
        }
    }

    return noroute;
}

int NAV_OptimizeRoutes(active_node_data_t *active_node_data)
{
    static short oi;
    static short oj;
    static float last_break_link;
    static float last_fixing_msg;

    int work_limit;
    int work_done;
    int changed;

    node_t *node;
    node_t *visnode;

    unsigned short new_dist;
    short i;

    work_limit = (int)nav_optimize->value;
    work_done = 0;
    changed = 0;

    for (; oi < active_node_data->node_count; ++oi)
    {
        if (work_done++ > work_limit)
            return changed;

        node = active_node_data->nodes[oi];

        for (; oj < active_node_data->node_count; ++oj)
        {
            if (oj == oi)
                continue;

            if (node->routes[oj].path == oj)
                continue;

            if (node->routes[oj].path > -1)
            {
                short next = node->routes[oj].path;
                short next_path = active_node_data->nodes[next]->routes[oj].path;

                if (next_path == oi)
                {
                    NAV_dprintf("Breaking looped link (%i - %i)\n", oi, next);

                    if (active_node_data->nodes[next]->goal_index != oi)
                        active_node_data->nodes[next]->routes[oj].path = -1;

                    if (node->goal_index != node->routes[oj].path)
                        node->routes[oj].path = -1;

                    last_break_link = level.time;
                }
                else if (next_path == -1)
                {
                    if (level.time - 0.5f > last_fixing_msg)
                    {
                        NAV_dprintf("Fixing broken route (%i - %i)\n", oi, next);
                        last_fixing_msg = level.time;
                    }

                    node->routes[oj].path = -1;
                    last_break_link = level.time;
                }
            }

            if (work_done++ > work_limit)
                return changed;

            if (level.time - 3.0f > last_break_link && node->num_visible)
            {
                for (i = 0; i < node->num_visible; ++i)
                {
                    if (work_done++ > work_limit)
                        return changed;

                    visnode = active_node_data->nodes[node->visible_nodes[i]];

                    if ((visnode->routes[oj].path != -1 || (visnode->index == oj && node->routes[oj].path != oj)) && visnode->routes[oj].path != oi)
                    {
                        new_dist = (unsigned short)VectorDistance(node->origin, visnode->origin);

                        if (node->visible_nodes[i] != oj)
                            new_dist += visnode->routes[oj].dist;

                        if (node->routes[oj].path == -1 ||
                            new_dist < node->routes[oj].dist - 16)
                        {
                            node->routes[oj].path = visnode->index;
                            node->routes[oj].dist = new_dist;
                            active_node_data->modified = true;
                            ++changed;
                        }
                    }
                }
            }
        }

        oj = 0;
    }

    oi = 0;
    return changed;
}

qboolean NAV_Visible(vec3_t src, vec3_t dest, int vis_type, int ducking)
{
    static vec3_t vec_min_partial = { -16.0f, -16.0f, -8.0f };
    static vec3_t vec_min_full    = { -16.0f, -16.0f, -24.0f };
    static vec3_t vec_max_ducking = {  16.0f,  16.0f,  4.0f };
    static vec3_t vec_max_full    = {  16.0f,  16.0f,  48.0f };

    static int trace_mask = MASK_DEADSOLID | CONTENTS_MONSTERCLIP;//0x30003

    trace_t tr;
    vec_t *mins;
    vec_t *maxs;
    int mask;

    if (!vis_type)
    {
        mins = maxs = vec3_origin;
    }
    else
    {
        if (vis_type == VIS_PARTIAL)
            mins = vec_min_partial;
        else
            mins = vec_min_full;

        if (ducking)
            maxs = vec_max_ducking;
        else
            maxs = vec_max_full;
    }

    mask = trace_mask;

    /*
    ** dynamic navigation includes monsters/dead bodies
    */
    if (nav_dynamic->value)
        mask |= MASK_PLAYERSOLID;//0x2010003

	tr = gi.trace(src, mins, maxs, dest, NULL, mask);

    if (tr.ent && VectorCompare(tr.ent->s.origin, src))
    {
        tr = gi.trace(src, mins, maxs, dest, tr.ent, mask);
    }

    if (tr.ent && VectorCompare(tr.ent->s.origin, dest))
        return true;

    return (!tr.startsolid && !tr.allsolid && tr.fraction == 1.0f);
}

qboolean NAV_Reachable(vec3_t src, vec3_t dest, byte src_waterlevel, byte dest_waterlevel, int ducking, int reachable_type)
{
    static vec3_t vec_min_reach   = { -16.0f, -16.0f, -8.0f };
    static vec3_t vec_max_ducking = {  16.0f,  16.0f,  4.0f };
    static vec3_t vec_max_full    = {  16.0f,  16.0f,  32.0f };
    static int trace_mask = MASK_DEADSOLID | CONTENTS_MONSTERCLIP;

    vec3_t point;
    vec3_t dir;
    vec3_t end;
    vec_t *maxs;
    float scale;
    float length;
    float step;
    trace_t tr;

    if (src_waterlevel < 3)
        src_waterlevel = 0;

    if (dest_waterlevel < 3)
        dest_waterlevel = 0;

    if ((src_waterlevel && dest_waterlevel) || (dest_waterlevel && src[2] + 16.0f > dest[2]))
        return true;

    if (ducking)
        maxs = vec_max_ducking;
    else
        maxs = vec_max_full;

    VectorSubtract(dest, src, dir);
    VectorMA(src, 0.5f, dir, point);

    end[0] = point[0];
    end[1] = point[1];
    end[2] = point[2] - 44.0f;

    tr = gi.trace(point, vec_min_reach, maxs, end, NULL, trace_mask);

    if (tr.startsolid || tr.fraction == 1.0f ||
        (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
        return false;

    length = VectorLength(dir);

    /*
    ** keep this float copy if you want to stay close to the decompiled code
    */
    step = reachable_type;

    if (length < step)
        return true;

    VectorNormalize(dir);

    for (scale = 16.0f; scale < length - 16.0f; scale += step)
    {
        VectorMA(src, scale, dir, point);

        end[0] = point[0];
        end[1] = point[1];
        end[2] = point[2] - 44.0f;

        tr = gi.trace(point, vec_min_reach, maxs, end, NULL, trace_mask);

        if (tr.startsolid || tr.fraction == 1.0f || (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
            return false;
    }

    return true;
}

void NAV_CalcNodeSurface(node_t *node)
{
    static vec3_t vec_min = { -16.0f, -16.0f, -24.0f };
    static vec3_t vec_max = {  16.0f,  16.0f,  32.0f };
    static int trace_mask = MASK_DEADSOLID | CONTENTS_MONSTERCLIP;

    trace_t tr;
    vec3_t end;

    VectorCopy(node->origin, end);
    end[2] -= 8.0f;

	tr = gi.trace(node->origin, vec_min, vec_max, end, NULL, trace_mask);

    node->surface = tr.surface;
}


int NAV_GetReachableTypeForEntity(edict_t *ent)
{
    if (ent->nav_build_data)
        return REACHABLE_THOROUGH;

    if (ent->svflags & SVF_MONSTER)
        return REACHABLE_POOR;

    if (ent->item)
        return REACHABLE_AVERAGE;

    return REACHABLE_THOROUGH;
}

qboolean NAV_ClearSight(edict_t *self, vec3_t dest, edict_t *dest_ent)
{
    static vec3_t mins = { -8.0f, -8.0f, 0.0f };
    static vec3_t maxs = {  8.0f,  8.0f, 2.0f };
    static int trace_mask = MASK_PLAYERSOLID | MASK_MONSTERSOLID; //0x2030003

    trace_t tr;

    tr = gi.trace(self->s.origin, mins, maxs, dest, self, trace_mask);

    return (tr.ent == dest_ent || tr.fraction == 1.0f);
}

node_t *NAV_GetClosestNode(edict_t *ent, int vis_type, int ignore_cached, qboolean away_from_enemy)
{
    vec3_t enemy_dir;
    vec3_t node_dir;
    int reachable_type;
    float enemy_dist;
    float dist;
    float best_dist;
    float dot;
	int cell_x, cell_y;

    active_node_data_t *node_data;
    cell_node_t *cell;
    node_t *node;
    node_t *best;

    enemy_dist = 0.0f;

    if (ent->enemy)
    {
        VectorSubtract(ent->enemy->s.origin, ent->s.origin, enemy_dir);
        enemy_dist = VectorNormalize(enemy_dir);
    }

    if (!ignore_cached)
    {
        if (ent->nav_data.flags & ND_STATIC)
        {
            if (ent->nav_data.cache_node)
                return ent->active_node_data->nodes[ent->nav_data.cache_node];

            if (ent->nav_data.cache_node_time != 0.0f &&
                level.time - 3.0f < ent->nav_data.cache_node_time)
                return NULL;
        }
        else
        {
            if (ent->nav_data.cache_node <= -1 ||
                ent->nav_data.cache_node_time == 0.0f)
            {
                if (!ent->nav_build_data &&
                    level.time - 0.2f < ent->nav_data.cache_node_time)
                    return NULL;
            }
            else
            {
                if (level.time - 0.5f < ent->nav_data.cache_node_time)
                    return ent->active_node_data->nodes[ent->nav_data.cache_node];

                node = ent->active_node_data->nodes[ent->nav_data.cache_node];

                if (ent->nav_build_data || node->ignore_time < level.time)
                {
                    if (VectorDistance(ent->s.origin, node->origin) <= 128.0f)
                    {
                        if (NAV_Visible(ent->s.origin, node->origin, VIS_PARTIAL, ent->maxs[2] < 30.0f) &&
                            (!(ent->svflags & SVF_MONSTER) || NAV_ClearSight(ent, node->origin, NULL)))
                        {
                            ent->nav_data.cache_node_time = level.time;
                            return node;
                        }
                    }
                }
            }
        }
    }

    reachable_type = NAV_GetReachableTypeForEntity(ent);

    node_data = ent->active_node_data;

    cell_x = NAV_GetCellIndexForAxis(ent->s.origin[0]);
    cell_y = NAV_GetCellIndexForAxis(ent->s.origin[1]);
    cell = ent->active_node_data->cells[cell_x][cell_y];

    best = NULL;
    best_dist = 999999.0f;

    while (cell)
    {
        node = cell->node;
        cell = cell->next;

        if (!ent->nav_build_data && node->ignore_time >= level.time)
            continue;

        VectorSubtract(node->origin, ent->s.origin, node_dir);
        dist = VectorNormalize(node_dir);

        if ((ent->svflags & SVF_MONSTER) && ent->enemy && away_from_enemy)
        {
            VectorSubtract(ent->enemy->s.origin, ent->s.origin, enemy_dir);

            if (VectorNormalize(enemy_dir) < 2048.0f)
            {
                dot = DotProduct(node_dir, enemy_dir);

                if (dot > 0.0f)
                    dist = (dot * 3.0f + 1.0f) * (dist + 256.0f);
            }
        }

        if (dist <= best_dist &&
            NAV_Visible(ent->s.origin, node->origin, vis_type, node->node_type & NODE_DUCKING) &&
            (!(ent->svflags & SVF_MONSTER) ||
             NAV_ClearSight(ent, node->origin, NULL)))
        {
            if (NAV_Reachable(ent->s.origin, node->origin, ent->waterlevel, node->waterlevel, node->node_type & NODE_DUCKING, reachable_type))
            {
                best = node;
                best_dist = dist;

                if (dist < 64.0f)
                    break;
            }
        }
    }

    if (best)
        ent->nav_data.cache_node = best->index;
    else
        ent->nav_data.cache_node = -1;

    ent->nav_data.cache_node_time = level.time;

    return best;
}

int NAV_Route_EntityToEntity(edict_t *src, node_t *current_node, edict_t *dest, int vis_type, int check_all_nodes, route_t *route_out)
{
    node_t *dest_node;
    node_t *src_node;
    node_t *node;
    float max_dist;
    float dist;
    int reachable_type;
    qboolean ducking;
    int i;

    if (dest->client)
        max_dist = 1024.0f;
    else
        max_dist = 384.0f;

    dist = VectorDistance(src->s.origin, dest->s.origin);
    ducking = (src->maxs[2] < 30.0f);

    /*
    ** Try direct path first
    */
    if (dist < max_dist && (!src->nav_build_data || dist < 64.0f) &&
        NAV_Visible(src->s.origin, dest->s.origin, vis_type, ducking) &&
        (!(src->svflags & SVF_MONSTER) ||
         NAV_ClearSight(src, dest->s.origin, dest)))
    {
        reachable_type = NAV_GetReachableTypeForEntity(src);

        if (NAV_Reachable(src->s.origin, dest->s.origin, src->waterlevel, dest->waterlevel, ducking, reachable_type))
        {
            route_out->dist = (unsigned short)dist;
            route_out->path = -1;
            return ROUTE_DIRECT;
        }
    }

    /*
    ** Resolve destination node
    */
    dest_node = NAV_GetClosestNode(dest, vis_type, 0, false);
    if (!dest_node)
    {
        route_out->path = -1;
        return ROUTE_NONE;
    }

    /*
    ** Resolve source node
    */
    src_node = current_node;
    if (!src_node)
    {
        src_node = NAV_GetClosestNode(src, vis_type, check_all_nodes, false);
        if (!src_node)
        {
            route_out->path = -1;
            return ROUTE_NONE;
        }
    }

    /*
    ** Same node = direct
    */
    if (src_node == dest_node)
    {
        route_out->dist = (unsigned short)dist;
        route_out->path = -1;
        return ROUTE_DIRECT;
    }

    /*
    ** No route from source node
    */
    if (src_node->routes[dest_node->index].path < 0)
    {
        route_out->path = -1;
        return ROUTE_NONE;
    }

    /*
    ** If current_node was supplied, immediately advance to the first routed hop
    */
    if (current_node)
        src_node = src->active_node_data->nodes[src_node->routes[dest_node->index].path];

    /*
    ** Look-ahead optimisation: up to 2 hops
    */
    if (src_node->routes[dest_node->index].path >= 0)
    {
        node = src->active_node_data->nodes[src_node->routes[dest_node->index].path];

        for (i = 0; i < 2 && node; i++)
        {
            if (NAV_Visible(src->s.origin, node->origin, vis_type, ducking))
            {
                reachable_type = NAV_GetReachableTypeForEntity(src);

                if (NAV_Reachable(src->s.origin, node->origin, src->waterlevel, node->waterlevel, ducking, reachable_type))
                {
                    src_node = node;
                }
            }

            if (node->routes[dest_node->index].path < 0)
                break;

            node = src->active_node_data->nodes[node->routes[dest_node->index].path];
        }
    }

    route_out->path = src_node->index;
    route_out->dist = src_node->routes[dest_node->index].dist;
    return ROUTE_INDIRECT;
}

int NAV_Route_NodeToEntity(node_t *node, edict_t *dest, int vis_type, route_t *route_out)
{
    node_t *dest_node;
    float dist;
    int reachable_type;
    qboolean ducking;

    dist = VectorDistance(node->origin, dest->s.origin);
    ducking = (node->node_type == NODE_DUCKING);

    /*
    ** Direct route
    */
    if (dist < 400.0f &&
        (!dest->nav_build_data || dist < 64.0f) &&
        NAV_Visible(node->origin, dest->s.origin, vis_type, ducking))
    {
        reachable_type = NAV_GetReachableTypeForEntity(dest);

        if (NAV_Reachable(node->origin, dest->s.origin, node->waterlevel, dest->waterlevel, ducking, reachable_type))
        {
            route_out->dist = (unsigned short)dist;
            route_out->path = -1;
            return ROUTE_DIRECT;
        }
    }

    /*
    ** Fallback via nav nodes
    */
    dest_node = NAV_GetClosestNode(dest, vis_type, dest->nav_build_data != 0, false);

    if (!dest_node)
    {
        route_out->path = -1;
        return ROUTE_NONE;
    }

    /*
    ** Same node means direct
    */
    if (node == dest_node)
    {
        route_out->dist = (unsigned short)dist;
        route_out->path = -1;
        return ROUTE_DIRECT;
    }

    /*
    ** Route table lookup
    */
    if (node->routes[dest_node->index].path < 0)
    {
        route_out->path = -1;
        return ROUTE_NONE;
    }

    route_out->path = node->routes[dest_node->index].path;
    route_out->dist = node->routes[dest_node->index].dist;
    return ROUTE_INDIRECT;
}

int NAV_Route_EntityToNode(edict_t *src, node_t *current_node, node_t *dest_node, int vis_type, int check_all_nodes, int check_direct, route_t *route_out)
{
    node_t *src_node;
    node_t *scan_node;
    float dist;
    int reachable_type;
    int i;
    qboolean ducking;

    ducking = (src->maxs[2] < 30.0f);

    /*
    ** Optional direct route check
    */
    if (check_direct)
    {
        dist = VectorDistance(src->s.origin, dest_node->origin);

        if (dist < 400.0f &&
            (!src->nav_build_data || dist < 64.0f) &&
            NAV_Visible(src->s.origin, dest_node->origin, vis_type, ducking) &&
            (!(src->svflags & SVF_MONSTER) ||
             NAV_ClearSight(src, dest_node->origin, NULL)))
        {
            reachable_type = NAV_GetReachableTypeForEntity(src);

            if (NAV_Reachable(src->s.origin, dest_node->origin, src->waterlevel, dest_node->waterlevel, ducking, reachable_type))
            {
                route_out->dist = (unsigned short)dist;
                route_out->path = -1;
                return ROUTE_DIRECT;
            }
        }
    }

    /*
    ** Resolve source node
    */
    src_node = current_node;
    if (!src_node)
    {
        src_node = NAV_GetClosestNode(src, vis_type, check_all_nodes, false);
        if (!src_node)
        {
            route_out->path = -1;
            return ROUTE_NONE;
        }
    }

    /*
    ** Route table must contain a path to dest_node
    */
    if (src_node->routes[dest_node->index].path < 0)
    {
        route_out->path = -1;
        return ROUTE_NONE;
    }

    /*
    ** If current_node was supplied, immediately step to its first routed hop
    */
    if (current_node)
        src_node = src->active_node_data->nodes[src_node->routes[dest_node->index].path];

    /*
    ** Look-ahead optimisation:
    ** try to advance up to 5 nodes if src can already directly see/reach farther route nodes
    */
    if (src_node->routes[dest_node->index].path >= 0)
    {
        scan_node = src->active_node_data->nodes[src_node->routes[dest_node->index].path];

        for (i = 0; i < 5 && scan_node; i++)
        {
            if (NAV_Visible(src->s.origin, scan_node->origin, vis_type, ducking))
            {
                reachable_type = NAV_GetReachableTypeForEntity(src);

                if (NAV_Reachable(src->s.origin, scan_node->origin, src->waterlevel, scan_node->waterlevel, ducking, reachable_type))
                {
                    src_node = scan_node;
                }
            }

            if (scan_node->routes[dest_node->index].path < 0)
                break;

            scan_node = src->active_node_data->nodes[scan_node->routes[dest_node->index].path];
        }
    }

    route_out->path = src_node->index;
    route_out->dist = src_node->routes[dest_node->index].dist;
    return ROUTE_INDIRECT;
}

void NAV_EvaluateMove(edict_t *ent)
{
    route_t route_entity;
    route_t route_node;
    node_t *node1;
    node_t *node2;
    edict_t *jump_ent;
    int route;
    vec3_t temp_vector;
    vec3_t jump_vel;
    short node_type = NODE_NORMAL;

    if (!ent->nav_build_data)
        return;

    /*
    ** Invalid builder state
    */
    if (ent->movetype == MOVETYPE_NOCLIP || ent->solid != SOLID_BBOX)
    {
        ent->nav_build_data->flags &= ~NBD_JUMPING;
        ent->nav_build_data->current_node = NULL;
        return;
    }

    /*
    ** Stop building in unsupported situations
    */
    if (ent->active_node_data->node_count >= 695 || ent->waterlevel > 2)
    {
        ent->nav_build_data->flags &= ~NBD_JUMPING;
        return;
    }

    /*
    ** Ensure we have a current node
    */
    if (!ent->nav_build_data->current_node)
    {
        ent->nav_build_data->current_node = NAV_GetClosestNode(ent, VIS_PARTIAL, true, false);

        if (!ent->nav_build_data->current_node)
        {
            NAV_CreateNode(ent, ent->s.origin, vec3_origin, NODE_NORMAL, -1, ent->waterlevel);

            ent->nav_build_data->old_groundentity = ent->groundentity;
			ent->nav_build_data->last_max_z = ent->maxs[2];
			VectorCopy(ent->s.origin, ent->nav_build_data->old_org);
            return;

        }
    }

    /*
    ** Ignore moving ground entities (platforms, movers, etc.)
    */
    if (ent->groundentity && ent->groundentity->use && !VectorCompare(ent->groundentity->velocity, vec3_origin))
    {
		ent->nav_build_data->old_groundentity = ent->groundentity;
		ent->nav_build_data->last_max_z = ent->maxs[2];
		VectorCopy(ent->s.origin, ent->nav_build_data->old_org);
		return;
    }

    /*
    ** Track ducking / standing transition
    */
    if (ent->maxs[2] != ent->nav_build_data->last_max_z)
	{
		if (ent->maxs[2] >= 30.0f)
		{
			if (ent->nav_build_data->flags & NBD_DUCK_NEWPATH)
			{
				NAV_CreateNode(ent, ent->s.origin, vec3_origin, NODE_DUCKING, -1, ent->waterlevel);
				ent->nav_build_data->flags &= ~NBD_DUCK_NEWPATH;
			}
		}
		else
		{
			VectorCopy(ent->s.origin, ent->nav_build_data->ducking_org);
		}
	}

    /*
    ** Jump/fall start
    */
    if (!ent->groundentity && ent->waterlevel < 3 && ent->nav_build_data->old_groundentity)
    {
        VectorCopy(ent->nav_build_data->old_org, ent->nav_build_data->jump_ent->s.origin);
        VectorCopy(ent->velocity, ent->nav_build_data->jump_ent->velocity);
        VectorCopy(ent->s.angles, ent->nav_build_data->jump_ent->s.angles);

        ent->nav_build_data->jump_ent->waterlevel = ent->waterlevel;
        ent->nav_build_data->flags |= NBD_JUMPING;
    }
    else
    {
        /*
        ** Landing after jump/fall
        */
        jump_ent = ent->nav_build_data->jump_ent;

        
		if (ent->nav_build_data->old_groundentity &&
			ent->groundentity &&
			(ent->nav_build_data->flags & NBD_JUMPING) &&
			jump_ent &&
			(jump_ent->s.origin[2] - ent->s.origin[2] < 512.0f) &&
			!(NAV_Route_EntityToEntity(jump_ent, NULL, ent, VIS_PARTIAL, true, &route_entity) &&
			  NAV_Route_NodeToEntity(ent->nav_build_data->current_node, ent, VIS_PARTIAL, &route_node) &&
			  route_entity.dist <= (short)(VectorDistance(jump_ent->s.origin, ent->s.origin) * 4.0f + 512.0f)))
		{

            if (jump_ent && jump_ent->s.origin[2] > ent->s.origin[2] &&
                NAV_Visible(jump_ent->s.origin, ent->s.origin, VIS_PARTIAL, ent->maxs[2] < 30.0f) &&
                NAV_Reachable(jump_ent->s.origin, ent->s.origin, jump_ent->waterlevel, ent->waterlevel, ent->maxs[2] < 30.0f, REACHABLE_THOROUGH) &&
                NAV_Reachable(ent->s.origin, jump_ent->s.origin, ent->waterlevel, jump_ent->waterlevel, ent->maxs[2] < 30.0f, REACHABLE_THOROUGH))
            {
                NAV_CreateNode(ent, jump_ent->s.origin, vec3_origin, NODE_DUCKING * (ent->maxs[2] < 30.0f), -1, jump_ent->waterlevel);

                NAV_CreateNode(ent, ent->s.origin, vec3_origin, NODE_DUCKING * (ent->maxs[2] < 30.0f), -1, ent->waterlevel);

                ent->nav_build_data->flags &= ~NBD_JUMPING;
            }
            else if (jump_ent)
            {
                node1 = NAV_CreateNode(ent, ent->s.origin, vec3_origin, (NODE_DUCKING * (ent->maxs[2] < 30.0f)) | NODE_LANDING, -1, ent->waterlevel);

                NAV_CreateNode(ent, jump_ent->s.origin, jump_ent->velocity, (NODE_DUCKING * (ent->maxs[2] < 30.0f)) | NODE_JUMP, node1->index, jump_ent->waterlevel);

                /*
                ** Reverse jump
                */
                if (jump_ent->s.origin[2] < (ent->s.origin[2] + 32.0f) - (VectorDistance(jump_ent->s.origin, ent->s.origin) * 0.5f))
                {
                    VectorScale(ent->velocity, -2.0f, jump_vel);

                    if (jump_vel[2] > 100.0f)
                        jump_vel[2] = 310.0f;
                    else if (jump_vel[2] < 80.0f)
                        jump_vel[2] = 80.0f;

                    level.time -= 0.01f;

                    node2 = NAV_CreateNode(ent, jump_ent->s.origin, vec3_origin, (NODE_DUCKING * (ent->maxs[2] < 30.0f)) | NODE_LANDING, -1, jump_ent->waterlevel);

                    NAV_CreateNode(ent, ent->s.origin, jump_vel, (NODE_DUCKING * (ent->maxs[2] < 30.0f)) | NODE_JUMP, node2->index, ent->waterlevel);

                    NAV_CalculateRoutes(ent->active_node_data, node2);
                    level.time += 0.01f;
                }

                NAV_CalculateRoutes(ent->active_node_data, node1);
                ent->nav_build_data->flags &= ~NBD_JUMPING;
            }
        }
        else if ((ent->nav_build_data->flags & NBD_JUMPING) && ent->groundentity)
        {
            ent->nav_build_data->flags &= ~NBD_JUMPING;
        }
    }

    /*
    ** Validate current node
    */
    VectorSubtract(ent->nav_build_data->current_node->origin, ent->s.origin, temp_vector);

    if (ent->groundentity &&
        (VectorLength(temp_vector) > 128.0f ||
         !NAV_Visible(ent->s.origin, ent->nav_build_data->current_node->origin, VIS_FULL, ent->nav_build_data->current_node->node_type & NODE_DUCKING) ||
         !NAV_Reachable(ent->nav_build_data->current_node->origin, ent->s.origin,
                        ent->nav_build_data->current_node->waterlevel, ent->waterlevel,
                        ent->nav_build_data->current_node->node_type & NODE_DUCKING, REACHABLE_THOROUGH)))
    {
        route = NAV_Route_NodeToEntity(ent->nav_build_data->current_node, ent, VIS_PARTIAL, &route_entity);

    	if (route == ROUTE_NONE ||
            (route == ROUTE_INDIRECT &&
             VectorDistance(ent->active_node_data->nodes[ent->nav_data.cache_node]->origin, ent->s.origin) > 128.0f) ||
            (route == ROUTE_DIRECT &&
             VectorDistance(ent->nav_build_data->current_node->origin, ent->s.origin) > 128.0f))
        {
            if (ent->maxs[2] < 30.0f)
            {
                node_type = NODE_DUCKING;

                if (!(ent->nav_build_data->flags & NBD_DUCK_NEWPATH))
                {
                    NAV_CreateNode(ent, ent->nav_build_data->ducking_org, vec3_origin, NODE_DUCKING, -1, ent->waterlevel);
                    ent->nav_build_data->flags |= NBD_DUCK_NEWPATH;
                }
            }

            NAV_CreateNode(ent, ent->nav_build_data->old_org, vec3_origin, node_type, -1, ent->waterlevel);
        }
        else if (route == ROUTE_INDIRECT)
        {
            if (VectorDistance(ent->active_node_data->nodes[ent->nav_data.cache_node]->origin, ent->s.origin) <
                VectorDistance(ent->nav_build_data->current_node->origin, ent->s.origin))
            {
                ent->nav_build_data->current_node = ent->active_node_data->nodes[ent->nav_data.cache_node];
            }
        }
    }

    ent->nav_build_data->old_groundentity = ent->groundentity;
    ent->nav_build_data->last_max_z = ent->maxs[2];
    VectorCopy(ent->s.origin, ent->nav_build_data->old_org);
}
