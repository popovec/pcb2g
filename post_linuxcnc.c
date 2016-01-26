/*
    post_linuxcnc.c

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

    Postprocesor for linuxcnc (G code generator)

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "post.h"


#define FD_TEST(p) {if(!p) return;if(!(p->data))return;}
#define DATA(p) ((struct linuxcnc_local_data *)((p)->data))


struct linuxcnc_local_data
{
  FILE *fd;

  enum POST_MACHINE_OPS op;

  double safe_traverse;
  double route_retract;
  double hole_retract;

  int spindle_etch_rpm;
  int spindle_drill_rpm;
  int spindle_cut_rpm;

  double etch_tool_dia;
  double etch_speed;
  double etch_rpm;

  double drill_speed;
  double drill_rpm;

  double cnc_G64P;
  double cnc_G64Q;

  double cut_tool_dia;
  double cut_speed;
  double cut_rpm;

  int mm_inch;			// 0 mm 1 inch
  int conditional;		// if set, print "/" before commands
  int comp;			// cutter compensation 0 none, comp<0 comp LEFT, comp>0 comp RIGHT
  // diameter from cut_tool_dia

  double last_x, last_y, last_z;

};


static void
linuxcnc_operation (struct postprocesor *p, enum POST_MACHINE_OPS op)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  switch (op)
    {
    case MACHINE_IDLE:
      if (DATA (p)->comp)
	{
	  DATA (p)->comp = 0;
	  fprintf (DATA (p)->fd,
		   "G40 (turn off cutter radius compensation)\n");
	}
      if (DATA (p)->conditional)
	fprintf (DATA (p)->fd, "/");
      fprintf (DATA (p)->fd, "G0 Z%.4f\n", DATA (p)->safe_traverse);
      DATA (p)->last_z = DATA (p)->safe_traverse;

      if (DATA (p)->conditional)
	fprintf (DATA (p)->fd, "/");
      fprintf (DATA (p)->fd, "M5\n");
      DATA (p)->conditional = 0;
      if (DATA (p)->op == MACHINE_ETCH || DATA (p)->op == MACHINE_CUT)
	fprintf (DATA (p)->fd, "/M0\n");
      break;
    case MACHINE_SETUP:
      if (DATA (p)->mm_inch == 0)
	fprintf (DATA (p)->fd, "G21 (milimeter mode)\nG90 (absolute mode)\n");
      else
	fprintf (DATA (p)->fd, "G20 (inch mode)\nG90 (absolute mode)\n");

      fprintf (DATA (p)->fd, "G40 (turn off cutter radius compensation)\n");
      fprintf (DATA (p)->fd, "G49 (Cancel tool length compensation)\n");
      fprintf (DATA (p)->fd, "G17 (select xy plane)\n");
      fprintf (DATA (p)->fd, "G90.1 (Absolute Arc Distance Mode)\n");

      fprintf (DATA (p)->fd,
	       "G98 (return tool to original position after canned cycle)\n");

      fprintf (DATA (p)->fd, "G64 P%f Q%f (Set Path Control Mode)\n",
	       DATA (p)->cnc_G64P, DATA (p)->cnc_G64Q);
      fprintf (DATA (p)->fd, "G0 Z%.4f\n", DATA (p)->safe_traverse);
      DATA (p)->last_z = DATA (p)->safe_traverse;
      fprintf (DATA (p)->fd, "G0 X0 Y0\n");
      DATA (p)->last_x = 0;
      DATA (p)->last_y = 0;

      break;
    case MACHINE_ETCH:
      DATA (p)->conditional = 1;
      fprintf (DATA (p)->fd, "/G0 Z%.4f\n", DATA (p)->safe_traverse);
      DATA (p)->last_z = DATA (p)->safe_traverse;
      fprintf (DATA (p)->fd, "/M3 S%d\n", DATA (p)->spindle_etch_rpm);
      fprintf (DATA (p)->fd, "/F%.4f\n", DATA (p)->etch_speed);
      break;

    case MACHINE_DRILL:
      fprintf (DATA (p)->fd, "G0 Z%.4f\n", DATA (p)->safe_traverse);
      DATA (p)->last_z = DATA (p)->safe_traverse;
      fprintf (DATA (p)->fd, "M3 S%d\n", DATA (p)->spindle_drill_rpm);
      fprintf (DATA (p)->fd, "F%.4f\n", DATA (p)->drill_speed);
      fprintf (DATA (p)->fd,
	       "G99 (the canned cycle will use the R value as the Z return position)\n");
      break;

    case MACHINE_CUT:
      DATA (p)->conditional = 1;
      fprintf (DATA (p)->fd, "/G0 Z%.4f\n", DATA (p)->safe_traverse);
      DATA (p)->last_z = DATA (p)->safe_traverse;
      fprintf (DATA (p)->fd, "/M3 S%d\n", DATA (p)->spindle_cut_rpm);
      fprintf (DATA (p)->fd, "/F%.4f\n", DATA (p)->cut_speed);
      break;

    case MACHINE_END:
      fprintf (DATA (p)->fd, "M2\n");
      break;
    }
  DATA (p)->op = op;
}


static void
linuxcnc_open (struct postprocesor *p, char *filename)
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
	  if (0 == strcmp (filename + len - 4, ".ngc"))
	    DATA (p)->fd = fopen (filename, "w");
	}

      if (!(DATA (p)->fd))
	{
	  char *f;
	  f = calloc (sizeof (char), len + 5);
	  strcpy (f, filename);
	  strcat (f, ".ngc");
	  DATA (p)->fd = fopen (f, "w");
	  free (f);
	}
    }
  else
    DATA (p)->fd = stderr;
}

static void
linuxcnc_close (struct postprocesor *p)
{
  FD_TEST (p);
  if (DATA (p)->fd)
    fclose (DATA (p)->fd);
  free (p->data);
}

static void
linuxcnc_write_comment (struct postprocesor *p, char *comment)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;
  fprintf (DATA (p)->fd, "(%s)\n", comment);
}

static void
linuxcnc_set (struct postprocesor *p, enum POST_SET op, va_list ap)
{
  double dia;
  int comp;

  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  switch (op)
    {
    case POST_SET_X:
    case POST_SET_Y:
      break;
    case POST_SET_ETCH:
      DATA (p)->etch_tool_dia = va_arg (ap, double);
      DATA (p)->etch_rpm = va_arg (ap, double);
      DATA (p)->etch_speed = va_arg (ap, double);
      break;
    case POST_SET_CUT:
      DATA (p)->cut_tool_dia = va_arg (ap, double);
      DATA (p)->cut_rpm = va_arg (ap, double);
      DATA (p)->cut_speed = va_arg (ap, double);
      break;
    case POST_SET_ROUTE_RETRACT:
      DATA (p)->route_retract = va_arg (ap, double);
      break;
    case POST_SET_HOLE_RETRACT:
      DATA (p)->hole_retract = va_arg (ap, double);
    case POST_SET_SAFE_TRAVERSE:
      DATA (p)->safe_traverse = va_arg (ap, double);
      break;
    case POST_SET_CUTTER_COMP:
      comp = va_arg (ap, int);
      dia = 0;

      if (DATA (p)->op == MACHINE_ETCH)
	dia = DATA (p)->etch_tool_dia;
      if (DATA (p)->op == MACHINE_CUT)
	dia = DATA (p)->cut_tool_dia;

      if (dia == 0 || comp == 0)
	{
	  if (DATA (p)->comp != 0)
	    {
	      if (DATA (p)->conditional)
		fprintf (DATA (p)->fd, "/");

	      fprintf (DATA (p)->fd,
		       "G40 (turn off cutter radius compensation)\n");
	    }
	  DATA (p)->comp = 0;
	  break;
	}
      if (DATA (p)->conditional)
	fprintf (DATA (p)->fd, "/");

      if (comp < 0)
	fprintf (DATA (p)->fd, "G41.1 D%.2f\n", dia);
      else
	fprintf (DATA (p)->fd, "G42.1 D%.2f\n", dia);

      DATA (p)->comp = comp;
      break;
    }
}

//move tool down if needed
static void
route_entry (struct postprocesor *p)
{
  if (DATA (p)->last_z > DATA (p)->route_retract)
    {
      if (DATA (p)->conditional)
	fprintf (DATA (p)->fd, "/");
      fprintf (DATA (p)->fd, "G0 Z%.3f\n", DATA (p)->route_retract);
      DATA (p)->last_z = DATA (p)->route_retract;
    }

  if (DATA (p)->last_z > 0)
    {
      if (DATA (p)->conditional)
	fprintf (DATA (p)->fd, "/");
      fprintf (DATA (p)->fd, "G1 Z0\n");
      DATA (p)->last_z = 0;
    }
}

static void
linuxcnc_route (struct postprocesor *p, double x, double y)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  route_entry (p);
  if (DATA (p)->conditional)
    fprintf (DATA (p)->fd, "/");
  fprintf (DATA (p)->fd, "G1 X%.3f Y%.3f\n", x, y);
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
}

static void
linuxcnc_route_arc (struct postprocesor *p, double x, double y, double cx,
		    double cy, int dir)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;
  route_entry (p);

  if (DATA (p)->conditional)
    fprintf (DATA (p)->fd, "/");
  fprintf (DATA (p)->fd, "G%c X%.3f Y%.3f I %.3f J %.3f\n",
	   dir < 0 ? '2' : '3', x, y, cx, cy);
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
}

static void
linuxcnc_rapid (struct postprocesor *p, double x, double y)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;
  if (DATA (p)->last_z < DATA (p)->route_retract)
    {
      if (DATA (p)->conditional)
	fprintf (DATA (p)->fd, "/");
      fprintf (DATA (p)->fd, "G0 Z%.3f\n", DATA (p)->route_retract);
      DATA (p)->last_z = DATA (p)->route_retract;
    }
  if (DATA (p)->conditional)
    fprintf (DATA (p)->fd, "/");
  fprintf (DATA (p)->fd, "G0 X%.3f Y%.3f\n", x, y);
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
}


static void
linuxcnc_hole (struct postprocesor *p, double x, double y, double dia)
{
  FD_TEST (p);
  if (!DATA (p)->fd)
    return;

  fprintf (DATA (p)->fd, "G81 X%.3f Y%.3f Z0 R%.3f\n", x, y,
	   DATA (p)->hole_retract);
  DATA (p)->last_z = DATA (p)->hole_retract;
  DATA (p)->last_x = x;
  DATA (p)->last_y = y;
}



static struct post_operations post_linuxcnc = {
  .open = linuxcnc_open,
  .close = linuxcnc_close,

  .operation = linuxcnc_operation,
  .set = linuxcnc_set,
  .write_comment = linuxcnc_write_comment,
  .route = linuxcnc_route,	//move tool by G1 move .. 
  .route_arc = linuxcnc_route_arc,
  .rapid = linuxcnc_rapid,	//do rapid, first move tool to level Z, them go to x,y position, move Z to 0
  .hole = linuxcnc_hole,

};




struct postprocesor *
init (void)
{
  struct linuxcnc_local_data *data;

  struct postprocesor *p;

  data = calloc (sizeof (struct linuxcnc_local_data), 1);
  data->spindle_etch_rpm = 9000;
  data->spindle_drill_rpm = 18000;
  data->spindle_cut_rpm = 18000;

  data->mm_inch = 0;		//mm mode (1 = inch) 
  if (!data->mm_inch)
    {
      data->cnc_G64P = 0.01;
      data->cnc_G64Q = 0.01;
      data->safe_traverse = 25.0;	//retract for safe move to PCB area (over wise etc..)

      data->route_retract = 2.0;	//in routing mode safe retract value
      data->hole_retract = 3.0;	//for holes safe retract

      data->etch_tool_dia = 0.5;	//for etching isolation lines
      data->etch_speed = 500;
      data->etch_rpm = 18000;
//1mm type 626 or 726 (HPTec, www.hptec.de)
      data->cut_tool_dia = 1.0;	//for cutting internal holes and PCB border
      data->cut_speed = 150;
      data->cut_rpm = 18000;

//drill parameters
//0.7mm drill (HPTec standard, www.hptec.de)
      data->drill_speed = 1000;	//vertical movement speed
      data->drill_rpm = 18000;	//RPM
    }
  else
    {
      data->cnc_G64P = 0.254;
      data->cnc_G64Q = 0.254;
      data->safe_traverse = 1.0;	//retract for safe move to PCB area (over wise etc..)
      data->route_retract = 0.08;	//in routing mode safe retract value
      data->hole_retract = 0.12;	//for holes safe retract

      data->etch_tool_dia = 0.02;	//for etching isolation lines
      data->etch_speed = 20;
      data->etch_rpm = 18000;

      data->cut_tool_dia = 0.04;	//for cutting internal holes and PCB border
      data->cut_speed = 20;
      data->cut_rpm = 18000;

      data->drill_speed = 40;
      data->drill_rpm = 18000;
    }

  p = postprocesor_register (&post_linuxcnc, data, "linuxcnc");
  if (!p)
    free (data);

  return p;
}
