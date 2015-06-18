package mod_perl::config;

use strict;
use warnings;

my $config_path = '/home/dead/dev/cprog/newbot/cprog/cbot/cbot.conf';
my %conf;


BEGIN: {
	load_config();
};


sub load_config
{
	do $config_path;
}

1;
