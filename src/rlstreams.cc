#include "rlstreams.h"

bitmap_t create_bitmap(int width, int height) {
	BITMAPINFO bmi;
	ZeroMemory(&bmi, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;

	HDC hdc = GetDC(NULL);
	uint8_t* bits = nullptr;
	HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
	ReleaseDC(NULL, hdc);

	return { hbm, bits };
}

bool RLStreams::is_recording() const {
	return m_recording_state;
}

std::string RLStreams::get_current_frame_data() const {
	return m_current_frame_data;
}

std::filesystem::path RLStreams::get_recording_path() const {
	return *cv_rec_path;
}

void RLStreams::start_recording(effect_runtime* runtime, device* const device, const resource rtv_resource) {
	auto file_name_t = get_recording_path() / std::format("{}.txt", (*cv_rec_name));
	auto file_name_r = get_recording_path() / std::format("{}.avi", (*cv_rec_name));
	auto file_name_d = get_recording_path() / std::format("{} - depth.avi", (*cv_rec_name));
	auto file_name_n = get_recording_path() / std::format("{} - normal.avi", (*cv_rec_name));
	auto file_name_c = get_recording_path() / std::format("{} - custom pass.avi", (*cv_rec_name));

	// Initialize AVI compress options (x264vfw)
	ZeroMemory(&m_avi_opts, sizeof(m_avi_opts));
	m_avi_opts.fccHandler = mmioFOURCC('x', '2', '6', '4');
	m_avi_opts.dwFlags = AVICOMPRESSF_VALID;
	m_avi_opts.cbParms = 9336; // i tried to figure out where this comes from, no idea

	// Delete old recording files if they exist
	std::filesystem::remove(file_name_t);
	std::filesystem::remove(file_name_r);
	std::filesystem::remove(file_name_d);
	std::filesystem::remove(file_name_n);
	std::filesystem::remove(file_name_c);

	// Create host resource
	reshade::api::resource_desc desc = device->get_resource_desc(rtv_resource);

	desc.type = reshade::api::resource_type::texture_2d;
	desc.heap = reshade::api::memory_heap::gpu_to_cpu;
	desc.usage = reshade::api::resource_usage::copy_dest;
	desc.flags = reshade::api::resource_flags::none;

	if (device->create_resource(desc, nullptr, reshade::api::resource_usage::copy_dest, &m_data.host_resource)) {
		reshade::log_message(3, "Starting video recording...");
	} else {
		reshade::log_message(1, "Failed to create host resource!");
		m_recording_state = false;
		return;
	}

	// Initialize data
	m_custom_pass_should_apply = false;
	m_data.frame_count = 0;
	m_data.start_point = std::chrono::steady_clock::now();
	
	// Create data file
	if (*cv_rec_data) {
		m_data.data_file.open(file_name_t, std::ios::out);
	}
	
	// Get width and height of buffer
	runtime->get_screenshot_width_and_height(&m_data.width, &m_data.height);

	// Allocate buffer
	m_data.bitmap = create_bitmap(m_data.width, m_data.height);

	m_data.avi = CreateAvi(file_name_r.string().c_str(), 1000 / 30, NULL);

	if (*cv_rec_depth) {
		m_data.avi_d = CreateAvi(file_name_d.string().c_str(), 1000 / 30, NULL);
	}

	if (*cv_rec_normal) {
		m_data.avi_n = CreateAvi(file_name_n.string().c_str(), 1000 / 30, NULL);
	}

	if (*cv_rec_custom_pass) {
		m_data.avi_c = CreateAvi(file_name_c.string().c_str(), 1000 / 30, NULL);
	}

	//// Open files
	//m_data.rec_file.open(get_recording_path() / (*cv_rec_name), std::ios_base::app | std::ios_base::binary);
	//if (*cv_rec_depth || *cv_rec_normal) {
	//	m_data.tex_file.open(get_recording_path() / ((*cv_rec_name) + "-nnnd"), std::ios_base::app | std::ios_base::binary);
	//}
}

void RLStreams::stop_recording(effect_runtime* runtime, device* const device) {
	// Log message
	reshade::log_message(3, "Stopping video recording...");

	// Destroy host resource
	runtime->get_command_queue()->wait_idle();
	device->destroy_resource(m_data.host_resource);
	m_data.host_resource = { 0 };

	// Free buffer
	DeleteObject(m_data.bitmap.hbm);
	m_data.bitmap = { NULL, nullptr };

	// Close data file
	if (*cv_rec_data) {
		m_data.data_file.close();
	}

	// Calculate FPS
	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_data.start_point).count();
	auto fps = m_data.frame_count / (duration / 1000.0f);
	m_data.recorded_fps = fps;

	// Close recording files
	//m_data.rec_file.close();
	//if (*cv_rec_depth || *cv_rec_normal) {
	//	m_data.tex_file.close();
	//}
	CloseAvi(m_data.avi);

	if (*cv_rec_depth) {
		CloseAvi(m_data.avi_d);
	}

	if (*cv_rec_normal) {
		CloseAvi(m_data.avi_n);
	}

	if (*cv_rec_custom_pass) {
		CloseAvi(m_data.avi_c);
	}

	reshade::log_message(4, std::format("Recorded {} frames in {} ms", m_data.frame_count, duration).c_str());
	reshade::log_message(4, std::format("FPS: {}", fps).c_str());
	
	// Reset frame count, so that this function doesn't get called anymore
	m_data.frame_count = 0;
}
