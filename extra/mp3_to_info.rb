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

# This is a simple and somewhat working example of how to export your mp3's
# to squash's database.

# Just call on directory or filename.

require "find"

class String
  def sq
    "'#{self.gsub(/'/,"'\\\\''")}'"
  end
end

$trans_id3v2_to_vorbis = {
 "TIT2"=>"title",
 "TT2"=>"title",
 "TOPE"=>"original_artist",
 "TPE1"=>"artist",
 "TP1"=>"artist",
 "TOAL"=>"original_album",
 "TALB"=>"album",
 "TAL"=>"album",
 "TSSE"=>"encoder",
 "TSS"=>"encoder",
 "TRCK"=>"tracknumber",
 "TYER"=>"date",
 "TCON"=>"genre",
}
   
ARGV.each do |path|
  path = File.expand_path(path)

  Find.find(path) do |filename|
    next unless filename =~ /\.mp[23g]$/i
    basename = File.basename(filename)
    dirname = File.dirname(filename)
    infoname = filename + ".info"
    if FileTest.exists?(infoname)
      puts "Skipping #{filename} because .info already exists."
    else
      system "id3v2 -R #{filename.sq} > /tmp/tmp.file"
      v1, v2 = 0, 0
      File.open(infoname, "w") do |f|
        File.open("/tmp/tmp.file", "r").each_line do |line|
          if line =~ /^id3v1 tag/
	    v1 = 1
	    f.puts "# V1 tag:"
	    next
	  end
	  if line =~ /^id3v2 tag/
	    v2 = 1
	    next
	  end
          if v1 == 1 and v2 == 0  # we are in the version 1 tag
            if line =~ /^Title\s*:(.*)Artist:(.*)$/
	      $1.strip!
	      $2.strip!
	      f.puts "#title=%s" % $1 unless $1.empty?
	      f.puts "#artist=%s" % $2 unless $2.empty?
	    elsif line =~ /^Album\s*:(.*)Year:(.*),\s*Genre:(.*)\((.*)\)\s*$/
	      $1.strip!
	      $2.strip!
	      $3.strip!
	      f.puts "#album=%s" % $1 unless $1.empty?
	      f.puts "#date=%s" % $2 unless $2.empty?
	      f.puts "#genre=%s" % $3 unless $3.empty?
	    elsif line =~ /^Comment\s*:(.*)$/
	      $1.strip!
	      f.puts "#comment=%s" % $1 unless $1.empty?
	    end
	  elsif v2 == 1 # we are in the version 2 tag
	    if line =~ /^(.*?)\s*\(.*?\):(.*)$/
		    v2_tagname, value = $1.strip, $2.strip
		    key = $trans_id3v2_to_vorbis[v2_tagname]
			  if key.nil?
		      f.puts "# Unknown tag %s=%s" % [v2_tagname, value]
			  else
				  f.puts "%s=%s" % [key, value]
				end
	    else
				f.puts "# Unknown line: %s" % line
			end
	  end
        end
      end
    end
  end
end
