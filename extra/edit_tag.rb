#!/usr/bin/ruby
require 'find'
require 'ftools'
require 'tempfile'

# edit_tag.rb will take a list of directories and either
# process the cinfo files or info files contained it those
# directories.  Each file is processed according to a
# list of commands which is read in from standard in.

$help = <<EOF
Please run edit_tag.rb with a list of info or cinfo files.
Also please specify commands to perform, which will be read
from standard input.
For each file name, commands will be issued that will effect
the key value pairs
Commands will be processed one at a time, rewriting the info
file(s) each time.
You may also specify these options:
    --info, -i              Only process info files not cinfo
    --cinfo, -c             Only process cinfo files not info
                            (default)
    --prefix=name           set cinfo filename prefix to name
                            (i.e. name.cinfo).  Defaults to
                            'default'.
    --verbose, -v           print lots of status informatio
And these commands:
    add key=value           Add the key=value pair to each
                            info file
    del key[=value]         Delete the key (value is optional)
                            from each info file.  You may specify
                            a regex for value.
    sub key=regex,value     Substitute using regex and value on
                            any key found in each info file.
                            regex may not contain a comma!
EOF

extension_regex = "\\.(mp?|ogg)$"

verbose = false
noarg = false
dirlist = []
which = "cinfo"
prefix = "default"

ARGV.each do |arg|
    if noarg == false
        if arg == "--"
            noarg = true
            next
        end
        if arg =~ /--prefix=(.*)/
            prefix = $1
        end
        if arg == "-i" or arg == "--info"
            which = "info"
            next
        end
        if arg == "-c" or arg == "--cinfo"
            which = "cinfo"
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
    exit
end

$command_list = []

until (command = $stdin.gets).nil? do
    command =~ /([^ ]+) +(.*)/
    command_type = $1
    command_rest = $2
    key = nil
    value = nil
    regex = nil
    if command_type == "add"
        command_rest =~ /([^=]+) *= *(.*)/
        key = $1
        value = $2
    elsif command_type == "del"
        command_rest =~ /([^=]+)( *= *(.*))?/
        key = $1
        value = $3
    elsif command_type == "sub"
        command_rest =~ /([^=]+) *= *([^,]*),(.*)/
        key = $1
        regex = $2
        value = $3
    else
        puts("Unknown command '#{command}'") if verbose
        next
    end
    $command_list.push( {"type"=>command_type, "key"=>key, "value"=>value, "regex"=>regex } )
end

def process( filename, where_to_add )
    temp_file = Tempfile.new( File.basename(filename)+"__", File.dirname(filename) )
    temp_file.close( false )
    File.move( filename, temp_file.path ) \
        if FileTest.file?( filename )

    temp_file.open
    info = File.open( filename, "w" )

    new_lines = []
    if where_to_add == "at_beginning"
        $command_list.each_index do |command_number|
            type, key, value = $command_list[command_number].indices("type", "key", "value")
            if type == "add"
                new_lines << { "command_number"=>command_number, "line"=>"#{key}=#{value}" }
            end
        end
    end
    while true do
        # command number is on which command the line was added,
        # this is to keep added lines from having commands entered
        # earlier affect them.
        if new_lines.empty?
            line = temp_file.gets
            line_command_number = -1
            break if line.nil?
        else
            line_info = new_lines.shift
            line_command_number, line = line_info.indices("command_number", "line")
        end
        delete = false
        $command_list.each_index do |command_number|
            type, key, value, regex = $command_list[command_number].indices("type", "key", "value", "regex")
            if line =~ /^===/ and where_to_add == "at_equals"
                if type == "add"
                    new_lines << { "command_number"=>command_number, "line"=>"#{key}=#{value}" }
                end
            else
                next unless line_command_number < command_number
                line =~ /([^=]+)=(.*)/
                line_key = $1
                line_value = $2
                if type == "del"
                    if line_key == key and ( value.nil? or line_value =~ value )
                        delete = true
                        break
                    end
                elsif type == "sub"
                    if line_key == key
                        line.gsub!( regex, value )
                    end
                end
            end
        end
        info.puts line unless delete
    end
    info.close
    temp_file.close(true)
end

dirlist.each do |dir|
    if which == "cinfo"
        cinfo_filename = File.join( dir, prefix+".cinfo" )
        puts( "Reading: #{cinfo_filename}" ) if verbose

        process( cinfo_filename, "at_equals" )
    elsif which == "info"
        Find.find(dir) do |filename|
            next unless FileTest.file?( filename )
            next unless filename =~ extension_regex
            puts( "Reading: #{filename}" ) if verbose
            info_filename = filename+".info"

            process( info_filename, "at_beginning" )
        end
    end
end

