package mod_perl::modules::utils;

#########################################################################
# Utility functions for the various modules
#########################################################################

use strict;
use warnings;

use LWP::UserAgent;
use WWW::Shorten 'TinyURL', ':short';
use Try::Tiny;

use constant MIN_LENGTH => 25;

=pod

=head2 tinyurl

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

	# Don't bother shortening if it's already this short
    if (length($uri) < MIN_LENGTH) {
        return $uri;
    }
	my $short_url = $uri;
	try {
		$short_url = short_link($uri);
	} catch { 
		warn "Failed to shorten $uri: $_";
	};
	return $short_url;
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
	my $ua = LWP::UserAgent->new;
	my $host = 'http://paste.drains.me';

	my $res = $ua->post($host, 'Content-Type' => 'form-data', 'Content' => [ f => $content ]);
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
