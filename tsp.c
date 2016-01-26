/*
    tsp.c

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

    External TSP solver wrapper / simple tsp solver if external 
    solver is not available.

*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "tsp.h"

//#define TSP_DEBUG 1

#ifdef TSP_DEBUG
#define  DPRINT(msg...) printf(msg)
#else
#define  DPRINT(msg...)
#endif


struct tsp_data
{
  double x;
  double y;
  void *private_data;
};

struct tsp
{
  FILE *f;
  char *ifile;
  int count;
  struct tsp_data *data;
  char *line;
  struct tsp_data *ret_data;
  int ret_index;
  int ok;
};
/*
null fail
other = descriptor
*/
struct tsp *
tsp_init ()
{
  return calloc (sizeof (struct tsp), 1);
}

/*
 0 ok,
 -1 fail
 */
int
tsp_add (struct tsp *tsp, double x, double y, void *data)
{

  if (!tsp)
    return -1;

  if ((tsp->count & 511) == 0)
    {
      tsp->data =
	realloc (tsp->data,
		 sizeof (struct tsp_data) * (tsp->count / 512 + 1) * 512);
      DPRINT ("TSP realloc %p bytes %ld\n", tsp->data,
	      (long) sizeof (struct tsp_data) * (tsp->count / 512 + 1) * 512);
    }
  if (tsp->data == NULL)
    return -1;
  tsp->data[tsp->count].x = x;
  tsp->data[tsp->count].y = y;
  tsp->data[tsp->count].private_data = data;
  tsp->count++;
  return 0;
}


static int
tsp_solve_prepare_data (struct tsp *tsp)
{
  int count = 0;

  if (!tsp)
    return -1;

  fprintf (tsp->f,
	   "NAME : pcb\nCOMMENT : pcb2g\nTYPE : TSP\nDIMENSION : %d\n",
	   tsp->count);
  fprintf (tsp->f, "EDGE_WEIGHT_TYPE : EUC_2D\nNODE_COORD_SECTION\n");

  while (count < tsp->count)
    {
      fprintf (tsp->f, "%d %e %e\n", count + 1, tsp->data[count].x,
	       tsp->data[count].y);
      count++;
    }
  fprintf (tsp->f, "EOF\n");
  fclose (tsp->f);

  return 0;
}

int
tsp_solve (struct tsp *tsp)
{
  char *command;
  FILE *tmp_file;
  char *line = NULL;
  size_t count;

  if (!tsp)
    return -1;

  tsp->ret_data = malloc (sizeof (struct tsp_data) * (tsp->count));
  if (!tsp->ret_data)
    return -1;

  if (-1 == asprintf (&(tsp->ifile), ".tmp_file_%p", tsp))
    return -1;

  tsp->f = fopen (tsp->ifile, "w");
  if (!tsp->f)
    return -1;

  DPRINT ("TSP preparing input file\n");
  tsp_solve_prepare_data (tsp);
  asprintf (&command, "linkern -o %s.out %s", tsp->ifile, tsp->ifile);

  if (system (command))
    {
      free (command);
      return -1;
    }
  free (command);
  DPRINT ("TSP child run ok\n");

  asprintf (&command, "%s.out", tsp->ifile);
  tmp_file = fopen (command, "r");
  free (command);
  if (!tmp_file)
    return -1;

  DPRINT ("TSP: Reading TSP output\n");
  if (-1 == getline (&line, &count, tmp_file))
    {
      DPRINT ("TSP: unable to read TSP solver output file\n");
      goto tsp_solve_fail;
    }
  DPRINT ("TSP: %s", line);
  if (tsp->count != strtol (line, NULL, 10))
    {
      DPRINT ("TSP: number of nodes not match (wanted %d, solver output %s\n",
	      tsp->count, line);
      goto tsp_solve_fail;
    }
  DPRINT ("TSP: number of nodes is ok\n");

  long node_num;
  while (-1 != getline (&line, &count, tmp_file))
    {
      node_num = strtol (line, NULL, 10);

      DPRINT ("TSP: node: %ld %f %f\n", node_num, tsp->data[node_num].x,
	      tsp->data[node_num].y);
      tsp->ret_data[tsp->ret_index].x = tsp->data[node_num].x;
      tsp->ret_data[tsp->ret_index].y = tsp->data[node_num].y;
      tsp->ret_data[tsp->ret_index].private_data =
	tsp->data[node_num].private_data;

      tsp->ret_index = tsp->ret_index + 1;
    }

  if (tsp->ret_index == tsp->count && tsp->ok == 0)
    {
      DPRINT ("TSP: OK\n");
      tsp->ok = 1;
      fclose (tmp_file);
      free (line);
      tsp->ret_index = 0;
      return 0;
    }

tsp_solve_fail:
  DPRINT ("TSP: FAIL\n");
  fclose (tmp_file);
  free (line);
  return -1;
}

//if no tsp solver is available use heuristic nearest neighbour (NN) algorithm 
static void
nn (struct tsp *tsp)
{

  struct tsp_data *tr, tmp_t;
  int i, j, flag;
  double min, len;

  if (!tsp)
    return;

  //if ret_data is unallocated, use input data to return .. 
  if (!tsp->ret_data)
    {
      tsp->ret_data = tsp->data;
      return;
    }
  memcpy (tsp->ret_data, tsp->data, sizeof (struct tsp_data) * tsp->count);
  if (tsp->count < 4)
    return;

  tr = tsp->ret_data;
  for (i = 0; i < tsp->count - 2; i++)
    {
      min =
	sqrt ((tr[i].x - tr[i + 1].x) * (tr[i].x - tr[i + 1].x) +
	      (tr[i].y - tr[i + 1].y) * (tr[i].y - tr[i + 1].y));

      for (j = i + 1; j < tsp->count; j++)
	{
	  len =
	    sqrt ((tr[i].x - tr[j].x) * (tr[i].x - tr[j].x) +
		  (tr[i].y - tr[j].y) * (tr[i].y - tr[j].y));

	  if (len < min)
	    {
	      min = len;
	      memcpy (&tmp_t, &tr[i + 1], sizeof (struct tsp_data));
	      memcpy (&tr[i + 1], &tr[j], sizeof (struct tsp_data));
	      memcpy (&tr[j], &tmp_t, sizeof (struct tsp_data));
	    }
	}
    }
  for (len = j = 0; j < tsp->count; j++)
    {
      len += sqrt ((tr[j].x - tr[j + 1].x) * (tr[j].x - tr[j + 1].x) +
		   (tr[j].y - tr[j + 1].y) * (tr[j].y - tr[j + 1].y));
    }

  DPRINT ("TSP len %f\n", len);
  min = len;
  for (flag = 1; flag;)
    {
      flag = 0;
      for (i = 0; i < tsp->count - 1; i++)
	{
	  memcpy (&tmp_t, &tr[i + 1], sizeof (struct tsp_data));
	  memcpy (&tr[i + 1], &tr[i], sizeof (struct tsp_data));
	  memcpy (&tr[i], &tmp_t, sizeof (struct tsp_data));

	  for (len = j = 0; j < tsp->count - 1; j++)
	    {
	      len += sqrt ((tr[j].x - tr[j + 1].x) * (tr[j].x - tr[j + 1].x) +
			   (tr[j].y - tr[j + 1].y) * (tr[j].y - tr[j + 1].y));
	    }
	  if (len >= min)
	    {
	      DPRINT ("TSP new len %f, min =%f\n", len, min);
	      memcpy (&tmp_t, &tr[i + 1], sizeof (struct tsp_data));
	      memcpy (&tr[i + 1], &tr[i], sizeof (struct tsp_data));
	      memcpy (&tr[i], &tmp_t, sizeof (struct tsp_data));
	      continue;
	    }
	  else
	    {
	      min = len;
	      flag = 1;
	    }
	}
    }

}

int
tsp_getnext (struct tsp *tsp, double *x, double *y, void **data)
{

  if (!tsp)
    return -1;

  if (tsp->ok != 1)
    {
      DPRINT
	("TSP: warning, external solver fail, using nearest neighbour algorithm\n");
      nn (tsp);
      tsp->ok = 1;
      tsp->ret_index = 0;
    }

  if (x != NULL)
    *x = tsp->ret_data[tsp->ret_index].x;
  if (y != NULL)
    *y = tsp->ret_data[tsp->ret_index].y;
  if (data != NULL)
    *data = tsp->ret_data[tsp->ret_index].private_data;
  tsp->ret_index++;
  if (tsp->ret_index >= tsp->count)
    tsp->ret_index = 0;

  return tsp->ret_index;;
}

int
tsp_close (struct tsp *tsp)
{
  char *name;

  if (!tsp)
    return -1;
  if (tsp->data)
    free (tsp->data);
  if (tsp->ret_data)
    free (tsp->ret_data);
  if (tsp->line)
    free (tsp->line);
  if (tsp->ifile)
    {
      if (-1 != asprintf (&name, "%s.out", tsp->ifile))
	{
	  unlink (name);
	  free (name);
	}
      unlink (tsp->ifile);
      free (tsp->ifile);
    }
  free (tsp);
  return 0;
}
