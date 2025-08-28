#pragma once
#ifndef EGLBUFFERS_HPP
#define EGLBUFFERS_HPP

#include "SharedContext.hpp"

#include <map>
#include <unistd.h>
#include <sys/syscall.h>
#include <iostream>



#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include <GLES2/gl2ext.h> // this is correct

#include <libcamera/control_ids.h>
#include <libcamera/controls.h>

// DEBUG LIBS
#include <sys/mman.h>

#include <cstdio> // for fprintf



// EGL_KHR_image extensions
typedef EGLImage (EGLAPIENTRYP eglCreateImageKHR_type)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP eglDestroyImageKHR_type)(EGLDisplay dpy, EGLImage image);
// GL_OES_EGL_image extensions
typedef void (EGLAPIENTRYP glEGLImageTargetTexture2DOES_type)(GLenum target, GLeglImageOES image);
// EGL_EXT_platform_base and EGL_EXT_device_base extensions
typedef EGLBoolean (EGLAPIENTRYP eglQueryDevicesEXT_type)(EGLint max_devices, EGLDeviceEXT *devices, EGLint *num_devices);
typedef EGLDisplay (EGLAPIENTRYP eglGetPlatformDisplayEXT_type)(EGLenum platform, void *native_display, const EGLint *attrib_list);

#ifdef EGL_BUFFERS_IMPLEMENTATION
    // This part is the definition. It will be included only in EglBuffers.cpp.
    eglCreateImageKHR_type p_eglCreateImageKHR = nullptr;
    eglDestroyImageKHR_type p_eglDestroyImageKHR = nullptr;
    glEGLImageTargetTexture2DOES_type p_glEGLImageTargetTexture2DOES = nullptr;
    eglQueryDevicesEXT_type p_eglQueryDevicesEXT = nullptr;
    eglGetPlatformDisplayEXT_type p_eglGetPlatformDisplayEXT = nullptr;
#else
    // This part is the declaration with 'extern'.
    // It will be included in all other files that include this header.
    extern eglCreateImageKHR_type p_eglCreateImageKHR;
    extern eglDestroyImageKHR_type p_eglDestroyImageKHR;
    extern glEGLImageTargetTexture2DOES_type p_glEGLImageTargetTexture2DOES;
    extern eglQueryDevicesEXT_type p_eglQueryDevicesEXT;
    extern eglGetPlatformDisplayEXT_type p_eglGetPlatformDisplayEXT;
#endif





namespace controls = libcamera::controls;

enum BufferType {
    RAW,
    ISP,
    LORES,
    LUMA
};

struct Buffer
{
    Buffer() : fd(-1) {}
    int fd;
    size_t size;
    StreamInfo info;
    GLuint texture;
    EGLint encoding;
    EGLint range;
};

struct FrameBuffer
{
    FrameBuffer() : mapped(-1) {}
    Buffer raw;
    Buffer isp;
    Buffer lores;
    Buffer luma;
    libcamera::ControlList metadata;
    unsigned int sequence;
    float framerate;
    int mapped;
};


class EglBuffers {
    public:
        EglBuffers(SharedContext &context) : 
            shared(context),
            context(shared.get_context()),
            current_index_(-1),
            first_time_(true),
            last_fd_(-1),
            newFrame_(false)
            {
                console = spdlog::stdout_color_mt("egl_buffers");
            };
        ~EglBuffers()
        {
            reset();
        };

        int initEGLExtensions();

        void update();
        bool new_frame(){
            bool val = newFrame_;
            newFrame_ = false;
            return val;
        }

        GLuint texture(BufferType type){
            FrameBuffer &buffer = buffers_[current_index_];
            GLuint texture;
            switch(type){
                case RAW:
                    texture = buffer.raw.texture;
                    break;
                case ISP:
                    texture = buffer.isp.texture;
                    break;
                case LORES:
                    texture = buffer.lores.texture;
                    break;
                case LUMA:
                    texture = buffer.luma.texture;
                    break;
                default:
                    texture = buffer.isp.texture;
                    break;
            }
            return texture;
        } 

        FrameBuffer& getBuffer() {
            return buffers_[current_index_];
        }

    private:

        void makeBuffer(const SharedMemoryBuffer* context, FrameBuffer &buffer);

        void reset(){
            for (auto &it : buffers_){
                glDeleteTextures(1, &it.second.raw.texture);
                glDeleteTextures(1, &it.second.isp.texture);
                glDeleteTextures(1, &it.second.lores.texture);
                glDeleteTextures(1, &it.second.luma.texture);
                close(it.second.raw.fd);
                close(it.second.isp.fd);
                close(it.second.lores.fd);
            }
            buffers_.clear();
            last_fd_ = -1;
            first_time_ = true;
            console->info("Reset buffers!");
        }

        EGLDisplay egl_display_;

        SharedContext &shared;
        const SharedMemoryBuffer* context;

        int current_index_;
        std::map<int, FrameBuffer> buffers_; // map the DMABUF's fd to the Buffer
        bool first_time_;
        int last_fd_;
        bool newFrame_;

        std::shared_ptr<spdlog::logger> console;
};

#endif // EGLBUFFERS_HPP
