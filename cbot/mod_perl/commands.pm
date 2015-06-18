package mod_perl::commands;
use strict;
use warnings;

use blib 'IRC';
use IRC;
use mod_perl::base;
use mod_perl::commands::feeds;
use mod_perl::commands::stock;
use mod_perl::commands::linkbot;

use Getopt::Long qw(:config no_ignore_case);
use Safe;


##################################################################
# UTILITY
##################################################################
sub handle_arg
{
    my ($oref, $msg, $href, $arg, @ostr) = @_;
    if (!$arg) {
        ref($href) eq 'CODE' && $href->($msg);
        return;
    }
    my ($ret,$argv) = Getopt::Long::GetOptionsFromString($arg, $oref, @ostr);

    if (!$ret) {
        print "[error] $!\n";
        return (undef,undef);
    }

    if (ref($href) eq 'CODE' && $oref->{help}) {
        $href->($msg);
        return;
    }

    return (1, @$argv);
}

##################################################################
# Commands/Help
##################################################################
mod_perl::base::command_register('feeds', \&mod_perl::commands::feeds::run);
mod_perl::base::command_register('stock', \&mod_perl::commands::stock::run);
mod_perl::base::command_register('talk', \&talk);
mod_perl::base::command_register('raw', \&raw);
mod_perl::base::command_register('perl', \&execperl);
mod_perl::base::command_register('help', \&help);
mod_perl::base::command_register('connect', \&connect);
mod_perl::base::command_register('tell', \&tell);
mod_perl::base::command_register('reload', sub { my $irc = shift; $irc->say("Reloading..."); $irc->reload(); });
# Example event register 
mod_perl::base::event_register('JOIN',
  sub {
    my $irc = shift;
    print STDERR "join target: ".$irc->text, "\n";
  }
);

my %messages = ();
# Linkbot
mod_perl::base::event_register('PRIVMSG', \&mod_perl::commands::linkbot::run);
# Tell handler
mod_perl::base::event_register('PRIVMSG', \&tell_handler);

sub tell_handler {
    my ($irc) = @_;
    my $ident = lc($irc->nick);

    if (my $data = $messages{$ident}) {
        foreach my $message (@$data) {
            my $timestamp = "" . gmtime($message->{time});
            $irc->say(sprintf("%s: %s <%s> %s",
                    $irc->nick, $timestamp, $message->{from}, $message->{message}
                )
            );
        }
        # Delete their messages now
        delete $messages{$ident};
    }
}

sub tell {
    my ($irc, $arg) = @_;

    my ($to, $message) = split(/\s/, $arg, 2);
    my $ident = lc($to);

    # Add message to the queue
    $messages{$ident} ||= [];
    push(@{$messages{$to}}, {'time' => time, from => $irc->nick, message => $message});

    $irc->say("I'll be sure to let $to know when they're around.");
}


sub help
{
    my ($irc, $arg) = @_;
    if ($arg) {
        foreach my $a (split(/\s+/, $arg)) {
            if (exists($mod_perl::base::command_registry{$a}) && defined &{$a."_help"}) {
                return &{$a."_help"}->($irc);
            }
        }
        return;
    }

    # else 
    $irc->say("[help] Available commands: ".join(", ", keys %mod_perl::base::command_registry));
}

sub execperl
{
    my ($irc, $arg) = @_;
    my $msg = '[empty]';
    eval {
        local $SIG{ALRM} = sub { die "timed out\n"; };
        alarm 30;
        $msg = Safe->new->reval($arg);
    };
    alarm;
    if ($@) {
        chomp($msg = $@);
    }

    $irc->say($msg);
}

sub raw
{
    my ($irc, $arg) = @_;
    $irc->raw($arg);
}

sub talk_help
{
    my ($irc) = @_;
    $irc->say('.talk [--target|-t target] <message>');
    $irc->say('--> "target" specifies a target to send the message to');
    $irc->say('--> Hint: Use double quotes around extra spaces or they will be collapsed');
}

sub talk
{
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = handle_arg(\%opt, $irc, \&talk_help, $arg, qw(target|t=s));
    return if (!$ret);
    my $msg = join(' ', @argv);
    if ($opt{target}) { $irc->privmsg($opt{target}, $msg); } 
    else { $irc->say($msg); }
}

sub connect
{
    my ($irc, $arg) = @_;
    my ($host,$port,$ssl) = split(/\s+/,$arg);
    print STDERR "connecting to new server: $host:$port".($ssl?" using ssl":"")."\n";
    $irc->connect($host,$port,$ssl);
}


1;
