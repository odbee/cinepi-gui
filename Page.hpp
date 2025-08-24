#ifndef PAGE_HPP
#define PAGE_HPP

#pragma once

#include "imgui.h"

#include "Application.hpp"

#include "EglBuffers.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
            yuv_preview("cinepi-gui/shaders/eglYuv.vert","cinepi-gui/shaders/eglYuv.frag")
            {
                console = spdlog::stdout_color_mt("viewport");
            }
        ~Viewport() {}

        void process();
        void init();

        virtual void show() override;

        Shader yuv_preview;

    private:
};
#endif // PAGE_HPP
