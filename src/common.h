// Общий код для обеих программ (interaction и cube):
// подключение OpenGL/GLUT, простые 4x4 матрицы, загрузка шейдеров и мешей.
#pragma once

// Отключаем предупреждения macOS о том, что OpenGL помечен как устаревший.
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
// gl3.h уже объявляет все функции; не даём glut.h подключить старый gl.h.
#define __gl_h_
#include <GLUT/glut.h>

#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Цвет RGB (компоненты от 0 до 1).
struct Vec3 {
    float r;
    float g;
    float b;
};

// Матрица 4x4, хранится по столбцам (column-major) —
// именно в таком виде её ждёт glUniformMatrix4fv.
struct Mat4 {
    float m[16];
};

// Функции построения матриц преобразований (углы — в градусах).
Mat4 identity();
Mat4 operator*(const Mat4& a, const Mat4& b);
Mat4 translate(float x, float y, float z);
Mat4 scale(float x, float y, float z);
Mat4 rotateX(float degrees);
Mat4 rotateY(float degrees);
Mat4 rotateZ(float degrees);

// Геометрия на стороне CPU: вершины, (необязательно) цвета и способ отрисовки.
struct Mesh {
    std::vector<float> positions;  // по posSize чисел на вершину (2 — для 2D, 3 — для 3D)
    std::vector<Vec3> colors;      // необязательно; по одному цвету на вершину
    int posSize = 2;
    GLenum mode = GL_TRIANGLES;    // GL_TRIANGLES, GL_TRIANGLE_FAN и т.д.
};

// Тот же меш, уже загруженный в видеопамять: VAO + число вершин.
struct GPUMesh {
    GLuint vao = 0;
    GLsizei count = 0;
    GLenum mode = GL_TRIANGLES;
};

// Читает, компилирует и линкует вершинный и фрагментный шейдеры из папки shaders/.
GLuint loadShaderProgram(const std::string& vertexFile, const std::string& fragmentFile);
// Создаёт VAO/VBO и привязывает атрибуты vPosition и vColor шейдера.
GPUMesh uploadMesh(GLuint program, const Mesh& mesh);
// Рисует меш одним вызовом glDrawArrays.
void drawMesh(const GPUMesh& mesh);

// Сохраняет задний буфер текущего окна GLUT в бинарный файл PPM (для скриншотов).
void saveWindowPPM(const std::string& path);
