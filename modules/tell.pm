package mod_perl::modules::tell;
use strict;
use warnings;

use mod_perl::commands;

use IPC::Shareable;

my %messages;
tie %messages, 'IPC::Shareable', 'tell', {
    exclusive => 0, destroy => 1, create => 1
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
    my $from = $irc->nick();

    # Add message to the queue
    $messages{$ident} ||= [];
    push(@{$messages{$to}}, {'time' => time, from => $irc->nick, message => $message});


    my @responses = (
        "$from: I'll be sure to let $to know when they're arround.",
        "$from: Ok, ok, stop bothering me!",
        "$from: You got it, friend.",
        "$from: Yeah, yeah, I'll let them know -- you sure talk to $to a lot. You gotta thing for them or something?",
        "$from: Right right, my brother.",
        "$from: Will do.",
        "$from: As you wish.",
        "$from: $to will be notified as soon as possible.",
        "$from: Ok boss.",
        "$from: Ok cheif.",
        "$from: Gotcha."
    );
    $irc->say($responses[int(rand(scalar(@responses)))]);
}


1;
