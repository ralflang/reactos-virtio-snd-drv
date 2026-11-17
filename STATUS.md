# DEVELOPMENT STATUS

We can play sound (yay).

The driver targets PCI ID `1AF4:1059`, initializes the ReactOS VirtIO PCI transport, negotiates `VIRTIO_F_VERSION_1` and reads the sound configuration. 

We currently support one WaveCyclic stream at a time and advertise
mono/stereo S16 and S32 PCM at 44.1/48/96 kHz.

VirtIO channel maps are queried at startup and used to set `WAVEFORMATEXTENSIBLE` 
speaker masks for matching mono/stereo formats. 
Thestandard mono/stereo masks are used when the device reports no matching map.

Volume is exposed in 1 dB steps from -96 dB to 0 dB. 
Mute silences subsequent PCM submissions. 

The driver does not yet implement VirtIO device mixer controls.

PortCls STOP/REMOVE dispatch quiesces playback before transport teardown.
Mixer controls affect submitted audio.

The event queue is populated and drained, PCM XRUN notifications are requested
only when advertised by the selected stream. 

The current QEMU device reports no PCM features and does not provide XRUN
events. 

A build-gated synthetic XRUN test hook exercises the same recovery
dispatch without advertising unsupported device features. 
