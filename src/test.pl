use strict;
use warnings;
use LWP::UserAgent;

sub upload_content {
    my $content = join("", @_);
    my $host = 'http://sprunge.us/';
    my $ua = LWP::UserAgent->new;

	print "Content: $content\n";
    my $res = $ua->post($host, 'Content-Type' => 'form-data', Content => [ 'sprunge' => $content ]);
    my $url;
    if ($res->is_success()) {
        chomp($url = $res->decoded_content());
    } else {
        print STDERR "Failed to post [$content]: ".$res->status_line."\n";
        return undef;
    }

    return $url;
}

print upload_content("testing this crap");
