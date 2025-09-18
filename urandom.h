#ifndef _URANDOM_H
#define _URANDOM_H

/*****************************************************************************
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

/* random number generator functions for POSIX systems with /dev/urandom */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

static int urandom_fd;

static void urandom_init(void)
{
  urandom_fd = open("/dev/urandom", O_RDONLY);

  if (urandom_fd <= 0)
  {
    fprintf(stderr, "ERROR: urandom failed\n");
    exit(-1);
  }
}

static void urandom_get(uint8_t *data, int numbytes)
{
  while (numbytes)
  {
    int result = read(urandom_fd, data, numbytes);
    data += result; numbytes -= result;
  }
}

static void urandom_deinit(void)
{
  if (!urandom_fd) return;

  close(urandom_fd);
  urandom_fd = 0;
}

#endif

