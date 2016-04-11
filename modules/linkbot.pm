package mod_perl::modules::linkbot;

use LWP::UserAgent;
use LWP::MediaTypes qw(guess_media_type);
use HTML::TreeBuilder;

use strict;
use warnings;

use mod_perl::modules::utils;

my $line_limit = 256; # Maximum characters to output for untitled text-like pages
my %accepted_protocols = (
    'http' => 1, 
    'https' => 1,
);

# Create our exemption and threat checking functions
my $is_exempt = make_exempt_checker();
my $threat_check = make_threat_checker();

# Linkbot
mod_perl::base::event_register('PRIVMSG', \&run);

sub get_user_agent
{
	my $ua = LWP::UserAgent->new(
		'timeout' => 120,
		'max_redirect' => 8,
		'max_size' => 384 * 1024 * 1024,  # 384K
		'agent' => 'Mozilla/5.0 (X11; Linux i686; rv:21.0) Gecko/20100101 Firefox/21.0'
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

sub get_title
{
	my ($source) = @_;

    my $err = undef;
    my $title = undef;

    my $ua = get_user_agent();
	my $r = $ua->head($source);
    my $content_type = 'html'; 
	if ($r->is_success) {
		$content_type = $r->header('Content-Type');
	} else {
		$err = $r->status_line;
	}

	# Assume html if our head request failed
	if ($content_type =~ /html/io) {
		# We must perform a GET now to get the <title> and <h2> elements
		$r = $ua->get($source);
		my $parser = HTML::TreeBuilder->new;
		my $doc;
		if ($r->is_success) {
			$doc = $parser->parse_content($r->decoded_content);
		} else {
			$err = $r->status_line;
		}

		if (!$doc) {
			print STDERR "linkbot: failed to retrieve webpage: $source $err\n";
			return ($title, $err);
		}

		# Get title or first h2 element
		if ( ($title = $doc->find('title')) ) {
			$title = $title->as_trimmed_text();
		} elsif ( ($title = $doc->find('h2')) ) {
			$title = $title->as_trimmed_text();
		}
	} elsif ($content_type =~ /text/io) {
		$title = $r->decoded_content;
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
		my $grank = "".$doc->getElementsByTagName('google_page_rank')->to_literal;
		my $alexa = "".$doc->getElementsByTagName('alexa_rank')->to_literal;
		my $domain_age = "".$doc->getElementsByTagName('domain_age')->to_literal;

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
        $irc->say(" $tinyurl :: $threat :: $title ");
    }
}

1;
