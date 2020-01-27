#pragma once

#include <DirectXMath.h>

// When performing transformations on the camera, 
// it is sometimes useful to express which space this 
// transformation should be applied.
enum class Space
{
	kLocal,
	kWorld,
};

class Camera
{
public:
	/**
	* Create the application singleton with the application instance handle.
	*/
	static void Create();

	/**
	* Destroy the application instance and all windows created by this application instance.
	*/
	static void Destroy();
	/**
	* Get the application singleton.
	*/
	static Camera& Get();

	void XM_CALLCONV SetLookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up) const;
	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetInverseViewMatrix() const;

	/**
	 * Set the camera to a perspective projection matrix.
	 * @param fovy The vertical field of view in degrees.
	 * @param aspect The aspect ratio of the screen.
	 * @param z_near The distance to the near clipping plane.
	 * @param z_far The distance to the far clipping plane.
	 */
	void SetProjection(float fovy, float aspect, float z_near, float z_far);
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetInverseProjectionMatrix() const;

	/**
	 * Set the field of view in degrees.
	 */
	void SetFoV(float fovy);

	/**
	 * Get the field of view in degrees.
	 */
	float GetFoV() const;

	/**
	* Get the near clip of the projection frustum.
	*/
	float GetNearClip() const { return near_clip_; };

	/**
	* Get the near clip of the projection frustum.
	*/
	float GetFarClip() const { return far_clip_; };

	/**
	 * Set the camera's position in world-space.
	 */
	void XM_CALLCONV SetTranslation(DirectX::FXMVECTOR translation) const;
	DirectX::XMVECTOR GetTranslation() const;

	/**
	 * Set the camera's rotation in world-space.
	 * @param rotation The rotation quaternion.
	 */
	void XM_CALLCONV SetRotation(DirectX::FXMVECTOR rotation) const;
	/**
	 * Query the camera's rotation.
	 * @returns The camera's rotation quaternion.
	 */
	DirectX::XMVECTOR GetRotation() const;

	void XM_CALLCONV Translate(DirectX::FXMVECTOR translation, Space space = Space::kLocal) const;
	void Rotate(DirectX::FXMVECTOR quaternion) const;

protected:
	Camera();
	virtual ~Camera();

	virtual void UpdateViewMatrix() const;
	virtual void UpdateInverseViewMatrix() const;
	virtual void UpdateProjectionMatrix() const;
	virtual void UpdateInverseProjectionMatrix() const;

	// This data must be aligned otherwise the SSE intrinsics fail
	// and throw exceptions.
	__declspec(align(16)) struct AlignedData
	{
		// World-space position of the camera.
		DirectX::XMVECTOR Translation;
		// World-space rotation of the camera.
		// THIS IS A QUATERNION!!!!
		DirectX::XMVECTOR Rotation;

		DirectX::XMMATRIX ViewMatrix, InverseViewMatrix;
		DirectX::XMMATRIX ProjectionMatrix, InverseProjectionMatrix;
	};

	AlignedData* p_data_;

	// projection parameters
	float vFoV_; // Vertical field of view.
	float aspect_ratio_; // Aspect ratio
	float near_clip_; // Near clip distance
	float far_clip_; // Far clip distance.

	// True if the view matrix needs to be updated.
	mutable bool view_dirty_, inverse_view_dirty_;
	// True if the projection matrix needs to be updated.
	mutable bool projection_dirty_, inverse_projection_dirty_;
};
