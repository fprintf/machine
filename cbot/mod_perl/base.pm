package mod_perl::base;
use strict;
use warnings;

my $cmd_char_pat = qr/^[%\/\.#]/o;
our %event_registry = ();
our %command_registry = ();

sub event_register
{
    my ($event,$code) = @_;
    $event = uc($event);
    my $evmap = {'PRIVMSG' => 1, 'NOTICE' => 1, JOIN => 1};

    if (!exists($evmap->{$event})) {
        print STDERR "[error] Attempt to register invalid event '$event'\n";
        return;
    } else {
        print "[info] Registered event '$event' -> '$code'\n";
    }

    # Push our code ref onto our event array
    push(@{$event_registry{$event}}, $code);
}

# Check event registry
sub check_and_run_events
{
    my ($event, $msg) = @_;
    return if (!exists($mod_perl::base::event_registry{$event}));

    # Execute our event handler for this event
    my $ref = $mod_perl::base::event_registry{$event}; 
    if (ref($ref) eq 'CODE') { $ref->($msg); }
    elsif (ref($ref) eq 'ARRAY') {
        foreach my $code (@$ref) {
            # Execute each handler in this array
            next if (ref($code) ne 'CODE');
            $code->($msg);
        }
    }
}

#
# Run commands from authorized users
# (currently no authorization unfortunately)
#

sub command_register
{
    my ($cmd,$code) = @_;
    $cmd = lc($cmd);

    if (exists($command_registry{$cmd})) {
        print "[warn] overwriting existing '$cmd'\n";
    }

    $command_registry{$cmd} = $code;
}

sub check_and_run_commands
{
    my $msg = shift;

    # Not a command 
    return if ($msg->text !~ /$cmd_char_pat(\S+)(?:\s(.+))?$/o);
    my ($cmd,$arg) = ($1,$2);

    if (exists($command_registry{lc($cmd)})) {
        exec_command($msg,$cmd,$arg);
    }
}

sub exec_command
{
    my ($msg,$cmd,$arg) = @_;

    $command_registry{$cmd}->($msg,$arg);
}

1;
