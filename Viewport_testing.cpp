#include "Page_testing.hpp"
#include "Utilities.hpp"

#include <utility>

std::pair<int, int> calculateNewImageSize(int screenWidth, int screenHeight, int imageWidth, int imageHeight) {
    // Calculate the scaling factor for both dimensions
    double widthScale = static_cast<double>(screenWidth) / imageWidth;
    double heightScale = static_cast<double>(screenHeight) / imageHeight;

    // Use the smaller scaling factor to keep the image within screen bounds
    double scale = std::min(widthScale, heightScale);


    // Calculate the new dimensions
    int newWidth = static_cast<int>(imageWidth * scale);
    int newHeight = static_cast<int>(imageHeight * scale);
    SDL_Log("Screen: %dx%d, Image: %dx%d, Scale: %f, New: %dx%d", 
        screenWidth, screenHeight, imageWidth, imageHeight, scale, newWidth, newHeight);

    return std::make_pair(newWidth, newHeight);
}

void Viewport::process()
{
    FrameBuffer fb = app.buffers.getBuffer();

    // Add validation for the framebuffer
    if (fb.isp.texture == 0) {
        SDL_Log("Warning: ISP texture is 0 (invalid)");
        return;
    }

    SDL_Log("Processing with ISP texture ID: %u, dimensions: %dx%d", 
        fb.isp.texture, fb.isp.info.width, fb.isp.info.height);

    if(!yuv_preview.initialized){
        SDL_Log("Setting up yuv_preview...");
        auto [newWidth, newHeight] = calculateNewImageSize(app.app_width, app.app_height, fb.isp.info.width, fb.isp.info.height);
        yuv_preview.setup(newWidth, newHeight, GL_LINEAR);
        SDL_Log("Setup complete, initialized = %s", yuv_preview.initialized ? "true" : "false");

                // Validate the setup
        if (yuv_preview.frameBuffer == 0) {
            SDL_Log("Error: yuv_preview.frameBuffer is 0!");
            return;
        }
        if (yuv_preview.texture == 0) {
            SDL_Log("Error: yuv_preview.texture is 0!");
            return;
        }
        if (yuv_preview.program == 0) {
            SDL_Log("Error: yuv_preview.program is 0!");
            return;
        }

    } else {

        SDL_Log("Rendering frame...");
        // Save current framebuffer state
        GLint currentFB;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFB);
        SDL_Log("Current framebuffer before bind: %d", currentFB);


        glBindFramebuffer(GL_FRAMEBUFFER, yuv_preview.frameBuffer);
        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);
        // oho
        // Check framebuffer completeness
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        SDL_Log("Framebuffer status: 0x%x", status);
        // Check what's attached to the framebuffer
        GLint colorAttachment;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colorAttachment);
        SDL_Log("Color attachment: %d, yuv_preview.texture: %u", colorAttachment, yuv_preview.texture);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SDL_Log("Framebuffer not complete: 0x%x", status);
            return;
        }

        glViewport(0, 0, yuv_preview.width, yuv_preview.height);  
        SDL_Log("Set viewport to: %dx%d", yuv_preview.width, yuv_preview.height);

        // Clear the framebuffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f); // Magenta for debugging
        SDL_Log("About to clear framebuffer %u attached to texture %u", yuv_preview.frameBuffer, yuv_preview.texture);

        glClear(GL_COLOR_BUFFER_BIT);
        glFinish(); // Force completion
        SDL_Log("Clear completed");
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // Check for GL errors after clear
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            SDL_Log("Error after clear: 0x%x", error);
        }


        glUseProgram(yuv_preview.program);
        GLint linkStatus;
        glGetProgramiv(yuv_preview.program, GL_LINK_STATUS, &linkStatus);
        
        if (linkStatus != GL_TRUE) {
            char log[512];
            glGetProgramInfoLog(yuv_preview.program, 512, NULL, log);
            SDL_Log("Shader program link error: %s", log);
            return;
        } else {
            SDL_Log("Shader program linked successfully");
        }



        if (linkStatus != GL_TRUE) {
            char log[512];
            glGetProgramInfoLog(yuv_preview.program, 512, NULL, log);
            SDL_Log("Shader program link error: %s", log);
        }
       // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, fb.isp.texture);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            SDL_Log("Error binding EGL texture: 0x%x", error);
        } else {
            SDL_Log("Successfully bound EGL texture: %u", fb.isp.texture);
        }


        // Check if the texture is actually bound
        GLint boundTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &boundTexture);
        SDL_Log("Currently bound external texture: %d, expected: %u", boundTexture, fb.isp.texture);
        


        // Set uniform
        GLint texUniformLocation = glGetUniformLocation(yuv_preview.program, "tex");
        if (texUniformLocation == -1) {
            SDL_Log("Warning: Could not find 'tex' uniform in shader");
        } else {
            SDL_Log("Found 'tex' uniform at location: %d", texUniformLocation);
        }
        glUniform1i(texUniformLocation, 0);
       
        // Check VAO
        if (yuv_preview.quadVAO == 0) {
            SDL_Log("Error: quadVAO is 0!");
            return;
        }

        glBindVertexArray(yuv_preview.quadVAO);
        SDL_Log("Drawing 6 triangles with VAO: %u", yuv_preview.quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL);  // Force GPU sync
        GLenum readError = glGetError();
        SDL_Log("ReadPixels test error: 0x%x", readError);
        // Check for draw errors
        error = glGetError();
        if (error != GL_NO_ERROR) {
            SDL_Log("Error after draw: 0x%x", error);
        }

        glBindVertexArray(0);


        // Unbind FBO and restore viewport
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, app.app_width, app.app_height);
        
        SDL_Log("Frame rendering complete, restored viewport to: %dx%d", app.app_width, app.app_height);


    }
}



void Viewport::show()
{
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    window_flags |= ImGuiWindowFlags_NoDecoration;
    window_flags |= ImGuiWindowFlags_NoInputs;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoNav;
    window_flags |= ImGuiWindowFlags_NoBackground;

    FrameBuffer fb = app.buffers.getBuffer();

    ImGui::Begin("Viewport", NULL, window_flags); 

    if (ImGui::Button("Click Me")) {
        // Log a message to the console when the button is pressed.
        SDL_Log("Button pressed!");
    }

    // Add debug info to the UI
    if (!yuv_preview.initialized) {
        ImGui::Text("YUV Preview not initialized");
    } else {
        ImGui::Text("YUV Preview initialized");
        ImGui::Text("Texture ID: %u", yuv_preview.texture);
        ImGui::Text("Framebuffer ID: %u", yuv_preview.frameBuffer);
        ImGui::Text("Size: %dx%d", yuv_preview.width, yuv_preview.height);
        ImGui::Text("ISP Texture: %u", fb.isp.texture);
        ImGui::Text("ISP Size: %dx%d", fb.isp.info.width, fb.isp.info.height);
    }
    
    
    SDL_Log("Displaying texture ID: %u", yuv_preview.texture);

    auto [pos, scale] = centerImage(fb.isp);
    SDL_Log("Image position: (%f, %f), scale: (%f, %f)", pos.x, pos.y, scale.x, scale.y);

    // ImGui::GetBackgroundDrawList()->AddImage(
    //     (void*)(intptr_t)yuv_preview.texture,
    //     pos,
    //     scale,
    //     ImVec2(0, 0),
    //     ImVec2(1, 1)
    // );

    // DISPLAY THE IMAGE - Multiple debugging approaches
    if (yuv_preview.initialized && yuv_preview.texture != 0) {
        SDL_Log("=== TEXTURE DEBUGGING ===");
        
        // Check if texture is valid in current GL context
        GLboolean isTexture = glIsTexture(yuv_preview.texture);
        SDL_Log("Is yuv_preview.texture valid? %s", isTexture ? "YES" : "NO");
        
        // Clear any existing errors
        while(glGetError() != GL_NO_ERROR);
        
        // Try to get texture parameters - might fail if not GL_TEXTURE_2D
        glBindTexture(GL_TEXTURE_2D, yuv_preview.texture);
        GLenum bindError = glGetError();
        if (bindError != GL_NO_ERROR) {
            SDL_Log("Error binding as GL_TEXTURE_2D: 0x%x", bindError);
            SDL_Log("This texture might be a different type (renderbuffer, external, etc.)");
        } else {
            GLint width, height, format;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
            GLenum queryError = glGetError();
            if (queryError != GL_NO_ERROR) {
                SDL_Log("Error querying texture parameters: 0x%x", queryError);
            } else {
                SDL_Log("Texture dimensions: %dx%d, format: 0x%x", width, height, format);
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Test if it might be a renderbuffer instead
        GLboolean isRenderbuffer = glIsRenderbuffer(yuv_preview.texture);
        SDL_Log("Is yuv_preview.texture a renderbuffer? %s", isRenderbuffer ? "YES" : "NO");
        
        // Test 1: Fixed small rectangle
        SDL_Log("Test 1: Drawing fixed 50x50 rectangle at (20,20)");
        ImGui::GetBackgroundDrawList()->AddImage(
            (void*)(intptr_t)yuv_preview.texture,
            ImVec2(20, 20),
            ImVec2(70, 70),
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
        
        // Test 2: Try with different UV coordinates (flipped)
        SDL_Log("Test 2: Drawing with flipped UVs at (80,20)");
        ImGui::GetBackgroundDrawList()->AddImage(
            (void*)(intptr_t)yuv_preview.texture,
            ImVec2(80, 20),
            ImVec2(130, 70),
            ImVec2(0, 1),  // Flipped UV
            ImVec2(1, 0)
        );
        
        // Test 3: Try with different color tint
        SDL_Log("Test 3: Drawing with red tint at (140,20)");
        ImGui::GetBackgroundDrawList()->AddImage(
            (void*)(intptr_t)yuv_preview.texture,
            ImVec2(140, 20),
            ImVec2(190, 70),
            ImVec2(0, 0),
            ImVec2(1, 1),
            IM_COL32(255, 0, 0, 255)  // Red tint
        );
        
        // Test 4: Try a different texture (if available) - use ISP texture directly
        SDL_Log("Test 4: Try ISP texture directly at (20,80)");
        if (fb.isp.texture != 0) {
            GLboolean isISPTexture = glIsTexture(fb.isp.texture);
            SDL_Log("Is ISP texture valid? %s", isISPTexture ? "YES" : "NO");
            
            // Note: This might not work if it's GL_TEXTURE_EXTERNAL_OES
            ImGui::GetBackgroundDrawList()->AddImage(
                (void*)(intptr_t)fb.isp.texture,
                ImVec2(20, 80),
                ImVec2(70, 130),
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32(0, 255, 0, 255)  // Green tint for distinction
            );
        }
        
        // Test 5: Try with ImGui::Image instead of background draw list
        SDL_Log("Test 5: Using ImGui::Image");
        ImGui::SetCursorPos(ImVec2(80, 80));
        ImGui::Image((void*)(intptr_t)yuv_preview.texture, ImVec2(50, 50));
        
        // Test 6: Draw a colored rectangle to verify ImGui drawing works
        SDL_Log("Test 6: Drawing colored rectangle for reference");
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            ImVec2(140, 80),
            ImVec2(190, 130),
            IM_COL32(0, 0, 255, 255)  // Blue rectangle
        );
        
        SDL_Log("=== END TEXTURE DEBUGGING ===");
    } else {
        SDL_Log("Cannot debug: yuv_preview not initialized or texture is 0");
        
        // Draw a test rectangle to verify ImGui is working
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            ImVec2(10, 10),
            ImVec2(60, 60),
            IM_COL32(255, 255, 0, 255)  // Yellow rectangle
        );
        ImGui::GetBackgroundDrawList()->AddText(ImVec2(70, 20), IM_COL32(255, 255, 255, 255), "No YUV");
    }


    ImGui::End();
}

Page *make_viewport_page(Application& app){
    return new Viewport(app);
}