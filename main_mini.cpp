#include <SDL.h>            // Core SDL library for window and event management.
#include <SDL_opengl.h>     // SDL's OpenGL support for rendering.
#include "imgui.h"          // The core ImGui library header.
#include "imgui_impl_sdl2.h" // ImGui's SDL2 backend for input handling.
#include "imgui_impl_opengl3.h" // ImGui's OpenGL3 backend for rendering.
#include <string> 
int main() {
    // 1. Initialization
    // Initialize the SDL video subsystem to create a window.
    SDL_Init(SDL_INIT_VIDEO);
    
    // Create an SDL window with OpenGL support.
    SDL_Window* window = SDL_CreateWindow("One Button",SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1), 240, 240, SDL_WINDOW_OPENGL);
    
    SDL_Log( std::to_string(SDL_GetNumVideoDisplays()).c_str());
    // Create an OpenGL context and link it to the window.
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    // Set up ImGui.
    // Ensure the version of ImGui is correct.
    IMGUI_CHECKVERSION();
    // Create the ImGui context, which holds all of its state.
    ImGui::CreateContext();
    // Initialize the ImGui backends for SDL2 and OpenGL.
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");
    // Set a dark color scheme for the UI.
    ImGui::StyleColorsDark();

    // 2. Main Loop
    bool done = false;
    SDL_Event e;
    while (!done) {
        // Event handling.
        // Poll all pending SDL events.
        while (SDL_PollEvent(&e)) {
            // Forward events to the ImGui backend to handle mouse and keyboard input.
            ImGui_ImplSDL2_ProcessEvent(&e);
            // Check if the user has clicked the window's close button.
            if (e.type == SDL_QUIT) {
                done = true;
            }
        }
        
        // Start a new ImGui frame.
        // This prepares ImGui to accept new UI elements.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 3. UI Code
        // Create a single button. If it's clicked, the "if" statement is true.
        if (ImGui::Button("Click Me")) {
            // Log a message to the console when the button is pressed.
            SDL_Log("Button pressed!");
        }

        // 4. Rendering
        // Convert the ImGui UI elements into a renderable command list.
        ImGui::Render();
        
        // Set the OpenGL viewport to match the window's size.
        glViewport(0, 0, 240, 240);
        
        // Clear the screen with a black color.
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render the ImGui draw data using the OpenGL3 backend.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Swap the window's buffers to display the rendered frame.
        SDL_GL_SwapWindow(window);
    }

    // 5. Cleanup
    // Clean up all the ImGui and SDL resources to prevent memory leaks.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}