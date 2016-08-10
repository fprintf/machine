package mod_perl::modules::wolfram;

use mod_perl::commands;
use mod_perl::config;
use mod_perl::modules::utils;
use strict;
use warnings;

use LWP::UserAgent;
use XML::LibXML;
use URI::Escape;
use Encode qw(encode_utf8);
use HTML::Entities;

my $wolfram_api = $mod_perl::config::conf{wolfram_api_url} || 
'https://api.wolframalpha.com/v2/query?format=plaintext&appid=__KEY__&input=';

mod_perl::commands::register_command('wa', \&wolfram_run);
mod_perl::commands::register_command('wolfram', \&wolfram_run);

my $XML_PARSER;
sub wolfram_getXMLdoc
{
	my ($irc, $source) = @_;
	if (!$XML_PARSER) {
		$XML_PARSER = XML::LibXML->new;
	}
	my $ua = LWP::UserAgent->new;
	my $r = $ua->get($source);

	my $doc = undef;
	if ($r->is_success) {
#		print STDERR "debug:".$r->decoded_content."\n";
		$doc = $XML_PARSER->load_xml(string => $r->decoded_content);
	} else {
		$irc->say("[error] Couldn't retrieve '$source': ".$r->status_line);
	}

	return $doc;
}

sub wolfram_help {
	my $irc = shift;
    $irc->say("usage: wa <query>");
    $irc->say("-> Query Wolfram Alpha API service");
    return;
}

sub lookup {
    my ($irc, $query) = @_;
	my $max_results = 5;
    my $ua = LWP::UserAgent->new;
	my $key = $mod_perl::config::conf{wolfram_api_key};
	$wolfram_api =~ s/__KEY__/$key/;

	my $doc = wolfram_getXMLdoc($irc, $wolfram_api . uri_escape($query));
	return if (!$doc);

	my @pods = $doc->findnodes('//pod');
	my $count = 0;
	my $total_message = '';
	foreach my $pod (@pods) {
		foreach my $sub ($pod->findnodes('subpod')) {
			my $message = '';
			if ($pod->getAttribute('title')) { 
				$message = "[".$pod->getAttribute('title')."]";
			}
			# Collapse newlines one line
			my $result = $sub->find("plaintext")->to_literal;
			$result =~ s/\n+/ | /g;
			$message .= ' '. $result;
			$total_message .= $message . "\n";
		}
	}

	# Print message line by line
	my @lines = split(/\n/, $total_message);
	foreach my $line (@lines[0 .. ($max_results - 1)]) {
		$irc->say($line);
	}
	if (@lines > $max_results) {
		my $url = mod_perl::modules::utils::upload_content(encode_utf8($total_message));
		$irc->say($url);
	}


	if (!@pods) {
		my @didyoumean = $doc->findnodes('//didyoumean');
		my $error = 'No results.';
		if (@didyoumean) { 
			$error = join(" ", "Did you mean:", join(", ", map { $_->to_literal } @didyoumean), "?"); 
		}

		$irc->say($error);
		return;
	}
}

sub wolfram_run {
    my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $irc, \&wolfram_help, $arg, qw(help|h));
    return if (!$ret);

	if (!$arg) {
		wolfram_help($irc);
		return;
	}

	lookup($irc, $arg);
}

1;
