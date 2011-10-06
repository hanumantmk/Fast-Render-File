#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <frf.h>

typedef frf_t * FRF;

MODULE = FRF		PACKAGE = FRF	PREFIX = frf

FRF
frf_new(file_name)
    char * file_name

MODULE = FRF		PACKAGE = FRF		PREFIX = frf_

int
frf_write(frf, fd)
    FRF frf;
    int fd;

int
frf_next(frf)
    FRF frf;

double
frf_get_offset(frf)
    FRF frf;

int frf_seek(frf, offset)
    FRF frf;
    double offset;
  INIT:
    uint32_t uoffset = (uint32_t)offset;
  CODE:
    RETVAL = frf_seek(frf, uoffset);

void
frf_DESTROY(frf)
    FRF frf
  CODE:
    frf_destroy(frf);
