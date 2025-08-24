#ifndef PAGE_HPP
#define PAGE_HPP

#pragma once

#include "imgui.h"

#include "Application_testing.hpp"

#include "EglBuffers.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

class Page {
    public:
        Page(Application& app) :
            app(app)
        {} 
        virtual ~Page() {} 

        virtual void show() = 0;

    protected:
        std::shared_ptr<spdlog::logger> console;
        Application& app;

    private:
        
};


Page *make_viewport_page(Application& app);



class Viewport : public Page
{
    public:
        Viewport(Application& app) : 
            Page(app),
            yuv_preview(get_shader_path("eglYuv.vert").c_str(), get_shader_path("eglYuv.frag").c_str())
            {
                console = spdlog::stdout_color_mt("viewport");
            }
        ~Viewport() {}

        void process();
        void init();

        virtual void show() override;

        Shader yuv_preview;

    private:
        static std::string get_shader_path(const std::string& filename) {

            char exe_path[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
            std::string path;
            if (len != -1) {
                exe_path[len] = '\0';
                path = std::string(dirname(exe_path)) + "/../shaders/" + filename;
            } else {
                path = "../shaders/" + filename;
            }
            return path;
        }
};
#endif // PAGE_HPP
