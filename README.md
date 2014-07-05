Frost
=====

A backup program that does deduplication, compression, encryption.
It's based on the ideas of Syncany, but reimplemented in C++, using state of art compression (BSC library), no dependency on anything except libcrypto.
It provides a console mode that has been tested on both Linux and MacOSX.
It allows saving backups to a remote server that's considered hostile, with no modification to the remote server software required.

Because of deduplication, space saving is considerable between backup and in a backup itself.
Then even more data is preserved with compression (using either zLib or BSC library), and compressed data is encrypted in local files (or remote files if you mounted then beforehand).


More info, documentation and encryption algorithm description <a href="http://x-ryl669.github.io/Frost/">here</a>
