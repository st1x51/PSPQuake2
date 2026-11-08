#include "../g_local.h"

void NAV_CreateCellNode(node_t *node, cell_node_t **list)
{
    cell_node_t *cell;

    cell = gi.TagMalloc(sizeof(cell_node_t), TAG_GAME);

    cell->node = node;
    cell->next = *list;

    *list = cell;
}

int NAV_GetCellIndexForAxis(float pos)
{
    float tmp;
    float half = (float)CELL_AXIS_SUBDIVISION * 0.5f;
    float scale = half / MAX_MAP_AXIS;

    if (pos < -MAX_MAP_AXIS)
        pos = 1 - MAX_MAP_AXIS;
    else if (pos > MAX_MAP_AXIS)
        pos = MAX_MAP_AXIS - 1;

    tmp = pos * scale + half;

    return (int)floor(tmp + 0.5f);
}

void NAV_AddNodeToCells(active_node_data_t *active_node_data, node_t *node)
{
    int cell_x, cell_y;
    float node_x, node_y;
	int row;
	int column;
	float half = (float)CELL_AXIS_SUBDIVISION * 0.5f;
	float scale = half / MAX_MAP_AXIS;

    cell_x = NAV_GetCellIndexForAxis(node->origin[0]);
    cell_y = NAV_GetCellIndexForAxis(node->origin[1]);

    NAV_CreateCellNode(node, &active_node_data->cells[cell_x][cell_y]);

	node_x = node->origin[0] * scale + half;
	node_y = node->origin[1] * scale + half;

	if (cell_x >= node_x)
    {
        if (cell_x > 0)
            row = -1;
        else
            row = 0;
    }
    else
    {
        if (cell_x < CELL_AXIS_SUBDIVISION - 1)
            row = 1;
        else
            row = 0;
    }

    /*
    ** Determine Y direction
    */
    if (cell_y >= node_y)
    {
        if (cell_y > 0)
            column = -1;
        else
            column = 0;
    }
    else
    {
        if (cell_y < CELL_AXIS_SUBDIVISION - 1)
            column = 1;
        else
            column = 0;
    }

    /*
    ** Insert into neighbouring cells (max 4 total)
    */
    if (row)
        NAV_CreateCellNode(node, &active_node_data->cells[cell_x + row][cell_y]);

    if (column)
        NAV_CreateCellNode(node, &active_node_data->cells[cell_x][cell_y + column]);

    if (row && column)
        NAV_CreateCellNode(node, &active_node_data->cells[cell_x + row][cell_y + column]);
}


