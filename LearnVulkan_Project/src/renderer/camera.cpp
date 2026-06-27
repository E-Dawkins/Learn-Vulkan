#include "pch.h"
#include "renderer/camera.h"

Camera::Camera(float _fovDegrees, float _nearClip, float _farClip) {
	// Call these instead of direct set, as they clamp their inputs
	SetFovDegrees(_fovDegrees);
	SetNearClip(_nearClip);
	SetFarClip(_farClip);
}

const CameraData& Camera::GetGraphicsData() {
	if (mIsDirtyProjMatrix) {
		mCachedGraphicsData.proj = glm::perspective(
			glm::radians(mFovDegrees),
			GetAspectRatio(),
			mNearClip,
			mFarClip
		);

		// Account for OpenGL flipped y coordinate, if you
		// don't do this, the image is rendered upside down
		mCachedGraphicsData.proj[1][1] *= -1.f;

		mIsDirtyProjMatrix = false;
	}

	// TODO: find a way to only update this if the transform is modified
	RecalculateViewMatrix();

	return mCachedGraphicsData;
}

inline const float Camera::GetAspectRatio() {
	if (mIsDirtyAspectRatio) {
		mCachedAspectRatio = mWidth / mHeight;
		mIsDirtyAspectRatio = false;
	}

	return mCachedAspectRatio;
}

void Camera::SetWidth(float _width) {
	mWidth = _width;
	mIsDirtyAspectRatio = true;
	mIsDirtyProjMatrix = true;
}

void Camera::SetHeight(float _height) {
	mHeight = _height;
	mIsDirtyAspectRatio = true;
	mIsDirtyProjMatrix = true;
}

void Camera::SetViewSize(const glm::vec2& _sizeXY) {
	SetWidth(_sizeXY.x);
	SetHeight(_sizeXY.y);
}

void Camera::SetFovDegrees(float _fovDegrees) {
	mFovDegrees = std::clamp(_fovDegrees, 10.f, 170.f);
	mIsDirtyProjMatrix = true;
}

void Camera::SetNearClip(float _nearClip) {
	mNearClip = std::clamp(_nearClip, gMinClipDist, mFarClip - gMinClipDist);
	mIsDirtyProjMatrix = true;
}

void Camera::SetFarClip(float _farClip) {
	mFarClip = std::max(_farClip, mNearClip + gMinClipDist);
	mIsDirtyProjMatrix = true;
}

void Camera::RecalculateViewMatrix() {
	// Construct view matrix directly from our direction vectors
	// as the standard 'glm::inverse(transform)' method only works
	// if our world axes are what GLM expects: X/Y/Z = right/up/forward

	glm::mat4& v = mCachedGraphicsData.view;
	Transform& t = transform;
	
	v = glm::mat4(1.f);

	// Cam right in world space
	v[0][0] = t.GetRightVector().x;
	v[1][0] = t.GetRightVector().y;
	v[2][0] = t.GetRightVector().z;

	// Cam forward in world space
	v[0][1] = t.GetForwardVector().x;
	v[1][1] = t.GetForwardVector().y;
	v[2][1] = t.GetForwardVector().z;

	// Cam up in world space
	v[0][2] = t.GetUpVector().x;
	v[1][2] = t.GetUpVector().y;
	v[2][2] = t.GetUpVector().z;

	// How far along each local axis is the camera?
	// i.e. 5 units along local right will result in v[3][0] = -5
	v[3][0] = -glm::dot(t.GetRightVector(), t.GetPosition());
	v[3][1] = -glm::dot(t.GetForwardVector(), t.GetPosition());
	v[3][2] = -glm::dot(t.GetUpVector(), t.GetPosition());

	// Convert view matrix to Vulkan view space
	v = gEngineToView * v;
}

FlyCamera::FlyCamera(float _fov, float _nearClip, float _farClip, float _flySpeed, float _lookSpeed)
	: Camera(_fov, _nearClip, _farClip), flySpeed(_flySpeed), lookSpeed(_lookSpeed) {}

void FlyCamera::Update(float _deltaTime) {
	// Do we have any move input?
	if (mMoveInput != glm::zero<glm::vec3>()) {
		mMoveInput *= (flySpeed * _deltaTime);

		transform.AddPosition(mMoveInput);
		
		mMoveInput = glm::zero<glm::vec3>();
	}

	// Do we have any look input?
	if (mLookInput != glm::zero<glm::vec2>()) {
		// x => yaw, y => pitch
		mLookInput *= (lookSpeed * _deltaTime);

		// We use world up, as local up will apply some unwanted roll
		transform.AddRotation(
			glm::angleAxis(mLookInput.x, gWorldUp)
			* glm::angleAxis(mLookInput.y, transform.GetRightVector())
		);

		mLookInput = glm::zero<glm::vec2>();
	}
}

void FlyCamera::AddMoveInput(float _scale, glm::vec3 _worldDirection) {
	if (_scale == 0.f || _worldDirection == glm::zero<glm::vec3>()) {
		// Can't move by 0 units
		return;
	}

	mMoveInput += glm::normalize(_worldDirection) * _scale;
}

void FlyCamera::AddLookInput(float _scale, glm::vec2 _mouseDirection) {
	if (_scale == 0.f || _mouseDirection == glm::zero<glm::vec2>()) {
		// Can't look by 0 units
		return;
	}

	mLookInput += glm::normalize(_mouseDirection) * _scale;
}
