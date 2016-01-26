/*
    postprocesor.c

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

    Postprocesor wrapper functions

*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dlfcn.h>
#include <string.h>
#include <stdarg.h>
#include "post.h"

struct postprocesor *first;

//typedef int entrypoint (const char *argument);
typedef struct postprocesor *entrypoint (void);

#ifndef POST_PATH
#define POST_PATH "./", "/usr/lib/pcb2g/", 0
#endif

static void
postprocesor_init0 (char *modulepath)
{

  void *module = NULL;
  entrypoint *run = NULL;
  struct postprocesor *post;
  char *error;
  const char *m_path[] = { POST_PATH };
  int i;
  char *mp;

  for (i = 0; m_path[i] != 0; i++)
    {
      mp = NULL;
      asprintf (&mp, "%s%s", m_path[i], modulepath);
      printf ("opening %s\n", mp);
      module = dlopen (mp, RTLD_NOW);
      free (mp);
      if (module)
	break;
    }
  if (!module)
    {
      printf ("Unable to open postprocesor %s\n", modulepath);
      return;
    }
  dlerror ();			// clear error 

  run = dlsym (module, "init");
  if ((error = dlerror ()) != NULL)
    {
      printf ("Unable to init postprocesor %s,%s \n", modulepath, error);
      dlclose (module);
      return;
    }

  post = (*run) ();

  if (post == NULL)
    {
      dlclose (module);
      return;
    }
  post->module = module;
  post->next = first;
  first = post;
}

void
postprocesor_init ()
{

  postprocesor_init0 ("post_linuxcnc.so");
  postprocesor_init0 ("post_svg.so");
}

struct postprocesor *
postprocesor_register (struct post_operations *p, void *data, char *name)
{
  struct postprocesor *tmp;

  if (!name)
    return NULL;

  if (!p)
    {
      printf ("Postprocesor %s does not set any opeations\n", name);
      return NULL;
    }
  tmp = calloc (sizeof (struct postprocesor), 1);
  tmp->name = name;
  printf ("Registering postprocesor: %s\n", name);
  tmp->ops = p;
  tmp->data = data;
  return tmp;
}

void
postprocesor_close ()
{
  struct postprocesor *p, *fr;

  for (p = first; p != NULL;)
    {
      if ((p->ops->close))
	(p->ops->close) (p);
      dlclose (p->module);
      fr = p;
      p = p->next;
      free (fr);
    }
}

void
postprocesor_write_comment (char *comment)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->write_comment))
      (p->ops->write_comment) (p, comment);
}

void
postprocesor_open (char *filename)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->open))
      (p->ops->open) (p, filename);
}

void
postprocesor_set (enum POST_SET op, ...)
{
  struct postprocesor *p;
  va_list ap;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->set))
      {
	va_start (ap, op);
	(p->ops->set) (p, op, ap);
	va_end (ap);
      }
}

void
postprocesor_route (double x, double y)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->route))
      (p->ops->route) (p, x, y);
}

void
postprocesor_rapid (double x, double y)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->rapid))
      (p->ops->rapid) (p, x, y);
}

void
postprocesor_route_arc (double x, double y, double cx, double cy, int dir)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->route_arc))
      (p->ops->route_arc) (p, x, y, cx, cy, dir);
}

void
postprocesor_hole (double x, double y, double dia)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if ((p->ops->hole))
      (p->ops->hole) (p, x, y, dia);
}

void
postprocesor_operation (enum POST_MACHINE_OPS op)
{
  struct postprocesor *p;

  for (p = first; p != NULL; p = p->next)
    if (p->ops->operation)
      (p->ops->operation) (p, op);
}
