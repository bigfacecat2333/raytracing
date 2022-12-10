#pragma once
#include "Walnut/Image.h"
#include <memory>
#include <glm/glm.hpp>

class Renderer
{
public:
	Renderer() = default;
	
	void onResize(uint32_t width, uint32_t height);

	void Render();

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }

private:
	// ����shadertoy�ĺ��� ��������(uvҲ����coord)���������ֵ
	uint32_t PerPixel(glm::vec2 coord);
	
private:
	// ���յ�����Ŀ����һ��image
	std::shared_ptr<Walnut::Image> m_FinalImage;
	
	// ���ÿ�����ص���ɫ
	uint32_t* m_ImageData = nullptr;
};

