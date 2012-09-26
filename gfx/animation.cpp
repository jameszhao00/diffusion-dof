#include "stdafx.h"
#include "animation.h"
/*
float4x4 JointAnimationData::xformAt(float frameIndex) const
{
	assert(frameIndex >= 0);
	assert(frameIndex < this->data.size() - 1); //if we hit the end, we should've wrapped to 0
	int frameIndices[2] = {floor(frameIndex), ceil(frameIndex)};
	
	if(frameIndices[0] == frameIndices[1])
	{
		return data[frameIndex];
	}
	else
	{
		float t = frameIndex - frameIndices[0];
		assert(t >= 0 && t <= 1);
		float4x4 result;
		XMStoreFloat4x4(
			&result,
			lerp(data[frameIndices[0]], data[frameIndices[1]], t));
		return result;		
	}
	
	return data[frameIndex];
}
float JointAnimation::durationInSeconds() const
{
	//scenarios
	//2 fps, 2 frames, duration = 0.5 seconds
	//2 fps, 3 frames, duration = 1 seconds
	//we don't go past the last frame
	return (float)(numFrames - 1) / (float)fps;
}
float timeToFrameIndex(float t, int numFrames, float duration)
{
	//scenarios (2 fps)
	//2 frames, t = 1.25, duration = 0.5, frameIndex = 0.5
	//2 frames, t = 1, duration = 0.5, frameIndex = 0 (starts over at frame 0)
	//3 frames, t = 1, duration = 1, frameIndex = 0 (starts over at frame 0)
	//4 frames, t = 1, duration = 1.5, frameIndex = 2 (left over)
	float frameIndex = fmod(t, duration) * numFrames;
	assert(frameIndex < numFrames - 1); //if we hit numFrames - 1, it should wrap to 0
	assert(frameIndex >= 0);
	return frameIndex;
}
//assumes the animation is baked... i.e. every frame is a keyframe
void JointAnimation::xformsAtImpl(const Joint& joint, 
							 float t,
							 const float4x4& parentXform,
							 float4x4* data) const
{
	//my xform
	float4x4 localXform = animationData.at(joint.id)->xformAt(timeToFrameIndex(t, numFrames, fps));
	float4x4 compositeXform;
	XMStoreFloat4x4(&compositeXform, XMLoadFloat4x4(&localXform) * XMLoadFloat4x4(&parentXform));
	//child joint xforms
	for(const auto& childJoint : joint.children)
	{
		xformsAtImpl(*childJoint, t, compositeXform, data);
	}
}
void JointAnimation::xformsAt(float t, float4x4* data) const
{
	float4x4 identity;
	XMStoreFloat4x4(&identity, XMMatrixIdentity());
	for(const auto& joint : rootJoints)
	{
		xformsAtImpl(*joint, t, identity, data);
	}
}
*/