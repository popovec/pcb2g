/*
    vectorize.c

    This is part of pcb2g - pcb bitmap to G code converter

    Copyright (C) 2011- 2015 Peter Popovec, popovec@fei.tuke.sk

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Path planer for G code (optimization for rapisd etc..)
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include "pcb2g.h"
#include "tsp.h"
#include "pgeom.h"
#include "polyline.h"
#include "vectorize.h"
#include "post.h"
//#define PATH_DEBUG 1

#ifdef PATH_DEBUG
#define  DPRINT(msg...) printf(msg)
#else
#define  DPRINT(msg...)
#endif

static struct multigraph *
search_mg (struct image *image, int x, int y)
{
  struct multigraph *mg;

  for (mg = image->first_mg; mg != NULL; mg = mg->next)
    if (mg->x == x && mg->y == y)
      return mg;
  return NULL;
}

/*
drake-hougardy:
http://stackoverflow.com/questions/5203894/a-good-approximation-algorithm-for-the-maximum-weight-perfect-match-in-non-bipar

Given: a simple undirected graph G with weighted edges

(0) Define two sets of edges L and R, initially empty.
(1) While the set of edges of G is not empty, do:
(2)    Choose arbitrary vertex v to which an edge is incident.
(3)    While v has incident edges, do:
(4)        Choose heaviest edge {u,v} incident to v.
(5)        Add edge {u,v} to L or R in alternating fashion.
(6)        Remove vertex v (and its incident edges) from G.
(7)        Let u take the role of v.
(8)    Repeat 3.
(9) Repeat 1.

Return L or R, whichever has the greater total weight.

next code is for full graph:
*/

static double
drake_hougardy (struct image *image, int count)
{

  struct multigraph **L[2];
  int index[2];
  double s_len[2];
  struct multigraph *mg1, *mg2, *mg_min;
  double min;
  double len;
  int alternate = 0;

  if (!image->first_mg)
    {
      printf ("drake_hougardy fail, internal error, no graph nodes ?\n");
      exit (1);
    }
  if (!image->first_mg->next)
    {
      printf ("drake_hougardy: no rapids to add, only one graph node\n");
      return 0;
    }

  L[0] = calloc (sizeof (struct multigraph *), count);
  L[1] = calloc (sizeof (struct multigraph *), count);
  index[0] = index[1] = 0;
  s_len[0] = s_len[1] = 0;

  for (mg1 = image->first_mg;;)
    {
      // exclude nodes with even edges
      if ((mg1->count & 1) == 0)
	{
	  mg1 = mg1->next;
	  continue;
	}
      min = DBL_MAX;
      mg_min = NULL;
      for (mg2 = image->first_mg; mg2 != NULL; mg2 = mg2->next)
	{
	  // exclude nodes with even edges
	  if ((mg2->count & 1) == 0)
	    continue;
	  if (mg1 == mg2)
	    continue;
	  if (mg2->dh)
	    continue;
	  len = (mg1->rx - mg2->rx) * (mg1->rx - mg2->rx);
	  len += (mg1->ry - mg2->ry) * (mg1->ry - mg2->ry);
	  len = sqrt (len);
	  if (len < min)
	    {
	      mg_min = mg2;
	      min = len;
	    }
	}
      if (mg_min == NULL)
	{
	  if (alternate)
	    {
	      //this is last node, and can be paired to first node
	      //(first node is already paired but for alternate=0
	      //and for alternate=1 no rapid exist for first node)
	      mg_min = image->first_mg;
	      L[alternate][index[alternate]++] = mg1;
	      L[alternate][index[alternate]++] = mg_min;
	    }
	  else
	    {
	      printf ("drake_hougardy fail, internal error (alternate)\n");
	      exit (1);
	    }
	  mg1->dh = 1;
	  break;
	}
      L[alternate][index[alternate]++] = mg1;
      L[alternate][index[alternate]++] = mg_min;
      s_len[alternate] += min;
      alternate++;
      alternate &= 1;
      mg1->dh = 1;
      mg1 = mg_min;
    }

  for (mg1 = image->first_mg; mg1 != NULL; mg1 = mg1->next)
    // exclude nodes with even edges
    if (mg1->count & 1)
      if (!mg1->dh)
	{
	  printf ("drake_hougardy fail, internal error\n");
	  exit (1);
	}
  printf ("drake_hougardy %f %f %d %d\n", s_len[0], s_len[1], index[0],
	  index[1]);

  if (s_len[0] < s_len[1])
    {
      while (index[0] > 0)
	{
	  L[0][index[0] - 2]->rapid = L[0][index[0] - 1];
	  L[0][index[0] - 1]->rapid = L[0][index[0] - 2];
	  index[0] -= 2;
	}
      free (L[0]);
      free (L[1]);
      DPRINT ("drake len %f\n", s_len[0]);

      return s_len[0];
    }
  while (index[1] > 0)
    {
      L[1][index[1] - 2]->rapid = L[1][index[1] - 1];
      L[1][index[1] - 1]->rapid = L[1][index[1] - 2];
      index[1] -= 2;
    }
  free (L[0]);
  free (L[1]);
  DPRINT ("drake len %f\n", s_len[1]);
  return s_len[1];
}

/*
 polylines is real path for routing PCB division lines, but in nodes with 
 odd edges  rapids is needed. This procedure uses drake-hougardy minimal pairing
 algo to calculate minimal rapid len and add into edges informations about 
 calculated rapids
*/
static void
add_rapids (struct image *image)
{

  struct multigraph *mg;
  int count;
  int rapids = 0;

  for (count = 0, mg = image->first_mg; mg != NULL; count++, mg = mg->next);

  drake_hougardy (image, count);

  for (count = 1, mg = image->first_mg; mg != NULL; count++, mg = mg->next)
    {
      if ((mg->count & 1) == 0)
	continue;
      DPRINT ("node %d count %d [%p] x= %d y=%d  [%p %p %p  rapid %p ]\n",
	      count, mg->count, mg, mg->x, mg->y, mg->edge[0], mg->edge[1],
	      mg->edge[2], mg->rapid);
      if (!mg->rapid)
	rapids++;
    }
  if (rapids > 0)
    {
      printf ("wrong drake_hougardy, (%d) nodes without rapids\n", rapids);
      exit (1);
    }
}


//return start point for graph draw 

static struct multigraph *
next_point (struct image *image, double x, double y)
{

  double min = DBL_MAX, tmp_min;
  struct multigraph *mg, *start = NULL;

  for (mg = image->first_mg; mg != NULL; mg = mg->next)
    {
      if (mg->used)
	continue;

      tmp_min =
	sqrt ((mg->rx - x) * (mg->rx - x) + (mg->ry - y) * (mg->ry - y));

      //this rapid is not needed to go if this point is used as start
      if (mg->rapid)
	{
	  tmp_min -= sqrt ((mg->rx - mg->rapid->rx) * (mg->rx -
						       mg->rapid->rx) +
			   (mg->ry - mg->rapid->ry) * (mg->ry -
						       mg->rapid->ry));
	}

      if (tmp_min <= min)
	{
	  min = tmp_min;
	  start = mg;
	}
    }

  return start;
}

static struct multigraph *
create_graph_node (struct image *image, int x, int y)
{
  struct multigraph *mg;

  mg = calloc (sizeof (struct multigraph), 1);
  mg->next = image->first_mg;
  image->first_mg = mg;
  mg->x = x;
  mg->y = y;
  mg->rx = i2realX (image, x) / 2.0;
  mg->ry = i2realY (image, y) / 2.0;
  return mg;
}

/*
  run over all polylines, for all start/end points of polylines save 
  coordinates and create graph node.
*/

static void
create_graph (struct image *image)
{

  struct polyline *polyline;
  struct multigraph *mg_start, *mg_end;

  if (image->first_mg != NULL)
    {
      //TODO
      printf
	("this can be called only once, at end of code, or clear of multigraph points must be programmed\n");
      exit (0);
    }
  for (polyline = image->first_polyline; polyline != NULL;
       polyline = polyline->next)

    {
      mg_end = search_mg (image, polyline->end_x, polyline->end_y);
      if (!mg_end)
	mg_end = create_graph_node (image, polyline->end_x, polyline->end_y);

      mg_start = search_mg (image, polyline->points->x, polyline->points->y);
      if (!mg_start)
	mg_start =
	  create_graph_node (image, polyline->points->x, polyline->points->y);

      //insert polylines into nodes
      mg_start->edge[mg_start->count] = polyline;
      mg_end->edge[mg_end->count] = polyline;

      //mark nodes as neighbor
      mg_start->neighbor[mg_start->count] = mg_end;
      mg_end->neighbor[mg_end->count] = mg_start;

      //increment count
      mg_start->count++;
      mg_end->count++;

      if (mg_start->count > 4)
	{
	  printf ("too many edges from node, node count=%d\n [%d %d]",
		  mg_start->count, mg_start->x, mg_start->y);
	  exit (1);
	}
      if (mg_end->count > 4)
	{
	  printf ("too many edges from node, node count=%d\n [%d %d]",
		  mg_end->count, mg_end->x, mg_end->y);
	  exit (1);
	}
    }
}

static int
path_dump (struct polyline *ps, struct image *image, int g)
{
  int count = 0;
  struct polyline_point *p;

  for (p = ps->points; p != NULL; p = p->next)
    {
      count++;
      if (g)
	{
	  image->route_last_x = i2realX (image, p->x) / 2.0;
	  image->route_last_y = i2realY (image, p->y) / 2.0;
	  postprocesor_route (image->route_last_x, image->route_last_y);
	}
      if (p->next == NULL)
	{
	  if (p->x != p->head->end_x || p->y != p->head->end_y)
	    {
	      printf
		("internal error, wrong end point in polyline head (%d %d),(%d %d)\n",
		 p->x, p->y, p->head->end_x, p->head->end_y);
	      exit (1);
	    }
	}
    }
  return count;
}

struct g_data
{
  struct multigraph *self;	//pointer to self
  struct multigraph *node;	//to this node
  struct polyline *edge;	//by this edge (if path is missing, use rapid)
  struct g_data *next;
};

static struct g_data *
euler (struct image *image, struct multigraph *mg)
{
  struct g_data *last_g;
  int i;

  last_g = calloc (sizeof (struct g_data), 1);
  last_g->self = mg;

  for (i = 0; i < 4; i++)
    {
      if (mg->edge[i] == NULL)
	continue;
      if (!mg->edge[i]->used)	//get unused path
	{
	  if (mg->edge[i]->end_x == mg->x && mg->edge[i]->end_y == mg->y)
	    polyline_reverse (mg->edge[i]);
	  last_g->node =
	    search_mg (image, mg->edge[i]->end_x, mg->edge[i]->end_y);
	  last_g->edge = mg->edge[i];
	  //we going out from node, decrement edge count  
	  mg->count--;
	  //mark edge as used 
	  last_g->edge->used = 1;
	  //decrement count in incoming node
	  last_g->node->count--;
	  DPRINT ("selected node %p by path %p\n", last_g->node,
		  last_g->edge);
	  mg = last_g->node;
	  return last_g;
	}
    }
  if (mg->rapid)		//exist rapid ?
    {
      DPRINT ("selected node %p rapid\n", mg->rapid);
      last_g->node = mg->rapid;
      mg->rapid = NULL;
      mg = last_g->node;
      mg->rapid = NULL;
      return last_g;
    }

  free (last_g);
  return NULL;
}

static void
dump_graph_component (struct image *image, struct multigraph *mg)
{
  int i = 0;
  struct g_data *first_g = NULL;
  struct g_data *last_g, *rest_g;


  postprocesor_write_comment (" === ROUTING GRAPH COMPONENT === ");

  postprocesor_rapid (i2realX (image, mg->x) / 2.0,
		      i2realY (image, mg->y) / 2.0);

  first_g = euler (image, mg);

  for (last_g = first_g; last_g != NULL; last_g = last_g->next)
    {
      if (last_g->node->count || (last_g->node->rapid))
	{
	  i++;
	  DPRINT ("euler interation %d\n", i);
	  rest_g = last_g->next;
	  for (;;)
	    {
	      mg = last_g->node;
	      DPRINT ("going to node [%p] at %d %d  count %d\n", mg, mg->x,
		      mg->y, mg->count);
	      last_g->next = euler (image, mg);
	      if (!(last_g->next))
		{
		  last_g->next = rest_g;
		  last_g = first_g;
		  break;
		}
	      last_g = last_g->next;
	    }

	}
    }
  for (last_g = first_g; last_g != NULL; last_g = last_g->next)
    if (last_g->node->count)
      {
	printf ("Error, euler in %p count %d rapid %p\n", last_g->node,
		last_g->node->count, last_g->node->rapid);
	exit (1);
      }

  for (last_g = first_g; last_g != NULL; last_g = last_g->next)
    {
      if (last_g->node)
	{
	  last_g->node->used = 1;
	  if (last_g->edge)
	    path_dump (last_g->edge, image, 1);
	  else
	    postprocesor_rapid (i2realX (image, last_g->node->x / 2.0),
				i2realY (image, last_g->node->y / 2.0));
	}
    }

  while (first_g)
    {
      rest_g = first_g->next;
      free (first_g);
      first_g = rest_g;
    }
}

static void
optimized_dump (struct image *image)
{

  struct multigraph *mg;

  create_graph (image);
  add_rapids (image);

  postprocesor_write_comment (" === ROUTING start === ");
  postprocesor_operation (MACHINE_ETCH);
  //do not use last_drill, for now first routing is procesed, then drilling
  //setting image->drill_last creates workaround for now
  // TODO, rename this  variables.. 
  image->drill_last_x = 0;
  image->drill_last_y = 0;

  // first dump circles
  for (mg = image->first_mg; mg != NULL; mg = mg->next)
    if (mg->count == 2 && mg->used == 0)
      {
	//TODO rotate circle to get point in mg at shortest path to etching tool
	dump_graph_component (image, mg);
	//TODO update image->drill_last_x,y
      }

  //get start point for routing
  while ((mg = next_point (image, image->drill_last_x, image->drill_last_y)))
    {
      //clear rapids at both nodes of this subgraph
      if (mg->rapid)
	mg->rapid->rapid = NULL;
      mg->rapid = NULL;
      DPRINT ("starting at [%p] %d %d count=%d\n", mg, mg->x, mg->y,
	      mg->count);
      dump_graph_component (image, mg);
      //TODO update image->drill_last_x,y
    }


  postprocesor_write_comment (" === ROUTING end === ");
  postprocesor_operation (MACHINE_IDLE);

}

static void
free_multigraph (struct image *image)
{
  struct multigraph *mg;

  mg = image->first_mg;
  while (mg)
    {
      image->first_mg = mg->next;
      free (mg);
      mg = image->first_mg;
    }
}

void
dump_lines (struct image *image)
{
  debug_level = 1;
  optim (image);
  optimized_dump (image);
  polyline_free_all (image);
  free_multigraph (image);
  return;
}
