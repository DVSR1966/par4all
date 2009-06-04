#! /user/bin/perl -w
#
# $Id$
#
# Copyright 1989-2009 MINES ParisTech
#
# This file is part of PIPS.
#
# PIPS is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PIPS.  If not, see <http://www.gnu.org/licenses/>.
#

#
# nodeedge source destination
#
# routine pour construire un arc a partir du noeud source vers destination
#
sub nodeedge
{
    ($src,$dst) = @_;
#    print "\"$src->$dst\"\n";
    print "l(\"$src->$dst\",e(\"\",[],r(\"$src->$dst\")))\n";
}

#
# node src
#
sub node
{
    $src = $_[0];
    print "l(\"$src\",n(\"\",[a(\"OBJECT\",\"$src\"),a(\"COLOR\",\"red\")],[\n";
    undef %dejavu;
    $first=1;
    foreach $dst (@{$suivants{$src}})
    {
	unless ($dejavu{$dst}) { 
	     print "," unless $first;
	     $first = 0;
	     nodeedge($src,$dst); 
	}

	$dejavu{$dst} = 1;
    }
      print "]))\n";
}

#
# construction d un arc labelle.
#
sub edge
{
    $clef = $_[0];
    ($src, $dst) = split(/->/, $clef);
#    print "l(\"$clef\",n(\"\",[a(\"OBJECT\",\"$clef\")],[\n";

#    foreach $what (@{$infos{$clef}})
#    {
#	print "\"$clef\" what=$what\n";
#   }
    print "l(\"$clef\",n(\"\",[a(\"OBJECT\",\"";

   foreach $what (@{$infos{$clef}})
   {
	print "$what\\n";
   }
    print "\"),a(\"COLOR\",\"blue\")],[l(\"$clef->$dst\",e(\"\",[],r(\"$dst\" )))]))\n";
}

#
# MAIN
#
# lit les arguments.
# 

while (<>)
{
    chop;
    ($srcx, $dstx, $ef1, $ef2, $var1, $junk, $var2, $src, $dst, $junk2) = split(/ /);
    push @nodes, $dst;
    push @{$suivants{$src}}, $dst;
    push @{$infos{"$src->$dst"}}, "$ef1 $ef2 $var1 $var2";
}

undef %proccessed;

#
# creation des noeuds.
#
$first=1;
print "[";
foreach $nd (keys %suivants, @nodes)
{ 
    unless ($proccessed{$nd}) 
    {
	print "\," unless $first;
	node($nd);
	$proccessed{$nd} = 1;
	$first=0;
    }
}

#
# creation des arcs.
#

foreach $ed (keys %infos)
{
   print "\,";
   edge $ed;
}

print "]";
