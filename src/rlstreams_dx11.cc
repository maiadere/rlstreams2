#include "rlstreams.h"

HRESULT GenerateShader(ID3D11Device* pD3DDevice, float r, float g, float b, ID3D11PixelShader** pShader) {
	if (pD3DDevice == NULL) {
		return E_HANDLE;
	}

	std::string pixelShader = std::format("struct VS_OUT"
		"{{"
		"    float4 Position   : SV_Position;"
		"    float4 Color    : COLOR0;"
		"}};"

		"float4 main( VS_OUT input ) : SV_Target"
		"{{"
		"    float4 output;"
		"    output.a = 1.0;"
		"    output.r = {};"
		"    output.g = {};"
		"    output.b = {};"
		"    return output;"
		"}}", r, g, b);

	ID3D10Blob* pBlob;

	HRESULT hr = D3DCompile(pixelShader.c_str(), pixelShader.size(), "shader", NULL, NULL, "main", "ps_4_0", NULL, NULL, &pBlob, NULL);

	if (FAILED(hr))
		return hr;

	hr = pD3DDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, pShader);

	if (FAILED(hr))
		return hr;

	return S_OK;
}

bool operator==(const PropertiesModel& lhs, const PropertiesModel& rhs) {
	return !(lhs.stride != rhs.stride
		|| lhs.vedesc_ByteWidth != rhs.vedesc_ByteWidth
		|| lhs.indesc_ByteWidth != rhs.indesc_ByteWidth
		|| lhs.pscdesc_ByteWidth != rhs.pscdesc_ByteWidth);
}

std::size_t std::hash<PropertiesModel>::operator()(const PropertiesModel & obj) const noexcept {
	std::size_t h1 = std::hash<int>{}(obj.stride);
	std::size_t h2 = std::hash<int>{}(obj.vedesc_ByteWidth);
	std::size_t h3 = std::hash<int>{}(obj.indesc_ByteWidth);
	std::size_t h4 = std::hash<int>{}(obj.pscdesc_ByteWidth);
	return (h1 ^ h3 + h4) ^ (h2 << 1);
}

void RLStreams::draw_indexed(ID3D11DeviceContext* context, UINT index_count, UINT first_index, INT vertex_offset) {
	std::unique_lock<std::mutex> shader_lock(m_shader_mutex);

	if (m_device == NULL) {
		context->GetDevice(&m_device);
		GenerateShader(m_device, 1.0, 0.0, 1.0, &m_shader);
	}

	if (m_shader == NULL) {
		return;
	}

	if (!m_is_in_replay) {
		return;
	}

	ID3D11Buffer* veBuffer, * inBuffer, * pscBuffer;
	UINT Stride, veBufferOffset, inOffset, pscStartSlot = 0;
	D3D11_BUFFER_DESC vedesc, indesc, pscdesc;
	DXGI_FORMAT inFormat;

	context->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer) veBuffer->GetDesc(&vedesc);
	if (veBuffer != NULL) { veBuffer->Release(); veBuffer = NULL; }

	context->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	if (inBuffer) inBuffer->GetDesc(&indesc);
	if (inBuffer != NULL) { inBuffer->Release(); inBuffer = NULL; }

	context->PSGetConstantBuffers(pscStartSlot, 1, &pscBuffer);
	if (pscBuffer != NULL) pscBuffer->GetDesc(&pscdesc);
	if (pscBuffer != NULL) { pscBuffer->Release(); pscBuffer = NULL; }

	if ((Stride == 20 && vedesc.ByteWidth == 203640 && pscdesc.ByteWidth == 4096 && indesc.ByteWidth == 40274) ||
		(Stride == 20 && vedesc.ByteWidth == 208280 && pscdesc.ByteWidth == 4096 && indesc.ByteWidth == 40706) ||
		(Stride == 20 && vedesc.ByteWidth == 203880 && pscdesc.ByteWidth == 4096 && indesc.ByteWidth == 40394) ||
		(Stride == 20 && vedesc.ByteWidth == 242960 && pscdesc.ByteWidth == 4096 && indesc.ByteWidth == 44084) ||
		(Stride == 20 && vedesc.ByteWidth >= 200000 && pscdesc.ByteWidth == 4096 && indesc.ByteWidth >= 40000) ||
		((Stride == 32 || Stride == 12 || Stride == 16 || Stride == 48) && vedesc.ByteWidth == 4194304 && pscdesc.ByteWidth == 4096 && indesc.ByteWidth == 4194304)) {
		return;
	}

	PropertiesModel params_model{};
	params_model.stride = Stride;
	params_model.vedesc_ByteWidth = vedesc.ByteWidth;
	params_model.indesc_ByteWidth = indesc.ByteWidth;
	params_model.pscdesc_ByteWidth = pscdesc.ByteWidth;

	if (*cv_debug_models) {
		std::unique_lock<std::mutex> properties_models_lock(m_properties_models);
		m_seen_params.insert(params_model);

		if (params_model == m_current_params) {
			context->PSSetShader(m_shader, NULL, NULL);
		}
	}

	if ((*cv_rec_custom_pass && is_recording() && m_custom_pass_should_apply) || (*cv_rec_custom_pass_preview && !is_recording())) {
		std::unique_lock<std::mutex> custom_pass_lock(m_custom_pass_mutex);
		std::size_t current_params_hash = std::hash<PropertiesModel>{}(params_model);

		for (std::size_t custom_pass_hash : m_custom_pass) {
			if (current_params_hash == custom_pass_hash) {
				context->PSSetShader(m_shader, NULL, NULL);
			}
		}
	}
}
