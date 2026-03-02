# lopus_tool
A command-line tool for encoding and decoding lopus, a custom Opus audio format used in Nintendo Switch games. The resulting lopus files are compatible with  foobar2000 for playback.

Features:
Encode wav files to Nintendo lopus format
Decode lopus files back to standard wav format

Usage:
lopus.exe -e input.wav     (encode WAV to LOPUS)
lopus.exe -d input.lopus   (decode LOPUS to WAV)


Dependencies:
libopus (required for Opus encoding/decoding)

Playback Compatibility:
Files created with this tool can be played using:
foobar2000(with vgmstream plugins)
