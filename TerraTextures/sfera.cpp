/*
============================================================
A ROTATING EARTH WITH LAYERED TEXTURES
============================================================

Author: Lorenzo Dal Col
Written: 1 March - 1 May 2009, for the "Elementi di grafica digitale" exam,
         University of Trieste (prof. Matjaz Hmeljak)
Restored for Windows 10 in 2026: see README.md for what changed.

------------------------------------------------------------
WHAT IT DOES
------------------------------------------------------------
Draws the Earth as a textured sphere and layers up to six textures on it at
the same time with multitexturing (OpenGL 1.3, first the GL_ARB_multitexture
extension): the Earth, a latitude/longitude grid, red highlights on Europe,
America and Oceania, and clouds that drift on their own.

Keys:
    Space   stop or restart the rotation (the clouds keep moving)
    1       the Earth
    2       the grid
    3       red highlight on Europe
    4       red highlight on America
    5       red highlight on Oceania
    6       the clouds
    Esc, Q  quit

------------------------------------------------------------
HOW
------------------------------------------------------------
1) glutSolidSphere has no texture coordinates, so the sphere is built by hand:
   rings of triangle strips from the south pole to the north pole, with the
   texture coordinates of each vertex taken from its longitude (U) and
   latitude (V). The textures are equirectangular maps, 2:1, so this maps
   them onto the sphere directly.

2) glTexCoord and glBindTexture work with one texture at a time. With
   multitexturing each layer gets its own texture unit: glActiveTexture
   selects the unit, and glMultiTexCoord gives each unit its coordinates.
   Every unit multiplies the colour by its texture (GL_MODULATE): white parts
   of a layer leave the Earth as it is, black lines draw the grid, red areas
   tint the continents.

3) The clouds can't be a multiplication: they make the Earth lighter, not
   darker. They are drawn as a second pass over the same sphere with blending,
   result = earth * (1 - cloud) + cloud, so thick clouds are white.

4) OpenGL drivers give the fixed-function pipeline a limited number of
   texture units, 4 on current NVIDIA cards. When there are more layers than
   units, the extra ones are drawn in further passes, multiplied into the
   image with blending (glBlendFunc(GL_DST_COLOR, GL_ZERO)).

5) On Windows, opengl32.dll only exports OpenGL 1.1, so glActiveTexture and
   glMultiTexCoord2f are looked up at run time with wglGetProcAddress, after
   the window (and its OpenGL context) exists.
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif

#include "image.h"

// ====================================================
// OPENGL 1.3: what opengl32.dll on Windows doesn't declare
// ====================================================

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

#ifdef _WIN32
typedef void(APIENTRY* ActiveTextureProc)(GLenum texture);
typedef void(APIENTRY* MultiTexCoord2fProc)(GLenum target, GLfloat s, GLfloat t);
static ActiveTextureProc pActiveTexture = NULL;
static MultiTexCoord2fProc pMultiTexCoord2f = NULL;
#define glActiveTexture pActiveTexture
#define glMultiTexCoord2f pMultiTexCoord2f

// Looks up a function by its OpenGL 1.3 name, or by the name it had as an
// ARB extension.
static PROC getProc(const char* name, const char* arbName) {
	PROC p = wglGetProcAddress(name);
	if (!p) p = wglGetProcAddress(arbName);
	return p;
}

static bool loadMultitexture() {
	pActiveTexture = (ActiveTextureProc)getProc("glActiveTexture", "glActiveTextureARB");
	pMultiTexCoord2f = (MultiTexCoord2fProc)getProc("glMultiTexCoord2f", "glMultiTexCoord2fARB");
	return pActiveTexture && pMultiTexCoord2f;
}
#else
// Linux and macOS export OpenGL 1.3 directly.
static bool loadMultitexture() { return true; }
#endif

// ====================================================
// THE LAYERS
// ====================================================

enum Layer { EARTH, GRID, EUROPE, AMERICA, OCEANIA, CLOUDS, LAYERS };

static const char* const textureFiles[LAYERS] = {
	"earth.bmp", "earth-grid.bmp", "earth-europa.bmp",
	"earth-america.bmp", "earth-oceania.bmp", "fog.bmp",
};

static GLuint texture[LAYERS];
static bool showLayer[LAYERS] = { true, true, false, false, false, true };
static GLint textureUnits = 1;

// ====================================================
// THE SPHERE
// ====================================================

static const double PI = 3.14159265358979323846;

// Degrees between neighbouring vertices: 5 gives a smooth outline.
static const int STEP = 5;
static const int RINGS = 180 / STEP;    // bands from pole to pole
static const int SEGMENTS = 360 / STEP; // vertices around a band, plus one

struct Vertex {
	float x, y, z; // position on the unit sphere
	float u, v;    // texture coordinates
};

// One triangle strip per band: (RINGS) x (SEGMENTS + 1) x 2 vertices.
static Vertex sphere[RINGS][SEGMENTS + 1][2];

// Builds the unit sphere with its poles on the Y axis. Longitude 0 is at the
// texture's centre, latitude -90 (south) at its bottom.
static void buildSphere() {
	for (int ring = 0; ring < RINGS; ring++) {
		for (int seg = 0; seg <= SEGMENTS; seg++) {
			for (int k = 0; k < 2; k++) {
				// k = 0: upper edge of the band, k = 1: lower edge. This order
				// makes the triangles counter-clockwise seen from outside.
				double lat = -90.0 + (ring + 1 - k) * STEP;
				double lon = -180.0 + seg * STEP;
				double la = lat * PI / 180.0, lo = lon * PI / 180.0;
				Vertex& v = sphere[ring][seg][k];
				v.x = (float)(cos(la) * sin(lo));
				v.y = (float)sin(la);
				v.z = (float)(cos(la) * cos(lo));
				v.u = (float)((lon + 180.0) / 360.0);
				v.v = (float)((lat + 90.0) / 180.0);
			}
		}
	}
}

// Draws the sphere once, sending the texture coordinates to texture units
// 0 .. units-1. uOffset shifts unit 0 horizontally (for the drifting clouds).
static void drawSphere(int units, float uOffset) {
	for (int ring = 0; ring < RINGS; ring++) {
		glBegin(GL_TRIANGLE_STRIP);
		for (int seg = 0; seg <= SEGMENTS; seg++) {
			for (int k = 0; k < 2; k++) {
				const Vertex& v = sphere[ring][seg][k];
				for (int unit = 0; unit < units; unit++) {
					glMultiTexCoord2f(GL_TEXTURE0 + unit, unit == 0 ? v.u + uOffset : v.u, v.v);
				}
				glVertex3f(v.x, v.y, v.z);
			}
		}
		glEnd();
	}
}

// Binds the given layers to texture units 0, 1, 2... each multiplying the
// colour (GL_MODULATE), and turns off the units after them. The texture
// environment belongs to the unit, so it is set here, for each unit in use.
static void bindLayers(const int* layers, int count) {
	for (int unit = 0; unit < textureUnits; unit++) {
		glActiveTexture(GL_TEXTURE0 + unit);
		if (unit < count) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture[layers[unit]]);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		} else {
			glDisable(GL_TEXTURE_2D);
		}
	}
	glActiveTexture(GL_TEXTURE0);
}

// ====================================================
// ANIMATION AND DRAWING
// ====================================================

static bool rotate = true;
static double angle = 0;        // degrees around the Earth's axis
static double cloudOffset = 0;  // texture offset of the clouds
static int lastTime = 0;        // milliseconds, from glutGet(GLUT_ELAPSED_TIME)

static const double DEGREES_PER_SECOND = 18.0;
static const double CLOUD_DRIFT_PER_SECOND = 0.018; // texture widths per second
static const double AXIAL_TILT = 23.44;

static void display() {
	// Time-based animation: the same speed at any frame rate.
	int now = glutGet(GLUT_ELAPSED_TIME);
	double dt = (now - lastTime) / 1000.0;
	lastTime = now;
	if (rotate) angle = fmod(angle + DEGREES_PER_SECOND * dt, 360.0);
	cloudOffset = fmod(cloudOffset - CLOUD_DRIFT_PER_SECOND * dt, 1.0);

	glClearColor(0, 0, 0, 1);
	glClearDepth(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(0, 0, -3.2f);
	glRotatef(15, 1, 0, 0);               // look at it from a little above
	glRotatef((float)AXIAL_TILT, 0, 0, 1); // tilt the axis...
	glRotatef((float)angle, 0, 1, 0);      // ...and spin around it

	// The colour layers, as many per pass as there are texture units.
	int layers[LAYERS], count = 0;
	for (int l = EARTH; l < CLOUDS; l++)
		if (showLayer[l]) layers[count++] = l;

	glColor3f(1, 1, 1);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_BLEND);
	if (count == 0) {
		bindLayers(NULL, 0); // nothing on: a white sphere
		drawSphere(0, 0);
	}
	for (int first = 0; first < count; first += textureUnits) {
		int n = count - first < textureUnits ? count - first : textureUnits;
		if (first > 0) {
			// further passes: multiply into what is already there
			glEnable(GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
		}
		bindLayers(layers + first, n);
		drawSphere(n, 0);
	}

	// The clouds: earth * (1 - cloud) + cloud. Their texture stores the
	// cloud density, see loadTexture.
	if (showLayer[CLOUDS]) {
		int clouds = CLOUDS;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		bindLayers(&clouds, 1);
		drawSphere(1, (float)cloudOffset);
	}
	glDisable(GL_BLEND);

	glutSwapBuffers();
}

static void idle() {
	glutPostRedisplay();
}

static void reshape(int w, int h) {
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, (double)w / h, 0.1, 100.0);
	glMatrixMode(GL_MODELVIEW);
}

static void keyboard(unsigned char key, int, int) {
	switch (key) {
	case 27:
	case 'q':
	case 'Q':
		exit(0);
	case ' ':
		rotate = !rotate;
		break;
	default:
		if (key >= '1' && key <= '6') showLayer[key - '1'] = !showLayer[key - '1'];
	}
}

// ====================================================
// TEXTURES
// ====================================================

// Loads a 24-bit BMP as a mipmapped texture. For the clouds it stores
// 255 - value: the cloud texture is dark where the clouds are thick.
static GLuint loadTexture(const char* filename, bool invert) {
	FILE* f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Can't open %s: run the program from the folder with the textures.\n", filename);
		exit(1);
	}
	fclose(f);
	Image* image = loadBMP(filename);
	if (invert) {
		for (int i = 0; i < image->width * image->height * 3; i++)
			image->pixels[i] = (char)(255 - (unsigned char)image->pixels[i]);
	}

	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
	delete image;
	return id;
}

static void init() {
	if (!loadMultitexture()) {
		fprintf(stderr, "This OpenGL doesn't support multitexturing (OpenGL 1.3).\n");
		exit(1);
	}
	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &textureUnits);
	if (textureUnits < 1) textureUnits = 1;
	if (textureUnits > LAYERS) textureUnits = LAYERS;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glShadeModel(GL_SMOOTH);

	for (int l = 0; l < LAYERS; l++) texture[l] = loadTexture(textureFiles[l], l == CLOUDS);
	buildSphere();
	lastTime = glutGet(GLUT_ELAPSED_TIME);
}

// Renders `frames` frames, saves the last one as a PPM file and exits. Used
// to check the program without looking at it: TerraTextures --capture out.ppm
static void capture(const char* path, int frames) {
	int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
	reshape(w, h);
	for (int i = 0; i < frames; i++) {
		display();
		glutMainLoopEvent();
	}
	unsigned char* pixels = new unsigned char[w * h * 3];
	glReadBuffer(GL_FRONT);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	FILE* f = fopen(path, "wb");
	if (!f) exit(1);
	fprintf(f, "P6 %d %d 255\n", w, h);
	for (int y = h - 1; y >= 0; y--) fwrite(pixels + y * w * 3, 1, w * 3, f);
	fclose(f);
	delete[] pixels;
	printf("%s: %d texture units, saved %s\n", (const char*)glGetString(GL_RENDERER), textureUnits, path);
	exit(0);
}

int main(int argc, char** argv) {
	printf("Keys:\n");
	printf("  Space   stop or restart the rotation (the clouds keep moving)\n");
	printf("  1       the Earth\n");
	printf("  2       the grid\n");
	printf("  3       red highlight on Europe\n");
	printf("  4       red highlight on America\n");
	printf("  5       red highlight on Oceania\n");
	printf("  6       the clouds\n");
	printf("  Esc, Q  quit\n");

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(700, 700);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Rotating sphere with layered textures");
	init();
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyboard);
	glutIdleFunc(idle);

	if (argc >= 3 && strcmp(argv[1], "--capture") == 0) {
		// optional: which layers, as a string of digits, e.g. "1236"
		if (argc >= 4) {
			for (int l = 0; l < LAYERS; l++) showLayer[l] = strchr(argv[3], '1' + l) != NULL;
		}
		capture(argv[2], 120);
	}
	glutMainLoop();
	return 0;
}
