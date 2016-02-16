#include "opengl.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "math.h"
#include "font.h"
#include "fontdata.h"

extern GLuint CreateProgram(char *VertexFilename, char *FragmentFilename);

unsigned int Font_TextureID=0;
unsigned int Font_ShaderID=0;
unsigned int Font_Shader_vVert_ID=0;
unsigned int Font_Shader_Font_ID=0;
unsigned int Font_VBOID=0;

extern int Width, Height;

void Font_Print(float x, float y, char *string, ...)
{
	float *verts=NULL;
	char *ptr, text[4096];
	va_list	ap;
	int sx=(int)x, numchar, i;
	float cx, cy, Scale=2.0f;

	// Generate texture, shaders, etc once
	if(!Font_TextureID&&!Font_VBOID&&!Font_ShaderID)
	{

		glGenTextures(1, &Font_TextureID);
		glBindTexture(GL_TEXTURE_2D, Font_TextureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 256, 0, GL_RED, GL_UNSIGNED_BYTE, _FontData);

		Font_ShaderID=CreateProgram("font_v.glsl", "font_f.glsl");
		glUseProgram(Font_ShaderID);
		Font_Shader_vVert_ID=glGetAttribLocation(Font_ShaderID, "vVert");
		Font_Shader_Font_ID=glGetUniformLocation(Font_ShaderID, "Font");

		glGenBuffers(1, &Font_VBOID);
	}

	if(string==NULL)
		return;

	va_start(ap, string);
	vsprintf(text, string, ap);
	va_end(ap);

	numchar=strlen(text);

	verts=(float *)malloc(sizeof(float)*4*6*numchar);

	if(verts==NULL)
		return;

	for(ptr=text;*ptr!='\0';ptr++)
	{
		if(*ptr=='\n'||*ptr=='\r')
			y+=12;
	}

	glUseProgram(Font_ShaderID);
	glUniform1i(Font_Shader_Font_ID, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Font_TextureID);

	for(ptr=text, i=0;*ptr!='\0';ptr++)
	{
		if(*ptr=='\n'||*ptr=='\r')
		{
			x=(float)sx;
			y-=12;
			continue;
		}

		cx=     (float)(*ptr%16)*0.0625f;
		cy=1.0f-(float)(*ptr/16)*0.0625f;

		verts[i++]=(x)/Width*Scale;
		verts[i++]=(y+16.0f)/Height*Scale;
		verts[i++]=cx;
		verts[i++]=cy;

		verts[i++]=(x)/Width*Scale;
		verts[i++]=(y)/Height*Scale;
		verts[i++]=cx;
		verts[i++]=cy-0.0625f;

		verts[i++]=(x+16.0f)/Width*Scale;
		verts[i++]=(y+16.0f)/Height*Scale;
		verts[i++]=cx+0.0625f;
		verts[i++]=cy;

		verts[i++]=(x+16.0f)/Width*Scale;
		verts[i++]=(y+16.0f)/Height*Scale;
		verts[i++]=cx+0.0625f;
		verts[i++]=cy;

		verts[i++]=(x)/Width*Scale;
		verts[i++]=(y)/Height*Scale;
		verts[i++]=cx;
		verts[i++]=cy-0.0625f;

		verts[i++]=(x+16.0f)/Width*Scale;
		verts[i++]=(y)/Height*Scale;
		verts[i++]=cx+0.0625f;
		verts[i++]=cy-0.0625f;

		x+=8;
	}

	glBindBuffer(GL_ARRAY_BUFFER, Font_VBOID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*4*6*numchar, verts, GL_STATIC_DRAW);

	// Only need one vec4 to pass vertex and texture coords
	glVertexAttribPointer(Font_Shader_vVert_ID, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(Font_Shader_vVert_ID);

	glDrawArrays(GL_TRIANGLES, 0, 6*numchar);

	glDisableVertexAttribArray(Font_Shader_vVert_ID);

	free(verts);
}

