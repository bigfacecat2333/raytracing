#include "Renderer.h"

#include "Walnut/Random.h"

#include <execution>

namespace Utils {
	
	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);  // rgb相当于xyz
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}
	
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		// no need to resize
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
		{
			return;
		}

		m_FinalImage->Resize(width, height);
	}
	else
	{
		// create a new image
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;  // 释放之前的内存
	m_ImageData = new uint32_t[width * height];  // 重新分配内存

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));

#define MT 1
#if MT
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
			[this, y](uint32_t x)
				{
					glm::vec4 color = PerPixel(x, y);  // 外层循环是对每个像素进行追踪，不同于光栅化对每个变换来的物体 （原因是光线追踪考虑全局光照），也就是在开始离散化

					// path tracing
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
				});
		});

#else
	//render all pixels 注意：y是行 x是列
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		}
	}
#endif

	m_FinalImage->SetData(m_ImageData);  // 发送给GPU

	// 累计模式
	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray; 
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];
	
	// final color
	glm::vec3 color(0.0f);
	float multiplier = 1.0f;

	// path tracing
	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)  // miss时返回-1
		{
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			color += skyColor * multiplier;
			break;
		}

		glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));
		float lightIntensity = glm::max(glm::dot(payload.WorldNormal, -lightDir), 0.0f); // == cos(angle) irradiance?

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		glm::vec3 sphereColor = material.Albedo;
		sphereColor *= lightIntensity;
		color += sphereColor * multiplier;

		multiplier *= 0.5f;

		// 当hit到物体时，需要改变光线的起点和方向
		// 新的光线起点就是hit position + 一个很小的法线方向的偏移量（不然的话由精度决定会有随机性，有可能自己撞到自己而不做追踪）
		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		
		// ray.Direction = glm::reflect(ray.Direction, payload.WorldNormal);
		// PBR：Microfacet surface models are often described by a function that gives the distribution of microfacet normals 
		// with respect to the surface normal . 
		// (a) The greater the variation of microfacet normals, the rougher the surface is. 
		// (b) Smooth surfaces have relatively little variation of microfacet normals.
		ray.Direction = glm::reflect(ray.Direction,
			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
	}

	return glm::vec4(color, 1.0f);
}

// 相当于shader language
Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	// 计算与2d的圆是否相交的公式：(bx^2 + by^2) * t^2 + (2axbx + 2ayby) * t + (ax^2 + ay^2 - r^2) = 0 
	// 圆心在000
	// a是ray的原点
	// b是ray的方向
	// r是圆的半径
	// t是hit distance射线的长度
	int closestSphere = -1;  // 最近的球的序号index
	float hitDistance = std::numeric_limits<float>::max();  // 最近的球的距离 FLT_MAX相对来说占用更小
	
	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)  // 光追的内层循环是对每个物体（将像素投影到物体上）计算是否和物体相交（光栅化这步是在外循环中）
	{
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		// (x - a)  (y - a)
		glm::vec3 origin = ray.Origin - sphere.Position;

		// float a = rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y + rayDirection.z * rayDirection.z;
		float a = glm::dot(ray.Direction, ray.Direction);  // firstTerm
		float b = 2.0f * glm::dot(origin, ray.Direction);  // SecondTerm
		float c = glm::dot(origin, origin) - sphere.Radius * sphere.Radius;  // ThirdTerm

		// Quadratic forumula discriminant:
		// b^2 - 4ac

		float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f)
			continue;

		// Quadratic formula:
		// (-b +- sqrt(discriminant)) / 2a

		// float t0 = (-b + glm::sqrt(discriminant)) / (2.0f * a); // Second hit distance (currently unused)
		float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);

		// 更新序号和距离
		if (closestT > 0.0f && closestT < hitDistance)
		{
			hitDistance = closestT;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
		return Miss(ray);  // 未击中物体，hitdistance返回-1

	return ClosestHit(ray, hitDistance, closestSphere);  // 返回击中物体的信息

}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	// 最近的球体
	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	// 仍然假设球心在000，通过改变摄像机的位置（origin）来达到移动球体（球心不在0）的目的
	glm::vec3 origin = ray.Origin - closestSphere.Position;
	
	// hit point a + dir * t
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	// 球心始终在原点，所以法线就是hit point
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	// 前面改变了origin，所以这里要改回来
	payload.WorldPosition += closestSphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}