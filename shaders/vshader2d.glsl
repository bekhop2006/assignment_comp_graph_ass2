#version 410 core

// Вершинный шейдер для 2D-фигур (программа interaction).

in vec4 vPosition;  // позиция вершины (x, y); z и w по умолчанию 0 и 1
in vec4 vColor;     // цвет вершины (если задан)

uniform mat4 uModel;          // матрица преобразования фигуры
uniform vec4 uColor;          // один цвет на всю фигуру
uniform int uUseVertexColor;  // 1 — брать цвет из вершин, 0 — из uColor

out vec4 color;  // передаётся во фрагментный шейдер

void main() {
    gl_Position = uModel * vPosition;
    color = (uUseVertexColor != 0) ? vColor : uColor;
}
