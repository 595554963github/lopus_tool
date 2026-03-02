# lopus_tool
A command-line tool for encoding and decoding lopus, a custom Opus audio format used in Nintendo Switch games. The resulting lopus files are compatible with  foobar2000 for playback.

Features
Encode WAV files to Nintendo LOPUS format

Decode LOPUS files back to standard WAV format

Full compatibility with vgmstream and foobar2000 players

Supported sample rates: 48000, 24000, 16000, 12000, 8000 Hz

Mono and stereo channel support

Preserves original Nintendo LOPUS structure

Usage
text
lopus.exe -e input.wav     (encode WAV to LOPUS)
lopus.exe -d input.lopus   (decode LOPUS to WAV)
Dependencies
libopus (required for Opus encoding/decoding)

Playback Compatibility
Files created with this tool can be played using:

vgmstream

foobar2000 (with appropriate plugins)
