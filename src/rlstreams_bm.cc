#include "rlstreams.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_searchablecombo.h>

std::ostream& operator<<(std::ostream& os, const Vector& v) {
	os << v.X << "," << v.Y << "," << v.Z;
	return os;
}

std::ostream& operator<<(std::ostream& os, const Quat& q) {
	os << q.X << "," << q.Y << "," << q.Z << "," << q.W;
	return os;
}

std::ostream& operator<<(std::ostream& os, const Rotator& r) {
	os << /*r.Pitch << "," << r.Yaw << "," << r.Roll << "," <<*/ RotatorToQuat(r);
	return os;
}

void copy_to_clipboard(std::string data) {
	const char* output = data.c_str();
	const size_t len = strlen(output) + 1;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	LPVOID lock = GlobalLock(hMem);
	memcpy(lock, output, len);
	GlobalUnlock(lock);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
}

inline std::string Serialize(CameraWrapper& camera, BallWrapper ball, ArrayWrapper<CarWrapper> cars) {
	std::stringstream ss;
	//ss << std::fixed << std::setprecision(6);

	if (!camera.IsNull()) {
		ss << camera.GetLocation() << "," << camera.GetRotation() << "," << camera.GetFOV() << ";";
	}
	
	if (!ball.IsNull()) {
		ss << ball.GetLocation() << "," << ball.GetRotation() << ";";
	}

	for (auto car : cars) {
		ss << car.GetLocation() << "," << car.GetRotation() << ";";
	}

	return ss.str();
}

//#define DEV
#ifdef DEV
constexpr auto DEFAULT_REC_PATH = "C:\\Users\\Aku\\Desktop\\rlstreams\\test_rec";
#else
constexpr auto DEFAULT_REC_PATH = "C:\\RLStreams";
#endif

void RLStreams::onLoad() {
	cv_rec_path = std::make_shared<std::string>(DEFAULT_REC_PATH);
	cv_rec_name = std::make_shared<std::string>("video");
	cv_rec_depth = std::make_shared<bool>(true);
	cv_rec_normal = std::make_shared<bool>(false);
	cv_rec_custom_pass = std::make_shared<bool>(false);
	cv_rec_custom_pass_preview = std::make_shared<bool>(false);
	cv_rec_data = std::make_shared<bool>(true);
	cv_debug_models = std::make_shared<bool>(false);
	cv_fps = std::make_shared<int>(30);

	m_custom_pass_color[0] = 1.0f;
	m_custom_pass_color[1] = 0.0f;
	m_custom_pass_color[2] = 1.0f;

	this->initialize_kiero();

	gameWrapper->RegisterDrawable(std::bind(&RLStreams::onDraw, this, std::placeholders::_1));
	cvarManager->registerCvar("rlstreams_path", DEFAULT_REC_PATH, "Path to save recordings to", true, true, 0, true, 255, true).bindTo(cv_rec_path);
	cvarManager->registerCvar("rlstreams_name", "video", "Name of the recording", true, true, 0, true, 255, true).bindTo(cv_rec_name);
	cvarManager->registerCvar("rlstreams_depth", "1", "Record depth map", true, true, 0, true, 1, true).bindTo(cv_rec_depth);
	cvarManager->registerCvar("rlstreams_normal", "0", "Record normal map", true, true, 0, true, 1, true).bindTo(cv_rec_normal);
	cvarManager->registerCvar("rlstreams_custom_pass", "0", "Record custom pass", true, true, 0, true, 1, true).bindTo(cv_rec_custom_pass);
	cvarManager->registerCvar("rlstreams_custom_pass_preview", "0", "Preview custom pass", true, true, 0, true, 1, true).bindTo(cv_rec_custom_pass_preview);
	cvarManager->registerCvar("rlstreams_frame_data", "1", "Record camera, cars and ball data", true, true, 0, true, 1, true).bindTo(cv_rec_data);
	cvarManager->registerCvar("rlstreams_debug_models", "0", "Debug models", true, true, 0, true, 1, true).bindTo(cv_debug_models);
	cvarManager->registerCvar("rlstreams_fps", "30", "Video framerate", true, true, 1, true, 999, true).bindTo(cv_fps);

	cvarManager->registerNotifier("rlstreams_start", [this](std::vector<std::string> params) {
		if (!gameWrapper->IsInReplay()) {
			cvarManager->log("You must be in a replay to start recording");
			return;
		}

		if (params.size() != 1) {
			cvarManager->log("Error: `rlstreams_start` doesn't take any parameters");
			return;
		}

		if (!reshade_effects_enabled) {
			cvarManager->log("Error: ReShade effects must be enabled to start recording");
			return;
		}

		if (this->is_recording()) {
			return;
		}

		this->m_recording_state = true;
		}, "Start recording", PERMISSION_ALL);

	cvarManager->registerNotifier("rlstreams_stop", [this](std::vector<std::string> params) {
		this->m_recording_state = false;
		}, "Stop recording", PERMISSION_ALL);

	// Debug models notifiers

	cvarManager->registerNotifier("rlstreams_debug_models_prev", [this](std::vector<std::string> params) {
		if (params.size() != 1) {
			cvarManager->log("Error: `rlstreams_debug_models_prev` doesn't take any parameters");
			return;
		}

		m_properties_models.lock();

		auto current = m_seen_params.find(m_current_params);

		if (current == m_seen_params.begin()) {
			current = m_seen_params.end();
			m_current_param_position = m_seen_params.size();
		}
		else {
			current--;
			m_current_param_position--;
		}
		m_current_params = *current;
		std::size_t hash = std::hash<PropertiesModel>{}(m_current_params);
		cvarManager->log(std::format("[rlstreams_debug_models]: {:x}", hash));
		copy_to_clipboard(std::format("{:x}", hash));

		m_properties_models.unlock();

		}, "Switch to the previous model in debug mode", PERMISSION_ALL);

	cvarManager->registerNotifier("rlstreams_debug_models_next", [this](std::vector<std::string> params) {
		if (params.size() != 1) {
			cvarManager->log("Error: `rlstreams_debug_models_next` doesn't take any parameters");
			return;
		}

		m_properties_models.lock();

		auto current = m_seen_params.find(m_current_params);

		if (current == m_seen_params.end()) {
			current = m_seen_params.begin();
			m_current_param_position = 1;
		}
		else {
			current++;
			m_current_param_position++;
		}
		m_current_params = *current;
		std::size_t hash = std::hash<PropertiesModel>{}(m_current_params);
		cvarManager->log(std::format("[rlstreams_debug_models]: {:x}", hash));
		copy_to_clipboard(std::format("{:x}", hash));

		m_properties_models.unlock();

		}, "Switch to the next model in debug mode", PERMISSION_ALL);

	cvarManager->registerNotifier("rlstreams_debug_models_add", [this](std::vector<std::string> params) {
		if (params.size() != 1) {
			cvarManager->log("Error: `rlstreams_debug_models_add` doesn't take any parameters");
			return;
		}

		m_properties_models.lock();
		m_custom_pass_mutex.lock();
		m_custom_pass.insert(std::hash<PropertiesModel>{}(m_current_params));
		m_custom_pass_mutex.unlock();
		m_properties_models.unlock();

		}, "Add currently selected model to the custom pass", PERMISSION_ALL);

	// Custom pass notifiers

	cvarManager->registerNotifier("rlstreams_custom_pass_clear", [this](std::vector<std::string> params) {
		if (params.size() != 1) {
			cvarManager->log("Error: `rlstreams_custom_pass_clear` doesn't take any parameters");
			return;
		}

		m_custom_pass_mutex.lock();
		m_custom_pass.clear();
		m_custom_pass_mutex.unlock();

		}, "Clear all hashes from custom pass", PERMISSION_ALL);

	cvarManager->registerNotifier("rlstreams_custom_pass_add", [this](std::vector<std::string> params) {
		if (params.size() != 2) {
			cvarManager->log("Error: `rlstreams_custom_pass_add` requires 1 parameter");
			return;
		}

		m_custom_pass_mutex.lock();
		m_custom_pass.insert(parse_hex(params.at(1)));
		m_custom_pass_mutex.unlock();

		}, "Add the hash of a draw call to the list for custom pass", PERMISSION_ALL);

	cvarManager->registerNotifier("rlstreams_custom_pass_save", [this](std::vector<std::string> params) {
		if (params.size() != 2) {
			cvarManager->log("Error: `rlstreams_custom_pass_save` requires 1 parameter");
			return;
		}

		m_custom_pass_mutex.lock();

		auto path = gameWrapper->GetBakkesModPath() / "cfg" / "rlstreams";
		std::filesystem::create_directory(path);

		std::ofstream cfg_file(path / (params.at(1) + ".cfg"), std::ofstream::out);

		cfg_file << "rlstreams_custom_pass_clear\n";

		for (auto hash : m_custom_pass) {
			cfg_file << std::format("rlstreams_custom_pass_add {:x}\n", hash);
		}

		m_custom_pass_mutex.unlock();

		}, "Saves custom pass as a config", PERMISSION_ALL);

	cvarManager->registerNotifier("rlstreams_custom_pass_color", [this](std::vector<std::string> params) {
		if (params.size() != 2) {
			cvarManager->log("Error: `rlstreams_custom_pass_color` requires 1 parameter");
			return;
		}

		const std::string& hex_color = params.at(1);

		if (hex_color.size() != 7 || hex_color[0] != '#') {
			return;
		}

		std::stringstream ss;
		ss << std::hex << hex_color.substr(1);
		int color_value;
		ss >> color_value;

		float red = static_cast<float>((color_value >> 16) & 0xFF) / 255.0f;
		float green = static_cast<float>((color_value >> 8) & 0xFF) / 255.0f;
		float blue = static_cast<float>(color_value & 0xFF) / 255.0f;

		std::unique_lock<std::mutex> shader_lock(m_shader_mutex);

		if (m_shader) {
			m_shader->Release();
		}

		GenerateShader(m_device, red, green, blue, &m_shader);

		m_custom_pass_color[0] = red;
		m_custom_pass_color[1] = green;
		m_custom_pass_color[2] = blue;

		}, "Change color of a shader", PERMISSION_ALL);


	cvarManager->executeCommand("exec rlstreams/default.cfg");
}

void RLStreams::onUnload() {
	kiero::shutdown();
	if (m_shader) {
		m_shader->Release();
	}
	if (m_device) {
		m_device->Release();
	}
}

void RLStreams::onDraw(CanvasWrapper) {
	if (!gameWrapper->IsInReplay()) {
		m_current_frame_data = "";
		m_is_in_replay = false;
		return;
	}

	m_is_in_replay = true;

	auto camera = gameWrapper->GetCamera();

	if (camera.IsNull()) {
		m_current_frame_data = "";
		return;
	}

	auto server = gameWrapper->GetGameEventAsReplay();

	if (server.IsNull()) {
		m_current_frame_data = "";
		return;
	}

	auto ball = server.GetBall();

	if (ball.IsNull()) {
		m_current_frame_data = "";
		return;
	}

	auto cars = server.GetCars();

	if (cars.IsNull()) {
		m_current_frame_data = "";
		return;
	}

	m_current_frame_data = Serialize(camera, ball, cars);
}

void RLStreams::SetImGuiContext(uintptr_t ctx) {
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

std::string RLStreams::GetPluginName() {
	return "RLStreams 2.0";
}

void RLStreams::RenderSettings() {
	bool is_rec = is_recording();
	
	/// Recording path and file name
	static char buf[64] = "";
	strcpy(buf, cv_rec_path->c_str());

	if (ImGui::InputText("Recording directory", buf, 64)) {
		cvarManager->getCvar("rlstreams_path").setValue(buf);
	}
	
	strcpy(buf, cv_rec_name->c_str());

	if (ImGui::InputText("File name", buf, 64)) {
		cvarManager->getCvar("rlstreams_name").setValue(buf);
	}
	
	ImGui::Spacing();
	ImGui::Spacing();

	/// Recording options (depth, normal, frame data)
	bool state = *cv_rec_depth;
	if (ImGui::Checkbox("Depth map", &state)) {
		cvarManager->getCvar("rlstreams_depth").setValue(state);
	}
	state = *cv_rec_normal;
	if (ImGui::Checkbox("Normal map", &state)) {
		cvarManager->getCvar("rlstreams_normal").setValue(state);
	}
	state = *cv_rec_custom_pass;
	if (ImGui::Checkbox("Record custom pass", &state)) {
		cvarManager->getCvar("rlstreams_custom_pass").setValue(state);
	}
	state = *cv_rec_data;
	if (ImGui::Checkbox("Record camera, cars and ball data", &state)) {
		cvarManager->getCvar("rlstreams_frame_data").setValue(state);
	}

	ImGui::Spacing();
	ImGui::Spacing();

	/// FPS
	if (ImGui::SliderInt("Framerate", cv_fps.get(), 1, 60)) {
		cvarManager->getCvar("rlstreams_fps").setValue(*cv_fps);
	}
	
	ImGui::Spacing();
	ImGui::Spacing();

	/// Start and stop recording button

	if (is_rec || !reshade_effects_enabled) {
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	
	if (ImGui::Button("Start recording")) {
		cvarManager->executeCommand("closemenu settings");
		cvarManager->executeCommand("rlstreams_start");
	}

	if (is_rec || !reshade_effects_enabled) {
		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
	}
	
	ImGui::SameLine();

	if (!is_rec || !reshade_effects_enabled) {
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	
	if (ImGui::Button("Stop recording")) {
		cvarManager->executeCommand("rlstreams_stop");
	}

	if (!is_rec || !reshade_effects_enabled) {
		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Custom pass"))
	{
		ImGui::Spacing();
		ImGui::Spacing();

		if (ImGui::ColorEdit3("Color", m_custom_pass_color)) {
			std::unique_lock<std::mutex> shader_lock(m_shader_mutex);

			if (m_shader) {
				m_shader->Release();
			}

			GenerateShader(m_device, m_custom_pass_color[0], m_custom_pass_color[1], m_custom_pass_color[2], &m_shader);
		}

		ImGui::Spacing();
		ImGui::Spacing();

		state = *cv_rec_custom_pass_preview;
		if (ImGui::Checkbox("Preview custom pass", &state)) {
			cvarManager->getCvar("rlstreams_custom_pass_preview").setValue(state);
		}

		ImGui::Spacing();
		ImGui::Spacing();

		state = *cv_debug_models;
		if (ImGui::Checkbox("Debug models", &state)) {
			cvarManager->getCvar("rlstreams_debug_models").setValue(state);
		}

		ImGui::SameLine();

		if (ImGui::Button("Previous")) {
			cvarManager->executeCommand("rlstreams_debug_models_prev");
		}

		ImGui::SameLine();

		if (ImGui::Button("Add to custom pass")) {
			cvarManager->executeCommand("rlstreams_debug_models_add");
		}

		ImGui::SameLine();

		if (ImGui::Button("Next")) {
			cvarManager->executeCommand("rlstreams_debug_models_next");
		}
	}
}
