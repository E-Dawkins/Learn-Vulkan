#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform
{
private:
	glm::vec3 mPosition{ 0, 0, 0 };
	glm::quat mRotation{ glm::vec3{ 0, 0, 0 } };
	glm::vec3 mScale{ 1, 1, 1 };

	// TODO - remove cached matrix as it costs 64 bytes
	// of extra memory for every transform instance
	
	// Note to self: 'mutable' is so we can manipulate
	// these in const context within 'GetMatrix'
	mutable glm::mat4 mMatrix;
	mutable bool mDirty = true;

public:
	const glm::mat4& GetMatrix() const;

	inline const glm::vec3& GetPosition() const { return mPosition; }
	inline const glm::quat& GetRotation() const { return mRotation; }
	inline const glm::vec3& GetScale() const { return mScale; }

	void SetPosition(const glm::vec3& _p);
	void SetRotation(const glm::quat& _r); // Normalizes '_r' before setting rotation
	void SetScale(const glm::vec3& _s);

	void AddRotation(const glm::quat& _deltaRotation); // Shorthand for 'SetRotation(deltaRotation * GetRotation())'
};