#!/usr/bin/env bash
set -ueo pipefail

TEST=bin/gcc-4.6/release/threading-multi/fs_test

function dump() {
  file=$1
  set +eo pipefail
  $TEST dump 3 $file | head
}

testdir=test
mkdir -p $testdir

# Create a test stream
echo >&2 "Generating dummy test stream"
$TEST gen 3 $testdir/ngrams.bin
dump $testdir/ngrams.bin

echo >&2 "Sorting in place..."
$TEST sort 3 $testdir/ngrams.bin
dump $testdir/ngrams.bin

echo >&2 "Counting..."
$TEST count 3 $testdir/counts.bin < $testdir/ngrams.bin
dump $testdir/counts.bin_counts

# TODO: Adjusting not yet implemented
echo >&2 "Adjusting Counts..."
#$TEST adjust 3 $testdir/adjusted.bin < $testdir/counts.bin_counts
#dump $testdir/adjusted.bin
