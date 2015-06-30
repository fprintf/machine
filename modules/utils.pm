package mod_perl::modules::utils;

#########################################################################
# Utility functions for the various modules
#########################################################################

use strict;
use warnings;

use LWP::UserAgent;

use constant MIN_LENGTH => 25;

=pod

=head2 tinurl

Takes a $url to shorten and an optional shortening site address
otherwise uses a random shortener from a builtin list.

Returns the shortened url or an error message. If the URL is shorter than
a specified default value (usually 25) it will just be returned unshortened.

=over 12

=item B<$url>

The URL to shorten

=item B<$site>

Full URL with query e.g: http://is.gd/create.php?format=simple&url=

=back

=cut

sub tinyurl {
    my ($uri) = @_;
    my @minimizers = (
		'https://is.gd/create.php?format=simple&url=',
		'https://v.gd/create.php?format=simple&url=',
	);
	my $ua = new LWP::UserAgent;
	# Don't bother shortening if it's already this short

    # Already short enough, don't bother
    if (length($uri) < MIN_LENGTH) {
        return $uri;
    }

    my $query = sprintf("%s%s", $minimizers[int(rand(@minimizers))], $uri);
	my $r = $ua->get($query);
    my $tinyurl = '';

	if ($r->is_success) {
        $tinyurl = $r->decoded_content();
	} else {
        print STDERR "failed to shorten: $uri: ".$r->status_line."\n";
		$tinyurl = "can't shorten";
	}

    return $tinyurl;
}

1;
