#!/usr/bin/perl -w

use strict;

use GenData qw( gen_data );

gen_data(\*STDOUT, $ARGV[0],
  [number => 10],
  [email => 10],
  'state',
  'city',
  [string => 5],
  [string => 5],
  [enum => ['m','f','o']],
)
