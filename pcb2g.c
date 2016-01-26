/*
    pcb2g.c

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

    Main file

*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include "pcb2g.h"
#include "post.h"

#ifndef VERSION
VERSION = 0
#endif
int debug_level = 0;

static uint16_t
ucrc (uint16_t crc, uint8_t data)
{
  int i;

  crc ^= data;
  for (i = 0; i < 8; ++i)
    if (crc & 1)
      crc = (crc >> 1) ^ 0xa001;
    else
      crc = (crc >> 1);

  return crc;
}

void
image_crc (struct image *image)
{

  uint16_t crc = 0xffff;
  int i;

  for (i = 0; i < (image->x * image->y); i++)
    crc = ucrc (crc, image->data[i]);

  snprintf (image->crc, 5, "%04X", crc);
}

double
i2realX (struct image *image, int x)
{
  if (image->real_x > 0 && image->real_y > 0)
    {
      return ((double) x * image->real_x) / (double) (image->x);
    }
  else
    return (double) x *127.0 / (double) image->dpi / 5.0;
}

double
i2realY (struct image *image, int y)
{
  if (image->real_x > 0 && image->real_y > 0)
    {
      return ((double) y * image->real_y) / (double) (image->y);
    }
  else
    return (double) y *127.0 / (double) image->dpi / 5.0;
}

int
main (int argc, char *argv[])
{

  int sx, sy;
  int opt;

  struct image *image, *image_fast;
  struct fill_data *v;
  struct timespec ts_temp;
  struct timespec ts;
  time_t now;
  char *str_now;


  time (&now);
  str_now = strdup (ctime (&now));
  *(strchr (str_now, '\n')) = 0;

  image = calloc (1, sizeof (struct image));

  clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &(image->ts));
  clock_gettime (CLOCK_REALTIME, &(ts));

  image->dpi = 254;
  image->route_border = 0;	//1 route border over pcb
  image->auto_border = 0;	//1 if PCB is not retrangular, expand border
  image->route_retract = 2.0;	//in routing mode safe retract value
  image->hole_retract = 3.0;	//for holes safe retract
  image->safe_traverse = 25.0;	//retract for safe move to PCB area (over wise etc..)
  image->route_optimize = 0;	//default no optimize router path
  image->drill_file = NULL;
  image->image_file = strdup ("pcb.pgm");
  image->output_file = NULL;
  image->cut_file = NULL;
  image->debug_files = 0;	//do not create debug files
  image->real_x = 0;
  image->real_y = 0;
  image->first_hole = NULL;
  image->comment = NULL;
  image->mtime = NULL;
  image->cnc_G64P = 0.01;
  image->cnc_G64Q = 0.01;
  image->hole_asymmetry = 0.16;

  image->cut.rpm = 15000;
  image->cut.dia = 1.0;
  image->cut.a_speed = 120;
  image->cut.r_speed = 120;

  image->drill.rpm = 18000;
  image->drill.dia = 1.0;
  image->drill.a_speed = 1000;
  image->drill.r_speed = 0;

  image->etch.rpm = 12000;
  image->etch.dia = 0.5;
  image->etch.a_speed = 150;
  image->etch.r_speed = 150;


  for (sx = 0, opt = 1; opt < argc; opt++)
    sx += 1 + strlen (argv[opt]);

  image->commandline = calloc (sizeof (char), sx);
  for (sx = 0, opt = 1; opt < argc; opt++)
    {
      if (opt != 1)
	image->commandline = strcat (image->commandline, " ");
      image->commandline = strcat (image->commandline, argv[opt]);
    }
  for (opt = 0; opt < sx; opt++)
    if (!isprint (image->commandline[opt])
	|| image->commandline[opt] == ')' || image->commandline[opt] == '(')
      image->commandline[opt] = '.';


  while ((opt = getopt (argc, argv, "+dbBo::ht:r:R:D:O:X:Y:e:c:H:")) != -1)
    {
      switch (opt)
	{
	case 'H':
	  image->hole_asymmetry = atoi (optarg) / 100.0;
	  if (image->hole_asymmetry > 0.5)
	    image->hole_asymmetry = 0.16;
	  if (image->hole_asymmetry < 0.05)
	    image->hole_asymmetry = 0.16;
	  break;
	case 'X':
	  image->real_x = fabs (atof (optarg));
	  break;
	case 'Y':
	  image->real_y = fabs (atof (optarg));
	  break;
	case 'r':
	  image->route_retract = fabs (atof (optarg));
	  break;
	case 'R':
	  image->hole_retract = fabs (atof (optarg));
	  break;
	case 't':
	  image->safe_traverse = fabs (atof (optarg));
	  break;
	case 'D':
	  image->dpi = abs (atoi (optarg));
	  if (image->dpi < 150)
	    {
	      fprintf (stderr,
		       "DPI below 150, please use better image (DPI in range 150 - 1600)\n");
	      exit (1);
	    }
	  break;
	  if (image->dpi > 1600)
	    printf
	      ("Warning DPI over 1600, running time can take several minutes\n");
	  break;
	case 'b':
	  image->route_border = 1;
	  break;
	case 'B':
	  image->auto_border = 1;
	  break;
	case 'o':
	  if (NULL != optarg)
	    image->route_optimize = atoi (optarg);
	  else
	    image->route_optimize++;

	  break;
	case 'O':
	  image->output_file = strdup (optarg);
	  break;
	case 'd':
	  image->debug_files = 1;
	  break;
	case 'e':
	  {
	    char *p = optarg;
	    if (p)
	      {
		sscanf (p, "%lf", &(image->etch.dia));
		p = strstr (p, ",");
	      }
	    if (p)
	      {
		sscanf (++p, "%lf", &(image->etch.r_speed));
		p = strstr (p, ",");
	      }
	    if (p)
	      {
		sscanf (++p, "%lf", &(image->etch.a_speed));
		p = strstr (p, ",");
	      }
	    if (p)
	      {
		sscanf (++p, "%lf", &(image->etch.rpm));
	      }
	  }
	  break;
	case 'c':
	  {
	    char *p = optarg;
	    if (p)
	      {
		sscanf (p, "%lf", &(image->cut.dia));
		p = strstr (p, ",");
	      }
	    if (p)
	      {
		sscanf (++p, "%lf", &(image->cut.r_speed));
		p = strstr (p, ",");
	      }
	    if (p)
	      {
		sscanf (++p, "%lf", &(image->cut.a_speed));
		p = strstr (p, ",");
	      }
	    if (p)
	      {
		sscanf (++p, "%lf", &(image->cut.rpm));
	      }
	  }
	  break;
	case 'h':
	  printf
	    ("pcb2g [options] <input_image_file> [input_drill_file] [input_cut_file]\n");
	  printf ("-h this help\n");
	  printf ("-b route PCB border\n");
	  printf
	    ("-B expand border for non retrangular PCB (experimental)\n");
	  printf ("-r retract Z in route mode (default %2.2f)\n",
		  image->route_retract);
	  printf ("-R retract Z in drill hole mode (default %2.2f)\n",
		  image->hole_retract);
	  printf ("-t safe traverse value (default %2.2f)\n",
		  image->safe_traverse);
	  printf ("-X image size X\n");
	  printf ("-Y image size Y\n");
	  printf
	    ("-D DPI of image (default 254), do not use if X and Y is set\n");
	  printf ("-O output G code file (default stderr)\n");
	  printf
	    ("-o optimization level, multile -o can be used or argument\n   can be used to set optimization level\n");
	  printf
	    ("-H defines hole asymmetry for automatic hole detection (5 to 20%%, default 16%%)\n");
	  printf
	    ("\nTOOL parameter definition: diameter(mm), radial speed (mm/min), axial speed(mm/min), rpm)\n");
	  printf
	    ("-e etching tool parameters (default %2.2f,%.0f,%.0f,%.0f)\n",
	     image->etch.dia, image->etch.r_speed, image->etch.a_speed,
	     image->etch.rpm);
	  printf
	    ("-c cutting tool parameters (default %2.2f,%.0f,%.0f,%.0f)\n",
	     image->cut.dia, image->cut.r_speed, image->cut.a_speed,
	     image->cut.rpm);
	  exit (0);
	}
    }

  /* check rest of command line arguments */
  if (argc > optind)
    {
      /* set command line filename for input image */
      if (image->image_file)
	free (image->image_file);
      image->image_file = strdup (argv[optind]);

      if (argc > optind + 1)	/* if command line describe drill file, use it */
	{
	  if (image->drill_file)
	    free (image->drill_file);
	  image->drill_file = strdup (argv[optind + 1]);
	  get_drill_file (image);
	}
      if (argc > optind + 2)	/* if command line describe drill file, use it */
	{
	  if (image->cut_file)
	    free (image->cut_file);
	  image->cut_file = strdup (argv[optind + 2]);
	}
    }
  else
    {
      printf ("Use pcb2g -h to get short help\n");
      exit (0);
    }

  if (input_img_read (image, 0))
    exit (1);
  image_crc (image);

  create_border (image);

  for (sy = 0; sy < image->y; sy++)
    for (sx = 0; sx < image->x; sx++)

      if (*(image->data + sx + image->x * sy) == 255)
	{
	  if (NULL != (v = image_fill (image, sx, sy, 100, 0)))
	    {
/*
	      int dx, dy;
	      double p1, p2, p3;
	      dx = v->xmax - v->xmin;
	      dy = v->ymax - v->ymin;

	      p1 = 2.0 * sqrt (v->count / 3.14159);
	      p2 = (dx + dy) / 2.0;
	      printf
		("area %d %d   %d %d\ncenter= %f %f\ncount=%d[%f] (%d %d)[%f] level=%d\n",
		 v->xmin, v->xmax, v->ymin, v->ymax,
		 (v->xmin + v->xmax) / 2.0, (v->ymin + v->ymax) / 2.0,
		 v->count, p1, dx, dy, p2, v->level);
	      p3 = fabs (p1 - p2);
	      printf ("%f %f %f %f\n", (p3 + p1) / p3, (p3 + p2) / p3, p3,
		      p2 / p1);
	      printf ("width %d height %d\n", (v->xmax - v->xmin),
		      (v->ymax - v->ymin));
*/

	      if (v->level == 1)
		{
		  if (fabs
		      (1 -
		       (double) (v->xmax - v->xmin) /
		       (double) (v->ymax - v->ymin)) <
		      image->hole_asymmetry && (v->xmax - v->xmin) > 1)
		    {
		      if (image->drill_file == NULL)
			create_hole (image, v);
		      image_fill (image, sx, sy, C_HOLE, 0);
		    }

		  else
		    {
		      if (image->auto_border)
			image_fill (image, sx, sy, C_HOLE, 0);
		    }
		}
	    }
	}
  /* if already exist image for acceleration, use it */
  image_fast = calloc (1, sizeof (struct image));
  image_fast->image_file = strdup ("out.pgm");

  if (0 == input_img_read (image_fast, M_GRAY))
    {
      // Check if dimensions and CRC is same as original omage
      if (image_fast->comment
	  && 0 == strncmp (image_fast->comment + 1, image->crc, 4)
	  && image->x == image_fast->x && image->y == image_fast->y)
	{
	  printf ("Using debug image to accelerate\n");
	  free (image->data);
	  image->data = image_fast->data;
	}

      else
	{
	  free (image_fast->data);
	  printf ("debug image not match  this image, expanding copper\n");
	  expand_copper (image);
	}
      free (image_fast->comment);
      free (image_fast->mtime);
      free (image_fast->data_orig);
    }
  else
    {
      printf ("no debug image available, expanding copper\n");
      expand_copper (image);
    }
  if (image->debug_files)
    img_write (image, "out.pgm", image->crc);

  free (image_fast->image_file);
  free (image_fast);

  trace (image);
  mark_holes (image);
  if (image->debug_files)
    debug_write (image, "debug.pgm");

  postprocesor_init ();
  postprocesor_open (image->output_file);
  postprocesor_set (POST_SET_X, image->real_x);
  postprocesor_set (POST_SET_Y, image->real_y);

  postprocesor_set (POST_SET_ETCH, (double) image->etch.dia,
		    (double) image->etch.rpm, (double) image->etch.r_speed);

  postprocesor_set (POST_SET_CUT, (double) image->cut.dia,
		    (double) image->cut.rpm, (double) image->cut.r_speed);

  postprocesor_set (POST_SET_ROUTE_RETRACT, image->route_retract);
  postprocesor_set (POST_SET_HOLE_RETRACT, image->hole_retract);
  postprocesor_set (POST_SET_SAFE_TRAVERSE, image->safe_traverse);

  P_COMMENT ("Created by pcb2g [%d], http://pcb2g.fei.tuke.sk at %s", VERSION,
	     str_now);

  free (str_now);

  if (image->commandline)
    P_COMMENT ("pcb2g %s", image->commandline);
  if (image->comment)
    P_COMMENT ("img comment: %s", image->comment);
  P_COMMENT ("image size: %dx%d pixels", image->x, image->y);

  if (image->mtime)
    P_COMMENT ("image date: %s", image->mtime);

  postprocesor_operation (MACHINE_SETUP);

  if (image->route_border && image->cut_file == NULL)
    {
      postprocesor_write_comment (" === BORDER etch start === ");
      postprocesor_operation (MACHINE_CUT);
      postprocesor_rapid (-image->cut.dia / 2.0, -image->cut.dia / 2.0);
      postprocesor_route (i2realX (image, image->x) +
			  image->cut.dia / 2.0, -image->cut.dia / 2.0);
      postprocesor_route (i2realX (image, image->x) +
			  image->cut.dia / 2.0, i2realY (image,
							 image->y) +
			  image->cut.dia / 2.0);
      postprocesor_route (-image->cut.dia / 2.0,
			  i2realY (image, image->y) + image->cut.dia / 2.0);
      postprocesor_route (-image->cut.dia / 2.0, -image->cut.dia / 2.0);

      postprocesor_operation (MACHINE_IDLE);
      postprocesor_write_comment (" === BORDER end === ");
    }
  if (image->cut_file)
    create_cut (image->cut_file);

  dump_lines (image);
  dump_holes (image);
  postprocesor_operation (MACHINE_END);

  clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &ts_temp);
  ts_temp = tsx_diff (image->ts, ts_temp);
  printf ("proces CPU run time %ld.%09ld\n", ts_temp.tv_sec, ts_temp.tv_nsec);
  clock_gettime (CLOCK_REALTIME, &(ts_temp));
  ts_temp = tsx_diff (ts, ts_temp);
  printf ("proces run time %ld.%09ld\n", ts_temp.tv_sec, ts_temp.tv_nsec);
  postprocesor_close ();

  free (image->mask_data);
  free (image->or_mask);
  free (image->data);
  free (image->data_orig);
  if (image->comment)
    free (image->comment);
  if (image->drill_file)
    free (image->drill_file);
  free (image->image_file);
  free (image->mtime);
  free (image->commandline);
  if (image->output_file)
    free (image->output_file);
  if (image->cut_file)
    free (image->cut_file);

  free (image);
  return 0;
}

struct timespec
tsx_diff (struct timespec start, struct timespec end)
{
  struct timespec temp;
  if ((end.tv_nsec - start.tv_nsec) < 0)
    {
      temp.tv_sec = end.tv_sec - start.tv_sec - 1;
      temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
  else
    {
      temp.tv_sec = end.tv_sec - start.tv_sec;
      temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
  return temp;
}
