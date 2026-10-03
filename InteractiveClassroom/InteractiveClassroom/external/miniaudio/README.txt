MINIAUDIO DEPENDENCY
====================

This project uses miniaudio 0.11.25.

CMake downloads the pinned official source automatically with FetchContent and
builds miniaudio.c as a private static library. No system-wide audio SDK,
manual include path, DLL copy, or package-manager configuration is required.

Official project: https://github.com/mackron/miniaudio
Version/tag: 0.11.25
License: public domain or MIT-0, as offered by the upstream project.

Only WAV decoding is needed by this classroom. MP3/FLAC decoding and encoding
are disabled in CMake to keep the build focused and lightweight.
