/*
    image.c

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

    netpbm image input/output functions

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "pcb2g.h"

double
bres_line_test (struct image *image, int x0, int y0, int x1, int y1)
{

  int dx = abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;


  for (;;)
    {
      if (*(image->data_orig + x0 + image->x * y0) == 0)
	return -1;
      if (x0 == x1 && y0 == y1)
	break;
      e2 = err;
      if (e2 > -dx)
	{
	  err -= dy;
	  x0 += sx;
	}
      if (e2 < dy)
	{
	  err += dx;
	  y0 += sy;
	}
    }
  return 0;
}


unsigned char c_chars;
char comment[255];

static char
file_read_char (FILE * f)
{
  int ret;
  if (EOF == (ret = fgetc (f)))
    {
      fprintf (stderr, "Error, short image file \n");
      exit (2);
    }
  if ((ret & 255) == '#')
    {
      do
	{
	  if (EOF == (ret = fgetc (f)))
	    {
	      fprintf (stderr, "Error, short image file \n");
	      exit (2);
	    }
	  if (c_chars < 254)
	    comment[c_chars++] = ret & 255;
	}
      while ((ret & 255) != '\n' && (ret & 255) != '\r');
      comment[c_chars - 1] = ' ';
    }
  return (ret & 255);
}

static unsigned int
file_read_uint (FILE * f)
{
  unsigned char c;
  int i = 0;

  do
    {
      c = file_read_char (f);
    }
  while (c == ' ' || c == '\t' || c == '\n' || c == '\r');

  while (c >= '0' && c <= '9')
    {
      i *= 10;
      i += c - '0';
      if (i > 10000)
	{
	  fprintf (stderr, "Error, big resolution/color value in file\n");
	  exit (2);
	}
      c = file_read_char (f);
    }
  return i;
}

int
input_img_read (struct image *image, int flags)
{

  FILE *f;
  int i;
  int gscale = 0;
  int mode = 4;
  struct stat sb;


  c_chars = 0;

  if (stat (image->image_file, &sb) == -1)
    return (1);
  image->mtime = strdup (ctime (&sb.st_mtime));
  *(strchr (image->mtime, '\n')) = 0;

  f = fopen (image->image_file, "r");
  if (!f)
    return (1);

  i = fgetc (f);
  if (i == EOF)
    goto input_img_read_error;
  if ((i & 255) != 'P')
    goto input_img_read_error;
  i = fgetc (f);
  if (i == EOF)
    goto input_img_read_error;
  if ((i & 255) != '4' && (i & 255) != '5')
    goto input_img_read_error;

  if ((i & 255) == '5')
    mode = 5;

  image->x = file_read_uint (f);
  if (image->x < 64)
    {
      fprintf (stderr, "Error, image size is too small\n");
      return (2);
    }

  image->y = file_read_uint (f);
  if (image->y < 64)
    {
      fprintf (stderr, "Error, image size is too small\n");
      return (2);
    }
  if (mode == 5)
    {
      gscale = file_read_uint (f);
      if (gscale < 1)
	{
	  fprintf (stderr, "Error, zero image collors\n");
	  return (2);
	}
      gscale /= 2;
    }

  if (c_chars)
    {
      comment[--c_chars] = 0;
      while (c_chars)
	{
	  c_chars--;
	  if (!isprint (comment[c_chars])
	      || comment[c_chars] == ')' || comment[c_chars] == '(')
	    comment[c_chars] = '.';
	}
      image->comment = strdup (comment);
    }

  image->data = malloc (sizeof (unsigned char) * image->x * image->y);
  image->data_orig = malloc (sizeof (unsigned char) * image->x * image->y);

  for (i = 0; i < image->y; i++)
    {
      if (mode == 5)
	if (1 !=
	    fread (image->data + image->x * i, image->x,
		   sizeof (unsigned char), f))
	  {
	    fprintf (stderr, "Error, short image file\n");
	    return (2);
	  }
      if (mode == 4)
	if (1 !=
	    fread (image->data + image->x * i,
		   image->x / 8 + ((image->x % 8) != 0),
		   sizeof (unsigned char), f))
	  {
	    fprintf (stderr, "Error, short image file\n");
	    return (2);
	  }
    }
  fclose (f);

  if (mode == 5)
    {
      if (!(flags & M_GRAY))
	for (i = 0; i < (image->x * image->y); i++)
	  *(image->data + i) = *(image->data + i) > gscale ? 255 : 0;
    }

  if (mode == 4)
    {
      int x;

      for (i = 0; i < image->y; i++)
	for (x = image->x - 1; x >= 0; x--)
	  *(image->data + image->x * i + x) =
	    (*(image->data + image->x * i + x / 8) & (128 >> (x % 8))) ? 0 :
	    255;
    }
  memcpy (image->data_orig, image->data, image->x * image->y);
  return 0;

input_img_read_error:
  fprintf (stderr, " unknown image, not P4 or P5 pgm \n");
  return (2);
}


void
create_border (struct image *image)
{
  int sx;

  for (sx = 0; sx < image->x; sx++)
    {
      *(image->data + sx) = 0;
      *(image->data + sx + image->x) = 0;
      *(image->data + sx + image->x * (image->y - 2)) = 0;
      *(image->data + sx + image->x * (image->y - 1)) = 0;
    }
  for (sx = 0; sx < image->y; sx++)
    {
      *(image->data + sx * image->x) = 0;
      *(image->data + sx * image->x + 1) = 0;
      *(image->data + sx * image->x + image->x - 2) = 0;
      *(image->data + sx * image->x + image->x - 1) = 0;
    }
}

int
img_write (struct image *image, char *name, char *comment)
{
  FILE *f;
  int i;

  f = fopen (name, "w");
  if (!f)
    return 1;
  fprintf (f, "P5\n# %s\n%d %d\n255\n", comment, image->x, image->y);
  for (i = 0; i < image->y; i++)
    fwrite (image->data + image->x * i, image->x, sizeof (unsigned char), f);
  fclose (f);
  return 0;
}

int
debug_write (struct image *image, char *name)
{
  FILE *f;
  int i, x, y;
  unsigned char *buffer;

  f = fopen (name, "w");
  if (!f)
    return 1;
  fprintf (f, "P5\n%d %d\n255\n", image->x * 3, image->y * 3);

  buffer = malloc (sizeof (unsigned char) * image->x * 3);


  for (y = 0; y < image->y; y++)
    {
      for (i = x = 0; x < image->x; x++, i += 3)
	buffer[i] = buffer[i + 1] = buffer[i + 2] =
	  *(image->data + x + image->x * y);

      fwrite (buffer, image->x * 3, sizeof (unsigned char), f);
      fwrite (buffer, image->x * 3, sizeof (unsigned char), f);
      fwrite (buffer, image->x * 3, sizeof (unsigned char), f);
    }
  fclose (f);
  free (buffer);
  return 0;
}
