#!/usr/bin/ruby

# Copyright 2003 by Adam Luter
# This file is part of Squash, a C/Ncurses-based unix music player.
#
# Squash is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Squash is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Squash; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

require 'find'

# collect_info.rb will take several info files found in
# a common directory tree, and create a consolidated info
# file and a corresponding index file.  (default.cinfo and
# default.cindex).  These can then be edited and
# de-cosolidated later (see edit_tag.rb and
# uncollect_info.rb).

$help = <<EOF;
Please run collect_info.rb with a list of directory names,
for each directory name, a consolidated info file will be
created, and a matching index file will also be created.
You may also specify these options:
    --prefix=name           set cinfo and cindex filename
                            prefixes to name (i.e. name.cinfo,
                            and name.cindex).  Defaults to
                            'default'.
    --noindex, -n           do not create index file(s).
    --verbose, -v           print lots of status information
EOF

extension_regex = Regexp.compile( /\.(mp3|ogg|flac)$/i )

verbose = false
noindex = false
noarg = false
prefix = "default"
dirlist = []

ARGV.each do |arg|
    if noarg == false
        if arg == "--"
            noarg = true
            next
        end
        if arg =~ /--prefix=(.*)/
            prefix = $1
        end
        if arg == "-n" or arg == "--noindex"
            noindex = true
            next
        end
        if arg == "-v" or arg == "--verbose"
            verbose = true
            next
        end
    end
    arg = File.expand_path( arg )
    if FileTest.directory?( arg )
        dirlist.push(arg)
    else
        $stderr.puts("'#{arg}' is not a directory or does not exist")
        exit
    end
end

if dirlist.empty?
    print $help
end

dirlist.each do |dir|
    cinfo_filename = File.join(dir, prefix+".cinfo")
    puts( "Creating: #{cinfo_filename}" ) if verbose
    cinfo = File.open( cinfo_filename, "w" )

    unless noindex
        cindex_filename = File.join(dir, prefix+".cindex")
        puts( "Creating: #{cindex_filename}" ) if verbose
        cindex = File.open( cindex_filename, "w" )
    end

    Find.find(dir) do |filename|
        next unless FileTest.file?( filename )
        next unless extension_regex =~ filename
        puts( "Processing: #{filename}" ) if verbose
        cindex.puts( filename ) unless noindex
        cinfo.puts( "=== #{filename}" )
        info_filename = filename+".info"
        next unless FileTest.file?( info_filename ) \
                and FileTest.readable?( info_filename )
        File.open( info_filename, "r" ).each do |line|
            cinfo.puts( line ) unless line =~ /^\s*$/
        end
    end
    cinfo.puts
    cinfo.close
    cindex.close unless noindex
end
