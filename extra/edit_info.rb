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

# This is an atrocious example of how to edit the meta information for the
# database that squash uses.
# You call it with the directory or file you want to edit.

# DO NOT CALL THIS ROUTINE MORE THAN ONCE AT A TIME, as the temporary file names
# suck quite badly.

# This routine will never -erase- mp3 tags.  You will have to be brave enough to
# do that yourself.  But it will overwrite existing tags that it knows how to handle.
# Oggs also have there meta information overwrit.

require "find"

class String
  def sq
    "'#{self.gsub(/'/,"'\\\\''")}'"
  end
end

info = ""

ARGV.each do |path|
  path = File.expand_path(path)

  Find.find(path) do |filename|
    Find.prune if filename =~ /\.info$/
    next unless FileTest.file?(filename)
    puts "Reading #{filename}"
    basename = File.basename(filename)
    dirname = File.dirname(filename)
    infoname = filename + ".info"
    if FileTest.file?(infoname)
      info += "--- FILE: #{filename} ---\n"
      info += `cat #{infoname.sq}`;
    end
  end
end

File.open("/tmp/tmp.file", "w") do |file|
  file.puts info
end

system ENV["EDITOR"], "/tmp/tmp.file"

filename = nil
infoname = nil
info = ""

def update_ogg(filename, infoname)
  system "/usr/local/bin/vorbiscomment -R -l #{filename.sq} > /tmp/tmp2.file"
  system "diff -b #{infoname.sq} /tmp/tmp2.file > /tmp/tmp3.file"
  if File.stat("/tmp/tmp3.file").size > 0
    puts ".info has new information, updating %s" % filename
    system "/usr/local/bin/vorbiscomment -R -w -c #{infoname.sq} #{filename.sq}"
  end
end

$trans_vorbis_to_id3v2 = {
 "title"=>"TIT2",
 "artist"=>"TPE1",
 "original_artist"=>"TOPE",
 "album"=>"TALB",
 "original_album"=>"TOAL",
 "encoder"=>"TSSE",
 "tracknumber"=>"TRCK",
 "genre"=>"TCON",
 "date"=>"TYER",
}

$trans_short_to_long_v2 = {
 "TT2"=>"TIT2",
 "TP1"=>"TPE1",
 "TAL"=>"TALB",
 "TSS"=>"TSSE",
}

def update_mp3(filename, info_raw)
  info = {}  # keys are standard .info format
  tags_v2 = {}  # keys are id3v2 format
  info_raw.split("\n").each do |line|
    next if line =~ /^#/
    line =~ /(.*?)\s*=\s*(.*)/
    key, value = $1, $2
    info[key] = value
  end
  system "id3v2 -R #{filename.sq} > /tmp/tmp2.file"
  v2 = 0
  command_tags = ""
  File.open("/tmp/tmp2.file", "r").each_line do |line|
    if line =~ /^id3v2 tag/
      v2 = 1
      next
    end
    if v2 == 1
      next unless line =~ /^(.*?)\s*\(.*?\):(.*)$/
      key, value = $1.strip, $2.strip
      if key.length < 4
        key = $trans_short_to_long_v2[key]
	if key.nil?
	  #WARN unknown tag
	end
      end
      tags_v2[key] = value
    end
  end

  info.each do |key, value|
    v2_key = $trans_vorbis_to_id3v2[key]
    v2_value = tags_v2[v2_key]
    if value != v2_value
      if v2_key.nil?
        # WARN don't know how to translate std tag to id3v2 tag
	puts "WARNING: Don't know how to translate tag #{key} into id3v2 tag!"
      else
        command_tags += "--%s %s " % [v2_key, value.sq]
      end
    end
  end
  unless command_tags.empty?
    puts ".info has new information, updating %s" % filename
    command = "id3v2 -2 %s %s" % [command_tags, filename.sq]
    system command
  end
end

File.open("/tmp/tmp.file", "r").each_line do |line|
  line.strip!
  next if line.empty?
  line += "\n"
  if line =~ /^--- FILE: (.*) ---$/
    newfilename = $1
    unless filename.nil?
      File.open(filename+".info", "w") do |file|
        file.puts info
      end
      if filename =~ /\.ogg$/
        update_ogg(filename, infoname)
      end
      if filename =~ /\.mp3$/
      	update_mp3(filename, info)
      end
    end
    filename = newfilename
    infoname = filename + ".info"
    info = ""
    puts "Working on #{filename}"
  else
    info += line
  end
end

unless filename.nil?
  File.open(filename+".info", "w") do |file|
    file.puts info
  end
  if filename =~ /\.ogg$/
    update_ogg(filename, infoname)
  end
  if filename =~ /\.mp3$/
    update_mp3(filename, info)
  end
end

