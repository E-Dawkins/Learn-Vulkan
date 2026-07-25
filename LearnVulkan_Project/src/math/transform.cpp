#include "pch.h"
#include "math/transform.h"

void Transform::SetPosition(const glm::vec3& _p) {
	mPosition = _p;
	mDirty = true;
}

void Transform::SetRotation(const glm::quat& _r) {
	mRotation = glm::normalize(_r);
	mDirty = true;

	RecalculateAxisVectors();
}

void Transform::SetScale(const glm::vec3& _s) {
	mScale = _s;
	mDirty = true;
}

void Transform::AddPosition(const glm::vec3& _deltaPosition) {
	SetPosition(mPosition + _deltaPosition);
}

void Transform::AddRotation(const glm::quat& _deltaRotation) {
	SetRotation(_deltaRotation * mRotation);
}

void Transform::LookAt(const glm::vec3& _targetPos, bool _zeroRoll) {
	const glm::vec3 forward = glm::normalize(_targetPos - mPosition);

	// Zero roll requires up to be world up, so just default to it
	glm::vec3 upRef = gWorldUp;
	if (!_zeroRoll) {
		// Set 'upRef' to axis that is most perpendicular to 'forward'
		float dR = std::abs(glm::dot(forward, gWorldRight));
		float dF = std::abs(glm::dot(forward, gWorldForward));
		float dU = std::abs(glm::dot(forward, gWorldUp));

		if (dR < dF && dR < dU) {
			upRef = gWorldRight;
		}
		else if (dF < dU) {
			upRef = gWorldForward;
		}
	}

	const glm::vec3 right = glm::normalize(glm::cross(forward, upRef));
	const glm::vec3 upActual = glm::cross(right, forward);

	// This must be defined using axis order: X, Y, Z
	const glm::mat3 rot(right, forward, upActual);

	SetRotation(glm::quat_cast(rot));
}

void Transform::RecalculateAxisVectors() {
	// No need to normalize mRotation, as 'SetRotation' does that for us
	glm::mat3 m = glm::toMat3(mRotation);
	mRight =	m[0]; // X
	mForward =	m[1]; // Y
	mUp =		m[2]; // Z
}

void RenderTransform::SetMappedMatrix(glm::mat4& _matrixMapping) {
	delete mMappedMatrix;
	mMappedMatrix = &_matrixMapping;
}

void RenderTransform::UpdateMappedMatrix() {
	if (!mMappedMatrix || !mDirty) {
		return;
	}

	// Grab matrix from gpu mapped pointer
	glm::mat4& m = *static_cast<glm::mat4*>(mMappedMatrix);

	// Standard T*R*S transformation matrix
	m = glm::translate(glm::mat4(1), mPosition)
		* glm::toMat4(mRotation)
		* glm::scale(glm::mat4(1), mScale);

	mDirty = false;
}
