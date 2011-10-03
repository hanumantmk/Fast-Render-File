package GenData;

use strict;
use warnings;

use Data::Dumper;
use DateTime;

use base 'Exporter';

our @EXPORT_OK = qw( gen_data );


use constant DATA_LEN => 10;

use constant STRING_ALPHA => [qw( a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9)];

use constant STRING_ALPHA_LEN => scalar(@{+STRING_ALPHA});

sub _make_date {
  my $len = shift;
  
  my $d = DateTime->now;
  join ' ', $d->dmy("/"), $d->hms;
}

sub _make_string {
  my $len = shift;

  join('', map { STRING_ALPHA->[int(rand(STRING_ALPHA_LEN))] } ( 1..$len ));
}

sub _make_num {
  my $len = shift;

  join('', map { int(rand(10)) } ( 1..$len));
}

use constant FUNCTIONS => {
  date => sub {
	
	&_make_date;
  },
  string => sub {
	 my $len = shift;
	 
    _make_string($len);
  },
  
  number => sub {
     my ($len,$precision,$scale) = @_;
     if ( defined $precision  ) {
      if ( defined $scale  ) {
      $len  = $precision - $scale;
      }
      else {
      $len  = $precision ;
      }
     }
      
    _make_num($len);
  },
  
  email => sub {
	my @ext = qw(com net org);
	my $max_len = shift;
	
	my $len = $max_len;
	
    my $r = join('@',
      _make_string($len/2),
      _make_string($len/2),
	  
    );
	
	join '.', $r,$ext[int(rand(@ext))];
	
	
  },
  enum => sub {
    my $enum = shift;

    my $enum_len = scalar(@$enum);

    $enum->[int(rand($enum_len))];
  },
  
  state => sub {
	my @states = qw( AL AK AS AZ AR CA CO CT DE DC FM FL GA GU HI ID IL IN IA KS KY LA ME MH MD MA MI MN MS MO MT NE NV NH NJ NM NY NC ND MP OH OK OR PW PA PR RI SC SD TN TX UT VT VI VA WA WV WI WY );
    $states[int(rand(scalar(@states)))];
  },
  
  city => sub {
	my @cities = qw(Montgomery Juneau Phoenix Little Rock Sacramento Denver Hartford Dover Tallahassee Atlanta Honolulu Boise Springfield Indianapolis Des Moines Topeka Frankfort Baton Rouge Augusta Annapolis Boston Lansing
	Saint Paul Jackson Jefferson City Helena Lincoln Carson City Concord Trenton Santa Fe Albany Raleigh Bismarck Columbus Oklahoma City Salem Harrisburg Providence Columbia Pierre Nashville Austin Salt Lake City Montpelier
    Richmond Olympia Charleston Madison Cheyenne);
	$cities[int(rand(scalar(@cities)))];
  },
};

sub gen_data {
  my ($fh, $rows, @params) = @_;

  #print Dumper @params;
  #exit;
  
  for (my $i = 0; $i < $rows; $i++) {
    print $fh join("\t",
    map {
      if (ref $_ eq 'ARRAY') {
	my @a = @$_;

        my $func = FUNCTIONS->{shift @a};
	
	if ( defined $func ) {
	  $func->(@a);
	}
	else {
	  print $_->[0], "\n";
	  print $_->[1], "\n";
	  join '!!!!!!!!!!!!!!!!!!!!!!!!!', @$_;	 
	}
      }
      else {
	    FUNCTIONS->{$_}->();
      }
    } @params) . "\n";
  }
}



1;
