#!/usr/bin/perl -w

use strict;
use warnings;

use bytes;

use List::Util qw( sum );

use Getopt::Long;

my $content_file;
my $p13n_file;
my $output_file;

GetOptions(
  "content=s" => \$content_file,
  "p13n=s"    => \$p13n_file,
  "output=s"  => \$output_file,
);

open my $p13n_ifd, $p13n_file or die "Terribly: $!";

my $content = do {
  open FILE, $content_file or die "Terribly: $!";

  local $/;

  my $b = <FILE>;

  close FILE or die "Terribly: $!";

  $b;
};

my $offsets;
my $string_table;
my $vector_header;
my $vector;

run($content, $p13n_ifd, \$string_table, \$vector_header, \$vector);

my $string_table_size  = length($string_table);
my $vector_header_size = length($vector_header);

$offsets = pack("NNN", 12, 12 + $string_table_size, 12 + $string_table_size + $vector_header_size);

print $offsets, $string_table, $vector_header, $vector;

exit 0;

sub run {
  my ($content, $p13n_ifd, $string_table, $vector_header, $vector) = @_;

  my $cc = compile_content($content);

  my %strings;

  my $written = 0;
  
  my $num_lines = 0;
  my @string_table;
  my $num_uniq_strings = 0;

  while (my $line = <$p13n_ifd>) {
    chomp $line;

    my @p13n = split("\t", $line);

    my @vector;

    foreach my $item (@$cc) {
      my $string = ref $item
	? $p13n[$$item - 1]
	: $item;

      if (! $strings{$string}) {
	$strings{$string} = $num_uniq_strings++;

	push @string_table, $written, length($string);

	$$string_table .= $string;
	$written += length $string;
      }

      push @vector, $strings{$string};

    }
    $$vector .= pack("nn*", scalar(@vector), @vector);

    $num_lines++;
  }

  my $i = 0;

  $$vector_header = pack("N*", @string_table);
  $$vector = pack("N", $num_lines) . $$vector;
}

sub compile_content {
  my $content = shift;

  my @cc;

  while ($content =~ s/(.*?)%%\s*([^% ]+)\s*%%//s) {
    my ($before, $tag) = ($1, $2);

    push @cc, $before, \$tag;
  }

  push @cc, $content;

  return \@cc;
}
