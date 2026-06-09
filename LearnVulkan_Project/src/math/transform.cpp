#include "pch.h"
#include "math/transform.h"

const glm::mat4& Transform::GetMatrix() const {
	if (mDirty) {
		// Standard T*R*S transformation matrix
		mMatrix = glm::translate(glm::mat4(1), mPosition)
				* glm::toMat4(mRotation)
				* glm::scale(glm::mat4(1), mScale);
		mDirty = false;
	}

	return mMatrix;
}

void Transform::SetPosition(const glm::vec3& _p) {
	mPosition = _p;
	mDirty = true;
}

void Transform::SetRotation(const glm::quat& _r) {
	mRotation = glm::normalize(_r);
	mDirty = true;
}

void Transform::SetScale(const glm::vec3& _s) {
	mScale = _s;
	mDirty = true;
}

void Transform::AddRotation(const glm::quat& _deltaRotation) {
	SetRotation(_deltaRotation * mRotation);
}
