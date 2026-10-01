// license:BSD-3-Clause
//
// **液晶の字を手描きで差し替える**（`art/lcdfont.txt`）。
//
// 手元にある字形 ROM（MAME の `mulcd.zip` の `hd44780u_b04.bin`）は
// MU2000 自身のものではないらしく、記号のところが実機と違う
// （doc/native-engine.md の 6.148・6.157）。実機の画面を見ながら
// 1 マスずつ描き起こしたものを、この形で上から被せる。
//
// **ROM から起こした字は入れない。** ここに入れてよいのは、実機の画面を
// 見て人が描いたものだけ（配れるのはそれだけ）。
//
// 書き方:
//
//     # 行頭が # なら覚え書き
//     10  プログラム No. の前の三角（黒塗り）
//     .#...
//     .##..
//     .###.
//     .####
//     .###.
//     .##..
//     .#...
//     .....
//
// 2 桁の 16 進で字のコードを書き、そのあとに `.` と `#` だけの行を並べる。
// 1 行 5 桁・1 字 8 行。足りない行は消灯として扱う。
// コードの行には、そのあとに好きな覚え書きを書いてよい。

#ifndef S_MU2000_LCDFONT_H
#define S_MU2000_LCDFONT_H

#pragma once

#include "compat/paths.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace smu2000 {
namespace lcdfont {

// 1 字ぶん（8 行 × 5 ドット）を 16 バイトの並びへ。上位 3 ビットは 0
struct glyph {
	int code = -1;
	unsigned char row[8] = {};
};

// 読む。読めた字の数を返す（0 なら何もしない）
inline int parse(const std::string &text, std::vector<glyph> &out)
{
	glyph cur;
	int nrow = 0;
	auto flush = [&]() {
		if (cur.code >= 0)
			out.push_back(cur);
		cur = glyph();
		nrow = 0;
	};
	size_t i = 0;
	while (i <= text.size()) {
		const size_t e = text.find('\n', i);
		std::string line = text.substr(i, (e == std::string::npos ? text.size() : e) - i);
		i = (e == std::string::npos) ? text.size() + 1 : e + 1;
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		size_t b = 0;
		while (b < line.size() && (line[b] == ' ' || line[b] == '\t'))
			b++;
		line = line.substr(b);
		if (line.empty()) {
			flush();
			continue;
		}
		// **点の行かどうかを、覚え書きより先に見る**。`.` と `#` だけで
		// 5 桁までなら点の行。いちばん左が点いている行（`###.#` など）は
		// `#` で始まるので、先に覚え書きとして捨てていた（利用者の報告）
		bool dots = line.size() <= 5;
		if (dots)
			for (char c : line)
				if (c != '.' && c != '#') { dots = false; break; }
		if (dots && cur.code >= 0 && nrow < 8) {
			unsigned char v = 0;
			for (size_t x = 0; x < 5 && x < line.size(); x++)
				if (line[x] == '#')
					v |= (unsigned char)(1u << (4 - x));
			cur.row[nrow++] = v;
			continue;
		}
		if (line[0] == '#')
			continue;                      // 覚え書き
		// それ以外は「コード＋覚え書き」の行
		char *end = nullptr;
		const long code = std::strtol(line.c_str(), &end, 16);
		if (end != line.c_str() && code >= 0 && code < 256) {
			flush();
			cur.code = int(code);
		}
	}
	flush();
	return int(out.size());
}

inline bool read_file(const std::string &path, std::string &out)
{
	std::FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
		return false;
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
		out.append(buf, n);
	std::fclose(f);
	return true;
}

// 字形 ROM（4KB 以上・1 字 16 バイト）へ上書きする。書いた字の数を返す
// （`apply` という名前は std::apply と当たるので使わない）
inline int overlay(std::vector<unsigned char> &rom, const std::string &path)
{
	std::string text;
	if (rom.size() < 0x1000 || !read_file(path, text))
		return 0;
	std::vector<glyph> gs;
	if (!parse(text, gs))
		return 0;
	for (const glyph &g : gs)
		for (int y = 0; y < 8; y++)
			rom[size_t(g.code) * 16 + size_t(y)] = g.row[y];
	return int(gs.size());
}

// 探す順: `SMU2000_LCDFONT` → **実行ファイルの隣**（とその親を 3 つ上まで）
// → **設定ディレクトリ** → いまいるディレクトリ。
//
// gui は build/ から起動することも、ショートカットから起動することもあるので、
// いまいるディレクトリだけを見ていると見つからない（利用者の報告）。
//
// **プラグイン（VST3・CLAP・AU）では実行ファイルの隣が使えない**。
// Windows の `exe_dir()` は **DAW 本体**の場所を返すので、
// そこに `art/lcdfont.txt` は置かない。そこで ROM の場所と同じく
// **設定ディレクトリ**（Windows なら `%LOCALAPPDATA%\S-MU2000`）も見る。
// `lcdfont.txt` をそこに置けば、DAW でも手描きの字が出る
inline int overlay_default(std::vector<unsigned char> &rom)
{
	if (const char *e = std::getenv("SMU2000_LCDFONT"))
		return overlay(rom, e);
	static const char *const REL[] = {
		"art/lcdfont.txt", "../art/lcdfont.txt",
		"../../art/lcdfont.txt", "../../../art/lcdfont.txt",
	};
	const std::string base = smu2000::exe_dir();
	if (!base.empty())
		for (const char *r : REL)
			if (const int n = overlay(rom, smu2000::join(base, r)))
				return n;
	const std::string cfg = smu2000::config_dir();
	if (!cfg.empty()) {
		if (const int n = overlay(rom, smu2000::join(cfg, "lcdfont.txt")))
			return n;
		if (const int n = overlay(rom, smu2000::join(cfg, "art/lcdfont.txt")))
			return n;
	}
	for (const char *r : REL)
		if (const int n = overlay(rom, r))
			return n;
	return 0;
}

} // namespace lcdfont
} // namespace smu2000

#endif // S_MU2000_LCDFONT_H
