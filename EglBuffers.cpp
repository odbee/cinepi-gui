
#define EGL_BUFFERS_IMPLEMENTATION // Define a special macro here

#include "EglBuffers.hpp"

#include <libdrm/drm_fourcc.h>

static void MyEglError(){
    EGLint error = eglGetError();  // Get the last error
    std::string errorMessage;

    // Match the error code with predefined EGL error codes
    switch(error) {
        case EGL_SUCCESS:
            errorMessage = "The last function succeeded without error.";
            return;
            break;
        case EGL_NOT_INITIALIZED:
            errorMessage = "EGL is not initialized, or could not be initialized, for the specified EGL display connection.";
            break;
        case EGL_BAD_ACCESS:
            errorMessage = "EGL cannot access a requested resource.";
            break;
        case EGL_BAD_ALLOC:
            errorMessage = "EGL failed to allocate resources for the requested operation.";
            break;
        case EGL_BAD_ATTRIBUTE:
            errorMessage = "An unrecognized attribute or attribute value was passed in the attribute list.";
            break;
        case EGL_BAD_CONTEXT:
            errorMessage = "An EGLContext argument does not name a valid EGL rendering context.";
            break;
        case EGL_BAD_CONFIG:
            errorMessage = "An EGLConfig argument does not name a valid EGL frame buffer configuration.";
            break;
        case EGL_BAD_CURRENT_SURFACE:
            errorMessage = "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.";
            break;
        case EGL_BAD_DISPLAY:
            errorMessage = "An EGLDisplay argument does not name a valid EGL display connection.";
            break;
        case EGL_BAD_SURFACE:
            errorMessage = "An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.";
            break;
        default:
            errorMessage = "Unknown EGL error.";
    }

    std::cout << "EGL ERROR: "
                    << errorMessage << " :::: " << error << std::endl;
}

int getSharedProcFd(int procid, int fd){
    int pid_fd = syscall(SYS_pidfd_open, procid, 0);
    if (pid_fd == -1) {
        perror("pidfd_open");
        return -1;
    }

    int new_fd_raw = syscall(SYS_pidfd_getfd, pid_fd, fd, 0);
    if (new_fd_raw == -1) {
        printf("%d , %d\r\n", pid_fd, fd);
        perror("pidfd_getfd");
        close(pid_fd);
        return -1;
    }

    close(pid_fd);

    return new_fd_raw;
}

static void get_colour_space_info(std::optional<libcamera::ColorSpace> const &cs, EGLint &encoding, EGLint &range)
{
	encoding = EGL_ITU_REC601_EXT;
	range = EGL_YUV_NARROW_RANGE_EXT;

	if (cs == libcamera::ColorSpace::Sycc)
		range = EGL_YUV_FULL_RANGE_EXT;
	else if (cs == libcamera::ColorSpace::Smpte170m)
		/* all good */;
	else if (cs == libcamera::ColorSpace::Rec709)
		encoding = EGL_ITU_REC709_EXT;
}

int EglBuffers::initEGLExtensions() {
    // 1. Check for EGL_KHR_image support
    const char* platform_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (platform_extensions == nullptr) {
        fprintf(stderr, "Failed to query EGL platform extensions.\n");
        return false;
    }

    if (strstr(platform_extensions, "EGL_EXT_device_base") == nullptr) {
        fprintf(stderr, "EGL_EXT_device_base extension not supported.\n");
        return false;
    }

    if (strstr(platform_extensions, "EGL_EXT_platform_base") == nullptr) {
        fprintf(stderr, "EGL_EXT_platform_base extension not supported.\n");
        return false;
    }
    
    p_eglQueryDevicesEXT = (eglQueryDevicesEXT_type)eglGetProcAddress("eglQueryDevicesEXT");
    p_eglGetPlatformDisplayEXT = (eglGetPlatformDisplayEXT_type)eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (!p_eglQueryDevicesEXT || !p_eglGetPlatformDisplayEXT) {
        fprintf(stderr, "Failed to load EGL platform/device functions.\n");
        return false;
    }

    // Example call to get the display from a device.
    static const int MAX_DEVICES = 4;
    EGLDeviceEXT devices[MAX_DEVICES];
    EGLint num_devices;
    p_eglQueryDevicesEXT(1, devices, &num_devices);
    printf("Detected %d devices\n", num_devices);

    if (num_devices < 1) {
        fprintf(stderr, "No EGL devices found.\n");
        return false;
    }

    egl_display_ = p_eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, (void*)devices[0], nullptr);

    if (egl_display_ == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get platform display.\n");
        return false;
    }
    fprintf(stderr, "EGLDisplay handle: %p\n", egl_display_);

    EGLBoolean initialized = eglInitialize(egl_display_, nullptr, nullptr);
    if (initialized == EGL_FALSE) {
        fprintf(stderr, "Failed to initialize EGL display. EGL error: %d\n", eglGetError());
        return false;
    }

    const char* display_extensions = eglQueryString(egl_display_, EGL_EXTENSIONS);
    if (display_extensions == nullptr) {
        fprintf(stderr, "EGL display failed to return extension string.\n");
        return false;
    }
    
    if (strstr(display_extensions , "EGL_KHR_image") == nullptr) {
        fprintf(stderr, "EGL_KHR_image extension not supported.\n");
        return false;
    }

    p_eglCreateImageKHR = (eglCreateImageKHR_type)eglGetProcAddress("eglCreateImageKHR");
    p_eglDestroyImageKHR = (eglDestroyImageKHR_type)eglGetProcAddress("eglDestroyImageKHR");

    if (!p_eglCreateImageKHR || !p_eglDestroyImageKHR) {
        fprintf(stderr, "Failed to load EGL_KHR_image functions.\n");
        return false;
    }

    const char* gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (gl_extensions == nullptr) {
        // This will happen if no context is current, which is expected.
        // It's often handled in the main render loop, not init().
        // For this example, let's assume it's valid.
    } else {
        if (strstr(gl_extensions, "GL_OES_EGL_image") == nullptr) {
            fprintf(stderr, "GL_OES_EGL_image extension not supported.\n");
            return false;
        }

        p_glEGLImageTargetTexture2DOES = (glEGLImageTargetTexture2DOES_type)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        if (!p_glEGLImageTargetTexture2DOES) {
            fprintf(stderr, "Failed to load glEGLImageTargetTexture2DOES.\n");
            return false;
        }
    }

        EGLBoolean bound = eglBindAPI(EGL_OPENGL_ES_API);
    if (bound == EGL_FALSE) {
        fprintf(stderr, "Failed to bind EGL_OPENGL_ES_API.\n");
        return false;
    }

    // If we reach here, all functions are loaded successfully
    return true;

}
void EglBuffers::makeBuffer(const SharedMemoryBuffer* context, FrameBuffer &buffer)
{

    
    buffer.isp.fd = getSharedProcFd(context->procid, context->fd_isp);
    buffer.isp.size = context->isp_length;
    buffer.isp.info = context->isp;


    buffer.framerate = context->framerate;
    buffer.sequence = context->sequence;
    get_colour_space_info(buffer.isp.info.colour_space, buffer.isp.encoding, buffer.isp.range);

	static const EGLint attribs_isp[] = {
		EGL_WIDTH, static_cast<EGLint>(buffer.isp.info.width),
		EGL_HEIGHT, static_cast<EGLint>(buffer.isp.info.height),
		EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUV420,
		EGL_DMA_BUF_PLANE0_FD_EXT, buffer.isp.fd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(buffer.isp.info.stride),
		EGL_DMA_BUF_PLANE1_FD_EXT, buffer.isp.fd,
		EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(buffer.isp.info.stride * buffer.isp.info.height),
		EGL_DMA_BUF_PLANE1_PITCH_EXT, static_cast<EGLint>(buffer.isp.info.stride / 2),
		EGL_DMA_BUF_PLANE2_FD_EXT, buffer.isp.fd,
		EGL_DMA_BUF_PLANE2_OFFSET_EXT, static_cast<EGLint>(buffer.isp.info.stride * buffer.isp.info.height + (buffer.isp.info.stride / 2) * (buffer.isp.info.height / 2)),
		EGL_DMA_BUF_PLANE2_PITCH_EXT, static_cast<EGLint>(buffer.isp.info.stride / 2),
		EGL_YUV_COLOR_SPACE_HINT_EXT, buffer.isp.encoding,
		EGL_SAMPLE_RANGE_HINT_EXT, buffer.isp.range,
		EGL_NONE
	};


	EGLImage image_isp = p_eglCreateImageKHR(egl_display_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs_isp);

    MyEglError();
    if (!image_isp){
        MyEglError();
		throw std::runtime_error("failed to import fd " + std::to_string(buffer.isp.fd));
    }

    MyEglError();

	glGenTextures(1, &buffer.isp.texture);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer.isp.texture);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	p_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image_isp);

    MyEglError();

	p_eglDestroyImageKHR(egl_display_, image_isp);
    buffer.mapped = 1;
    console->info("Buffer mapped! isp:{}, lores:{}", buffer.isp.fd, buffer.lores.fd);
}


void EglBuffers::update(){
    if(!shared.connected()){
        if(!first_time_){
            reset();
        }
        return;
    }

    if(first_time_)
        first_time_ = false;
        
    current_index_ = context->fd_raw;
    FrameBuffer &buffer = buffers_[current_index_];
	if (buffer.mapped == -1 && current_index_ != -1){
        makeBuffer(context, buffer);
    }
		
    if(last_fd_ != current_index_){
        newFrame_ = true;
        console->info("FD: {}, FN: {}, FR: {}, SEQ: {}", current_index_, context->frame, context->metadata.focus, context->metadata.exposure_time);
    }

    last_fd_ = current_index_;
}