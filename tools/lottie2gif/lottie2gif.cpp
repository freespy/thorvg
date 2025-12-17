/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <string.h>
#include <vector>
#include <memory>
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
   char full[PATH_MAX];    //full path
   uint32_t fps = 30;
   uint32_t width = 600;
   uint32_t height = 600;
   uint8_t r, g, b;        //background color
   bool background = false;

   void helpMsg()
   {
      cout << "Usage: \n   tvg-lottie2gif [Lottie file] or [Lottie folder] [-r resolution] [-f fps] [-b background color]\n\nExamples: \n    $ tvg-lottie2gif input.json\n    $ tvg-lottie2gif input.json -r 600x600\n    $ tvg-lottie2gif input.json -f 30\n    $ tvg-lottie2gif input.json -r 600x600 -f 30\n    $ tvg-lottie2gif lottiefolder\n    $ tvg-lottie2gif lottiefolder -r 600x600 -f 30 -b fa7410\n\n";
   }

   bool validate(string& lottieName)
   {
      string extn = ".json";
      if (lottieName.size() <= extn.size() || lottieName.substr(lottieName.size() - extn.size()) != extn) {
         cerr << "Error: \"" << lottieName << "\" is invalid." << endl;
         return false;
      }
      return true;
   }

   bool convert(string& in, string& out)
   {
      if (Initializer::init() != Result::Success) {
         cerr << "Error: ThorVG initializer failed." << endl;
         return false;
      }
      bool success = false;
      {
         auto animation = Animation::gen();
         auto picture = animation->picture();
         if (picture->load(in.c_str()) != Result::Success) {
            cerr << "Error: Failed to load Lottie file: " << in << endl;
            return false;
         }

         float width_, height_;
         picture->size(&width_, &height_);
         float scale = static_cast<float>(this->width) / width_;
         picture->size(width_ * scale, height_ * scale);

         auto saver = unique_ptr<Saver>(Saver::gen());

         //set a background color
         if (background) {
            auto bg = Shape::gen();
            bg->fill(r, g, b);
            bg->appendRect(0, 0, width_ * scale, height_ * scale);
            saver->background(bg);
         }

         if (saver->save(animation, out.c_str(), 100, fps) != Result::Success) {
            cerr << "Error: Failed to save GIF: " << out << endl;
         } else if (saver->sync() != Result::Success) {
            cerr << "Error: Saver sync failed: " << out << endl;
         } else {
            success = true;
         }
      }
      if (Initializer::term() != Result::Success) {
         cerr << "Error: ThorVG termination failed." << endl;
      }
      return success;
   }

   void convert(string& lottieName)
   {
      auto gifName = lottieName;
      gifName.replace(gifName.length() - 4, 4, "gif");

      if (convert(lottieName, gifName)) {
         cout << "Generated Gif file : " << gifName << endl;
      } else {
         cerr << "Failed converting: " << lottieName << endl;
      }
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
      if (attr == INVALID_FILE_ATTRIBUTES) return false;
      return attr & FILE_ATTRIBUTE_DIRECTORY;
#else
      struct stat buf;
      if (stat(path, &buf) != 0) return false;
      return S_ISDIR(buf.st_mode);
#endif
   }

   bool handleDirectory(const string& path)
   {
#ifdef _WIN32
        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFileEx((path + "\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "Couldn't open directory \"" << path << "\"." << endl;
            return false;
        }
        do {
            if (*fd.cFileName == '.' || *fd.cFileName == '$') continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                string subpath = path + "\\" + fd.cFileName;
                if (!handleDirectory(subpath)) continue;
            } else {
                string lottieName(fd.cFileName);
                if (!validate(lottieName)) continue;
                lottieName = path + "\\" + fd.cFileName;
                convert(lottieName);
            }
        } while (FindNextFile(h, &fd));
        FindClose(h);
#else
        auto dir = opendir(path.c_str());
        if (!dir) {
            cerr << "Couldn't open directory \"" << path << "\"." << endl;
            return false;
        }
        while (auto entry = readdir(dir)) {
            if (*entry->d_name == '.' || *entry->d_name == '$') continue;
            if (entry->d_type == DT_DIR) {
                string subpath = path + "/" + entry->d_name;
                if (!handleDirectory(subpath)) continue;
            } else {
                string svgName(entry->d_name);
                if (!validate(svgName)) continue;
                svgName = path + "/" + entry->d_name;
                convert(svgName);
            }
        }
#endif
        return true;
    }

public:
   int setup(int argc, char** argv)
   {
      vector<const char*> inputs;

      for (int i = 1; i < argc; ++i) {
         const char* p = argv[i];
         if (*p == '-') {
            const char* p_arg = (i + 1 < argc) ? argv[++i] : nullptr;
            if (p[1] == 'r') {
               if (!p_arg) {
                  cerr << "Error: Missing resolution attribute." << endl;
                  return 1;
               }
               const char* x = strchr(p_arg, 'x');
               if (x) {
                  width = atoi(p_arg);
                  height = atoi(x + 1);
               }
               if (!x || width <= 0 || height <= 0) {
                  cerr << "Error: Resolution (" << p_arg << ") is corrupted." << endl;
                  return 1;
               }
            } else if (p[1] == 'f') {
               if (!p_arg) {
                  cerr << "Error: Missing fps value." << endl;
                  return 1;
               }
               fps = atoi(p_arg);
            } else if (p[1] == 'b') {
               if (!p_arg) {
                  cerr << "Error: Missing background color." << endl;
                  return 1;
               }
               auto bgColor = (uint32_t) strtol(p_arg, NULL, 16);
               r = (uint8_t)((bgColor & 0xff0000) >> 16);
               g = (uint8_t)((bgColor & 0x00ff00) >> 8);
               b = (uint8_t)(bgColor & 0x0000ff);
               background = true;
            } else {
               cerr << "Warning: Unknown flag (" << p << ")." << endl;
            }
         } else {
            inputs.push_back(argv[i]);
         }
      }

      if (inputs.empty()) {
         helpMsg();
         return 0;
      }

      for (auto input : inputs) {
         auto path = realPath(input);
         if (!path) {
            cerr << "Invalid file or path: \"" << input << "\"" << endl;
            continue;
         }
         if (isDirectory(path)) {
            cout << "Directory: \"" << path << "\"" << endl;
            if (!handleDirectory(path)) break;
         } else {
            string lottieName(input);
            if (!validate(lottieName)) continue;
            convert(lottieName);
         }
      }
      return 0;
   }
};

int main(int argc, char **argv)
{
   App app;
   return app.setup(argc, argv);
}
