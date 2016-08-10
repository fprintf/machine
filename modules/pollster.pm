package mod_perl::modules::pollster;

use mod_perl::commands;
use mod_perl::modules::utils;

use LWP::UserAgent;
use JSON::XS;
use Date::Format;
use Date::Parse;

my $pollster_api_base_url = 'http://elections.huffingtonpost.com/pollster/api';
my $output_limit = 350;
my $date_format = '%Y-%m-%d %H:%M:%S %z';

#####################################
# (INIT) Run when the module is first included
#####################################
mod_perl::commands::register_command('polls', \&run);
mod_perl::commands::register_command('poll', \&run);


#####################################
# Functions
#####################################
sub pretty_time {
	my ($timestr) = @_;

	my $time = str2time($timestr);
	my $elapsed = time - $time;

	my $pretty = $timestr;
	if ($elapsed < 120) {
		$pretty = 'less than a minute ago';
	} elsif($elapsed <= 3600 * 2) {
		$pretty = 'less than an hour ago';
	} elsif ($elapsed <= 3600 * 8) {
		$pretty = 'several hours ago';
	} elsif ($elapsed <= 86400) {
		$pretty = 'today';
	} elsif ($elapsed <= 86400 * 2) {
		$pretty = 'yesterday';
	} else {
		$pretty = time2str($date_format, $time);
	}
	
	return $pretty;
}


sub api_call {
	my ($query, %opt) = @_;
	my $call = sprintf("%s/%s", $pollster_api_base_url, $query);

	my $ua = LWP::UserAgent->new();
	my $res = $ua->get($call);

	my $json;
	if ($res->is_success) {
		# Croaks on error
		$json = decode_json($res->decoded_content);
	} else {
		die $res->status_line;
	}

#use Data::Dumper;
#	print STDERR Dumper($json), "\n";
	return $json;
}

sub list_objects {
	my ($type, %args) = @_;

	my %type_map = (
		'questions' => sub {
			my (%opt) = @_;
			my $json = $opt{json};
			my $poll_index = $opt{poll_number};
			my %questions = map { $_->{name} => 1 } @{$json->[$poll_index]->{questions}};
			return \@questions;
		},
		'polls' => sub {
		}
	);

	my $data;

	if ($type_map{$type}) {
		$data = $type_map{$type}->(%args);
	} else {
		my $query = $type =~ /(chart|topic)/ ? 'charts.json' : 'polls.json?sort=updated';
		my $json = api_call($query);

		# Map the data to an arrayref, rename index names correctly
		my %alias_map = ( 'chart' => 'slug', 'poll' => 'pollster' );
		foreach my $entry (@$json) {
			my $name = $entry->{$alias_map{$type} || $type};
			next if (!$name);
			$data->{$name} = 1;
		}
	}

	return $data;
}

sub chart {
	my ($slug) = @_;
	# These are IRC colors (will be used sequentially)
	my @colors = (8, 11, 3, 12, 4, 13, 2);
	# Display width of the chart 
	my $display_width = 40;

	my $json = api_call(join('', 'charts/', $slug, ".json"));
	my @estimates = @{$json->{estimates}};

	# If there were no current estimates, try to get the first previous estimate
	if (!@estimates) {
		if (exists($json->{estimates_by_date}) && scalar(@{$json->{estimates_by_date}})) {
			@estimates = @{$json->{estimates_by_date}->[0]->{estimates}};
		} else {
			die "There are currently no estimates past or present for $slug. Check back later\n";
		}
	}


	my @chart_lines;
	my $timestamp = pretty_time($json->{last_updated});
	push(@chart_lines, sprintf("(%d) %s [%s]", $json->{poll_count}, $json->{short_title} || $json->{title}, $timestamp));
	my $i = 0;
	foreach my $estimate (sort { $b->{lead_confidence} <=> $a->{lead_confidence} } @estimates) {
		my $width = int($estimate->{value} / 2.5 + 0.5); # width in blocks representing increments of 2.5 rounded up
		my $color = $colors[$i++ % scalar(@colors)];

		push(@chart_lines, 
			sprintf(" %-15.15s\003 |\003%02d,%02d%-$display_width.${display_width}s (%4.1f%%)",
				$estimate->{choice}, $color, $color, ' ' x $width . "\003", $estimate->{value}
			)
		);
	}
	push(@chart_lines, $json->{url}) if ($json->{url});

	return @chart_lines;
}

sub run {
	my ($irc, $arg) = @_;
    my %opt;
    my ($ret, @argv) = mod_perl::commands::handle_arg(\%opt, 
		$irc, undef, $arg, qw(list|l=s chart|c topic|t poll|polls|pollster|p help|h)
	);

	if (!$ret || $opt{help}) {
		$irc->say("[usage] .polls [-l <topic|chart|poll] [-ctp] <name>");
		$irc->say("-c  show chart by [name] (default)");
		$irc->say("-t  show available chart names in a given topic [name] (unimplemented currently)");
		$irc->say("-p  show results for poll by [name] (unimplemented currently)");
		return;
	}

	my %list_types = ( 'chart' => 1, 'topic' => 1, 'poll' => 1 );

	if ($opt{list} && !exists($list_types{lc($opt{list})})) {
		$irc->say("[error] '$opt{list}' is not supported, available list types are: 'topic', 'chart' and 'poll'");
		return;
	}

	if ($opt{topic} || $opt{poll}) {
		$irc->say("[error] currently unimplemented. Try -c instead.");
		return;
	}

	eval {
		# If we were asked for a list, get it and return, we're done.
		if ($opt{list}) {
			# There are shit ton of charts, so let's make sure
			# we only get charts for the current year.
			my $current_year = (localtime())[5] + 1900;

			my $data = list_objects($opt{list});
			my @results = sort keys %$data;
			my $output = "$opt{list}s: ".join(", ", @results);

			# Print the output in chunks or upload to a website if too long
			my @lines = $output =~ /.{$output_limit}/g;
			if (@lines >= 3) {
				$output = join("\n", @results), "\n";
				$irc->say(mod_perl::modules::utils::upload_content($output));
				return;
			}

			$irc->say($_) foreach (@lines);
			return;
		}


		# At this point they shouldn't be missing an argument
		if (!@argv) { return $irc->say("Please enter an object name."); }

		# Get the requested object and visualize it
		my @lines = chart($argv[0]);
		$irc->say($_) foreach (@lines);
	};

	if ($@) { chomp($@); $irc->say($@); }
}

1;
