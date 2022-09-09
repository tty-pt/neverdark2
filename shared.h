#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>

#define warn(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

#define TEXT_LEN 128

enum pkg_type {
	PT_TEXT,
};

union pkg_specific {
	char text[TEXT_LEN];
};

struct pkg {
	unsigned type;
	union pkg_specific sp;
};


#endif
