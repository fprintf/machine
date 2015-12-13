package mod_perl::modules::logchan;
use strict;
use warnings;

use mod_perl::commands;

use IPC::Shareable;

# Table of sources and destinations
# source => [ 'destination', 'destination', .... ]
my %logchan_routes;

tie %logchan_routes, 'IPC::Shareable', 'logchan', {
    exclusive => 0, destroy => 1, create => 1
};

# Handler to listen for routable messages and route them as appropriate
mod_perl::commands::register_handler('PRIVMSG', \&logchan_handler);
# Command to add/remove routes (turn on/off channel logging to another channel)
mod_perl::commands::register_command('logchan', \&logchan);

sub logchan_handler {
    my ($irc) = @_;

	my $source = $irc->servername . ":" . $irc->target;
	my $entries = $logchan_routes{$source};
	if ($entries && @$entries) {
		foreach my $dest (@$entries) {
			my ($server, $target) = split(/:/, $dest, 2);
#			print STDERR "sending message to $server $target :".$irc->text."\n";
			$irc->privmsg_server($server, $target, "[$source] <".$irc->nick."> " . $irc->text);
		}
	}
}

sub logchan {
    my ($irc, $arg) = @_;

    my ($source, $dest) = split(/\s/, $arg, 2);

	if (!$source || !$dest) {
		$irc->say("[error] Please provide a <source> and <destination> in the format server:#channel or #channel for both parameters.");
		$irc->say("[error] Example: .logchan freenode:#linux #clanz -- Redirect channel #linux on 'freenode' to current server on channel #clanz");
		return;
	}

	$source = lc($source);
	$dest = lc($dest);

	# Prepend the servername if it wasn't given
	if ($dest !~ /:/) {
		$dest = $irc->servername . ":" . $dest;
	}
	if ($source !~ /:/) {
		$source = $irc->servername . ":" . $source;
	}

	# Add the entry if its not there already
	if (!exists($logchan_routes{$source})) {
		$logchan_routes{$source} = [];
	}
	my $entries = $logchan_routes{$source};
	push(@{$logchan_routes{$source}}, $dest) if (!grep($dest, @$entries));

	$irc->say("Added destination $dest for source $source");
}


1;
