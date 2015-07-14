=pod

=head1 SYNOPSIS

   my $user = User->new(nick => "nickname",
                        host => "host",
   );

   my $pass = "12345";
   $user->password($pass); # Sets password for this user

   # Authenticate a user
   my $given_pass = "...";
   if ($user->auth($given_pass)) { # Authenticate a user with the given password
      # Successful login
   } else {
      # Failed to login
   }

   # Send an email to the user (if they have an email registered)
   $user->send_email(
            "Subject" => "yo what up user",
            "Message" => "how are you today?"
   );

   # Access values for a user
   $user->last_seen(); # Return last time we've seen this user
   $user->email(); # Return users email address (this is at the behest of the user to oblige this request)
   $user->nick(); # Return users registered nickname (nick they must use to identify with)
   $user->logged_in(); 

   # Set the user values
   $user->email(email); # sets the new email address
   $user->nick(nick); # sets the users new nickname

=cut

package User;
use strict;
use warnings;

use TokyoCabinet;

our $user_db_path = "users/users.tdb";

sub new {
    my $class = shift;
    my %args = @_;
    my $self = {
        nick => $args{nick},
        host => $args{host} || '',
        email => $args{email} || '',
    };

    bless $self, $class;
    $self->_load_user($self->{nick});

    return $self;
}

sub DESTROY {
    my ($self) = @_;

    # Make sure we close our tdb handle
    if (!$self->{tdb}->close()) {
        warn "close error $user_db_path: ", $self->{tdb}->errmsg($self->{tdb}->errcode), "\n";
        return;
    }
}


=pod

=head2 user_exists

Checks the database to see if a given nick exists
in the database or not. Useful if you want to check
it but not actually load the user/create the user if it
doesn't exist. This also is used without a user object
this ia  class function.

Returns the user columns if it exists, false otherwise

=head4 nick

the nickname of the user

=cut

sub user_exists {
    my $nick = shift;
    my $tdb = TokyoCabinet::TDB->new();

    if (!$tdb->open($user_db_path, $tdb->OWRITER|$tdb->OCREAT|$tdb->OREADER)) {
        warn "Failed to open $user_db_path ", $tdb->errmsg($tdb->ecode), "\n"; 
        return;
    }

    return $tdb->get($nick);
}

=pod

=cut

# set/access the email of the user
# 
sub email {
    my ($self, $email) = @_;
    $self->{email} = $email || $self->{email};
    return $self->{email};
}

# set/access the nick of the user
# 
sub nick {
    my ($self, $nick) = @_;
    $self->{nick} = $nick || $self->{nick};
    return $self->{nick};
}

# _load_user(nick) - load user from database
# this is the simple database abstraction
# to our TokyoCabinet::TDB database for the User
# object which is stored in a TDB with
# $nick - the username to lookup in the db (or create if doesn't exist)
sub _load_user {
    my ($self, $nick) = @_;
    my $tdb = TokyoCabinet::TDB->new();

    if (!$tdb->open($user_db_path, $tdb->OWRITER|$tdb->OCREAT|$tdb->OREADER)) {
        warn "Failed to open $user_db_path ", $tdb->errmsg($tdb->ecode), "\n"; 
        return;
    }

    my $user = $tdb->get($nick);
    # The user doesn't exist we need to create him
    if (!$user) { 
        $user = {
            nick => $nick,
            email => $self->{email},
            host => $self->{host},
            last_seen => scalar time
        };

        $tdb->put($nick, $user);
    } else {
        # Successfully loaded from database
        $self->{nick} = $nick;
        $self->{email} = $user->{email};
        $self->{host} = $user->{host};
        $self->{last_seen} = $user->{last_seen};
    }

    # Save the handle so we can easily update things later if need be
    $self->{tdb} = $tdb;
}

1;

package mod_perl::modules::users;

use strict;
use warnings;

1;
