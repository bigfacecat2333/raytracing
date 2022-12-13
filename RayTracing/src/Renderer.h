#pragma once
#include "Walnut/Image.h"
#include <memory>
#include <glm/glm.hpp>

#include "Camera.h"
#include "Ray.h"
#include "Scene.h"

class Renderer
{
public:
	struct Settings
	{
		// �ۼ�����
		bool Accumulate = true;
	};
	
public:
	Renderer() = default;
	
	void OnResize(uint32_t width, uint32_t height);

	void Render(const Scene& scene, const Camera& camera);

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }
	
	void ResetFrameIndex() { m_FrameIndex = 1; }
	Settings& GetSettings() { return m_Settings; }

private:
	// Payload��ʾ���ȥ�Ĺ��߷�����ʲô:�Ƿ�������壬�Լ������������ź�λ���Լ�����
	struct HitPayload
	{
		float HitDistance;
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;

		int ObjectIndex;
	};
	// ����shadertoy�ĺ��� ��������(uvҲ����coord)���������ֵ
	HitPayload TraceRay(const Ray& ray);
	glm::vec4 PerPixel(uint32_t x, uint32_t y); // RayGen in Vulkan/Directx
	HitPayload ClosestHit(const Ray& ray, float hitDistance, int objectIndex);  // ����WorldPosition��WorldNormal
	HitPayload Miss(const Ray& ray);
	
private:
	// ���յ�����Ŀ����һ��image
	std::shared_ptr<Walnut::Image> m_FinalImage;

	Settings m_Settings;

	std::vector<uint32_t> m_ImageHorizontalIter, m_ImageVerticalIter;

	const Scene* m_ActiveScene = nullptr;
	const Camera* m_ActiveCamera = nullptr;
	
	// ���ÿ�����ص���ɫ
	uint32_t* m_ImageData = nullptr;

	// path tracing! ÿ���ƶ�����ͷ����¼·�����ۼ�����
	glm::vec4* m_AccumulationData = nullptr;
	// ·������
	uint32_t m_FrameIndex = 1;
};

