package mod_perl::modules::feedsnew;

use mod_perl::config;
use strict;
use warnings;

use mod_perl::modules::utils;

# Holds access to our feeds database within this process
my $feeds = Feeds->new(path => $mod_perl::config::module_db);
mod_perl::commands::register_command('feedsnew', \&run);


sub feeds_help {
	my $irc = shift;
    $irc->say("usage: feeds [--list] [--add <name> <source>] [--search <pattern> [source]] <source> [source ...]");
    $irc->say("-> RSS show feeds from various sources");
    $irc->say("-> 'list' list the available sources");
    $irc->say("-> 'add' add a new feed, requires a name and a feed url");
    $irc->say("-> 'search' searches all the feeds (or feeds matching pattern [source]) for the given pattern");
    return;
}


# Add a new feed
sub feeds_add {
	my ($irc, $name, $source) = @_;

	eval {
		$feeds->add($name, $source);
	};
	if ($@) { 
		chomp($@); 
		$irc->say($@); 
		return;
	}
}

# Search feeds and their titles for given query
# $query to search for
# $limit is a restriction on the feed names
sub feeds_search {
	my ($irc, $query, $limit) = @_;

	# Make sure we can compile our query and feed pattern regex
	eval {
		$query = qr/$query/io;
		$limit = qr/$limit/io if $limit;
	};
	if ($@) {
		$irc->say($@);
		return;
	}

	my @results;
	foreach my $feed ($feeds->list) {
		# We're only looking for feeds matching pattern '$feeds' (or all feeds if no pattern given)
		next if ($feeds && $feed !~ $feeds);

		foreach my $entry ($feed->update()->entries()) {
			if ($entry->title =~ $query) {
				push(@results, sprintf("[%s] %s :: %s", $feed->title(), $entry->title, $entry->link));
			}
		}
	}

	if (!scalar(@results)) {
		$irc->say("No results found.");
		return;
	}

	# Print out up to 5 results if any were found
	$irc->say("Showing max 5 out of ".(scalar(@results))." total found");
	foreach my $result (@results[0..4]) {
		$irc->say($result);
	}
}

sub feeds_lookup {
	my ($irc, $name) = @_;

	# Handle paging requests (repeated lookup for same feed, shows more results, no further lookups)
	my $feed = eval { $feeds->get_latest($name) };
	if ($@) {
		$irc->say($@);
		return;
	}

	my $count;
	foreach my $entry ($feed->entries()) {
		# Print feed title on first iteration
		if (!$count) { $irc->say($feed->title); }

		my $title = $entry->title();
		my $link = mod_perl::modules::utils::tinyurl($entry->link());
		$irc->say(sprintf("* %s :: %s", $title, $link));

		# Reached limit
		last if (++$count >= 5);
	}
}

sub run {
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&feeds_help, $arg, qw(list|l add|a help|h search|s));
    return if (!$ret);

	# Make sure we're up to date on each run, this only does something if the file changed
	$feeds->update();

	if ($opt{list}) {
		$irc->say("Sources: ".join(", ", $feeds->list));
		return;
	}

	if (!@argv) { return feeds_help($irc); }
	if ($opt{add}) { return feeds_add($irc, @argv); }
	if ($opt{search}) { return feeds_search($irc, @argv); }

	foreach my $f (@argv) {
		$f = lc($f);
		feeds_lookup($irc, $f);
	}
}


################################################################################
# Feeds object
################################################################################
# Load feeds from database and store them in memory,
# when list of feeds is requested check filestat of database
# against last update time stored in our object
package Feeds;

use strict;
use warnings;

use DBI;

our %defaults = (
	dbname => 'test.db',
	dbuser => '',
	dbpass => '',
);

sub new {
	my $class = shift;
	my %opt = @_;

	my $self = bless { 
		last_update => 0,
		dbname => $opt{path} || $defaults{dbname},
		dbuser => $opt{user} || $defaults{dbuser},
		dbpass => $opt{pass} || $defaults{dbpass},
	}, $class;

	$self->{dbh} = DBI->connect(
		sprintf("dbi:SQLite:dbname=%s", $self->{dbname}),
		$self->{dbuser}, $self->{dbpass}
	);

	# Create the table and update the database all in one swoop
	return $self->_create_table()->update();
}

sub update {
	my $self = shift;

	# Check to see if we even need to do an update
	my $mtime = (lstat($self->{dbname}))[9]; # 9 = mtime
	if ($mtime > 0 && $mtime < $self->{last_update}) {
		# No update needed, done!
		return $self;
	}

	# Key field is the 'name' field
	my $feeds = $self->{dbh}->selectall_hashref("select * from feeds", 'name');

	if (!scalar(keys %$feeds)) {
		die "No feeds found in database, probably something went wrong.\n";
	}

	# Load list of feeds into Feed objects but don't parse the content yet
	foreach my $name (keys %$feeds) {
		my $feed = $feeds->{$name};
		$self->{feeds}->{$name} = Feed->new(
			feed_name => $name,
			source_url => $feed->{url}, 
			dont_parse => 1
		);
	}
	# Record that we've now updated our records
	$self->{last_update} = time;

	return $self;
}

# Get feed at given name (current state, possibly not parsed)
sub get {
	my $self = shift;
	my $name = shift;

	# Invalid feed
	my $feed = $self->{feeds}->{$name};
	if (!$feed) {
		die "Feed $name does not exist\n";
	}

	return $feed;
}


# Return a feed's latest updates
# '$name' name of the feed being looked up
sub get_latest {
	my $self = shift;
	my $name = shift;

	return $self->get($name)->update();
}

# Return an array of feed names in the database
sub list {
	my $self = shift;
	return sort keys %{$self->{feeds}};
}

# Add a feed to the database
# name => name used to refer to feed
# url => url used to refer to feed
# NOTE this will overwrite existing feeds by the same name, in the database
sub add {
	my $self = shift;
	my %opt = @_;

	if (!$opt{name} || !$opt{url}) {
		die "Can't create new feed with no name or url\n";
	}

	my $feed = Feed->new(feed_name => $opt{name}, source_url => $opt{url});

	# Update the database
	my $sth = $self->{dbh}->prepare("replace into feeds (name, title, url) values(?, ?, ?)");
	$sth->execute($feed->name, $feed->title, $feed->url);

	# Update our in memory object
	$self->{feeds}->{$feed->name} = {
		name => $feed->name,
		title => $feed->title,
		url => $feed->url,
	};

	return $self;
}

sub _create_table {
	my $self = shift;
	my $schema = q|
		CREATE TABLE IF NOT EXISTS feeds(
			name text primary key not null,
			title text default 'Untitled',
			url text not null
		)
	|;

	$self->{dbh}->do($schema);

	return $self;
}


################################################################################
# Feed object
################################################################################
package Feed;
use strict;
use warnings;

use XML::Feed;

# source_url => 'url to pulldown feed' (this is the url the feed is located at)
# feed_name => 'one-word-name of feed' (this is used to lookup/refer to the feed in commands)
# dont_parse => 'don't parse the feed immediately'
# throws exceptions on error
sub new {
	my $class = shift;
	my %opt = @_;

	die "Feed requires 'source_url'\n" if (!$opt{source_url});
	die "Feed requires 'feed_name'\n" if (!$opt{feed_name});

	my $self = bless {
		%opt,
	}, $class;

	# Parse feed (unless requested not to)
	$self->update() unless $opt{dont_parse};

	return $self;
}

# Re-parse feed for new updates
sub update {
	my $self = shift;
	$self->{_document} = XML::Feed->parse(URI->new($self->{source_url}));
	return $self;
}

sub name {
	my $self = shift;
	return $self->{feed_name};
}

sub url {
	my $self = shift;
	return $self->{source_url};
}

sub title {
	my $self = shift;
	return $self->{_document}->title();
}

sub entries {
	my $self = shift;
	return $self->{_document}->entries();
}

1;
