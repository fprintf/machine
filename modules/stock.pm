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

sub stock_help {
	my $irc = shift;
	$irc->say("usage: stock [--lookup|-l] <symbol> [symbol ...]");
	$irc->say("-> Enter stock symbol to look up (can enter multiple separated by space)");
	$irc->say("-> 'lookup' Lookup the stock symbol for a company");
	return;
}

sub yql_query {
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

sub symbol_lookup {
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


# Compare 2 differences and return a color to use for the given $diff
# The closer $diff is to $comparediff (or greater than it) determines the color chosen
# as seen in the @colors table below
### $diff - the value being compared
### $comparediff - the value to compare against.
### Returns an IRC color
sub color_for {
	my ($diff, $comparediff) = @_;
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

# Given a color, fmt/value and args return a colorized version
### $color - IRC color to use (00,00 format e.g.: 01,02 13 00,04))
### $fmt - passed to sprintf() first then colorized
### @args - args for use of with $fmt
sub colorize {
	my ($color, $fmt, @args) = @_;
	my $value = sprintf($fmt, @args);
	my ($fg, $bg) = split(/[,:\.]/, $color);

	if ($bg) {
		$value = sprintf("\003%02d,%02d%s\003", $fg, $bg, $value);
	} else {
		$value = sprintf("\003%02d%s\003", $fg, $value);
	}
	return $value;
}

sub stock_display {
	my (%v) = @_;
	my $display = "$v{symbol} a:$v{ask} b:$v{bid} lt:$v{last_trade} $v{change}($v{change_pct}) y:$v{year_low} => $v{year_high} P/E:$v{pe} PE/G:$v{peg}";
	return $display;
}

sub run {
    my ($msg, $arg) = @_;
    my %opt = ();
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, $msg, \&stock_help, $arg, qw(lookup|l help|h));
    return if (!$ret);

	if ($opt{lookup}) {
		symbol_lookup($msg, @argv);
		return;
	}

	my $symbols = join(', ', map { $_=uc("\"$_\""); } @argv);
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
	foreach my $s (@data) {
		my $ask = $s->{AskRealtime} // $s->{Ask} // $s->{LastTradePriceOnly};
		# We didn't find any informatoin, ignore it.
		next if (!$ask);

#		use Data::Dumper;
#		print STDERR Dumper($s), "\n";

		# More data we'll use for our view
		my $last_trade = $s->{LastTradePriceOnly};
		my $bid = $s->{BidRealTime} // $s->{Bid};
		my $year_low = $s->{YearLow};
		my $year_high = $s->{YearHigh};
		my $year_diff = $year_high - $year_low;


		# Build our display model
		# diff values against year_low and compare to year_high - year_low diff
		my (@day_range) = $s->{DaysRange} =~ /([\d\.]+)/g;
		my %view = (
			symbol => colorize(14, "%-6s", $s->{symbol}),
			change => colorize($s->{Change} > 0.0 ? 9 : 4, "%-+6.2f", $s->{Change}),
			change_pct => colorize($s->{Change} > 0.0 ? 9 : 4, "%-+.2f%%", ($s->{Change} / $last_trade) * 100),
			ask => colorize(color_for($ask - $year_low, $year_diff), "%7.2f", $ask),
			bid => colorize(color_for($bid - $year_low, $year_diff), "%7.2f", $bid),
			last_trade => colorize(color_for($last_trade - $year_low, $year_diff), "%7.2f", $last_trade),
			year_low => colorize(4, "%.2f", $year_low),
			year_high => colorize(9, "%.2f", $year_high),
			day_low => colorize(color_for($day_range[0] - $year_low, $year_diff), "%.2f", $day_range[0]),
			day_high => colorize(color_for($day_range[1] - $year_low, $year_diff), "%.2f", $day_range[1]),
			pe => $s->{PERatio} ? sprintf("%.2f", $s->{PERatio}) : "N/A",
			peg => $s->{PEGRatio} ? sprintf("%.2f", $s->{PEGRatio}) : "N/A",
		);

		# Display output (saved in variable for debugging purposes )
		my $output = stock_display(%view);
		$msg->say($output);
	}
}

1;
