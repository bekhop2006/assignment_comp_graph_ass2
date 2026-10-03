# Computer Graphics — Assignment 2

Two modern OpenGL programs (shaders + VAOs, no `glBegin`/`glVertex`), using GLUT for
windows, sub-windows, menus and callbacks.

**Tested on:** macOS (Apple Silicon), Apple GLUT framework, OpenGL 4.1 Core Profile.

| Program | Part | Description |
|---|---|---|
| `interaction` | Interaction, Windows, Menus and Animation | Assignment 1 square + sub-window ellipse + "window 2" (circle & triangle), idle animation, menus, extra credit |
| `cube` | 3D Transformations and Geometry | Colored cube transformed with the keyboard (scale → rotate → translate) |

## Dependencies

None to install — GLUT and OpenGL ship with macOS (Xcode Command Line Tools).

## Build

```bash
make
```

## Run

Run from this folder (the programs load `shaders/*.glsl`):

```bash
./interaction
```

```bash
./cube
```

### `interaction` controls

| Where | Input | Action |
|---|---|---|
| Main window | right click | Stop Animation / Start Animation / Square Colors → White, Red, Green |
| Main window | left click | new breathing circle with a random color (extra credit) |
| Sub window (top-left, yellow) | right click | change the sub-window background color |
| window 2 | `r` `g` `b` `y` `o` `p` `w` | color of the circle and triangle |
| any | `Esc` | quit |

### `cube` controls

| Input | Action |
|---|---|
| right click | menu: SCALE / ROTATE / TRANSLATE |
| `q` / `a` | increase / decrease X |
| `w` / `s` | increase / decrease Y |
| `e` / `d` | increase / decrease Z |
| `+` / `-` | increase / decrease the delta of the current transformation |
| `r` | reset all transformations |
| `1` `2` `3` | shortcuts for SCALE / ROTATE / TRANSLATE |
| `Esc` | quit |

The current mode and its delta are shown in the window title.
