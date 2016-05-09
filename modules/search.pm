package mod_perl::modules::search;

use mod_perl::commands;
use mod_perl::config;
use strict;
use warnings;

#use lib '/home/dead/perl5/lib/perl5';
use LWP::UserAgent;
use JSON::XS;
use URI::Escape;
use Encode;
use HTML::Entities;

use Data::Dumper;

my $google_api = $mod_perl::config::conf{google_api_url} || 
'https://ajax.googleapis.com/ajax/services/search/web?v=1.0&q=';

# Turn off HTML and skip the disambiguation page, also return JSON results
my $ddg_api = $mod_perl::config::conf{ddg_api_url} ||
'https://api.duckduckgo.com/?no_html=1&format=json&q=';

mod_perl::commands::register_command('g', \&google_run);
mod_perl::commands::register_command('google', \&google_run);
mod_perl::commands::register_command('ddg', \&ddg_run);

sub google_help {
	my $irc = shift;
    $irc->say("usage: g[oogle] <query>");
    $irc->say("-> Search google");
    return;
}

sub ddg_help {
	my $irc = shift;
    $irc->say("usage: ddg <query>");
    $irc->say("-> Search google");
    return;
}

sub lookup {
    my ($irc, $api_url, $query) = @_;
    my $ua = LWP::UserAgent->new;
    my $req = $ua->get($api_url . uri_escape($query));

    if ($req->is_success) {
        my $content = $req->decoded_content;
        # Must ensure the data is encoded as utf-8 
        my $json = decode_json(encode('utf-8', $req->decoded_content));
		print STDERR Dumper($json), "\n";
        my @results = @{$json->{responseData}->{results}};

        my $resultCount = $json->{responseData}->{cursor}->{resultCount} || 0;
        my $searchTime = $json->{responseData}->{cursor}->{searchResultTime} || 'n/a';
        my $extraResultsUrl = $json->{responseData}->{cursor}->{moreResultsUrl} || '';

        return if (!@results);

        $irc->say(sprintf("[%s] results: %s took %.2fms", $query, $resultCount, $searchTime));
        foreach my $result (@results[0..5]) {
            my $title = decode_entities($result->{titleNoFormatting} || '');
            my $url = uri_unescape($result->{url} || '');
            next if (!$title || !$url);
            $irc->say("$title :: $url");
        }
    } 
}

sub ddg_lookup {
	my ($irc, $api_url, $query) = @_;

	my $ua = LWP::UserAgent->new;
	my $req = $ua->get($api_url . uri_escape($query));
	if (!$req->is_success) {
		print STDERR $api_url.uri_escape($query).": ".$req->status_line."\n";
		return;
	}

	my $json = decode_json($req->content);
	my ($summary, $url, $source, $results);
	if ($json->{AbstractText}) {
		$summary = $json->{AbstractText};
		$url = $json->{AbstractURL};
		$source = $json->{AbstractSource};
	} elsif ($json->{Answer}) {
		$summary = $json->{Answer};
		$source = $json->{AnswerType};
	} elsif ($json->{Definition}) {
		$summary = $json->{Definition};
		$source = $json->{DefinitionSource};
		$url = $json->{DefinitionURL};
	} elsif ($json->{Redirect}) {
		$summary = $json->{Redirect};
	} else {
		# Try to see if there was a 'Results' array, return the first URL if so.
		$results = $json->{RelatedTopics};
	}

	foreach my $result (@$results) {
		$summary = $result->{Text};
		$url = $result->{FirstURL};
		last;
	}

	if (!$summary) {
		if ($json->{Type} eq 'D') {
			$irc->say("Ambigous result. Please be more specific");
			return;
		}
		$irc->say("No results returned.");
		use Data::Dumper;
		print STDERR Dumper($json), "\n";
		return;
	}

	my $output = sprintf("%s%s%s", 
		$source ? "[$source] " : '', 
		$summary, 
		$url ? " :: ".$url : ''
	);
	$irc->say($output);
}

sub ddg_run {
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&ddg_help, $arg, qw(help|h));
    return if (!$ret);

	if (!$arg) {
		ddg_help($irc);
		return;
	}

	ddg_lookup($irc, $ddg_api, $arg);
}

sub google_run {
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&google_help, $arg, qw(help|h));
    return if (!$ret);

	if (!$arg) {
		google_help($irc);
		return;
	}

	$irc->say("Sorry, google has disabled their login-free API. This function disabled.");
	return;

	lookup($irc, $google_api, $arg);
}

1;
