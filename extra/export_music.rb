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

# export_info.rb will take a list of directories, and
# for each music file (mp3 or ogg) it will read a
# matching info file.  The data in the info file is then
# written to the music file.  You can either add only
# new key's to music files, or overwrite all existing
# keys.

$help = <<EOF;
Please run export_info.rb with a list of directory names,
for each directory name, all music files (mp3 or ogg) will be
processed, and their corresponding info file will be read.
The info file's data will then be written to the music file.
You may also specify these options:
    --additive, -a          this is the default, only only
                            overwite the same keys that are being
                            written.  instead of clearing all
                            existing music file information.
    --overwrite, -o         overwrite existing music files with
                            the info files
    --force, -f             force operations even if nothing is
                            different
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
replace_type = "additive"
force = false
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
            next
        end
        if arg == "-o" or arg == "--overwrite"
            replace_type = "overwrite"
            next
        end
        if arg == "-d" or arg == "--diff"
            replace_type = "diff"
            next
        end
        if arg == "-f" or arg == "--force"
            force = true
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

class String
  def sq
    "'#{self.gsub(/'/,"'\\\\''")}'"
  end
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
        db_info = {}
        if FileTest.exists?( info_filename )
            File.open( info_filename, "r" ).each do |line|
                line =~ /([^=]+)=(.*)/
                insert_pair( db_info, $1, $2 ) \
                    unless $1.nil?
            end
        end
        if file_type == "ogg"
            music_info = read_ogg( filename )
        elsif file_type == "mp3"
            music_info = read_mp3( filename )
        elsif file_type == "flac"
            music_info = read_flac( filename )
        end
        if replace_type == "diff"
            info_file = Tempfile.new( File.basename(info_filename)+"__", File.dirname(info_filename) )
            music_info.each do |key, value|
                value.each do |val|
                    info_file.puts( "#{key}=#{val}" )
                end
            end
            info_file.puts
            info_file.close(false)
            system "diff -uBN #{info_filename.sq} #{info_file.path.sq}"
            info_file.close(true)
            next
        end
        unless force
            same = true
            # once id3v2 0.1.9 is fixed we won't have to ignore urls
            if music_info.reject{ |k,v| file_type == "mp3" and k == "url" }.length \
                != db_info.reject{ |k,v| file_type == "mp3" and k == "url" }.length
                same = false
            else
                music_info.each do |key, value|
                    # ignore these tags until id3v2 0.1.9 is fixed
                    if file_type == "mp3" and (key == "comment" or key == "lyrics")
                        next
                    end
                    if replace_type == "additive"
                        unless !db_info.has_key?( key ) \
                            or (value.sort <=> db_info[ key ].sort) == 0
                            same = false
                            break
                        end
                    else
                        unless db_info.has_key?( key ) \
                            and (value.sort <=> db_info[ key ].sort) == 0
                            same = false
                            break
                        end
                    end
                end
            end
            if same
                next
            else
                puts " New info ^^^" if verbose
            end
        end

        # Ogg and Flac commands are easier to work with if we do the adding
        if replace_type == "additive" and file_type == "ogg"
            music_info.each do |key, value|
                unless db_info.has_key?( key )
                    insert_pair( db_info, key, value )
                end
            end
        end
        # db_info now has we need to write

        # We need to clear the mp3 tag if we are doing an overwrite,
        # and we need to always clear out the flac, ogg is cleared implicitly
        if replace_type == "overwrite" and file_type == "mp3"
            system "id3v2 -D #{filename.sq}"
        end
        if file_type == "flac"
            system "metaflac --remove-vc-all #{filename.sq}"
        end

        if file_type == "ogg" or file_type == "flac"
            if file_type == "ogg"
                command = "vorbiscomment -R -w -c - #{filename.sq}"
            else
                command = "metaflac --import-vc-from=- #{filename.sq}"
            end
            out = IO.popen( command, "w" )
            db_info.each do |key, value|
                out.puts "#{key}=#{value}"
            end
            out.close
        else
            command = "id3v2 -2 "
            db_info.each do |key, value|
                next if key == "disc_total" or key == "tracknumber_total"
                new_key = $db_to_id3v2[key]
                if new_key.nil?
                    puts "Warning didn't know what to do with #{key}" if verbose
                    next
                end
                new_value = ""
                value.each do |val|
                    if key == "disc" and db_info.has_key?( "disc_total " )
                        disc_total = db_info[ "disc_total" ].first
                        val += "/" + disc_total unless disc_total.nil?
                    end
                    if key == "tracknumber" and db_info.has_key?( "tracknumber_total " )
                        tracknumber_total = db_info[ "tracknumber_total" ].first
                        val += "/" + tracknumber_total unless tracknumber_total.nil?
                    end
                    new_value += "; " unless new_value.empty?
                    new_value += val
                end
                if key == "url"
                    # if id3v2 0.1.9 is fixed we can then add url tags
                    # until then don't even bother
                    # (when it is fixed, it will work like "user_text" below.
                    next
                end
                if key == "user_text"
                    new_value = ":" + new_value
                elsif key == "comment" or key == "lyrics"
                    # remove ':' until id3v2 0.1.9 is fixed, when it is
                    # use this line instead:
                    # new_value = ":" + new_value + ":"
                    new_value.gsub!(/:/, "")
                end
                command += "--#{new_key} #{new_value.sq} "
            end
            command += filename.sq
            system command
        end
    end
end

