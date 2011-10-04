#!/usr/bin/perl -w

use strict;
use warnings;

use bytes;

use List::Util qw( sum );

use File::Temp;

use Getopt::Long;

use constant SMALL_HEADER  => 1 << 31;
use constant MEDIUM_HEADER => 1 << 30;
use constant DYNAMIC       => 3 << 30;

use constant CACHE_SIZE => 100_000;

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

my @fhs = map { File::Temp->new(
  TEMPLATE => "$output_file.XXXXX",
) } 1..4;

my ($header, $string_table, $unique_cells, $vector) = @fhs;

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

system("cat " . join(" ", map { $_->filename } (@fhs) ) . " > $output_file");

exit 0;

sub run {
  my $cc = FRF::LazyContent->new($content)->to_cells;

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


my %strings;
my $added_string = CACHE_SIZE;
use constant STRING_TABLE_DISPATCH => [
  [ SMALL_HEADER,  "C", 1 ],
  [ MEDIUM_HEADER, "n", 2 ],
  [ 0,             "N", 4 ],
];

sub add_to_string_table {
  my $string = shift;

  if(! (exists $strings{$string})) {
    if (CACHE_SIZE != -1 && ! $added_string--) {
      $added_string = CACHE_SIZE;
      %strings = ();
    }

    my $length = length($string);

    my $dispatch;

    if ($length < 2 ** 8) {
      $dispatch = STRING_TABLE_DISPATCH->[0];
    } elsif ($length < 2 ** 16) {
      $dispatch = STRING_TABLE_DISPATCH->[1];
    } else {
      $dispatch = STRING_TABLE_DISPATCH->[2];
    }

    $strings{$string} = $string_table_written | $dispatch->[0];
    print $string_table pack($dispatch->[1], $length), $string;
    $string_table_written += $length + $dispatch->[2];
  }

  return $strings{$string};
}

package FRF::LazyContent;

sub new {
  my ($class, $c) = @_;

  return bless {
    c => $c,
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
	      }),
	    } keys %{$d->{alternatives}}
	  },
	  default => (ref $self)->new({
	    body    => $d->{default},
	    context => $context,
	  }),
	};
      }
    }
  }

  my $content = $c->{body};

  while ($content =~ s/(.*?)%%\s*([^% ]+)\s*%%//s) {
    my ($before, $tag) = ($1, $2);

    if ($before ne '') {
      push @cc, ['static' => main::add_to_string_table($before)];
    }
    
    if (exists $context->{p13n_lookup}{$tag}) {
      push @cc, ['p13n' => $context->{p13n_lookup}{$tag}];
    }

    if (exists $context->{cc_for_dc}{$tag}) {
      push @cc, ['dc' => $context->{cc_for_dc}{$tag}];
    }
  }

  if ($content ne '') {
    push @cc, ['static' => main::add_to_string_table($content)];
  }

  $self->{cells} = \@cc;

  return \@cc;
}
