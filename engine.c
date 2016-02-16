#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <malloc.h>
#include "math.h"
#include "opengl.h"
#include "image.h"
#include "font.h"
#include "3ds.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

#ifdef __GNUC__
#ifndef __int64
#define __int64 long long
#endif
#endif

GLContext_t Context;

char szAppName[]="OpenGL";

int Width=1024, Height=576;

int Done=0, Key[256];

unsigned __int64 Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;
int Frames=0;

GLint VAO;

typedef enum
{
	HELLKNIGHT_TEXTURE_BASE,
	HELLKNIGHT_TEXTURE_NORMAL,
	PINKY_TEXTURE_BASE,
	PINKY_TEXTURE_NORMAL,
	FATTY_TEXTURE_BASE,
	FATTY_TEXTURE_NORMAL,
	TILE_TEXTURE_BASE,
	TILE_TEXTURE_NORMAL,

	GLSL_LIGHT_SHADER,
	GLSL_LIGHT_MVINV,
	GLSL_LIGHT_MVP,
	GLSL_LIGHT_LIGHT0_POS,
	GLSL_LIGHT_LIGHT0_KD,
	GLSL_LIGHT_LIGHT1_POS,
	GLSL_LIGHT_LIGHT1_KD,
	GLSL_LIGHT_LIGHT2_POS,
	GLSL_LIGHT_LIGHT2_KD,
	GLSL_LIGHT_TEXTURE_BASE,
	GLSL_LIGHT_TEXTURE_NORMAL,

	NUM_OBJECTS
} ObjectNames;

unsigned int Objects[NUM_OBJECTS];

Model3DS_t Model[4];

float RotateX=0.0f, RotateY=0.0f, PanX=0.0f, PanY=0.0f, Zoom=-100.0f;

float Projection[16], ModelView[16], MVP[16];
float QuatX[4], QuatY[4], Quat[4];

float Light0_Pos[4]={ 0.0f, -25.0f, 0.0f, 1.0f/256.0f };
float Light0_Kd[4]={ 1.0f, 0.0f, 0.0f, 1.0f };

float Light1_Pos[4]={ 0.0f, 50.0f, 0.0f, 1.0f/256.0f };
float Light1_Kd[4]={ 0.0f, 1.0f, 0.0f, 1.0f };

float Light2_Pos[4]={ 0.0f, -80.0f, -15.0f, 1.0f/256.0f };
float Light2_Kd[4]={ 0.0f, 0.0f, 1.0f, 1.0f };

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void Render(void);
BOOL Init(void);
GLuint CreateProgram(char *VertexFilename, char *FragmentFilename);

unsigned __int64 rdtsc(void)
{
	unsigned long l, h;

#ifdef __GNUC__
	asm("rdtsc;"
		"movl %%eax, %0;"
		"movl %%edx, %1;"
		: "=r" (l), "=r" (h)
		:
		: "%eax", "%edx");
#else
	__asm
	{
		xor eax, eax
		xor ebx, ebx
		xor ecx, ecx
		xor edx, edx
		cpuid
		rdtsc
		mov l, eax
		mov h, edx
	}
#endif

	return ((unsigned __int64)l|((unsigned __int64)h<<32));
}

#ifdef __linux__
Display *dpy;

#include <time.h>

unsigned __int64 GetFrequency(void)
{
	unsigned __int64 StartTicks, StopTicks;
	struct timeval TimeStart, TimeStop;
	volatile unsigned long i;

	gettimeofday(&TimeStart, NULL);
	StartTicks=rdtsc();

	for(i=0;i<1000000;i++);

	StopTicks=rdtsc();
	gettimeofday(&TimeStop, NULL);

	return (StopTicks-StartTicks)*1000000/(TimeStop.tv_usec-TimeStart.tv_usec);
}

void EventLoop(void)
{
	int Keysym;
	XEvent Event;

	while(!Done)
	{
		while(XPending(dpy)>0)
		{
			XNextEvent(dpy, &Event);

			switch(Event.type)
			{
				case Expose:
					break;

				case ConfigureNotify:
					Width=Event.xconfigure.width;
					Height=Event.xconfigure.height;
					break;

				case ButtonPress:
					break;

				case KeyPress:
					Keysym=XLookupKeysym(&Event.xkey, 0);
					Key[Keysym]=1;

					switch(Keysym)
					{
						case XK_Escape:
							Done=1;
							break;

						default:
							break;
					}
					break;

				case KeyRelease:
					Keysym=XLookupKeysym(&Event.xkey, 0);
					Key[Keysym]=0;

					switch(Keysym)
					{
						default:
							break;
					}
					break;
			}
		}

		StartTime=rdtsc();
		Render();
		glXSwapBuffers(dpy, Context.win);
		EndTime=rdtsc();

		fTimeStep=(float)(EndTime-StartTime)/Frequency;
		fTime+=fTimeStep;
		avgfps+=1.0f/fTimeStep;

		if(Frames++>100)
		{
			fps=avgfps/Frames;
			avgfps=0.0f;
			Frames=0;
		}
	}
}

int main(int argc, char **argv)
{
	XVisualInfo *visinfo=NULL;
	XSetWindowAttributes Attrib;
	XTextProperty WindowProp;
	Window root;

	dpy=XOpenDisplay(NULL);

	if(dpy==NULL)
	{
		fprintf(stderr, "Can't open display\n");

		return -1;
	}

	Frequency=GetFrequency();

	if(!CreateContext(&Context, 32, 24, 0, 0, OGL_CORE33|OGL_DOUBLEBUFFER|OGL_SAMPLES8X))
	{
		fprintf(stderr, "Error: CreateContext failed\n");
		XCloseDisplay(dpy);

		return -1;
	}

	if(Context.visualid)
	{
		XVisualInfo visinfotemp;
		int visinfoitems;

		visinfotemp.visualid=Context.visualid;
		visinfo=XGetVisualInfo(dpy, VisualIDMask, &visinfotemp, &visinfoitems);

		if(visinfo==NULL&&visinfoitems!=1)
			return -1;
	}

	root=RootWindow(dpy, DefaultScreen(dpy));

	Attrib.background_pixel=0;
	Attrib.border_pixel=0;
	Attrib.colormap=XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	Attrib.event_mask=StructureNotifyMask|ExposureMask|ButtonPressMask|KeyPressMask|KeyReleaseMask;

	Context.win=XCreateWindow(dpy, root, 0, 0, Width, Height, 0, visinfo->depth, InputOutput, visinfo->visual, CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &Attrib);

	XStringListToTextProperty(&szAppName, 1, &WindowProp);
	XSetWMName(dpy, Context.win, &WindowProp);
	XFree(WindowProp.value);

	XMapWindow(dpy, Context.win);

	XFree(visinfo);

	glXMakeCurrent(dpy, Context.win, Context.ctx);

	if(!Init())
	{
		fprintf(stderr, "Error: Init failed\n");

		DestroyContext(&Context);
		XDestroyWindow(dpy, Context.win);
		XCloseDisplay(dpy);

		return -1;
	}

	EventLoop();

	Free3DS(&Model[0]);
	Free3DS(&Model[1]);
	Free3DS(&Model[2]);

	DestroyContext(&Context);
	XDestroyWindow(dpy, Context.win);

	XCloseDisplay(dpy);

	return 0;
}
#endif

#ifdef WIN32
unsigned __int64 GetFrequency(void)
{
	unsigned __int64 TimeStart, TimeStop, TimeFreq;
	unsigned __int64 StartTicks, StopTicks;
	volatile unsigned __int64 i;

	QueryPerformanceFrequency((LARGE_INTEGER *)&TimeFreq);

	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStart);
	StartTicks=rdtsc();

	for(i=0;i<1000000;i++);

	StopTicks=rdtsc();
	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStop);

	return (StopTicks-StartTicks)*TimeFreq/(TimeStop-TimeStart);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	MSG msg;
	RECT Rect;

	wc.style=CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
	wc.lpfnWndProc=WndProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=hInstance;
	wc.hIcon=LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=szAppName;

	RegisterClass(&wc);

	SetRect(&Rect, 0, 0, Width, Height);
	AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);

	Context.hWnd=CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(Context.hWnd, SW_SHOW);
	SetForegroundWindow(Context.hWnd);

	if(!CreateContext(&Context, 32, 24, 0, 0, OGL_CORE33|OGL_DOUBLEBUFFER|OGL_SAMPLES8X))
	{
		DestroyWindow(Context.hWnd);

		return -1;
	}

	if(!Init())
	{
		DestroyContext(&Context);
		DestroyWindow(Context.hWnd);

		return -1;
	}

	Frequency=GetFrequency();

	while(!Done)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				Done=1;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			StartTime=rdtsc();
			Render();
			SwapBuffers(Context.hDC);
			EndTime=rdtsc();

			fTimeStep=(float)(EndTime-StartTime)/Frequency;
			fTime+=fTimeStep;
			avgfps+=1.0f/fTimeStep;

			if(Frames++>100)
			{
				fps=avgfps/Frames;
				avgfps=0.0f;
				Frames=0;
			}
		}
	}

	Free3DS(&Model[0]);
	Free3DS(&Model[1]);
	Free3DS(&Model[2]);

	DestroyContext(&Context);
	DestroyWindow(Context.hWnd);

	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static POINT old;
	POINT pos, delta;

	switch(uMsg)
	{
		case WM_CREATE:
			break;

		case WM_CLOSE:
			PostQuitMessage(0);
			break;

		case WM_DESTROY:
			break;

		case WM_SIZE:
			Width=LOWORD(lParam);
			Height=HIWORD(lParam);
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			SetCapture(hWnd);
			ShowCursor(FALSE);

			GetCursorPos(&pos);
			old.x=pos.x;
			old.y=pos.y;
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			ShowCursor(TRUE);
			ReleaseCapture();
			break;

		case WM_MOUSEMOVE:
			GetCursorPos(&pos);

			if(!wParam)
			{
				old.x=pos.x;
				old.y=pos.y;
				break;
			}

			delta.x=pos.x-old.x;
			delta.y=old.y-pos.y;

			if(!delta.x&&!delta.y)
				break;

			SetCursorPos(old.x, old.y);

			switch(wParam)
			{
				case MK_LBUTTON:
					RotateX+=delta.x;
					RotateY-=delta.y;
					break;

				case MK_MBUTTON:
					PanX+=delta.x;
					PanY+=delta.y;
					break;

				case MK_RBUTTON:
					Zoom+=delta.y;
					break;
			}
			break;

		case WM_KEYDOWN:
			Key[wParam]=1;

			switch(wParam)
			{
				case VK_ESCAPE:
					PostQuitMessage(0);
					break;

				default:
					break;
			}
			break;

		case WM_KEYUP:
			Key[wParam]=0;
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
#endif

void DrawModel3DS(Model3DS_t *Model)
{
	int i;

	for(i=0;i<Model->NumMesh;i++)
	{
		glBindBuffer(GL_ARRAY_BUFFER, Model->Mesh[i].VertID);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*16, BUFFER_OFFSET(sizeof(float)*0));	//Vertex
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float)*16, BUFFER_OFFSET(sizeof(float)*4));	//UV
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float)*16, BUFFER_OFFSET(sizeof(float)*7));	//TANGENT
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float)*16, BUFFER_OFFSET(sizeof(float)*10));	//BINORMAL
		glEnableVertexAttribArray(3);

		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(float)*16, BUFFER_OFFSET(sizeof(float)*13));	//NORMAL
		glEnableVertexAttribArray(4);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Model->Mesh[i].ElemID);
		glDrawElements(GL_TRIANGLES, Model->Mesh[i].NumFace*3, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void BuildVBO3DS(Model3DS_t *Model)
{
	int i, j;

	for(i=0;i<Model->NumMesh;i++)
	{
		void *data=NULL;
		float *fPtr=NULL;
		unsigned short *sPtr=NULL;

		if(!Model->Mesh[i].NumVertex)
			continue;

		glGenBuffers(1, &Model->Mesh[i].VertID);
		glBindBuffer(GL_ARRAY_BUFFER, Model->Mesh[i].VertID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*Model->Mesh[i].NumVertex*16, NULL, GL_STATIC_DRAW);

		if(glGetError()==GL_NO_ERROR)
			data=glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

		if(data==NULL)
		{
			char temp[1024];

			sprintf(temp, "VBO data buffer memory map failed or out of memory for object: %s", Model->Mesh[i].Name);
			MessageBox(NULL, temp, "Error", MB_OK);

			glDeleteBuffers(1, &Model->Mesh[i].VertID);
			Model->Mesh[i].VertID=0;
			break;
		}

		for(j=0, fPtr=(float *)data;j<Model->Mesh[i].NumVertex;j++)
		{
			// Copy vertex/texture/tangent/binormal/normal data, padded to 16 floats (64 byte alignment, is this needed?)
			*fPtr++=Model->Mesh[i].Vertex[3*j+0];
			*fPtr++=Model->Mesh[i].Vertex[3*j+1];
			*fPtr++=Model->Mesh[i].Vertex[3*j+2];
			*fPtr++=1.0f; // Padding
			*fPtr++=Model->Mesh[i].UV[2*j+0];
			*fPtr++=Model->Mesh[i].UV[2*j+1];
			*fPtr++=0.0f; // Padding
			*fPtr++=Model->Mesh[i].Tangent[3*j+0];
			*fPtr++=Model->Mesh[i].Tangent[3*j+1];
			*fPtr++=Model->Mesh[i].Tangent[3*j+2];
			*fPtr++=Model->Mesh[i].Binormal[3*j+0];
			*fPtr++=Model->Mesh[i].Binormal[3*j+1];
			*fPtr++=Model->Mesh[i].Binormal[3*j+2];
			*fPtr++=Model->Mesh[i].Normal[3*j+0];
			*fPtr++=Model->Mesh[i].Normal[3*j+1];
			*fPtr++=Model->Mesh[i].Normal[3*j+2];
		}

		glUnmapBuffer(GL_ARRAY_BUFFER);

		glGenBuffers(1, &Model->Mesh[i].ElemID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Model->Mesh[i].ElemID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short)*Model->Mesh[i].NumFace*3, NULL, GL_STATIC_DRAW);

		if(glGetError()==GL_NO_ERROR)
			data=glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

		if(data==NULL)
		{
			glDeleteBuffers(1, &Model->Mesh[i].ElemID);
			Model->Mesh[i].ElemID=0;
			break;
		}

		for(j=0, sPtr=(unsigned short *)data;j<Model->Mesh[i].NumFace;j++)
		{
			*sPtr++=Model->Mesh[i].Face[3*j+0];
			*sPtr++=Model->Mesh[i].Face[3*j+1];
			*sPtr++=Model->Mesh[i].Face[3*j+2];
		}

		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Render(void)
{
	Light0_Pos[0]=sinf(fTime)*150.0f;
	Light0_Pos[2]=cosf(fTime)*150.0f;

	Light1_Pos[0]=cosf(fTime)*100.0f;
	Light1_Pos[2]=sinf(fTime)*100.0f;

	Light2_Pos[0]=cosf(fTime)*100.0f;

//	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	// Set viewport and calculate a projection matrix (perspective, with infinite z-far plane)
	glViewport(0, 0, Width, Height);
	MatrixIdentity(Projection);
	InfPerspective(90.0f, (float)Width/Height, 0.01f, Projection);

	// Set up model view matrix (translate and rotation)
	MatrixIdentity(ModelView);
	MatrixTranslate(PanX, PanY, Zoom, ModelView);

	QuatAngle(RotateX, 0.0f, 1.0f, 0.0f, QuatX);
	QuatAngle(RotateY, 1.0f, 0.0f, 0.0f, QuatY);
	QuatMultiply(QuatX, QuatY, Quat);
	QuatMatrix(Quat, ModelView);

	MatrixMult(ModelView, Projection, MVP);

	// Select the shader program
	glUseProgram(Objects[GLSL_LIGHT_SHADER]);

	// Sampler texture unit assignments
	glUniform1i(Objects[GLSL_LIGHT_TEXTURE_BASE], 0);
	glUniform1i(Objects[GLSL_LIGHT_TEXTURE_NORMAL], 1);

	// Light parameters
	glUniform4fv(Objects[GLSL_LIGHT_LIGHT0_POS], 1, Light0_Pos);
	glUniform4fv(Objects[GLSL_LIGHT_LIGHT0_KD], 1, Light0_Kd);
	glUniform4fv(Objects[GLSL_LIGHT_LIGHT1_POS], 1, Light1_Pos);
	glUniform4fv(Objects[GLSL_LIGHT_LIGHT1_KD], 1, Light1_Kd);
	glUniform4fv(Objects[GLSL_LIGHT_LIGHT2_POS], 1, Light2_Pos);
	glUniform4fv(Objects[GLSL_LIGHT_LIGHT2_KD], 1, Light2_Kd);

	// Inverse model view matrix for "eye" vector
	MatrixInverse(ModelView, ModelView);
	glUniformMatrix4fv(Objects[GLSL_LIGHT_MVINV], 1, GL_FALSE, ModelView);

	// Model view projection matrix for vertex to clipspace transform (vertex shader)
	glUniformMatrix4fv(Objects[GLSL_LIGHT_MVP], 1, GL_FALSE, MVP);

	// Bind correct textures per model
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Objects[HELLKNIGHT_TEXTURE_BASE]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, Objects[HELLKNIGHT_TEXTURE_NORMAL]);

	// Render model
	DrawModel3DS(&Model[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Objects[PINKY_TEXTURE_BASE]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, Objects[PINKY_TEXTURE_NORMAL]);

	DrawModel3DS(&Model[1]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Objects[FATTY_TEXTURE_BASE]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, Objects[FATTY_TEXTURE_NORMAL]);

	DrawModel3DS(&Model[2]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Objects[TILE_TEXTURE_BASE]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, Objects[TILE_TEXTURE_NORMAL]);

	DrawModel3DS(&Model[3]);

	glActiveTexture(GL_TEXTURE0);

	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
		// Because of shaders, zero origin is center of screen.
		// Could fix with a ortho projection transform, but it works as is.
		Font_Print(-(float)Width/2.0f, -(float)Height/2.0f, "FPS: %0.1f", fps);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

BOOL Init(void)
{
	// Load texture images
	Objects[HELLKNIGHT_TEXTURE_BASE]=Image_Upload("hellknight.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Objects[HELLKNIGHT_TEXTURE_NORMAL]=Image_Upload("hellknight_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Objects[PINKY_TEXTURE_BASE]=Image_Upload("pinky.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Objects[PINKY_TEXTURE_NORMAL]=Image_Upload("pinky_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Objects[FATTY_TEXTURE_BASE]=Image_Upload("fatty.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Objects[FATTY_TEXTURE_NORMAL]=Image_Upload("fatty_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Objects[TILE_TEXTURE_BASE]=Image_Upload("tile.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Objects[TILE_TEXTURE_NORMAL]=Image_Upload("tile_b.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALMAP|IMAGE_NORMALIZE);

	// Load models
	if(Load3DS(&Model[0], "hellknight.3ds"))
		BuildVBO3DS(&Model[0]);

	if(Load3DS(&Model[1], "pinky.3ds"))
		BuildVBO3DS(&Model[1]);

	if(Load3DS(&Model[2], "fatty.3ds"))
		BuildVBO3DS(&Model[2]);

	if(Load3DS(&Model[3], "level.3ds"))
		BuildVBO3DS(&Model[3]);

	// Load shaders
	Objects[GLSL_LIGHT_SHADER]=CreateProgram("light_v.glsl", "light_f.glsl");
	glUseProgram(Objects[GLSL_LIGHT_SHADER]);
	Objects[GLSL_LIGHT_MVINV]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "mvinv");
	Objects[GLSL_LIGHT_MVP]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "mvp");
	Objects[GLSL_LIGHT_LIGHT0_POS]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "Light0_Pos");
	Objects[GLSL_LIGHT_LIGHT0_KD]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "Light0_Kd");
	Objects[GLSL_LIGHT_LIGHT1_POS]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "Light1_Pos");
	Objects[GLSL_LIGHT_LIGHT1_KD]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "Light1_Kd");
	Objects[GLSL_LIGHT_LIGHT2_POS]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "Light2_Pos");
	Objects[GLSL_LIGHT_LIGHT2_KD]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "Light2_Kd");
	Objects[GLSL_LIGHT_TEXTURE_BASE]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "TexBase");
	Objects[GLSL_LIGHT_TEXTURE_NORMAL]=glGetUniformLocation(Objects[GLSL_LIGHT_SHADER], "TexNormal");

	// Set OpenGL states
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Dummy VAO, because I don't use them :P
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	return TRUE;
}

int LoadShader(GLuint Shader, char *Filename)
{
	FILE *stream;
	char *buffer;
	int length;

	if((stream=fopen(Filename, "rb"))==NULL)
		return 0;

	fseek(stream, 0, SEEK_END);
	length=ftell(stream);
	fseek(stream, 0, SEEK_SET);

	buffer=(char *)malloc(length+1);
	fread(buffer, 1, length, stream);
	buffer[length]='\0';

	glShaderSource(Shader, 1, (const char **)&buffer, NULL);

	fclose(stream);
	free(buffer);

	return 1;
}

GLuint CreateProgram(char *VertexFilename, char *FragmentFilename)
{
	GLuint Program, Vertex, Fragment;
	GLint Status, LogLength;
	char *Log=NULL;

	Program=glCreateProgram();

	if(VertexFilename)
	{
		Vertex=glCreateShader(GL_VERTEX_SHADER);

		if(LoadShader(Vertex, VertexFilename))
		{
			glCompileShader(Vertex);
			glGetShaderiv(Vertex, GL_COMPILE_STATUS, &Status);

			if(!Status)
			{
				glGetShaderiv(Vertex, GL_INFO_LOG_LENGTH, &LogLength);
				Log=(char *)malloc(LogLength);

				if(Log)
				{
					glGetShaderInfoLog(Vertex, LogLength, NULL, Log);
					MessageBox(Context.hWnd, Log, VertexFilename, MB_OK);
					free(Log);
				}
			}
			else
				glAttachShader(Program, Vertex);
		}

		glDeleteShader(Vertex);
	}

	if(FragmentFilename)
	{
		Fragment=glCreateShader(GL_FRAGMENT_SHADER);

		if(LoadShader(Fragment, FragmentFilename))
		{
			glCompileShader(Fragment);
			glGetShaderiv(Fragment, GL_COMPILE_STATUS, &Status);

			if(!Status)
			{
				glGetShaderiv(Fragment, GL_INFO_LOG_LENGTH, &LogLength);
				Log=(char *)malloc(LogLength);

				if(Log)
				{
					glGetShaderInfoLog(Fragment, LogLength, NULL, Log);
					MessageBox(Context.hWnd, Log, FragmentFilename, MB_OK);
					free(Log);
				}
			}
			else
				glAttachShader(Program, Fragment);
		}

		glDeleteShader(Fragment);
	}

	glLinkProgram(Program);
	glGetProgramiv(Program, GL_LINK_STATUS, &Status);

	if(!Status)
	{
		glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
		Log=(char *)malloc(LogLength);

		if(Log)
		{
			glGetProgramInfoLog(Program, LogLength, NULL, Log);
			MessageBox(Context.hWnd, Log, "Link", MB_OK);
			free(Log);
		}
	}

	return Program;
}
