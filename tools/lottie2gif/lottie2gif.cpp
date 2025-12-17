/*
 * Copyright (c) 2023 - 2025 the ThorVG project.
 * MIT License
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <thorvg.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <limits.h>
    #include <sys/stat.h>
#endif

using namespace std;
using namespace tvg;

struct App
{
private:
    char full[PATH_MAX]{};

    uint32_t fps = 30;
    uint32_t width = 600;
    uint32_t height = 600;

    uint8_t r = 0, g = 0, b = 0;
    bool background = false;

    string outputFile;

private:
    void helpMsg()
    {
        cout <<
        "Usage:\n"
        "  tvg-lottie2gif <file|dir> [options]\n\n"
        "Options:\n"
        "  -r WxH        Resolution (default 600x600)\n"
        "  -f FPS        Frames per second (default 30)\n"
        "  -b RRGGBB     Background color (hex)\n"
        "  -o FILE.gif   Output file (single input only)\n\n"
        "Examples:\n"
        "  tvg-lottie2gif anim.json\n"
        "  tvg-lottie2gif anim.json -r 240x240 -f 24 -o out.gif\n"
        "  tvg-lottie2gif lottie_folder\n\n";
    }

    bool validate(const string& name)
    {
        return name.size() > 5 &&
               name.substr(name.size() - 5) == ".json";
    }

    const char* realPath(const char* path)
    {
    #ifdef _WIN32
        return _fullpath(full, path, PATH_MAX);
    #else
        return realpath(path, full);
    #endif
    }

    bool isDirectory(const char* path)
    {
    #ifdef _WIN32
        DWORD attr = GetFileAttributes(path);
        return (attr != INVALID_FILE_ATTRIBUTES &&
                (attr & FILE_ATTRIBUTE_DIRECTORY));
    #else
        struct stat st{};
        return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    #endif
    }

    // ------------------------------
    // Core conversion (FIXED)
    // ------------------------------
    bool convert(const string& in, const string& out)
    {
        cout << "[INFO] Converting: " << in << endl;

        if (Initializer::init() != Result::Success) {
            cerr << "[ERROR] ThorVG init failed\n";
            return false;
        }

        {
            auto animation = Animation::gen();
            if (!animation) {
                cerr << "[ERROR] Animation::gen failed\n";
                return false;
            }

            if (animation->load(in.c_str()) != Result::Success) {
                cerr << "[ERROR] Failed to load lottie: " << in << endl;
                return false;
            }

            auto picture = animation->picture();
            if (!picture) {
                cerr << "[ERROR] animation->picture failed\n";
                return false;
            }

            float w, h;
            picture->size(&w, &h);

            float scaleX = static_cast<float>(width) / w;
            float scaleY = static_cast<float>(height) / h;
            float scale = min(scaleX, scaleY);

            picture->size(w * scale, h * scale);

            auto saver = unique_ptr<Saver>(Saver::gen());
            if (!saver) {
                cerr << "[ERROR] Saver::gen failed (GIF saver missing?)\n";
                return false;
            }

            if (background) {
                auto bg = Shape::gen();
                bg->fill(r, g, b);
                bg->appendRect(0, 0, w * scale, h * scale);
                saver->background(bg);
            }

            if (saver->save(animation, out.c_str(), 0, fps) != Result::Success) {
                cerr << "[ERROR] saver->save failed\n";
                return false;
            }

            if (saver->sync() != Result::Success) {
                cerr << "[ERROR] saver->sync failed\n";
                return false;
            }
        }

        Initializer::term();
        cout << "[OK] Generated: " << out << endl;
        return true;
    }

    void convertSingle(const string& input)
    {
        string out = outputFile.empty()
            ? input.substr(0, input.size() - 5) + ".gif"
            : outputFile;

        convert(input, out);
    }

    bool handleDirectory(const string& path)
    {
    #ifdef _WIN32
        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFileEx(
            (path + "\\*").c_str(),
            FindExInfoBasic, &fd,
            FindExSearchNameMatch,
            nullptr, 0);

        if (h == INVALID_HANDLE_VALUE) {
            cerr << "[ERROR] Cannot open directory: " << path << endl;
            return false;
        }

        do {
            if (fd.cFileName[0] == '.') continue;

            string fullpath = path + "\\" + fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                handleDirectory(fullpath);
            } else if (validate(fd.cFileName)) {
                convertSingle(fullpath);
            }
        } while (FindNextFile(h, &fd));

        FindClose(h);
    #else
        auto dir = opendir(path.c_str());
        if (!dir) return false;

        while (auto ent = readdir(dir)) {
            if (ent->d_name[0] == '.') continue;

            string fullpath = path + "/" + ent->d_name;

            if (ent->d_type == DT_DIR) {
                handleDirectory(fullpath);
            } else if (validate(ent->d_name)) {
                convertSingle(fullpath);
            }
        }
        closedir(dir);
    #endif
        return true;
    }

public:
    int setup(int argc, char** argv)
    {
        vector<string> inputs;

        for (int i = 1; i < argc; ++i) {
            const char* p = argv[i];

            if (*p == '-') {
                const char* arg = (i + 1 < argc) ? argv[++i] : nullptr;

                switch (p[1]) {
                case 'r': {
                    if (!arg) return 1;
                    auto x = strchr(arg, 'x');
                    if (!x) return 1;
                    width = atoi(arg);
                    height = atoi(x + 1);
                    break;
                }
                case 'f':
                    fps = arg ? atoi(arg) : fps;
                    break;
                case 'b': {
                    auto c = strtol(arg, nullptr, 16);
                    r = (c >> 16) & 0xff;
                    g = (c >> 8) & 0xff;
                    b = c & 0xff;
                    background = true;
                    break;
                }
                case 'o':
                    outputFile = arg ? arg : "";
                    break;
                default:
                    cout << "[WARN] Unknown option: " << p << endl;
                }
            } else {
                inputs.emplace_back(p);
            }
        }

        if (inputs.empty()) {
            helpMsg();
            return 0;
        }

        for (auto& in : inputs) {
            auto path = realPath(in.c_str());
            if (!path) continue;

            if (isDirectory(path)) {
                cout << "[INFO] Directory: " << path << endl;
                handleDirectory(path);
            } else {
                if (!validate(in)) {
                    cerr << "[WARN] Skipped non-lottie: " << in << endl;
                    continue;
                }
                convertSingle(in);
            }
        }
        return 0;
    }
};

int main(int argc, char** argv)
{
    App app;
    return app.setup(argc, argv);
}
