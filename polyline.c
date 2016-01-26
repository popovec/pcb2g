/*
    polyline.c

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

    Polyline creating/changing/deleting/searching functions

*/
#include <stdlib.h>
#include "pcb2g.h"
#include "polyline.h"

//#define PATH_DEBUG
#ifdef PATH_DEBUG
#define  DPRINT(msg...) printf(msg)
#else
#define  DPRINT(msg...)
#endif

/***
   * \brief generate debug output 
   *
   * print all polylines to stdout
   */

void
polylines_dump (struct image *image)
{
  struct polyline *poly;

  for (poly = image->first_polyline; poly != NULL; poly = poly->next)
    printf ("P_DUMP start %d %d end %d %d\n", poly->points->x,
	    poly->points->y, poly->end_x, poly->end_y);
}

/***
   * \brief delete all points in polyline
   *
   *
   */

static void
polyline_remove_all_points (struct polyline *p)
{
  struct polyline_point *p_old, *p_point = p->points;

  while (p_point)
    {
      p_old = p_point;
      p_point = p_point->next;
      free (p_old);
    }
}

/***
   * \brief delete polyline tree data
   *
   *
   */

static void
free_polyline_tree (struct polyline_tree *t)
{
  if (!t)
    return;

  if (t->left)
    {
      free_polyline_tree (t->left);
      free (t->left);
    }
  if (t->right)
    {
      free_polyline_tree (t->right);
      free (t->right);
    }
}

/***
   * \brief delete last added polyline
   *
   * \return parameter t 
   */

void
polyline_delete_default (struct image *image)
{
  struct polyline *p_line;

  if (!image->first_polyline)
    return;

  polyline_remove_all_points (image->first_polyline);
  if (image->first_polyline->tree)
    free_polyline_tree (image->first_polyline->tree);


  p_line = image->first_polyline->next;
  free (image->first_polyline);
  image->first_polyline = p_line;
}

/***
   * \brief find polyline that match starting point x,y
   *
   * \return struct polyline *
   */

struct polyline_point *
polyline_search_at_start (struct image *image, int x, int y)
{
  struct polyline *p_line;

  for (p_line = image->first_polyline; p_line != NULL; p_line = p_line->next)
    {
      if (p_line->points->x == x && p_line->points->y == y)
	return p_line->points;
    }
  return NULL;
}

/*
  * \brief replace start point for matching polyline

  search all polylines start point, if matching point is found, 
  replace coordinates by new values
  NOT USED, use polyline_find_append()... because  rewrite remove
  ponts if polylines crossing diagonaly .. 

   * \return 1 if start point is replaced 0 if not found
 
*/

int
polyline_rewrite (struct image *image, int x, int y, int rx, int ry)
{
  struct polyline_point *p_point;

  p_point = polyline_search_at_start (image, x, y);
  if (p_point)
    {
      p_point->x = rx;
      p_point->y = ry;
      return 1;
    }
  return 0;
}

/*
  * \brief search polyline that match  start point and append point

  search all polylines start point, if matching point is found,
  append new point to this polyline

   * \return 1 if point is appended 0 if not found
*/

int
polyline_find_append (struct image *image, int x, int y, int rx, int ry)
{
  struct polyline_point *p_point, *a_point;

  p_point = polyline_search_at_start (image, x, y);
  if (p_point)
    {
      a_point = calloc (sizeof (struct polyline_point), 1);
      a_point->head = p_point->head;
      a_point->x = rx;
      a_point->y = ry;
      a_point->next = p_point;
      p_point->head->points = a_point;
      return 1;
    }
  return 0;
}

/*
  * \brief append point to last created polyline
  append point to default polyline (default is last created polyline)
*/

void
polyline_append (struct image *image, int x, int y)
{
  struct polyline_point *p_point;

  DPRINT ("appending to %d %d\n", x, y);
  p_point = calloc (sizeof (struct polyline_point), 1);
  p_point->head = image->first_polyline;
  p_point->x = x;
  p_point->y = y;
  p_point->next = p_point->head->points;
  p_point->head->points = p_point;
}

/*
 * \brief append/extend point to last created polyline
 *
 *  append point to default polyline, but if point can extend
 *  previous line, only rewrite latest point..
*/
void
polyline_extend (struct image *image, int x, int y)
{

  struct polyline_point *p_point;

  p_point = image->first_polyline->points;
  //test if minimum two point exists
  if (p_point->next != NULL)
    {
      int k1c, k2c, k1m, k2m;

      k1c = x - p_point->x;
      k1m = y - p_point->y;
      k2c = p_point->next->x - p_point->x;
      k2m = p_point->next->y - p_point->y;

      if (k1c * k2m == k2c * k1m)
	{
	  DPRINT ("extending to %d %d\n", x, y);
	  p_point->x = x;
	  p_point->y = y;

	  return;
	}
    }

  polyline_append (image, x, y);
}

/*
create new polyline start point, this polyline is set as "default"
this point is not checkd if is a part f other polyline!
*/
void
polyline_start (struct image *image, int x, int y)
{
  struct polyline *p_line;

  DPRINT ("starting polyline %d %d\n", x, y);
  p_line = calloc (sizeof (struct polyline), 1);
  p_line->next = image->first_polyline;
  p_line->end_x = x;
  p_line->end_y = y;
  image->first_polyline = p_line;
  polyline_append (image, x, y);
}


void
polyline_reverse (struct polyline *ps)
{
  struct polyline_point *tmp_point, *rest_points;

  ps->end_x = ps->points->x;
  ps->end_y = ps->points->y;

  rest_points = ps->points;
  ps->points = NULL;

  while (rest_points)
    {
      tmp_point = rest_points->next;
      rest_points->next = ps->points;
      ps->points = rest_points;
      rest_points = tmp_point;
    }
}


void
polyline_free_all (struct image *image)
{
  struct polyline *p, *p_old;

  for (p = image->first_polyline; p != NULL;)
    {
      polyline_remove_all_points (p);
      free_polyline_tree (p->tree);
      free (p->tree);
      p_old = p->next;
      free (p);
      p = p_old;
    }
}

static void
polyline_delete (struct image *image, struct polyline *p_del)
{
  struct polyline **p;

  for (p = &(image->first_polyline); (*p) != NULL; p = &((*p)->next))
    {
      if ((*p) == p_del)
	{
	  polyline_remove_all_points (p_del);
	  free_polyline_tree (p_del->tree);
	  (*p) = p_del->next;
	  free (p_del);
	  return;
	}
    }
}

/*
  search all polylines, if excatly two can be joined in x,y point, do it
*/
static int
polyline_join_in_point (struct image *image, int x, int y)
{

  struct polyline *p, *p1 = NULL, *p2 = NULL;

  for (p = image->first_polyline; p != NULL; p = p->next)
    {

      if (p->points->x == x && p->points->y == y)
	{
	  if (p1 == NULL)
	    p1 = p;
	  else if (p2 == NULL)
	    p2 = p;
	  else
	    return -1;
	}

      if (p->end_x == x && p->end_y == y)
	{
	  polyline_reverse (p);
	  if (p1 == NULL)
	    p1 = p;
	  else if (p2 == NULL)
	    p2 = p;
	  else
	    return -1;
	}
    }
  if (p1 != NULL && p2 != NULL)
    {
      struct polyline_point *p_point;

      while (p1->points)
	{
	  p_point = p1->points;
	  p1->points = p1->points->next;
	  p_point->next = p2->points;
	  p2->points = p_point;
	  p_point->head = p2;
	}
      polyline_delete (image, p1);
      return 1;
    }
  return 0;
}

/*

  m_line is special line created by trace algo. This polyline is "special" because
  consist exactly by 3 points. All points is on one line, 2nd point is exactly
  on middle of start and end of polyline. Some of m_lines can be removed at end
  of trace process.
  
*/

void
polyline_delete_m_line (struct image *image, int x1, int y1, int x2, int y2)
{
  struct polyline *p;
  struct polyline_point *p_point;

  for (p = image->first_polyline; p != NULL; p = p->next)
    {
      if (p->points == NULL)
	continue;
      p_point = p->points;	//start
      if (p_point->next == NULL)
	continue;
      p_point = p_point->next;	//2nd
      if (p_point->next == NULL)
	continue;
      if (p_point->next->next != NULL)
	continue;
      if (p->end_x != p_point->next->x)
	continue;
      if (p->end_y != p_point->next->y)
	continue;
      if (p->points->x + p->end_x != 2 * p_point->x)
	continue;
      if (p->points->y + p->end_y != 2 * p_point->y)
	continue;

      if ((x1 == p->points->x && y1 == p->points->y
	   && x2 == p->end_x && y2 == p->end_y) ||
	  (x2 == p->points->x && y2 == p->points->y
	   && x1 == p->end_x && y1 == p->end_y))
	{
	  DPRINT ("deleting m_point line %d %d  %d %d  %d %d\n", p->points->x,
		  p->points->y, p_point->x, p_point->y, p->end_x, p->end_y);

	  polyline_delete (image, p);
	  polyline_join_in_point (image, x1, y1);
	  polyline_join_in_point (image, x2, y2);
	  return;
	}
    }
}

void
polyline_statist (struct image *image)
{
  struct polyline *p;
  struct polyline_point *p_point;

  for (p = image->first_polyline; p != NULL; p = p->next)
    {
      if (p->points == NULL)
	continue;
      p_point = p->points;	//start
      if (p_point->next == NULL)
	continue;
      p_point = p_point->next;	//2nd
      if (p_point->next == NULL)
	continue;
      if (p_point->next->next != NULL)
	continue;
      if (p->end_x != p_point->next->x)
	continue;
      if (p->end_y != p_point->next->y)
	continue;
      if (p->points->x + p->end_x != 2 * p_point->x)
	continue;
      if (p->points->y + p->end_y != 2 * p_point->y)
	continue;


      printf ("m_point line %d %d  %d %d  %d %d\n", p->points->x,
	      p->points->y, p_point->x, p_point->y, p->end_x, p->end_y);

    }
}
