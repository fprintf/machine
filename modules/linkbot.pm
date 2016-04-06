package mod_perl::modules::linkbot;

use LWP::UserAgent;
use LWP::MediaTypes qw(guess_media_type);
use HTML::TreeBuilder;

use strict;
use warnings;

use mod_perl::modules::utils;

my %accepted_protocols = (
    'http' => 1, 
    'https' => 1,
);

my $XML_PARSER = undef;
my $UA = undef;

my $line_limit = 256; # Maximum characters to output for untitled text-like pages

# Linkbot
mod_perl::base::event_register('PRIVMSG', \&run);

sub connect_useragent
{
    if (!$UA) {
        $UA = LWP::UserAgent->new(
            'timeout' => 120,
            'max_redirect' => 8,
            'max_size' => 384 * 1024 * 1024,  # 384K
            'agent' => 'Mozilla/5.0 (X11; Linux i686; rv:21.0) Gecko/20100101 Firefox/21.0'
        );
    }
    return $UA;
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

sub gettitle
{
	my ($source) = @_;

    my $err = undef;
    my $title = undef;

    my $ua = connect_useragent();
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

sub threatcheck
{
    my ($uri) = @_;
    my $threat_check = 1; # Set to 1 if check for threats (can be slow..)
    my $api = {
        key => '298aa0a47a9c4703103f0c9b2d7c0e8394485613',
        uri => 'http://api.urlvoid.com/api1000'
    };
    # Extract only the hostname portion of the url
    (my $host = $uri) =~ s/^\S+:\/\/(?:www)?\.?//;
    $host =~ s/\/.*$//;

    my $threat = $host;
    # IF we were told not to check threats, stop here
    return $threat if (!$threat_check);

    my $ua = connect_useragent();
    my $query = sprintf("%s/%s/host/%s/", $api->{uri}, $api->{key}, $host);
    #print STDERR "threatcheck query: $query\n";

    my $req = $ua->get($query);
    if ($req->is_success()) {
        if (!$XML_PARSER) {
            $XML_PARSER = XML::LibXML->new;
        }

        my $doc = $XML_PARSER->load_xml(string => $req->decoded_content());
        if (!$doc) {
            print STDERR "threatcheck: parse failure: $!\n";
            return $threat;
        }
        # Parse xml..

#<response>
#<details>
# <host>ibtimes.co.uk</host>
# <updated>1420571546</updated>
# <http_response_code>200</http_response_code>
# <domain_age>1141794000</domain_age>
# <google_page_rank>7</google_page_rank>
# <alexa_rank>0</alexa_rank>
# <connect_time>0.151433</connect_time>
# <header_size>510</header_size>
# <download_size>109061</download_size>
# <speed_download>208234</speed_download>
# <external_url_redirect></external_url_redirect>
# <ip>
#<addr>64.147.114.55</addr>
#<hostname>64.147.114.55.static.nyinternet.net</hostname>
#<asn>11403</asn>
#<asname>The New York Internet Company</asname>
#<country_code>US</country_code>
#<country_name>United States</country_name>
#<region_name>New York</region_name>
#<city_name>New York</city_name>
#<continent_code>NA</continent_code>
#<continent_name>North America</continent_name>
#<latitude>40.7089</latitude>
#<longitude>-74.0012</longitude>
# </ip>
#</details>
#<page_load>0.00</page_load>
#</response>
#
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

        $threat .= ' rank:'.$rank;

    } else {
        print STDERR "Failed to retrieve threat information: ".$req->status_line."\n";
    }

    return $threat;
}

# Check against exemptions in the config 
# nickname exemptions e.g. "nickname" (ignores all lines from nickname)
# channel exemptions  e.g. "#channel" (ignore all lines from channel #channel)
# exempt all on a server   (ignore all messages for any channel/nickname on this server)
# Returns true if there is a matching exemption and false otherwise
sub linkbot_exemptions {
	my ($irc) = @_;
	my $servername = $irc->servername;
	my $nick = lc($irc->nick);
	my $channel = lc($irc->target);

	my $conf = $mod_perl::config::conf{servers}{$servername};
	if ($conf && exists($conf->{linkbot_exemptions})) {
		my @exemptions = @{$conf->{linkbot_exemptions}};
		foreach my $ex (@exemptions) {
			$ex = lc($ex);
			if ($ex eq $nick || $ex eq $channel) {
				return 1;
			}
		}
	}

	return undef;
}

sub run
{
    my $irc = shift;
    my $text = $irc->text;

	# Check if we should ignore this line due to an exemption
	if (linkbot_exemptions($irc)) {
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

        my ($title, $err) = gettitle($uri);
        if ($err) {
            print STDERR "Failed to retrieve $uri: $err\n";
            next;
        }
        my $threat = threatcheck($uri);
        my $tinyurl = mod_perl::modules::utils::tinyurl($uri);
        $irc->say(" $tinyurl :: $threat :: $title ");
    }
}

1;
