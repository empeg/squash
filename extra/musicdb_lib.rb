#!/usr/bin/ruby

class String
    def sq
        "'#{self.gsub(/'/,"'\\\\''")}'"
    end
end

# Addes a value to the hash.
# Hash is normal, but values are arrays.
def insert_pair( hash, key, value )
    hash[key] = [] unless hash.has_key?( key )
    hash[key] << value
end

# Reads an ogg's metadata into a hash.
def read_ogg( filename )
    info = {}
    `vorbiscomment -R -l #{filename.sq}`.each_line do |line|
        line =~ /([^=]+)=(.*)/
        insert_pair(info, $1, $2) unless $1.nil?
    end
    return info
end

# Reads an mp3's metadata into a hash.
def read_mp3( filename )
    info = {}
    `id3v2 -R #{filename.sq}`.each_line do |line|
        line =~ /([A-Z][A-Z0-9]{3})[^:]*: *(.*)/
        tag = $1;
        value = $2;
        if $id3v2_to_db[tag].nil?
            puts "Warning, skipping tag: #{tag}=#{value}" if $verbose
            next
        end
        key = $id3v2_to_db[tag]
        if key == "tracknumber"
            value, value2 = value.split("/", 2)
            insert_pair(info, key, value)
            insert_pair(info, "tracknumber_total", value2)
            next
        elsif key == "disc"
            value, value2 = value.split("/", 2)
            insert_pair(info, key, value)
            insert_pair(info, "disc_total", value2)
            next
        elsif key == "comment" or key == "url"
            value =~ /[^:]*:? *(.*)/
            value = $1
        end
        insert_pair(info, key, value)
    end
    return info
end

# This is a list of tables on how to convert id3 tags to
# database format, and vice versa.

# Taken from http://www.id3.org/id3v2.4.0-frames.txt
$id3v2_to_db = {

# Identification frames
#"TIT1"=>"music_group",
"TIT2"=>"title",
"TIT3"=>"song_mix",
"TALB"=>"album",
"TOAL"=>"album_original",
"TRCK"=>"tracknumber", # extend track_total
"TPOS"=>"disc", # extend disc_total
#"TSST"=>"subtitle",
"TSRC"=>"isrc",

# Involved persons frames
"TPE1"=>"artist",
"TPE2"=>"artist_extra",
"TPE3"=>"artist_conductor",
"TPE4"=>"artist_remix",
"TOPE"=>"artist_original",
"TEXT"=>"artist_lyrics",
"TOLY"=>"artist_lyrics_original",
"TCOM"=>"artist_composer",
#"TMCL"=>"musicians", # this one is too weird
#"TENC"=>"file_creator",

# Derived and subjective properties
"TBPM"=>"bpm",
"TLEN"=>"length", # in miliseconds
#"TKEY"=>"initial_key",
"TLAN"=>"language",
"TCON"=>"genre",
"TFLT"=>"file_type",
"TMED"=>"original_media",
"TMOO"=>"mood",

# Rights and license frames
#"TCOP"=>"copyright",
#"TPRO"=>"produced_by",
#"TPUB"=>"publisher",
#"TOWN"=>"owner",
#"TRSN"=>"radiostation_name",
#"TRSO"=>"radiostation_owner",

# Other
#"TOFN"=>"filename",
#"TDLY"=>"delay",
#"TDEN"=>"encoded_date",
#"TDRC"=>"recorded_date",
#"TDRL"=>"release_date",
#"TDTG"=>"tag_date",
#"TSSE"=>"encoder",
#"TSOA"=>"album_sort",
#"TSOP"=>"artist_sort",
#"TSOT"=>"title_sort",
"TXXX"=>"user_text",
"WXXX"=>"url",
"WCOM"=>"url_commercial",
"WCOP"=>"url_copyright",
"WOAF"=>"url_file",
"WOAR"=>"url_artist",
"WOAS"=>"url_source",
"WPAY"=>"url_purchase",
"WPUB"=>"url_publisher",
#"MCDI"=>"mcdi", # cddb info
#"ETCO"=>"timing",
#"MLLT"=>"mpeg_lookup",
#"SYTC"=>"syncronized_tempo",
"USLT"=>"lyrics",
"SYLT"=>"lyrics_syncronized",
"COMM"=>"comment",
"RVA2"=>"relative_volume",
"EQU2"=>"equalization",
"RVRB"=>"reverb",
#"APIC"=>"picture",
#"GEOB"=>"general_object",
#"PCNT"=>"play_count",
"POPM"=>"popularity",
#"RBUF"=>"recommended_buffer",
#"AENC"=>"audio_encryption",
#"LINK"=>"link",
#"POSS"=>"start_position",
#"USER"=>"terms_of_use",
#"OWNE"=>"owner",
#"COMR"=>"commercial",
#"ENCR"=>"encryption",
#"GRID"=>"group_id",
#"PRIV"=>"private",
#"SIGN"=>"signature",
#"SEEK"=>"seek",
#"ASPI"=>"audio_seek_points",

# From http://replaygain.hydrogenaudio.org/file_format_id3v2.html
"RGAD"=>"replay_gain",
}

$db_to_id3v2 = $id3v2_to_db.invert
