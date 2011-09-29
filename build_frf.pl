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

my $content = do $content_file;

my $offsets = '';
my $string_table = '';
my $vector_header = '';
my $vector = '';

my %strings;
my $num_uniq_strings = 0;
my $written = 0;

run();

my $string_table_size  = length($string_table);
my $vector_header_size = length($vector_header);

$offsets = pack("NNN", 12, 12 + $string_table_size, 12 + $string_table_size + $vector_header_size);

print $offsets, $string_table, $vector_header, $vector;

exit 0;

sub run {
  my $cc = compile_content($content);

  my $num_lines = 0;

  while (my $line = <$p13n_ifd>) {
    chomp $line;

    my @p13n = split("\t", $line);

    my @vector;

    my @todo = @$cc;

    while (my $item = shift @todo) {
      if ($item->[0] eq 'static') {
	push @vector, $item->[1];
      } elsif ($item->[0] eq 'p13n') {
	my $string = $p13n[$item->[1]];
	push @vector, add_to_string_table($string) if $string ne '';
      } elsif ($item->[0] eq 'dc') {
	my $dc = $item->[1];

	my $string = $p13n[$dc->{key}];

	my $new_work;
	if (exists $dc->{alternatives}{$string}) {
	  $new_work = $dc->{alternatives}{$string};
	} else {
	  $new_work = $dc->{default};
	}

	unshift @todo, @$new_work;
      } else {
	die "unknown content type";
      }
    }

    $vector .= pack("nn*", scalar(@vector), @vector);

    $num_lines++;
  }

  $vector = pack("N", $num_lines) . $vector;
}

sub compile_content {
  my ($c) = @_;

  my @cc;

  my $i = 0;

  my %p13n_lookup = map { $_, $i++ } @{$c->{p13n}};

  my $dc = $c->{dc};
  
  my %cc_for_dc;

  foreach my $key (keys %$dc) {
    my $d = $dc->{$key};
    if (exists $p13n_lookup{$d->{key}}) {
      $cc_for_dc{$key} = {
	key => $p13n_lookup{$d->{key}},
	alternatives => {
	  map {
	    $_, compile_content({
	      p13n => $c->{p13n},
	      body => $d->{alternatives}{$_},
	    }),
	  } keys %{$d->{alternatives}}
	},
	default => compile_content({
	  p13n => $c->{p13n},
	  body => $d->{default},
	}),
      };
    }
  }

  my $content = $c->{body};

  while ($content =~ s/(.*?)%%\s*([^% ]+)\s*%%//s) {
    my ($before, $tag) = ($1, $2);

    if ($before ne '') {
      push @cc, ['static' => add_to_string_table($before)];
    }
    
    if (exists $p13n_lookup{$tag}) {
      push @cc, ['p13n' => $p13n_lookup{$tag}];
    }

    if (exists $cc_for_dc{$tag}) {
      push @cc, ['dc' => $cc_for_dc{$tag}];
    }
  }

  if ($content ne '') {
    push @cc, ['static' => add_to_string_table($content)];
  }

  return \@cc;
}

sub add_to_string_table {
  my ($string) = @_;

  if (! defined $string) {
    die "Can't have undefined string";
  } elsif ($string eq '') {
    die "Don't add empty strings";
  } elsif (exists $strings{$string}) {
    return $strings{$string};
  } else {
    $strings{$string} = $num_uniq_strings;
    $string_table .= $string;

    $vector_header .= pack("NN", $written, length($string));
    $written += length($string);

    return $num_uniq_strings++;
  }
}
