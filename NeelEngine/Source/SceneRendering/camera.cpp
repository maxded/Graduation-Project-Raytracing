#include "neel_engine_pch.h"

#include "camera.h"

using namespace DirectX;

static Camera* p_camera = nullptr;

Camera::Camera()
	: vFoV_(45.0f)
	  , aspect_ratio_(1.0f)
	  , near_clip_(0.1f)
	  , far_clip_(100.0f)
	  , view_dirty_(true)
	  , inverse_view_dirty_(true)
	  , projection_dirty_(true)
	  , inverse_projection_dirty_(true)
{
	p_data_ = static_cast<AlignedData*>(_aligned_malloc(sizeof(AlignedData), 16));
	p_data_->Translation = XMVectorZero();
	p_data_->Rotation = XMQuaternionIdentity();
}

Camera::~Camera()
{
	_aligned_free(p_data_);
}

void Camera::Create()
{
	if (!p_camera)
	{
		p_camera = new Camera();
	}
}

void Camera::Destroy()
{
	if (p_camera)
	{
		delete p_camera;
		p_camera = nullptr;
	}
}

Camera& Camera::Get()
{
	assert(p_camera);
	return *p_camera;
}

void XM_CALLCONV Camera::SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up) const
{
	p_data_->ViewMatrix = XMMatrixLookAtLH(eye, target, up);

	p_data_->Translation = eye;
	p_data_->Rotation = XMQuaternionRotationMatrix(XMMatrixTranspose(p_data_->ViewMatrix));

	inverse_view_dirty_ = true;
	view_dirty_ = false;
}

XMMATRIX Camera::GetViewMatrix() const
{
	if (view_dirty_)
	{
		UpdateViewMatrix();
	}
	return p_data_->ViewMatrix;
}

XMMATRIX Camera::GetInverseViewMatrix() const
{
	if (inverse_view_dirty_)
	{
		p_data_->InverseViewMatrix = XMMatrixInverse(nullptr, p_data_->ViewMatrix);
		inverse_view_dirty_ = false;
	}

	return p_data_->InverseViewMatrix;
}

void Camera::SetProjection(float fovy, float aspect, float z_near, float z_far)
{
	vFoV_ = fovy;
	aspect_ratio_ = aspect;
	near_clip_ = z_near;
	far_clip_ = z_far;

	projection_dirty_ = true;
	inverse_projection_dirty_ = true;
}

XMMATRIX Camera::GetProjectionMatrix() const
{
	if (projection_dirty_)
	{
		UpdateProjectionMatrix();
	}

	return p_data_->ProjectionMatrix;
}

XMMATRIX Camera::GetInverseProjectionMatrix() const
{
	if (inverse_projection_dirty_)
	{
		UpdateInverseProjectionMatrix();
	}

	return p_data_->InverseProjectionMatrix;
}

void Camera::SetFoV(float fovy)
{
	if (vFoV_ != fovy)
	{
		vFoV_ = fovy;
		projection_dirty_ = true;
		inverse_projection_dirty_ = true;
	}
}

float Camera::GetFoV() const
{
	return vFoV_;
}


void XM_CALLCONV Camera::SetTranslation(FXMVECTOR translation) const
{
	p_data_->Translation = translation;
	view_dirty_ = true;
}

XMVECTOR Camera::GetTranslation() const
{
	return p_data_->Translation;
}

void Camera::SetRotation(FXMVECTOR rotation) const
{
	p_data_->Rotation = rotation;
}

XMVECTOR Camera::GetRotation() const
{
	return p_data_->Rotation;
}

void XM_CALLCONV Camera::Translate(FXMVECTOR translation, Space space) const
{
	switch (space)
	{
		case Space::kLocal:
			{
				p_data_->Translation += XMVector3Rotate(translation, p_data_->Rotation);
			}
			break;
		case Space::kWorld:
			{
				p_data_->Translation += translation;
			}
			break;
	}

	p_data_->Translation = XMVectorSetW(p_data_->Translation, 1.0f);

	view_dirty_ = true;
	inverse_view_dirty_ = true;
}

void Camera::Rotate(FXMVECTOR quaternion) const
{
	p_data_->Rotation = XMQuaternionMultiply(p_data_->Rotation, quaternion);

	view_dirty_ = true;
	inverse_view_dirty_ = true;
}

void Camera::UpdateViewMatrix() const
{
	XMMATRIX rotation_matrix = XMMatrixTranspose(XMMatrixRotationQuaternion(p_data_->Rotation));
	XMMATRIX translation_matrix = XMMatrixTranslationFromVector(-(p_data_->Translation));

	p_data_->ViewMatrix = translation_matrix * rotation_matrix;

	inverse_view_dirty_ = true;
	view_dirty_ = false;
}

void Camera::UpdateInverseViewMatrix() const
{
	if (view_dirty_)
	{
		UpdateViewMatrix();
	}

	p_data_->InverseViewMatrix = XMMatrixInverse(nullptr, p_data_->ViewMatrix);
	inverse_view_dirty_ = false;
}

void Camera::UpdateProjectionMatrix() const
{
	p_data_->ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(vFoV_), aspect_ratio_, near_clip_,
	                                                     far_clip_);

	projection_dirty_ = false;
	inverse_projection_dirty_ = true;
}

void Camera::UpdateInverseProjectionMatrix() const
{
	if (projection_dirty_)
	{
		UpdateProjectionMatrix();
	}

	p_data_->InverseProjectionMatrix = XMMatrixInverse(nullptr, p_data_->ProjectionMatrix);
	inverse_projection_dirty_ = false;
}
