/*
 * ThorVG - Lottie to GIF converter
 */

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <cstring>

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

    void helpMsg()
    {
        cout <<
            "Usage:\n"
            "  tvg-lottie2gif <file|dir> [options]\n\n"
            "Options:\n"
            "  -r WxH       Resolution (e.g. 240x240)\n"
            "  -f FPS       Frames per second\n"
            "  -b RRGGBB    Background color (hex)\n\n"
            "Examples:\n"
            "  tvg-lottie2gif input.json\n"
            "  tvg-lottie2gif input.json -r 240x240 -f 24\n"
            "  tvg-lottie2gif folder -r 512x512 -b 000000\n";
    }

    bool validate(const string& name)
    {
        return name.size() > 5 && name.substr(name.size() - 5) == ".json";
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
        DWORD attr = GetFileAttributesA(path);
        return (attr != INVALID_FILE_ATTRIBUTES) &&
               (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st{};
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
    }

    bool convertOne(const string& in)
    {
        string out = in;
        out.replace(out.size() - 4, 4, ".gif");

        cout << "[INFO] " << in << " -> " << out << endl;

        if (Initializer::init() != Result::Success) {
            cerr << "[ERROR] ThorVG init failed\n";
            return false;
        }

        auto picture = Picture::gen();
        if (!picture || picture->load(in.c_str()) != Result::Success) {
            cerr << "[ERROR] Failed to load lottie\n";
            Initializer::term();
            return false;
        }

        float vw, vh;
        picture->size(&vw, &vh);

        float scale = min(
            static_cast<float>(width) / vw,
            static_cast<float>(height) / vh
        );

        picture->size(vw * scale, vh * scale);

        auto animation = Animation::gen();
        animation->picture(picture);

        auto saver = unique_ptr<Saver>(Saver::gen());
        if (!saver) {
            cerr << "[ERROR] GIF saver not enabled (check -Dsavers=gif)\n";
            Initializer::term();
            return false;
        }

        if (background) {
            auto bg = Shape::gen();
            bg->fill(r, g, b);
            bg->appendRect(0, 0, width, height);
            saver->background(bg);
        }

        if (saver->save(animation, out.c_str(), 0, fps) != Result::Success ||
            saver->sync() != Result::Success)
        {
            cerr << "[ERROR] Save failed\n";
            Initializer::term();
            return false;
        }

        Initializer::term();
        cout << "[OK] Generated " << out << endl;
        return true;
    }

    void handlePath(const string& input)
    {
        auto path = realPath(input.c_str());
        if (!path) {
            cerr << "[ERROR] Invalid path: " << input << endl;
            return;
        }

        if (isDirectory(path)) {
            cout << "[DIR] " << path << endl;
#ifdef _WIN32
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA((string(path) + "\\*").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) return;

            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                string name = fd.cFileName;
                if (validate(name))
                    convertOne(string(path) + "\\" + name);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
#else
            DIR* dir = opendir(path);
            if (!dir) return;
            while (auto* e = readdir(dir)) {
                if (e->d_type != DT_REG) continue;
                string name = e->d_name;
                if (validate(name))
                    convertOne(string(path) + "/" + name);
            }
            closedir(dir);
#endif
        } else {
            if (validate(input))
                convertOne(input);
            else
                cerr << "[ERROR] Not a .json file\n";
        }
    }

public:
    int run(int argc, char** argv)
    {
        vector<string> inputs;

        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] == '-') {
                if (!strcmp(argv[i], "-r") && i + 1 < argc) {
                    sscanf(argv[++i], "%ux%u", &width, &height);
                } else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
                    fps = atoi(argv[++i]);
                } else if (!strcmp(argv[i], "-b") && i + 1 < argc) {
                    uint32_t c = strtoul(argv[++i], nullptr, 16);
                    r = (c >> 16) & 0xff;
                    g = (c >> 8) & 0xff;
                    b = c & 0xff;
                    background = true;
                }
            } else {
                inputs.emplace_back(argv[i]);
            }
        }

        if (inputs.empty()) {
            helpMsg();
            return 0;
        }

        for (auto& i : inputs)
            handlePath(i);

        return 0;
    }
};

int main(int argc, char** argv)
{
    App app;
    return app.run(argc, argv);
}
