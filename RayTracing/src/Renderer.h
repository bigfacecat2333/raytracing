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
	// 类似shadertoy的函数 输入坐标(uv也就是coord)，输出像素值
	uint32_t PerPixel(glm::vec2 coord);
	
private:
	// 最终的生成目标是一张image
	std::shared_ptr<Walnut::Image> m_FinalImage;
	
	// 存放每个像素的颜色
	uint32_t* m_ImageData = nullptr;
};

