package mod_perl::config;

use strict;
use warnings;

my $config_path = '/home/dead/machine/cbot.conf';
my %conf;


BEGIN: {
	load_config();
};


sub load_config
{
	do $config_path;
}

1;
