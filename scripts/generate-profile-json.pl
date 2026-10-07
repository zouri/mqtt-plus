#!/usr/bin/env perl
use strict;
use warnings;
use JSON::PP;
use Time::HiRes qw(clock_gettime CLOCK_MONOTONIC sleep);

my ($count, $bytes) = @ARGV;
die "Usage: generate-profile-json.pl COUNT PAYLOAD_BYTES\n"
    unless @ARGV == 2 && $count =~ /^[1-9][0-9]*$/ && $count <= 5000
        && $bytes =~ /^[1-9][0-9]*$/ && $bytes <= 1048576;
my $json = JSON::PP->new->canonical;
my $template = {sequence => '0000000000', value => ''};
my $body_bytes = $bytes - length($json->encode($template));
die "Payload size is too small for the sequence envelope\n" if $body_bytes < 0;
$template->{value} = 'x' x $body_bytes;
$| = 1;
my $start = clock_gettime(CLOCK_MONOTONIC);
for (my $index = 0; $index < $count; ++$index) {
    # Absolute pacing avoids cumulative per-message sleep drift.
    my $remaining = $start + $index / 100 - clock_gettime(CLOCK_MONOTONIC);
    sleep($remaining) if $remaining > 0;
    $template->{sequence} = sprintf('%010d', $index);
    print $json->encode($template), "\n" or die "Cannot write fixture stream: $!\n";
}
printf STDERR "PROFILE_PUBLISHER generated=%d elapsedSeconds=%.3f payloadBytes=%d\n",
    $count, clock_gettime(CLOCK_MONOTONIC) - $start, $bytes;
