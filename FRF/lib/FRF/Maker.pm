package FRF::Maker;

use 5.010001;
use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('FRF', $VERSION);

sub new {
  my $class = shift;
  return bless FRF::Maker::_new(@_);
}

1;
