#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

constexpr glm::vec3 gWorldRight = { 1, 0, 0 };
constexpr glm::vec3 gWorldForward = { 0, 1, 0 };
constexpr glm::vec3 gWorldUp = { 0, 0, 1 };

struct Transform
{
private:
	glm::vec3 mPosition{ 0, 0, 0 };
	glm::quat mRotation{ glm::vec3{ 0, 0, 0 } };
	glm::vec3 mScale{ 1, 1, 1 };

	glm::vec3 mRight{ 1, 0, 0 };
	glm::vec3 mForward{ 0, 1, 0 };
	glm::vec3 mUp{ 0, 0, 1 };

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
	inline const glm::vec3& GetRightVector() const { return mRight; }
	inline const glm::vec3& GetForwardVector() const { return mForward; }
	inline const glm::vec3& GetUpVector() const { return mUp; }

	void SetPosition(const glm::vec3& _p);
	// Normalizes '_r' before setting rotation
	void SetRotation(const glm::quat& _r);
	void SetScale(const glm::vec3& _s);

	// Shorthand for 'SetPosition(GetPosition() + deltaPosition)'
	void AddPosition(const glm::vec3& _deltaPosition);

	// Shorthand for 'SetRotation(deltaRotation * GetRotation())'
	void AddRotation(const glm::quat& _deltaRotation);
	
	// Set rotation to 'look at' _targetPos, optionally zeroing roll
	void LookAt(const glm::vec3& _targetPos, bool _zeroRoll = false);

private:
	void RecalculateAxisVectors();
};