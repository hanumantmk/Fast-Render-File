package FRF::Maker;

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

sub new {
  my ($class, $options) = @_;

  $options->{content} ||= '';
  $options->{file_name} ||= 'default.frf';

  my $self = bless {
    file_name => $options->{file_name},
    content   => $options->{content},
    fhs       => {map {
      $_, File::Temp->new(
	TEMPLATE => $options->{file_name} . ".XXXXX",
      )
    } @{+FRF_REGIONS}},
    unique_cells_written => 0,
    string_table_written => 0,
    num_lines => 0,

    strings => {},
    added_strings => 0,

    unique_cell_sets => {},
  };

  $self->{cc} = FRF::Maker::LazyContent->new($options->{content})->to_cells($self);

  return $self;
}

use constant ADD_TO_STRING_TABLE => <<'ADD_TO_STRING_TABLE'
do {
  my $strings = $self->{strings};

  if(! (exists $strings->{$string})) {
    if (CACHE_SIZE != -1 && $self->{added_strings}++ > CACHE_SIZE) {
      $self->{added_strings} = 0;
      %$strings = ();
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

    $strings->{$string} = $self->{string_table_written} | $dispatch->[0];
    $self->{fhs}{string_table}->print(pack($dispatch->[1], $length), $string);
    $self->{string_table_written} += $length + $dispatch->[2];
  }

  $self->{strings}{$string};
}
ADD_TO_STRING_TABLE
;

use constant ADD_TO_UNIQ_CELLS => <<'ADD_TO_UNIQ_CELLS'
do {
  my $unique_cell_sets = $self->{unique_cell_sets};

  my @data = map { $_->[0] eq 'static' ? $_->[1] : DYNAMIC } @$cells;

  my $cells_id = join('.', @data);

  if (! (exists $unique_cell_sets->{$cells_id})) {
    $unique_cell_sets->{$cells_id} = $self->{unique_cells_written};

    my $to_write = pack("N*", scalar(@$cells), scalar(@$vector), @data);
    $self->{unique_cells_written} += length($to_write);
    $self->{fhs}{unique_cells}->print($to_write);
  }

  $unique_cell_sets->{$cells_id};
}
ADD_TO_UNIQ_CELLS
;

BEGIN {
eval <<'ADD'
sub add {
  my ($self, $p13n) = @_;

  my @cells;
  my @vector;

  my @todo = @{$self->{cc}};
  while (my $item = shift @todo) {
    if ($item->[0] eq 'static') {
      push @cells, $item;
    } elsif ($item->[0] eq 'p13n') {
      push @cells, $item;
      my $string = $p13n->[$item->[1]];
      push @vector, 
ADD
. ADD_TO_STRING_TABLE . <<'ADD'
;
    } elsif ($item->[0] eq 'dc') {
      my $dc = $item->[1];

      my $string = $p13n->[$dc->{key}];

      my $new_work;
      if (exists $dc->{alternatives}{$string}) {
	$new_work = $dc->{alternatives}{$string};
      } else {
	$new_work = $dc->{default};
      }

      unshift @todo, @{$new_work->to_cells($self)};
    } else {
      die "unknown content type";
    }
  }

  my $cells = \@cells;
  my $vector = \@vector;

  $self->{fhs}{vector}->print(pack("N*",
ADD
. ADD_TO_UNIQ_CELLS . <<'ADD'
, @vector));

  $self->{num_lines}++;

  return;
}
ADD
; $@ and die $@;

eval <<'ADD'
sub add_to_string_table {
  my ($self, $string) = @_;

  return
ADD
. ADD_TO_STRING_TABLE . <<'ADD'
}
ADD
; $@ and die $@;

eval <<'ADD'
sub add_to_uniq_cells {
  my ($self, $cells, $vector) = @_;

  return
ADD
. ADD_TO_UNIQ_CELLS . <<'ADD'
}
ADD
; $@ and die $@;
};

sub finish {
  my $self = shift;

  $self->{fhs}{header}->print(pack("NNNN",
    $self->{num_lines},
    16,
    16 + $self->{string_table_written},
    16 + $self->{string_table_written} + $self->{unique_cells_written},
  ));

  system("cat " . join(" ", map { $self->{fhs}{$_}->filename } @{+FRF_REGIONS} ) . " > " . $self->{file_name});

  %$self = (
    file_name => $self->{file_name},
    done      => 1,
  );

  return;
}

sub load {
  my $self = shift;

  $self->{done} or die "Can't load an unfinished frf";

  require FRF;

  return FRF->load($self->{file_name});
}

package FRF::Maker::LazyContent;

sub new {
  my ($class, $c) = @_;

  return bless {
    c => $c,
  }, $class;
}

sub to_cells {
  my ($self, $maker) = @_;

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
      push @cc, ['static' => $maker->add_to_string_table($before)];
    }
    
    if (exists $context->{p13n_lookup}{$tag}) {
      push @cc, ['p13n' => $context->{p13n_lookup}{$tag}];
    }

    if (exists $context->{cc_for_dc}{$tag}) {
      push @cc, ['dc' => $context->{cc_for_dc}{$tag}];
    }
  }

  if ($content ne '') {
    push @cc, ['static' => $maker->add_to_string_table($content)];
  }

  $self->{cells} = \@cc;

  return \@cc;
}

1;
