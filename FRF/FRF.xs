#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <frf.h>
#include <frf_maker.h>

typedef frf_t * FRF;
typedef frf_maker_t * FRF_Maker;

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
  OUTPUT:
    RETVAL

void
frf_DESTROY(frf)
    FRF frf
  CODE:
    frf_destroy(frf);

MODULE = FRF		PACKAGE = FRF::Maker	PREFIX = frf_maker

FRF_Maker
frf_maker_new(content_file_name, output_file_name)
    char * content_file_name
    char * output_file_name

MODULE = FRF		PACKAGE = FRF::Maker		PREFIX = frf_maker_

int
frf_maker_add(frf_maker, p13n, lengths)
    FRF_Maker frf_maker;
    SV * p13n;
    SV * lengths;
  INIT:
    char ** p;
    int i;

    AV * av_p = (AV *)SvRV(p13n);

    p = calloc(sizeof(char *), av_len(av_p) + 1);

    for (i = 0; i < av_len(av_p) + 1; i++) {
      p[i] = SvPV_nolen(*(av_fetch(av_p, i, 0)));
    }
  CODE:
    RETVAL = frf_maker_add(frf_maker, p);
    free(p);
  OUTPUT:
    RETVAL

int
frf_maker_finish(frf_maker);
    FRF_Maker frf_maker;

void
frf_maker_DESTROY(frf_maker)
    FRF_Maker frf_maker;
  CODE:
    frf_maker_destroy(frf_maker);
