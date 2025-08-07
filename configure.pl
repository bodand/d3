#!/usr/bin/env perl
# Usage:
#   ./configure.pl
# 
# Note that you should not need to run this file manually.

use v5.36;
use warnings;
use strict;
use utf8;

use File::Which qw/which/;
sub check_library;

open my $out, ">", "config.mk"
	or die "cannot open config.mk: $!";
say $out <<EOF;
# d3 project configuration #
# This file is automagically generated, do not modify... # 
# ... or do, but I won't care about overwriting you changes the same way you #
# didn't care about this warning. #
EOF

print "checking if you have pkgconf... ";
my $pkgconf = which "pkgconf";
if ($pkgconf) {
	say $pkgconf
}
elsif (($pkgconf = which "pkg-config")) {
	say "$pkgconf, well, just pkg-config... will make do"
}
else {
	$pkgconf = !!0;
	say "no, skipping pkgconf lookups"
}

print "checking for asciidoctor... ";
my $asciidoctor = which "asciidoctor";
say $asciidoctor;
say $out qq,ASCIIDOCTOR_EXE = "$asciidoctor",;

check_library "libcurl", "LIBCURL";

# Impl #
sub check_library {
	my ($name, $env) = @_;
	die "missing library name" unless defined $name;
	die "missing env config name for $name" unless defined $env;

	print "finding CFLAGS for $name... ";
	say &get_cflags($name, $env);

	print "finding LIBS for $name... ";
	say &get_libs($name, $env);
}

sub get_cflags {
	my ($name, $env) = @_;

	if (defined $ENV{"${env}_CFLAGS"}) {
		say $out qq/CFG_${env}_CFLAGS = $ENV{"${env}_CFLAGS"}/;
		return "ok(env)"
	}

	return "maybe? (no pkgconf, nor manual config)" unless ($pkgconf);

	my $cflags = qx/$pkgconf --cflags $name/;
	if ($? == 0) {
		say $out qq/CFG_${env}_CFLAGS = $cflags/;
		return "ok(pkgconf)"
	}

	return "bad? (pkgconf returned $?)"
}

sub get_libs {
	my ($name, $env) = @_;

	if (defined $ENV{"${env}_LIBS"}) {
		say $out qq/CFG_${env}_LIBS = $ENV{"${env}_LIBS"}/;
		return "ok(env)"
	}

	return "bad? (no pkgconf, nor manual config)" unless ($pkgconf);

	my $libs = qx/$pkgconf --libs $name/;
	if ($? == 0) {
		say $out qq/CFG_${env}_LIBS = $libs/;
		return "ok(pkgconf)"
	}

	return "bad? (pkgconf returned $?)"
}

