#!/usr/bin/perl -w

use strict;
use warnings;

use bytes;

use List::Util qw( sum );
use Scalar::Util qw( refaddr );

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
my $vector_header = pack("NN", 0, 0);
my $unique_cells = '';
my $vector = '';

my %strings;
my %unique_cell_sets;
my $num_uniq_strings = 1; #start at 1 to save room for the special case of non-static content in unique cell lookups
my $unique_cells_written = 0;
my $written = 0;

run();

my $unique_cells_size  = length($unique_cells);
my $string_table_size  = length($string_table);
my $vector_header_size = length($vector_header);

$offsets = pack("NNNN",
  16,
  16 + $string_table_size,
  16 + $string_table_size + $vector_header_size,
  16 + $string_table_size + $vector_header_size + $unique_cells_size,
);

print $offsets, $string_table, $vector_header, $unique_cells, $vector;

exit 0;

sub run {
  my $cc = FRF::LazyContent->new($content, \&add_to_string_table)->to_cells;

  my $num_lines = 0;

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

    my $cells_id = join('.', map { refaddr($_) } @cells);

    if (! (exists $unique_cell_sets{$cells_id})) {
      $unique_cell_sets{$cells_id} = $unique_cells_written;

      my $to_write = pack("N*", scalar(@cells), scalar(@vector), map { $_->[0] eq 'static' ? $_->[1] : 0 } @cells);
      $unique_cells_written += length($to_write);
      $unique_cells .= $to_write;
    }

    $vector .= pack("N*", $unique_cell_sets{$cells_id}, @vector);

    $num_lines++;
  }

  $vector = pack("N", $num_lines) . $vector;
}


sub add_to_string_table {
  my ($string) = @_;

  if (! defined $string) {
    die "Can't have undefined string";
#  } elsif ($string eq '') {
#    die "Don't add empty strings";
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
