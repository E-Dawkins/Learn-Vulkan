#include "pch.h"
#include "math/transform.h"

void Transform::SetPosition(const glm::vec3& _p) {
	// Note to self - std::exchange does the following but in 1 line:
	//		vec3 old = mPosition;
	//		mPosition = _p;
	const glm::vec3 old = std::exchange(mPosition, _p);

	if (onPositionChanged) {
		onPositionChanged(old, mPosition);
	}
}

void Transform::SetRotation(const glm::quat& _r) {
	const glm::quat normR = glm::normalize(_r);
	const glm::quat old = std::exchange(mRotation, normR);

	RecalculateAxisVectors();

	if (onRotationChanged) {
		onRotationChanged(old, mRotation);
	}
}

void Transform::SetScale(const glm::vec3& _s) {
	const glm::vec3 old = std::exchange(mScale, _s);

	if (onScaleChanged) {
		onScaleChanged(old, mScale);
	}
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

void RenderTransform::SetPosition(const glm::vec3& _p) {
	Transform::SetPosition(_p);

	mDirty = true;
}

void RenderTransform::SetRotation(const glm::quat& _r) {
	Transform::SetRotation(_r);

	mDirty = true;
}

void RenderTransform::SetScale(const glm::vec3& _s) {
	Transform::SetScale(_s);

	mDirty = true;
}
