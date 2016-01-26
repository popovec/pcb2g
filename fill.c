/*
    fill.c

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

    image fill functions

*/
#include <stdlib.h>
#include "pcb2g.h"


static struct fill_data v;

struct stack_point
{
  int x, y;
//  int level;
  struct stack_point *next;
};

static struct stack_point *fill_stack;
static int fill_stack_level;
static int fill_stack_max_level;

static void
fill_push (int x, int y)
{
  struct stack_point *s;

  s = malloc (sizeof (struct stack_point));
  s->x = x;
  s->y = y;
  s->next = fill_stack;
  fill_stack = s;
  fill_stack_level++;
  if (fill_stack_level > fill_stack_max_level)
    fill_stack_max_level = fill_stack_level;
}

static struct stack_point *
fill_pop ()
{
  struct stack_point *s;

  s = fill_stack;
  if (s != NULL)
    {
      fill_stack = s->next;
      fill_stack_level--;
    }
  return s;
}


struct fill_data *
image_fill (struct image *image, int vx, int vy, unsigned char fill_data,
	    unsigned char border)
{

  struct stack_point *s;
  int sx, xmin, xmax;
  int f;

  if (*(image->data + vx + vy * image->x) == border)	//vyplneny
    return NULL;

  fill_stack_level = fill_stack_max_level = 0;
  fill_push (vx, vy);
  v.xmax = v.xmin = vx;
  v.ymax = v.ymin = vy;
  v.count = 1;


  for (;;)
    {
      s = fill_pop ();
      if (!s)
	{
	  v.level = fill_stack_max_level;
	  return &v;
	}
      *(image->data + s->x + s->y * image->x) = fill_data;	//vypln
      xmin = xmax = s->x;

      if (v.ymin > s->y)
	v.ymin = s->y;
      if (v.ymax < s->y)
	v.ymax = s->y;


//      printf ("scanline %d x at %d\n", s->y, s->x);
      if (s->x > 0)
	for (sx = s->x - 1; sx >= 0; sx--)
	  {
	    if (*(image->data + sx + s->y * image->x) == border)	//treba vyplnit?
	      break;
	    *(image->data + sx + s->y * image->x) = fill_data;
	    v.count++;
	    xmin = sx;
	  }

//      printf ("minX=%d\n", xmin);
      if (s->x < image->x - 1)
	for (sx = s->x + 1; sx < image->x; sx++)
	  {
	    if (*(image->data + sx + s->y * image->x) == border)	//treba vyplnit?
	      break;
	    *(image->data + sx + s->y * image->x) = fill_data;
	    v.count++;
	    xmax = sx;
	  }
//      printf ("maxX=%d\n", xmax);

      if (v.xmin > xmin)
	v.xmin = xmin;
      if (v.xmax < xmax)
	v.xmax = xmax;

      if (s->y > 0)
	{
	  f = 1;
	  for (sx = xmin; sx <= xmax; sx++)
	    {
	      if (*(image->data + sx + (s->y - 1) * image->x) != border
		  && *(image->data + sx + (s->y - 1) * image->x) != fill_data)
		{
		  if (f)
		    {
		      fill_push (sx, s->y - 1);
		      f = 0;
		    }
		}
	      else
		f = 1;
	    }
	}
      if (s->y < image->y - 1)
	{
	  f = 1;
	  for (sx = xmin; sx <= xmax; sx++)
	    {
	      if (*(image->data + sx + (s->y + 1) * image->x) != border
		  && *(image->data + sx + (s->y + 1) * image->x) != fill_data)
		{
		  if (f)
		    {
		      fill_push (sx, s->y + 1);
		      f = 0;
		    }
		}
	      else
		f = 1;
	    }
	}
      free (s);
    }
}
