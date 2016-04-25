package mod_perl::modules::users;


use strict;
use warnings;

use mod_perl::base;
use mod_perl::config;

use mod_perl::users::user;

#
# Register a new user in the database (unless they exist already)
# $mask - a host mask restriction only address that match this mask will allowed to identify as you
# $password - the password to identify yourself if you need to change registry information or add reminders
# $email - an email address for reminders to be sent to (this will not be displayed or given to anyone)
# $phone - a phone number to text (must be stored in email format 10DIGIT@carrier.com) for reminders (as well as email)
# stores the users in a SQLite database
sub register_user {
	my ($irc, $arg) = @_;
	my ($mask, $password, $email, $phone) = $arg =~ /(\S+)/g;


	my $user = mod_perl::users::user->new(
		mask => $mask,
		password => $password,
		email => $email,
		phone => $phone,
	);

	if ($@) {
		chomp($@);
		$irc->say("Invalid information: $@");
	}

}

#
# Add a reminder for the user running the command
# (if he is registered) - if not registered he must 
#
# Reminder syntax should be loosely parsed as the following:
# [] denotes optional and | denotes alternatives
# %remind [me] [about]|[to] <message> [time]
# if time is blank, message should be parsed for something that looks like a time
# otherwise default to reminding in 5 minutes (maybe make the default a user specific setting)
#
sub add_reminder {
	my ($irc, $arg) = @_;
}

# Register a new user
#mod_perl::base::command_register('register', \&register_user);
#mod_perl::base::command_register('remind', \&add_reminder);

1;
