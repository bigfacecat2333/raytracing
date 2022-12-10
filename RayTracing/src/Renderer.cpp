#include "Renderer.h"

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
			
			m_ImageData[x + y * m_FinalImage->GetWidth()] = PerPixel(coord);
		}
	}
	
	m_FinalImage->SetData(m_ImageData);  // �����ݴ���GPU
}

uint32_t Renderer::PerPixel(glm::vec2 coord)
{
	// channels
 	uint8_t r = (uint8_t)(coord.x * 255.0f);
	uint8_t g = (uint8_t)(coord.y * 255.0f);
	
	// 0xff00 ��Alpha��blue 0000 ����������
	return 0xff000000 | (g << 8) | r;
}
