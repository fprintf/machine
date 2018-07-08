package mod_perl::modules::linkbot;

use LWP::UserAgent;
use LWP::MediaTypes qw(guess_media_type);
use HTTP::Message;
use HTML::TreeBuilder;
use Time::Piece;

use strict;
use warnings;

use mod_perl::modules::utils;
use mod_perl::config;


my $line_limit = 256; # Maximum characters to output for untitled text-like pages
my %accepted_protocols = (
    'http' => 1, 
    'https' => 1,
);

# Create our exemption and threat checking functions
my $is_exempt = make_exempt_checker();
my $threat_check = make_threat_checker();
# Get handle to our link database
my $table_name = 'links';
my $table_schema = qq(
	CREATE TABLE IF NOT EXISTS $table_name (
		`date` int,
		`link` text primary key NOT NULL,
		`title` text DEFAULT NULL,
		`tinyurl` text DEFAULT NULL
	)
);
my $links = Links->new(); # Links is a class defined within this file

# Linkbot
mod_perl::base::event_register('PRIVMSG', \&run);

# Recall past links
mod_perl::base::command_register('link', \&link_handler);

sub get_user_agent
{
	my $ua = LWP::UserAgent->new(
		'timeout' => 120,
		'max_redirect' => 8,
		'max_size' => 5 * 1024 * 1024 * 1024,  # 5MB
	);

    return $ua;
}

sub humanBytes {
    my $bytes = shift(@_);
    my @units = ('B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB');
    my $unit_measure = 0;
    while( $bytes >= 1024 ){
        $bytes /= 1024;
        $unit_measure++;
    }
    $bytes = sprintf("%.2f", $bytes);
    return "$bytes$units[$unit_measure]";
}

sub get_title {
	my ($source) = @_;

    my $err = undef;
    my $title = undef;

    my $ua = get_user_agent();
	my $r = $ua->head($source);
    my $content_type = 'html'; 
	if ($r->is_success) {
		$content_type = $r->header('Content-Type') || $content_type;
	} else {
		$err = $r->status_line;
	}

	# Assume html if our head request failed
	if ($content_type =~ /html/io) {
		# We must perform a GET now to get the <title> and <h2> elements
		my $parser = HTML::TreeBuilder->new;
		my $doc;
		$r = $ua->get($source);
		if ($r->is_success) {
			$doc = $parser->parse_content($r->decoded_content());
			$err = undef; # ignore any HEAD request errors now
		} else {
			$err = $r->status_line;
		}

		if (!$doc) {
			print STDERR "linkbot: failed to retrieve webpage: $source $err\n";
			return ($title, $err);
		}

		# Get title or first h2 element
		if ( ($title = $doc->find('title')) ) {
#			print STDERR "got title: ".$title->as_trimmed_text()." for uri: $source\n";
#			print STDERR " decoded content was: ".substr($r->decoded_content(), 0, 4096)."\n";
			$title = $title->as_trimmed_text();
		} elsif ( ($title = $doc->find('h2')) ) {
			$title = $title->as_trimmed_text();
		} elsif ( ($title = $doc->find('h3')) ) {
			$title = $title->as_trimmed_text();
		} elsif ( ($title = $doc->find('h4')) ) {
			$title = $title->as_trimmed_text();
		}
	} elsif ($content_type =~ /text/io) {
		my $get = $ua->get($source);
		if ($get->is_success) {
			$title = $get->decoded_content;
		} else {
			$err = "text error:" . $get->status_line;
		}
	}

	if ($title) {
		$title = substr($title, 0, $line_limit);
		$title .= $line_limit < length($title) ? '...' : '';
	}

    # If we didn't find a title the normal way, just show the content information then
    if (!$title && !$err) {
        (my $basename = $source) =~ s/^.*\///;
        $basename = $r->filename || $basename;
        $content_type ||= 'application/octet-stream';
        my $size = $r->header('Content-Length') || '';
        $title = sprintf("%s [%s]%s", $basename, $content_type, $size ? " ".humanBytes($size) : '');
    }
    $title ||= 'Unknown';

	return ($title, $err);
}

sub make_threat_checker {
    my $api = {
        key => '298aa0a47a9c4703103f0c9b2d7c0e8394485613',
        uri => 'http://api.urlvoid.com/api1000'
    };
	my $ua = get_user_agent();
	my $parser = XML::LibXML->new;
	return sub {
		my ($uri) = @_;
		(my $host = $uri) =~ s/^\S+:\/\/(?:www)?\.?//;
		$host =~ s/\/.*$//;
		my $threat = $host;

		if (!$mod_perl::config::conf{linkbot_threat_check}) {
			return $host;
		}

		my $query = sprintf("%s/%s/host/%s/", $api->{uri}, $api->{key}, $host);
		#print STDERR "threatcheck query: $query\n";
		my $req = $ua->get($query);
		if (!$req->is_success()) {
			print STDERR "threatcheck: [$query] failed to retrieve: ".$req->status_line."\n";
			return $threat;
		}

		my $doc = $parser->load_xml(string => $req->content());
		if (!$doc) {
			print STDERR "threatcheck: parse failure: $!\n";
			return $threat;
		}

		# Decide threat of a site based on alexa rank and google rank
		# and if there are redirects and if the domain age 
		my $grank = int("".$doc->getElementsByTagName('google_page_rank')->to_literal);
		my $alexa = int("".$doc->getElementsByTagName('alexa_rank')->to_literal);
		my $domain_age = int("".$doc->getElementsByTagName('domain_age')->to_literal);

		my $rank = $grank + $alexa;
		my $good_threshold = 1000;
		my $bad_threshold = 5000;           
		my $age_limit = 3 * 365*24*3600; # 3 years

		if (($rank >= $bad_threshold || $rank < 1.0) && $domain_age < $age_limit) {
			$threat = "\00304$host\003"; # Bad site
		} elsif ($rank >= 1.0 && $rank <= $good_threshold) {
			$threat = "\00309$host\003"; # Good site
		} else {
			$threat = "\00307$host\003"; # Warning, unknown, but older than 3 years
		}

		# Only append rank if its actualy above 0.0
		if ($rank > 0.0) {
			$threat .= ' rank:'.$rank;
		}

		return $threat;
	};
}

sub make_exempt_checker {
	# Map the exemptions in the config into a more suitable format
	# for quick lookup purposes
	my %exemptions;
	while (my ($name, $val) = each %{$mod_perl::config::conf{servers}}) {
		my @tmp = @{$val->{linkbot_exemptions} || []};
		if (@tmp) {
			$exemptions{$name} = { map { $_ => 1 } @tmp };
		}
	}

	# Check against exemptions in the config 
	# nickname exemptions e.g. "nickname" (ignores all lines from nickname)
	# channel exemptions  e.g. "#channel" (ignore all lines from channel #channel)
	# exempt all on a server   (ignore all messages for any channel/nickname on this server)
	# Returns true if there is a matching exemption and false otherwise
	return sub {
		my ($irc) = @_;
		my $servername = $irc->servername;
		my $nick = $irc->nick;
		my $channel = $irc->target;
#		use Data::Dumper;
#		print STDERR "checking $servername and $channel and $nick against:", Dumper(\%exemptions), "\n";
		if (exists($exemptions{$servername}) && 
			exists($exemptions{$servername}{$channel}) || 
			exists($exemptions{$servername}{$nick})) {
			return 1;
		}
		return 0;
	};
}

sub run
{
    my $irc = shift;
    my $text = $irc->text;

	# Check if we should ignore this line due to an exemption
	if ($is_exempt->($irc)) {
#		print STDERR sprintf("linkbot exempting message from: %s nick: %s channel: %s\n", $irc->servername, $irc->nick, $irc->target);
		return;
	}

    # If not a url, ignore 
    while ($text =~ /((\S+):\/\/\S+\/?)/og) {
        my ($uri,$proto) = ($1, lc($2));
#        print STDERR "linkbot got proto: $proto uri: $uri\n";
        if (!exists($accepted_protocols{$proto})) {
# IF you want to be notified of unknown protocols
#            $irc->say(" $proto uknown :: threat:$threat :: [empty]");
            next;
        }

        my ($title, $err) = get_title($uri);
        if ($err) {
            print STDERR "Failed to retrieve $uri: $err\n";
            next;
        }
        my $threat = $threat_check->($uri);
        my $tinyurl = mod_perl::modules::utils::tinyurl($uri);
		# Add link to database
		$links->insert(title => $title, tinyurl => $tinyurl, link => $uri);
        $irc->say(" $tinyurl :: $threat :: $title ");
    }
}

#
# Recall links from the database
sub link_handler {
	my ($irc, $arg_string) = @_;
	my $max_results = 5;

	# Allow searching links by title or link itself
	# Allow retrieving (up to) last 5 links seen
	# Allow looking up links by date posted
	my $res = $links->search(title => $arg_string, link => $arg_string, tinyurl => $arg_string);
	for (my $i = 0; $i < @$res && $i < $max_results; ++$i) {
		my $data = $res->[$i];
		my $t = localtime($data->{date});
		$irc->say(sprintf("\00311[%02d]\003 (%s) %s \00311::\003 %s", $i + 1, 
			$t->strftime("%m/%d/%Y"), 
			$data->{title}, 
			$data->{link}
		));
	}
}

1;

package Links;

use DBI;

sub new {
	my ($class) = shift;
	my %opt = @_;
	my $self = { 
		%opt,
		dbh => undef,
	};
	bless $self, $class;
	$self->_create_table();

	return $self;
}

sub DESTROY {
	my $self = shift;
	if ($self->{dbh}) {
		$self->{dbh}->disconnect();
	}
}

# Search the links table for links by passing
# search criteria as a hashref. Start date and interval can also be passed to
# limit the search to that day
# 'title' => search the title of links
# 'link' => search the link text itself
# 'tinyurl' => search the tiny url for this link itself (this is not very useful) 
# 'limit' => limit the search results to this many, defaults to 10 if unset, unlimited results NOT supported
# TODO more granular search options not yet implemented:
# 'start_date' => search forward by 'interval' or 5 days, starting at this date
# 'interval' => search forward by this interval  if used with start_date, or search backward from today, if no start_date
sub search {
	my ($self, %pattern) = @_;
	$pattern{limit} ||= 10;
	my $query_template = "select {{columns}} from {{table_name}} {{where_clause}} {{order_by}}";
	my $data = { 
		sort_column => 'date',
		columns => '*',
		table_name => $table_name 
	};
	my @tmp;
	my @bindings;
	foreach my $type (qw(title link tinyurl)) {
		if (exists($pattern{$type})) {
			push(@tmp, sprintf("%s like ?", $type));
			push(@bindings, $pattern{$type} ? "%" . $pattern{$type} . "%" : "%");
		}
	}
	$data->{where_clause} = @tmp ? 'where ' . join(" OR ", @tmp) : '';
	$data->{order_by} = sprintf('order by %s desc limit %d', $data->{sort_column}, $pattern{limit});

	(my $query = $query_template) =~ s/\{\{(\S+)}}/$data->{$1}/eg;
#	print STDERR "linkdb query: [$query] bindings: [".join(", ", @bindings)."\n";
	my $ref = $self->{dbh}->selectall_arrayref($query, { Slice => {} }, @bindings);
#	use Data::Dumper;
#	print STDERR Dumper($ref);
	return $ref;
}

# Insert a record into the links table
# 'date' => date in UNIX time, defaults to current
# 'link' => link itself (the url)
# 'title' => title of link
# 'tinyurl' => tiny url generated for this link
sub insert {
	my ($self, %opt) = @_;
	my $query_template = "insert into %s (%s) values (%s)";
	my @cols;
	my @bindings;
	$opt{date} ||= time;
	foreach my $type (qw(date link title tinyurl)) {
		if (exists($opt{$type})) {
			push(@cols, $type);
			push(@bindings, $opt{$type});
		}
	}

	my $query = sprintf($query_template, 
		$table_name, join(", ", @cols), join(", ", map { "?" } @cols)
	);
#	print STDERR "linkdb insert query: [$query]\n";
	$self->{dbh}->do($query, {}, @bindings);
}

# Create link table if it doesn't exist already
sub _create_table {
	my $self = shift;
	if (!$self->{dbh}) {
		$self->{dbh}  = DBI->connect(
			sprintf("dbi:SQLite:dbname=%s", $mod_perl::config::module_db),
			'', ''
		);
	}
#	print STDERR "creating table $table_schema\n";
	$self->{dbh}->do($table_schema);
}

1;

