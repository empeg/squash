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

# This is a decent example on how to export an ogg to
# squash's database format

# Just call on a file or directory.

require "find"

class String
  def sq
    "'#{self.gsub(/'/,"'\\\\''")}'"
  end
end

ARGV.each do |path|
  path = File.expand_path(path)

  Find.find(path) do |filename|
    next unless filename =~ /\.ogg$/
    basename = File.basename(filename)
    dirname = File.dirname(filename)
    infoname = filename + ".info"
    if FileTest.exists?(infoname)
      puts "Skipping #{filename} because .info already exists."
    else
      system "vorbiscomment -R -l #{filename.sq} > #{infoname.sq}"
    end
  end
end
