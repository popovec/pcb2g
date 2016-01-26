/*
    optim.c

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

    Polyline optimization routines

*/
#include <float.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <math.h>
#include "pcb2g.h"
#include "polyline.h"
#include "vectorize.h"
#include "pgeom.h"


/* 
calculate minimal distance from hole to isolation line
*/
static double
hole_isolation_distance (struct image *image, struct hole *hole)
{
  double dist, min = DBL_MAX;
  struct polyline *ps;
  struct polyline_point *p;

  for (ps = image->first_polyline; ps != NULL; ps = ps->next)
    for (p = ps->points; p->next != NULL; p = p->next)
      {
	dist =
	  lineSegmentToPointDistance2D
	  (i2realX (image, p->x / 2.0),
	   i2realY (image, p->y / 2.0),
	   i2realX (image, p->next->x / 2.0),
	   i2realY (image, p->next->y / 2.0), hole->fx, hole->fy);
	if (dist < min)
	  min = dist;
      }
  hole->hole_to_line = min;
  return min;
}

/* for all holes calculate minimum distance center-isolation line */

static void
holes2isolation (struct image *image)
{
  struct hole *hole, *hole_min = NULL;
  double dist, min = DBL_MAX;

  for (hole = image->first_hole; hole != NULL; hole = hole->next)
    {
      dist = hole_isolation_distance (image, hole);
      if (dist < min)
	{
	  min = dist;
	  hole_min = hole;
	}
    }
  if (hole_min)
    printf ("Minimal hole to line distance %f at position %f %f\n", min,
	    hole_min->fx, hole_min->fy);
  image->hole_line_min = min;

}


static void
traverse_tree (struct image *image, struct polyline_tree *tree,
	       void (*fcion) (struct image * image,
			      struct polyline_tree * tree))
{
  (fcion) (image, tree);
  if (tree->left)
    traverse_tree (image, tree->left, fcion);
  if (tree->right)
    traverse_tree (image, tree->right, fcion);
}


/* check line to all holes distance, if some of distance is below old distance
return 1 to signalize "wrong" optimization" */

static void
holeToLine (struct image *image, struct polyline_tree *tree)
{
  struct hole *h;

  for (h = image->first_hole; h != NULL; h = h->next)
    {
      if ((lineSegmentToPointDistance2D
	   (i2realX (image, tree->start->x / 2.0),
	    i2realY (image, tree->start->y / 2.0),
	    i2realX (image, tree->end->x / 2.0),
	    i2realY (image, tree->end->y / 2.0),
	    h->fx, h->fy) + 0.0000001) < h->hole_to_line)
	{
	  tree->direct = -1;
	  return;
	}
    }
  return;
}

/* calculate statistical values, optimize */

static void
polyline_tree0 (struct image *image, struct polyline_tree *tree)
{
  struct polyline_point *p, *pmax = NULL;
  double dev;
  int prev_x, prev_y;

  tree->diagonal = 1;

  tree->air_len =
    sqrt ((tree->start->x - tree->end->x) * (tree->start->x - tree->end->x) +
	  (tree->start->y - tree->end->y) * (tree->start->y - tree->end->y));

//last 2 points
  if (tree->start->next == tree->end)
    {
      tree->components = 1;
      tree->len = tree->air_len;
      return;
    }

  prev_x = tree->start->x;
  prev_y = tree->start->y;
  tree->components = 0;

  for (p = tree->start->next; p != tree->end; p = p->next)
    {
      dev = lineToPointSDistance2D (tree->start->x, tree->start->y,
				    tree->end->x, tree->end->y, p->x, p->y);

      if (dev > 0)
	tree->plus++;
      else
	tree->minus++;

      //check if line segments alternating over direct line
      if (abs (tree->plus - tree->minus) > 1)
	tree->diagonal = 0;	//not, this is not posible direct line

      tree->aver_dev += dev;
      tree->quad_dev += dev * dev;

      dev = fabs (dev);
      if (dev >= tree->max_dev)
	{
	  tree->max_dev = dev;
	  pmax = p;
	}

      tree->len += sqrt ((p->x - prev_x) * (p->x - prev_x)
			 + (p->y - prev_y) * (p->y - prev_y));
      tree->components++;
      prev_x = p->x;
      prev_y = p->y;
    }

  tree->len += sqrt ((tree->end->x - prev_x) * (tree->end->x - prev_x)
		     + (tree->end->y - prev_y) * (tree->end->y - prev_y));

// line is diagonal .. do more tests
  if (tree->diagonal == 1)
    {
      //first mark this polyline as not diagonal, if line pass all tests, renew diagonal..
      tree->diagonal = 0;

      // line is not diagonal if this polyline not crossing over air line
      if (tree->plus == 0)
	goto not_diagonal;
      if (tree->minus == 0)
	goto not_diagonal;
      //in bressenham line, maximum deviation is 0.5 * sqrt(2) = 1/sqrt(2)
      //warning, all points is pushed from trace in raster 2x2, use sqrt(2) for max dev       
      if (tree->max_dev > /*M_SQRT1_2 */ M_SQRT2)
	goto not_diagonal;
      // deviation sum  must be small enough  (compared to air len)
      // optimize this only if optimization level is over 1
      if (image->route_optimize < 1)
	if (fabs (tree->aver_dev) / tree->air_len > 0.00000001)
	  goto not_diagonal;

      tree->diagonal = 1;
    }
not_diagonal:

  tree->sig = tree->quad_dev / tree->air_len;
//  tree->sig *= tree->len / tree->air_len;
//  tree->sig = tree->max_dev * tree->max_dev;


  if (tree->diagonal == 1)
    {
      //speed up optimization, test if diagonal line does not cross copper
      if (bres_line_test
	  (image, tree->start->x / 2, tree->start->y / 2, tree->end->x / 2,
	   tree->end->y / 2) != -1)
	{
	  //mark posible optimization
	  tree->direct = 1;
	  //check, if some of hole is closer to direct line ..
	  holeToLine (image, tree);
	  //if all ok, do not expand subtree

	  if (tree->direct == 1)
	    return;
	  tree->diagonal = 0;
	}
    }

  if (!pmax)
    return;

  tree->left = calloc (sizeof (struct polyline_tree), 1);
  tree->left->start = tree->start;
  tree->left->end = pmax;
  tree->left->level = 1 + tree->level;
  tree->right = calloc (sizeof (struct polyline_tree), 1);
  tree->right->start = pmax;
  tree->right->end = tree->end;
  tree->right->level = 1 + tree->level;
  polyline_tree0 (image, tree->left);
  polyline_tree0 (image, tree->right);
  return;
}


/* check if this tree can be eliminated by direct line from start to end */

static void
check_line (struct image *image, struct polyline_tree *tree)
{
  struct polyline *ps;
  struct polyline_point *p, *px;

  double new_min;
  double old_min;
  double dist;
  double count;

  old_min = new_min = DBL_MAX;

  //test if hole does not break this optimization
  if (tree->direct < 0)
    return;

  count = bres_line_test
    (image, tree->start->x / 2, tree->start->y / 2, tree->end->x / 2,
     tree->end->y / 2);

//crossing copper ? 
  if (count == -1)
    {
      tree->direct = -1;
      return;
    }

  if (tree->right && tree->left && image->route_optimize > 2)
    if (tree->sig < (tree->right->sig + tree->left->sig))
      tree->direct = 1;

/* check old distance - all lines to line segments in "tree" */
  for (ps = image->first_polyline; ps != NULL; ps = ps->next)
    {
      // TODO self 
      if (tree->start->head == ps)
	continue;
      for (p = ps->points; p != NULL; p = p->next)
	{
	  for (px = tree->start; px->next != tree->end; px = px->next)
	    {
	      dist =
		lineParaSegmentToPointDistance2D (px->x, px->y, px->next->x,
						  px->next->y, p->x, p->y);

	      if (dist < old_min)
		old_min = dist;
	    }
	}
    }

/* check new distance all lines to line from start to end in "tree" */
  for (ps = image->first_polyline; ps != NULL; ps = ps->next)
    for (p = ps->points; p != NULL; p = p->next)
      {
	//skip self 
	if (p == tree->start)
	  {
	    while (p != tree->end)
	      p = p->next;
	    continue;
	  }
	dist =
	  lineParaSegmentToPointDistance2D (tree->start->x, tree->start->y,
					    tree->end->x, tree->end->y, p->x,
					    p->y);
	if (dist < new_min)
	  new_min = dist;
      }
  if (new_min > 0)
    if (new_min >= old_min)
      {
	tree->direct = 1;	//mark posible optimization
	if (tree->start->head->optimized_count < tree->components)
	  tree->start->head->optimized_count = tree->components;
      }
}

static void
polyline_tree (struct image *image)
{

  struct polyline *ps;
  struct polyline_point *p;

/* create tree for this polyline, divide polylines by maximal deviation */
  printf ("OPTIMIZE creating path tree\n");
  for (ps = image->first_polyline; ps != NULL; ps = ps->next)
    {
      //printf ("===========================\n");
      ps->tree = calloc (sizeof (struct polyline_tree), 1);
      ps->tree->start = ps->points;
      for (p = ps->points; p->next != NULL; p = p->next);
      ps->tree->end = p;

      polyline_tree0 (image, ps->tree);
    }
}

/* exclude this subtree from polygon tree */

static void
exclude_tree (struct polyline_tree *tree)
{

  if (tree->left)
    {
      exclude_tree (tree->left);
      free (tree->left);
      tree->left = NULL;
    }
  if (tree->right)
    {
      exclude_tree (tree->right);
      free (tree->right);
      tree->right = NULL;
    }
}

/* exclude and dealloc path components between start and end */
static void
exclude_path_nodes (struct polyline_tree *tree)
{

  struct polyline_point *p, *p_next;

  p = tree->start->next;
  tree->start->next = tree->end;

  while (p != tree->end)
    {
      p_next = p->next;
      free (p);
      p = p_next;
    }
}

static void
exclude_path (struct image *image, struct polyline_tree *tree)
{
  exclude_path_nodes (tree);
  exclude_tree (tree);
}


static void
exclude_direct (struct image *image, struct polyline_tree *tree)
{
  if (tree->direct > 0)
    {
      exclude_path (image, tree);
      tree->direct = -1;
    }
}

void
optim (struct image *image)
{
  struct polyline *ps, *ps_max;
  int max;

  if (image->route_optimize < 0)	// for debug only, do optimization
    return;

  printf ("OPTIMIZE calculating minimal hole to isolation distance\n");
  holes2isolation (image);
  polyline_tree (image);

//always otimize diagonal lines
  printf ("OPTIMIZE: excluding diagonal lines\n");
  for (ps = image->first_polyline; ps != NULL; ps = ps->next)
    traverse_tree (image, ps->tree, &exclude_direct);

//check optimization level.. 
  if (image->route_optimize < 2)
    return;

  printf ("OPTIMIZE checking tree isolation line to holes distances\n");
  for (ps = image->first_polyline; ps != NULL; ps = ps->next)
    traverse_tree (image, ps->tree, &holeToLine);

  for (;;)
    {
      //calculate distances to other points and mark tree->direct by 1 if can be optimized
      printf ("OPTIMIZE checking tree line to line distances\n");
      for (ps = image->first_polyline; ps != NULL; ps = ps->next)
	traverse_tree (image, ps->tree, &check_line);

      max = 0;
      ps_max = NULL;
      for (ps = image->first_polyline; ps != NULL; ps = ps->next)
	{
	  if (ps->optimized_count > max)
	    {
	      ps_max = ps;
	      max = ps->optimized_count;
	    }
	}
      printf ("OPTIMIZE excluding optimized\n");
      if (!ps_max)
	break;

      traverse_tree (image, ps_max->tree, &exclude_direct);
      for (ps = image->first_polyline; ps != NULL; ps = ps->next)
	ps->optimized_count = 0;
      printf ("OPTIMIZE excluded %d\n", max);

    }
}
