#!/usr/bin/ruby
require 'find'

# uncollect_info.rb will a consolidated info file and
# (optionally) a corresponding index file (default.cinfo
# and default.cindex) and recreate several info files.

$help = <<EOF;
Please run uncollect_info.rb with a list of directory names,
for each directory name, a consolidated info file will be
read, along with a matching index file.  The collected info
files within will then be written back out to the original
info files.
You may also specify these options:
    --prefix=name           set cinfo and cindex filename
                            prefixes to name (i.e. name.cinfo,
                            and name.cindex).  Defaults to
                            'default'.
    --noindex, -n           do not use index file(s).
    --verbose, -v           print lots of status information
EOF

extension_regex = "\\.(mp?|ogg)$"

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
    puts( "Reading: #{cinfo_filename}" ) if verbose
    cinfo = File.open( cinfo_filename, "r" )

    unless noindex
        cindex_filename = File.join(dir, prefix+".cindex")
        puts( "Reading: #{cindex_filename}" ) if verbose
        cindex = File.open( cindex_filename, "r" )
    end

    info = nil
    filename = nil
    info_filename = nil
    until (cinfo_line = cinfo.gets).nil? do
        if cinfo_line =~ /^=== (.*)/
            unless info.nil?
                info.puts
                info.close
            end
            cinfo_filename = $1
            if noindex
                filename = cinfo_filename
            else
                filename = cindex.gets
                filename.chomp!
                if filename.nil?
                    $stderr.puts("Premature end of cindex file (did you add entries to the cinfo file?)")
                    exit
                end
                if filename != cinfo_filename
                    puts("Warning file names differ in cindex versus cinfo (probably ok)")
                end
            end
            puts( "Processing: #{filename}" ) if verbose
            info_filename = filename+".info"
            info = File.open(info_filename, "w")
            next
        end
        next if info_filename.nil?
        info.puts( cinfo_line ) unless cinfo_line =~ /^\s*$/
    end
    unless info.nil?
        info.puts
        info.close
    end
    unless noindex
        until (cindex_line = cinfo.gets).nil? do
            filename = cindex_line
            filename.chomp!
            info_filename = filename+".info"
            info = File.open(info_filename, "w")
            info.puts
            info.close
        end
    end

    cinfo.close
    cindex.close unless noindex
end
