/*

============================================================
SFERA ROTANTE CON SOVRAPPOSIZIONE DI TESSITURE
============================================================

AUTORE: Lorenzo Dal Col
EMAIL:	lorenzo.dalcol@gmail.com
DATA:	1 marzo - 1 maggio 2009

============================================================

UNIVERSITA' DEGLI STUDI DI TRIESTE
Progetto per l'esame:	Elementi di grafica digitale
PROFESSORE:				Matjaž  Hmeljak

============================================================

<INTRODUZIONE>

L'obiettivo è quello di applicare delle texture ad un oggetto sovrapponendole.
Di per sè non dovrebbe essere un'operazione particolarmente complicata, ma la
mancanza di documentazione rende quest'operazione più difficile del previsto.
Fino ad alcuni anni fa per sovrapporre le texture si utilizzava una libreria
aggiuntiva chiamata GLAUX.LIB, ma nelle ultime versioni di OpenGL non viene
più supportata.

<ISTRUZIONI>
Durante l'esecuzione della dimostrazione è possibile agire con i seguenti
input da tastiera:

	(spazio) ferma la rotazione della Terra o la fa ripartire (non ferma le nuvole)
1	attiva/disattiva la texture della Terra
2	attiva/disattiva la texture della griglia
3	attiva/disattiva macchia rossa su Europa
4	attiva/disattiva macchia rossa su America
5	attiva/disattiva macchia rossa su Oceania
6	attiva/disattive le nuvole

<OBIETTIVO>

L'idea è di realizzare un programma che visualizzi una sfera tridimensionale
a cui possano essere applicate varie texture ed eventualmente in movimento
tra loro.
E' stato realizzato un globo terrestre, con applicato il planisfero opportunamente
deformato ai poli in modo da ricoprire correttamente la sfera.

<BREVE INTRODUZIONE AL MULTI-TEXTURING>

In questo programma viene utilizzata la tecnica del multi-texturing: non serve
moltissimo codice per farla funzionare ma il risultato, con un uso appropriato
può avere un grande effetto.

Con i continui sviluppi di OpenGL negli anni è stata aggiunta la funzionalità
di sovrapporre più texture. Purtroppo non esistono funzioni standard per fare
questa operazione, ma solamente delle estensioni di OpenGL, che si possono utilizzare
includendo il file glext.h (che sta per GLUT Extensions), che contiene i prototipi
di tutte le funzioni extra aggiunte.

Bisogna avere una delle ultime versioni di OpenGL per poter sfruttare queste funzioni,
in teoria sono supportate dalla versione 1.3 (per Microsoft Windows).

Per controllare quali estensioni sono abilitate si può chiamare la funzione
glGetString(GL_EXTENSIONS)
che ritorna un (char *) con dentro tutte le estensioni disponibili.

<PROBLEMI>

1)	Usando la funzione per disegnare una sfera fornita da GLUT "glutSolidSphere"
	non è possibile applicare una texture fissando le coordinate della texture
	correttamente.
	Ho cercato attentamente su internet, non sembra possibile gestire manualmente
	l'applicazione di una texture alla sfera creata da GLUT.
2)	Le funzioni classiche usate per applicare una texture a un oggetto:
			glBindTexture( GL_TEXTURE_2D, texture  );
			glTexCoord2f( 0.0, 0.0); glVertex3f(0,0,0);
	non permettono di sovrapporre correttamente più texture, se si tenta di farlo
	verrà visualizzata solo l'ultima.

<RISOLUZIONE>

1)	Per quanto riguarda il problema dell'applicazione delle coordinate
	della texture alla sfera è stato risolto costruendo manualmente la sfera
	come un insieme di triangoli uniti tra loro.
	I vertici dei triangoli della sfera sono stati calcolati con delle funzioni
	tridimensionali che rappresentano appunto la sfera nello spazio.

2)	Per risolvere il problema dei limiti imposti da glBindTecture e glTexCoord
	che funzionano con solo una tessitura alla volta è stato utilizzato un altro metodo:
	le funzioni glActiveTextureARB, glMultiTexCoord2fARB che fanno parte delle estensioni
	di OpenGL. Queste funzioni devono essere chiamate dinamicamente.
	Vengono presi gli indirizzi di queste con la funzione wglGetProcAddress, che restituisce
	un indirizzo che dipende dal "rendering context" corrente di OpenGL.
	Deve essere effettuato un casting alle nostre funzioni desiderate.
		glActiveTextureARB		= (PFNGLACTIVETEXTUREARBPROC)		wglGetProcAddress("glActiveTextureARB");
		glMultiTexCoord2fARB	= (PFNGLMULTITEXCOORD2FARBPROC)		wglGetProcAddress("glMultiTexCoord2fARB");

<LE COORDINATE>
Poichè ho creato le coordinate della sfera con numeri interi abbastanza grandi
ho la necessità di ridimensionare tutta la sfera.
	glScalef (0.0125 * R, 0.0125 * R, 0.0125 * R);
Devo inoltre ruotarla per dare un'effetto di inclinazione dell'asse di rotazione terrestre
	glRotatef (70, 1, 0, 0);
*/

#include <iostream>
#include <stdlib.h>
#include <vector>
#include <math.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

// File da includere per usare le estensioni di GLUT
#include "glext.h"
#include "image.h"

// ====================================================
// SEZIONE 1: VARIABILI
// ====================================================

//In questo array vengono salvati i puntatori alle texture generate da GLUT.
UINT texture[6];

// queste sono le variabili booleane che determinano quali texture sono visibili
// e se è il caso di fare l'animazione
bool animaTerra = true;
bool showTexture[6];

/*
Questi sono i puntatori alle funzioni che vengono usate per chiamare le funzioni
necessarie per il multitexturing.
Vengono inizializzati dinamicamente con la funzione wglGetProcAddress,
e bisogna avere incluso glext.h.

Il primo puntatore a funzione è l'equivalente di glTexCoord2f(), ma per più texture.
ARB sta per Architecture Review Board, che ha definito questi standard.

Il secondo puntatore a funzione ci permette di definire quale texture stiamo per utilizzare.
*/
PFNGLMULTITEXCOORD2FARBPROC		glMultiTexCoord2fARB	= NULL;
PFNGLACTIVETEXTUREARBPROC		glActiveTextureARB		= NULL;

// L'angolo che viene applicato al globo ogni volta che viene disegnato.
// Viene incrementato per fare l'effetto rotante.
double angolo = 0;

// Questa struttura contiene 5 coordinate: X,Y,Z sono la posizione di ogni punto
// che compone i vertici dei triangoli della sfera.
// U, V sono le coordinate planari della texture associate.
typedef struct 
{
	int X;
	int Y;
	int Z;
	double U;
	double V;
} PUNTO5;

const double PI = 3.1415926535897;
// Questa costante rappresenta la distanza tra i vertici dei triangolini
// Determina direttamente il numero di triangoli che compongono la sfera
// 10 risulta un buon compromesso tra qualità e prestazioni
const int DIST = 10;
const int Nvertici = (90 / DIST) * (360 / DIST) * 4;

// Array che contiene tutti i vertici dei triangoli che compongono
// la sfera associati alla corrispondente posizione della texture.
PUNTO5 VERTICI[Nvertici];

// Funzione per generare texture a partire da immagini BITMAP
GLuint GetTextureBMP( const char * filename, int );

// Funzione che disegna la sfera ad ogni refresh
void DrawSphere(double R){
	int b;

	static float wrap = 0;

	glScalef (0.0125 * R, 0.0125 * R, 0.0125 * R);

	glRotatef(70, 1, 0, 0);

	glShadeModel(GL_SMOOTH);
	glEnable( GL_TEXTURE_2D );

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,  texture[0]);

	glActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,  texture[1]);

	glActiveTextureARB(GL_TEXTURE2_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,  texture[2]);

	glActiveTextureARB(GL_TEXTURE3_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,  texture[3]);

	glActiveTextureARB(GL_TEXTURE4_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,  texture[4]);

	glActiveTextureARB(GL_TEXTURE5_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,  texture[5]);

	glBegin (GL_TRIANGLE_STRIP);

	//glColor3f(0.0f,0.0f,1.0f);
	for ( b = 0; b <= Nvertici-1; b++){
		if(showTexture[0])
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB, VERTICI[b].U, -VERTICI[b].V);
		if(showTexture[1])
			glMultiTexCoord2fARB(GL_TEXTURE1_ARB, VERTICI[b].U, -VERTICI[b].V);
		if(showTexture[2])
			glMultiTexCoord2fARB(GL_TEXTURE2_ARB, VERTICI[b].U, -VERTICI[b].V);
		if(showTexture[3])
			glMultiTexCoord2fARB(GL_TEXTURE3_ARB, VERTICI[b].U, -VERTICI[b].V);
		if(showTexture[4])
			glMultiTexCoord2fARB(GL_TEXTURE4_ARB, VERTICI[b].U, -VERTICI[b].V);
		if(showTexture[5])
			glMultiTexCoord2fARB(GL_TEXTURE5_ARB, VERTICI[b].U+wrap, -VERTICI[b].V);
		glVertex3f(VERTICI[b].X, VERTICI[b].Y, -VERTICI[b].Z);
	}
	wrap-=0.0003f;

	glEnd();

	glBegin (GL_TRIANGLE_STRIP);
		glTexCoord2f (VERTICI[b].U, VERTICI[b].V);
		glVertex3f(0.0f, 0.0f, 70.0f);

	for ( b = 0; b <= Nvertici-1; b++){
		if(showTexture[0])
			glMultiTexCoord2fARB(GL_TEXTURE0_ARB, VERTICI[b].U, VERTICI[b].V);
		if(showTexture[1])
			glMultiTexCoord2fARB(GL_TEXTURE1_ARB, VERTICI[b].U, VERTICI[b].V);
		if(showTexture[2])
			glMultiTexCoord2fARB(GL_TEXTURE2_ARB, VERTICI[b].U, VERTICI[b].V);
		if(showTexture[3])
			glMultiTexCoord2fARB(GL_TEXTURE3_ARB, VERTICI[b].U, VERTICI[b].V);
		if(showTexture[4])
			glMultiTexCoord2fARB(GL_TEXTURE4_ARB, VERTICI[b].U, VERTICI[b].V);
		if(showTexture[5])
			glMultiTexCoord2fARB(GL_TEXTURE5_ARB, VERTICI[b].U+wrap, VERTICI[b].V);
		glVertex3f(VERTICI[b].X, VERTICI[b].Y, VERTICI[b].Z);
	}

	glEnd();
	glDisable( GL_TEXTURE_2D );
}

// Funzione che riempie l'array dei vertici dei triangoli della sfera
// secondo le funzioni matematiche apposite.
// Inoltre vengono calcolati U e V (coordinate della texture)
// con una trasformazione da coordinate sferiche a coordinate planari.
// R è il raggio della sfera
// H la traslazione lungo l'asse X
// K la traslazione lungo l'asse Y
// Z la traslazione lungo l'asse Z
void GeneraSfera(double R, double H, double K, double Z) {
	int n;
	double a,b;
	n = 0;

	
	// Qui calcolo le coordinate delle textures. Ho usato la formula 2*b
	// per la coordinata V della texture perchè questa è larga il doppio
	// rispetto a quanto è alta (rapporto 2:1). Si può togliere il prodotto
	// (2*) se si usano texture quadrate o impostare il giusto scalaggio.
	for( b = 0; b <= 90 - DIST; b+=DIST){
		for( a = 0; a <= 360 - DIST; a+=DIST){

			VERTICI[n].X = R * sin((a) / 180 * PI) * sin((b) / 180 * PI) - H;
			VERTICI[n].Y = R * cos((a) / 180 * PI) * sin((b) / 180 * PI) + K;
			VERTICI[n].Z = R * cos((b) / 180 * PI) - Z;
			VERTICI[n].V = (2 * b) / 360;
			VERTICI[n].U = (a) / 360;
			n++;

			VERTICI[n].X = R * sin((a) / 180 * PI) * sin((b + DIST) / 180 * PI) - H;
			VERTICI[n].Y = R * cos((a) / 180 * PI) * sin((b + DIST) / 180 * PI) + K;
			VERTICI[n].Z = R * cos((b + DIST) / 180 * PI) - Z;
			VERTICI[n].V = (2 * (b + DIST)) / 360;
			VERTICI[n].U = (a) / 360;
			n++;

			VERTICI[n].X = R * sin((a + DIST) / 180 * PI) * sin((b) / 180 * PI) - H;
			VERTICI[n].Y = R * cos((a + DIST) / 180 * PI) * sin((b) / 180 * PI) + K;
			VERTICI[n].Z = R * cos((b) / 180 * PI) - Z;
			VERTICI[n].V = (2 * b) / 360;
			VERTICI[n].U = (a + DIST) / 360;
			n++;

			VERTICI[n].X = R * sin((a + DIST) / 180 * PI) * sin((b + DIST) / 180 * PI) - H;
			VERTICI[n].Y = R * cos((a + DIST) / 180 * PI) * sin((b + DIST) / 180 * PI) + K;
			VERTICI[n].Z = R * cos((b + DIST) / 180 * PI) - Z;
			VERTICI[n].V = (2 * (b + DIST)) / 360;
			VERTICI[n].U = (a + DIST) / 360;
			n++;

		}
	}
}

void display(void) {
	//Sleep(1);
	glClearDepth(1);
	glClearColor (0.0,0.0,0.0,1.0);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

	glTranslatef(0,0,-10);

	// Applico la rotazione alla sfera.
	// Cambia ogni volta che viene richiamata display
	glPushMatrix();
	// Eseguo la rotazione sull'asse Y
	glRotatef(angolo,0,1,0);
	// Disegno la sfera di dimensione 4
	DrawSphere(4);
	glPopMatrix();

	glutSwapBuffers();
	
	// incremento l'angolo di un po'
	if(animaTerra)
		angolo+=0.3;
}

void init (void) {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);

	// Qui carichiamo i puntatori alle funzioni che si occupano del multi-texturing.
	// Vengono presi gli indirizzi di queste con la funzione wglGetProcAddress, che restituisce
	// un indirizzo che dipende dal "rendering context" corrente di OpenGL.
	// Deve essere effettuato un casting alle nostre funzioni desiderate.
	glActiveTextureARB		= (PFNGLACTIVETEXTUREARBPROC)		wglGetProcAddress("glActiveTextureARB");
	glMultiTexCoord2fARB	= (PFNGLMULTITEXCOORD2FARBPROC)		wglGetProcAddress("glMultiTexCoord2fARB");

	// Controllo se esistono le funzioni che gestiscono il multitexturing.
	// Se non vengono trovate le funzioni corrispondenti i puntatori rimangono NULL
	if(!glActiveTextureARB || !glMultiTexCoord2fARB)
	{
		// scrivo il messaggio di errore ed esco
		printf("La versione di OpenGL installata non supporta il multitexturing.\n");
		exit(1);
	}
	
	// imposto le texture inizialmente visualizzate
	showTexture[0] = true;	// la Terra
	showTexture[1] = true;	// la griglia
	showTexture[2] = false;	// europa
	showTexture[3] = false;	// america
	showTexture[4] = false;	// oceania
	showTexture[5] = true;	// le nuvole

	// Carico dai files tutte le texture utilizzate
	texture[0] = GetTextureBMP( "earth.bmp", GL_MODULATE );
	texture[1] = GetTextureBMP( "earth-grid.bmp", GL_MODULATE );
	texture[2] = GetTextureBMP( "earth-europa.bmp", GL_DECAL );
	texture[3] = GetTextureBMP( "earth-america.bmp", GL_DECAL );
	texture[4] = GetTextureBMP( "earth-oceania.bmp", GL_DECAL );
	texture[5] = GetTextureBMP( "fog.bmp", GL_REPLACE );
	// Genero la sfera con dimensione 70
	GeneraSfera(70,0,0,0);
}

// Funzione che gestisce il ridimensionamento dela finestra
// Cerca di non fare deformare la sfera.
void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	gluPerspective (60, (GLfloat)w / (GLfloat)h, 0.1, 100.0);
	glMatrixMode (GL_MODELVIEW);
}

// Funzione che riceve gli input da tastiera e li elabora
void myKeyboard( unsigned char key, int x, int y) {
    switch (key) {
		case 27 :
		case 'q':
		case 'Q':  
        exit(0);
      case ' ': animaTerra=!animaTerra; break;
	  case '1': showTexture[0]=!showTexture[0]; break;
	  case '2': showTexture[1]=!showTexture[1]; break;
	  case '3': showTexture[2]=!showTexture[2]; break;
	  case '4': showTexture[3]=!showTexture[3]; break;
	  case '5': showTexture[4]=!showTexture[4]; break;
	  case '6': showTexture[5]=!showTexture[5]; break;
    }
	glutPostRedisplay();
}

// Finalmente, il MAIN
int main(int argc, char **argv) {
	printf("Durante l'esecuzione della dimostrazione è possibile agire con i seguenti input da tastiera:\n\n");
	printf(" \t(spazio) ferma la rotazione della Terra o la fa ripartire (non ferma le nuvole)\n");
	printf("1\tattiva/disattiva la texture della Terra\n");
	printf("2\tattiva/disattiva la texture della griglia\n");
	printf("3\tattiva/disattiva macchia rossa su Europa\n");
	printf("4\tattiva/disattiva macchia rossa su America\n");
	printf("5\tattiva/disattiva macchia rossa su Oceania\n");
	printf("6\tattiva/disattive le nuvole\n");

    glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(700, 700);
	glutInitWindowPosition (100, 100);
    glutCreateWindow("SFERA ROTANTE CON SOVRAPPOSIZIONE DI TESSITURE");
	init();
    glutDisplayFunc(display);
	glutKeyboardFunc( myKeyboard );
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
    glutMainLoop();
    return 0;
}

// Funzione che genera la texture leggendo da file BITMAP
// Utilizzo la classe Image e la funzione loadBMP del file image.h
GLuint GetTextureBMP( const char * filename, int filter ) {
	Image *image = loadBMP(filename);
	GLuint texture;

	glGenTextures( 1, &texture ); 
	glBindTexture( GL_TEXTURE_2D, texture );
	
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, filter ); 
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->pixels );
	delete(image);
	return texture;
}