#include "Renderer.h"

#include "Walnut/Random.h"

#include <execution>

namespace Utils {
	
	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);  // rgb�൱��xyz
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

	delete[] m_ImageData;  // �ͷ�֮ǰ���ڴ�
	m_ImageData = new uint32_t[width * height];  // ���·����ڴ�

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
					glm::vec4 color = PerPixel(x, y);  // ���ѭ���Ƕ�ÿ�����ؽ���׷�٣���ͬ�ڹ�դ����ÿ���任�������� ��ԭ���ǹ���׷�ٿ���ȫ�ֹ��գ���Ҳ�����ڿ�ʼ��ɢ��

					// path tracing
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
				});
		});

#else
	//render all pixels ע�⣺y���� x����
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

	m_FinalImage->SetData(m_ImageData);  // ���͸�GPU

	// �ۼ�ģʽ
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
		if (payload.HitDistance < 0.0f)  // missʱ����-1
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

		// ��hit������ʱ����Ҫ�ı���ߵ����ͷ���
		// �µĹ���������hit position + һ����С�ķ��߷����ƫ��������Ȼ�Ļ��ɾ��Ⱦ�����������ԣ��п����Լ�ײ���Լ�������׷�٣�
		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		
		// ray.Direction = glm::reflect(ray.Direction, payload.WorldNormal);
		// PBR��Microfacet surface models are often described by a function that gives the distribution of microfacet normals 
		// with respect to the surface normal . 
		// (a) The greater the variation of microfacet normals, the rougher the surface is. 
		// (b) Smooth surfaces have relatively little variation of microfacet normals.
		ray.Direction = glm::reflect(ray.Direction,
			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
	}

	return glm::vec4(color, 1.0f);
}

// �൱��shader language
Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	// ������2d��Բ�Ƿ��ཻ�Ĺ�ʽ��(bx^2 + by^2) * t^2 + (2axbx + 2ayby) * t + (ax^2 + ay^2 - r^2) = 0 
	// Բ����000
	// a��ray��ԭ��
	// b��ray�ķ���
	// r��Բ�İ뾶
	// t��hit distance���ߵĳ���
	int closestSphere = -1;  // �����������index
	float hitDistance = std::numeric_limits<float>::max();  // �������ľ��� FLT_MAX�����˵ռ�ø�С
	
	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)  // ��׷���ڲ�ѭ���Ƕ�ÿ�����壨������ͶӰ�������ϣ������Ƿ�������ཻ����դ���ⲽ������ѭ���У�
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

		// ������ź;���
		if (closestT > 0.0f && closestT < hitDistance)
		{
			hitDistance = closestT;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
		return Miss(ray);  // δ�������壬hitdistance����-1

	return ClosestHit(ray, hitDistance, closestSphere);  // ���ػ����������Ϣ

}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	// ���������
	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	// ��Ȼ����������000��ͨ���ı��������λ�ã�origin�����ﵽ�ƶ����壨���Ĳ���0����Ŀ��
	glm::vec3 origin = ray.Origin - closestSphere.Position;
	
	// hit point a + dir * t
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	// ����ʼ����ԭ�㣬���Է��߾���hit point
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	// ǰ��ı���origin����������Ҫ�Ļ���
	payload.WorldPosition += closestSphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}