#include "Renderer.h"

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

	delete[] m_ImageData;  // 释放之前的内存
	m_ImageData = new uint32_t[width * height];  // 重新分配内存
}

void Renderer::Render()
{
	// render all pixels 注意：y是行 x是列
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec2 coord = { (float)x / m_FinalImage->GetWidth(), (float)y / m_FinalImage->GetHeight() };
			
			// remap coordinate
			coord = coord * 2.0f - 1.0f;  // (0, 0) -> (-1, -1); (1, 1) -> (1, 1)
			
			glm::vec4 color = PerPixel(coord);  // 调用PerPixel函数计算每个像素的颜色
			color = glm::clamp(color, 0.0f, 1.0f);  // clamp color to [0, 1]
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(color);
		}
	}
	
	m_FinalImage->SetData(m_ImageData);  // 将数据传入GPU
}

// 相当于shader language
glm::vec4 Renderer::PerPixel(glm::vec2 coord)
{
	// channels
 	uint8_t r = (uint8_t)(coord.x * 255.0f);
	uint8_t g = (uint8_t)(coord.y * 255.0f);

	// 计算与2d的圆是否相交的公式：(bx^2 + by^2) * t^2 + (2axbx + 2ayby) * t + (ax^2 + ay^2 - r^2) = 0 
	// 圆心在000
	// a是ray的原点
	// b是ray的方向
	// r是圆的半径
	// t是hit distance射线的长度
	glm::vec3 rayOrigin(0.0f, 0.0f, 1.0f);  // 此时摄像机与光源同位置
	glm::vec3 rayDirection(coord.x, coord.y, -1.0f);
	float radius = 0.5f;
	// rayDirection = glm::normalize(rayDirection); normalize后点积为1，但相比于计算点积，normalize更耗时
	
	// float a = rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y + rayDirection.z * rayDirection.z;
	float firstTerm = glm::dot(rayDirection, rayDirection);
	float secondTerm = 2.0f * glm::dot(rayOrigin, rayDirection);
	float thirdTerm = glm::dot(rayOrigin, rayOrigin) - radius * radius;

	// quadratic formula discriminant 二次方公式判别法：
	// b^2 - 4 * a * c
	float discriminant = secondTerm * secondTerm - 4.0f * firstTerm * thirdTerm;
	if (discriminant < 0)
	{
		// ray doesn't hit the sphere
		// 返回黑色
		return glm::vec4(0, 0, 0, 1); // 0xff000000;
	}
	// (-b +- sqrt(discriminat)) / (2.0f * a)
	float t0 = (-secondTerm + sqrt(discriminant)) / (2.0f * firstTerm);
	float closestT = (-secondTerm - sqrt(discriminant)) / (2.0f * firstTerm);

	// hit point
	glm::vec3 hitPoint = rayOrigin + rayDirection * closestT;

	// normal
	glm::vec3 normal = glm::normalize(hitPoint);  // 球心为000
	
	glm::vec3 sphereColor = normal * 0.5f + 0.5f;  // 将normal的值从[-1, 1]映射到[0, 1]之间
	glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));

	float cosAngle = glm::dot(normal, -lightDir);
	float d = glm::max(cosAngle, 0.0f);  // 如果光照和法线大于90度，就当等于90度处理
	
	sphereColor *= d;
	
	return glm::vec4(sphereColor, 1);

}
