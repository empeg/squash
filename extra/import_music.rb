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
require 'tempfile'
require 'musicdb_lib.rb'

# import_info.rb will take a list of directories, and
# for each music file (mp3 or ogg) it will create a
# matching info file.  You can optionally only add new
# key's to existing info files.

$help = <<EOF;
Please run import_info.rb with a list of directory names,
for each directory name, all music files (mp3 or ogg) will be
processed, and their meta info will be extracted into
corresponding info files.
You may also specify these options:
    --additive, -a          only add new keys instead of over-
                            writing existing info files.
    --strict_additive, -s   like additive but adds unless both
                            the key and value exist.
                            (note that duplicate key value pairs
                            cannot be added with this option)
    --overwrite, -o         overwrite existing info files with
                            meta info found in the music files
    --passive, -p           this is the default, it is like
                            overwrite, but will not delete
                            existing info files
    --diff, -d              only show the differences, don't make
                            any changes.
    --only_ogg              Only process ogg files
    --only_mp3              Only process mp3 files
    --only_flac             Only process flac files
    --verbose, -v           print lots of status information
EOF

extension_regex = "\\.(mp3|ogg|flac)$"

verbose = false
noarg = false
replace_type = "overwrite"
overwrite_existing = false
strict_additive = false
only_ogg = false
only_mp3 = false
only_flac = false
dirlist = []

ARGV.each do |arg|
    if noarg == false
        if arg == "--"
            noarg = true
            next
        end
        if arg == "-a" or arg == "--additive"
            replace_type = "additive"
            overwrite_existing = true
            strict_additive = false
            next
        end
        if arg == "-s" or arg == "--strict_additive"
            replace_type = "additive"
            overwrite_existing = true
            strict_additive = true
            next
        end
        if arg == "-o" or arg == "--overwrite"
            replace_type = "overwrite"
            overwrite_existing = true
            next
        end
        if arg == "-p" or arg == "--passive"
            replace_type = "overwrite"
            overwrite_existing = false
            next
        end
        if arg == "-d" or arg == "--diff"
            replace_type = "diff"
            overwrite_existing = true
            next
        end
        if arg == "--only_ogg"
            only_ogg = true
            next
        end
        if arg == "--only_mp3"
            only_mp3 = true
            next
        end
        if arg == "--only_flac"
            only_flac = true
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
    Find.find(dir) do |filename|
        next unless FileTest.file?( filename )
        next unless filename.downcase =~ extension_regex
        file_type = $1
        next if (file_type != "ogg" and only_ogg) \
                or (file_type != "mp3" and only_mp3) \
                or (file_type != "flac" and only_flac)
        puts( "Processing: #{filename}" ) if verbose
        info_filename = filename+".info"
        next if !overwrite_existing and FileTest.exists?( info_filename )
        old_info = {}
        if replace_type == "additive"
            File.open( info_filename, "r" ).each do |line|
                line =~ /([^=]+)=(.*)/
                unless $1.nil?
                    old_info[$1] = [] unless old_info.has_key?($1)
                    old_info[$1] << $2
                end
            end
        end
        if file_type == "ogg"
            info = read_ogg( filename )
        elsif file_type == "mp3"
            info = read_mp3( filename )
        elsif file_type == "flac"
            info = read_flac( filename )
        end
        if replace_type == "additive"
            info.each do |key, value|
                if old_info.has_key?( key )
                    if strict_additive
                        value.each do |val|
                            old_info[ key ] << val unless old_info[ key ].include?( val )
                        end
                    end
                else
                    old_info[ key ] = value
                end
            end
            info = old_info
        end
        if replace_type == "diff"
            info_file = Tempfile.new( File.basename(info_filename)+"__", File.dirname(info_filename) )
        else
            info_file = File.open( info_filename, "w")
        end
        info.each do |key, value|
            value.each do |val|
                info_file.puts( "#{key}=#{val}" )
            end
        end
        info_file.puts
        if replace_type == "diff"
            info_file.close(false)
            system "diff -uBN #{info_filename.sq} #{info_file.path.sq}"
            info_file.close(true)
        else
            info_file.close
        end
    end
end

