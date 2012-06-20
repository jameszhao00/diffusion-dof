#pragma once
#include <fstream>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using namespace std;
//typedef glm::mat4x4 float4x4;
//typedef glm::vec4 float4;
//typedef glm::vec3 float3;
//typedef glm::quat quat;
using namespace glm;
//typedef glm::vec2 float2;

inline void writeFloat4x4(ofstream& ofs, const float4x4& f4x4)
{
	for(int i = 0; i < 4; i++)
	{
		for(int j = 0; j < 4; j++)
		{
			ofs << f4x4[i][j] << " ";
		}
	}
}

inline void readFloat4x4(ifstream& ofs, float4x4* f4x4)
{
	for(int i = 0; i < 4; i++)
	{
		for(int j = 0; j < 4; j++)
		{
			ofs >> (*f4x4)[i][j];
		}
	}
}