/**
 * Copyright (c) 2011, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @author  Willem Thiart himself@willemthiart.com
 */

/*

dq - a command line tool for duraqueue the dead simple queue

Usage:
  dq info -f <queue_file>
  dq --help

Options:
  -f --file  Queue file
  -h --help  Prints a short usage summary.

*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include "duraqueue.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "usage.c"

int main(int argc, char **argv)
{
    int e;
    options_t opts;

    e = parse_options(argc, argv, &opts);
    if (-1 == e)
    {
        exit(-1);
    }

    void *qu = NULL;

    if (opts.help)
    {
        show_usage();
        exit(0);
    }
    else if (opts.info)
    {
        qu = dqueuer_open(opts.queue_file);
    }

    if (qu)
        printf("Item count: %d\n", dqueue_count(qu));

    return 1;
}
