#!/usr/bin/perl -w

use strict;

use FRF;

use Getopt::Long;

my $content_file;
my $p13n_file;
my $output_file;

GetOptions(
  "content=s" => \$content_file,
  "p13n=s"    => \$p13n_file,
  "output=s"  => \$output_file,
);

$output_file or die "Please Enter an output_file";

open my $p13n_ifd, $p13n_file or die "Terribly: $!";

my $content = do $content_file;

FRF::create($content, $p13n_ifd, $output_file);

exit 0;
