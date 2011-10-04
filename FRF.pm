package FRF;

use strict;
use warnings;

use bytes;

use List::Util qw( sum );

use File::Temp;

use constant SMALL_HEADER  => 1 << 31;
use constant MEDIUM_HEADER => 1 << 30;
use constant DYNAMIC       => 3 << 30;

use constant CACHE_SIZE => 100_000;

use constant FRF_REGIONS => [qw( header string_table unique_cells vector )];

use constant STRING_TABLE_DISPATCH => [
  [ SMALL_HEADER,  "C", 1 ],
  [ MEDIUM_HEADER, "n", 2 ],
  [ 0,             "N", 4 ],
];


sub create {
  my ($content, $p13n_fh, $output_filename) = @_;
  
  my %fhs = map {
    $_, File::Temp->new(
      TEMPLATE => "$output_filename.XXXXX",
    )
  } @{+FRF_REGIONS};

  my $unique_cells_written = 0;
  my $string_table_written = 0;
  my $num_lines = 0;

  my $add_to_string_table = make_add_to_string_table($fhs{string_table}, \$string_table_written);
  my $add_to_uniq_cells   = make_add_uniq_cells($fhs{unique_cells}, \$unique_cells_written);

  my $cc = FRF::LazyContent->new($content, $add_to_string_table)->to_cells;

  while (my $line = <$p13n_fh>) {
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
	push @vector, $add_to_string_table->($p13n[$item->[1]]);
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

    my $offset = $add_to_uniq_cells->(\@cells, \@vector);

    $fhs{vector}->print(pack("N*", $offset, @vector));

    $num_lines++;
  }

  $fhs{header}->print(pack("NNNN",
    $num_lines,
    16,
    16 + $string_table_written,
    16 + $string_table_written + $unique_cells_written,
  ));

  system("cat " . join(" ", map { $fhs{$_}->filename } @{+FRF_REGIONS} ) . " > $output_filename");
}

sub make_add_to_string_table {
  my ($string_table, $string_table_written) = @_;

  my %strings;
  my $added_string = CACHE_SIZE;

  return sub {
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

      $strings{$string} = $$string_table_written | $dispatch->[0];
      print $string_table pack($dispatch->[1], $length), $string;
      $$string_table_written += $length + $dispatch->[2];
    }

    return $strings{$string};
  }
}

sub make_add_uniq_cells {
  my ($unique_cells, $unique_cells_written) = @_;

  my %unique_cell_sets;

  return sub {
    my ($cells, $vector) = @_;

    my @data = map { $_->[0] eq 'static' ? $_->[1] : DYNAMIC } @$cells;

    my $cells_id = join('.', @data);

    if (! (exists $unique_cell_sets{$cells_id})) {
      $unique_cell_sets{$cells_id} = $$unique_cells_written;

      my $to_write = pack("N*", scalar(@$cells), scalar(@$vector), @data);
      $$unique_cells_written += length($to_write);
      print $unique_cells $to_write;
    }

    return $unique_cell_sets{$cells_id};
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

  my $add_to_string_table = $self->{add_to_string_table};

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
	      }, $add_to_string_table),
	    } keys %{$d->{alternatives}}
	  },
	  default => (ref $self)->new({
	    body    => $d->{default},
	    context => $context,
	  }, $add_to_string_table),
	};
      }
    }
  }

  my $content = $c->{body};

  while ($content =~ s/(.*?)%%\s*([^% ]+)\s*%%//s) {
    my ($before, $tag) = ($1, $2);

    if ($before ne '') {
      push @cc, ['static' => $add_to_string_table->($before)];
    }
    
    if (exists $context->{p13n_lookup}{$tag}) {
      push @cc, ['p13n' => $context->{p13n_lookup}{$tag}];
    }

    if (exists $context->{cc_for_dc}{$tag}) {
      push @cc, ['dc' => $context->{cc_for_dc}{$tag}];
    }
  }

  if ($content ne '') {
    push @cc, ['static' => $add_to_string_table->($content)];
  }

  $self->{cells} = \@cc;

  return \@cc;
}

1;
