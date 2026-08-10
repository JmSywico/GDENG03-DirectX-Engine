$input a_position, a_normal, a_texcoord0, a_tangent, i_data0, i_data1, i_data2, i_data3
$output v_normal, v_texcoord0, v_tangent, v_bitangent, v_worldPosition, v_viewDepth

#include "bgfx_shader.sh"

void main()
{
	mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	vec4 worldPosition = mul(model, vec4(a_position, 1.0));
	gl_Position = mul(u_viewProj, worldPosition);
	v_normal = normalize(mul(model, vec4(a_normal, 0.0)).xyz);
	v_tangent = normalize(mul(model, vec4(a_tangent.xyz, 0.0)).xyz);
	v_bitangent = normalize(cross(v_normal, v_tangent) * a_tangent.w);
	v_texcoord0 = a_texcoord0;
	v_worldPosition = worldPosition.xyz;
	v_viewDepth = mul(u_view, worldPosition).z;
}
