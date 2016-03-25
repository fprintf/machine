package mod_perl::config;

use strict;
use warnings;

my %conf;
my @config_search_list = ('/etc', '~/.cbot', '~/');


BEGIN: {
	load_config();
};

sub find_config_path {
	my $conf_name = 'cbot.conf';

	foreach my $path (@config_search_list) {
		my $full_name = join("/", $path, $conf_name);
		if (-e $full_name) {
			return $full_name;
		}
	}

	return undef;
}

sub load_config
{
	my $path = find_config_path();
	
	if (!$path) {
		die "Unable to locate config file in ".join(", ", @config_search_list)."\n";
	}

	do $path;
}

1;
