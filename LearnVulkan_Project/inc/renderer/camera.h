#pragma once
#include "math/transform.h"

constexpr float gMinClipDist = 0.01f;

// This converts some mat/vec from world space
// to camera view space, i.e. mat * gEngineToView
constexpr glm::mat4 gEngineToView(
	1, 0, 0, 0,
	0, 0, -1, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
);

struct CameraData
{
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

class Camera
{
public:
	Transform transform;

private:
	CameraData mCachedGraphicsData{};
	bool mIsDirtyProjMatrix = true;

	float mCachedAspectRatio = 1.f;
	bool mIsDirtyAspectRatio = true;

	float mWidth = 1920.f;
	float mHeight = 1080.f;
	float mFovDegrees = 45.f;
	float mNearClip = 0.1f;
	float mFarClip = 100.f;

public:
	Camera() = default;
	Camera(float _fov, float _nearClip, float _farClip);

	virtual void Update(float /*_deltaTime*/) {}

	const CameraData& GetGraphicsData();

	const float GetAspectRatio();
	inline const float GetWidth() const { return mWidth; }
	inline const float GetHeight() const { return mHeight; }
	inline const float GetFovDegrees() const { return mFovDegrees; }
	inline const float GetNearClip() const { return mNearClip; }
	inline const float GetFarClip() const { return mFarClip; }

	void SetWidth(float _width);
	void SetHeight(float _height);
	// Shorthand for SetWidth(...) & SetHeight(...) combo
	void SetViewSize(const glm::vec2& _sizeXY);
	// Clamped to [10-170]
	void SetFovDegrees(float _fovDegrees);
	// Clamped to [gMinClipDist, farClip - gMinClipDist]
	void SetNearClip(float _nearClip);
	// Clamped to [nearClip + gMinClipDist, ...]
	void SetFarClip(float _farClip);

private:
	void RecalculateViewMatrix();
};

class FlyCamera : public Camera
{
private:
	glm::vec3 mMoveInput{};
	glm::vec2 mLookInput{};

public:
	float flySpeed = 1.f;
	float lookSpeed = 1.f;

public:
	FlyCamera() = default;
	FlyCamera(float _fov, float _nearClip, float _farClip, float _flySpeed, float _lookSpeed);

	void Update(float _deltaTime) override;

	void AddMoveInput(float _scale, glm::vec3 _worldDirection);
	void AddYawInput(float _scale);
	void AddPitchInput(float _scale);
};