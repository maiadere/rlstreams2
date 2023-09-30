#include "rlstreams.h"
#include <reshade.hpp>

using namespace reshade::api;

extern "C" __declspec(dllexport) const char* NAME = "RLStreams2";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "RLStreams2 is a plugin for bakkesmod that allows you to record multiple passes (screen, depth map, normal map and a custom pass) and the camera. This ensures that they are lined up properly without having to re-record.";

BAKKESMOD_PLUGIN(RLStreams, "RLStreams2", "2.0.0", PLUGINTYPE_FREEPLAY);

DrawIndexed oDrawIndexed;

void hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
	singleton->draw_indexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
	oDrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

void RLStreams::initialize_kiero() {
	if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
		kiero::bind(73, (void**)&oDrawIndexed, hkDrawIndexed);
	}
}

static void on_reshade_finish_effects(effect_runtime* runtime, command_list* cmd_list, resource_view rtv, resource_view rtv_srgb) {
	singleton->on_finish_effects(runtime, cmd_list, rtv, rtv_srgb);
}

static void on_reshade_present(effect_runtime* runtime) {
	singleton->reshade_effects_enabled = runtime->get_effects_state();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hinstDLL))
			return FALSE;

		reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);

		reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hinstDLL);
		break;
	}
	return TRUE;
}
