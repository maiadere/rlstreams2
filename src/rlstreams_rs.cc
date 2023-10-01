#include "rlstreams.h"

void RLStreams::copy_to_buffer(effect_runtime* runtime, device* const device, resource rtv_resource) {
	reshade::api::command_list* const cmd_list = runtime->get_command_queue()->get_immediate_command_list();

	cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
	cmd_list->copy_texture_region(rtv_resource, 0, nullptr, m_data.host_resource, 0, nullptr);
	cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

	runtime->get_command_queue()->flush_immediate_command_list();
	runtime->get_command_queue()->wait_idle();

	reshade::api::subresource_data host_data;
	
	if (!device->map_texture_region(m_data.host_resource, 0, nullptr, reshade::api::map_access::read_only, &host_data)) {
		return;
	}

	memcpy(m_data.bitmap.bits, host_data.data, static_cast<size_t>(m_data.width) * m_data.height * 4);

	device->unmap_texture_region(m_data.host_resource, 0);
}

void RLStreams::on_finish_effects(effect_runtime* runtime, command_list* cmd_list, resource_view rtv, resource_view rtv_srgb) {
	reshade::api::device* const device = runtime->get_device();
	const reshade::api::resource rtv_resource = device->get_resource_from_view(rtv);
	
	if (!is_recording() && m_data.frame_count != 0) {
		stop_recording(runtime, device);
		return;
	}

	if (!is_recording()) {
		return;
	}

	if (m_data.frame_count == 0) {
		start_recording(runtime, device, rtv_resource);
	}

	runtime->enumerate_texture_variables("rlstreams2.fx", [&](reshade::api::effect_runtime* runtime, auto variable) {
		char source[32] = "";
		runtime->get_texture_variable_name(variable, source);

		if (*cv_rec_custom_pass && m_custom_pass_should_apply) {
			if ((*cv_custom_pass_main_buffer && std::strcmp(source, "RLStreams_Main") == 0) || (!(*cv_custom_pass_main_buffer) && std::strcmp(source, "RLStreams_CustomPass") == 0)) {
				reshade::api::resource_view srv = { 0 };
				runtime->get_texture_binding(variable, &srv);
				copy_to_buffer(runtime, device, device->get_resource_from_view(srv));

				if (m_data.frame_count <= 1) {
					SetAviVideoCompression(m_data.avi_c, m_data.bitmap.hbm, &m_avi_opts, false, NULL);
				}

				AddAviFrame(m_data.avi_c, m_data.bitmap.hbm);
			}
		}
		else {
			if (std::strcmp(source, "RLStreams_Main") == 0) {
				reshade::api::resource_view srv = { 0 };
				runtime->get_texture_binding(variable, &srv);
				copy_to_buffer(runtime, device, device->get_resource_from_view(srv));

				if (m_data.frame_count == 0) {
					SetAviVideoCompression(m_data.avi, m_data.bitmap.hbm, &m_avi_opts, false, NULL);
				}

				AddAviFrame(m_data.avi, m_data.bitmap.hbm);
			}
			if (*cv_rec_depth && std::strcmp(source, "RLStreams_Depth") == 0) {
				reshade::api::resource_view srv = { 0 };
				runtime->get_texture_binding(variable, &srv);
				copy_to_buffer(runtime, device, device->get_resource_from_view(srv));

				if (m_data.frame_count == 0) {
					SetAviVideoCompression(m_data.avi_d, m_data.bitmap.hbm, &m_avi_opts, false, NULL);
				}

				AddAviFrame(m_data.avi_d, m_data.bitmap.hbm);
			}
			if (*cv_rec_normal && std::strcmp(source, "RLStreams_Normal") == 0) {
				reshade::api::resource_view srv = { 0 };
				runtime->get_texture_binding(variable, &srv);
				copy_to_buffer(runtime, device, device->get_resource_from_view(srv));

				if (m_data.frame_count == 0) {
					SetAviVideoCompression(m_data.avi_n, m_data.bitmap.hbm, &m_avi_opts, false, NULL);
				}

				AddAviFrame(m_data.avi_n, m_data.bitmap.hbm);
			}
		}
		});

	if (*cv_rec_data && !(*cv_rec_custom_pass && m_custom_pass_should_apply)) {
		m_data.data_file << get_current_frame_data() << "|";
	}

	if (*cv_rec_custom_pass) {
		m_custom_pass_should_apply = !m_custom_pass_should_apply;
	}

	m_data.frame_count++;
}
