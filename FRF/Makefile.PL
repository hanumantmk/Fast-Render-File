use 5.010001;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'FRF',
    VERSION_FROM      => 'lib/FRF.pm', # finds $VERSION
    LIBS              => '-L.. -lfrf',
    INC               => '-I..',
);
