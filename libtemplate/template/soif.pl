#-*-perl-*-
#
#  soif.pl - Processing for the SOIF format.
#
#  Darren Hardy, hardy@cs.colorado.edu, January 1995
#
#  $Id: soif.pl,v 1.1 1999/11/03 21:41:04 golda Exp $
#
#######################################################################
#  Usage:
#
#    require 'soif.pl';
#    
#    $soif'input = 'WHATEVER'; 	# defaults to STDIN
#    ($ttype, $url, %SOIF) = &soif'parse();
#    foreach $k (sort keys %SOIF) {
#        print "KEY: $k\n";
#        print "DATA: $SOIF{$k}\n";
#    }
#    exit(0);
#    
#######################################################################
#  Copyright (c) 1994, 1995.  All rights reserved.
#  
#          Mic Bowman of Transarc Corporation.
#          Peter Danzig of the University of Southern California.
#          Darren R. Hardy of the University of Colorado at Boulder.
#          Udi Manber of the University of Arizona.
#          Michael F. Schwartz of the University of Colorado at Boulder. 
#
package soif;

$soif'debug = 0;
$soif'input = 'STDIN';
$soif'output = 'STDOUT';
$soif'sort_on_output = 1;

#
#  soif'parse - $soif'input is the file descriptor from which to read SOIF.
#  	        Returns an associative array containing the SOIF,
#		the template type, and the URL.
#
sub soif'parse {
	print "Inside soif'parse.\n" if ($soif'debug);

        return () if (eof($soif'input));       # DW
	local($template_type) = "UNKNOWN";
	local($url) = "UNKNOWN";
	local(%SOIF);
	undef %SOIF;

	while (<$soif'input>) {
		print "READING input line: $_\n" if ($soif'debug);
		last if (/^\@\S+\s*{\s*\S+\s*$/o);
	}
	if (/^\@(\S+)\s*{\s*(\S+)\s*$/o) {
		$template_type = $1, $url = $2 
	} else {
		return ($template_type, $url, %SOIF);	# done
	}

	while (<$soif'input>) {
                if (/^\s*([^{]+){(\d+)}:\t(.*\n)/o) {
			$attr = $1;
			$vsize = $2;
			$value = $3;
			if (length($value) < $vsize) {
				$nleft = $vsize - length($value);
				$end_value = "";
				$x = read($soif'input, $end_value, $nleft);
				die "Cannot read $nleft bytes: $!" 
					if ($x != $nleft);
				$value .= $end_value;
				undef $end_value;
			}
			chop($SOIF{$attr} = $value);
			next;
		} 
		last if (/^}/o);
	}

	return ($template_type, $url, %SOIF);
}

#
#  soif'print - $soif'output is the file descriptor to write SOIF.
#
sub soif'print {
	print "Inside soif'print.\n" if ($soif'debug);
	local($template_type, $url, %SOIF) = @_;

	# Write SOIF header, body, and trailer
	print $soif'output "\@$template_type { $url\n";
	if ($soif'sort_on_output) {
		foreach $k (sort keys %SOIF) {
			next if (length($SOIF{$k}) < 1);
			&soif'print_item($k, $SOIF{$k});
		}
	} else {
		foreach $k (keys %SOIF) {
			next if (length($SOIF{$k}) < 1);
			&soif'print_item($k, $SOIF{$k});
		}
	}
	print $soif'output "}\n";
}

sub soif'print_item {
	local($k, $v) = @_;
	print $soif'output "$k" , "{", length($v), "}:\t";
	print $soif'output $v, "\n";
}
1;
