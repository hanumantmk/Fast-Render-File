#!/usr/bin/perl -w

use strict;
use warnings;

use bytes;

use List::Util qw( sum );

use Getopt::Long;

use constant SMALL_HEADER  => 1 << 31;
use constant MEDIUM_HEADER => 1 << 30;
use constant DYNAMIC       => 3 << 30;

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

open my $header, "> $output_file.1" or die "Couldn't open output_file: $!";
open my $string_table, "> $output_file.2" or die "Couldn't open string_table: $!";
open my $unique_cells, "> $output_file.3" or die "Couldn't open unique_cells: $!";
open my $vector, "> $output_file.4" or die "Couldn't open vector: $!";

my %strings;
my %unique_cell_sets;
my $unique_cells_written = 0;
my $string_table_written = 0;
my $num_lines = 0;

run();

print $header pack("NNNN",
  $num_lines,
  16,
  16 + $string_table_written,
  16 + $string_table_written + $unique_cells_written,
);

system("cat " . join(" ", map { "$output_file.$_" } (1..4) ) . " > $output_file");

unlink map { "$output_file.$_" } 1..4;

exit 0;

sub run {
  my $cc = FRF::LazyContent->new($content, \&add_to_string_table)->to_cells;

  while (my $line = <$p13n_ifd>) {
    chomp $line;

    my @p13n = split("\t", $line);

    my @cells;

    my @vector;

    my @todo = @$cc;

    while (my $item = shift @todo) {
      if ($item->[0] eq 'static') {
	push @cells, $item;
      } elsif ($item->[0] eq 'p13n') {
	push @cells, $item;
	push @vector, add_to_string_table($p13n[$item->[1]]);
      } elsif ($item->[0] eq 'dc') {
	my $dc = $item->[1];

	my $string = $p13n[$dc->{key}];

	my $new_work;
	if (exists $dc->{alternatives}{$string}) {
	  $new_work = $dc->{alternatives}{$string};
	} else {
	  $new_work = $dc->{default};
	}

	unshift @todo, @{$new_work->to_cells};
      } else {
	die "unknown content type";
      }
    }

    my @data = map { $_->[0] eq 'static' ? $_->[1] : DYNAMIC } @cells;

    my $cells_id = join('.', @data);

    if (! (exists $unique_cell_sets{$cells_id})) {
      $unique_cell_sets{$cells_id} = $unique_cells_written;

      my $to_write = pack("N*", scalar(@cells), scalar(@vector), @data);
      $unique_cells_written += length($to_write);
      print $unique_cells $to_write;
    }

    print $vector pack("N*", $unique_cell_sets{$cells_id}, @vector);

    $num_lines++;
  }
}


my $added_string = 100_000;
sub add_to_string_table {
#  my $string = shift;

  if(! (exists $strings{$_[0]})) {
    unless ($added_string--) {
      $added_string = 100_000;
      %strings = ();
    }

    my $length = length($_[0]);

    if ($length >> 8) {
      $strings{$_[0]} = $string_table_written | SMALL_HEADER;
      print $string_table pack("C/A*", $length, $_[0]);
      $string_table_written += $length + 1;
    } elsif ($length >> 16) {
      $strings{$_[0]} = $string_table_written | MEDIUM_HEADER;
      print $string_table pack("n/A*", $length, $_[0]);
      $string_table_written += $length + 2;
    } else {
      $strings{$_[0]} = $string_table_written;
      print $string_table pack("N/A*", $length, $_[0]);
      $string_table_written += $length + 4;
    }

  }

  return $strings{$_[0]};
}

package FRF::LazyContent;

sub new {
  my ($class, $c, $add_to_string_table) = @_;

  return bless {
    c                   => $c,
    add_to_string_table => $add_to_string_table,
  }, $class;
}

sub to_cells {
  my $self = shift;

  return $self->{cells} if $self->{cells};

  my $c = $self->{c};

  my @cc;

  my $context;
  
  if ($c->{context}) {
    $context = $c->{context};
  } else {
    $context = {
      p13n_lookup => {},
      cc_for_dc   => {},
    };

    my $i = 0;
    $context->{p13n_lookup} = {map { $_, $i++ } @{$c->{p13n}}};

    my $dc = $c->{dc};
    
    foreach my $key (keys %$dc) {
      my $d = $dc->{$key};
      if (exists $context->{p13n_lookup}{$d->{key}}) {
	$context->{cc_for_dc}{$key} = {
	  key => $context->{p13n_lookup}{$d->{key}},
	  alternatives => {
	    map {
	      $_, (ref $self)->new({
		body    => $d->{alternatives}{$_},
		context => $context,
	      }, $self->{add_to_string_table}),
	    } keys %{$d->{alternatives}}
	  },
	  default => (ref $self)->new({
	    body    => $d->{default},
	    context => $context,
	  }, $self->{add_to_string_table}),
	};
      }
    }
  }

  my $content = $c->{body};

  while ($content =~ s/(.*?)%%\s*([^% ]+)\s*%%//s) {
    my ($before, $tag) = ($1, $2);

    if ($before ne '') {
      push @cc, ['static' => $self->{add_to_string_table}->($before)];
    }
    
    if (exists $context->{p13n_lookup}{$tag}) {
      push @cc, ['p13n' => $context->{p13n_lookup}{$tag}];
    }

    if (exists $context->{cc_for_dc}{$tag}) {
      push @cc, ['dc' => $context->{cc_for_dc}{$tag}];
    }
  }

  if ($content ne '') {
    push @cc, ['static' => $self->{add_to_string_table}->($content)];
  }

  $self->{cells} = \@cc;

  return \@cc;
}
