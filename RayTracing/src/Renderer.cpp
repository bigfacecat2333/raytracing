#include "Renderer.h"

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

void Renderer::onResize(uint32_t width, uint32_t height)
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
}

void Renderer::Render()
{
	// render all pixels ע�⣺y���� x����
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec2 coord = { (float)x / m_FinalImage->GetWidth(), (float)y / m_FinalImage->GetHeight() };
			
			// remap coordinate
			coord = coord * 2.0f - 1.0f;  // (0, 0) -> (-1, -1); (1, 1) -> (1, 1)
			
			glm::vec4 color = PerPixel(coord);  // ����PerPixel��������ÿ�����ص���ɫ
			color = glm::clamp(color, 0.0f, 1.0f);  // clamp color to [0, 1]
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(color);
		}
	}
	
	m_FinalImage->SetData(m_ImageData);  // �����ݴ���GPU
}

// �൱��shader language
glm::vec4 Renderer::PerPixel(glm::vec2 coord)
{
	// channels
 	uint8_t r = (uint8_t)(coord.x * 255.0f);
	uint8_t g = (uint8_t)(coord.y * 255.0f);

	// ������2d��Բ�Ƿ��ཻ�Ĺ�ʽ��(bx^2 + by^2) * t^2 + (2axbx + 2ayby) * t + (ax^2 + ay^2 - r^2) = 0 
	// Բ����000
	// a��ray��ԭ��
	// b��ray�ķ���
	// r��Բ�İ뾶
	// t��hit distance���ߵĳ���
	glm::vec3 rayOrigin(0.0f, 0.0f, 1.0f);  // ��ʱ��������Դͬλ��
	glm::vec3 rayDirection(coord.x, coord.y, -1.0f);
	float radius = 0.5f;
	// rayDirection = glm::normalize(rayDirection); normalize����Ϊ1��������ڼ�������normalize����ʱ
	
	// float a = rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y + rayDirection.z * rayDirection.z;
	float firstTerm = glm::dot(rayDirection, rayDirection);
	float secondTerm = 2.0f * glm::dot(rayOrigin, rayDirection);
	float thirdTerm = glm::dot(rayOrigin, rayOrigin) - radius * radius;

	// quadratic formula discriminant ���η���ʽ�б𷨣�
	// b^2 - 4 * a * c
	float discriminant = secondTerm * secondTerm - 4.0f * firstTerm * thirdTerm;
	if (discriminant < 0)
	{
		// ray doesn't hit the sphere
		// ���غ�ɫ
		return glm::vec4(0, 0, 0, 1); // 0xff000000;
	}
	// (-b +- sqrt(discriminat)) / (2.0f * a)
	float t0 = (-secondTerm + sqrt(discriminant)) / (2.0f * firstTerm);
	float closestT = (-secondTerm - sqrt(discriminant)) / (2.0f * firstTerm);

	// hit point
	glm::vec3 hitPoint = rayOrigin + rayDirection * closestT;

	// normal
	glm::vec3 normal = glm::normalize(hitPoint);  // ����Ϊ000
	
	glm::vec3 sphereColor = normal * 0.5f + 0.5f;  // ��normal��ֵ��[-1, 1]ӳ�䵽[0, 1]֮��
	glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));

	float cosAngle = glm::dot(normal, -lightDir);
	float d = glm::max(cosAngle, 0.0f);  // ������պͷ��ߴ���90�ȣ��͵�����90�ȴ���
	
	sphereColor *= d;
	
	return glm::vec4(sphereColor, 1);

}
