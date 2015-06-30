package mod_perl::modules::feeds;

use mod_perl::commands;
use mod_perl::config;
use strict;
use warnings;

#use lib '/home/dead/perl5/lib/perl5';
#use blib './IRC';
use XML::LibXML;
use LWP::UserAgent;
use HTML::Entities;
use Cache::FastMmap;

use mod_perl::modules::utils;

my $XML_PARSER;
my %feeds;

# Init our inter-process shared cache which we use
# to store the results from the last query (if its the same feed)
my $share_file_name = '/tmp/.feeds.sharefile.tmp.@@@';
unless (-e $share_file_name) {
	open(my $fh, ">", $share_file_name) or warn "Failed to initialize shared memory: $!\n";
	$fh && close($fh);
}
my $cache = Cache::FastMmap->new(
	share_file => $share_file_name,
	init_file => 0,
	unlink_on_exit => 1
);

# Register our run command
mod_perl::commands::register_command('feeds', \&run);

sub feeds_help
{
	my $irc = shift;
    $irc->say("usage: feeds [--list] [--add <name> <title> <source>] <source> [source ...]");
    $irc->say("-> RSS show feeds from various sources");
    $irc->say("-> 'list' list the available sources");
    return;
}

sub feeds_getXMLdoc
{
	my ($irc, $source) = @_;
	if (!$XML_PARSER) {
		$XML_PARSER = XML::LibXML->new;
	}
	my $ua = LWP::UserAgent->new;
	my $r = $ua->get($source);

	my $doc = undef;
	if ($r->is_success) {
		$doc = $XML_PARSER->load_xml(string => $r->decoded_content);
	} else {
		$irc->say("[error] Couldn't retrieve '$source': ".$r->status_line);
	}

	return $doc;
}

sub feeds_add
{
	my ($irc, $name, $title, $source) = @_;
	return 0 if (!$name || !$title || !$source);

	my $doc = feeds_getXMLdoc($irc, $source);
	return 0 if !$doc;

	$feeds{$name} = { title => $title, source => $source };

    my $tmp_path = ".".$mod_perl::config::conf{feeds_path}.".".$$.".tmp";
    if (open(my $out, ">$tmp_path")) {
        use Data::Dumper;
        my $FEEDS = \%feeds;
        print $out Dumper($FEEDS);
        close $out;

        # Now overwrite the feeds file, atomicly
        rename($tmp_path,$mod_perl::config::conf{feeds_path}) or do {
            print STDERR "Failed to overwrite $mod_perl::config::conf{feeds_path}!: $!\n";
        };
    } else {
        print STDERR "Failed to write out new feeds: $!\n";
    }

	return $doc;
}

sub feeds_load
{
    my ($path) = @_;

    my $FEEDS = do $path;
    if ($@) {
        chomp($@);
        print STDERR "Failed to load feeds: $@\n";
        return;
    }

    if (!ref($FEEDS) || ref($FEEDS) ne 'HASH') {
        print STDERR "<$FEEDS> is not a HASH reference!! fatal error!\n";
        return;
    }

    %feeds = %$FEEDS;
}

sub feeds_lookup
{
	my ($irc, $feed, $last) = @_;
	if (!exists($feeds{$feed})) {
		$irc->say("[error] Invalid feed '$feed'");
		return; 
	}

	# Handle paging requests (repeated lookup for same feed, shows more results, no further lookups)
	my @items = ();
	my $lookup_call = sub {
		my $doc = feeds_getXMLdoc($irc, $feeds{$feed}->{source});
		return if !$doc;
		@items = $doc->getElementsByTagName('item');
		if (!@items) {
			$irc->say("[error] Malformed XML received, can't continue");
			return;
		}
		@items = map {
			my $title = decode_entities($_->find('title'));
			my $link  = decode_entities($_->find('link'));
			$link = mod_perl::modules::utils::tinyurl($link);
			"* $title ::  $link";
		} @items;
	};

	if ($last->{feed} eq $feed) {
		$last->{count} += 5;
		if ($last->{items}) {
			@items = @{$last->{items}};
		} else {
			$lookup_call->();
		}
	} else {
		$last->{count} = 0;
		$last->{feed} = $feed;

		$lookup_call->();
	}

	# Went over max, start at beginning..
	if ($last->{count} >= @items) {
		$last->{count} = 0;
	}

	# Update cache now
	$cache->set("last_metadata", $last);
	$cache->set("last_results", \@items) or warn "feeds: unable to store last_results\n";

	my $current = $last->{count};
	my $max_current = $current+5 > @items ? @items : $current+5;

	my @citems = @items[$current .. ($max_current-1)]; #
	if (@citems) {
		$irc->say("[$feeds{$feed}->{title}] -> showing $current-$max_current (".scalar(@items).")");
		foreach my $item (@citems) {
			$irc->say($item);
		}
	}
}

sub run
{
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&feeds_help, $arg, qw(list|l add|a help|h));
    return if (!$ret);

    if (!%feeds) {
        feeds_load($mod_perl::config::conf{feeds_path});
    }

	if ($opt{list}) {
		$irc->say("Sources: ".join(", ", keys %feeds));
		return;
	}

	if (!@argv) {
		feeds_help($irc);
		return;
	}

	# Initialize our paging system, if needed
	my $last = $cache->get("last_metadata");
	if (!ref($last)) {
		$last = {feed => '', count => 0};
		$cache->set("last_metadata", $last);
		$cache->set("last_results", []);
	} else {
		$last->{items} = $cache->get("last_results");
	}

	if ($opt{add}) {
		my ($name,$title,$source) = @argv;
		my $doc = feeds_add($irc, $name, $title, $source);
		if (!$doc) {
			$irc->say("[error] Failed to add '$title': $source");
		}

		# We're done if adding..
		return;
	}

	foreach my $f (@argv) {
		$f = lc($f);
		feeds_lookup($irc, $f, $last);
	}
}

1;
