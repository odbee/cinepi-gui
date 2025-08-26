#version 310 es
#extension GL_OES_EGL_image_external_essl3 : require

precision highp float;

uniform samplerExternalOES tex;

out vec4 fragColor;
in vec2 fragCoord;

void main() {
    fragColor = texture(tex, fragCoord);
}