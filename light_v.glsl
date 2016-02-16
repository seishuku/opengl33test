#version 330

layout(location=0) in vec3 vPosition;
layout(location=1) in vec2 vUV;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec3 vBinormal;
layout(location=4) in vec3 vNormal;

uniform mat4 mvp;

out vec3 Position;
out vec2 UV;
out vec3 TangentX;
out vec3 TangentY;
out vec3 TangentZ;
out vec3 Eye;

void main()
{
	gl_Position=mvp*vec4(vPosition, 1.0);

	Position=vPosition;
	UV=vUV;

	TangentX.x=vTangent.x;
	TangentX.y=vBinormal.x;
	TangentX.z=vNormal.x;

	TangentY.x=vTangent.y;
	TangentY.y=vBinormal.y;
	TangentY.z=vNormal.y;

	TangentZ.x=vTangent.z;
	TangentZ.y=vBinormal.z;
	TangentZ.z=vNormal.z;
}
