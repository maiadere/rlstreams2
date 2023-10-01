#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include <windows.h>
#include <reshade.hpp>

#include <chrono>
#include <format>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_set>

#include <kiero.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "avi_utils.h"

using namespace reshade::api;

typedef void(__stdcall* DrawIndexed)(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);

inline std::size_t parse_hex(std::string hex) noexcept {
	size_t x;
	std::stringstream ss;
	ss << std::hex << hex;
	ss >> x;
	return x;
}

HRESULT GenerateShader(ID3D11Device* pD3DDevice, float r, float g, float b, ID3D11PixelShader** pShader);

struct bitmap_t {
	HBITMAP hbm;
	uint8_t* bits;
};

bitmap_t create_bitmap(int width, int height);

struct PropertiesModel {
	UINT stride;
	UINT vedesc_ByteWidth;
	UINT indesc_ByteWidth;
	UINT pscdesc_ByteWidth;
};

bool operator==(const PropertiesModel& lhs, const PropertiesModel& rhs);

namespace std {
	template<> struct hash<PropertiesModel> {
		std::size_t operator()(const PropertiesModel& obj) const noexcept;
	};
}

class RLStreams: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow {
public:
	// BakkesMod events
	virtual void onLoad();
	virtual void onUnload();

	void onDraw(CanvasWrapper);

	// UI
	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;

	// ReShade events
	void on_finish_effects(effect_runtime* runtime, command_list *cmd_list, resource_view rtv, resource_view rtv_srgb);

	// DX11
	void initialize_kiero();
	void draw_indexed(ID3D11DeviceContext* context, UINT index_count, UINT first_index, INT vertex_offset);

private:
	// Recording functions
	void start_recording(effect_runtime* runtime, device* const device, const resource rtv_resource);
	void stop_recording(effect_runtime* runtime, device* const device);

	bool is_recording() const;

	// Copy frame to buffer (m_data.buffer)
	void copy_to_buffer(effect_runtime* runtime, device* const device, resource rtv_resource);
	
	// Get serialized camera, ball and cars data
	std::string get_current_frame_data() const;

	// Get recording path
	std::filesystem::path get_recording_path() const;

public:
	// ReShade runtime
	// effect_runtime* runtime;
	bool reshade_effects_enabled = true;
	
private:
	// CVar for recording path
	std::shared_ptr<std::string> cv_rec_path;

	// CVar for recording name
	std::shared_ptr<std::string> cv_rec_name;

	// CVar for recording depth map
	std::shared_ptr<bool> cv_rec_depth;

	// CVar for recording frame data
	std::shared_ptr<bool> cv_rec_data;

	// CVar for recording normal map
	std::shared_ptr<bool> cv_rec_normal;

	// CVar for recording custom pass
	std::shared_ptr<bool> cv_rec_custom_pass;

	// CVar for previewing custom pass
	std::shared_ptr<bool> cv_rec_custom_pass_preview;

	// CVar for debugging models
	std::shared_ptr<bool> cv_debug_models;

	// CVar for video framerate
	std::shared_ptr<int> cv_fps;

	// CVar that controls whether custom pass uses main buffer or separate one
	std::shared_ptr<bool> cv_custom_pass_main_buffer;

private:
	// Serialized camera, ball and cars data for current frame
	// 
	// Format: "cam_loc,cam_rot,cam_fov;ball_loc,ball_rot;car1_loc,car1_rot;car2_loc,car2_rot;...;"
	//
	// Updated every frame in onDraw()
	std::string m_current_frame_data;

	// Recording state
	bool m_recording_state = false;

	// Controls when custom pass should be applied when recording
	bool m_custom_pass_should_apply = false;

	// Used to prevent DX11 from being applied outside of replays.
	std::atomic_bool m_is_in_replay = false;

private:
	struct recording_data {
		reshade::api::resource host_resource;
		uint32_t width, height;
		bitmap_t bitmap;
		
		std::ofstream data_file;
		std::chrono::steady_clock::time_point start_point;

		HAVI avi;
		HAVI avi_d; // depth
		HAVI avi_n; // normal
		HAVI avi_c; // custom pass

		int frame_count = 0;
		std::atomic_int recorded_fps = 0;
	} m_data;

	AVICOMPRESSOPTIONS m_avi_opts;

	float m_custom_pass_color[3] = { 1.0f, 0.0f, 1.0f };
	ID3D11PixelShader* m_shader = NULL;
	std::mutex m_shader_mutex;
	ID3D11Device* m_device = NULL;

	struct PropertiesModel m_current_params;
	std::unordered_set<PropertiesModel> m_seen_params;
	std::mutex m_properties_models;
	int m_current_param_position = 1;

	std::unordered_set<std::size_t> m_custom_pass;
	std::mutex m_custom_pass_mutex;
};
