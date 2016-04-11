package mod_perl::commands;
use strict;
use warnings;

use blib 'IRC';
use IRC;
use mod_perl::base;
use mod_perl::config;

use Getopt::Long qw(:config no_ignore_case);
use Safe;


# MAIN
load_modules();

# END MAIN

sub load_modules {
	print "Loading modules\n";
	push(@mod_perl::config::module_dirs, $mod_perl::config::conf{module_path});
	foreach my $module_path (@mod_perl::config::module_dirs) {
		print "Searching for modules in $module_path\n";
		if (! -e $module_path) {
			print STDERR "Module directory doesn't exist: $module_path\n";
			next;
		}

		my $dh;
		if (!opendir($dh, $module_path)) {
			print STDERR "Failed to read $module_path: $!\n";
			next;
		}

		while (my $module = readdir($dh)) {
			next if ($module =~ /^\.*$/);
			next if ($module !~ /\.pm$/);
			$module =~ s/\.pm$//;
			eval "use lib '$module_path'; use $module";
			if ($@) {
				print STDERR "Failed to load module: $module: $@";
				next;
			}
			print "Loaded module: $module\n";
		}
	}
}


##################################################################
# UTILITY
##################################################################
sub handle_arg {
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

#
## Register a command to the bot
# so you can register the commands from within 
# your module instead of rewriting
# the main command module all the time
sub register_command {
    my ($command, $coderef) = @_;
    if (!ref($coderef) || ref($coderef) ne 'CODE') {
        print STDERR "Invalid command registry: $command: $coderef\n";
        return;
    }

    mod_perl::base::command_register($command, $coderef);
}

#
## Register an event handler to the bot
# so you can register the event handlers from within 
# your module instead of rewriting
# the main command module all the time
sub register_handler {
    my ($event, $coderef) = @_;
    if (!ref($coderef) || ref($coderef) ne 'CODE') {
        print STDERR "Invalid command registry: $event: $coderef\n";
        return;
    }

    mod_perl::base::event_register($event, $coderef);
}

##################################################################
# Commands/Help
##################################################################
mod_perl::base::command_register('talk', \&talk);
mod_perl::base::command_register('raw', \&raw);
mod_perl::base::command_register('perl', \&execperl);
mod_perl::base::command_register('help', \&help);
mod_perl::base::command_register('connect', \&connect);
mod_perl::base::command_register('reload', sub { my $irc = shift; $irc->say("Reloading..."); $irc->reload(); });
# Example event register 
#mod_perl::base::event_register('JOIN',
#  sub {
#    my $irc = shift;
#    print STDERR "join target: ".$irc->text, "\n";
#  }
#);

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
		# Redirect stdout/stderr to variables also
		my $stdout;
		my $stderr;
		open(OLDSTDOUT, ">&STDOUT");
		open(OLDSTDERR, ">&STDERR");
		close(STDOUT);
		close(STDERR);
		open(STDOUT, ">", \$stdout);
		open(STDERR, ">", \$stderr);

		$stderr =~ s/[\r\n]+(.)/ | $1/g if ($stderr);
		$stdout =~ s/[\r\n]+(.)/ | $1/g if ($stdout);

		my $safe = Safe->new;
		$safe->permit(qw(time sort print));
        $msg = $safe->reval($arg);

		# Restore original stdout/stderr
		close(STDERR);
		close(STDOUT);
		open(STDOUT, ">&OLDSTDOUT");
		open(STDERR, ">&OLDSTDERR");

		# Return output from whatever source we got it from
		$msg = $stderr || $stdout || $msg;
    };
    alarm 0;
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
    $irc->say('--> "target" specifies a target to send the message to (use servername:target for cross-server communication)');
    $irc->say('--> Hint: Use double quotes around extra spaces or they will be collapsed');
}

sub talk
{
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = handle_arg(\%opt, $irc, \&talk_help, $arg, qw(target|t=s));
    return if (!$ret);
    my $msg = join(' ', @argv);
    if ($opt{target}) { 
		my ($server, $target) = split(/:/, $opt{target});

		# If they passed a server in the target, then use it, otherwise just use the target
		if ($server && $target) { 
			$irc->privmsg_server($server, $target, $msg); 
		} else {
			$irc->privmsg($server, $msg); 
		}
	} else { 
		$irc->say($msg); 
	}
}

sub connect
{
    my ($irc, $arg) = @_;
    my ($host,$port,$ssl) = split(/\s+/,$arg);
    print STDERR "connecting to new server: $host:$port".($ssl?" using ssl":"")."\n";
    $irc->connect($host,$port,$ssl);
}


1;
