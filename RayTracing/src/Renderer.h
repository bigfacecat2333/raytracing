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
		// 累加设置
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
	// Payload表示射出去的光线发生了什么:是否击中物体，以及击中物体的序号和位置以及法线
	struct HitPayload
	{
		float HitDistance;
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;

		int ObjectIndex;
	};
	// 类似shadertoy的函数 输入坐标(uv也就是coord)，输出像素值
	HitPayload TraceRay(const Ray& ray);
	glm::vec4 PerPixel(uint32_t x, uint32_t y); // RayGen in Vulkan/Directx
	HitPayload ClosestHit(const Ray& ray, float hitDistance, int objectIndex);  // 计算WorldPosition和WorldNormal
	HitPayload Miss(const Ray& ray);
	
private:
	// 最终的生成目标是一张image
	std::shared_ptr<Walnut::Image> m_FinalImage;

	Settings m_Settings;

	std::vector<uint32_t> m_ImageHorizontalIter, m_ImageVerticalIter;

	const Scene* m_ActiveScene = nullptr;
	const Camera* m_ActiveCamera = nullptr;
	
	// 存放每个像素的颜色
	uint32_t* m_ImageData = nullptr;

	// path tracing! 每次移动摄像头，记录路径的累计像素
	glm::vec4* m_AccumulationData = nullptr;
	// 路径数量
	uint32_t m_FrameIndex = 1;
};

