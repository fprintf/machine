#use ExtUtils::testlib;
#use Cbot;

use strict;
use warnings;
use lib '.';
require mod_perl::base;
require mod_perl::commands;
use mod_perl::config;


sub event_handler
{
    my ($event,$msg) = @_;

    # Check event registry
    mod_perl::base::check_and_run_events($event,$msg);

    # Check command registry 
    mod_perl::base::check_and_run_commands($msg);
}

# We handle this immediately in our core code.
#sub PING
#{
#    my $irc = shift;
#    $irc->raw(sprintf("PONG %s",  $irc->text()));
#}

# MOTD
sub raw_372 {
	my $msg = shift;
	print STDERR $msg->text(), "\n";
}


# Nickname in use, we need to pick a new one
sub raw_433 {
    my ($irc) = shift;
    $irc->raw("NICK $mod_perl::config::conf{nickname}".int(rand(5000)));
}

sub raw_001 {
    my $msg = shift;
    my @channels = @{$mod_perl::config::conf{servers}->{$msg->servername()}->{channels}};
    foreach my $chan (@channels) {
	    $msg->raw("JOIN $chan");
    }
}

# EVENT Handlers
sub PRIVMSG
{
    my $msg = shift;
    event_handler('PRIVMSG', $msg);
}

sub NOTICE
{
    my $msg = shift;
    event_handler('NOTICE', $msg);
}

sub KICK
{
    my $msg = shift;
#    event_handler('NOTICE', $msg);
}

sub NICK
{
    my $msg = shift;
#    event_handler('NOTICE', $msg);
}

sub JOIN
{
    my $msg = shift;
    # Check event registry
    mod_perl::base::check_and_run_events('JOIN',$msg);
}

sub PART
{
    my $msg = shift;
#    event_handler('NOTICE', $msg);
}

sub QUIT
{
    my $msg = shift;
#    event_handler('NOTICE', $msg);
}

sub raw_485 {
	my $msg = shift;
	print STDERR "485 ".$msg->text."\n";
}

sub raw_477 {
	my $msg = shift;
	print STDERR $msg->text(), "\n";
}

sub raw_350 {
	my $msg = shift;
	print STDERR $msg->text(), "\n";
}

sub raw_408 {
	my $msg = shift;
	print STDERR $msg->text(), "\n";
}

1;
