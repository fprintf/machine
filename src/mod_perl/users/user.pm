package mod_perl::users::user;

sub new {
	my $class = shift;
	my %opt = @_;

	my $self = bless {
		%opt
	}, $class;

	return $self;
}

# Userid is readonly
sub userid {
	my $self = shift;
	return $self->{userid};
}

sub mask {
	my ($self, $new) = @_;
	if ($new) { $self->{mask} = $new; }
	return $self->{mask};
}

sub email {
	my ($self, $new) = @_;
	if ($new) { $self->{email} = $new; }
	return $self->{email};
}

sub phone {
	my ($self, $new) = @_;
	if ($new) { $self->{phone} = $new; }
	return $self->{phone};
}

sub password {
	my ($self, $new) = @_;
	if ($new) { $self->{password} = $new; }
	return $self->{password};
}

sub store {
	my $self = shift;

	my $dbh = DBI->connect(
		sprintf('dbi:SQLite:dbname=%s',$mod_perl::config::module_db),
		'', '',
		{ RaiseError => 1 }
	);

	my $sth = $dbh->prepare('replace into users (userid, password, mask, email, phone) values(?, ?, ?, ?, ?)');
	$sth->execute(
		$self->password, 
		$self->mask, 
		$self->email, 
		$self->phone,
	);
	$sth->finish();
}

1;
