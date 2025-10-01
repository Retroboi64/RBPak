# RBPak

A asset packaging library for Project32 game engine
RBPak is a lightweight, fast asset compression and packaging library designed specifically for the *Project32* game engine. It creates `.rbk` (RBPak) files that efficiently store and compress game assets like sprites, sounds, levels, and other resources.

## Features

- 🗜️ *Zlib Compression* - Efficient compression reduces file sizes

- 📦 *Single File Packaging* - Bundle all game assets into one `.rbk` file

- ⚡ *Fast Dictionary Access* - Get assets with simple `pak\["filename"]` syntax

- 🔧 *Lazy Loading* - Files are only decompressed when accessed

- ✅ *Data Integrity* - CRC32 checksums prevent corruption

## Why RBPak?

Created as the official asset management system for *Project32*, RBPak solves common game development challenges:

- *Simple Distribution*: Ship one `.rbk` file instead of hundreds of loose assets

- *Fast Loading*: Pre-compressed assets load quickly on older hardware

- *Clean Code*: Dictionary-style access keeps game code readable

- *Easy To Use*: Just add the static libary and it's headers and then you can implement a filesystem/assetmanager with RBPak

## About RBPak

RBPak is developed as part of the *Project32* game engine - a modern engine. *RBPak* focuses:

## License

MIT License - feel free to use in your own projects!

## Contributing

RBPak is primarily developed for Project32, but contributions are welcome! Please ensure any changes maintain compatibility with the Project32 engine architecture.

---

*Made with ❤️ by Retroboi64*
