/*
 * todext — extract TODO/BUG/NOTE marked comments from source files
 *
 * Uses ripgrep (rg) if available, falls back to grep.
 * Multiline detection: a comment line following a TODO/BUG/NOTE match at the same
 * or deeper indentation level (and beginning with a comment marker or pure
 * whitespace+text) is treated as a continuation.
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

static const char* VERSION = "1.2";

// ---------------------------------------------------------------------------
// ANSI colours
// ---------------------------------------------------------------------------
namespace clr {
static bool enabled = true;

static const char* reset()  { return enabled ? "\033[0m"  : ""; }
static const char* bold()   { return enabled ? "\033[1m"  : ""; }
static const char* red()    { return enabled ? "\033[31m" : ""; }
static const char* yellow() { return enabled ? "\033[33m" : ""; }
static const char* cyan()   { return enabled ? "\033[36m" : ""; }
static const char* green()  { return enabled ? "\033[32m" : ""; }
static const char* dim()    { return enabled ? "\033[2m"  : ""; }
static const char* magenta(){ return enabled ? "\033[35m" : ""; }
}

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------
enum class TagKind { TODO, BUG, NOTE, UNKNOWN };

struct Entry {
    std::string file;
    int         line   = 0;
    TagKind     kind   = TagKind::UNKNOWN;
    std::string text;           // full comment body (possibly multiline)
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// Returns true if 'needle' appears as a whole word (case-insensitive) in 'hay'.
// A "word" boundary means the character before and after must not be [A-Za-z0-9_].
static bool containsWholeWord(const std::string& hay, const std::string& needle) {
    std::string up = hay;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    size_t pos = 0;
    while ((pos = up.find(needle, pos)) != std::string::npos) {
        bool leftOk  = (pos == 0) || (!std::isalnum((unsigned char)up[pos - 1]) && up[pos - 1] != '_');
        bool rightOk = (pos + needle.size() >= up.size()) ||
                       (!std::isalnum((unsigned char)up[pos + needle.size()]) &&
                        up[pos + needle.size()] != '_');
        if (leftOk && rightOk) return true;
        ++pos;
    }
    return false;
}

static TagKind classifyTag(const std::string& s) {
    if (containsWholeWord(s, "TODO")) return TagKind::TODO;
	if (containsWholeWord(s, "NOTE")) return TagKind::NOTE;
    if (containsWholeWord(s, "BUG"))  return TagKind::BUG;
    return TagKind::UNKNOWN;
}

static int leadingSpaces(const std::string& s) {
    int n = 0;
    for (char c : s) {
        if (c == ' ')       ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

// Returns true if the line is a comment (starts with a recognised comment marker
// after optional whitespace). This filters out code lines that happen to contain
// the word TODO/BUG in a string literal, variable name, logger call, etc.
static bool isCommentLine(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i >= s.size()) return false;
    char c = s[i];
    if (c == '#' || c == ';') return true;
    if (c == '/' && i + 1 < s.size() && (s[i+1] == '/' || s[i+1] == '*')) return true;
    if (c == '-' && i + 1 < s.size() && s[i+1] == '-') return true;
    // block-comment body line: starts with '*' not followed by '/'
    if (c == '*' && !(i + 1 < s.size() && s[i+1] == '/')) return true;
    return false;
}

// Strip the comment marker prefix from a raw source line so we only store
// the human-readable text. Handles //, #, --, *, ; style markers.
// Uses manual scanning to avoid regex backtracking on long lines.
static std::string stripCommentMarker(const std::string& raw) {
    size_t i = 0;
    // skip leading whitespace
    while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t')) ++i;
    // skip one or more repeated comment-marker characters
    char first = i < raw.size() ? raw[i] : '\0';
    if (first == '/' || first == '#' || first == '-' || first == '*' || first == ';') {
        while (i < raw.size() && raw[i] == first) ++i;
        // skip one optional space after the marker
        if (i < raw.size() && raw[i] == ' ') ++i;
        return raw.substr(i);
    }
    // block-comment continuation: line is purely indented text — strip leading ws
    if (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t')) {
        size_t start = 0;
        while (start < raw.size() && (raw[start] == ' ' || raw[start] == '\t')) ++start;
        size_t end = raw.size();
        while (end > start && (raw[end-1] == ' ' || raw[end-1] == '\t')) --end;
        return raw.substr(start, end - start);
    }
    return raw;
}

// Extract the text after "TODO:" / "BUG:" from the first matched line.
// Avoids regex on the full line to prevent backtracking on long/minified lines.
static std::string extractTag(const std::string& raw) {
    std::string stripped = stripCommentMarker(raw);

    // Find TODO, BUG, or NOTE (case-insensitive) in the stripped line
    auto findTag = [](const std::string& s) -> size_t {
        std::string up = s;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        size_t p = up.find("TODO");
        if (p != std::string::npos) return p;
        p = up.find("NOTE");
        if (p != std::string::npos) return p;
        p = up.find("BUG");
        return p;
    };

    auto extractAfterTag = [](const std::string& s, size_t tagPos) -> std::string {
        // skip "TODO" or "BUG"
        size_t i = tagPos;
        while (i < s.size() && s[i] != ':' && s[i] != ' ' && s[i] != '\t') ++i;
        // skip optional colon and whitespace
        if (i < s.size() && s[i] == ':') ++i;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        std::string result = s.substr(i);
        // strip trailing block-comment closers (*/, *))
        for (const char* suffix : {"*/", "*)"}) {
            size_t sf = result.rfind(suffix);
            if (sf != std::string::npos && sf + 2 >= result.size() - 2) {
                result = result.substr(0, sf);
                // trim trailing whitespace
                while (!result.empty() && (result.back() == ' ' || result.back() == '\t'))
                    result.pop_back();
            }
        }
        return result;
    };

    size_t pos = findTag(stripped);
    if (pos != std::string::npos)
        return extractAfterTag(stripped, pos);

    pos = findTag(raw);
    if (pos != std::string::npos)
        return extractAfterTag(raw, pos);

    return stripped;
}

// Is this line plausibly a comment continuation?
static bool isContinuation(const std::string& line, int baseIndent) {
    if (line.empty()) return false;
    int ind = leadingSpaces(line);
    if (ind < baseIndent) return false;   // dedented — new block
    // Check for comment-marker characters manually (avoids lookahead regex)
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i < line.size()) {
        char c = line[i];
        if (c == '/' && i + 1 < line.size() && line[i+1] == '/') return true;
        if (c == '#' || c == ';') return true;
        if (c == '-' && i + 1 < line.size() && line[i+1] == '-') return true;
        // '*' only if not followed by '/' (block-comment close)
        if (c == '*' && !(i + 1 < line.size() && line[i+1] == '/')) return true;
    }
    // plain indented text (e.g. inside /* ... */ blocks)
    if (ind > baseIndent) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Tool detection
// ---------------------------------------------------------------------------
static bool commandExists(const std::string& cmd) {
    std::string probe = "command -v " + cmd + " > /dev/null 2>&1";
    return std::system(probe.c_str()) == 0;
}

// ---------------------------------------------------------------------------
// Run search tool and return stdout as a string
// Each output line format:  <file>:<lineno>:<content>
// ---------------------------------------------------------------------------
static std::string runSearch(const std::string& path, bool useRg) {
    std::string tmpFile = "/tmp/todext_out_" + std::to_string(getpid());
    std::string cmd;
    if (useRg) {
        // rg: --vimgrep gives file:line:col:content; we parse file:line:content
        cmd = "rg --vimgrep -i -e '\\bTODO\\b' -e '\\bBUG\\b' -e '\\bNOTE\\b' "
              "--glob '!*.{o,a,so,dll,exe,bin,jpg,jpeg,png,gif,pdf,zip,tar,gz,ipynb}' "
              "\"" + path + "\" > \"" + tmpFile + "\" 2>/dev/null";
    } else {
        cmd = "grep -rn --include='*' "
              "--exclude='*.o' --exclude='*.a' --exclude='*.so' --exclude='*.ipynb' "
              "-iE '\\b(TODO|BUG|NOTE)\\b' "
              "\"" + path + "\" > \"" + tmpFile + "\" 2>/dev/null";
    }
    std::system(cmd.c_str());

    std::ifstream f(tmpFile);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::remove(tmpFile.c_str());
    return ss.str();
}

// ---------------------------------------------------------------------------
// Parse raw search output into (file, lineno, rawLine) triples
// rg --vimgrep:  file:line:col:content
// grep -n:       file:line:content
// ---------------------------------------------------------------------------
struct RawHit {
    std::string file;
    int         line;
    std::string content;
};

static std::vector<RawHit> parseSearchOutput(const std::string& raw, bool isRg) {
    std::vector<RawHit> hits;
    std::istringstream ss(raw);
    std::string ln;
    while (std::getline(ss, ln)) {
        if (ln.empty()) continue;
        // Split on ':' — be careful with Windows-style C:\ paths (not our
        // concern on Linux, but we guard anyway).
        // Format for rg: file:line:col:content
        // Format for grep: file:line:content
        // We find the first colon, then the second (which should be a digit
        // group), then the rest.
        auto firstColon = ln.find(':');
        if (firstColon == std::string::npos) continue;

        std::string file = ln.substr(0, firstColon);
        std::string rest = ln.substr(firstColon + 1);

        auto secondColon = rest.find(':');
        if (secondColon == std::string::npos) continue;

        std::string lineStr = rest.substr(0, secondColon);
        std::string afterLine = rest.substr(secondColon + 1);

        std::string content;
        if (isRg) {
            // afterLine = col:content
            auto thirdColon = afterLine.find(':');
            if (thirdColon == std::string::npos) continue;
            content = afterLine.substr(thirdColon + 1);
        } else {
            content = afterLine;
        }

        // lineStr must be numeric
        bool numeric = !lineStr.empty() &&
            std::all_of(lineStr.begin(), lineStr.end(), ::isdigit);
        if (!numeric) continue;

        hits.push_back({file, std::stoi(lineStr), content});
    }
    return hits;
}

// ---------------------------------------------------------------------------
// Read a specific line from a file by line number (1-indexed)
// ---------------------------------------------------------------------------
static std::string readLine(const std::string& file, int lineNo) {
    std::ifstream f(file);
    if (!f) return "";
    std::string ln;
    for (int i = 1; i <= lineNo; ++i) {
        if (!std::getline(f, ln)) return "";
    }
    return ln;
}

// ---------------------------------------------------------------------------
// Build Entry list from raw hits, adding multiline continuation
// ---------------------------------------------------------------------------
static std::vector<Entry> buildEntries(const std::vector<RawHit>& hits) {
    // Group hits by file
    std::vector<Entry> entries;

    // For multiline we need to read ahead in the actual files.
    // Cache currently open file lines to avoid reopening repeatedly.
    std::string cachedFile;
    std::vector<std::string> cachedLines;

    auto loadFile = [&](const std::string& path) {
        if (path == cachedFile) return;
        cachedFile = path;
        cachedLines.clear();
        std::ifstream f(path);
        if (!f) return;
        std::string ln;
        while (std::getline(f, ln)) cachedLines.push_back(ln);
    };

    auto getLine = [&](const std::string& path, int no) -> const std::string& {
        loadFile(path);
        static const std::string empty;
        if (no < 1 || no > (int)cachedLines.size()) return empty;
        return cachedLines[no - 1];
    };

    for (const auto& hit : hits) {
        // Read the actual source line to verify it is a comment; the search
        // tool may return code lines that contain TODO/BUG in strings or names.
        const std::string& sourceLine = getLine(hit.file, hit.line);
        if (!isCommentLine(sourceLine)) continue;

        // Re-classify from the actual source line (richer than grep content).
        TagKind kind = classifyTag(sourceLine);
        if (kind == TagKind::UNKNOWN) kind = classifyTag(hit.content);
        if (kind == TagKind::UNKNOWN) continue;

        Entry e;
        e.file = hit.file;
        e.line = hit.line;
        e.kind = kind;

        std::string firstText = extractTag(hit.content);
        std::ostringstream body;
        body << firstText;

        // matchLine is the same as sourceLine already loaded above
        const std::string& matchLine = sourceLine;
        int baseIndent = leadingSpaces(matchLine);

        // Scan following lines for continuation
        int next = hit.line + 1;
        while (true) {
            const std::string& nxtLine = getLine(hit.file, next);
            if (nxtLine.empty() && next > (int)cachedLines.size()) break;
            if (!isContinuation(nxtLine, baseIndent + 1)) break;
            std::string cont = stripCommentMarker(nxtLine);
            if (!cont.empty()) {
                body << "\n  " << cont;
            }
            ++next;
        }

        e.text = body.str();
        // Trim trailing whitespace from text
        while (!e.text.empty() && std::isspace((unsigned char)e.text.back()))
            e.text.pop_back();

        entries.push_back(e);
    }
    return entries;
}

// ---------------------------------------------------------------------------
// Print to console with ANSI colour
// ---------------------------------------------------------------------------
static void printEntries(const std::vector<Entry>& entries) {
    int maxLine = 0;
    for (const auto& e : entries)
        if (e.line > maxLine) maxLine = e.line;
    int lineWidth = (int)std::to_string(maxLine).size();

    std::string lastFile;
    for (const auto& e : entries) {
        if (e.file != lastFile) {
            std::cout << "\n"
                      << clr::bold() << clr::cyan()
                      << e.file
                      << clr::reset() << "\n";
            lastFile = e.file;
        }

        const char* tagColor = (e.kind == TagKind::BUG)  ? clr::red()  :
                                 (e.kind == TagKind::NOTE) ? clr::cyan() : clr::yellow();
        // [TODO] = 6 chars, [BUG]  = 6 chars (trailing space keeps text column fixed)
		const char* tagLabel = "";
		switch (e.kind) {
			case TagKind::BUG:
				tagLabel = "BUG";
				break;
			case TagKind::TODO:
				tagLabel = "TODO";
				break;
			case TagKind::NOTE:
				tagLabel = "NOTE";
				break;
			default:
				break;	
		}
        //  const char* tagLabel = (e.kind == TagKind::BUG) ? "BUG" : "TODO";
        const char* tagPad   = (e.kind == TagKind::BUG) ? " "   : "";

        std::string lineStr(lineWidth - (int)std::to_string(e.line).size(), ' ');

        std::cout << "  "
                  << clr::dim() << "line " << clr::reset()
                  << clr::bold() << clr::green() << lineStr << e.line << clr::reset()
                  << "  "
                  << tagColor << clr::bold() << "[" << tagLabel << "]" << tagPad << clr::reset()
                  << "  "
                  << clr::magenta() << e.text << clr::reset()
                  << "\n";
    }
    std::cout << "\n"
              << clr::bold() << clr::green()
              << entries.size() << " item(s) found."
              << clr::reset() << "\n";
}

// ---------------------------------------------------------------------------
// Export to Markdown
// ---------------------------------------------------------------------------
static void exportMarkdown(const std::vector<Entry>& entries,
                           const std::string& outPath) {
    std::ofstream f(outPath);
    if (!f) {
        std::cerr << "Error: cannot write to " << outPath << "\n";
        return;
    }

    f << "# TODO / BUG / NOTE Tracker\n\n";
    f << "_Generated by todext_\n\n";

    // Count totals
    int todos = 0, bugs = 0, notes = 0;
    for (const auto& e : entries) {
        if (e.kind == TagKind::TODO) ++todos;
        else if (e.kind == TagKind::BUG) ++bugs;
		else if (e.kind == TagKind::NOTE) ++notes;
    }
    f << "**Total:** " << entries.size()
      << "  |  **TODO:** " << todos
      << "  |  **NOTE:** " << notes
      << "  |  **BUG:** " << bugs << "\n\n";
    f << "---\n\n";

    std::string lastFile;
    for (const auto& e : entries) {
        if (e.file != lastFile) {
            f << "## `" << e.file << "`\n\n";
            lastFile = e.file;
        }
		const char* badge = "";
		switch (e.kind) {
			case TagKind::BUG:
				badge = "🐛 BUG";
				break;
			case TagKind::TODO:
				badge = "✅ TODO";
				break;
			case TagKind::NOTE:
				badge = "📝 NOTE";
				break;
			default:
				break;
		}
        //  const char* badge = (e.kind == TagKind::BUG) ? "🐛 BUG" : "✅ TODO";
        f << "- **" << badge << "** _(line " << e.line << ")_: " << e.text << "\n";
    }
    f << "\n";
    std::cout << clr::bold() << clr::green()
              << "Exported to " << outPath
              << clr::reset() << "\n";
}

// ---------------------------------------------------------------------------
// Open file at line number in an editor
// ---------------------------------------------------------------------------
static void openInEditor(const std::string& file, int line,
                         const std::string& editor) {
    // Determine which editor to use
    std::string editorCmd = editor;
    if (editorCmd.empty()) {
        const char* env = std::getenv("EDITOR");
        if (env && env[0] != '\0') {
            editorCmd = env;
        } else {
            std::cerr << "Error: no editor specified and $EDITOR is not set.\n";
            return;
        }
    }

    // Extract basename for heuristic matching
    std::string base = editorCmd;
    auto slash = base.rfind('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);

    std::string cmd;
    if (base == "code" || base == "code-insiders") {
        // VS Code uses --goto file:line
        cmd = editorCmd + " --goto \"" + file + ":" + std::to_string(line) + "\"";
    } else if (base == "subl" || base == "sublime" || base == "sublime_text") {
        // Sublime Text uses file:line
        cmd = editorCmd + " \"" + file + ":" + std::to_string(line) + "\"";
    } else {
        // vim, nvim, nano, emacs, etc. use +line file
        cmd = editorCmd + " +" + std::to_string(line) + " \"" + file + "\"";
    }

    std::cout << clr::dim() << "Opening: " << cmd << clr::reset() << "\n";
    std::system(cmd.c_str());
}

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------
static void usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [OPTIONS] [PATH]\n\n"
        "  PATH          File or directory to scan (default: current directory)\n\n"
        "Options:\n"
        "  -o FILE:LINE          Open a specific TODO/BUG location in an editor\n"
        "  -E EDITOR             Editor to use with -o (default: $EDITOR)\n"
        "  -e, --export [FILE]   Export to markdown (default: todo.md)\n"
        "  --no-color            Disable ANSI colour output\n"
        "  -v, --version         Show version\n"
        "  -h, --help            Show this help\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}
    std::string path = ".";
    bool doExport    = false;
    std::string exportFile = "todo.md";
    bool doOpen      = false;
    std::string openTarget;       // "file:line"
    std::string editorOverride;   // from -E

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "todext " << VERSION << "\n"
                      << "Copyright (C) 2026 Satwik Srivastava\n"
                      << "\n"
                      << "This is free software; you are free to change and redistribute it.\n"
                      << "There is NO WARRANTY, to the extent permitted by law.\n";
            return 0;
        } else if (arg == "--no-color") {
            clr::enabled = false;
        } else if (arg == "-o") {
            doOpen = true;
            if (i + 1 < argc) {
                openTarget = argv[++i];
            } else {
                std::cerr << "Error: -o requires a FILE:LINE argument.\n";
                return 1;
            }
        } else if (arg == "-E") {
            if (i + 1 < argc) {
                editorOverride = argv[++i];
            } else {
                std::cerr << "Error: -E requires an EDITOR argument.\n";
                return 1;
            }
        } else if (arg == "-e" || arg == "--export") {
            doExport = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                exportFile = argv[++i];
            }
        } else {
            path = arg;
        }
    }

    // Handle -o: open file at line and exit
    if (doOpen) {
        auto colonPos = openTarget.rfind(':');
        if (colonPos == std::string::npos || colonPos == 0 ||
            colonPos + 1 >= openTarget.size()) {
            std::cerr << "Error: -o expects FILE:LINE format.\n";
            return 1;
        }
        std::string file = openTarget.substr(0, colonPos);
        std::string lineStr = openTarget.substr(colonPos + 1);
        bool numeric = !lineStr.empty() &&
            std::all_of(lineStr.begin(), lineStr.end(), ::isdigit);
        if (!numeric) {
            std::cerr << "Error: line number must be numeric in '" << openTarget << "'.\n";
            return 1;
        }
        int lineNo = std::stoi(lineStr);
        if (!fs::exists(file)) {
            std::cerr << "Error: file does not exist: " << file << "\n";
            return 1;
        }
        openInEditor(file, lineNo, editorOverride);
        return 0;
    }

    // Validate path
    if (!fs::exists(path)) {
        std::cerr << "Error: path does not exist: " << path << "\n";
        return 1;
    }

    bool useRg = commandExists("rg");
    if (!useRg && !commandExists("grep")) {
        std::cerr << "Error: neither rg nor grep found in PATH.\n";
        return 1;
    }

    std::cout << clr::dim()
              << "Using " << (useRg ? "ripgrep (rg)" : "grep")
              << " to scan: " << path
              << clr::reset() << "\n";

    std::string raw  = runSearch(path, useRg);
    auto hits        = parseSearchOutput(raw, useRg);
    auto entries     = buildEntries(hits);

    if (entries.empty()) {
        std::cout << clr::bold() << "No TODO/BUG comments found.\n" << clr::reset();
        return 0;
    }

    printEntries(entries);

    if (doExport) {
        exportMarkdown(entries, exportFile);
    }

    return 0;
}
