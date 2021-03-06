/*
    post_svg.c

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

    Postprocesor, SVG file generator

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "post.h"
#include "pgeom.h"

//SVG unit = 0.01
#define ZOOM 2.0
//measurment unit
#define MM "mm"
//0.5mm etch tool
#define SW  0.50
//drill stroke 0.1mm
#define DSW 0.10
//drill default 0.7mm holes (70+stroke)
#define DRILL 0.70

#define FD_TEST(p) {if(!p) return;if(!(p->data))return;}
#define DATA(p) ((struct svg_local_data *)((p)->data))


struct svg_local_data
{
  FILE *fd;

  enum POST_MACHINE_OPS op;
  double etch_tool_dia;
  double cut_tool_dia;
  double real_x, real_y;

  double last_x, last_y;
  int r_flag;
  double tool_dia;

//TODO use compensation
  int comp;			//tool radius compensation
};



static void
svg_operation (struct postprocesor *p, enum POST_MACHINE_OPS op)
{
  double h;

  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  switch (op)
    {
    case MACHINE_IDLE:
      if (DATA (p)->r_flag)
	fprintf (DATA (p)->fd, "\" />\n");
      DATA (p)->r_flag = 0;
      fprintf (DATA (p)->fd, "</g>\n");
      DATA (p)->tool_dia = 0;
      break;
    case MACHINE_SETUP:
      h = DATA (p)->cut_tool_dia;
      if (DATA (p)->etch_tool_dia > DATA (p)->cut_tool_dia)
	h = DATA (p)->etch_tool_dia;
      fprintf (DATA (p)->fd, "<?xml version=\"1.0\" standalone=\"no\"?>\n");
      fprintf (DATA (p)->fd,
	       "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n");
      fprintf (DATA (p)->fd,
	       "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
      fprintf (DATA (p)->fd,
	       "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"\n");

      fprintf (DATA (p)->fd, "  width=\"%f%s\" height=\"%f%s\" viewBox=\"%f %f %f %f \" >\n", DATA (p)->real_x + h * 2.0, MM, DATA (p)->real_y + h * 2.0, MM, (-h * ZOOM),	//min X
	       (-h * ZOOM),	//min Y
	       ((DATA (p)->real_x + h * 2.0) * ZOOM),	//width 
	       ((DATA (p)->real_y + h * 2.0) * ZOOM));	//height
      fprintf (DATA (p)->fd, "<title>PCB2G, debug file</title>\n");
      fprintf (DATA (p)->fd,
	       "<desc>Picture generated by pcb2g, http://pcb2g.fei.tuke.sk</desc>\n");
      fprintf (DATA (p)->fd,
	       "<g style=\"fill:black; stroke:black; stroke-width:1\"></g>\n");
      DATA (p)->tool_dia = 0;
      break;

    case MACHINE_ETCH:
      fprintf (DATA (p)->fd, "<g style=\"fill:#FFFFFF; fill-opacity:0.0; \n");
      fprintf (DATA (p)->fd,
	       "  stroke:#000000; stroke-linecap:round; stroke-linejoin:round; stroke-opacity:1.0; stroke-width:%f\"\n",
	       DATA (p)->etch_tool_dia * ZOOM);
      DATA (p)->tool_dia = DATA (p)->etch_tool_dia;
      fprintf (DATA (p)->fd, "   transform=\"translate(0 0) scale(1 1)\">\n");
      DATA (p)->r_flag = 0;
      break;

    case MACHINE_DRILL:
      fprintf (DATA (p)->fd,
	       "<g style=\"fill:#FFFFFF; fill-opacity:1.0; stroke:#FF0000; stroke-linecap:round; ");
      fprintf (DATA (p)->fd,
	       "stroke-linejoin:round; stroke-opacity:1.0; stroke-width:%f\"\n",
	       DSW);
      fprintf (DATA (p)->fd, "transform=\"translate(0 0) scale(1 1)\">\n");
      break;
    case MACHINE_CUT:
      fprintf (DATA (p)->fd, "<g style=\"fill:#FFFFFF; fill-opacity:0.0; \n");
      fprintf (DATA (p)->fd,
	       "  stroke:#606060; stroke-linecap:round; stroke-linejoin:round; stroke-opacity:1.0; stroke-width:%f\"\n",
	       DATA (p)->cut_tool_dia * ZOOM);
      DATA (p)->tool_dia = DATA (p)->cut_tool_dia;
      fprintf (DATA (p)->fd, "   transform=\"translate(0 0) scale(1 1)\">\n");
      DATA (p)->r_flag = 0;
      break;
    case MACHINE_END:
      fprintf (DATA (p)->fd, "</svg>\n");

      break;
    }
  DATA (p)->op = op;
}


static void
svg_open (struct postprocesor *p, char *filename)
{
  FD_TEST (p);

  if (filename)
    {
      int len;

      DATA (p)->fd = NULL;
      //test default extension...
      len = strlen (filename);
      if (len > 4)
	{
	  if (0 == strcmp (filename + len - 4, ".svg"))
	    DATA (p)->fd = fopen (filename, "w");
	}

      if (!(DATA (p)->fd))
	{
	  char *f;
	  f = calloc (sizeof (char), len + 5);
	  strcpy (f, filename);
	  strcat (f, ".svg");
	  DATA (p)->fd = fopen (f, "w");
	  free (f);
	}
    }
  else
    DATA (p)->fd = stderr;
}

static void
svg_close (struct postprocesor *p)
{
  FD_TEST (p);
  if (DATA (p)->fd)
    fclose (DATA (p)->fd);
  free (p->data);
}

static void
svg_write_comment (struct postprocesor *p, char *comment)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;
}

static void
svg_set (struct postprocesor *p, enum POST_SET op, va_list ap)
{

  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  switch (op)
    {
    case POST_SET_X:
      DATA (p)->real_x = va_arg (ap, double);
      break;
    case POST_SET_Y:
      DATA (p)->real_y = va_arg (ap, double);
      break;
    case POST_SET_ETCH:
      DATA (p)->etch_tool_dia = va_arg (ap, double);
      va_arg (ap, double);
      va_arg (ap, double);
      break;
    case POST_SET_CUT:
      DATA (p)->cut_tool_dia = va_arg (ap, double);
      va_arg (ap, double);
      va_arg (ap, double);
      break;
    case POST_SET_ROUTE_RETRACT:
    case POST_SET_HOLE_RETRACT:
    case POST_SET_SAFE_TRAVERSE:
      break;
    case POST_SET_CUTTER_COMP:
      DATA (p)->comp = va_arg (ap, int);
      break;

    }
}


static void
svg_route (struct postprocesor *p, double x, double y)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  DATA (p)->r_flag = 1;
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
  fprintf (DATA (p)->fd, "L%f %f ", (x * ZOOM), (y * ZOOM));
}

static void
svg_route_arc (struct postprocesor *p, double x, double y, double cx,
	       double cy, int dir)
{
  double radius;
  double dist;
  int lg;

  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  dir = dir ? 1 : 0;
  //calculate if center is on left or right side of line start-end
  dist =
    lineToPointSDistance2D (DATA (p)->last_x, DATA (p)->last_y, x, y, cx, cy);
  radius = ZOOM * sqrt ((cx - x) * (cx - x) + (cy - y) * (cy - y));

  lg = dist > 0 ? 0 : 1;
  lg ^= dir;

  if (x == DATA (p)->last_x && y == DATA (p)->last_y)	//circle
    {
      fprintf (DATA (p)->fd, "a%f,%f 0 0,0 +%f 0 ", radius, radius,
	       2.0 * radius + DATA (p)->tool_dia / 2.0);
      fprintf (DATA (p)->fd, "a%f,%f 0 0,0 -%f 0 ", radius, radius,
	       2.0 * radius + DATA (p)->tool_dia / 2.0);
    }
  else
    fprintf (DATA (p)->fd, "A%f,%f 0 %d,%d  %f %f ", radius, radius, lg, dir,
	     (x * ZOOM), (y * ZOOM));

  DATA (p)->r_flag = 1;
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
}


static void
svg_rapid (struct postprocesor *p, double x, double y)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;
  if (DATA (p)->r_flag)
    fprintf (DATA (p)->fd, "\" />\n");
  DATA (p)->r_flag = 1;
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
  fprintf (DATA (p)->fd, "<path d=\"M%f %f ", (x * ZOOM), (y * ZOOM));
}


static void
svg_hole (struct postprocesor *p, double x, double y, double r)
{
  double ri;

  FD_TEST (p);
  if (!DATA (p)->fd)
    return;
  r = r / 2.0;
  ri = (r * ZOOM - DSW / 2.0);
  //0.8mm drill hole is too small for SVG 
  if (ri < DSW / 2.0)
    ri = DRILL;
  fprintf (DATA (p)->fd,
	   "<ellipse cx=\"%f\" cy=\"%f\" rx=\"%f\" ry=\"%f\"  />\n",
	   (x * ZOOM), (y * ZOOM), ri, ri);
}



static struct post_operations post_svg = {
  .open = svg_open,
  .close = svg_close,

  .operation = svg_operation,
  .set = svg_set,
  .write_comment = svg_write_comment,
  .route = svg_route,		//move tool by G1 move .. 
  .rapid = svg_rapid,		//do rapid, first move tool to level Z, them go to x,y position, move Z to 0
  .hole = svg_hole,
  .route_arc = svg_route_arc,

};



struct postprocesor *
init (void)
{
  struct svg_local_data *data;

  struct postprocesor *p;
  data = calloc (sizeof (struct svg_local_data), 1);
  data->etch_tool_dia = 0.5;	//for etching isolation lines
  data->cut_tool_dia = 1.0;	//for cut board
  data->r_flag = 0;

  p = postprocesor_register (&post_svg, data, "svg");
  if (!p)
    free (data);

  return p;
}
