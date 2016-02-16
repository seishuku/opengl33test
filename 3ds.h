#ifndef __3DS_H__
#define __3DS_H__

#pragma pack(push, 1)

typedef struct
{
	unsigned short ID;
	unsigned long Length;
} Chunk3DS_t;

typedef struct
{
	char Name[32];
	float Ambient[4];
	float Diffuse[4];
	float Specular[4];
	float Emission[4];
	float Shininess;
	char Texture[32];
} Material3DS_t;

typedef struct
{
	char Name[32];
	int Material;
	int Start;
	int Size;
	unsigned short *Face;
} Group3DS_t;

typedef struct
{
	char Name[32];

	int NumVertex;
	float *Vertex;
	float *Tangent;
	float *Binormal;
	float *Normal;

	int NumUV;
	float *UV;

	int NumFace;
	unsigned short *Face;

	int NumGroup;
	Group3DS_t *Group;

	long *Smooth;

	unsigned int VertID, ElemID;

	float Minimum[3], Maximum[3], Center[3];
} Mesh3DS_t;

typedef struct
{
	int NumMaterial;
	Material3DS_t *Material;

	int NumMesh;
	Mesh3DS_t *Mesh;
} Model3DS_t;

#pragma pack(pop)

int Load3DS(Model3DS_t *Model, char *Filename);
void Free3DS(Model3DS_t *Model);

#endif
