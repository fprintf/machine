package mod_perl::modules::users;

use mod_perl::commands;
use mod_perl::config;
use strict;
use warnings;

use JSON::XS;

my $UsersDB = Users->get_instance(file => "$mod_perl::config::conf_dir/users.tct");
mod_perl::commands::register_command('users', \&main);
mod_perl::commands::register_command('user', \&main);

sub users_help {
	my $irc = shift;
    $irc->say("usage: users [--add|-a <name> [feeds]] [--set|-s <setting> <value>] [--del|-d <name>] [--list [search]]");
    $irc->say("-> Users database management tool");
    $irc->say("-> 'list' [search] list the users in the database, list users by given search term if given (default operation)");
    $irc->say("-> 'add' add a new user to the database, optionally giving the feeds for that user");
    $irc->say("-> 'del' delete a user from the database");
    $irc->say("-> 'set' updates a setting for a user. This setting operates on the user for the username running the command.");
    return;
}

# Return the users instance to any other modules that might want to use it
sub get_instance {
	return $UsersDB;
}

sub main {
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&users_help, $arg, qw(list|l add|a=s help|h del|d=s set|s=s));
    return if (!$ret);

	if ($opt{list}) {
		my $search = $argv[0];
		$irc->say("Users: ".join(", ", $UsersDB->list($search)));
		return;
	}

	if ($opt{add}) { 
		if (!$UsersDB->add(username => $opt{add})) {
			$irc->say("Error adding user $opt{add}");
		}
		return;
	}

	if ($opt{del}) { 
		if (!$UsersDB->del(username => $opt{del})) {
			$irc->say("Error deleting user $opt{del}");
		}
		return;
	}

	if ($opt{set} && @argv) { 
		my $username = $irc->nick();
		my $user = $UsersDB->update(username => $username, "$opt{set}" => $argv[0]);
		if (!$user) {
			$irc->say("Error updating user setting: $opt{set}!");
		}
		return;
	}

	# Print out user settings
	foreach my $username (@argv) {
		my $user = $UsersDB->get(username => $username);
		next if (!$user);
		delete($user->{password});
		$irc->say("User $username: " . encode_json($user));
	}
}

1;

package Users;

use strict;
use warnings;

use TokyoCabinet;

our $db_file = "$mod_perl::config::conf_dir/users.tct";
our $db_ref;

sub get_instance {
	my $class = shift;
	my %opt = @_;

	if (!$Users::db_ref) {
		$Users::db_ref = bless {}, $class;
		$Users::db_ref->initdb($opt{file});
	}

	return $Users::db_ref;
}

sub initdb {
	my ($self, $dbfile) = @_;
	return if ($self->{tdb});

	my $tdb = TokyoCabinet::TDB->new();
	$self->{tdb} = $tdb;
	$self->{dbfile} = $dbfile;

	if (!-e $dbfile) {
		if (!$tdb->open($dbfile, $tdb->OWRITER | $tdb->OCREAT)) {
			my $ecode = $tdb->ecode();
			printf STDERR ("Error opening userdb %s+wc: %s\n", $dbfile, $tdb->errmsg($ecode));
			return;
		}
		$tdb->close();
	} 
}

# Get a write handle to the database
sub opendb {
	my ($self, $mode) = @_;
	my $tdb = TokyoCabinet::TDB->new();
	my $dbfile = $self->{dbfile};

	$mode ||= "r";
	my $dbmode = $mode eq "w" ? $tdb->OWRITER | $tdb->OCREAT : $tdb->OREADER;
	if (!$tdb->open($dbfile, $dbmode)) {
		my $ecode = $tdb->ecode();
		printf STDERR ("Error opening userdb %s+%s: %s\n", $dbfile, $mode, $tdb->errmsg($ecode));
		return;
	}
	return $tdb;
}

# Get/lookup user from database
sub get {
	my ($self, %opt) = @_;
	my $dbfile = $self->{dbfile};
	my $tdb = $self->opendb("r");
	if ($tdb) {
		my $user = $tdb->get($opt{username});
		$tdb->close();
		return $user;
	}
}

# Add user to the database
sub add {
	my ($self, %opt) = @_;
	my $username = $opt{username};
	my $user = {
		username => $username,
		feeds => "",
	};
	my $tdb = $self->opendb("w");
	if (!$tdb || !$tdb->put($username, $user)) {
		my $error = $tdb && $tdb->errmsg($tdb->ecode()) || "Internal Error";
		print STDERR "Failed to add new user: $user: $error\n";
		return;
	}
	# Success
	return $tdb->close();
}

# Update user settings
sub update {
	my ($self, %opt) = @_;
	my $username = $opt{username};
	my $user = $self->get(username => $username);
	if (!$user) {
		print STDERR "No such user $username\n";
		return;
	}

	# Update user with data from options
	$user = { %$user, %opt };

	my $tdb = $self->opendb("w");
	if (!$tdb || !$tdb->put($username, $user)) {
		my $error = $tdb && $tdb->errmsg($tdb->ecode()) || "Internal Error";
		print STDERR "Failed to update user: $username: $error\n";
		return;
	}
	return $tdb->close();
}

# Remove user from the database
sub del {
	my ($self, %opt) = @_;
	my $username = $opt{username};
	my $tdb = $self->opendb("w");
	if (!$tdb || !$tdb->out($username)) {
		my $error = $tdb && $tdb->errmsg($tdb->ecode()) || "Internal Error";
		print STDERR "Failed to delete user: $username: $error\n";
		return;
	}
	return $tdb->close();
}

# List users in the database
sub list {
	my ($self, $search) = @_;
	my @results;
	my $tdb = $self->{tdb};
	my $value;
	my $count = 0;
	my $max = 25;

	my $dbfile = $self->{dbfile};
	if (!$tdb->open($dbfile, $tdb->OREADER)) {
		print STDERR "Failed to list users in db: %s: %s\n", $dbfile, $tdb->errmsg($tdb->ecode);
		return;
	}
	for ($tdb->iterinit(), $value = $tdb->iternext(); $value && $count < $max; $value = $tdb->iternext()) {
		if (!$search || $value =~ /\Q$search\E/) {
			push(@results, $value);
			$count++;
		}
	}
	$tdb->close();
	return @results;
}

1;

#
#    # create the object
#    my $tdb = TokyoCabinet::TDB->new();
#
#    # open the database
#    if(!$tdb->open("casket.tct", $tdb->OWRITER | $tdb->OCREAT)){
#        my $ecode = $tdb->ecode();
#        printf STDERR ("open error: %s\n", $tdb->errmsg($ecode));
#    }
#
#    # store a record
#    my $pkey = $tdb->genuid();
#    my $cols = { "name" => "mikio", "age" => "30", "lang" => "ja,en,c" };
#    if(!$tdb->put($pkey, $cols)){
#        my $ecode = $tdb->ecode();
#        printf STDERR ("put error: %s\n", $tdb->errmsg($ecode));
#    }
#
#    # store another record
#    $cols = { "name" => "falcon", "age" => "31", "lang" => "ja", "skill" => "cook,blog" };
#    if(!$tdb->put("x12345", $cols)){
#        my $ecode = $tdb->ecode();
#        printf STDERR ("put error: %s\n", $tdb->errmsg($ecode));
#    }
#
#    # search for records
#    my $qry = TokyoCabinet::TDBQRY->new($tdb);
#    $qry->addcond("age", $qry->QCNUMGE, "20");
#    $qry->addcond("lang", $qry->QCSTROR, "ja,en");
#    $qry->setorder("name", $qry->QOSTRASC);
#    $qry->setlimit(10);
#    my $res = $qry->search();
#    foreach my $rkey (@$res){
#        my $rcols = $tdb->get($rkey);
#        printf("name:%s\n", $rcols->{name});
#    }
#

