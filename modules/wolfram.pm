package mod_perl::modules::wolfram;

use mod_perl::commands;
use mod_perl::config;
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
		print STDERR "debug:".$r->decoded_content."\n";
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
    my $ua = LWP::UserAgent->new;
	my $key = $mod_perl::config::conf{wolfram_api_key};
	$wolfram_api =~ s/__KEY__/$key/;

	my $doc = wolfram_getXMLdoc($irc, $wolfram_api . uri_escape($query));
	return if (!$doc);

	my @pods = $doc->findnodes('//pod[@primary="true"]');
	unshift(@pods, $doc->findnodes('//pod[@title="Input"]'));
	unshift(@pods, $doc->findnodes('//pod[@title="Input interpretation"]'));
	foreach my $pod (@pods) {
		my $message;
		if ($pod->getAttribute('title')) { 
			$message = "[WA] ". $pod->getAttribute('title');
		}
		my $result = $pod->find('subpod//plaintext')->to_literal;
		# Collapse newlines one line
		$result =~ s/\n+/    /g;
		$message .= ' || '. $result;
		$irc->say($message);
	}

	if (!@pods) {
		my @didyoumean = $doc->findnodes('//didyoumean');
		my $error = 'No results.';
		if (@didyoumean) { 
			$error = join(" ", "Did you mean:", $didyoumean[0]->to_literal, "?"); 
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
