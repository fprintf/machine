package mod_perl::commands::stock;
use mod_perl::commands;

use lib '/home/dead/perl5/lib/perl5/';
use JSON::XS;
use LWP::UserAgent;
use URI::Escape;

our $UserAgent;
my $api_base = 'http://query.yahooapis.com/v1/public/yql?q=';
my $symbol_api_base = "http://autoc.finance.yahoo.com/autoc?query=";


sub stock_help
{
	my $irc = shift;
	$irc->say("usage: stock [--lookup|-l] <symbol> [symbol ...]");
	$irc->say("-> Enter stock symbol to look up (can enter multiple separated by space)");
	$irc->say("-> 'lookup' Lookup the stock symbol for a company");
	return;
}

sub yql_query
{
	my ($msg, $source) = @_;

	if (!$UserAgent) {
		$UserAgent = LWP::UserAgent->new;
	}
	my $r = $UserAgent->get($source);

	my $json = undef;
	if ($r->is_success) {
		my $content = $r->decoded_content;
		# This is required to be stripped off of the Symbol lookup returned data
		# for some reason it's returned as an argument to a function call.. :/
		$content =~ s/^YAHOO.Finance.SymbolSuggest.ssCallback\(//;
		$content =~ s/\)$//;
		$json = decode_json $content;
	} else {
		$msg->say("[error] Couldn't retrieve '$source': ".$r->status_line);
	}

	return $json;
}

sub symbol_lookup
{
	my ($msg, @search) = @_;
	my $api_query = uri_escape(join(" ",@search));
	my $api_params = "&callback=YAHOO.Finance.SymbolSuggest.ssCallback";

	my $json = yql_query($msg,join('',$symbol_api_base,$api_query,$api_params));
	return if !$json;
	my $results = $json->{ResultSet}->{Result};
	foreach my $s (@$results) {
		$msg->say(sprintf("%-10s %30s %20s", $s->{symbol}, $s->{name}, $s->{exchDisp}));
	}
}

sub run
{
    my ($msg, $arg) = @_;
    my %opt = ();
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $msg, \&stock_help, $arg, qw(lookup|l help|h));
    return if (!$ret);

	if ($opt{lookup}) {
		symbol_lookup($msg, @argv);
		return;
	}

	my $symbols = join(', ', map { $_="\"$_\""; } @argv);
	my $api_query = uri_escape("select * from yahoo.finance.quotes where symbol in ($symbols)");
	my $api_params = "&env=http://datatables.org/alltables.env&format=json";
	my $json = yql_query($msg, join('',$api_base,$api_query,$api_params));
	return if !$json;

	# Report info for each symbol requested
	my $results = $json->{query}->{results}->{quote};
	my @data = ();
	if (ref($results) eq 'ARRAY') {
		@data = @$results;
	} elsif (ref($results) eq 'HASH') {
		push(@data, $results);
	} else {
		$msg->say( "[error] Invalid stock data returned");
		return;
	}

	# Display the results
	my $hfmt = "%-6s %7s %-6s %10s %-10s %15s";
	my $fmt = "%-6s %7.2f %-+6.2f %10.2f %-10.2f %15s";
	$msg->say(sprintf($hfmt, "symbol", "price", "change", "ylow", "yhigh", "drange"));
	foreach my $s (@data) {
		$msg->say(sprintf($fmt, "$s->{symbol}",$s->{LastTradePriceOnly},$s->{ChangeRealtime},$s->{YearLow},$s->{YearHigh},$s->{DaysRange}));
	}
}

1;
