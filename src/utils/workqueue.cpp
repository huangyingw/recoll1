/* Copyright (C) 2014 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
// Test program for the workqueue module

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "safeunistd.h"

#include "workqueue.h"

static char *thisprog;

static char usage [] =
"  \n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s	  0x2 
#define OPT_b	  0x4 

class Task {
public:
    Task()
	: m_id(o_id++)
    {}
    int m_id;
    static int o_id;
};
int Task::o_id;

void *worker(void *vtp)
{
    fprintf(stderr, "Worker working\n");
    WorkQueue<Task> *tqp = (WorkQueue<Task> *)vtp;
    Task tsk;
    for (;;) {
	if (!tqp->take(&tsk)) {
	    fprintf(stderr, "Worker: take failed\n");
	    return (void*)0;
	}
	fprintf(stderr, "WORKER: got task %d\n", tsk.m_id);
	if (tsk.m_id > 20) {
	    tqp->workerExit();
	    break;
	}
    }
    return (void*)1;
}

int main(int argc, char **argv)
{
  int count = 10;
    
  thisprog = argv[0];
  argc--; argv++;

  while (argc > 0 && **argv == '-') {
    (*argv)++;
    if (!(**argv))
      /* Cas du "adb - core" */
      Usage();
    while (**argv)
      switch (*(*argv)++) {
      case 's':	op_flags |= OPT_s; break;
      case 'b':	op_flags |= OPT_b; if (argc < 2)  Usage();
	if ((sscanf(*(++argv), "%d", &count)) != 1) 
	  Usage(); 
	argc--; 
	goto b1;
      default: Usage();	break;
      }
  b1: argc--; argv++;
  }

  if (argc != 0)
    Usage();

  WorkQueue<Task> wq("testwq", 10);

  if (!wq.start(2, &worker, &wq)) {
      fprintf(stderr, "Start failed\n");
      exit(1);
  }

  for (;;) {
      Task tsk;
      fprintf(stderr, "BOSS: put task %d\n", tsk.m_id);
      if (!wq.put(tsk)) {
	  fprintf(stderr, "Boss: put failed\n");
	  exit(1);
      }
      if ((tsk.m_id % 10) == 0)
	  sleep(1);
  }
  exit(0);
}

