#pragma once
#include "lwmath.h"
struct Camera
{
	Camera() 
		: position(0, 5, 15), orientation(float3(0, glm::radians(180.f), 0))
		, f(1), n(600), fovy(3.1415f/4.f) { }
	float3 position;
	quat orientation;
	float fovy;
	float f;
	float n;
	float ar;
	float3 forward() const
	{
		return glm::rotate(orientation, float3(0, 0, 1));
	}
	float3 left() const
	{
		return glm::rotate(orientation, float3(-1, 0, 0));
	}
	float3 up() const
	{
		return glm::rotate(orientation, float3(0, 1, 0));
	}

	float4x4 viewMatrix() const;
	float4x4 projectionMatrix() const;
	float2 Camera::projectionConstants() const;

	void move(float tForward, float tLeft);
	void turn(float dx, float dy);
};

