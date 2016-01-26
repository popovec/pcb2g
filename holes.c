/*
    holes.c

    This is part of pcb2g - pcb bitmap to G code converter

    Copyright (C) 2011- 2014 Peter Popovec, popovec@fei.tuke.sk

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

    pcb holes functions

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb2g.h"
#include "tsp.h"
#include "post.h"

#define X_POS 0
#define Y_POS 1

//#define HOLES_DEBUG 1

#ifdef HOLES_DEBUG
#define  DPRINT(msg...) printf(msg)
#else
#define  DPRINT(msg...)
#endif

//sip - single inline package .. 
struct sip_group
{
  int dir;			//0/1 row/cullom
  int count;			//numbers of holes
  int start;			//first hole position
  int end;			//last hole position

  int position;			//row/cullom position (multiplied by count!!)
  int dia;			//hole count multiplied by number of holes (like position..)

  struct sip_group *next;

//not only dip exists, for 3 or more SIP lines define master SIP and slave SIP
//master is first, and have in dip_s link to slave, slave have link in dim_m to master.
//slave can be master for next line. Perpendicular gorups for this dip must be linked 
//in dip_per to master. TODO, dip can be 4x5 .. but organize this to 5x4 (1st count must be bigger)
  struct sip_group *dip_m;	//if paralel sip exist link them
  struct sip_group *dip_s;
  struct sip_group *dip_per;	//link 2/more holes group for this dip to master 


//statistical parameters  
  double position_divergence;
  double dia_divergence;

  int removed;			//group is not removed from list, only marked to remove
  //optional, garbage collector can remove all marked group 
  //but  remove must be done in lile gorup list too 
};

struct sip_group *first_sip_group;


//linked list of hole groups
struct h_group
{
  struct sip_group *sip;
  struct h_group *next;
};


void
create_hole (struct image *image, struct fill_data *vd)
{

  struct hole *h;

  h = calloc (sizeof (struct hole), 1);
  h->xmin = vd->xmin;
  h->xmax = vd->xmax;
  h->ymin = vd->ymin;
  h->ymax = vd->ymax;
  h->center[X_POS] = vd->xmin + vd->xmax;
  h->center[Y_POS] = vd->ymin + vd->ymax;
  h->count = vd->count;
  h->level = vd->level;
  h->next = image->first_hole;
  image->first_hole = h;
  h->fx = i2realX (image, h->center[0] / 2.0);
  h->fy = i2realY (image, h->center[1] / 2.0);
}

/*  

kicad drill file import

*/
void
get_drill_file (struct image *image)
{
  FILE *f;
  char *line = NULL;
  size_t n;
  char *ptr;
  int i;
  int header;
  double tool_dia[100];
  double x = 0, y = 0;
  struct hole *h;


  for (i = 0; i < 100; i++)
    tool_dia[i] = 0;
  if (image->drill_file == NULL)
    return;

  f = fopen (image->drill_file, "r");
  if (f == NULL)
    {
      printf ("unable to open drill file %s\n", image->drill_file);
      return;
    }
  else
    printf ("reading drill file..\n");
//simple parser
  header = 1;
  while (-1 != getline (&line, &n, f) && header)
    {
      switch (line[0])
	{
	case 'T':
	  i = strtol (&line[1], &ptr, 10);
	  if (ptr != &line[1] && i >= 0 && i < 100 && *ptr == 'C')
	    {
	      printf ("tool %d", i);
	      ptr++;
	      tool_dia[i] = atof (ptr);
	      printf (" dia %f\n", tool_dia[i]);
	    }
	  break;
	case '%':
	  header = 0;
	  break;
	}
    }

  while (-1 != getline (&line, &n, f))
    {
      switch (line[0])
	{
	case 'T':
	  i = strtol (&line[1], &ptr, 10);
	  if (ptr != &line[1] && i >= 0 && i < 100 && *ptr == '\n')
	    printf ("selected tool %d\n", i);
	  if (i > 100)
	    i = 1;
	  break;
	case 'X':
	  x = strtod (&line[1], &ptr);
	  if (*ptr == 'Y')
	    y = strtod (++ptr, &ptr);
	  h = calloc (sizeof (struct hole), 1);
	  h->fx = x;
	  h->fy = y;
/* TODO, use real_x and real_y, for now not needed if drill file is used*/
	  h->xmin = h->xmax = h->center[0] =
	    (x + 0.005) * image->dpi * 5 / 127;
	  h->ymin = h->ymax = h->center[1] =
	    (y + 0.005) * image->dpi * 5 / 127;
	  h->center[0] *= 2;
	  h->center[1] *= 2;
	  h->count = h->level = 1;
	  h->dia = tool_dia[i];
	  h->drill_file = 1;
	  h->next = image->first_hole;
	  image->first_hole = h;
	  printf ("hole %f %f dia %f\n", x, y, tool_dia[i]);
	  break;
	case 'Y':
	  y = strtod (&line[1], &ptr);
	  if (*ptr == 'X')
	    x = strtod (++ptr, &ptr);
	  h = calloc (sizeof (struct hole), 1);
	  h->fx = x;
	  h->fy = y;
/* TODO, use real_x and real_y, for now not needed if drill file is used*/
	  h->xmin = h->xmax = h->center[0] =
	    (x + 0.005) * image->dpi * 5 / 127;
	  h->ymin = h->ymax = h->center[1] =
	    (y + 0.005) * image->dpi * 5 / 127;
	  h->center[0] *= 2;
	  h->center[1] *= 2;
	  h->count = h->level = 1;
	  h->dia = tool_dia[i];
	  h->drill_file = 1;
	  h->next = image->first_hole;
	  image->first_hole = h;
	  printf ("hole %f %f dia %f\n", x, y, tool_dia[i]);
	  break;
	}
    }
  free (line);
  fclose (f);
}


static void
free_holes (struct image *image)
{
  struct hole *h;

  h = image->first_hole;

  while (h)
    {
      image->first_hole = h->next;
      free (h);
      h = image->first_hole;
    }
}

void
dump_holes (struct image *image)
{
  struct hole *h;
  double x, y, x0, y0, len;
  void *hh, *hh0;
  tspdata *tsp;


  x0 = image->route_last_x;
  y0 = image->route_last_y;

  if (!image->first_hole)
    return;
  printf ("starting hole dump\n");


  hh0 = image->first_hole;
  len =
    sqrt ((image->first_hole->fx - x0) * (image->first_hole->fx - x0) +
	  (image->first_hole->fy - y0) * (image->first_hole->fy - y0));

  tsp = tsp_init ();
  for (h = image->first_hole; h != NULL; h = h->next)
    {
      if (0 != tsp_add (tsp, h->fx, h->fy, h))
	{
	  postprocesor_write_comment
	    (" ===  No holes, because error in TSP procedure === ");
	  return;
	}
      if (sqrt ((h->fx - x0) * (h->fx - x0) + (h->fy - y0) * (h->fy - y0)) <
	  len)
	{
	  len =
	    sqrt ((h->fx - x0) * (h->fx - x0) + (h->fy - y0) * (h->fy - y0));
	  hh0 = h;
	}
    }
  if (0 == tsp_solve (tsp))
    {
      postprocesor_write_comment (" === Using Lin-Kernighan heuristic === ");
    }

  //search for hole up to x0,y0

  for (;;)
    {
      if (-1 == tsp_getnext (tsp, &x, &y, &hh))
	{
	  postprocesor_write_comment
	    (" ===  No holes, because error get TSP data === ");
	  return;
	}
      if (hh == hh0)
	break;
    }
  h = hh;
  postprocesor_write_comment (" ===  HOLES start === ");
  postprocesor_operation (MACHINE_DRILL);
  len = 0;
  do
    {
      h = hh;
      DPRINT ("TSP %e %e %f %f\n", x, y, h->fx, h->fy);
      postprocesor_hole (h->fx, h->fy, h->dia);
      len += sqrt ((h->fx - image->drill_last_x) * (h->fx -
						    image->drill_last_x) +
		   (h->fy - image->drill_last_y) * (h->fy -
						    image->drill_last_y));
      //save last position of tool
      image->drill_last_x = h->fx;
      image->drill_last_y = h->fy;
      tsp_getnext (tsp, &x, &y, &hh);
    }
  while (hh != hh0);
  printf ("rapid distance for drilling (in XY plane) %f)\n", len);
  tsp_close (tsp);
  postprocesor_operation (MACHINE_IDLE);
  postprocesor_write_comment (" === HOLES end === ");
  free_holes (image);
}

void
mark_holes (struct image *image)
{
  struct hole *h;

  int hx, hy, isize;

//mark holes only if image size is available
  if (image->real_x <= 0 || image->real_y <= 0)
    return;


  isize = image->x * image->y;
  for (h = image->first_hole; h != NULL; h = h->next)
    {
      if (h->drill_file)
	{
	  hx = image->x * h->fx / image->real_x;
	  hy = image->y * h->fy / image->real_y;
	  hy = hy * image->x + hx;
	  if (hy < isize && hy > 0)	//prevent segfault for wrong hole positions
	    *(image->data + hy) = 20;
	}
    }
}
