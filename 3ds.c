#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <memory.h>
#include "math.h"
#include "3ds.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

typedef void (*cb)(Chunk3DS_t *, void *);

FILE *File3DS=NULL;
char szString[64];

typedef int (*sortFunc_t)(const void *, const void *);

typedef struct
{
	int count;
	int *indices;
	char *data;
	int dataSize;
	sortFunc_t sortFunc;
} index_t;

int indexAlloc(index_t *index)
{
	if(index->indices==NULL)
		index->indices=(int *)malloc(sizeof(int));
	else
		index->indices=(int *)realloc(index->indices, sizeof(int)*(index->count+1));

	if(index->indices==NULL)
		return -1;

	return 0;
}

int indexBinaryTraverse(void *value, void *data, int *indices, int stride, int count, int *result, int (*compare)(const void *, const void *))
{
	int high=count, low=0, mid, nextMid;

	mid=low+((high-low)>>1);
	*result=-1;

	while(low!=high)
	{
		*result=compare(value, (void *)((unsigned int)data+indices[mid]*stride));

		if(*result==0)
			break;
		else
		{
			if(*result<0)
				high=mid;
			else
				low=mid;
		}

		nextMid=low+((high-low)>>1);

		if(mid==nextMid)
			break;

		mid=nextMid;
	}

	return mid;
}

int indexSortedInsert(index_t *ind, int value)
{
	int position, result=0;

	position=indexBinaryTraverse((char *)ind->data+(value*ind->dataSize), ind->data, ind->indices, ind->dataSize, ind->count, &result, ind->sortFunc);

	if(indexAlloc(ind)==-1)
		return -1;

	if(result==0)
		return 0;
	else
	{
		if(result<0)
		{
			if(ind->count!=0)
				memmove(&ind->indices[position+1], &ind->indices[position], (ind->count-position)*sizeof(int));

			ind->indices[position]=value;
			ind->count++;

			return 0;
		}
		else
		{
			if((ind->count!=0)&&(position<(ind->count-1)))
				memmove(&ind->indices[position+2], &ind->indices[position+1], (ind->count-position-1)*sizeof(int));

			ind->indices[position+1]=value;
			ind->count++;

			return 0;
		}
	}
}

void indexArray(index_t *index, char *array, int size, int count, sortFunc_t sortFunc)
{
	int i;

	memset(index, 0, sizeof(index_t));
	index->data=array;
	index->dataSize=size;
	index->sortFunc=sortFunc;

	for(i=0;i<count;i++)
		indexSortedInsert(index, i);
}

int indexFind(index_t *index, void *element, int *result)
{
	return indexBinaryTraverse(element, index->data, index->indices, index->dataSize, index->count, result, index->sortFunc);
}

void indexFree(index_t *index)
{
	if(index->indices!=NULL)
		free(index->indices);

	memset(index, 0, sizeof(index_t));
}

int ComparePosition(float *a, float *b)
{
	int i;

	for(i=0;i<3;i++)
	{
		if(a[i]>b[i])
			return 1;

		if(a[i]<b[i])
			return -1;
	}

	return 0;
}

void CalculateTangent(Mesh3DS_t *Mesh)
{
	int i, j, k, Count, Vertex=0, Result=0;
	index_t VertexIndex;
	long (*TriangleRefs)[32];
	float *FaceTangent, *FaceBinormal, *FaceNormal;
	float v0[3], v1[3], uv0[2], uv1[2];
	float s[3], t[3], n[3], r;

	Mesh->Tangent=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Tangent==NULL)
		return;

	memset(Mesh->Tangent, 0, sizeof(float)*3*Mesh->NumVertex);

	Mesh->Binormal=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Binormal==NULL)
		return;

	memset(Mesh->Binormal, 0, sizeof(float)*3*Mesh->NumVertex);

	Mesh->Normal=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Normal==NULL)
		return;

	memset(Mesh->Normal, 0, sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Smooth==NULL)
	{
		for(i=0;i<Mesh->NumFace;i++)
		{
			unsigned short i1=Mesh->Face[3*i+0];
			unsigned short i2=Mesh->Face[3*i+1];
			unsigned short i3=Mesh->Face[3*i+2];

			v0[0]=Mesh->Vertex[3*i2+0]-Mesh->Vertex[3*i1+0];
			v0[1]=Mesh->Vertex[3*i2+1]-Mesh->Vertex[3*i1+1];
			v0[2]=Mesh->Vertex[3*i2+2]-Mesh->Vertex[3*i1+2];

			uv0[0]=Mesh->UV[2*i2+0]-Mesh->UV[2*i1+0];
			uv0[1]=Mesh->UV[2*i2+1]-Mesh->UV[2*i1+1];

			v1[0]=Mesh->Vertex[3*i3+0]-Mesh->Vertex[3*i1+0];
			v1[1]=Mesh->Vertex[3*i3+1]-Mesh->Vertex[3*i1+1];
			v1[2]=Mesh->Vertex[3*i3+2]-Mesh->Vertex[3*i1+2];

			uv1[0]=Mesh->UV[2*i3+0]-Mesh->UV[2*i1+0];
			uv1[1]=Mesh->UV[2*i3+1]-Mesh->UV[2*i1+1];

			r=1.0f/(uv0[0]*uv1[1]-uv1[0]*uv0[1]);

			s[0]=(uv1[1]*v0[0]-uv0[1]*v1[0])*r;
			s[1]=(uv1[1]*v0[1]-uv0[1]*v1[1])*r;
			s[2]=(uv1[1]*v0[2]-uv0[1]*v1[2])*r;
			Normalize(s);

			Mesh->Tangent[3*i1+0]+=s[0];	Mesh->Tangent[3*i1+1]+=s[1];	Mesh->Tangent[3*i1+2]+=s[2];
			Mesh->Tangent[3*i2+0]+=s[0];	Mesh->Tangent[3*i2+1]+=s[1];	Mesh->Tangent[3*i2+2]+=s[2];
			Mesh->Tangent[3*i3+0]+=s[0];	Mesh->Tangent[3*i3+1]+=s[1];	Mesh->Tangent[3*i3+2]+=s[2];

			t[0]=(uv0[0]*v1[0]-uv1[0]*v0[0])*r;
			t[1]=(uv0[0]*v1[1]-uv1[0]*v0[1])*r;
			t[2]=(uv0[0]*v1[2]-uv1[0]*v0[2])*r;
			Normalize(t);

			Mesh->Binormal[3*i1+0]-=t[0];	Mesh->Binormal[3*i1+1]-=t[1];	Mesh->Binormal[3*i1+2]-=t[2];
			Mesh->Binormal[3*i2+0]-=t[0];	Mesh->Binormal[3*i2+1]-=t[1];	Mesh->Binormal[3*i2+2]-=t[2];
			Mesh->Binormal[3*i3+0]-=t[0];	Mesh->Binormal[3*i3+1]-=t[1];	Mesh->Binormal[3*i3+2]-=t[2];

			Cross(v0, v1, n);
			Normalize(n);

			Mesh->Normal[3*i1+0]+=n[0];		Mesh->Normal[3*i1+1]+=n[1];		Mesh->Normal[3*i1+2]+=n[2];
			Mesh->Normal[3*i2+0]+=n[0];		Mesh->Normal[3*i2+1]+=n[1];		Mesh->Normal[3*i2+2]+=n[2];
			Mesh->Normal[3*i3+0]+=n[0];		Mesh->Normal[3*i3+1]+=n[1];		Mesh->Normal[3*i3+2]+=n[2];
		}

		return;
	}

	indexArray(&VertexIndex, (char *)Mesh->Vertex, sizeof(float)*3, Mesh->NumVertex, (sortFunc_t)ComparePosition);

	TriangleRefs=(long (*)[32])malloc(sizeof(long)*32*VertexIndex.count);

	if(TriangleRefs==NULL)
		return;

	memset(TriangleRefs, 0, sizeof(long)*32*VertexIndex.count);

	for(i=0;i<Mesh->NumFace;i++)
	{
		Vertex=indexFind(&VertexIndex, &Mesh->Vertex[3*Mesh->Face[3*i+0]], &Result);

		if(TriangleRefs[Vertex][0]<48)
		{
			TriangleRefs[Vertex][0]++;
			TriangleRefs[Vertex][TriangleRefs[Vertex][0]]=i;
		}

		Vertex=indexFind(&VertexIndex, &Mesh->Vertex[3*Mesh->Face[3*i+1]], &Result);

		if(TriangleRefs[Vertex][0]<48)
		{
			TriangleRefs[Vertex][0]++;
			TriangleRefs[Vertex][TriangleRefs[Vertex][0]]=i;
		}

		Vertex=indexFind(&VertexIndex, &Mesh->Vertex[3*Mesh->Face[3*i+2]], &Result);

		if(TriangleRefs[Vertex][0]<48)
		{
			TriangleRefs[Vertex][0]++;
			TriangleRefs[Vertex][TriangleRefs[Vertex][0]]=i;
		}
	}

	FaceTangent=(float *)malloc(sizeof(float)*3*Mesh->NumFace);

	if(FaceTangent==NULL)
		return;

	memset(FaceTangent, 0, sizeof(float)*3*Mesh->NumFace);

	FaceBinormal=(float *)malloc(sizeof(float)*3*Mesh->NumFace);

	if(FaceBinormal==NULL)
		return;

	memset(FaceBinormal, 0, sizeof(float)*3*Mesh->NumFace);

	FaceNormal=(float *)malloc(sizeof(float)*3*Mesh->NumFace);

	if(FaceNormal==NULL)
		return;

	memset(FaceNormal, 0, sizeof(float)*3*Mesh->NumFace);

	for(i=0;i<Mesh->NumFace;i++)
	{
		unsigned short i1=Mesh->Face[3*i+0];
		unsigned short i2=Mesh->Face[3*i+1];
		unsigned short i3=Mesh->Face[3*i+2];

		v0[0]=Mesh->Vertex[3*i2+0]-Mesh->Vertex[3*i1+0];
		v0[1]=Mesh->Vertex[3*i2+1]-Mesh->Vertex[3*i1+1];
		v0[2]=Mesh->Vertex[3*i2+2]-Mesh->Vertex[3*i1+2];

		uv0[0]=Mesh->UV[2*i2+0]-Mesh->UV[2*i1+0];
		uv0[1]=Mesh->UV[2*i2+1]-Mesh->UV[2*i1+1];

		v1[0]=Mesh->Vertex[3*i3+0]-Mesh->Vertex[3*i1+0];
		v1[1]=Mesh->Vertex[3*i3+1]-Mesh->Vertex[3*i1+1];
		v1[2]=Mesh->Vertex[3*i3+2]-Mesh->Vertex[3*i1+2];

		uv1[0]=Mesh->UV[2*i3+0]-Mesh->UV[2*i1+0];
		uv1[1]=Mesh->UV[2*i3+1]-Mesh->UV[2*i1+1];

		r=1.0f/(uv0[0]*uv1[1]-uv1[0]*uv0[1]);

		FaceTangent[3*i+0]=(uv1[1]*v0[0]-uv0[1]*v1[0])*r;
		FaceTangent[3*i+1]=(uv1[1]*v0[1]-uv0[1]*v1[1])*r;
		FaceTangent[3*i+2]=(uv1[1]*v0[2]-uv0[1]*v1[2])*r;
		Normalize(&FaceTangent[3*i]);

		FaceBinormal[3*i+0]=-(uv0[0]*v1[0]-uv1[0]*v0[0])*r;
		FaceBinormal[3*i+1]=-(uv0[0]*v1[1]-uv1[0]*v0[1])*r;
		FaceBinormal[3*i+2]=-(uv0[0]*v1[2]-uv1[0]*v0[2])*r;
		Normalize(&FaceBinormal[3*i]);

		Cross(v0, v1, &FaceNormal[3*i]);
		Normalize(&FaceNormal[3*i]);
	}

	for(i=0;i<Mesh->NumFace;i++)
	{
		for(j=0;j<3;j++)
		{
			Vertex=indexFind(&VertexIndex, (void *)&Mesh->Vertex[3*Mesh->Face[3*i+j]], &Result);

			Count=0;

			for(k=1;k<=TriangleRefs[Vertex][0];k++)
			{
				if(Mesh->Smooth[i]==Mesh->Smooth[TriangleRefs[Vertex][k]])
				{
					Mesh->Tangent[3*Mesh->Face[3*i+j]+0]+=FaceTangent[3*TriangleRefs[Vertex][k]+0];
					Mesh->Tangent[3*Mesh->Face[3*i+j]+1]+=FaceTangent[3*TriangleRefs[Vertex][k]+1];
					Mesh->Tangent[3*Mesh->Face[3*i+j]+2]+=FaceTangent[3*TriangleRefs[Vertex][k]+2];

					Mesh->Binormal[3*Mesh->Face[3*i+j]+0]+=FaceBinormal[3*TriangleRefs[Vertex][k]+0];
					Mesh->Binormal[3*Mesh->Face[3*i+j]+1]+=FaceBinormal[3*TriangleRefs[Vertex][k]+1];
					Mesh->Binormal[3*Mesh->Face[3*i+j]+2]+=FaceBinormal[3*TriangleRefs[Vertex][k]+2];

					Mesh->Normal[3*Mesh->Face[3*i+j]+0]+=FaceNormal[3*TriangleRefs[Vertex][k]+0];
					Mesh->Normal[3*Mesh->Face[3*i+j]+1]+=FaceNormal[3*TriangleRefs[Vertex][k]+1];
					Mesh->Normal[3*Mesh->Face[3*i+j]+2]+=FaceNormal[3*TriangleRefs[Vertex][k]+2];

					Count++;
				}
			}

			Mesh->Tangent[3*Mesh->Face[3*i+j]+0]/=(float)Count;
			Mesh->Tangent[3*Mesh->Face[3*i+j]+1]/=(float)Count;
			Mesh->Tangent[3*Mesh->Face[3*i+j]+2]/=(float)Count;

			Mesh->Binormal[3*Mesh->Face[3*i+j]+0]/=(float)Count;
			Mesh->Binormal[3*Mesh->Face[3*i+j]+1]/=(float)Count;
			Mesh->Binormal[3*Mesh->Face[3*i+j]+2]/=(float)Count;

			Mesh->Normal[3*Mesh->Face[3*i+j]+0]/=(float)Count;
			Mesh->Normal[3*Mesh->Face[3*i+j]+1]/=(float)Count;
			Mesh->Normal[3*Mesh->Face[3*i+j]+2]/=(float)Count;
		}
	}

	indexFree(&VertexIndex);

	for(i=0;i<Mesh->NumVertex;i++)
	{
		Normalize(&Mesh->Tangent[3*i]);
		Normalize(&Mesh->Binormal[3*i]);
		Normalize(&Mesh->Normal[3*i]);
	}

	FREE(TriangleRefs);
	FREE(FaceTangent);
	FREE(FaceBinormal);
	FREE(FaceNormal);
}

void ReadString(char *String)
{
	char *Ptr=String;

	for(;;Ptr++)
	{
		fread(Ptr, sizeof(char), 1, File3DS);

		if(*Ptr=='\0')
			break;
	}
}

void ReadChunkArray(long Length, void (*Callback)(Chunk3DS_t *, void *), void *Data)
{
	Chunk3DS_t Chunk;
	long Start, Position=0;

	do
	{
		Start=ftell(File3DS);
		fread(&Chunk, sizeof(Chunk3DS_t), 1, File3DS);

		Callback(&Chunk, Data);

		fseek(File3DS, Start+Chunk.Length, SEEK_SET);

		Position+=Chunk.Length;
	}
	while(Position<Length);
}

void InspectChunkArray(long Length, void (*Callback)(Chunk3DS_t *, void *), void *Data)
{
	Chunk3DS_t Chunk;
	long Start, ChunkStart, Position=0;

	ChunkStart=ftell(File3DS);

	do
	{
		Start=ftell(File3DS);

		fread(&Chunk, sizeof(Chunk3DS_t), 1, File3DS);

		Callback(&Chunk, Data);

		fseek(File3DS, Start+Chunk.Length, SEEK_SET);

		Position+=Chunk.Length;
	}
	while(Position<Length);

	fseek(File3DS, ChunkStart, SEEK_SET);
}

void ReadColorChunk(Chunk3DS_t *Chunk, float *Color)
{
	unsigned char RGB[3];

	switch(Chunk->ID)
	{
		case 0x0010:
			fread(Color, sizeof(float), 3, File3DS);
			break;

		case 0x0011:
			fread(RGB, sizeof(unsigned char), 3, File3DS);

			Color[0]=(float)RGB[0]/255.0f;
			Color[1]=(float)RGB[1]/255.0f;
			Color[2]=(float)RGB[2]/255.0f;
			break;
	}
}

void ReadPercentageChunk(Chunk3DS_t *Chunk, float *Value)
{
	short Percent;

	switch(Chunk->ID)
	{
		case 0x0030:
			fread(&Percent, sizeof(short), 1, File3DS);
			*Value=(float)Percent;
			break;

		case 0x0031:
			fread(Value, sizeof(float), 1, File3DS);
			break;
	}

	*Value/=100.0f;
}

void InspectEditObject(Chunk3DS_t *Chunk, Model3DS_t *Model)
{
	switch(Chunk->ID)
	{
		case 0x4100:
			Model->NumMesh++;
			break;
	}
}

void InspectEdit3DS(Chunk3DS_t *Chunk, Model3DS_t *Model)
{
	switch(Chunk->ID)
	{
		case 0xAFFF:
			Model->NumMaterial++;
			break;

		case 0x4000:
			ReadString(szString);
			InspectChunkArray(Chunk->Length-6+strlen(szString), (cb)InspectEditObject, Model);
			break;
	}
}

void ReadVertexList(Mesh3DS_t *Mesh)
{
	unsigned short i, NumVertex;

	fread(&NumVertex, sizeof(unsigned short), 1, File3DS);

	Mesh->NumVertex=NumVertex;
	Mesh->Vertex=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	for(i=0;i<NumVertex;i++)
	{
		float Vertex[3];

		fread(Vertex, sizeof(float), 3, File3DS);

		Mesh->Vertex[3*i+0]= Vertex[0];
		Mesh->Vertex[3*i+1]= Vertex[2];
		Mesh->Vertex[3*i+2]=-Vertex[1];
	}
}

void ReadMaterialGroup(Group3DS_t *Group)
{
	unsigned short NumFace;

	ReadString(Group->Name);

	fread(&NumFace, sizeof(unsigned short), 1, File3DS);
	Group->Face=(unsigned short *)malloc(NumFace*sizeof(unsigned short));

	if(Group->Face)
	{
		memset(Group->Face, 0, NumFace*sizeof(unsigned short));

		Group->Size=NumFace;
		Group->Material=0;

		fread(Group->Face, sizeof(unsigned short), NumFace, File3DS);
	}
}

void InspectFaceSubs(Chunk3DS_t *Chunk, Mesh3DS_t *Mesh)
{
	switch(Chunk->ID)
	{
		case 0x4130:
			Mesh->NumGroup++;
			break;
	}
}

void ReadFaceSubs(Chunk3DS_t *Chunk, Mesh3DS_t *Mesh)
{
	switch(Chunk->ID)
	{
		case 0x4130:
			ReadMaterialGroup(&Mesh->Group[Mesh->NumGroup++]);
			break;

		case 0x4150:
			Mesh->Smooth=(long *)malloc(sizeof(long)*Mesh->NumFace);

			if(Mesh->Smooth)
				fread(Mesh->Smooth, sizeof(long), Mesh->NumFace, File3DS);
			break;
	}
}

void ReadFaceList(Mesh3DS_t *Mesh, unsigned long Length)
{
	unsigned long Position=6;
	unsigned short NumFace;
	int i;

	fread(&NumFace, sizeof(unsigned short), 1, File3DS);
	Position+=sizeof(unsigned short);

	Mesh->NumFace=NumFace;
	Mesh->Face=(unsigned short *)malloc(sizeof(unsigned short)*3*Mesh->NumFace);

	for(i=0;i<NumFace;i++)
	{
		unsigned short FaceFlag;

		fread(&Mesh->Face[3*i], sizeof(unsigned short), 3, File3DS);
		fread(&FaceFlag, sizeof(unsigned short), 1, File3DS);
	}

	Position+=sizeof(unsigned short)*4*NumFace;

	if(Position<Length)
	{
		InspectChunkArray(Position-6, (cb)InspectFaceSubs, Mesh);

		if(Mesh->NumGroup)
		{
			Mesh->Group=(Group3DS_t *)malloc(sizeof(Group3DS_t)*Mesh->NumGroup);
			memset(Mesh->Group, 0, sizeof(Group3DS_t)*Mesh->NumGroup);

			if(Mesh->Group)
			{
				Mesh->NumGroup=0;
				ReadChunkArray(Position-6, (cb)ReadFaceSubs, Mesh);
			}
		}
	}
}

void ReadTextureCoordinates(Mesh3DS_t *Mesh)
{
	unsigned short NumUV;

	fread(&NumUV, sizeof(unsigned short), 1, File3DS);
	Mesh->NumUV=NumUV;

	Mesh->UV=(float *)malloc(sizeof(float)*2*Mesh->NumUV);
	fread(Mesh->UV, sizeof(float), 2*Mesh->NumUV, File3DS);
}

void ReadTriangleObject(Chunk3DS_t *Chunk, Mesh3DS_t *Mesh)
{
	switch(Chunk->ID)
	{
		case 0x4110:
			ReadVertexList(Mesh);
			break;

		case 0x4120:
			ReadFaceList(Mesh, Chunk->Length-6);
			break;

		case 0x4140:
			ReadTextureCoordinates(Mesh);
			break;
	}
}

void ReadEditObject(Chunk3DS_t *Chunk, Model3DS_t *Model)
{
	switch(Chunk->ID)
	{
		case 0x4100:
			strcpy(Model->Mesh[Model->NumMesh].Name, szString);
			ReadChunkArray(Chunk->Length-6, (cb)ReadTriangleObject, &Model->Mesh[Model->NumMesh++]);
			break;
	}
}

void ReadTexture(Chunk3DS_t *Chunk, Material3DS_t *Material)
{
	switch(Chunk->ID)
	{
		case 0xA300:
			ReadString(Material->Texture);
			break;
	}
}

void ReadMaterial(Chunk3DS_t *Chunk, Material3DS_t *Material)
{
	switch(Chunk->ID)
	{
		case 0xA000:
			ReadString(Material->Name);
			break;

		case 0xA010:
			ReadChunkArray(Chunk->Length-6, (cb)ReadColorChunk, Material->Ambient);
			Material->Ambient[3]=1.0f;
			break;

		case 0xA020:
			ReadChunkArray(Chunk->Length-6, (cb)ReadColorChunk, Material->Diffuse);
			Material->Diffuse[3]=1.0f;
			break;

		case 0xA030:
			ReadChunkArray(Chunk->Length-6, (cb)ReadColorChunk, Material->Specular);
			Material->Specular[3]=1.0f;
			break;

		case 0xA040:
			ReadChunkArray(Chunk->Length-6, (cb)ReadPercentageChunk, &Material->Shininess);
			Material->Shininess*=140.0f;
			break;

		case 0xA080:
			ReadChunkArray(Chunk->Length-6, (cb)ReadColorChunk, Material->Emission);
			Material->Emission[3]=1.0f;
			break;

		case 0xA200:
			ReadChunkArray(Chunk->Length-6, (cb)ReadTexture, Material);
			break;
	}
}

void ReadEdit3DS(Chunk3DS_t *Chunk, Model3DS_t *Model)
{
	switch(Chunk->ID)
	{
		case 0x4000:
			ReadString(szString);
			ReadChunkArray(Chunk->Length-6+strlen(szString), (cb)ReadEditObject, Model);
			break;

		case 0xAFFF:
			ReadChunkArray(Chunk->Length-6, (cb)ReadMaterial, &Model->Material[Model->NumMaterial++]);
			break;
	}
}

void ReadMain3DS(Chunk3DS_t *Chunk, Model3DS_t *Model)
{
	int i, j, k;

	switch(Chunk->ID)
	{
		case 0x3D3D:
			InspectChunkArray(Chunk->Length-6, (cb)InspectEdit3DS, Model);

			if(Model->NumMesh)
			{
				Model->Mesh=(Mesh3DS_t *)malloc(sizeof(Mesh3DS_t)*Model->NumMesh);
				memset(Model->Mesh, 0, sizeof(Mesh3DS_t)*Model->NumMesh);
			}

			if(Model->NumMaterial)
			{
				Model->Material=(Material3DS_t *)malloc(sizeof(Material3DS_t)*Model->NumMaterial);
				memset(Model->Material, 0, sizeof(Material3DS_t)*Model->NumMaterial);
			}

			Model->NumMesh=0;
			Model->NumMaterial=0;

			ReadChunkArray(Chunk->Length-6, (cb)ReadEdit3DS, Model);

			for(i=0;i<Model->NumMesh;i++)
			{
				for(j=0;j<Model->Mesh[i].NumGroup;j++)
				{
					for(k=0;k<Model->NumMaterial;k++)
					{
						if(!strcmp(Model->Mesh[i].Group[j].Name, Model->Material[k].Name))
						{
							Model->Mesh[i].Group[j].Material=k;
							break;
						}
					}
				}
			}
			break;

		default:
			break;
	}
}

int Load3DS(Model3DS_t *Model, char *Filename)
{
	int i, j;
	Chunk3DS_t Chunk;

	memset(Model, 0, sizeof(Model3DS_t));

	if((File3DS=fopen(Filename, "rb"))==NULL)
		return 0;

	fread(&Chunk, sizeof(Chunk3DS_t), 1, File3DS);

	if(Chunk.ID==0x4D4D)
		ReadChunkArray(Chunk.Length-6, (cb)ReadMain3DS, Model);

	fclose(File3DS);

	for(i=0;i<Model->NumMesh;i++)
	{
		CalculateTangent(&Model->Mesh[i]);

		Model->Mesh[i].Minimum[0]=Model->Mesh[i].Maximum[0]=Model->Mesh[i].Vertex[0];
		Model->Mesh[i].Minimum[1]=Model->Mesh[i].Maximum[1]=Model->Mesh[i].Vertex[1];
		Model->Mesh[i].Minimum[2]=Model->Mesh[i].Maximum[2]=Model->Mesh[i].Vertex[2];

		for(j=0;j<Model->Mesh[i].NumVertex*3;j+=3)
		{
			if(Model->Mesh[i].Vertex[j]<Model->Mesh[i].Minimum[0])
				Model->Mesh[i].Minimum[0]=Model->Mesh[i].Vertex[j];

			if(Model->Mesh[i].Vertex[j+1]<Model->Mesh[i].Minimum[1])
				Model->Mesh[i].Minimum[1]=Model->Mesh[i].Vertex[j+1];

			if(Model->Mesh[i].Vertex[j+2]<Model->Mesh[i].Minimum[2])
				Model->Mesh[i].Minimum[2]=Model->Mesh[i].Vertex[j+2];

			if(Model->Mesh[i].Vertex[j]>Model->Mesh[i].Maximum[0])
				Model->Mesh[i].Maximum[0]=Model->Mesh[i].Vertex[j];

			if(Model->Mesh[i].Vertex[j+1]>Model->Mesh[i].Maximum[1])
				Model->Mesh[i].Maximum[1]=Model->Mesh[i].Vertex[j+1];

			if(Model->Mesh[i].Vertex[j+2]>Model->Mesh[i].Maximum[2])
				Model->Mesh[i].Maximum[2]=Model->Mesh[i].Vertex[j+2];
		}

		Model->Mesh[i].Center[0]=(Model->Mesh[i].Minimum[0]+Model->Mesh[i].Maximum[0])*0.5f;
		Model->Mesh[i].Center[1]=(Model->Mesh[i].Minimum[1]+Model->Mesh[i].Maximum[1])*0.5f;
		Model->Mesh[i].Center[2]=(Model->Mesh[i].Minimum[2]+Model->Mesh[i].Maximum[2])*0.5f;
	}

	return 1;
}

void Free3DS(Model3DS_t *Model)
{
	int i, j;

	if(Model->Mesh)
	{
		for(i=0;i<Model->NumMesh;i++)
		{
			FREE(Model->Mesh[i].Vertex);
			FREE(Model->Mesh[i].Face);
			FREE(Model->Mesh[i].Tangent);
			FREE(Model->Mesh[i].Binormal);
			FREE(Model->Mesh[i].Normal);
			FREE(Model->Mesh[i].UV);
			FREE(Model->Mesh[i].Smooth);

			if(Model->Mesh[i].Group)
			{
				for(j=0;j<Model->Mesh[i].NumGroup;j++)
					FREE(Model->Mesh[i].Group[j].Face);

				FREE(Model->Mesh[i].Group);
			}
		}

		FREE(Model->Mesh);
	}

	FREE(Model->Material);
}
