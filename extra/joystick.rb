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

# This is an example of how to use the squash control file.
# This example isn't the best, and a better joystick interface would be nice.
# But this example is short and clear.

require 'joystick'

fork do
    Joystick::Device.open('/dev/input/js0') do |joy|
        loop do
            until joy.pending? do
                sleep 0.1
            end
            File.open(`echo -n ~/.squash_control`, 'a') do |fifo|
                event = joy.next_event
                case event.type
                when Joystick::Event::INIT
                    # ignore
                when Joystick::Event::BUTTON
                    if event.value == 0  # (button release)
                        case event.num
                        when 0
                            # Restart song
                            fifo.puts "stop"
                            fifo.puts "play"
                        when 1
                            # Play/Pause
                            fifo.puts "play_button"
                        when 2
                            # Toggle full screen spectrum
                            fifo.puts "spectrum_button"
                        when 3
                            # Skip song
                            fifo.puts "skip"
                        end
                    end
                when Joystick::Event::AXIS
                    # ignore
                end
            end
        end
    end
end
