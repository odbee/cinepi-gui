#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <utility> 

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>



#include <SDL.h>
#include <SDL_opengl.h>

#include "shaders/Shader.hpp"

#include "SharedContext.hpp"
#include "EglBuffers.hpp"


class Application {
    public:
        Application() :
            buffers(cinepiraw),
            abortThread_(false)
        {
            console = spdlog::stdout_color_mt("application");
            console->info("Application Constructed");

            // main_thread_ = std::thread(std::bind(&Application::threadTask, this));

        } 
        ~Application()
        {   
            console->info("Application Destructed");

            abortThread_ = true;
            if (main_thread_.joinable()) {
                main_thread_.join();
            }       
        } 

        SDL_Window* window;
        SDL_GLContext gl_context;

        SharedContext cinepiraw;
        EglBuffers buffers;

        void init(unsigned int w, unsigned int h);
        void beginDraw();
        void endDraw();
        void cleanup();

        unsigned int app_width;
        unsigned int app_height;


        std::shared_ptr<spdlog::logger> console;

    private:
        
        bool abortThread_;
        std::thread main_thread_;

        void threadTask();

        void internal_overlay();
};

#endif // APPLICATION_HPP
