#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>

#define warn(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

#endif
