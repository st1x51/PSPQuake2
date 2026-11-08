#include "../g_local.h"

void NAV_DrawLine(vec3_t sorg, vec3_t dorg)
{
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BFG_LASER);
    gi.WritePosition(sorg);
    gi.WritePosition(dorg);
    gi.multicast(sorg, MULTICAST_PHS);
}

float NAV_Debug_DrawPath(edict_t *src, edict_t *dest)
{
    route_t route_out;
    node_t *node;
    node_t *prev;
    float result;
    int steps = 0;

    int route = NAV_Route_EntityToEntity(src, NULL, dest, VIS_PARTIAL, 0, &route_out);

    if (route == ROUTE_NONE)
    {
        NAV_dprintf("NAV_Debug_DrawPath: No path to destination\n");
        return -1.0f;
    }

    if (src->client)
        gi.cprintf(src, PRINT_HIGH, "Path distance = %i\n", route_out.dist);

    if (route == ROUTE_DIRECT)
        return (float)route_out.dist;

    node = level.node_data->nodes[route_out.path];

    result = VectorDistance(src->s.origin, node->origin) + (float)route_out.dist;

    NAV_DrawLine(src->s.origin, node->origin);

    if (NAV_Route_NodeToEntity(node, dest, VIS_PARTIAL, &route_out) != ROUTE_DIRECT)
    {
        while (route_out.path != -1)
        {
            prev = node;
            node = src->active_node_data->nodes[route_out.path];

            NAV_DrawLine(prev->origin, node->origin);

            if (steps++ > 16)
                break;

            if (NAV_Route_NodeToEntity(node, dest, VIS_PARTIAL, &route_out) == ROUTE_DIRECT)
                break;
        }

        if (route_out.path == -1)
        {
            NAV_dprintf("NAV_Debug_DrawPath: broken link in route\n");
            return -1.0f;
        }
    }

    NAV_DrawLine(node->origin, dest->s.origin);

    return result;
}

void NAV_dprintf(char *fmt, ...)
{
    char bigbuffer[0x10000];
    va_list argptr;

    if (!nav_debug->value)
        return;

    va_start(argptr, fmt);
    vsprintf(bigbuffer, fmt, argptr);
    va_end(argptr);

    gi.bprintf(PRINT_HIGH, "%s", bigbuffer);
}

void NAV_RebuildRoutes(active_node_data_t *node_data)
{
    short i, j;

    // clear all routes
    for (i = 0; i < node_data->node_count; i++)
    {
        node_t *node = node_data->nodes[i];

        for (j = 0; j < node_data->node_count; j++)
        {
            node->routes[j].path = -1;
            node->routes[j].dist = 0;
        }
    }

    // build direct visibility routes
    for (i = 0; i < node_data->node_count; i++)
    {
        node_t *node = node_data->nodes[i];

        for (j = 0; j < node->num_visible; j++)
        {
            short target = node->visible_nodes[j];

            node->routes[target].path = j;
            node->routes[target].dist = (short)VectorDistance(node->origin, node_data->nodes[target]->origin);
        }
    }
}

float *NAV_GetCombatPos(edict_t *ent, edict_t *enemy, qboolean melee)
{
    static float last_called = 0.0f;
    int i;
    int cell_x, cell_y;
    short path;
    float *best_org;
    float best_score;
    float cand_score;
    float enemy_dist;
    float dot;
    float min_enemy_dist;
    route_t route_out;
    trace_t tr;
    cell_node_t *cell;
    node_t *cand;
    node_t *node;
    vec3_t start_org;
    vec3_t dir_to_enemy;
    vec3_t prev_org;
    vec3_t step_dir;
    vec3_t cur_enemy_dir;
    vec3_t saved_org;

    if (!enemy)
        return NULL;

    if (last_called == level.time)
        return NULL;

    last_called = level.time;
    ent->last_getcombatpos = level.time;

    if (melee)
    {
        if (VectorDistance(ent->s.origin, enemy->s.origin) > 256.0f)
            return NULL;

        if (!NAV_Route_EntityToEntity(ent, 0, enemy, VIS_PARTIAL, 0, &route_out))
            return NULL;

        if (route_out.dist > 512)
            return NULL;
    }

    VectorSubtract(enemy->s.origin, ent->s.origin, dir_to_enemy);
    enemy_dist = VectorNormalize(dir_to_enemy);

    VectorCopy(ent->s.origin, start_org);

    cell_x = NAV_GetCellIndexForAxis(ent->s.origin[0]);
    cell_y = NAV_GetCellIndexForAxis(ent->s.origin[1]);
    cell = ent->active_node_data->cells[cell_x][cell_y];

    best_score = 999999.0f;
    best_org = NULL;

    while (cell)
    {
        cand = cell->node;
        cell = cell->next;

        if (!ent->nav_build_data && cand->ignore_time >= level.time)
            continue;

        if (cand->cast_group &&
            cand->cast_group != (byte)ent->cast_group &&
            (ent->cast_group != GANG_PLAYER || !ent->leader))
            continue;

        cand_score = VectorDistance(start_org, cand->origin);

        if (cand_score > best_score)
            continue;

        if ((ent->cast_info.aiflags & AI_GRENADE_GUY) &&
            cand->origin[2] - enemy->s.origin[2] < -256.0f)
            continue;

        VectorCopy(cand->origin, ent->s.origin);

        if (!AI_ClearSight(ent, enemy, true))
        {
            VectorCopy(start_org, ent->s.origin);
            continue;
        }

        if (!ValidBoxAtLoc(ent->s.origin, ent->mins, ent->maxs, ent, MASK_MONSTERSOLID | CONTENTS_PLAYERCLIP))
        {
            VectorCopy(start_org, ent->s.origin);
            continue;
        }

        if (melee)
        {
            if (VectorDistance(ent->s.origin, enemy->s.origin) > 256.0f)
            {
                VectorCopy(start_org, ent->s.origin);
                continue;
            }

            if (!NAV_Visible(ent->s.origin, enemy->s.origin, VIS_LINE, cand->node_type & NODE_DUCKING))
            {
                VectorCopy(start_org, ent->s.origin);
                continue;
            }

            if (!NAV_Reachable(ent->s.origin, enemy->s.origin, ent->waterlevel, enemy->waterlevel, cand->node_type & NODE_DUCKING, REACHABLE_POOR))
            {
                VectorCopy(start_org, ent->s.origin);
                continue;
            }
        }

        VectorCopy(start_org, ent->s.origin);

        if (!NAV_Route_EntityToNode(ent, 0, cand, VIS_PARTIAL, 0, 0, &route_out))
            continue;

        if (route_out.dist > cand_score)
        {
            cand_score = route_out.dist;

            if (cand_score > best_score)
                continue;
        }

        if (route_out.path >= 0)
        {
            qboolean reject = false;

            VectorCopy(ent->s.origin, saved_org);
            VectorCopy(start_org, prev_org);

            path = route_out.path;
            node = level.node_data->nodes[path];
            VectorCopy(node->origin, ent->s.origin);

            for (i = 0; !reject; ++i)
            {
                VectorSubtract(ent->s.origin, prev_org, step_dir);
                VectorNormalize(step_dir);
                dot = DotProduct(step_dir, dir_to_enemy);

                if ((VectorDistance(ent->s.origin, enemy->s.origin) < 200.0f ||
                     VectorDistance(prev_org, enemy->s.origin) < 200.0f) &&
                    dot > 0.5f)
                {
                    reject = true;
                    break;
                }

                if (i > 0)
                {
                    VectorSubtract(enemy->s.origin, ent->s.origin, cur_enemy_dir);
                    VectorNormalize(cur_enemy_dir);

                    if (DotProduct(dir_to_enemy, cur_enemy_dir) < -0.5f)
                    {
                        reject = true;
                        break;
                    }
                }

                min_enemy_dist = enemy_dist * 0.5f;
                if (min_enemy_dist < 128.0f)
                    min_enemy_dist = 128.0f;

                if (VectorDistance(ent->s.origin, enemy->s.origin) < min_enemy_dist)
                {
                    reject = true;
                    break;
                }

                tr = gi.trace(prev_org, NULL, NULL, ent->s.origin, ent, MASK_MONSTERSOLID | CONTENTS_PLAYERCLIP);

                if (tr.startsolid || tr.fraction < 1.0f)
                {
                    reject = true;
                    break;
                }

                path = level.node_data->nodes[path]->routes[cand->index].path;

                if (path < 0)
                    break;

                node = level.node_data->nodes[path];
                VectorCopy(ent->s.origin, prev_org);
                VectorCopy(node->origin, ent->s.origin);
            }

            VectorCopy(saved_org, ent->s.origin);

            if (reject)
                continue;
        }

        best_org = cand->origin;
        best_score = cand_score;

        if (cand_score < 256.0f)
            break;
    }

    VectorCopy(start_org, ent->s.origin);
    ent->cover_ent = enemy;

    return best_org;
}

float *NAV_GetHidePos(edict_t *ent, edict_t *enemy, int hidepos_type)
{
    static float last_called = 0.0f;
    int cell_x, cell_y;
    int offset_x, offset_y;
    int i;
    int route_steps;
    short path;
    float *best_org;
    float best_metric;
    float route_metric;
    float dist_to_enemy;
    float enemy_dist;
    float dot;
    float min_enemy_dist;
    float saved_viewheight;
    float saved_maxs2;
    float side_scale;
    float height_scale;
    float scale;
    qboolean can_see_enemy;
    qboolean reject;
    qboolean bad_probe;
    route_t route_out;
    trace_t tr;
    cell_node_t *cell;
    node_t *cand;
    node_t *node;
    vec3_t start_org;
    vec3_t dir_to_enemy;
    vec3_t hide_to_enemy;
    vec3_t side_dir;
    vec3_t prev_org;
    vec3_t step_dir;
    vec3_t cur_enemy_dir;
    vec3_t saved_org;

    if (!enemy)
        return NULL;

    cell_x = NAV_GetCellIndexForAxis(ent->s.origin[0]);
    cell_y = NAV_GetCellIndexForAxis(ent->s.origin[1]);

    /* per-entity cooldown */
    if (level.time - 1.0f < ent->last_gethidepos)
        return NULL;

    /* global one-call-per-frame throttle */
    if (last_called == level.time)
        return NULL;

    last_called = level.time;

    saved_viewheight = ent->viewheight;
    saved_maxs2 = ent->maxs[2];

    /*
    ** If we queried recently, sometimes bias the search into a nearby cell.
    */
    if (level.time - 3.0f < ent->last_gethidepos && random() < 0.3f)
    {
        do
            offset_x = rand() % 9 - 4;
        while (!offset_x);

        cell_x += offset_x;
        if (cell_x < 0)
            cell_x = (cell_x == 0);
        else if (cell_x > 31)
            cell_x = 31 - (cell_x == 31);

        do
            offset_y = rand() % 9 - 4;
        while (!offset_y);

        cell_y += offset_y;
        if (cell_y < 0)
            cell_y = (cell_y == 0);
        else if (cell_y > 31)
            cell_y = 31 - (cell_y == 31);

        if (!ent->active_node_data->cells[cell_x][cell_y])
        {
            cell_x = NAV_GetCellIndexForAxis(ent->s.origin[0]);
            cell_y = NAV_GetCellIndexForAxis(ent->s.origin[1]);
        }
    }

    ent->last_getcombatpos = level.time;
    ent->last_gethidepos = level.time;

    VectorCopy(ent->s.origin, start_org);

    cell = ent->active_node_data->cells[cell_x][cell_y];
    best_org = NULL;
    best_metric = 999999.0f;

    if (hidepos_type == HIDEPOS_FURTHER)
        best_metric = 0.0f;

    VectorSubtract(enemy->s.origin, ent->s.origin, dir_to_enemy);
    enemy_dist = VectorNormalize(dir_to_enemy);

    can_see_enemy = AI_ClearSight(ent, enemy, false);

    while (cell)
    {
        cand = cell->node;
        cell = cell->next;

        ent->maxs[2] = saved_maxs2;
        ent->viewheight = saved_viewheight;

        if (!ent->nav_build_data && cand->ignore_time >= level.time)
            continue;

        if (cand->cast_group &&
            cand->cast_group != (byte)ent->cast_group &&
            (ent->cast_group != GANG_PLAYER || !ent->leader))
            continue;

        /*
        ** Territory preference for gangs.
        */
        if (ent->cast_group > GANG_PLAYER &&
            ent->time_territory_touched != 0.0f &&
            cand->cast_group != (byte)ent->cast_group)
        {
            if (ent->current_territory == ent->cast_group ||
                level.time - 7.0f < ent->time_territory_touched)
                continue;
        }

        route_metric = VectorDistance(start_org, cand->origin);
        dist_to_enemy = VectorDistance(cand->origin, enemy->s.origin);

        if (dist_to_enemy < 128.0f)
            continue;

        if (cand->node_type & NODE_DUCKING)
            dist_to_enemy += dist_to_enemy;

        /*
        ** Hide-position type filtering is based on enemy distance.
        */
        if (hidepos_type == HIDEPOS_CLOSER)
        {
            if (enemy_dist - 48.0f < dist_to_enemy)
                continue;

            if (dist_to_enemy > best_metric)
                continue;
        }
        else if (hidepos_type == HIDEPOS_FURTHER)
        {
            if (enemy_dist + 48.0f > dist_to_enemy)
                continue;

            if (dist_to_enemy < best_metric)
                continue;
        }

        /*
        ** Test directly at the candidate as a tucked hide position.
        */
        VectorCopy(cand->origin, ent->s.origin);
        ent->viewheight = 0;
        ent->maxs[2] = 4.0f;

        /* per-node LOS cache: hide nodes must NOT see the enemy */
        if (cand->last_sight_check != level.time)
        {
            cand->last_sight_check = level.time;
            cand->last_sight_result = AI_ClearSight(ent, enemy, false);
        }

        if (cand->last_sight_result)
            continue;

        /*
        ** If close enough, the box must fit here.
        */
        if (route_metric < 128.0f &&
            !ValidBoxAtLoc(ent->s.origin,
                           ent->mins,
                           ent->maxs,
                           ent,
                           MASK_MONSTERSOLID | CONTENTS_PLAYERCLIP))
        {
            continue;
        }

        /*
        ** Build a horizontal vector away from the enemy and a sideways vector,
        ** then probe small offsets around the hide node.
        **
        ** If any tested nearby offset still has clear sight to the enemy,
        ** reject the candidate.
        */
        hide_to_enemy[0] = ent->s.origin[0] - enemy->s.origin[0];
        hide_to_enemy[1] = ent->s.origin[1] - enemy->s.origin[1];
        hide_to_enemy[2] = 0.0f;
        VectorNormalize(hide_to_enemy);

        side_dir[0] = hide_to_enemy[1];
        side_dir[1] = hide_to_enemy[0];
        side_dir[2] = 0.0f;

        bad_probe = false;

        for (side_scale = -1.0f; side_scale <= 1.1f && !bad_probe; side_scale += 2.0f)
        {
            for (height_scale = -1.0f; height_scale <= 0.6f; height_scale += 1.5f)
            {
                if (rand() & 1)
                    continue;

                scale = side_scale * 16.0f;
                VectorMA(cand->origin, scale, side_dir, ent->s.origin);
                ent->s.origin[2] += height_scale * 20.0f;

                if (AI_ClearSight(ent, enemy, false))
                {
                    bad_probe = true;
                    break;
                }
            }
        }

        ent->viewheight = saved_viewheight;
        ent->maxs[2] = saved_maxs2;
        VectorCopy(start_org, ent->s.origin);

        if (bad_probe)
            continue;

        /*
        ** Route from current position to the hide node.
        */
        if (!NAV_Route_EntityToNode(ent, 0, cand, VIS_PARTIAL, 0, 0, &route_out))
            continue;

        ent->nav_data.goal_index = route_out.path + 1;

        if ((float)route_out.dist > route_metric)
            route_metric = (float)route_out.dist;

        /* Only non-FURTHER mode filters on route metric. */
        if (hidepos_type != HIDEPOS_FURTHER && route_metric > best_metric)
            continue;

        if (route_out.path >= 0)
        {
            reject = false;

            /*
            ** Count hops first; 8 hops is treated as reject.
            */
            path = route_out.path;
            route_steps = 0;
            while (path >= 0)
            {
                path = level.node_data->nodes[path]->routes[cand->index].path;
                route_steps++;
            }

            if (route_steps == 8)
                reject = true;

            if (!reject)
            {
                VectorCopy(ent->s.origin, saved_org);
                VectorCopy(start_org, prev_org);

                path = route_out.path;
                node = level.node_data->nodes[path];
                VectorCopy(node->origin, ent->s.origin);

                for (i = 0; !reject; ++i)
                {
                    /*
                    ** If we currently do not see the enemy, reject route nodes
                    ** that do see the enemy.
                    */
                    if (!can_see_enemy)
                    {
                        if (node->last_sight_check != level.time)
                        {
                            node->last_sight_check = level.time;
                            node->last_sight_result = AI_ClearSight(ent, enemy, false);
                        }

                        if (node->last_sight_result)
                        {
                            reject = true;
                            break;
                        }
                    }

                    VectorSubtract(ent->s.origin, prev_org, step_dir);
                    VectorNormalize(step_dir);
                    dot = DotProduct(step_dir, dir_to_enemy);

                    if (hidepos_type == HIDEPOS_FURTHER || can_see_enemy)
                    {
                        if ((VectorDistance(ent->s.origin, enemy->s.origin) < 200.0f ||
                             VectorDistance(prev_org, enemy->s.origin) < 200.0f) &&
                            dot > 0.5f)
                        {
                            reject = true;
                            break;
                        }

                        if (i > 0)
                        {
                            VectorSubtract(enemy->s.origin, ent->s.origin, cur_enemy_dir);
                            VectorNormalize(cur_enemy_dir);

                            if (DotProduct(dir_to_enemy, cur_enemy_dir) < -0.5f)
                            {
                                reject = true;
                                break;
                            }
                        }
                    }

                    min_enemy_dist = enemy_dist * 0.5f;
                    if (min_enemy_dist < 128.0f)
                        min_enemy_dist = 128.0f;

                    if (VectorDistance(ent->s.origin, enemy->s.origin) < min_enemy_dist)
                    {
                        reject = true;
                        break;
                    }

                    tr = gi.trace(prev_org,
                                  NULL,
                                  NULL,
                                  ent->s.origin,
                                  ent,
                                  MASK_MONSTERSOLID | CONTENTS_PLAYERCLIP);

                    if (tr.startsolid || tr.fraction < 1.0f)
                    {
                        reject = true;
                        break;
                    }

                    path = level.node_data->nodes[path]->routes[cand->index].path;
                    if (path < 0)
                        break;

                    node = level.node_data->nodes[path];
                    VectorCopy(ent->s.origin, prev_org);
                    VectorCopy(node->origin, ent->s.origin);
                }

                VectorCopy(saved_org, ent->s.origin);

                if (reject)
                    continue;
            }
        }

        best_metric = dist_to_enemy;
        best_org = cand->origin;

        /*
        ** Hurt non-player gangs accept the first workable hide spot.
        */
        if (ent->cast_group != GANG_PLAYER && ent->health <= 149)
            break;
    }

    ent->s.origin[0] = start_org[0];
    ent->s.origin[1] = start_org[1];
    ent->s.origin[2] = start_org[2];
    ent->maxs[2] = saved_maxs2;
    ent->viewheight = saved_viewheight;
    ent->cover_ent = enemy;

    return best_org;
}

qboolean NAV_GetAvoidDirection(edict_t *ent, edict_t *avoid, vec3_t dir)
{
	int cell_x, cell_y;
    cell_node_t *cell;
    node_t *best_node;
    float best_dot;
    float dot;
    vec_t *src;
    vec3_t node_dir;
    vec3_t avoid_dir;
    vec3_t best_dir;

    best_node = NULL;
    best_dot = 0.0f;

    src = ent->s.origin;

	cell_x = NAV_GetCellIndexForAxis(ent->s.origin[0]);
	cell_y = NAV_GetCellIndexForAxis(ent->s.origin[1]);
    cell = ent->active_node_data->cells[cell_x][cell_y];

    if (!cell)
        return false;

    do
    {
        cell_node_t *cur = cell;
        cell = cell->next;

        if (!ent->nav_build_data && cur->node->ignore_time >= level.time)
            continue;

        VectorSubtract(cur->node->origin, ent->s.origin, node_dir);
        VectorNormalize(node_dir);

        VectorSubtract(avoid->s.origin, ent->s.origin, avoid_dir);
        VectorNormalize(avoid_dir);

        dot = DotProduct(avoid_dir, node_dir);

        if (dot < -0.2f)
            continue;

        if (!NAV_Visible(src, cur->node->origin, VIS_PARTIAL, cur->node->node_type & NODE_DUCKING))
            continue;

        if ((ent->svflags & SVF_MONSTER) && !NAV_ClearSight(ent, cur->node->origin, NULL))
            continue;

        if (!NAV_Reachable(src, cur->node->origin, ent->waterlevel, cur->node->waterlevel, cur->node->node_type & NODE_DUCKING, REACHABLE_AVERAGE))
            continue;

        if (dot > 0.5f)
            break;

        if (!best_node || best_dot < dot)
        {
            best_dot = dot;
            best_node = cur->node;
            VectorCopy(node_dir, best_dir);
        }
    }
    while (cell);

    if (cell)
    {
        VectorCopy(node_dir, dir);
        return true;
    }

    if (!best_node)
        return false;

    VectorCopy(best_dir, dir);
    return true;
}

float *NAV_GetReachableNodeOutsideBounds(edict_t *ent, vec3_t bmins, vec3_t bmaxs)
{
	int cell_x, cell_y;
    cell_node_t *cell;
    node_t *node;
    vec_t *src;

    /*
    ** Short-term cache reuse (0.5 seconds)
    */
    if (level.time - 0.5f < ent->nav_data.cache_node_time)
    {
        if (ent->nav_data.cache_node >= 0)
            return level.node_data->nodes[ent->nav_data.cache_node]->origin;
    }

    cell_x = NAV_GetCellIndexForAxis(ent->s.origin[0]);
    cell_y = NAV_GetCellIndexForAxis(ent->s.origin[1]);

    cell = ent->active_node_data->cells[cell_x][cell_y];
    if (!cell)
        return NULL;

    src = ent->s.origin;

    while (cell)
    {
        node = cell->node;
        cell = cell->next;

        if (!ent->nav_build_data && node->ignore_time >= level.time)
            continue;

        /*
        ** Skip nodes that are strictly inside the bounds.
        ** Boundary-touching nodes count as outside and are allowed.
        */
        if (!(node->origin[0] <= bmins[0] || node->origin[0] >= bmaxs[0] ||
              node->origin[1] <= bmins[1] || node->origin[1] >= bmaxs[1] ||
              node->origin[2] <= bmins[2] || node->origin[2] >= bmaxs[2]))
        {
            continue;
        }

        if (!NAV_Visible(src, node->origin, VIS_PARTIAL, node->node_type & NODE_DUCKING))
            continue;

        if ((ent->svflags & SVF_MONSTER) && !NAV_ClearSight(ent, node->origin, NULL))
            continue;

        if (!NAV_Reachable(src, node->origin, ent->waterlevel, node->waterlevel, node->node_type & NODE_DUCKING, REACHABLE_THOROUGH))
            continue;

        ent->nav_data.cache_node = node->index;
        ent->nav_data.cache_node_time = level.time;

        return node->origin;
    }

    return NULL;
}