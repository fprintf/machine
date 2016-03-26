package mod_perl::config;

use strict;
use warnings;
use File::Basename;

my %conf;
my @config_search_list = ("$ENV{HOME}/.cbot", "$ENV{HOME}", "/etc");
my $conf_dir;
my $module_db;    # Path to the shared sqlite3 database that modules can create tables in
my @module_dirs;  # List of paths where modules should be loaded from (in sequence)

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

	# Set up our globals now
	$conf_dir = basename($path);
	$module_db = join("/", $conf_dir, "module_data.sqlite3");
	@module_dirs = (
		join("/", $conf_dir, "modules"), 
		"/etc/cbot/modules" 
	);

	do $path;
}

1;
