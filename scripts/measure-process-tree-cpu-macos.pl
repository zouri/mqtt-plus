#!/usr/bin/env perl
use strict;
use warnings;
use Time::HiRes qw(clock_gettime CLOCK_MONOTONIC sleep);
use JSON::PP qw(encode_json);

sub parse_snapshot {
    my ($text) = @_;
    my %processes;
    for my $line (split /\n/, $text) {
        next if $line =~ /^\s*$/;
        $line =~ /^\s*(\d+)\s+(\d+)\s+(\S+\s+\S+\s+\d+\s+\S+\s+\d+)\s+(\S+)\s*$/
            or die "Malformed process snapshot\n";
        my ($pid, $parent, $identity, $time) = ($1, $2, $3, $4);
        my @parts = split /:/, $time;
        die "Malformed CPU time\n" unless @parts == 2 || @parts == 3;
        die "Malformed CPU time\n" if grep { !/^\d+(?:\.\d+)?$/ } @parts;
        my $seconds = pop @parts;
        $seconds += 60 * pop @parts;
        $seconds += 3600 * pop @parts if @parts;
        die "Duplicate process ID\n" if exists $processes{$pid};
        $processes{$pid} = {parent => 0 + $parent, identity => $identity, cpu => $seconds};
    }
    return \%processes;
}

sub tree {
    my ($processes, $root) = @_;
    die "Root process exited\n" unless exists $processes->{$root};
    my %selected = ($root => $processes->{$root});
    my $changed = 1;
    while ($changed) {
        $changed = 0;
        for my $pid (keys %$processes) {
            if (!exists $selected{$pid} && exists $selected{$processes->{$pid}{parent}}) {
                $selected{$pid} = $processes->{$pid};
                $changed = 1;
            }
        }
    }
    return \%selected;
}

sub assert_stable {
    my ($before, $after) = @_;
    die "Process tree changed; measurement is incomplete\n"
        unless join(',', sort {$a <=> $b} keys %$before) eq join(',', sort {$a <=> $b} keys %$after);
    for my $pid (keys %$before) {
        die "Process identity changed; measurement is incomplete\n"
            unless $before->{$pid}{identity} eq $after->{$pid}{identity}
                && $before->{$pid}{parent} == $after->{$pid}{parent};
        die "CPU counter moved backwards\n" if $after->{$pid}{cpu} < $before->{$pid}{cpu};
    }
}

if (@ARGV == 1 && $ARGV[0] eq '--self-test') {
    my $fixture = "10 1 Thu Oct 8 12:00:00 2026 1:02.50\n"
        . "11 10 Thu Oct 8 12:00:01 2026 0:01.25\n"
        . "12 11 Thu Oct 8 12:00:02 2026 1:00:03.00\n"
        . "20 1 Thu Oct 8 12:00:00 2026 9:00.00\n";
    my $parsed = parse_snapshot($fixture);
    my $selected = tree($parsed, 10);
    die "Tree selection failed\n" unless keys(%$selected) == 3 && !exists $selected->{20};
    die "Time parsing failed\n" unless $selected->{10}{cpu} == 62.5 && $selected->{12}{cpu} == 3603;
    assert_stable($selected, $selected);
    for my $mutation ('exit', 'reuse', 'counter', 'parent', 'spawn') {
        my %copy = map { $_ => {%{$selected->{$_}}} } keys %$selected;
        delete $copy{11} if $mutation eq 'exit';
        $copy{11}{identity} = 'different' if $mutation eq 'reuse';
        $copy{11}{cpu} = 0 if $mutation eq 'counter';
        $copy{11}{parent} = 1 if $mutation eq 'parent';
        $copy{13} = {%{$copy{11}}} if $mutation eq 'spawn';
        eval { assert_stable($selected, \%copy); };
        die "Did not reject $mutation\n" unless $@;
    }
    eval { tree($parsed, 999); };
    die "Did not reject missing root\n" unless $@;
    for my $invalid ('invalid', "$fixture$fixture", '10 1 Thu Oct 8 12:00:00 2026 bad') {
        eval { parse_snapshot($invalid); };
        die "Did not reject malformed snapshot\n" unless $@;
    }
    print "Process-tree CPU self-test PASS\n";
    exit 0;
}

die "Usage (macOS): perl scripts/measure-process-tree-cpu-macos.pl PID [seconds=30]\n"
    unless $^O eq 'darwin' && @ARGV >= 1 && @ARGV <= 2;
my ($root, $duration) = ($ARGV[0], $ARGV[1] // 30);
die "PID and duration must be positive integers (duration <= 300)\n"
    unless $root =~ /^[1-9]\d*$/ && $duration =~ /^[1-9]\d*$/ && $duration <= 300;

sub snapshot {
    my $start = clock_gettime(CLOCK_MONOTONIC);
    local $ENV{LC_ALL} = 'C';
    open my $ps, '-|', '/bin/ps', '-axo', 'pid=,ppid=,lstart=,time=' or die "Cannot start ps: $!\n";
    local $/;
    my $text = <$ps>;
    close $ps or die "Cannot read process snapshot\n";
    my $end = clock_gettime(CLOCK_MONOTONIC);
    return (tree(parse_snapshot($text), $root), ($start + $end) / 2, $end - $start);
}

my ($before, $start, $max_snapshot_seconds) = snapshot();
my $previous = $before;
my ($after, $end);
while (1) {
    my $remaining = $duration - (clock_gettime(CLOCK_MONOTONIC) - $start);
    sleep($remaining < 1 ? $remaining : 1) if $remaining > 0;
    my $cost;
    ($after, $end, $cost) = snapshot();
    $max_snapshot_seconds = $cost if $cost > $max_snapshot_seconds;
    assert_stable($previous, $after);
    $previous = $after;
    last if $end - $start >= $duration;
}
my @processes = map {
    {pid => 0 + $_, cpu_seconds => $after->{$_}{cpu} - $before->{$_}{cpu}}
} sort {$a <=> $b} keys %$before;
my $total = 0;
$total += $_->{cpu_seconds} for @processes;
print encode_json({cpu_percent => 100 * $total / ($end - $start),
    elapsed_seconds => $end - $start, cpu_seconds => $total,
    process_count => scalar @processes, processes => \@processes,
    max_snapshot_seconds => $max_snapshot_seconds,
    unit => '100 percent = one core', stable_observed_tree => JSON::PP::true,
    limitation => 'Children that both spawn and exit between snapshots are not observable'}), "\n";
