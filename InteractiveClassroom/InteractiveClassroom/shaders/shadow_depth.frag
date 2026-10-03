#version 330 core

void main()
{
    // Intentionally empty: the shadow framebuffer has no color attachment
    // (glDrawBuffer(GL_NONE) / glReadBuffer(GL_NONE) are set on it), so
    // there is nothing to write here. gl_FragDepth is filled in
    // automatically from gl_Position.z, which is exactly what the shadow
    // map needs to store.
}
