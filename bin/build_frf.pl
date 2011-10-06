#!/usr/bin/perl -w

use strict;

use FRF::Maker;

use Getopt::Long;

my $content_file;
my $p13n_file;
my $output_file;
my $dump_on_finish;

GetOptions(
  "content=s" => \$content_file,
  "p13n=s"    => \$p13n_file,
  "output=s"  => \$output_file,
  "dump_on_finish" => \$dump_on_finish,
);

$output_file or die "Please Enter an output_file";

open my $p13n_ifd, $p13n_file or die "Terribly: $!";

my $content = do $content_file;

my $frf = FRF::Maker->new({
  content   => $content,
  file_name => $output_file,
});

while (my $line = <$p13n_ifd>) {
  chomp $line;
  $frf->add([split /\t/, $line]);
}

$frf->finish;

if ($dump_on_finish) {
  $frf = $frf->load;

  do {
    $frf->write(1);
  } while ($frf->next);
}

exit 0;
