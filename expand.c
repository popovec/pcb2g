/*
    expand.c

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

    PCB copper expansion routines

*/
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "pcb2g.h"

//#define SPEED_UP1

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


static int
local_end (struct image *image, int fill_data, int border)
{
  int count = 0;
  int i;

  unsigned char mask;

  for (i = 0; i < image->size; i++)
    {
      if (*(image->data + i + 1 + image->x) <= border)
	continue;

      mask = get_mask (image, i, border);
      switch (mask)		// o ->x if match mask
	{
	case 0xa7:		// a7
	case 0xcb:		// .xx x.x xx. x.x x.. xxx xxx ..x
	case 0xbc:		// .ox .ox xo. xo. .ox .ox xo. xo.
	case 0x7a:		// x.x .xx x.x xx. xxx x.. ..x xxx
	case 0xe9:
	case 0x2f:
	case 0x9e:
	case 0xf2:

	case 0xbe:		//  be  fa  af  eb
	case 0xfa:		// xxx x.x xxx x.x
	case 0xaf:		// xo. xo. .ox .ox
	case 0xeb:		// x.x xxx x.x xxx

	case 0xe0:		//  e0  0e  83  38
	case 0x0e:		// ... xxx ..x x..
	case 0x83:		// .o. .o. .ox xo.
	case 0x38:		// xxx ... ..x x..

	case 0x06:		//  06  0c  30  18  c0  60  81  03
	case 0x0c:		// .xx xx. ... x.. ... ... ... ..x
	case 0x30:		// .o. .o. xo. xo. .o. .o. .ox .ox
	case 0x18:		// ... ... x.. ... .xx xx. ..x ...
	case 0xc0:
	case 0x60:
	case 0x81:
	case 0x03:


	  *(image->data + i + 1 + image->x) = fill_data;
	  count++;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  //printf ("end filter %d\n", count);
  return count;
}

static int
local_T1 (struct image *image, int fill_data, int border)
{
  int count = 0;
  int i;

  unsigned char mask;

  for (i = 0; i < image->size; i++)
    {
      if (*(image->data + i + 1 + image->x) <= border)
	continue;
      mask = get_mask (image, i, border);
      switch (mask)		// o ->x if match mask x = copper
	{

	case 0x2b:		//  2b  a9  a6  ac  9a  b2  6a  ca
	case 0xa9:		// x.x x.. .xx xx. x.x ..x x.x x.x 
	case 0xa6:		// .ox .ox .o. .o. xo. xo. .o. .o.
	case 0xac:		// x.. x.x x.x x.x ..x x.x xx. .xx
	case 0x9a:
	case 0xb2:
	case 0x6a:
	case 0xca:

	case 0xae:		//  ae  ea  ab  ba
	case 0xea:		// xxx x.x x.x x.x
	case 0xab:		// .o. .o. .ox xo.
	case 0xba:		// x.x xxx x.x x.x

	  *(image->data + i + 1 + image->x) = fill_data;
	  count++;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  //printf ("end filter T1 %d\n", count);
  return count;
}


void
recolor (struct image *image, int fill_color, int level)
{
  int i;
  for (i = 0; i < image->x * image->y; i++)
    if (*(image->data + i) == level)
      *(image->data + i) = fill_color;
  return;
}

static void bit_e (struct image *image);

int
expand_copper (struct image *image)
{

  int ret, i1, i2, flag;

  bit_e (image);

  for (i1 = 0, i2 = image->x; i1 < image->size; i1++, i2++)
    if (image->mask_data[i1] & 0x10)
      if (image->data[i2] != 0)
	image->data[i2] = 1;

  //printf ("all ok\n");

  for (flag = 1; flag;)
    {
      flag = 0;
      for (;;)
	{
	  ret = local_T1 (image, 70, 90);
	  if (!ret)
	    break;
	  flag = 1;
	  printf ("local_t1 found\n");
	}
      for (;;)
	{
	  ret = local_end (image, 70, 90);
	  if (!ret)
	    break;
	  flag = 1;
	  printf ("local_end found\n");
	}
    }
  recolor (image, 160, 70);

  recolor (image, 150, C_HOLE);
  recolor (image, 190, C_HOLE_ALIGNED);
  recolor (image, 200, 1);
  recolor (image, 255, 0);
  recolor (image, 0, 100);
/*
  for (sy = 0, sx = 0; sx < image->x * image->y; sx++)
    if (*(image->data + sx) < 64)
      sy++;
  printf ("track=%d\n", sy);
*/
  return 0;
}


#define SET_OR_MASK  count++;\
	  image->or_mask[i + 1] |= 0x10; \
	  image->or_mask[i - 1] |= 0x01; \
	  image->or_mask[i - 1 - image->x] |= 0x80; \
	  image->or_mask[i - image->x] |= 0x40; \
	  image->or_mask[i + 1 - image->x] |= 0x20; \
	  image->or_mask[i - 1 + image->x] |= 0x02; \
	  image->or_mask[i + image->x] |= 0x04; \
	  image->or_mask[i + 1 + image->x] |= 0x08;

//convert 4 to 8 continuous
/*   34  85  58  43  16  0D  D0 16
321  .x. .x. x.. ..x .xx xx.
4 0  x.. ..x x.. ..x x.. ..x
567  x.. ..x .x. .x. ... ...
*/


static int
l_4to8 (struct image *image)
{
  int count = 0;
  int i;

  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x34:
	case 0x85:
	case 0x58:
	case 0x43:

	case 0x16:
	case 0x0d:
	case 0x61:
	case 0xd0:

	  SET_OR_MASK;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  printf ("4to8 %d\n", count);
  return count;
}

//convert diagonal
/*   14  05  50  41
321  .x. .x. ... ...
4 0  x.. ..x x.. ..x
567  ... ... .x. .x.
*/


static int
l_diag (struct image *image)
{
  int count = 0;
  int i;

  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x14:
	case 0x05:
	case 0x50:
	case 0x41:


	  SET_OR_MASK;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  printf ("diag %d\n", count);
  return count;
}

//diag_fill
/*   97   D3   3D   79   5e  4f  f4  e5
321  .xx  ..x  xx.  x..  xxx xxx .x. .x.
4 0  x.x  x.x  x.x  x.x  x.. ..x x.. ..x
567  ..x  .xx  x..  xx.  .x. .x. xxx xxx
*/


static int
l_diag_fill (struct image *image)
{
  int count = 0;
  int i;

  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x97:
	case 0xd3:
	case 0x3d:
	case 0x79:
	case 0x5e:
	case 0xe5:
	case 0x4f:
	case 0xf4:


	  SET_OR_MASK;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  printf ("diag_fill %d\n", count);
  return count;
}




static int
l_noexpand (struct image *image)
{
  int i, count = 0;

  for (i = image->x + 1; i < image->size;)
    {
      if ((image->or_mask[i - 1] & 1))
	{
	  i++;
	  continue;
	}


      switch (image->or_mask[i])
	{

	  //x.x
	  //x.x
	  //xxx
	case 0xfb:
	  SET_OR_MASK;
	  if (i - image->x > 0)
	    i -= image->x;
	  else
	    i++;
	  break;

	  //xxx
	  //x.x
	  //x.x  
	case 0xbf:
	  SET_OR_MASK;
	  if (i + image->x < image->size)
	    i += image->x;
	  else
	    i++;
	  break;

	  //xxx
	  //..x
	  //xxx
	case 0xef:
	  SET_OR_MASK;
	  i--;
	  break;

/*   f1  f9  f3  1f  3f  9f  c7  cf  e7  fd  f7  df  7f 
321  ... x.. ..x xxx xxx xxx .xx XXX .XX xx. .xx xxx xxx
4 0  x.x x.x x.x x.x x.x x.x ..x ..X ..X x.x x.x x.x x.x
567  xxx xxx xxx ... x.. ..x .xx .XX XXX xxx xxx .xx xx.
*/
	case 0xf1:
	case 0xf9:
	case 0xf3:
	case 0x1f:
	case 0x3f:
	case 0x9f:
	case 0xc7:
	case 0xcf:
	case 0xe7:
	case 0xfd:
	case 0xf7:
	case 0xdf:
	case 0x7f:
	  SET_OR_MASK;
	  i += 2;
	  break;

/*   7c  fe  7e  fc 
321  xx. xxx XXX xx.
4 0  x.. x.. x.. x..
567  xx. xxx XX. xxx
*/
	case 0x7c:
	case 0xfe:
	case 0x7e:
	case 0xfc:
	  SET_OR_MASK;
	default:
	  i++;
	  break;
	}			//switch
    }				//for
//  printf ("noexpand run with %d points \n", count);
  return count;
}

/*   e0   e1  f0 
321  ...  ... ...
4 0  ...  ..x x..
567  xxx  xxx xxx

x = copper
*/

static int
l_up (struct image *image)
{
  int count = 0;
  int i = 0;
/*
  if ((image->mask_data[1] & 0x10))
    switch (image->mask_data[0])
      {
      case 0xe0:
      case 0xe1:
      case 0xf0:
	SET_OR_MASK;
      }
*/
  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;
      switch (image->mask_data[i])
	{
	case 0xe0:
	case 0xe1:
	case 0xf0:
	  SET_OR_MASK;
	}

#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  //printf ("up %d\n", count);
  return count;
}


/*   0e   1e  0f    
321  XXX  xxx xxx
4 0  ...  x.. ..x
567  ...  ... ...
*/

static int
l_down (struct image *image)
{
  int count = 0;
  int i;


  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x0e:
	case 0x1e:
	case 0x0f:
	  SET_OR_MASK;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  //printf ("down %d\n", count);
  return count;
}



/*   83   87  c3 
321  ..x .xx ..x
4 0  ..x ..x ..x
567  ..x ..x .xx
*/

static int
l_left (struct image *image)
{
  int count = 0;
  int i;


  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x83:
	case 0x87:
	case 0xc3:
	  SET_OR_MASK;
	  i++;
	}
    }
  //printf ("left %d\n", count);
  return count;
}


/*   38  3c  78   
321  x.. xx. x..
4 0  x.. x.. x..
567  x.. x.. xx.
*/

static int
l_right (struct image *image)
{
  int count = 0;
  int i;

  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x38:
	case 0x3c:
	case 0x78:
	  SET_OR_MASK;
	}
    }
  //printf ("right %d\n", count);
  return count;
}

/*   1c  70  07  c1 
321  xx. ... .xx ...
4 0  x.. x.. ..x ..x
567  ... xx. ... .xx
*/

/*    3e  8f  f8  e3
321  XXX xxx x.. ..x
4 0  X.. ..x x.. ..x
567  X.. ..x xxx xxx
*/


static int
l_rozok (struct image *image)
{
  int count = 0;
  int i;

  for (i = image->x + 1; i < image->size; i++)
    {
      if ((image->mask_data[i - 1] & 1))
	continue;

      switch (image->mask_data[i])
	{
	case 0x1c:
	case 0x70:
	case 0x07:
	case 0xc1:

	case 0x3e:
	case 0x8f:
	case 0xf8:
	case 0xe3:
	  SET_OR_MASK;
	}
#ifdef SPEED_UP1
      i += (mask & 1);
#endif
    }
  //printf ("rozok %d\n", count);
  return count;
}


static void
debug_dump_bit_image (struct image *image)
{

  FILE *f;
  int i;
  f = fopen ("bdebug.pgm", "w");
  if (!f)
    return;

  fprintf (f, "P5\n%d %d\n255\n", image->x, image->y);

  for (i = 1; i < image->x * image->y; i++)
    {
      if (image->or_mask[i] & 0x10)
	fputc (255, f);
      else
	fputc (0, f);

    }
  fputc (255, f);

  fclose (f);
}

static void
bit_e (struct image *image)
{

  int flag = 0;
  int i, j, k, ret, allret;
  struct timespec ts, ts_old, ts_diff;

  image->size = image->x * (image->y - 2) - 2;

  clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &ts_old);
  image->mask_data =
    calloc (image->x * (image->y + 2), sizeof (unsigned char));
  image->or_mask = calloc (image->x * (image->y + 2), sizeof (unsigned char));

  for (i = 0; i < image->size; i++)
    image->or_mask[i] = get_mask (image, i, 70);

  memcpy (image->mask_data, image->or_mask, image->size);

  l_4to8 (image);

  for (j = 0;; j++)
    {
      allret = 0;

      for (k = 0, ret = 1; ret; k++)
	{
	  ret = l_noexpand (image);
	  memcpy (image->mask_data, image->or_mask, image->size);
	  allret += ret;
	}

      if (!flag)
	{
	  ret += l_diag (image);
	  memcpy (image->mask_data, image->or_mask, image->size);
	  flag++;
	  ret += l_diag_fill (image);
	  memcpy (image->mask_data, image->or_mask, image->size);
	}

      ret = l_up (image);
      memcpy (image->mask_data, image->or_mask, image->size);

      ret += l_down (image);
      memcpy (image->mask_data, image->or_mask, image->size);

      ret += l_left (image);
      memcpy (image->mask_data, image->or_mask, image->size);

      ret += l_right (image);
      memcpy (image->mask_data, image->or_mask, image->size);

      ret += l_rozok (image);
      memcpy (image->mask_data, image->or_mask, image->size);

      allret += ret;
      if (!allret)
	break;

      clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &ts);
      ts_diff = tsx_diff (ts_old, ts);
      printf ("................... iteration %d, time %ld.%09ld", j,
	      ts_diff.tv_sec, ts_diff.tv_nsec);
      ts_diff = tsx_diff (image->ts, ts);
      printf (" sum: %ld.%09ld\n", ts_diff.tv_sec, ts_diff.tv_nsec);

      ts_old.tv_sec = ts.tv_sec;
      ts_old.tv_nsec = ts.tv_nsec;
    }

  printf ("all ok\n");
  if (image->debug_files)
    debug_dump_bit_image (image);
}
