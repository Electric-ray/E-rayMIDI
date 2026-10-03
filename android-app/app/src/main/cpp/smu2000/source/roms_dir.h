// license:BSD-3-Clause
//
// What a ROM directory has to hold, and how to bring a set of them into place.
//
// Two callers, one list, so the two cannot drift apart:
//
//   src/vst3/engine.cpp   picks the first directory holding a whole set. That
//                         engine runs the VST3, the AUv2 and the AUv3 alike.
//   src/auv3/main_app.mm  copies a set into the AUv3 extension's own
//                         Application Support directory, which is the only
//                         place outside the bundle a sandboxed extension may
//                         read.
//
// The images are Yamaha's, so they are in no build anybody hands out: the
// user brings their own dump and the plug-in finds it in its per-user
// settings directory (config_dir() in compat/paths.h). A local build can still
// bake them into the bundle (make auv3 AUV3_ROMS=roms) when it helps to
// develop against.
//
// Header-only on purpose: the AUv3 container app links nothing but Cocoa, so a
// .cpp of its own would drag the whole emulator into it. std::filesystem needs
// no library of its own on any of the toolchains this project builds with
// (verified on Apple clang and MinGW-w64, which is where an extra
// -lstdc++fs would otherwise be needed).

#ifndef S_MU2000_ROMS_DIR_H
#define S_MU2000_ROMS_DIR_H

#pragma once

#include "compat/paths.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace smu2000 {

// Not at global scope: this header goes into the plug-in, the AU and the AUv3
// container app alike, and a bare alias there would land in all of them
namespace fs = std::filesystem;

// The four 8MB wave ROMs, in load_wave()'s order. mu2000::WAVE_ROM_NAMES is
// defined from these, so the names live here only
inline constexpr const char *const kWaveRomNames[4] = {
	"xv364a0.ic49", "xv365a0.ic50", "xw848a0.ic53", "xw849a0.ic54"
};

// The sizes the loader insists on (read_file() in src/mu2000.cpp)
inline constexpr unsigned long long kProgramRomBytes = 0x400000;   // 4MB
inline constexpr unsigned long long kWaveRomBytes    = 0x800000;   // 8MB each

// A path is a path, never a string with a separator in it. Everything here
// takes and returns fs::path; std::string appears only for human-readable
// reasons (the `why`, log lines, window text). Callers convert once at the
// boundary -- from NSURL, from config_dir() -- and never spell a separator.
inline std::vector<fs::path> roms_required()
{
	std::vector<fs::path> out{ fs::path("mu2000_flash.bin") };
	for (const char *name : kWaveRomNames)
		out.push_back(fs::path("dump") / name);
	return out;
}

// The files that are wanted but not needed. The engine carries substitutes for
// both, so it says so in the log and plays on without them (src/vst3/engine.cpp)
inline std::vector<fs::path> roms_optional()
{
	return { fs::path("standin") / "sin-table.bin", fs::path("hd44780u_b04.bin"),
	         fs::path("standin") / "hd44780u_b04.bin" };
}

// What is missing, in the order of roms_required(). Empty means the directory
// can play. Paths come back relative, so a message can be shown next to the
// directory it is about.
//
// The checks use error codes, never throwing: a missing file, a wrong size
// and an unreadable directory all just mean "not here".
inline std::vector<fs::path> roms_missing(const fs::path &dir)
{
	std::vector<fs::path> out;
	if (dir.empty())
		return out;
	for (const fs::path &rel : roms_required()) {
		const bool wave = (rel.parent_path() == fs::path("dump"));
		const unsigned long long want = wave ? kWaveRomBytes : kProgramRomBytes;
		const fs::path path = dir / rel;
		std::error_code ec;
		const bool whole = fs::is_regular_file(path, ec) && !ec &&
		                   fs::file_size(path, ec) == want && !ec;
		if (!whole)
			out.push_back(rel);
	}
	return out;
}

// True when the directory holds a whole set. This is the test the ROM search
// uses: a half-finished directory must not shadow a complete one
inline bool has_roms(const fs::path &dir)
{
	return !dir.empty() && roms_missing(dir).empty();
}

namespace detail {

// One file, through a temporary name. The rename is atomic, so a copy cut
// short leaves no half-written image that a later size check could mistake
// for a whole one. Paths in, paths throughout; the only string here is the
// human-readable reason, built once at the point of failure.
inline bool copy_file(const fs::path &from, const fs::path &to, std::string &why)
{
	std::error_code ec;
	fs::path tmp = to;
	tmp += ".tmp";

	fs::create_directories(to.parent_path(), ec);
	if (ec) {
		why = "cannot make a directory for " + to.string() + ": " + ec.message();
		return false;
	}
	fs::copy_file(from, tmp, fs::copy_options::overwrite_existing, ec);
	if (ec) {
		const std::string what = ec.message();
		fs::remove(tmp, ec);
		why = "cannot copy " + from.string() + " to " + to.string() + ": " + what;
		return false;
	}
	// Replaces the destination atomically: a reader never sees a half-written
	// file, failed or not.
	fs::rename(tmp, to, ec);
	if (ec) {
		const std::string what = ec.message();
		fs::remove(tmp, ec);
		why = "cannot copy " + from.string() + " to " + to.string() + ": " + what;
		return false;
	}
	return true;
}

} // namespace detail

// Copies the images the engine opens out of one ROM directory into another,
// making the destination as it goes.
//
// A dump folder usually holds more than this needs (bank images, patches, the
// updater), so only the files the loader reads are taken: 36MB rather than a
// whole tree. Returns false with a reason in `why` when a required file cannot
// be read or written; the optional ones are copied when they are there and
// skipped when they are not, which is what the engine makes of them anyway
inline bool install_roms(const fs::path &from, const fs::path &to, std::string &why)
{
	if (!has_roms(from)) {
		why = "not a ROM directory: " + from.string();
		return false;
	}
	std::error_code ec;
	fs::create_directories(to, ec);
	if (ec) {
		why = "cannot make " + to.string() + ": " + ec.message();
		return false;
	}
	for (const fs::path &rel : roms_required())
		if (!detail::copy_file(from / rel, to / rel, why))
			return false;
	for (const fs::path &rel : roms_optional())
		detail::copy_file(from / rel, to / rel, why);   // best effort
	return true;
}

} // namespace smu2000

#endif // S_MU2000_ROMS_DIR_H
