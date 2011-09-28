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

open my $offsets, ">$output_file.1" or die "terribly: $!";
open my $string_table_ofd, ">$output_file.2" or die "terribly: $!";
open my $vector_header_ofd, ">$output_file.3" or die "terribly: $!";
open my $vector_ofd, ">$output_file.4" or die "terribly: $!";

run($content, $p13n_ifd, $string_table_ofd, $vector_header_ofd, $vector_ofd);

close $string_table_ofd or die "terribly: $!";
close $vector_ofd or die "terribly: $!";
close $vector_header_ofd or die "terribly: $!";
close $p13n_ifd or die "terribly: $!";

my $string_table_size  = [stat("$output_file.2")]->[7];
my $vector_header_size = [stat("$output_file.3")]->[7];

print $offsets pack("NNN", 12, 12 + $string_table_size, 12 + $string_table_size + $vector_header_size);

close $offsets or die "terribly: $!";

system("cat " . join(" ", map { join('.', $output_file, $_) } 1..4) . " > $output_file.frf");
unlink(map { join('.', $output_file, $_) } 1..4);

exit 0;

sub run {
  my ($content, $p13n_ifd, $string_table_ofd, $vector_header_ofd, $vector_ofd) = @_;

  my $cc = compile_content($content);

  my %strings;

  my $written = 0;
  
  my $num_lines = 0;

  while (my $line = <$p13n_ifd>) {
    chomp $line;

    my @p13n = split("\t", $line);

    foreach my $item (@$cc) {
      my $string = ref $item
	? $p13n[$$item - 1]
	: $item;

      if ($strings{$string}) {
	$strings{$string}{freq}++;
      } else {
	$strings{$string} = {
	  offset => $written,
	  length => length($string),
	  freq   => 1,
	};

	print $string_table_ofd $string;
	$written += length $string;
      }
    }

    $num_lines++;
  }

  my $i = 0;
  my @string_table = map {
    $_->{index} = $i++;
    ($_->{offset}, $_->{length});
  } sort {$b->{freq} <=> $a->{freq}} values %strings;

  print $vector_header_ofd pack("N*", @string_table);
  print $vector_ofd pack("N", $num_lines);

  seek($p13n_ifd, 0, 0);

  while (my $line = <$p13n_ifd>) {
    chomp $line;

    my @p13n = split("\t", $line);

    my @vector;

    foreach my $item (@$cc) {
      my $string = ref $item
	? $p13n[$$item - 1]
	: $item;

      push @vector, $strings{$string}{index};
    }

    print $vector_ofd pack("nn*", scalar(@vector), @vector);
  }
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
