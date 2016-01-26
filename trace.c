/*
    trace.c

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

    trace function does search on prepared bitmap and search for all pixels with
    more that two neighbour. These points is handled as polylines starts/ends
    All pixels with exact two neighbours is handled as polyline points. Trace algo 
    creates linked list of polylines. Polyline is defined by linked list of polyline
    points. This module pubises only one public symbol trace().

    Limitation: image edges must be prepared, because code does not recognize 
    x/y limits.

*/

#include <stdio.h>
#include <stdlib.h>
#include "pcb2g.h"
#include "vectorize.h"
#include "polyline.h"

//#define PATH_DEBUG
#ifdef PATH_DEBUG
#define  DPRINT(msg...) printf(msg)
#else
#define  DPRINT(msg...)
#endif


#define C_M_POINT 30
#define C_TRACED_POINT 130

struct m_point
{
  int x, y;
  unsigned char initmask, mask;
  struct m_point *next;

};


static int artefact (struct image *image);

/* tracing fcions */

// TODO this is duplicity with code in expand.c
static unsigned char
get_mask (struct image *image, int i, int border)
{
  unsigned char mask = 0;

  if (*(image->data + i) <= border)
    mask |= 1 << 3;
  if (*(image->data + i + 1) <= border)
    mask |= 1 << 2;
  if (*(image->data + i + 2) <= border)
    mask |= 1 << 1;
  if (*(image->data + i + 2 + image->x) <= border)
    mask |= 1 << 0;
  if (*(image->data + i + image->x) <= border)
    mask |= 1 << 4;
  if (*(image->data + i + image->x + image->x) <= border)
    mask |= 1 << 5;
  if (*(image->data + i + 1 + image->x + image->x) <= border)
    mask |= 1 << 6;
  if (*(image->data + i + 2 + image->x + image->x) <= border)
    mask |= 1 << 7;
  return mask;
}

// TODO end this is duplicity with code in expand.c


static int
bit_2_data (unsigned char mask, int *x, int *y)
{
  int i;

  for (i = 0; i < 8; i++)
    if (mask & (1 << i))
      {
	switch (mask & (1 << i))
	  {
//prefer left/right/up/down  trace 
	  case 1:
	    *x = 1;
	    *y = 0;
	    return 1 << i;
	  case 4:
	    *x = 0;
	    *y = -1;
	    return 1 << i;
	  case 16:
	    *x = -1;
	    *y = 0;
	    return 1 << i;
	  case 64:
	    *x = 0;
	    *y = 1;
	    return 1 << i;

	  case 2:
	    *x = 1;
	    *y = -1;
	    return 1 << i;
	  case 8:
	    *x = -1;
	    *y = -1;
	    return 1 << i;
	  case 32:
	    *x = -1;
	    *y = 1;
	    return 1 << i;
	  case 128:
	    *x = 1;
	    *y = 1;
	    return 1 << i;
	  }
      }
  return 0;
}

/* return number of direction from this pixel */
static int
mask_test (unsigned char mask)
{
  int count = 0, i;

  count = mask & 1;
  for (i = 0; i < 8; i++)
    {
      mask /= 2;
      count += (mask & 1);
    }
  return count;
}

struct m_point *first_m_point;


static int
next_pixel (struct image *image, int x, int y, int dx, int dy,
	    unsigned char dir)
{
  unsigned char mask, dirmask;
  int nx = x + dx, ny = y + dy;


  DPRINT ("tracing to %d %d\n", nx, ny);
//return if m_point/traced point is found
  if (*(image->data + nx + ny * image->x) == C_M_POINT)
    return -1;
  if (*(image->data + nx + ny * image->x) == C_TRACED_POINT)
    return -2;

//mark this pixel as traced
  *(image->data + nx + ny * image->x) = C_TRACED_POINT;

// save line from previous point to curent point
  polyline_extend (image, nx * 2, ny * 2);
//calculate posible direction from this pixel
  switch (dir)
    {
    case 1:
      dirmask = 0xc7;
      break;
    case 2:
      dirmask = 0x8f;
      break;
    case 4:
      dirmask = 0x1f;
      break;
    case 8:
      dirmask = 0x3e;
      break;
    case 0x10:
      dirmask = 0x7c;
      break;
    case 0x20:
      dirmask = 0xf8;
      break;
    case 0x40:
      dirmask = 0xf1;
      break;
    case 0x80:
      dirmask = 0xe3;
      break;
    default:
      printf ("wrong dir %02x\n", dir);
      exit (1);
    }
//get mask 
  mask = get_mask (image, nx - 1 + (ny - 1) * image->x, 120);

  return mask & dirmask;
}


static int
trace_m_point (struct image *image, struct m_point *m_point)
{
  unsigned char dir;
  int ret;
  int dx, dy;
  int lx, ly;

  if (m_point->mask == 0)
    return 0;

  DPRINT ("start trace at point %d %d mask %02x (%02x)\n", m_point->x,
	  m_point->y, m_point->mask, m_point->initmask);
  //start polyline from this m_point
  polyline_start (image, m_point->x * 2, m_point->y * 2);

  //calculate posible direction from this point
  dir = bit_2_data (m_point->mask, &dx, &dy);

  // mark this direction as used
  m_point->mask &= ~dir;

  // set start coordinates
  lx = m_point->x;
  ly = m_point->y;
  //test neighbour points.. 
  ret = next_pixel (image, lx, ly, dx, dy, dir);
  if (ret == -1)
    {
      //struct polyline_point *p_point;
      DPRINT ("m_point %d %d neighbor m_point %d %d\n", lx, ly, lx + dx,
	      ly + dy);

      //create line to middle point of these m_points, but if this point already exists
      //then append point to this polyline to get line from m_point to old m_point
      if (polyline_find_append
	  (image, lx * 2 + dx, ly * 2 + dy, lx * 2, ly * 2))
	polyline_delete_default (image);
      else
	polyline_append (image, lx * 2 + dx, ly * 2 + dy);
      return 1;
    }
  if (ret == -2)
    {
      DPRINT ("already traced direction\n");
      polyline_delete_default (image);
      return 1;
    }
  // continue trace ..  
  for (;;)
    {
      dir = ret;
      //move to next point
      lx += dx;
      ly += dy;

      //calculate posible direction from this point (because this is not m_point, only one dir is vailable)
      dir = bit_2_data (dir, &dx, &dy);

      ret = next_pixel (image, lx, ly, dx, dy, dir);
      if (ret == -1)
	{
	  DPRINT ("trace ok, end at %d %d prevdir=%02x\n", lx + dx, ly + dx,
		  dir);
	  polyline_extend (image, (lx + dx) * 2, (ly + dy) * 2);
	  return 1;
	}
    }
}

struct m_point *
m_point_new (int x, int y, unsigned char mask)
{
  struct m_point *new_m_point;
  new_m_point = calloc (sizeof (struct m_point), 1);
  new_m_point->next = first_m_point;
  new_m_point->x = x;
  new_m_point->y = y;
  new_m_point->mask = mask;
  new_m_point->initmask = mask;
  first_m_point = new_m_point;
  return new_m_point;
}

int
trace (struct image *image)
{

  int x, y, flag = 1;
  unsigned char mask;
  struct m_point *new_m_point;

  for (y = 2; y < image->y - 2; y++)
    for (x = 2; x < image->x - 2; x++)
      if (*(image->data + x + y * image->x) == 0)
	{
	  mask = get_mask (image, x - 1 + (y - 1) * image->x, 128);

	  if (mask_test (mask) > 2)
	    {

	      DPRINT ("saving m_point %d %d for mask 0x%02x - %d dirs\n",
		      x, y, mask, mask_test (mask));
	      m_point_new (x, y, mask);
	      *(image->data + x + y * image->x) = C_M_POINT;	//mark m_point
	    }
	}

  for (new_m_point = first_m_point; new_m_point;
       new_m_point = new_m_point->next)
    while (trace_m_point (image, new_m_point));

#ifdef PATH_DEBUG
  polylines_dump (image);
#endif
  artefact (image);
#ifdef PATH_DEBUG
  polyline_statist (image);
#endif

  while (flag)
    {
      flag = 0;
      for (y = 2; y < image->y - 2; y++)
	for (x = 2; x < image->x - 2; x++)
	  if (*(image->data + x + y * image->x) == 0)
	    {
	      mask = get_mask (image, x - 1 + (y - 1) * image->x, 128);
	      if (mask_test (mask) == 2)
		{
		  DPRINT ("saving m_point %d %d for mask 0x%02x - 2 dirs\n",
			  x, y, mask);
		  *(image->data + x + y * image->x) = C_M_POINT;	//mark m_point
		  trace_m_point (image, m_point_new (x, y, mask));
		  flag = 1;
		}
	    }
    }

  // safety recheck, print warnings .. 
  for (y = 2; y < image->y - 2; y++)
    for (x = 2; x < image->x - 2; x++)
      if (*(image->data + x + y * image->x) == 0)
	{
	  mask = get_mask (image, x - 1 + (y - 1) * image->x, 128);
	  if (mask_test (mask))
	    printf ("Warning, pixel at end of line at %d %d\n", x, y);
	  else
	    printf ("Warning, isolated pixel at %d %d\n", x, y);
	}

  while (first_m_point)
    {
      new_m_point = first_m_point->next;
      free (first_m_point);
      first_m_point = new_m_point;
    }

  printf ("Trace OK\n");
  return 0;
}

/*
       This is OK    
     * . X     * . X 
     |         |     
     * . .     * . . 
     |\    ->  |     
     *-*-*     *-*-* 
                     
    X = * or .       
*/


static int
artefact (struct image *image)
{

  int x, y;
  unsigned char mask;
//  struct m_point *new_m_point;

  for (y = 2; y < image->y - 2; y++)
    for (x = 2; x < image->x - 2; x++)
      if (*(image->data + x + y * image->x) != 0)
	{
	  mask =
	    get_mask (image, x - 1 + (y - 1) * image->x, C_TRACED_POINT + 1);
	  if (mask == 0xf8 || mask == 0xFA)
	    {
	      DPRINT ("F8 at %d %d  %d %d\n", 2 * (x - 1), 2 * y, 2 * x,
		      2 * (y + 1));
	      polyline_delete_m_line (image, 2 * (x - 1), 2 * y, 2 * x,
				      2 * (y + 1));
	    }
	  if (mask == 0x3e || mask == 0xbe)
	    {
	      DPRINT ("3E at %d %d  %d %d\n", 2 * (x - 1), 2 * y, 2 * x,
		      2 * (y - 1));
	      polyline_delete_m_line (image, 2 * (x - 1), 2 * y, 2 * x,
				      2 * (y - 1));
	    }
	  if (mask == 0x8f || mask == 0xAF)
	    {
	      DPRINT ("8F at %d %d  %d %d\n", 2 * (x + 1), 2 * y, 2 * x,
		      2 * (y - 1));
	      polyline_delete_m_line (image, 2 * (x + 1), 2 * y, 2 * x,
				      2 * (y - 1));
	    }
	  if (mask == 0xe3 || mask == 0xeb)
	    {
	      DPRINT ("E3 at %d %d  %d %d\n", 2 * (x + 1), 2 * y, 2 * x,
		      2 * (y + 1));
	      polyline_delete_m_line (image, 2 * (x + 1), 2 * y, 2 * x,
				      2 * (y + 1));

	    }
	}
  return 0;
}
