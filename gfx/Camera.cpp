#include "stdafx.h"
#include "Camera.h"
#include <xnamath.h>
float4x4 Camera::viewMatrix() const 
{ 
	float4x4 ret;
	XMStoreFloat4x4((XMFLOAT4X4*)glm::value_ptr(ret), 
		XMMatrixTranspose(
			XMMatrixLookAtLH(
				XMLoadFloat3((XMFLOAT3*)glm::value_ptr(position)), 
				XMLoadFloat3((XMFLOAT3*)glm::value_ptr(position + forward())), 
				XMLoadFloat3((XMFLOAT3*)glm::value_ptr(up()))
				)
			)
	);
	return ret;
}
float4x4 Camera::projectionMatrix() const
{ 	
	float4x4 ret;
	XMStoreFloat4x4((XMFLOAT4X4*)glm::value_ptr(ret), 
		XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovy, ar, n, f)));
	return ret;
}
float2 Camera::projectionConstants() const
{
	return float2(
		f / (f - n), 
		(-f * n) / (f - n)
		);
}
	
void Camera::move(float tForward, float tLeft) 
{ 
	position += forward() * tForward + left() * tLeft;
}
void Camera::turn(float dx, float dy) 
{ 
	orientation = glm::toQuat(glm::eulerAngleXY(dx, dy)) * orientation;
}