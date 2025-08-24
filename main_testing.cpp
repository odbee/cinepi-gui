#include "Application_testing.hpp"
#include "Page_testing.hpp"

#define DISPLAY_WIDTH 1280
#define DISPLAY_HEIGHT 720

int main()
{

    Application app;
    app.init(DISPLAY_WIDTH,DISPLAY_HEIGHT);
    printf("making viewport page");

    Page* viewport_page = make_viewport_page(app);
    Viewport* viewport = reinterpret_cast<Viewport*>(viewport_page);

    printf("done with viewport settup");

    bool done = false;
    while (!done)
    {
        app.buffers.update();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT){
                done = true;
            }
            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(app.window)){
                done = true;
            }
        }

        // ---- start render ---- //
        app.beginDraw();

        // when new frame ready
        if(app.buffers.new_frame())
        {
            FrameBuffer fb = app.buffers.getBuffer();

            // app.console->info(fb.framerate);
            // execute shaders and run image processing on new frame here

            // render the viewport shaders 
            viewport->process();
        }

        // show the viewport 
        viewport->show();
        // ---- end render ----- //
        app.endDraw();
    }

    app.cleanup();
    return 0;
}