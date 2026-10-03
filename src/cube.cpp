// Задание 2, часть 2 — 3D-преобразования и геометрия.
//
// Цветной куб (геометрия и цвета из лекции), который преобразуется с клавиатуры.
// Матрица модели собирается в программе как  M = T * R * S
// (сначала масштаб, потом поворот, потом перенос) и применяется в вершинном шейдере.
// Камера и проекция не используются.

#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

// Какое преобразование сейчас меняется с клавиатуры.
enum Mode { SCALE = 0, ROTATE = 1, TRANSLATE = 2 };
static const char* kModeNames[] = {"SCALE", "ROTATE", "TRANSLATE"};

// Три значения преобразования — по осям X, Y, Z.
struct Transform {
    float value[3];
};

// Значения по умолчанию: при запуске и при сбросе клавишей r.
static const Transform kDefaults[3] = {
    {{1.0f, 1.0f, 1.0f}},  // коэффициенты масштаба
    {{0.0f, 0.0f, 0.0f}},  // углы поворота вокруг X, Y, Z (в градусах)
    {{0.0f, 0.0f, 0.0f}},  // перенос
};
// Шаг изменения по умолчанию для каждого режима (масштаб, градусы, перенос).
static const float kDefaultDelta[3] = {0.1f, 5.0f, 0.05f};

// Текущее состояние: значения всех трёх преобразований, их шаги и активный режим.
static Transform gTransform[3];
static float gDelta[3];
static Mode gMode = ROTATE;

// Объекты OpenGL: шейдерная программа, расположение uniform uModel, меш куба.
static GLuint gProgram = 0;
static GLint gModelLoc = -1;
static GPUMesh gCube;

// ---------- геометрия куба (colorcube из лекции) ----------

// 8 вершин куба со стороной 1 и центром в начале координат.
static const float kVertices[8][3] = {
    {-0.5f, -0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
    {-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
};

// Цвет каждой из 8 вершин.
static const Vec3 kVertexColors[8] = {
    {0.0f, 0.0f, 0.0f},  // чёрный
    {1.0f, 0.0f, 0.0f},  // красный
    {1.0f, 1.0f, 0.0f},  // жёлтый
    {0.0f, 1.0f, 0.0f},  // зелёный
    {0.0f, 0.0f, 1.0f},  // синий
    {1.0f, 0.0f, 1.0f},  // пурпурный
    {1.0f, 1.0f, 1.0f},  // белый
    {0.0f, 1.0f, 1.0f},  // голубой
};

// Одна грань = два треугольника (a,b,c) и (a,c,d);
// вся грань окрашивается в цвет своей первой вершины a.
static void quad(Mesh& mesh, int a, int b, int c, int d) {
    const int order[6] = {a, b, c, a, c, d};
    for (int index : order) {
        mesh.positions.push_back(kVertices[index][0]);
        mesh.positions.push_back(kVertices[index][1]);
        mesh.positions.push_back(kVertices[index][2]);
        mesh.colors.push_back(kVertexColors[a]);
    }
}

// Собирает куб из 6 граней: 6 * 2 треугольника * 3 вершины = 36 вершин.
static Mesh colorCube() {
    Mesh mesh;
    mesh.posSize = 3;
    mesh.mode = GL_TRIANGLES;
    quad(mesh, 1, 0, 3, 2);  // передняя  (z = +0.5)
    quad(mesh, 2, 3, 7, 6);  // правая    (x = +0.5)
    quad(mesh, 3, 0, 4, 7);  // нижняя    (y = -0.5)
    quad(mesh, 6, 5, 1, 2);  // верхняя   (y = +0.5)
    quad(mesh, 4, 5, 6, 7);  // задняя    (z = -0.5)
    quad(mesh, 5, 4, 0, 1);  // левая     (x = -0.5)
    return mesh;
}

// ---------- преобразования ----------

// Строит матрицу модели из текущих значений масштаба, поворота и переноса.
static Mat4 modelMatrix() {
    const float* s = gTransform[SCALE].value;
    const float* r = gTransform[ROTATE].value;
    const float* t = gTransform[TRANSLATE].value;

    Mat4 S = scale(s[0], s[1], s[2]);
    // Поворот: сначала вокруг X, затем вокруг Y, затем вокруг Z.
    Mat4 R = rotateZ(r[2]) * rotateY(r[1]) * rotateX(r[0]);
    Mat4 T = translate(t[0], t[1], t[2]);
    return T * R * S;  // применяется справа налево: масштаб, поворот, перенос
}

// Сбрасывает все преобразования и шаги к значениям по умолчанию.
static void resetAll() {
    for (int i = 0; i < 3; ++i) {
        gTransform[i] = kDefaults[i];
        gDelta[i] = kDefaultDelta[i];
    }
}

// Показывает текущий режим и его шаг в заголовке окна.
static void updateTitle() {
    char title[128];
    std::snprintf(title, sizeof(title), "Assignment 2 - Cube | mode: %s | delta: %g",
                  kModeNames[gMode], gDelta[gMode]);
    glutSetWindowTitle(title);
}

// Печатает в терминал подсказку по управлению.
static void printInstructions() {
    std::cout
        << "Assignment 2 - Part 2: 3D colored cube\n"
        << "\n"
        << "Right click the window to choose the transformation to modify:\n"
        << "  SCALE / ROTATE / TRANSLATE   (current mode is shown in the title bar)\n"
        << "\n"
        << "Keys (change the current transformation by its delta):\n"
        << "  q / a   increase / decrease X\n"
        << "  w / s   increase / decrease Y\n"
        << "  e / d   increase / decrease Z\n"
        << "  + / -   increase / decrease the delta of the current transformation\n"
        << "  r       reset all transformations and deltas\n"
        << "  1 2 3   shortcut for SCALE / ROTATE / TRANSLATE\n"
        << "  Esc     quit\n"
        << "\n"
        << "Note: with the default orthographic view, translating along Z does not\n"
        << "change the cube's size; it is clipped once it leaves the view volume.\n";
}

// ---------- колбэки GLUT ----------

// Отрисовка кадра: очищаем цвет и глубину, передаём матрицу в шейдер, рисуем куб.
static void display() {
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gProgram);
    Mat4 model = modelMatrix();
    glUniformMatrix4fv(gModelLoc, 1, GL_FALSE, model.m);
    drawMesh(gCube);

    glutSwapBuffers();  // показываем нарисованный задний буфер
}

// При изменении размера окна область вывода растягивается на всё окно.
static void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

static void setMode(Mode mode) {
    gMode = mode;
    updateTitle();
}

// Обработка клавиш: меняем значение текущего преобразования на его шаг.
static void keyboard(unsigned char key, int, int) {
    float* v = gTransform[gMode].value;
    float d = gDelta[gMode];

    switch (key) {
        case 'q': v[0] += d; break;  // X +
        case 'a': v[0] -= d; break;  // X -
        case 'w': v[1] += d; break;  // Y +
        case 's': v[1] -= d; break;  // Y -
        case 'e': v[2] += d; break;  // Z +
        case 'd': v[2] -= d; break;  // Z -
        case '+':
        case '=': gDelta[gMode] *= 2.0f; break;  // шаг в 2 раза больше ('=' — та же клавиша без Shift)
        case '-':
        case '_': gDelta[gMode] /= 2.0f; break;  // шаг в 2 раза меньше
        case 'r': resetAll(); break;
        case '1': gMode = SCALE; break;
        case '2': gMode = ROTATE; break;
        case '3': gMode = TRANSLATE; break;
        case 27: exit(0);  // Esc
        default: return;   // остальные клавиши игнорируем
    }
    updateTitle();
    glutPostRedisplay();  // куб перерисовывается только после нажатия клавиши
}

// Обработчик меню: номер пункта совпадает со значением Mode.
static void menu(int item) {
    setMode(static_cast<Mode>(item));
}

// Контекстное меню по правой кнопке мыши.
static void createMenu() {
    glutCreateMenu(menu);
    glutAddMenuEntry("SCALE", SCALE);
    glutAddMenuEntry("ROTATE", ROTATE);
    glutAddMenuEntry("TRANSLATE", TRANSLATE);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

// Режим для тестов/отчёта: A2_KEYS="<клавиши>" A2_SCREENSHOT=<файл.ppm>
// проигрывает нажатия клавиш, сохраняет один кадр и завершает программу.
static void runScreenshot(const char* keys, const char* path) {
    for (const char* k = keys; k && *k; ++k) {
        keyboard(static_cast<unsigned char>(*k), 0, 0);
    }
    glViewport(0, 0, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(gProgram);
    Mat4 model = modelMatrix();
    glUniformMatrix4fv(gModelLoc, 1, GL_FALSE, model.m);
    drawMesh(gCube);
    glFinish();  // ждём, пока GPU закончит рисовать, прежде чем читать пиксели
    saveWindowPPM(path);
    exit(0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    // Core Profile (современный OpenGL), RGBA, двойная буферизация и буфер глубины.
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Assignment 2 - Cube");

    // Шейдеры и меш создаются после окна: только тогда существует контекст OpenGL.
    gProgram = loadShaderProgram("vshader_cube.glsl", "fshader.glsl");
    gModelLoc = glGetUniformLocation(gProgram, "uModel");
    gCube = uploadMesh(gProgram, colorCube());

    glEnable(GL_DEPTH_TEST);  // невидимые грани убираются буфером глубины

    resetAll();
    updateTitle();
    createMenu();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);

    if (const char* shot = std::getenv("A2_SCREENSHOT")) {
        runScreenshot(std::getenv("A2_KEYS"), shot);
    }

    printInstructions();
    glutMainLoop();  // главный цикл GLUT: дальше программа живёт в колбэках
    return 0;
}
