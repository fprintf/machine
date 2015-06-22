package mod_perl::modules::tell;
use strict;
use warnings;

use mod_perl::commands;

use IPC::Shareable;

my %messages;
tie %messages, 'IPC::Shareable', 'tell', {
    exclusive => 1, destroy => 1, create => 1
};
# Tell handler
mod_perl::commands::register_handler('PRIVMSG', \&tell_handler);
# Tell command
mod_perl::commands::register_command('tell', \&tell);

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


1;
