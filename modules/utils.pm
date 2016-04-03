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

=pod

=head2 upload_content

This function takes the given content and uploads it to a
pastebin site (it must be text only content). It then
returns the url it got from uploading the text.

If multiple values are passed they are concatenated together and uploaded as one string.

=over 12

=item B<content>

The content to upload, stored as string.

=back

=cut

sub upload_content {
	my $content = shift;
	my $host = 'http://sprunge.us';
	my $ua = LWP::UserAgent->new;

	my $res = $ua->post($host, 'Content-Type' => 'form-data', 'Content' => [ sprunge => $content ]);
	my $url;
	if ($res->is_success()) {
		chomp($url = $res->decoded_content());
	} else {
		print STDERR "Failed to post [$content]: ".$res->status_line."\n";
		return undef;
	}
		
	return $url;
}

1;
