package mod_perl::modules::stock;
use mod_perl::commands;

use JSON::XS;
use LWP::UserAgent;
use URI::Escape;

our $UserAgent;
my $api_base = 'http://query.yahooapis.com/v1/public/yql?q=';
my $symbol_api_base = "http://autoc.finance.yahoo.com/autoc?query=";


# Register our stock command
mod_perl::commands::register_command('stock', \&run);

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
	my $api_params = "&region=US&lang=en";

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

	my $func = sub {
		my ($value, $diff, $comparediff) = @_;
		my @colors = (
			{ ratio => .85, color => 9 }, # .85 or higher
			{ ratio => .70, color => 11 },
			{ ratio => .40, color => 0 }, # neutral (about the average)
			{ ratio => .25, color => 8 },
			{ ratio => .15, color => 4 }, # .25 - .15  (and also any less than this, will use this color also)
		);

		my $chosen = $colors[$#colors]->{color}; # default to the last possible color (worst)
		foreach my $color (@colors) {
			if ($diff >= $color->{ratio} * $comparediff) {
				$chosen = $color->{color};
				last;
			}
		}

		return $chosen;
	};

	# Display the results
	my $fmt = "\003%02d%-6s \003%02d%7.2f \003%02d%-+6.2f\003 Year[%.2f - %.2f] %s";
	foreach my $s (@data) {
		my $price = $s->{AskRealtime} || $s->{Ask} || $s->{LastTradePriceOnly};
		next if (!$price);

		my $change_color = $s->{Change} > 0.0 ? 9 : 4;
		my $symbol_color = 14;
		my $price_color = $func->($price, $price - $s->{YearLow}, $s->{YearHigh} - $s->{YearLow});
		my (@day_range) = $s->{DaysRange} =~ /([\d\.]+)/g;
		my $day_lowcolor = $func->($day_range[0], $day_range[0] - $s->{YearLow}, $s->{YearHigh} - $s->{YearLow});
		my $day_highcolor = $func->($day_range[1], $day_range[1] - $s->{YearLow}, $s->{YearHigh} - $s->{YearLow});
		$msg->say(sprintf($fmt, 
				$symbol_color,
				uc($s->{symbol}), 
				$price_color,
				$price,
				$change_color,
				$s->{Change},
				$s->{YearLow},
				$s->{YearHigh},
				$s->{DaysRange} ? sprintf("Day[\003%02d%.2f\003 - \003%02d%.2f\003]", $day_lowcolor, $day_range[0], $day_highcolor, $day_range[1]) : ''
			)
		);
	}
}

1;
