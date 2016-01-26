/*
    cut.c

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

    board outline cutting 

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "pcb2g.h"
#include "post.h"

struct polygon
{
  double seg[6];
  int arc;
  struct polygon *next;
};

struct polygons
{
  double start[2];
  double end[2];
  struct polygon *p;
  struct polygons *next;
  double area;
};

struct polygons *polygons_first;

#ifdef CUT_TEST
void
dump ()
{
  struct polygons *pgs;
  struct polygon *p;


  for (pgs = polygons_first; pgs; pgs = pgs->next)
    {
      for (p = pgs->p; p; p = p->next)
	printf ("[%.3f %.3f]%c", p->seg[0], p->seg[1],
		p->arc == 0 ? ' ' : 'A');
      printf ("[%.3f %.3f]\n", pgs->end[0], pgs->end[1]);
    }
}
#endif

static void
flip_segment (struct polygon *p)
{
  double h;

  h = p->seg[0];
  p->seg[0] = p->seg[2];
  p->seg[2] = h;

  h = p->seg[1];
  p->seg[1] = p->seg[3];
  p->seg[3] = h;

  //arc parameters must be recalculated only if center is relative to start, but in this
  //code absolute  position of center are used 

  //only G2/G3 change ..
  p->arc *= -1;

}

static struct polygons *
find_polygon (struct polygon *p)
{
  struct polygons *pgs;

  for (pgs = polygons_first; pgs; pgs = pgs->next)
    {
      if (p->seg[0] == pgs->start[0] && p->seg[1] == pgs->start[1])
	flip_segment (p);
      if (p->seg[2] == pgs->start[0] && p->seg[3] == pgs->start[1])
	{
	  p->next = pgs->p;
	  pgs->p = p;
	  pgs->start[0] = p->seg[0];
	  pgs->start[1] = p->seg[1];
	  return pgs;
	}

      if (p->seg[2] == pgs->end[0] && p->seg[3] == pgs->end[1])
	flip_segment (p);
      if (p->seg[0] == pgs->end[0] && p->seg[1] == pgs->end[1])
	{
	  struct polygon *pn;

	  for (pn = pgs->p; pn->next != NULL; pn = pn->next);
	  pn->next = p;
	  pgs->end[0] = p->seg[2];
	  pgs->end[1] = p->seg[3];
	  return pgs;
	}
    }
  return NULL;
}

static void
append (struct polygons *p1, struct polygons *p2)
{

  struct polygon *pn;
  struct polygons **pgs;

  for (pn = p1->p; pn->next != NULL; pn = pn->next);
  pn->next = p2->p;
  p1->end[0] = p2->end[0];
  p1->end[1] = p2->end[1];

  //dealocate
  for (pgs = &polygons_first; *pgs != p2; pgs = &((*pgs)->next));
  *pgs = p2->next;
  free (p2);
}

static void
flip_polygon (struct polygons *pgs)
{
  struct polygon *p, *pn;
  double h;


  for (pn = pgs->p, pgs->p = NULL; pn != NULL;)
    {
      p = pn->next;
      pn->next = pgs->p;
      pgs->p = pn;
      flip_segment (pn);
      pn = p;
    }

  h = pgs->start[0];
  pgs->start[0] = pgs->end[0];
  pgs->end[0] = h;
  h = pgs->start[1];
  pgs->start[1] = pgs->end[1];
  pgs->end[1] = h;
}

#define C_TOL 0.0005

static int
connect_polygons (struct polygons *pgs)
{
  struct polygons *p;

  for (p = polygons_first; p; p = p->next)
    {
      if (p == pgs)
	continue;

      if (fabs (p->start[0] - pgs->end[0]) < C_TOL
	  && fabs (p->start[1] - pgs->end[1]) < C_TOL)
	{
	  append (pgs, p);
	  return 1;
	}
      if (fabs (p->end[0] - pgs->start[0]) < C_TOL
	  && fabs (p->end[1] - pgs->start[1]) < C_TOL)
	{
	  flip_polygon (pgs);
	  flip_polygon (p);
	  append (pgs, p);
	  return 1;
	}
      if (fabs (p->end[0] - pgs->end[0]) < C_TOL
	  && fabs (p->end[1] - pgs->end[1]) < C_TOL)
	{
	  flip_polygon (p);
	  append (pgs, p);
	  return 1;
	}
      if (fabs (p->start[0] - pgs->start[0]) < C_TOL
	  && fabs (p->start[1] - pgs->start[1]) < C_TOL)
	{
	  flip_polygon (pgs);
	  append (pgs, p);
	  return 1;
	}
    }
  return 0;
}

static void
create_polygons (struct polygon *p)
{
  struct polygons *pgs;

  pgs = calloc (sizeof (struct polygons), 1);
  pgs->next = polygons_first;
  polygons_first = pgs;
  pgs->p = p;
  pgs->start[0] = p->seg[0];
  pgs->start[1] = p->seg[1];
  pgs->end[0] = p->seg[2];
  pgs->end[1] = p->seg[3];
}

static char *
skip_arg (char *p)
{
  while (p)
    {
      if (*p == '#')
	break;
      if (!isspace (*p))
	p++;
      else
	break;
    }
  return p;
}

static char *
skip_space (char *p)
{
  while (p)
    {
      if (isspace (*p))
	p++;
      else
	break;
    }
  return p;
}

static void
read_cut (char *filename)
{
  FILE *f;
  char *lineptr = NULL;
  size_t n;
  struct polygons *pgs;
  f = fopen (filename, "r");
  if (!f)
    return;
  while (getline (&lineptr, &n, f) > 0)
    {
      char *p = lineptr;
      int count = 0;
      double seg[6];

      while (p && count < 6)
	{
	  p = skip_space (p);
	  if (*p == '#')
	    break;
	  if (1 != sscanf (p, "%lf", &(seg[count])))
	    break;
	  p = skip_arg (p);
	  count++;
	}

      //TODO error handling .. 
      if (count == 4 || count == 6)
	{
	  struct polygon *p;

	  p = calloc (sizeof (struct polygon), 1);
	  memcpy (p->seg, seg, sizeof (double) * 6);
	  p->arc = count == 4 ? 0 : 1;

	  pgs = find_polygon (p);
	  if (!pgs)
	    create_polygons (p);
	  else
	    while (connect_polygons (pgs));
	}
#ifdef CUT_TEST
      dump ();
      printf ("-----------------------------\n");
#endif
    }
  if (lineptr)
    free (lineptr);
  fclose (f);
}


#ifndef CUT_TEST
#define C_LEFT -1
#define C_RIGHT 1
#define C_NONE 0


#define DIR_CCW 0
#define DIR_CW  1

static void
r_segment (struct polygon *p)
{
  if (p->arc)
    postprocesor_route_arc (p->seg[2], p->seg[3], p->seg[4], p->seg[5],
			    p->arc);
  else
    postprocesor_route (p->seg[2], p->seg[3]);
}

static void
post_cut (struct polygons *polygons, int comp)
{
  struct polygon *p;

  if (!polygons->p->next)	//circle
    {
      if (!polygons->p->arc)
	{
	  printf ("Internal error not arc ? \n");
	  exit (1);
	}
      //TODO better lead in/out ..
      postprocesor_rapid (polygons->p->seg[4], polygons->p->seg[5]);
      postprocesor_set (POST_SET_CUTTER_COMP, (int) comp);
      postprocesor_rapid (polygons->p->seg[0], polygons->p->seg[1]);

      postprocesor_route_arc (polygons->p->seg[2], polygons->p->seg[3],
			      polygons->p->seg[4], polygons->p->seg[5],
			      comp > 0 ? -1 : 1);
      postprocesor_rapid (polygons->p->seg[0], polygons->p->seg[1]);
      postprocesor_set (POST_SET_CUTTER_COMP, (int) 0);

      return;
    }
  //move to polygon start
  postprocesor_rapid (polygons->p->seg[0], polygons->p->seg[1]);

  postprocesor_set (POST_SET_CUTTER_COMP, (int) comp);

  //entry move to get cutter compensation work (lead in move)
  postprocesor_rapid (polygons->p->seg[2], polygons->p->seg[3]);

  for (p = polygons->p->next; p; p = p->next)
    r_segment (p);
  //first segment is not routed  (was used to get cutter compensation work)
  r_segment (polygons->p);
  //already routed segment use as lead out
  postprocesor_rapid (polygons->p->next->seg[2], polygons->p->next->seg[3]);
  //turn off compensation
  postprocesor_set (POST_SET_CUTTER_COMP, (int) 0);

}

static void
polygon_area (struct polygons *polygons)
{
  struct polygon *p;

  if (!polygons->p->next)	//circle
    {
      double radius;

      p = polygons->p;
      radius =
	sqrt ((p->seg[0] - p->seg[4]) * (p->seg[0] - p->seg[4]) -
	      (p->seg[1] - p->seg[5]) * (p->seg[1] - p->seg[5]));

      polygons->area = M_PI * radius * radius;
      return;
    }

// Shoelace formula for polygon area ..
// TODO aproximate arc with hexagon..
  for (p = polygons->p; p; p = p->next)
    polygons->area += (p->seg[2] - p->seg[0]) * (p->seg[3] + p->seg[1]);
}

static void
rotate_cut (struct polygons *polygons, int dir)
{
//determine direction by Shoelace formula (signum of area )

//rotate cut

  if (polygons->area > 0 && dir == DIR_CCW)
    flip_polygon (polygons);
  if (polygons->area < 0 && dir == DIR_CW)
    flip_polygon (polygons);
}

#endif
void
create_cut (char *f)
{
  struct polygons *polygons, *p_max;
  double max = 0;


  read_cut (f);

  // calculate area of all polygons
  for (polygons = polygons_first; polygons; polygons = polygons->next)
    polygon_area (polygons);
  //polygon with biggest area is  board outline, all others are holes
  for (p_max = polygons = polygons_first; polygons; polygons = polygons->next)
    if (fabs (polygons->area) > max)
      {
	max = fabs (polygons->area);
	p_max = polygons;
      }

  for (polygons = polygons_first; polygons; polygons = polygons->next)
    if ((polygons->start[0] != polygons->end[0])
	|| (polygons->start[1] != polygons->end[1]))
      {
	printf ("cut is not closed path, please check cut file\n");
	exit (1);
      }
#ifndef CUT_TEST

  postprocesor_write_comment (" === BORDER cut start [cut file] === ");
  postprocesor_operation (MACHINE_CUT);

//cut holes first
//TODO calculate minimal path to other holes ..
  for (polygons = polygons_first; polygons; polygons = polygons->next)
    {
      if (polygons == p_max)
	continue;
      rotate_cut (polygons, DIR_CW);
      post_cut (polygons, C_RIGHT);

    }
//TODO get beter start point
//TODO create bridges for breaking board out

//route board edges
  if (!p_max->p->next)		//circle
    {

      //always cut from lower point of circle

      double radius;
      struct polygon *p = p_max->p;
      radius =
	sqrt ((p->seg[0] - p->seg[4]) * (p->seg[0] - p->seg[4]) -
	      (p->seg[1] - p->seg[5]) * (p->seg[1] - p->seg[5]));
      postprocesor_set (POST_SET_CUTTER_COMP, (int) C_RIGHT);
      postprocesor_rapid (p->seg[4], p->seg[5] - radius);
      postprocesor_route_arc (p->seg[4], p->seg[5] - radius, p->seg[4],
			      p->seg[5], 1);
      postprocesor_rapid (p->seg[4], p->seg[5] - radius);
      postprocesor_set (POST_SET_CUTTER_COMP, (int) C_NONE);
    }
  else
    {
      rotate_cut (p_max, DIR_CCW);
      post_cut (p_max, C_RIGHT);
    }
  postprocesor_operation (MACHINE_IDLE);
  postprocesor_write_comment (" === BORDER cut end [cut file] === ");

#endif
}

#ifdef CUT_TEST
int
main ()
{

  create_cut ("tmp.cut");

  dump ();

  return 0;
}
#endif
