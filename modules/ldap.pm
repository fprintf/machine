package mod_perl::commands::ldap;
use strict;
use warnings;

use blib './IRC';
use mod_perl::commands;

use Net::LDAP;
use Net::LDAP::Filter;
use Cache::FastMmap;

my $ldap_handle;

# Shared process cache that stores the results of the last query
my $share_file_name = '/tmp/.ldap.sharefile.tmp.@@@';
unless (-e $share_file_name) {
	open(my $fh, ">", $share_file_name) or warn "Failed to open $share_file_name: $!\n";
	$fh && close($fh);
}
my $cache = Cache::FastMmap->new(
	share_file => $share_file_name,
	init_file => 0,      # file is created manually using exclusive lock so only one child creates it
	unlink_on_exit => 1
);

my %ldap_conf = (
	server => '10.8.96.26',
	binddn => 'test123@cudanet.local',
	bindpw => 'B@rracudA',
	searchbase => 'DC=cudanet, DC=local',
	admin_groups => [],
	user_groups => [],
);

mod_perl::commands::register_command('ldap', \&ldap_run);

sub ldap_help
{
	my $irc = shift;
	$irc->say("usage: ldap [--qtype type] [--help] [--raw] <search query>");
	$irc->say("-> Search query can be a name, phone number, email, etc.. and you can use wildcards.");
	$irc->say("-> Example: .ldap john*");
	$irc->say("-> 'qtype' allows you to control the results shown, following types supported");
	$irc->say("-->> 'normal' (default) displays name, department, phone, etc..");
	$irc->say("-->> 'groups'  displays groups the user is in");
	$irc->say("-->> 'personal' displays as much personal information available in the ldap record (non work-info)");
	$irc->say("-> 'raw' this makes the search query a raw LDAP filter");
	return;
}

sub ldap_bind
{
	my ($irc, $handle) = @_;

	if (!$handle) {
		$handle = new Net::LDAP($ldap_conf{server});
	}
	if (!$handle) {
		$irc->say("[error] Failed to connect to ldap: $!");
		warn "$!\n";
		return undef;
	}

	my $r = $handle->bind($ldap_conf{binddn}, password => $ldap_conf{bindpw});
	$r->code && do {
		$irc->say("[error] Failed to bind to LDAP: ".$r->error);
		warn "Failed to bind: ".$r->error."\n";
		$handle = undef;
	};

	return $handle;
}

sub ldap_stringify_entry {
	my ($e, $qtype) = @_;

	my $output = '';
	if ($qtype && $qtype eq 'personal') {
		my $name = $e->get_value('displayName') || $e->get_value('name') || $e->get_value('cn');
		my $mobile = $e->get_value('mobile') || $e->get_value('telephone') || 'NA';
		my $locale = $e->get_value('l') || 'Unknown';
		my $title = $e->get_value('title') || 'n/a';
		my $manager = $e->get_value('manager') || '';
		my $homepath = ($e->get_value('homeDrive') || '').($e->get_value('homeDirectory') || '');
		$output = sprintf("%-20s %9s %-10s %-25s %20s %-30s",
			$name, $mobile, $locale, $title, $manager, $homepath
		);
	} elsif ($qtype && $qtype eq 'groups') {
		my $name = $e->get_value('displayName') || $e->get_value('name') || $e->get_value('cn');
		my @groups = ($e->get_value('memberOf'));
		$output = sprintf("%-20s ". join(", ", @groups), $name);
	} else {
		my $name = $e->get_value('displayName') || $e->get_value('name') || $e->get_value('cn') || '';
		my $mail = $e->get_value('mail') || $e->get_value('userPrincipleName') || '';
		my $mobile = $e->get_value('mobile') || $e->get_value('telephone') || 'NA';
		my $ext = $e->get_value('ipPhone') || '0000';
		my $department = $e->get_value('department') || 'None';
		my $locale = $e->get_value('l') || 'Unknown';
		$output = sprintf("%-20s %-25s %9s(x%4s) %-12s %10s",
			$name, $mail, $mobile, $ext, $department, $locale	
		);
	}

	return $output;
}

sub ldap_display_entry
{
	my ($irc, $e, $qtype) = @_;
	$irc->say(ldap_stringify_entry($e, $qtype));
}


sub ldap_lookup
{
		my ($irc, $last, $handle, $search, $qtype, $raw) = @_;
		$handle = ldap_bind($irc, $handle);
		return if !$handle;

		my $filter;
		if ($raw) {
			$filter = new Net::LDAP::Filter($search);
		} else {
			$filter = new Net::LDAP::Filter("(|(cn=$search) (mail=$search) (sAMAccountName=$search) (userPrincipleName=$search) (ipPhone=$search) (mobile=$search) (telephone=$search))"); 
		}

		#DEBUG print filter
		#$filter->print(\*STDERR);
		my @entries = ();

		my $lookup_call = sub {
			my $res = $handle->search(
				base => $ldap_conf{searchbase},
				filter => $filter
			);
			$res->code && do { 
				$irc->say("[error] ".$res->error);
				warn $res->error; 
				return -1; 
			};
			# Repeated queries for the same lookup, will keep showing new results
			@entries = $res->entries;
			if (!@entries) {
				$irc->say("[error] No results found");
				return;
			}

			@entries = map { ldap_stringify_entry($_) } @entries;
		};

		# Check if we hit limit on this search, or if its a new search
		# Handle paging requests (repeated lookup for same feed, shows more results, no further lookups)
		if ($last->{query} eq $search) {
			$last->{count} += 5;
			if ($last->{entries}) {
				@entries = @{$last->{entries}};
			} else {
				if ($lookup_call->() <= 0) {
					# There was an error, exit
					return;
				}
			}
		} else {
			$last->{count} = 0;
			$last->{query} = $search;
			if ($lookup_call->() <= 0) {
				# There was an error, exit
				return;
			}
		}

		# Went over max, start at beginning..
		if ($last->{count} >= @entries) {
			$last->{count} = 0;
		}

		# Update cache with changes we made to $last
		# NOTE: We can't just use set() because set() doesn't allow you to overwrite existing cache and will
		# zero out teh value
		$cache->set("last_metadata", $last);
		$cache->set("last_results", \@entries) or warn "failed to store complex \@entries list\n";

		my $current = $last->{count};
		my $max_current = $current+5 > @entries ? @entries : $current+5;

		# Let them know we're showing offsetted results 
		if (@entries >= 2) {
			$irc->say("[LDAP] -> '$search' showing $last->{count}-$max_current (".scalar(@entries).")");
		}

		for (; $current < scalar(@entries) && $current < $max_current; ++$current) {
			$irc->say($entries[$current]);
		}
}

sub ldap_run
{
    my ($irc, $arg) = @_;
    my %opt = ();
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&ldap_help, $arg, qw(qtype|t=s raw|r help|h));
    return if (!$ret);

	# Initialize our paging system, if needed
	my $last = $cache->get("last_metadata");
	if (!$last || !ref($last)) {
		print STDERR "nothing from cache, using default...\n";
		$last = {query => '', count => 0};
		$cache->set("last_metadata", $last);
		$cache->set("last_results", []);
	} else {
		# Pull in the array of results we have stored
		$last->{entries} = $cache->get("last_results");
	}

	foreach my $search (@argv) {
		ldap_lookup($irc, $last, $ldap_handle, $search, $opt{qtype} || '', $opt{raw});
	}
}

1;
