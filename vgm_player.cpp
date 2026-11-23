// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 VGM Player for GameShell contributors
//
// VGM Player for ClockworkPi GameShell (320x240, armv7l)
// Playback engine: libvgm (PlayerA + VGMPlayer), rendered to PCM via SDL audio.
// Controls: Shift+X prev, Shift+B next, Shift+Y play/pause, Shift+A info,
//           START file browser, Y/A prev/next saved directory, MENU quit.
//           Browser: D-pad navigate, SELECT use directory, MENU exit.
//           Unshifted ABXY and D-pad L/R are aliases.

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <vector>

#include "player/playera.hpp"
#include "player/vgmplayer.hpp"
#include "utils/FileLoader.h"
#include "emu/SoundDevs.h"

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const char* FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
static const char* FONT_CJK_PATH = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
static const int SAMPLE_RATE = 44100;
static const char* VGM_DIR_CFG = "/home/cpi/games/VGMPlayer/vgm_dir.txt";
static const char* VGM_DIR_IDX = "/home/cpi/games/VGMPlayer/vgm_dir_index.txt";

static PlayerA   g_player;
static SDL_mutex* g_lock = NULL;
static volatile int g_paused = 0;
static volatile int g_quit = 0;
static int g_showInfo = 1;
static Uint32 g_lastAuto = 0;

static SDL_Surface* g_screen = NULL;

// SDL_ttf 1.x has no font fallback, so pair a Latin font with a CJK font.
struct FontPair
{
	TTF_Font* latin;
	TTF_Font* cjk;
};
static FontPair g_fontTitle = {NULL, NULL};
static FontPair g_fontText  = {NULL, NULL};
static FontPair g_fontSmall = {NULL, NULL};

static std::vector<std::string> g_files;
static int g_cur = -1;
static DATA_LOADER* g_loader = NULL;

static std::string g_title, g_game, g_system, g_author, g_date, g_comment;
static std::string g_chips, g_filebase;
static double g_totalTime = 0.0;

// Saved playback directories (one per line in vgm_dir.txt).
static std::vector<std::string> g_dirs;
static int g_dirIdx = 0;

// File browser state.
struct BrowserEntry
{
	std::string name;
	std::string path;
	bool isDir;
	int vgmCount;   // -1 = parent entry
};
static int g_browserOpen = 0;
static std::string g_browserDir;
static std::vector<BrowserEntry> g_entries;
static int g_browserSel = 0;
static int g_browserTop = 0;
static Uint32 g_browserOpenedAt = 0;
static std::string g_browserMsg;
static Uint32 g_browserMsgAt = 0;

// Toast shown on the player screen.
static std::string g_toast;
static Uint32 g_toastAt = 0;

// ---------------------------------------------------------------- utilities
static bool hasExt(const std::string& name, const char* ext)
{
	size_t elen = strlen(ext);
	if (name.size() < elen) return false;
	return strcasecmp(name.c_str() + name.size() - elen, ext) == 0;
}

static bool ciLess(const std::string& a, const std::string& b)
{
	return strcasecmp(a.c_str(), b.c_str()) < 0;
}

static std::string baseName(const std::string& path)
{
	std::string s = path;
	size_t slash = s.find_last_of("/\\");
	if (slash != std::string::npos) s = s.substr(slash + 1);
	size_t dot = s.find_last_of('.');
	if (dot != std::string::npos) s = s.substr(0, dot);
	return s;
}

static void scanDir(const std::string& dir)
{
	g_files.clear();
	DIR* d = opendir(dir.c_str());
	if (d == NULL) return;
	struct dirent* e;
	while ((e = readdir(d)) != NULL)
	{
		std::string n(e->d_name);
		if (n == "." || n == "..") continue;
		if (!hasExt(n, ".vgm") && !hasExt(n, ".vgz")) continue;
		std::string p = dir;
		if (!p.empty() && p[p.size()-1] != '/') p += '/';
		p += n;
		g_files.push_back(p);
	}
	closedir(d);
	std::sort(g_files.begin(), g_files.end(), ciLess);
}

// Directory list management: vgm_dir.txt holds one directory per line.
static void loadDirList(void)
{
	g_dirs.clear();
	FILE* f = fopen(VGM_DIR_CFG, "r");
	if (f != NULL)
	{
		char buf[1024];
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			std::string s(buf);
			while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r'))
				s.erase(s.size()-1);
			if (s.empty()) continue;
			if (std::find(g_dirs.begin(), g_dirs.end(), s) == g_dirs.end())
				g_dirs.push_back(s);
		}
		fclose(f);
	}
	if (g_dirs.empty())
		g_dirs.push_back("/home/cpi/games/DOSBOX/MODM/vgm");

	int idx = 0;
	FILE* fi = fopen(VGM_DIR_IDX, "r");
	if (fi != NULL)
	{
		if (fscanf(fi, "%d", &idx) != 1) idx = 0;
		fclose(fi);
	}
	if (idx < 0 || idx >= (int)g_dirs.size()) idx = 0;
	g_dirIdx = idx;
}

static void saveDirIndex(void)
{
	FILE* f = fopen(VGM_DIR_IDX, "w");
	if (f == NULL) return;
	fprintf(f, "%d\n", g_dirIdx);
	fclose(f);
}

// Append a directory to vgm_dir.txt (dedupe) and make it active.
static void addVgmDir(const std::string& dir)
{
	for (size_t i = 0; i < g_dirs.size(); i ++)
	{
		if (g_dirs[i] == dir)
		{
			g_dirIdx = (int)i;
			saveDirIndex();
			return;
		}
	}
	g_dirs.push_back(dir);
	FILE* f = fopen(VGM_DIR_CFG, "w");
	if (f != NULL)
	{
		for (size_t i = 0; i < g_dirs.size(); i ++)
			fprintf(f, "%s\n", g_dirs[i].c_str());
		fclose(f);
	}
	g_dirIdx = (int)g_dirs.size() - 1;
	saveDirIndex();
}

static std::string configuredDirSafe(void);

static std::string dirOfCurrent(void)
{
	if (g_cur >= 0 && g_cur < (int)g_files.size())
	{
		const std::string& p = g_files[g_cur];
		size_t s = p.find_last_of('/');
		if (s != std::string::npos) return p.substr(0, s == 0 ? 1 : s);
	}
	return g_dirs.empty() ? configuredDirSafe() : g_dirs[g_dirIdx];
}

// forward-declared fallback used by dirOfCurrent
static std::string configuredDirSafe(void)
{
	return std::string("/home/cpi/games/DOSBOX/MODM/vgm");
}

static std::string fmtTime(double sec)
{
	int s = (int)(sec < 0 ? 0 : sec);
	char buf[32];
	snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
	return std::string(buf);
}

static void setTag(std::string& dst, const std::string& key,
                   const std::string& k, const std::string& v)
{
	if (key == k && dst.empty() && !v.empty()) dst = v;
}

// ------------------------------------------------------------ track control
static void extractMeta(void)
{
	g_title = g_game = g_system = g_author = g_date = g_comment = "";
	g_chips = "";
	g_totalTime = 0.0;

	PlayerBase* p = g_player.GetPlayer();
	if (p != NULL)
	{
		const char* const* tags = p->GetTags();
		if (tags != NULL)
		{
			for (int i = 0; tags[i] != NULL && tags[i+1] != NULL; i += 2)
			{
				std::string k(tags[i]);
				std::string v(tags[i+1]);
				setTag(g_title,  k, "TITLE", v);
				setTag(g_title,  k, "TITLE-JPN", v);
				setTag(g_game,   k, "GAME", v);
				setTag(g_game,   k, "GAME-JPN", v);
				setTag(g_system, k, "SYSTEM", v);
				setTag(g_system, k, "SYSTEM-JPN", v);
				setTag(g_author, k, "ARTIST", v);
				setTag(g_author, k, "ARTIST-JPN", v);
				setTag(g_date,   k, "DATE", v);
				setTag(g_comment,k, "COMMENT", v);
			}
		}

		std::vector<PLR_DEV_INFO> devs;
		if (p->GetSongDeviceInfo(devs) == 0)
		{
			for (size_t i = 0; i < devs.size(); i ++)
			{
				if (devs[i].parentIdx != (UINT32)-1) continue;
				if (devs[i].devDecl == NULL || devs[i].devCfg == NULL) continue;
				const char* nm = devs[i].devDecl->name(devs[i].devCfg);
				if (nm == NULL || nm[0] == '\0') continue;
				if (g_chips.find(nm) != std::string::npos) continue;
				if (!g_chips.empty()) g_chips += ", ";
				g_chips += nm;
			}
		}
	}

	if (g_cur >= 0 && g_cur < (int)g_files.size())
		g_filebase = baseName(g_files[g_cur]);
	else
		g_filebase = "";
	if (g_title.empty()) g_title = g_filebase;

	g_totalTime = g_player.GetTotalTime(PLAYTIME_LOOP_INCL | PLAYTIME_TIME_FILE);
}

// Switch to playlist index 'idx'; if it fails, walk in 'dir' until one loads.
// Returns true when a file was successfully loaded.
static bool switchTo(int idx, int dir)
{
	if (g_files.empty()) return false;
	int n = (int)g_files.size();
	SDL_LockMutex(g_lock);

	g_player.Stop();
	g_player.UnloadFile();
	if (g_loader != NULL) { DataLoader_Deinit(g_loader); g_loader = NULL; }

	int tries = 0;
	bool loaded = false;
	while (tries < n)
	{
		int slot = idx + dir * tries;
		slot %= n;
		if (slot < 0) slot += n;
		g_cur = slot;

		const char* path = g_files[slot].c_str();
		g_loader = FileLoader_Init(path);
		if (g_loader != NULL)
		{
			DataLoader_SetPreloadBytes(g_loader, 0x100);
			if (DataLoader_Load(g_loader) == 0)
			{
				if (g_player.LoadFile(g_loader) == 0)
				{
					g_player.SetLoopCount(1);
					loaded = true;
					break;
				}
			}
			DataLoader_Deinit(g_loader);
			g_loader = NULL;
		}
		g_player.UnloadFile();
		tries ++;
	}

	if (loaded)
	{
		extractMeta();
		g_player.Start();
		g_paused = 0;
		g_lastAuto = SDL_GetTicks();
	}
	else
	{
		g_title = "No playable VGM file";
		g_game = g_system = g_author = g_date = g_comment = g_chips = "";
		g_filebase = "";
		g_totalTime = 0.0;
	}

	SDL_UnlockMutex(g_lock);
	return loaded;
}

// --------------------------------------------------------- directory actions
static void showToast(const std::string& msg)
{
	g_toast = msg;
	g_toastAt = SDL_GetTicks();
}

static bool setPlaybackDir(const std::string& dir, bool remember)
{
	std::vector<std::string> oldFiles = g_files;
	int oldCur = g_cur;

	scanDir(dir);
	if (g_files.empty())
	{
		g_files = oldFiles;
		g_cur = oldCur;
		return false;
	}
	if (!switchTo(0, 1))
	{
		g_files = oldFiles;
		g_cur = -1;
		switchTo(oldCur, 1);   // restore previous playback
		return false;
	}
	if (remember) addVgmDir(dir);
	return true;
}

// Cycle through saved directories with Y/A.
static void switchDir(int delta)
{
	int n = (int)g_dirs.size();
	if (n <= 1)
	{
		showToast("Only one saved directory");
		return;
	}
	std::vector<std::string> oldFiles = g_files;
	int oldCur = g_cur;

	bool ok = false;
	for (int t = 0; t < n; t ++)
	{
		int next = g_dirIdx + delta;
		next %= n;
		if (next < 0) next += n;
		g_dirIdx = next;
		scanDir(g_dirs[g_dirIdx]);
		if (!g_files.empty() && switchTo(0, 1)) { ok = true; break; }
	}
	saveDirIndex();

	char buf[300];
	if (ok)
		snprintf(buf, sizeof(buf), "DIR %d/%d  %s", g_dirIdx + 1, n,
		         g_dirs[g_dirIdx].c_str());
	else
		snprintf(buf, sizeof(buf), "No playable VGM in saved dirs");
	showToast(buf);

	if (!ok)
	{
		g_files = oldFiles;
		g_cur = -1;
		switchTo(oldCur, 1);
	}
}

// ------------------------------------------------------------ file browser
static bool isDirectory(const std::string& p)
{
	struct stat st;
	if (stat(p.c_str(), &st) != 0) return false;
	return S_ISDIR(st.st_mode);
}

static std::string joinPath(const std::string& a, const std::string& b)
{
	if (a.empty() || a == "/") return "/" + b;
	return a + "/" + b;
}

static std::string parentPath(const std::string& p)
{
	if (p.empty() || p == "/") return "/";
	size_t s = p.find_last_of('/');
	if (s == std::string::npos || s == 0) return "/";
	return p.substr(0, s);
}

static int countVgm(const std::string& dir)
{
	DIR* d = opendir(dir.c_str());
	if (d == NULL) return 0;
	int n = 0;
	struct dirent* e;
	while ((e = readdir(d)) != NULL)
	{
		std::string name(e->d_name);
		if (hasExt(name, ".vgm") || hasExt(name, ".vgz")) n ++;
	}
	closedir(d);
	return n;
}

static int entryRank(const BrowserEntry& e)
{
	if (e.name == "..") return 0;
	if (e.isDir) return 1;
	return 2;
}

static bool browserLess(const BrowserEntry& a, const BrowserEntry& b)
{
	int ra = entryRank(a), rb = entryRank(b);
	if (ra != rb) return ra < rb;
	return ciLess(a.name, b.name);
}

static void loadBrowserDir(const std::string& path)
{
	g_browserDir = path;
	g_entries.clear();
	DIR* d = opendir(path.c_str());
	if (d == NULL)
	{
		g_browserMsg = "Cannot open directory";
		g_browserMsgAt = SDL_GetTicks();
		return;
	}
	struct dirent* e;
	int budget = 40;   // limit per-directory stat work on huge folders
	while ((e = readdir(d)) != NULL)
	{
		std::string name(e->d_name);
		if (name == "." || name == "..") continue;
		if (name.empty() || name[0] == '.') continue;   // hide dotfiles
		std::string full = joinPath(path, name);
		bool dir = isDirectory(full);
		BrowserEntry ent;
		ent.name = name;
		ent.path = full;
		ent.isDir = dir;
		ent.vgmCount = (dir && budget > 0) ? (budget --, countVgm(full)) : 0;
		g_entries.push_back(ent);
	}
	closedir(d);

	BrowserEntry up;
	up.name = "..";
	up.path = parentPath(path);
	up.isDir = true;
	up.vgmCount = -1;
	g_entries.push_back(up);

	std::sort(g_entries.begin(), g_entries.end(), browserLess);
	g_browserSel = 0;
	g_browserTop = 0;
}

// -------------------------------------------------------------- audio output
static void audioCallback(void* userdata, Uint8* stream, int len)
{
	(void)userdata;
	if (g_paused || g_quit || g_lock == NULL)
	{
		memset(stream, 0, (size_t)len);
		return;
	}
	SDL_LockMutex(g_lock);
	g_player.Render((UINT32)len, stream);
	SDL_UnlockMutex(g_lock);
}

// ------------------------------------------------------------------- drawing
static unsigned int utf8Decode(const std::string& s, size_t i, int* len)
{
	unsigned char c = (unsigned char)s[i];
	if (c < 0x80) { *len = 1; return c; }
	if ((c & 0xE0) == 0xC0 && i + 1 < s.size())
	{
		*len = 2;
		return ((unsigned int)(c & 0x1F) << 6) | ((unsigned char)s[i+1] & 0x3F);
	}
	if ((c & 0xF0) == 0xE0 && i + 2 < s.size())
	{
		*len = 3;
		return ((unsigned int)(c & 0x0F) << 12) |
		       ((unsigned int)((unsigned char)s[i+1] & 0x3F) << 6) |
		       ((unsigned int)((unsigned char)s[i+2] & 0x3F));
	}
	if ((c & 0xF8) == 0xF0 && i + 3 < s.size())
	{
		*len = 4;
		return ((unsigned int)(c & 0x07) << 18) |
		       ((unsigned int)((unsigned char)s[i+1] & 0x3F) << 12) |
		       ((unsigned int)((unsigned char)s[i+2] & 0x3F) << 6) |
		       ((unsigned int)((unsigned char)s[i+3] & 0x3F));
	}
	*len = 1;
	return c;
}

static TTF_Font* pickFont(const FontPair& fp, unsigned int cp)
{
	return (cp >= 0x2E80) ? fp.cjk : fp.latin;
}

static int textWidth(const FontPair& fp, const std::string& text)
{
	int total = 0;
	size_t i = 0;
	while (i < text.size())
	{
		int l = 1;
		unsigned int cp = utf8Decode(text, i, &l);
		TTF_Font* f = pickFont(fp, cp);
		int w = 0, h = 0;
		if (f != NULL && TTF_SizeUTF8(f, text.substr(i, l).c_str(), &w, &h) == 0)
			total += w;
		i += l;
	}
	return total;
}

static void blitText(SDL_Surface* surf, const FontPair& fp, int x, int y,
                     const char* text, SDL_Color color)
{
	if (text == NULL || text[0] == '\0') return;
	std::string s(text);
	size_t i = 0;
	while (i < s.size())
	{
		int l = 1;
		unsigned int cp = utf8Decode(s, i, &l);
		TTF_Font* f = pickFont(fp, cp);
		if (f != NULL)
		{
			SDL_Surface* one = TTF_RenderUTF8_Blended(f, s.substr(i, l).c_str(), color);
			if (one != NULL)
			{
				SDL_Rect dst;
				dst.x = (Sint16)x; dst.y = (Sint16)y;
				dst.w = one->w; dst.h = one->h;
				SDL_BlitSurface(one, NULL, surf, &dst);
				x += one->w;
				SDL_FreeSurface(one);
			}
		}
		i += l;
	}
}

static void blitTextRight(SDL_Surface* surf, const FontPair& fp, int right, int y,
                          const char* text, SDL_Color color)
{
	if (text == NULL) return;
	blitText(surf, fp, right - textWidth(fp, std::string(text)), y, text, color);
}

// Keep the tail of a long string so the most relevant part stays visible.
static std::string fitTail(const std::string& s, int maxW, const FontPair& fp)
{
	if (textWidth(fp, s) <= maxW) return s;
	for (size_t pos = 0; pos < s.size(); pos ++)
	{
		std::string cand = "..." + s.substr(pos);
		if (textWidth(fp, cand) <= maxW) return cand;
	}
	return "...";
}

// UTF-8 aware word wrap; returns Y after last drawn line.
static int blitWrapped(SDL_Surface* surf, const FontPair& fp, int x, int y,
                       int maxW, const std::string& text, SDL_Color color,
                       int maxLines)
{
	if (text.empty() || fp.latin == NULL) return y;
	int lh = TTF_FontHeight(fp.latin) + 1;
	std::string line;
	int lines = 0;
	size_t i = 0;
	while (i < text.size())
	{
		int slen = 1;
		unsigned int cp = utf8Decode(text, i, &slen);
		if (cp == '\n')
		{
			blitText(surf, fp, x, y, line.c_str(), color);
			y += lh;
			line.clear();
			if (++lines >= maxLines) return y;
			i += slen;
			continue;
		}
		std::string piece = text.substr(i, slen);
		std::string cand = line + piece;
		if (textWidth(fp, cand) > maxW && !line.empty())
		{
			blitText(surf, fp, x, y, line.c_str(), color);
			y += lh;
			line.clear();
			if (++lines >= maxLines) return y;
			cand = piece;
		}
		line += piece;
		i += slen;
	}
	if (!line.empty() && lines < maxLines)
	{
		blitText(surf, fp, x, y, line.c_str(), color);
		y += lh;
	}
	return y;
}

static void fillRect(SDL_Surface* s, int x, int y, int w, int h, Uint32 col)
{
	SDL_Rect r;
	r.x = (Sint16)x; r.y = (Sint16)y; r.w = (Sint16)w; r.h = (Sint16)h;
	SDL_FillRect(s, &r, col);
}

static void drawBrowser(void)
{
	SDL_Color cWhite = {235, 242, 250, 0};
	SDL_Color cDim   = {150, 170, 190, 0};
	SDL_Color cLabel = {110, 200, 220, 0};
	SDL_Color cTitle = {255, 220, 110, 0};
	SDL_Color cGreen = {120, 235, 160, 0};
	SDL_Color cRed   = {255, 150, 130, 0};

	SDL_FillRect(g_screen, NULL, SDL_MapRGB(g_screen->format, 12, 16, 26));
	fillRect(g_screen, 0, 0, SCREEN_W, 22, SDL_MapRGB(g_screen->format, 22, 66, 88));
	blitText(g_screen, g_fontTitle, 8, 2, "FILE BROWSER", cWhite);
	blitTextRight(g_screen, g_fontSmall, SCREEN_W - 8, 6, "MENU=EXIT", cDim);

	std::string path = fitTail(g_browserDir, SCREEN_W - 16, g_fontSmall);
	blitText(g_screen, g_fontSmall, 8, 26, path.c_str(), cLabel);
	fillRect(g_screen, 0, 40, SCREEN_W, 1, SDL_MapRGB(g_screen->format, 40, 120, 150));

	const int ROW = 14;
	const int TOP = 44;
	const int MAXROW = 11;
	if (g_browserSel < g_browserTop) g_browserTop = g_browserSel;
	if (g_browserSel >= g_browserTop + MAXROW) g_browserTop = g_browserSel - MAXROW + 1;
	if (g_browserTop < 0) g_browserTop = 0;

	for (int r = 0; r < MAXROW; r ++)
	{
		int i = g_browserTop + r;
		if (i >= (int)g_entries.size()) break;
		int y = TOP + r * ROW;
		const BrowserEntry& e = g_entries[i];
		if (i == g_browserSel)
			fillRect(g_screen, 4, y - 1, SCREEN_W - 8, ROW,
			         SDL_MapRGB(g_screen->format, 30, 80, 100));

		const char* mark = "[F]";
		if (e.name == "..") mark = "[..]";
		else if (e.isDir) mark = "[D]";
		blitText(g_screen, g_fontSmall, 8, y, mark, e.isDir ? cGreen : cDim);

		std::string nm = fitTail(e.name, 150, g_fontSmall);
		blitText(g_screen, g_fontSmall, 44, y, nm.c_str(),
		         i == g_browserSel ? cWhite : cDim);

		if (e.isDir && e.vgmCount > 0)
		{
			char cnt[32];
			snprintf(cnt, sizeof(cnt), "%d vgm", e.vgmCount);
			blitTextRight(g_screen, g_fontSmall, SCREEN_W - 8, y, cnt, cGreen);
		}
	}

	fillRect(g_screen, 0, 202, SCREEN_W, 38, SDL_MapRGB(g_screen->format, 18, 26, 38));
	blitText(g_screen, g_fontSmall, 8, 205,
	         "UP/DN MOVE  LEFT BACK  RIGHT ENTER", cDim);
	blitText(g_screen, g_fontSmall, 8, 218,
	         "SELECT USE DIR   MENU EXIT", cTitle);
	if (!g_browserMsg.empty() && SDL_GetTicks() - g_browserMsgAt < 3500)
		blitText(g_screen, g_fontSmall, 8, 231, g_browserMsg.c_str(), cRed);

	SDL_Flip(g_screen);
}

static void draw(void)
{
	if (g_screen == NULL) return;
	if (g_browserOpen) { drawBrowser(); return; }

	SDL_Color cWhite = {235, 242, 250, 0};
	SDL_Color cDim   = {150, 170, 190, 0};
	SDL_Color cLabel = {110, 200, 220, 0};
	SDL_Color cTitle = {255, 220, 110, 0};
	SDL_Color cGreen = {120, 235, 160, 0};

	SDL_FillRect(g_screen, NULL, SDL_MapRGB(g_screen->format, 12, 16, 26));

	// header
	fillRect(g_screen, 0, 0, SCREEN_W, 24, SDL_MapRGB(g_screen->format, 22, 66, 88));
	blitText(g_screen, g_fontTitle, 8, 3, "VGM PLAYER", cWhite);
	char dlabel[32];
	snprintf(dlabel, sizeof(dlabel), "D%d/%d",
	         g_dirs.empty() ? 0 : g_dirIdx + 1, (int)g_dirs.size());
	blitText(g_screen, g_fontSmall, 112, 7, dlabel, cLabel);
	char pos[32];
	snprintf(pos, sizeof(pos), "%d/%d", g_files.empty() ? 0 : g_cur + 1,
	         (int)g_files.size());
	blitTextRight(g_screen, g_fontText, SCREEN_W - 8, 6, pos, cGreen);
	fillRect(g_screen, 0, 24, SCREEN_W, 1, SDL_MapRGB(g_screen->format, 40, 120, 150));

	// title
	int y = 32;
	y = blitWrapped(g_screen, g_fontTitle, 8, y, SCREEN_W - 16,
	                g_title.empty() ? "(untitled)" : g_title, cTitle, 2);
	y += 3;

	// detail lines
	char buf[512];
	if (g_showInfo)
	{
		if (!g_game.empty())   { snprintf(buf,sizeof(buf),"GAME    %s",g_game.c_str());   blitText(g_screen,g_fontText,8,y,buf,cDim); y+=15; }
		if (!g_system.empty()) { snprintf(buf,sizeof(buf),"SYSTEM  %s",g_system.c_str()); blitText(g_screen,g_fontText,8,y,buf,cDim); y+=15; }
		if (!g_author.empty()) { snprintf(buf,sizeof(buf),"AUTHOR  %s",g_author.c_str()); blitText(g_screen,g_fontText,8,y,buf,cDim); y+=15; }
		if (!g_date.empty())   { snprintf(buf,sizeof(buf),"DATE    %s",g_date.c_str());   blitText(g_screen,g_fontText,8,y,buf,cDim); y+=15; }
		if (!g_chips.empty())  { snprintf(buf,sizeof(buf),"CHIPS   %s",g_chips.c_str());  blitText(g_screen,g_fontText,8,y,buf,cLabel); y+=15; }
		if (!g_comment.empty())
		{
			snprintf(buf,sizeof(buf),"COMMENT %s",g_comment.c_str());
			y = blitWrapped(g_screen, g_fontSmall, 8, y, SCREEN_W - 16, buf, cDim, 2);
		}
	}
	else
	{
		if (!g_game.empty())   { snprintf(buf,sizeof(buf),"GAME    %s",g_game.c_str());   blitText(g_screen,g_fontText,8,y,buf,cDim); y+=15; }
		if (!g_system.empty()) { snprintf(buf,sizeof(buf),"SYSTEM  %s",g_system.c_str()); blitText(g_screen,g_fontText,8,y,buf,cDim); y+=15; }
	}
	if (!g_filebase.empty())
	{
		snprintf(buf, sizeof(buf), "FILE    %s", g_filebase.c_str());
		blitText(g_screen, g_fontSmall, 8, y, buf, cDim);
		y += 13;
	}

	// decorative level bars (always at fixed position)
	int baseY = 158;
	Uint32 barCol = SDL_MapRGB(g_screen->format, 42, 110, 130);
	for (int i = 0; i < 28; i ++)
	{
		double phase = (double)SDL_GetTicks() / 130.0 + i * 0.55;
		double v = (sin(phase) + sin(phase * 0.47 + 1.3) + 2.0) / 4.0;
		int h = 2 + (int)(v * 12.0);
		fillRect(g_screen, 8 + i * 11, baseY - h, 7, h, barCol);
	}

	// progress bar
	int py = 174;
	fillRect(g_screen, 8, py, SCREEN_W - 16, 8, SDL_MapRGB(g_screen->format, 34, 44, 58));
	double cur = g_player.GetCurTime(PLAYTIME_LOOP_INCL | PLAYTIME_TIME_FILE);
	double ratio = (g_totalTime > 0.0) ? (cur / g_totalTime) : 0.0;
	if (ratio < 0.0) ratio = 0.0;
	if (ratio > 1.0) ratio = 1.0;
	if (ratio > 0.0)
		fillRect(g_screen, 9, py + 1, (int)((SCREEN_W - 18) * ratio), 6,
		         SDL_MapRGB(g_screen->format, 90, 210, 190));
	std::string t = fmtTime(cur) + " / " + fmtTime(g_totalTime);
	blitText(g_screen, g_fontSmall, 8, py + 11, t.c_str(), cWhite);

	if (g_paused)
		blitText(g_screen, g_fontText, SCREEN_W - 70, py + 10, "PAUSED", cTitle);

	if (!g_toast.empty() && SDL_GetTicks() - g_toastAt < 4000)
	{
		fillRect(g_screen, 4, 196, SCREEN_W - 8, 12,
		         SDL_MapRGB(g_screen->format, 30, 80, 100));
		std::string ts = fitTail(g_toast, SCREEN_W - 20, g_fontSmall);
		blitText(g_screen, g_fontSmall, 8, 197, ts.c_str(), cGreen);
	}

	// key legend
	fillRect(g_screen, 0, 208, SCREEN_W, 32, SDL_MapRGB(g_screen->format, 18, 26, 38));
	blitText(g_screen, g_fontSmall, 8, 211,
	         "Sh+X PREV  Sh+B NEXT  Sh+Y PLAY  Sh+A INFO", cDim);
	blitText(g_screen, g_fontSmall, 8, 225,
	         "Y/A DIR -/+  START BROWSE  MENU QUIT", cDim);

	SDL_Flip(g_screen);
}

// ---------------------------------------------------------------------- main
int main(int argc, char* argv[])
{
	loadDirList();

	std::string dirStr = g_dirs[g_dirIdx];
	const char* env = getenv("VGM_DIR");
	if (env != NULL && env[0] != '\0') dirStr = env;
	if (argc > 1) dirStr = argv[1];
	const char* dir = dirStr.c_str();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	if (TTF_Init() != 0)
	{
		fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
		SDL_Quit();
		return 1;
	}

	g_screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 16, SDL_SWSURFACE | SDL_FULLSCREEN);
	if (g_screen == NULL)
	{
		fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_WM_SetCaption("VGM Player", "VGM Player");
	SDL_ShowCursor(SDL_DISABLE);

	g_fontTitle.latin = TTF_OpenFont(FONT_PATH, 17);
	g_fontTitle.cjk   = TTF_OpenFont(FONT_CJK_PATH, 17);
	g_fontText.latin  = TTF_OpenFont(FONT_PATH, 11);
	g_fontText.cjk    = TTF_OpenFont(FONT_CJK_PATH, 11);
	g_fontSmall.latin = TTF_OpenFont(FONT_PATH, 10);
	g_fontSmall.cjk   = TTF_OpenFont(FONT_CJK_PATH, 10);
	if (g_fontTitle.latin == NULL || g_fontTitle.cjk == NULL ||
	    g_fontText.latin == NULL || g_fontText.cjk == NULL ||
	    g_fontSmall.latin == NULL || g_fontSmall.cjk == NULL)
	{
		fprintf(stderr, "Font load failed: %s\n", TTF_GetError());
		return 1;
	}

	g_lock = SDL_CreateMutex();
	if (g_lock == NULL)
	{
		fprintf(stderr, "SDL_CreateMutex failed\n");
		return 1;
	}

	PlayerA::Config cfg = g_player.GetConfiguration();
	cfg.loopCount = 1;
	cfg.fadeSmpls = 0;
	cfg.endSilenceSmpls = SAMPLE_RATE / 4;
	g_player.SetConfiguration(cfg);
	g_player.RegisterPlayerEngine(new VGMPlayer);
	g_player.SetOutputSettings(SAMPLE_RATE, 2, 16, 4096);

	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = SAMPLE_RATE;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 2048;
	desired.callback = audioCallback;
	desired.userdata = NULL;
	if (SDL_OpenAudio(&desired, NULL) != 0)
	{
		fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
	}

	scanDir(dir);
	if (!g_files.empty()) switchTo(0, 1);
	else
	{
		g_title = "No .vgm/.vgz files found";
		g_game = dir;
		SDL_LockMutex(g_lock);
		g_player.Stop();
		g_player.UnloadFile();
		SDL_UnlockMutex(g_lock);
	}

	SDL_PauseAudio(0);

	while (!g_quit)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			if (ev.type == SDL_QUIT) { g_quit = 1; break; }
			if (ev.type != SDL_KEYDOWN) continue;
			SDLKey key = ev.key.keysym.sym;

			if (g_browserOpen)
			{
				switch (key)
				{
				case SDLK_UP:
					if (g_browserSel > 0) g_browserSel --;
					break;
				case SDLK_DOWN:
					if (g_browserSel + 1 < (int)g_entries.size()) g_browserSel ++;
					break;
				case SDLK_LEFT: case SDLK_k: case SDLK_l: case SDLK_y: case SDLK_u:
					if (g_browserDir != "/")
					{
						loadBrowserDir(parentPath(g_browserDir));
						g_browserMsg = "";
					}
					break;
				case SDLK_RIGHT: case SDLK_j: case SDLK_h: case SDLK_o: case SDLK_i:
					if (!g_entries.empty())
					{
						loadBrowserDir(g_entries[g_browserSel].path);
						g_browserMsg = "";
					}
					break;
				case SDLK_SPACE:   // SELECT: use highlighted dir as playback dir
					{
						std::string target = g_browserDir;
						if (!g_entries.empty())
						{
							const BrowserEntry& e = g_entries[g_browserSel];
							target = e.isDir ? e.path : parentPath(e.path);
						}
						g_browserOpen = 0;
						if (setPlaybackDir(target, true))
							showToast("SAVED + PLAYING: " + fitTail(target, 150, g_fontSmall));
						else
							showToast("NO PLAYABLE VGM IN THIS DIR");
					}
					break;
				case SDLK_ESCAPE:   // MENU: close browser without changes
					g_browserOpen = 0;
					g_browserMsg = "";
					break;
				default: break;
				}
				continue;
			}

			// ------------------------------- player mode
			switch (key)
			{
			case SDLK_RETURN:   // START: open file browser
				g_browserOpen = 1;
				g_browserOpenedAt = SDL_GetTicks();
				g_browserMsg = "";
				loadBrowserDir(dirOfCurrent());
				break;
			case SDLK_y: case SDLK_u: case SDLK_LEFT:
				switchTo(g_cur - 1, -1); break;
			case SDLK_l: case SDLK_k: case SDLK_RIGHT:
				switchTo(g_cur + 1, 1); break;
			case SDLK_o:                 // Shift+Y: play/pause
				g_paused = !g_paused; break;
			case SDLK_h:                 // Shift+A: info toggle
				g_showInfo = !g_showInfo; break;
			case SDLK_i:                 // Y: previous saved directory
				switchDir(-1); break;
			case SDLK_j:                 // A: next saved directory
				switchDir(1); break;
			case SDLK_ESCAPE: case SDLK_q:   // MENU: quit player
				g_quit = 1; break;
			default: break;
			}
		}
		if (g_quit) break;

		if (!g_browserOpen && !g_paused && !g_files.empty() && (g_player.GetState() & PLAYSTATE_FIN))
		{
			Uint32 now = SDL_GetTicks();
			if (g_lastAuto == 0 || now - g_lastAuto > 800)
			{
				g_lastAuto = now;
				switchTo(g_cur + 1, 1);
			}
		}

		draw();
		SDL_Delay(30);
	}

	SDL_PauseAudio(1);
	SDL_CloseAudio();

	SDL_LockMutex(g_lock);
	g_player.Stop();
	g_player.UnloadFile();
	if (g_loader != NULL) { DataLoader_Deinit(g_loader); g_loader = NULL; }
	SDL_UnlockMutex(g_lock);
	g_player.UnregisterAllPlayers();

	SDL_DestroyMutex(g_lock);
	if (g_fontTitle.latin) TTF_CloseFont(g_fontTitle.latin);
	if (g_fontTitle.cjk)   TTF_CloseFont(g_fontTitle.cjk);
	if (g_fontText.latin)  TTF_CloseFont(g_fontText.latin);
	if (g_fontText.cjk)    TTF_CloseFont(g_fontText.cjk);
	if (g_fontSmall.latin) TTF_CloseFont(g_fontSmall.latin);
	if (g_fontSmall.cjk)   TTF_CloseFont(g_fontSmall.cjk);
	TTF_Quit();
	SDL_Quit();
	return 0;
}
