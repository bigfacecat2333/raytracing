#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"

#include "Renderer.h"

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");
		ImGui::Text("Last render time: %.3f ms", m_LastRenderTime);
		if (ImGui::Button("Render")) 
		{
			Render();
		}
		ImGui::End();
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_ViewportWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = (uint32_t)ImGui::GetContentRegionAvail().y;
		
		auto image = m_renderer.GetFinalImage();
		// ͨ��ImGui::Image����ʾͼƬ
		if (image) {
			ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight() }, 
				ImVec2(0, 1), ImVec2(1, 0));  // ͨ��ImVec2������ͼƬ����ʾ���򣨵ߵ�uv�ᣩ
		}

		ImGui::End();
		ImGui::PopStyleVar();

		// �Զ�ȥ���ϵ���Ⱦ
		Render();
	}

	void Render()
	{
		Timer timer;

		// renderer resize
		m_renderer.onResize(m_ViewportWidth, m_ViewportHeight);
		// renderer render
		m_renderer.Render();
		
		m_LastRenderTime = timer.Elapsed();  // ������Ⱦʱ��
	}
private:
	Renderer m_renderer;
	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
	uint32_t* m_ImageData = nullptr;
	
	float m_LastRenderTime = 0.0f;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Ray Tracing";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}