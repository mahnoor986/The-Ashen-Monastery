#version 330
// world.vs - Blackthorn Manor world/character vertex shader.
// Passes world position, UV, normal and the baked light (vertex color) to the fragment shader.

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform vec2 snapGrid;      // PS1 wobble: snap to this screen grid (0 = off)

out vec3 fragPos;
out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    if (snapGrid.x > 0.0 && gl_Position.w > 0.001) {
        vec2 ndc = gl_Position.xy / gl_Position.w;
        ndc = floor(ndc * snapGrid * 0.5 + 0.5) / (snapGrid * 0.5);
        gl_Position.xy = ndc * gl_Position.w;
    }
}
