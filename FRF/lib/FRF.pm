package FRF;

use 5.010001;
use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('FRF', $VERSION);

sub new {
  return bless FRF::_new($_[1]);
}

1;
