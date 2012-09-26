#pragma once
#include <map>
#include <vector>
#include <memory>
using namespace std;
#include "lwmath.h"
#include "package.h"
typedef int JointId;
struct Joint
{
	void deserialize(Deserializer& deserializer)
	{
		id = deserializer.read<int>();
		int numChildren = deserializer.read<int>();
		for(int i = 0; i < numChildren; i++)
		{
			unique_ptr<Joint> child(new Joint());
			child->deserialize(deserializer);
			children.push_back(move(child));
		}
	}
	void serialize(Serializer& serializer) const
	{
		serializer.write(id);
		serializer.write(children.size());
		for(const auto& child : children)
		{
			child->serialize(serializer);
		}
	}

	JointId id;
	std::vector<unique_ptr<Joint>> children;
};
struct JointAnimationData
{
	void deserialize(Deserializer& deserializer)
	{
		jointId = deserializer.read<int>();
		deserializer.readVec(data);
	}
	void serialize(Serializer& serializer) const
	{
		serializer.write(jointId);
		serializer.writeVec(data);
	}
	JointId jointId;
	std::vector<float4x4> data;
	float4x4 xformAt(float frameIndex) const;
};
struct JointAnimation
{
	JointAnimation();
	void deserialize(Deserializer& deserializer)
	{
		fps = deserializer.read<int>();
		numFrames = deserializer.read<int>();
		int numRootJoints = deserializer.read<int>();
		for(int i = 0; i < numRootJoints; i++)
		{
			unique_ptr<Joint> curRootJoint(new Joint());
			curRootJoint->deserialize(deserializer);
			rootJoints.push_back(move(curRootJoint));
		}
		int numAnimationData = deserializer.read<int>();
		for(int i = 0; i < numAnimationData; i++)
		{
			int jointId = deserializer.read<int>();
			unique_ptr<JointAnimationData> curAnimData(new JointAnimationData());
			curAnimData->deserialize(deserializer);
			animationData[jointId] = move(curAnimData);
		}
	}
	void serialize(Serializer& serializer) const
	{
		serializer.write(fps);
		serializer.write(numFrames);
		serializer.write(rootJoints.size());
		for(const auto& joint : rootJoints)
		{
			joint->serialize(serializer);
		}
		serializer.write(animationData.size());
		for(const auto& kv : animationData)
		{
			serializer.write(kv.first);
			kv.second->serialize(serializer);
		}
	}

	std::map<JointId, unique_ptr<JointAnimationData>> animationData;
	std::vector<unique_ptr<Joint>> rootJoints;

	int fps;
	int numFrames;

	float durationInSeconds() const;
	void xformsAt(float t, float4x4* data) const;
private:
	void xformsAtImpl(const Joint& joint, 
							 float t,
							 const float4x4& parentXform,
							 float4x4* data) const;
};
