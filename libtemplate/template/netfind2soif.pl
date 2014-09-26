: # *-*-perl-*-*
    eval 'exec perl -S $0 "$@"'
    if $running_under_some_shell;  
#
#  netfind2soif.pl - Converts the Netfind seed database into a SOIF stream.
#		     Groups the SOIF templates by second-level domains.
#
#  Darren Hardy, hardy@cs.colorado.edu, January 1995
#
#  $Id: netfind2soif.pl,v 1.1 1999/11/03 21:41:04 golda Exp $
#
#  The netfind seed database has records that look like this:
#
#	%D domain-name
#	%O organization-name
#	%H hostnames
#
#  Generates SOIF that looks like this:
#
#	@DOMAIN { nop:domain-name
#	Embed<x>-Domain{x}:	foo.domain-name
#	Embed<x>-Organization{x}:	organization-name
#	Embed<x>-Hosts{x}:	hosts
#	Embed<y>-Domain{x}:	bar.domain-name
#	Embed<y>-Organization{x}:	organization-name
#	Embed<y>-Hosts{x}:	hosts
#	}
#  or
#	@DOMAIN { nop:domain-name
#	Domain{x}:	domain-name
#	Organization{x}:	organization-name
#	Hosts{x}:	hosts
#	}
#
$ENV{'HARVEST_HOME'} = "/usr/local/harvest" if (!defined($ENV{'HARVEST_HOME'}));
unshift(@INC, "$ENV{'HARVEST_HOME'}/lib");      # use local files 
$ENV{'TMPDIR'} = "/tmp" if (!defined($ENV{'TMPDIR'}));

require 'soif.pl';

$sld_file = "$ENV{'TMPDIR'}/nfdomains.$$";
$slo_file = "$ENV{'TMPDIR'}/nforgs.$$";
$slh_file = "$ENV{'TMPDIR'}/nfhosts.$$";


&do_reset();
while (<>) {
	chop;
	$d = $1, next if (/^%D\s*(.*)$/io);
	$o = $1, next if (/^%O\s*(.*)$/io);
	$h = $1, next if (/^%H\s*(.*)$/io);
	if (defined($d) && defined($o) && defined($h)) {
		if ($d =~ /^(\d+)\.(\d+)\.(\d+)$/o) {
			$sld = "$1.$2";
		} elsif ($d =~ /^([+\-\w]+\.[+\-\w]+)$/o) {
			$sld = $1;
		} elsif ($d =~ /^.*\.([+\-\w]+\.[+\-\w]+)$/o) {
			$sld = $1;
		} else {
			next;
		}
		$sl_domain{$sld} .= "$d\n";
		if ($o eq "") {
			$sl_org{$sld} .= "unspecified\n";
		} else {
			$sl_org{$sld} .= "$o\n";
		}
		if ($h eq "") {
			$sl_host{$sld} .= "unspecified\n";
		} else {
			$sl_host{$sld} .= "$h\n";
		}
		&do_reset();
	}
}

while (($key, $value) = each %sl_domain) {
	@domains = split(/\n/, $sl_domain{$key});
	@orgs = split(/\n/, $sl_org{$key});
	@hosts = split(/\n/, $sl_host{$key});

	undef %record;
	delete $sl_domain{$key};
	delete $sl_org{$key};
	delete $sl_host{$key};

	if ($#domains < 0 || $#orgs < 0 || $#hosts < 0) {
		print STDERR "Empty Record!\n";
		next;
	}
	$n = $#domains + 1;
	if ($n == 1) {
		$record{"Domain"} = $domains[0];
		$record{"Organization"} = $orgs[0]
			if ($orgs[0] ne "unspecified");
		$record{"Hosts"} = $hosts[0]
			if ($hosts[0] ne "unspecified");
	} else {
		for ($i = 1; $i <= $n; $i++) {
			$record{"Embed<$i>-Domain"} = $domains[$i-1];
			$record{"Embed<$i>-Organization"} = $orgs[$i-1] 
				if ($orgs[$i-1] ne "unspecified");
			$record{"Embed<$i>-Hosts"} = $hosts[$i-1]
				if ($hosts[$i-1] ne "unspecified");
		}
	}
	&soif'print("DOMAIN", "nop:$key", %record);
}


exit(0);

sub do_reset {
	undef $d;
	undef $o;
	undef $h;
	undef $sld;
}
