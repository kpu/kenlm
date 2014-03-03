#!/usr/bin/perl

use strict;

my $order = 1;
print "\\data\\\n";
foreach(@ARGV) {
    my ($c, @rest) = split(/\s+/, `gzip -dc $_ | wc -l`);
    print "ngram $order=$c\n", ;
    $order++;
}
print "\n";

$order = 1;
foreach(@ARGV) {
    print "\\$order-grams:\n";
    open(NGRAMS, "gzip -dc $_ | ");
    print while (<NGRAMS>);
    close(NGRAMS);
    print "\n";
    $order++;
}
print "\\end\\\n";